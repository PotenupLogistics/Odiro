#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"

#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"

namespace
{
	// Field row ViewModel 초기화 전에 확정되는 field metadata.
	struct FScenarioTemplateFieldSpec
	{
		// Command dispatch와 테스트에서 사용하는 안정적인 field id.
		FString Id;
		// Sidebar에 표시되는 사용자용 field label.
		FString Label;
		// 재사용 field row widget이 사용할 control shape.
		EScenarioEditorSidebarFieldInputType InputType = EScenarioEditorSidebarFieldInputType::Text;
		// Row가 editable value control을 노출해야 하는지 여부.
		bool bEditable = true;
		// Row가 repeated item add/remove command를 노출해야 하는지 여부.
		bool bArrayControlsEnabled = false;
		// 현재 schema branch에서 row를 표시할지 여부.
		bool bVisible = true;
	};

	// 전용 list widget이 생기기 전까지 comma-separated text로 다루는 string-list field를 식별한다.
	bool IsScenarioTemplateStringListField(const FString& fieldId)
	{
		return fieldId == TEXT("SpawnSegments")
			|| fieldId == TEXT("PlacementZoneSegments")
			|| fieldId == TEXT("PlacementZoneLanes")
			|| fieldId == TEXT("PlacementPaletteCategories")
			|| fieldId == TEXT("PlacementPaletteClasses");
	}

	// Count row와 add/remove command로 표현되는 repeated object collection field를 식별한다.
	bool IsScenarioTemplateObjectArrayField(const FString& fieldId)
	{
		return fieldId == TEXT("AxisPointsCount")
			|| fieldId == TEXT("BuildingSideCount")
			|| fieldId == TEXT("CurbSideCount")
			|| fieldId == TEXT("SegmentsCount")
			|| fieldId == TEXT("Placements")
			|| fieldId == TEXT("PlacementsCount")
			|| fieldId == TEXT("EncountersCount");
	}

	// schema 구조를 설명하는 보조 필드는 detail row 목록에서 제외한다.
	bool IsScenarioTemplateHiddenDetailField(const FString& fieldId)
	{
		return fieldId == TEXT("AxisType")
			|| fieldId == TEXT("AxisPointsCount")
			|| fieldId == TEXT("BuildingSideCount")
			|| fieldId == TEXT("CurbSideCount")
			|| fieldId == TEXT("SegmentsCount")
			|| fieldId == TEXT("Placements")
			|| fieldId == TEXT("PlacementsCount")
			|| fieldId == TEXT("BackgroundCount")
			|| fieldId == TEXT("SpawnSegments")
			|| fieldId == TEXT("EncountersCount");
	}

	// 현재 fallback widget shape만으로 드러나지 않는 semantic field metadata를 적용한다.
	FScenarioTemplateFieldSpec ResolveScenarioTemplateFieldSpec(
		const FString& id,
		const FString& label,
		const EScenarioEditorSidebarFieldInputType inputType,
		const bool bEditable,
		const bool bArrayControlsEnabled,
		const bool bVisible)
	{
		FScenarioTemplateFieldSpec fieldSpec;
		fieldSpec.Id = id;
		fieldSpec.Label = label;
		fieldSpec.InputType = inputType;
		fieldSpec.bEditable = bEditable;
		fieldSpec.bArrayControlsEnabled = bArrayControlsEnabled;
		fieldSpec.bVisible = bVisible && !IsScenarioTemplateHiddenDetailField(id);

		if (IsScenarioTemplateStringListField(fieldSpec.Id))
		{
			fieldSpec.InputType = EScenarioEditorSidebarFieldInputType::StringList;
		}
		else if (IsScenarioTemplateObjectArrayField(fieldSpec.Id))
		{
			fieldSpec.InputType = EScenarioEditorSidebarFieldInputType::ObjectArray;
		}

		return fieldSpec;
	}
}

void UScenarioTemplateSidebarViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
	RefreshDefaultFields();
}

void UScenarioTemplateSidebarViewModel::SelectPanel(const EScenarioTemplateSidebarPanel panel)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActivePanel, panel);
}

void UScenarioTemplateSidebarViewModel::RefreshDefaultFields()
{
	RefreshMainFieldItemsFromTemplate(FScenarioDocument());

	CorridorFieldItems.Reset();
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("AxisType"),
		TEXT("경로 유형"),
		FString(),
		EScenarioEditorSidebarFieldInputType::EnumText,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("AxisPointsCount"),
		TEXT("경로 점 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("WalkwayWidth"),
		TEXT("보행로 폭"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("BuildingSideCount"),
		TEXT("건물측 영역 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("CurbSideCount"),
		TEXT("도로측 영역 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("SegmentsCount"),
		TEXT("구간 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));

	ObstacleFieldItems.Reset();
	ObstacleFieldItems.Add(CreateFieldItem(
		TEXT("MinClearWidth"),
		TEXT("최소 통행 폭"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true));
	ObstacleFieldItems.Add(CreateFieldItem(
		TEXT("Placements"),
		TEXT("배치된 장애물 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Integer,
		false,
		true));

	PedestrianFieldItems.Reset();
	PedestrianFieldItems.Add(CreateFieldItem(
		TEXT("BackgroundCount"),
		TEXT("배경 보행자 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true));
	PedestrianFieldItems.Add(CreateFieldItem(
		TEXT("BackgroundSpeed"),
		TEXT("보행 속도"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true));
	PedestrianFieldItems.Add(CreateFieldItem(
		TEXT("SpawnSegments"),
		TEXT("스폰 구간"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Text,
		true,
		true));
	PedestrianFieldItems.Add(CreateFieldItem(
		TEXT("EncountersCount"),
		TEXT("상호작용 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CorridorFieldItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObstacleFieldItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PedestrianFieldItems);
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::GetVisibleFieldItems() const
{
	switch (ActivePanel)
	{
	case EScenarioTemplateSidebarPanel::Corridor:
		return GetCorridorFieldItems();
	case EScenarioTemplateSidebarPanel::Obstacle:
		return GetObstacleFieldItems();
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return GetPedestrianFieldItems();
	case EScenarioTemplateSidebarPanel::Main:
	default:
		return GetMainFieldItems();
	}
}

bool UScenarioTemplateSidebarViewModel::TryGetDraftScenario(
	FScenarioDocument& outScenario,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	const UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("ScenarioAuthoringSubsystem unavailable.");
		return false;
	}

	outScenario = authoringSubsystem->GetDraftScenario();
	return true;
}

bool UScenarioTemplateSidebarViewModel::SetDraftScenarioId(
	const FString& scenarioId,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetDraftScenarioId(scenarioId, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetDraftIntent(
	const FString& intent,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetDraftIntent(intent, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetDraftRobotStartAnchor(
	const FScenarioTemplateRobotAnchor& anchor,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetDraftRobotStartAnchor(anchor, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetDraftRobotGoalAnchor(
	const FScenarioTemplateRobotAnchor& anchor,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetDraftRobotGoalAnchor(anchor, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::CommitScenarioIdText(
	const FText& text,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetDraftScenarioId(text.ToString(), diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown scenario_id edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitIntentText(
	const FText& text,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetDraftIntent(text.ToString(), diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown intent edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitRobotAnchorText(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& text,
	FString& outStatusText)
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	FScenarioTemplateRobotAnchor anchor = target == EScenarioEditorSidebarRobotAnchorTarget::Start
		? scenarioTemplate.Robot.Start
		: scenarioTemplate.Robot.Goal;
	const FString trimmedText = text.ToString().TrimStartAndEnd();

	switch (field)
	{
	case EScenarioEditorSidebarRobotAnchorField::Type:
	{
		EScenarioTemplateRobotAnchorType anchorType = anchor.Type;
		if (!TryParseRobotAnchorType(text, anchorType))
		{
			outStatusText = TEXT("robot anchor type must be entry, exit, or corridor_pose.");
			return false;
		}
		anchor.Type = anchorType;
		if (anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
		{
			if (anchor.SegmentId.IsEmpty() && !scenarioTemplate.Corridor.Segments.IsEmpty())
			{
				anchor.SegmentId = scenarioTemplate.Corridor.Segments[0].SegmentId;
			}
			if (!anchor.AlongMeters.bIsSet)
			{
				anchor.AlongMeters = MakeFixedTemplateNumberValue(0.0);
			}
			if (!anchor.OffsetMeters.bIsSet)
			{
				anchor.OffsetMeters = MakeFixedTemplateNumberValue(0.0);
			}
			if (anchor.LaneId.IsEmpty())
			{
				anchor.LaneId = TEXT("walkway");
			}
		}
		break;
	}
	case EScenarioEditorSidebarRobotAnchorField::Segment:
		anchor.SegmentId = trimmedText;
		break;
	case EScenarioEditorSidebarRobotAnchorField::Along:
	case EScenarioEditorSidebarRobotAnchorField::Offset:
	{
		FScenarioTemplateNumberValue numberValue;
		if (!TryParseOptionalNumber(text, numberValue))
		{
			outStatusText = TEXT("robot anchor numeric fields must be finite numbers or empty optional values.");
			return false;
		}
		if (field == EScenarioEditorSidebarRobotAnchorField::Along)
		{
			anchor.AlongMeters = numberValue;
		}
		else
		{
			anchor.OffsetMeters = numberValue;
		}
		break;
	}
	case EScenarioEditorSidebarRobotAnchorField::Lane:
		anchor.LaneId = trimmedText;
		break;
	case EScenarioEditorSidebarRobotAnchorField::Heading:
	{
		EScenarioTemplateRobotHeading heading = anchor.Heading;
		if (!TryParseRobotHeading(text, heading))
		{
			outStatusText = TEXT("robot heading must be forward, backward, or auto.");
			return false;
		}
		anchor.Heading = heading;
		break;
	}
	default:
		break;
	}

	return CommitRobotAnchorValue(target, anchor, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitRobotAnchorRange(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& minText,
	const FText& maxText,
	FString& outStatusText)
{
	if (field != EScenarioEditorSidebarRobotAnchorField::Along
		&& field != EScenarioEditorSidebarRobotAnchorField::Offset)
	{
		outStatusText = TEXT("Only robot along_m and offset_m support range editing.");
		return false;
	}

	FScenarioTemplateNumberValue numberValue;
	if (!TryParseOptionalNumberRange(minText, maxText, numberValue))
	{
		outStatusText = TEXT("robot anchor range fields must use numeric min/max values.");
		return false;
	}

	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	FScenarioTemplateRobotAnchor anchor = target == EScenarioEditorSidebarRobotAnchorTarget::Start
		? scenarioTemplate.Robot.Start
		: scenarioTemplate.Robot.Goal;
	if (field == EScenarioEditorSidebarRobotAnchorField::Along)
	{
		anchor.AlongMeters = numberValue;
	}
	else
	{
		anchor.OffsetMeters = numberValue;
	}

	return CommitRobotAnchorValue(target, anchor, outStatusText);
}

void UScenarioTemplateSidebarViewModel::GetCorridorSurfaceIdOptions(TArray<FString>& outSurfaceIds) const
{
	outSurfaceIds.Reset();

	const UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return;
	}

	TArray<FScenarioCorridorSurfaceEntry> surfaceEntries;
	authoringSubsystem->GetCorridorSurfaceEntries(surfaceEntries);
	outSurfaceIds.Reserve(surfaceEntries.Num());
	for (const FScenarioCorridorSurfaceEntry& surfaceEntry : surfaceEntries)
	{
		if (!surfaceEntry.SurfaceId.IsNone())
		{
			outSurfaceIds.Add(surfaceEntry.SurfaceId.ToString());
		}
	}
}

bool UScenarioTemplateSidebarViewModel::SetCorridorWalkwayWidthMeters(
	const FScenarioTemplateNumberValue& widthMeters,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetCorridorWalkwayWidthMeters(widthMeters, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetCorridorSideLaneProfile(
	const EScenarioEditorCorridorSide side,
	const TArray<FScenarioTemplateLaneRule>& lanes,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetCorridorSideLaneProfile(side, lanes, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetCorridorAxisPointsMeters(
	const TArray<FVector2D>& pointsMeters,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetCorridorAxisPointsMeters(pointsMeters, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetCorridorSegments(
	const TArray<FScenarioTemplateSegment>& segments,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetCorridorSegments(segments, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetPedestrianRules(
	const FScenarioTemplatePedestrianRules& pedestrianRules,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetPedestrianRules(pedestrianRules, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorWalkwayWidthText(
	const FText& text,
	FString& outStatusText)
{
	outStatusText.Reset();
	double widthMeters = 0.0;
	if (!TryParseMeters(text, widthMeters))
	{
		outStatusText = TEXT("walkway_width_m must be a number in meters.");
		return false;
	}

	return CommitCorridorWalkwayWidthValue(MakeFixedTemplateNumberValue(widthMeters), outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorWalkwayWidthRangeText(
	const FText& minText,
	const FText& maxText,
	FString& outStatusText)
{
	outStatusText.Reset();
	double minWidthMeters = 0.0;
	double maxWidthMeters = 0.0;
	if (!TryParseMeters(minText, minWidthMeters) || !TryParseMeters(maxText, maxWidthMeters))
	{
		outStatusText = TEXT("walkway_width_m range must use numeric min/max meters.");
		return false;
	}

	return CommitCorridorWalkwayWidthValue(
		MakeRangeTemplateNumberValue(minWidthMeters, maxWidthMeters),
		outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorLaneSurfaceText(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text,
	FString& outStatusText)
{
	TArray<FScenarioTemplateLaneRule> lanes;
	if (!TryGetCorridorLaneProfile(side, lanes, outStatusText))
	{
		return false;
	}
	if (!lanes.IsValidIndex(laneIndex))
	{
		outStatusText = TEXT("Lane index is no longer valid.");
		return false;
	}

	lanes[laneIndex].SurfaceId = text.ToString().TrimStartAndEnd();
	return CommitCorridorLaneProfile(side, lanes, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorLaneWidthText(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text,
	FString& outStatusText)
{
	double widthMeters = 0.0;
	if (!TryParseMeters(text, widthMeters))
	{
		outStatusText = TEXT("lane width_m must be a number in meters.");
		return false;
	}

	TArray<FScenarioTemplateLaneRule> lanes;
	if (!TryGetCorridorLaneProfile(side, lanes, outStatusText))
	{
		return false;
	}
	if (!lanes.IsValidIndex(laneIndex))
	{
		outStatusText = TEXT("Lane index is no longer valid.");
		return false;
	}

	lanes[laneIndex].WidthMeters = MakeFixedTemplateNumberValue(widthMeters);
	return CommitCorridorLaneProfile(side, lanes, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorLaneWidthRangeText(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& minText,
	const FText& maxText,
	FString& outStatusText)
{
	double minWidthMeters = 0.0;
	double maxWidthMeters = 0.0;
	if (!TryParseMeters(minText, minWidthMeters) || !TryParseMeters(maxText, maxWidthMeters))
	{
		outStatusText = TEXT("lane width_m range must use numeric min/max meters.");
		return false;
	}

	TArray<FScenarioTemplateLaneRule> lanes;
	if (!TryGetCorridorLaneProfile(side, lanes, outStatusText))
	{
		return false;
	}
	if (!lanes.IsValidIndex(laneIndex))
	{
		outStatusText = TEXT("Lane index is no longer valid.");
		return false;
	}

	lanes[laneIndex].WidthMeters = MakeRangeTemplateNumberValue(minWidthMeters, maxWidthMeters);
	return CommitCorridorLaneProfile(side, lanes, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddCorridorLaneAfter(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateLaneRule> lanes;
	if (!TryGetCorridorLaneProfile(side, lanes, outStatusText))
	{
		return false;
	}

	const int32 insertionIndex = lanes.IsValidIndex(laneIndex)
		? laneIndex + 1
		: lanes.Num();
	lanes.Insert(MakeDefaultLaneRule(side, lanes, laneIndex), insertionIndex);
	return CommitCorridorLaneProfile(side, lanes, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemoveCorridorLaneAt(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateLaneRule> lanes;
	if (!TryGetCorridorLaneProfile(side, lanes, outStatusText))
	{
		return false;
	}
	const int32 resolvedLaneIndex = laneIndex == INDEX_NONE ? lanes.Num() - 1 : laneIndex;
	if (!lanes.IsValidIndex(resolvedLaneIndex))
	{
		outStatusText = TEXT("Lane index is no longer valid.");
		return false;
	}

	lanes.RemoveAt(resolvedLaneIndex);
	return CommitCorridorLaneProfile(side, lanes, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorAxisPointXText(
	const int32 pointIndex,
	const FText& text,
	FString& outStatusText)
{
	double xMeters = 0.0;
	if (!TryParseMeters(text, xMeters))
	{
		outStatusText = TEXT("axis point x must be a finite number in meters.");
		return false;
	}

	TArray<FVector2D> pointsMeters;
	if (!TryGetCorridorAxisPoints(pointsMeters, outStatusText))
	{
		return false;
	}
	if (!pointsMeters.IsValidIndex(pointIndex))
	{
		outStatusText = TEXT("Axis point index is no longer valid.");
		return false;
	}

	pointsMeters[pointIndex].X = xMeters;
	return CommitCorridorAxisPoints(pointsMeters, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorAxisPointYText(
	const int32 pointIndex,
	const FText& text,
	FString& outStatusText)
{
	double yMeters = 0.0;
	if (!TryParseMeters(text, yMeters))
	{
		outStatusText = TEXT("axis point y must be a finite number in meters.");
		return false;
	}

	TArray<FVector2D> pointsMeters;
	if (!TryGetCorridorAxisPoints(pointsMeters, outStatusText))
	{
		return false;
	}
	if (!pointsMeters.IsValidIndex(pointIndex))
	{
		outStatusText = TEXT("Axis point index is no longer valid.");
		return false;
	}

	pointsMeters[pointIndex].Y = yMeters;
	return CommitCorridorAxisPoints(pointsMeters, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddCorridorAxisPointAfter(
	const int32 pointIndex,
	FString& outStatusText)
{
	TArray<FVector2D> pointsMeters;
	if (!TryGetCorridorAxisPoints(pointsMeters, outStatusText))
	{
		return false;
	}

	const int32 insertionIndex = pointsMeters.IsValidIndex(pointIndex)
		? pointIndex + 1
		: pointsMeters.Num();
	pointsMeters.Insert(MakeDefaultAxisPoint(pointsMeters, pointIndex), insertionIndex);
	return CommitCorridorAxisPoints(pointsMeters, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemoveCorridorAxisPointAt(
	const int32 pointIndex,
	FString& outStatusText)
{
	TArray<FVector2D> pointsMeters;
	if (!TryGetCorridorAxisPoints(pointsMeters, outStatusText))
	{
		return false;
	}
	const int32 resolvedPointIndex = pointIndex == INDEX_NONE ? pointsMeters.Num() - 1 : pointIndex;
	if (!pointsMeters.IsValidIndex(resolvedPointIndex))
	{
		outStatusText = TEXT("Axis point index is no longer valid.");
		return false;
	}

	pointsMeters.RemoveAt(resolvedPointIndex);
	return CommitCorridorAxisPoints(pointsMeters, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorSegmentIdText(
	const int32 segmentIndex,
	const FText& text,
	FString& outStatusText)
{
	TArray<FScenarioTemplateSegment> segments;
	if (!TryGetCorridorSegments(segments, outStatusText))
	{
		return false;
	}
	if (!segments.IsValidIndex(segmentIndex))
	{
		outStatusText = TEXT("Segment index is no longer valid.");
		return false;
	}

	segments[segmentIndex].SegmentId = text.ToString().TrimStartAndEnd();
	return CommitCorridorSegments(segments, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorSegmentTypeText(
	const int32 segmentIndex,
	const FText& text,
	FString& outStatusText)
{
	EScenarioTemplateSegmentType segmentType = EScenarioTemplateSegmentType::Straight;
	if (!TryParseSegmentType(text, segmentType))
	{
		outStatusText = TEXT("segment type must be straight, narrowing, crosswalk, or entrance.");
		return false;
	}

	TArray<FScenarioTemplateSegment> segments;
	if (!TryGetCorridorSegments(segments, outStatusText))
	{
		return false;
	}
	if (!segments.IsValidIndex(segmentIndex))
	{
		outStatusText = TEXT("Segment index is no longer valid.");
		return false;
	}

	segments[segmentIndex].Type = segmentType;
	return CommitCorridorSegments(segments, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorSegmentAlongRangeText(
	const int32 segmentIndex,
	const FText& minText,
	const FText& maxText,
	FString& outStatusText)
{
	double startMeters = 0.0;
	double endMeters = 0.0;
	if (!TryParseMeters(minText, startMeters) || !TryParseMeters(maxText, endMeters))
	{
		outStatusText = TEXT("segment along_range_m must use numeric start/end meters.");
		return false;
	}

	TArray<FScenarioTemplateSegment> segments;
	if (!TryGetCorridorSegments(segments, outStatusText))
	{
		return false;
	}
	if (!segments.IsValidIndex(segmentIndex))
	{
		outStatusText = TEXT("Segment index is no longer valid.");
		return false;
	}

	segments[segmentIndex].AlongRangeMeters.StartMeters = startMeters;
	segments[segmentIndex].AlongRangeMeters.EndMeters = endMeters;
	return CommitCorridorSegments(segments, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorSegmentReplacedByText(
	const int32 segmentIndex,
	const FText& text,
	FString& outStatusText)
{
	TArray<FScenarioTemplateSegment> segments;
	if (!TryGetCorridorSegments(segments, outStatusText))
	{
		return false;
	}
	if (!segments.IsValidIndex(segmentIndex))
	{
		outStatusText = TEXT("Segment index is no longer valid.");
		return false;
	}

	const FString surfaceId = text.ToString().TrimStartAndEnd();
	FScenarioTemplateStringValue replacementSurface;
	if (!surfaceId.IsEmpty())
	{
		replacementSurface.bIsSet = true;
		replacementSurface.Mode = EScenarioTemplateStringValueMode::Fixed;
		replacementSurface.FixedValue = surfaceId;
	}
	segments[segmentIndex].ReplacedBySurfaceId = replacementSurface;
	return CommitCorridorSegments(segments, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddCorridorSegmentAfter(
	const int32 segmentIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateSegment> segments;
	if (!TryGetCorridorSegments(segments, outStatusText))
	{
		return false;
	}

	const int32 insertionIndex = segments.IsValidIndex(segmentIndex)
		? segmentIndex + 1
		: segments.Num();
	segments.Insert(MakeDefaultSegment(segments, segmentIndex), insertionIndex);
	return CommitCorridorSegments(segments, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemoveCorridorSegmentAt(
	const int32 segmentIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateSegment> segments;
	if (!TryGetCorridorSegments(segments, outStatusText))
	{
		return false;
	}
	const int32 resolvedSegmentIndex = segmentIndex == INDEX_NONE ? segments.Num() - 1 : segmentIndex;
	if (!segments.IsValidIndex(resolvedSegmentIndex))
	{
		outStatusText = TEXT("Segment index is no longer valid.");
		return false;
	}

	segments.RemoveAt(resolvedSegmentIndex);
	return CommitCorridorSegments(segments, outStatusText);
}

void UScenarioTemplateSidebarViewModel::GetStaticObstaclePaletteEntries(
	TArray<FScenarioStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();
	const UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	if (authoringSubsystem)
	{
		authoringSubsystem->GetStaticObstaclePaletteEntries(outEntries);
	}
}

bool UScenarioTemplateSidebarViewModel::SetObstacleMinClearWidthMeters(
	const FScenarioTemplateNumberValue& widthMeters,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetObstacleMinClearWidthMeters(widthMeters, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::SetObstaclePlacements(
	const TArray<FScenarioTemplateObstaclePlacement>& placements,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	UScenarioAuthoringSubsystem* authoringSubsystem = ResolveAuthoringSubsystem();
	return authoringSubsystem && authoringSubsystem->SetObstaclePlacements(placements, outDiagnostics);
}

bool UScenarioTemplateSidebarViewModel::CommitObstacleMinClearWidthText(
	const FText& text,
	FString& outStatusText)
{
	FScenarioTemplateNumberValue widthMeters;
	if (!TryParseOptionalNumber(text, widthMeters) || !widthMeters.bIsSet)
	{
		outStatusText = TEXT("min_clear_width_m must be a number in meters.");
		return false;
	}

	return CommitObstacleMinClearWidthValue(widthMeters, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitObstacleMinClearWidthRangeText(
	const FText& minText,
	const FText& maxText,
	FString& outStatusText)
{
	FScenarioTemplateNumberValue widthMeters;
	if (!TryParseOptionalNumberRange(minText, maxText, widthMeters) || !widthMeters.bIsSet)
	{
		outStatusText = TEXT("min_clear_width_m range must use numeric min/max meters.");
		return false;
	}

	return CommitObstacleMinClearWidthValue(widthMeters, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitObstaclePlacementText(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& text,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}
	if (!placements.IsValidIndex(placementIndex))
	{
		outStatusText = TEXT("Obstacle placement index is no longer valid.");
		return false;
	}

	FScenarioTemplateObstaclePlacement& placement = placements[placementIndex];
	const FString trimmedText = text.ToString().TrimStartAndEnd();
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::PlacementId:
		placement.PlacementId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Kind:
	{
		EScenarioTemplateObstaclePlacementKind placementKind = placement.Kind;
		if (!TryParsePlacementKind(text, placementKind))
		{
			outStatusText = TEXT("kind must be fixed, pattern, or scatter.");
			return false;
		}
		placement.Kind = placementKind;
		if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern)
		{
			if (placement.PatternId.IsEmpty())
			{
				placement.PatternId = TEXT("line");
			}
			if (!placement.Count.bIsSet)
			{
				placement.Count = MakeFixedTemplateIntegerValue(2);
			}
			if (!placement.SpacingMeters.bIsSet)
			{
				placement.SpacingMeters = MakeFixedTemplateNumberValue(1.0);
			}
			if (placement.At.LaneId.IsEmpty())
			{
				placement.At.LaneId = TEXT("across");
			}
		}
		else if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Scatter)
		{
			if (placement.Zone.SegmentIds.IsEmpty())
			{
				placement.Zone.SegmentIds.Add(MakeDefaultObstaclePlacement(placements, placementIndex).At.SegmentId);
			}
			if (placement.Zone.LaneIds.IsEmpty())
			{
				placement.Zone.LaneIds.Add(TEXT("walkway"));
			}
			if (!placement.DensityPer10Meters.bIsSet)
			{
				placement.DensityPer10Meters = MakeFixedTemplateNumberValue(1.0);
			}
		}
		else
		{
			if (placement.PropId.IsEmpty())
			{
				placement.PropId = MakeDefaultObstaclePlacement(placements, placementIndex).PropId;
			}
			if (placement.At.SegmentId.IsEmpty())
			{
				placement.At.SegmentId = MakeDefaultObstaclePlacement(placements, placementIndex).At.SegmentId;
			}
			if (!placement.At.AlongMeters.bIsSet)
			{
				placement.At.AlongMeters = MakeFixedTemplateNumberValue(0.0);
			}
			if (!placement.At.OffsetMeters.bIsSet)
			{
				placement.At.OffsetMeters = MakeFixedTemplateNumberValue(0.0);
			}
			if (placement.At.LaneId.IsEmpty())
			{
				placement.At.LaneId = TEXT("walkway");
			}
		}
		break;
	}
	case EScenarioEditorSidebarObstaclePlacementField::Prop:
		placement.PropId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Pattern:
		placement.PatternId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Segment:
		placement.At.SegmentId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Lane:
		placement.At.LaneId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Along:
	case EScenarioEditorSidebarObstaclePlacementField::Offset:
	case EScenarioEditorSidebarObstaclePlacementField::Spacing:
	case EScenarioEditorSidebarObstaclePlacementField::GapWidth:
	case EScenarioEditorSidebarObstaclePlacementField::Density:
	case EScenarioEditorSidebarObstaclePlacementField::Yaw:
	{
		FScenarioTemplateNumberValue numberValue;
		if (!TryParseOptionalNumber(text, numberValue) || !SetPlacementNumberField(placement, field, numberValue))
		{
			outStatusText = TEXT("Obstacle numeric fields must be finite numbers or empty optional values.");
			return false;
		}
		break;
	}
	case EScenarioEditorSidebarObstaclePlacementField::ZoneSegments:
		placement.Zone.SegmentIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::ZoneLanes:
		placement.Zone.LaneIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteCategories:
		placement.Palette.CategoryIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteClasses:
		placement.Palette.ClassIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Count:
	{
		FScenarioTemplateIntegerValue integerValue;
		if (!TryParseOptionalInteger(text, integerValue) || !SetPlacementIntegerField(placement, field, integerValue))
		{
			outStatusText = TEXT("count must be an integer, an integer range, or empty.");
			return false;
		}
		break;
	}
	case EScenarioEditorSidebarObstaclePlacementField::AllowBlocking:
	{
		bool bAllowBlocking = false;
		if (!TryParseBool(text, bAllowBlocking))
		{
			outStatusText = TEXT("allow_blocking must be true or false.");
			return false;
		}
		placement.bAllowBlocking = bAllowBlocking;
		break;
	}
	default:
		break;
	}

	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitObstaclePlacementRange(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& minText,
	const FText& maxText,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}
	if (!placements.IsValidIndex(placementIndex))
	{
		outStatusText = TEXT("Obstacle placement index is no longer valid.");
		return false;
	}

	if (field == EScenarioEditorSidebarObstaclePlacementField::Count)
	{
		FScenarioTemplateIntegerValue integerValue;
		if (!TryParseOptionalIntegerRange(minText, maxText, integerValue)
			|| !SetPlacementIntegerField(placements[placementIndex], field, integerValue))
		{
			outStatusText = TEXT("count range must use integer min/max values.");
			return false;
		}
		return CommitObstaclePlacements(placements, outStatusText);
	}

	FScenarioTemplateNumberValue numberValue;
	if (!TryParseOptionalNumberRange(minText, maxText, numberValue)
		|| !SetPlacementNumberField(placements[placementIndex], field, numberValue))
	{
		outStatusText = TEXT("Obstacle range fields must use numeric min/max values.");
		return false;
	}

	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddObstaclePlacementAfter(
	const int32 placementIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}

	const int32 insertIndex = placements.IsValidIndex(placementIndex)
		? placementIndex + 1
		: placements.Num();
	placements.Insert(MakeDefaultObstaclePlacement(placements, placementIndex), insertIndex);
	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemoveObstaclePlacementAt(
	const int32 placementIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}

	const int32 resolvedPlacementIndex = placementIndex == INDEX_NONE ? placements.Num() - 1 : placementIndex;
	if (!placements.IsValidIndex(resolvedPlacementIndex))
	{
		outStatusText = TEXT("Obstacle placement index is no longer valid.");
		return false;
	}

	placements.RemoveAt(resolvedPlacementIndex);
	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitObstaclePlacementStringListItemText(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const int32 itemIndex,
	const FText& text,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}
	if (!placements.IsValidIndex(placementIndex))
	{
		outStatusText = TEXT("Obstacle placement index is no longer valid.");
		return false;
	}

	TArray<FString>* values = nullptr;
	if (!ResolveObstaclePlacementStringListField(placements[placementIndex], field, values) || !values)
	{
		outStatusText = TEXT("Obstacle placement field is not a string list.");
		return false;
	}
	if (!values->IsValidIndex(itemIndex))
	{
		outStatusText = TEXT("Obstacle string-list item index is no longer valid.");
		return false;
	}

	const FString trimmedText = text.ToString().TrimStartAndEnd();
	if (trimmedText.IsEmpty())
	{
		outStatusText = TEXT("String-list item values cannot be empty.");
		return false;
	}

	(*values)[itemIndex] = trimmedText;
	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddObstaclePlacementStringListItemAfter(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const int32 itemIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}
	if (!placements.IsValidIndex(placementIndex))
	{
		outStatusText = TEXT("Obstacle placement index is no longer valid.");
		return false;
	}

	TArray<FString>* values = nullptr;
	if (!ResolveObstaclePlacementStringListField(placements[placementIndex], field, values) || !values)
	{
		outStatusText = TEXT("Obstacle placement field is not a string list.");
		return false;
	}

	const FString defaultValue = MakeDefaultObstaclePlacementStringListItem(field, *values);
	if (defaultValue.IsEmpty())
	{
		outStatusText = TEXT("No default value is available for this string-list field.");
		return false;
	}

	const int32 insertIndex = values->IsValidIndex(itemIndex) ? itemIndex + 1 : values->Num();
	values->Insert(defaultValue, insertIndex);
	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemoveObstaclePlacementStringListItemAt(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const int32 itemIndex,
	FString& outStatusText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements;
	if (!TryGetObstaclePlacements(placements, outStatusText))
	{
		return false;
	}
	if (!placements.IsValidIndex(placementIndex))
	{
		outStatusText = TEXT("Obstacle placement index is no longer valid.");
		return false;
	}

	TArray<FString>* values = nullptr;
	if (!ResolveObstaclePlacementStringListField(placements[placementIndex], field, values) || !values)
	{
		outStatusText = TEXT("Obstacle placement field is not a string list.");
		return false;
	}
	if (!values->IsValidIndex(itemIndex))
	{
		outStatusText = TEXT("Obstacle string-list item index is no longer valid.");
		return false;
	}

	values->RemoveAt(itemIndex);
	return CommitObstaclePlacements(placements, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::CommitPedestrianSpawnSegmentText(
	const int32 segmentIndex,
	const FText& text,
	FString& outStatusText)
{
	FScenarioTemplatePedestrianRules pedestrianRules;
	if (!TryGetPedestrianRules(pedestrianRules, outStatusText))
	{
		return false;
	}
	if (!pedestrianRules.Background.SpawnSegmentIds.IsValidIndex(segmentIndex))
	{
		outStatusText = TEXT("Pedestrian spawn segment index is no longer valid.");
		return false;
	}

	const FString trimmedText = text.ToString().TrimStartAndEnd();
	if (trimmedText.IsEmpty())
	{
		outStatusText = TEXT("Pedestrian spawn segment cannot be empty.");
		return false;
	}

	pedestrianRules.Background.SpawnSegmentIds[segmentIndex] = trimmedText;
	return CommitPedestrianRules(pedestrianRules, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddPedestrianSpawnSegmentAfter(
	const int32 segmentIndex,
	FString& outStatusText)
{
	FScenarioTemplatePedestrianRules pedestrianRules;
	if (!TryGetPedestrianRules(pedestrianRules, outStatusText))
	{
		return false;
	}

	const FString defaultSegmentId = MakeDefaultPedestrianSpawnSegmentId(
		pedestrianRules.Background.SpawnSegmentIds);
	if (defaultSegmentId.IsEmpty())
	{
		outStatusText = TEXT("No corridor segment is available for pedestrian spawning.");
		return false;
	}

	const int32 insertIndex = pedestrianRules.Background.SpawnSegmentIds.IsValidIndex(segmentIndex)
		? segmentIndex + 1
		: pedestrianRules.Background.SpawnSegmentIds.Num();
	pedestrianRules.Background.SpawnSegmentIds.Insert(defaultSegmentId, insertIndex);
	return CommitPedestrianRules(pedestrianRules, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemovePedestrianSpawnSegmentAt(
	const int32 segmentIndex,
	FString& outStatusText)
{
	FScenarioTemplatePedestrianRules pedestrianRules;
	if (!TryGetPedestrianRules(pedestrianRules, outStatusText))
	{
		return false;
	}
	if (!pedestrianRules.Background.SpawnSegmentIds.IsValidIndex(segmentIndex))
	{
		outStatusText = TEXT("Pedestrian spawn segment index is no longer valid.");
		return false;
	}

	pedestrianRules.Background.SpawnSegmentIds.RemoveAt(segmentIndex);
	return CommitPedestrianRules(pedestrianRules, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::AddPedestrianEncounterAfter(
	const int32 encounterIndex,
	FString& outStatusText)
{
	FScenarioTemplatePedestrianRules pedestrianRules;
	if (!TryGetPedestrianRules(pedestrianRules, outStatusText))
	{
		return false;
	}

	const int32 insertIndex = pedestrianRules.Encounters.IsValidIndex(encounterIndex)
		? encounterIndex + 1
		: pedestrianRules.Encounters.Num();
	pedestrianRules.Encounters.Insert(
		MakeDefaultPedestrianEncounter(pedestrianRules.Encounters, encounterIndex),
		insertIndex);
	return CommitPedestrianRules(pedestrianRules, outStatusText);
}

bool UScenarioTemplateSidebarViewModel::RemovePedestrianEncounterAt(
	const int32 encounterIndex,
	FString& outStatusText)
{
	FScenarioTemplatePedestrianRules pedestrianRules;
	if (!TryGetPedestrianRules(pedestrianRules, outStatusText))
	{
		return false;
	}

	const int32 resolvedEncounterIndex = encounterIndex == INDEX_NONE
		? pedestrianRules.Encounters.Num() - 1
		: encounterIndex;
	if (!pedestrianRules.Encounters.IsValidIndex(resolvedEncounterIndex))
	{
		outStatusText = TEXT("Pedestrian encounter index is no longer valid.");
		return false;
	}

	pedestrianRules.Encounters.RemoveAt(resolvedEncounterIndex);
	return CommitPedestrianRules(pedestrianRules, outStatusText);
}

FScenarioTemplateNumberValue UScenarioTemplateSidebarViewModel::MakeFixedTemplateNumberValue(const double value)
{
	return UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(value);
}

FScenarioTemplateNumberValue UScenarioTemplateSidebarViewModel::MakeRangeTemplateNumberValue(
	const double minValue,
	const double maxValue)
{
	return UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(minValue, maxValue);
}

FScenarioTemplateIntegerValue UScenarioTemplateSidebarViewModel::MakeFixedTemplateIntegerValue(const int32 value)
{
	return UScenarioAuthoringSubsystem::MakeFixedTemplateIntegerValue(value);
}

FScenarioTemplateIntegerValue UScenarioTemplateSidebarViewModel::MakeRangeTemplateIntegerValue(
	const int32 minValue,
	const int32 maxValue)
{
	return UScenarioAuthoringSubsystem::MakeRangeTemplateIntegerValue(minValue, maxValue);
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::GetMainFieldItems() const
{
	return CopyItems(MainFieldItems);
}

void UScenarioTemplateSidebarViewModel::RefreshMainFieldItemsFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	CacheCorridorSegmentOptions(scenarioTemplate);
	MainFieldItems.Reset();

	MainFieldItems.Add(CreateFieldItem(
		TEXT("ScenarioId"),
		TEXT("시나리오 이름"),
		scenarioTemplate.ScenarioId,
		EScenarioEditorSidebarFieldInputType::Text,
		true));
	MainFieldItems.Add(CreateFieldItem(
		TEXT("Intent"),
		TEXT("검증 목표"),
		scenarioTemplate.Intent,
		EScenarioEditorSidebarFieldInputType::MultilineText,
		true));
	MainFieldItems.Add(CreateFieldItem(
		TEXT("RobotStart"),
		TEXT("시작 위치"),
		FormatRobotAnchorSummary(scenarioTemplate.Robot.Start),
		EScenarioEditorSidebarFieldInputType::Text,
		false));
	MainFieldItems.Add(CreateFieldItem(
		TEXT("RobotGoal"),
		TEXT("목표 위치"),
		FormatRobotAnchorSummary(scenarioTemplate.Robot.Goal),
		EScenarioEditorSidebarFieldInputType::Text,
		false));

	AppendRobotAnchorFieldItems(MainFieldItems, TEXT("RobotStart"), scenarioTemplate.Robot.Start);
	AppendRobotAnchorFieldItems(MainFieldItems, TEXT("RobotGoal"), scenarioTemplate.Robot.Goal);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MainFieldItems);
}

UScenarioTemplateFieldRowViewModel* UScenarioTemplateSidebarViewModel::FindMainFieldItem(
	const FString& fieldId) const
{
	return FindFieldItemById(MainFieldItems, fieldId);
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::GetCorridorFieldItems() const
{
	return CopyItems(CorridorFieldItems);
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::GetObstacleFieldItems() const
{
	return CopyItems(ObstacleFieldItems);
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::GetPedestrianFieldItems() const
{
	return CopyItems(PedestrianFieldItems);
}

void UScenarioTemplateSidebarViewModel::RefreshPedestrianFieldItemsFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	CacheCorridorSegmentOptions(scenarioTemplate);
	const FScenarioTemplatePedestrianRules& pedestrians = scenarioTemplate.Pedestrians;

	PedestrianFieldItems.Reset();

	UScenarioTemplateFieldRowViewModel* backgroundCount = CreateFieldItem(
		TEXT("BackgroundCount"),
		TEXT("배경 보행자 수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyIntegerFieldValue(backgroundCount, pedestrians.Background.Count);
	PedestrianFieldItems.Add(backgroundCount);

	UScenarioTemplateFieldRowViewModel* backgroundSpeed = CreateFieldItem(
		TEXT("BackgroundSpeed"),
		TEXT("보행 속도"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(backgroundSpeed, pedestrians.Background.SpeedMetersPerSecond);
	PedestrianFieldItems.Add(backgroundSpeed);

	PedestrianFieldItems.Add(CreateFieldItem(
		TEXT("SpawnSegments"),
		TEXT("스폰 구간"),
		JoinStringList(pedestrians.Background.SpawnSegmentIds),
		EScenarioEditorSidebarFieldInputType::Text,
		true,
		true));

	PedestrianFieldItems.Add(CreateFieldItem(
		TEXT("EncountersCount"),
		TEXT("상호작용 수"),
		FString::FromInt(pedestrians.Encounters.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PedestrianFieldItems);
}

UScenarioTemplateFieldRowViewModel* UScenarioTemplateSidebarViewModel::FindPedestrianFieldItem(
	const FString& fieldId) const
{
	return FindFieldItemById(PedestrianFieldItems, fieldId);
}

void UScenarioTemplateSidebarViewModel::RefreshObstacleFieldItemsFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	CacheCorridorSegmentOptions(scenarioTemplate);
	const FScenarioTemplateObstacleRules& obstacles = scenarioTemplate.Obstacles;

	ObstacleFieldItems.Reset();

	UScenarioTemplateFieldRowViewModel* minClearWidth = CreateFieldItem(
		TEXT("MinClearWidth"),
		TEXT("최소 통행 폭"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(minClearWidth, obstacles.MinClearWidthMeters);
	ObstacleFieldItems.Add(minClearWidth);

	ObstacleFieldItems.Add(CreateFieldItem(
		TEXT("PlacementsCount"),
		TEXT("배치된 장애물 수"),
		FString::FromInt(obstacles.Placements.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false,
		false));

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObstacleFieldItems);
}

UScenarioTemplateFieldRowViewModel* UScenarioTemplateSidebarViewModel::FindObstacleFieldItem(
	const FString& fieldId) const
{
	return FindFieldItemById(ObstacleFieldItems, fieldId);
}

void UScenarioTemplateSidebarViewModel::RefreshCorridorFieldItemsFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	CacheCorridorSegmentOptions(scenarioTemplate);
	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;

	CorridorFieldItems.Reset();

	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("AxisType"),
		TEXT("경로 유형"),
		AxisTypeToString(corridor.Axis.Type),
		EScenarioEditorSidebarFieldInputType::EnumText,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("AxisPointsCount"),
		TEXT("경로 점 수"),
		FString::FromInt(corridor.Axis.PointsMeters.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));

	UScenarioTemplateFieldRowViewModel* walkwayWidth = CreateFieldItem(
		TEXT("WalkwayWidth"),
		TEXT("보행로 폭"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(walkwayWidth, corridor.WalkwayWidthMeters);
	CorridorFieldItems.Add(walkwayWidth);

	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("BuildingSideCount"),
		TEXT("건물측 영역 수"),
		FString::FromInt(corridor.BuildingSide.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("CurbSideCount"),
		TEXT("도로측 영역 수"),
		FString::FromInt(corridor.CurbSide.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));
	CorridorFieldItems.Add(CreateFieldItem(
		TEXT("SegmentsCount"),
		TEXT("구간 수"),
		FString::FromInt(corridor.Segments.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false));

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CorridorFieldItems);
}

UScenarioTemplateFieldRowViewModel* UScenarioTemplateSidebarViewModel::FindCorridorFieldItem(
	const FString& fieldId) const
{
	return FindFieldItemById(CorridorFieldItems, fieldId);
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::CreateCorridorPointFieldItems(
	const int32 pointIndex,
	const FVector2D& pointMeters)
{
	TArray<UScenarioTemplateFieldRowViewModel*> fieldItems;
	fieldItems.Reserve(2);

	fieldItems.Add(CreateFieldItem(
		TEXT("CorridorPointX"),
		TEXT("X 위치"),
		FormatEditableNumber(pointMeters.X),
		EScenarioEditorSidebarFieldInputType::Number,
		true));
	fieldItems.Add(CreateFieldItem(
		TEXT("CorridorPointY"),
		TEXT("Y 위치"),
		FormatEditableNumber(pointMeters.Y),
		EScenarioEditorSidebarFieldInputType::Number,
		true));

	for (UScenarioTemplateFieldRowViewModel* fieldItem : fieldItems)
	{
		if (fieldItem)
		{
			fieldItem->SetPayloadPath(FString::FromInt(pointIndex));
		}
	}
	return fieldItems;
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::CreateCorridorLaneFieldItems(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FScenarioTemplateLaneRule& lane,
	const TArray<FString>& surfaceOptions)
{
	TArray<UScenarioTemplateFieldRowViewModel*> fieldItems;
	fieldItems.Reserve(2);

	UScenarioTemplateFieldRowViewModel* surface = CreateFieldItem(
		TEXT("CorridorLaneSurface"),
		TEXT("표면 유형"),
		lane.SurfaceId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	surface->SetComboOptions(surfaceOptions);
	surface->SetComboAllowsUnset(false, FString());
	fieldItems.Add(surface);

	UScenarioTemplateFieldRowViewModel* width = CreateFieldItem(
		TEXT("CorridorLaneWidth"),
		TEXT("폭"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(width, lane.WidthMeters);
	fieldItems.Add(width);

	const FString payloadPath = FString::Printf(
		TEXT("%s/%d"),
		side == EScenarioEditorCorridorSide::Building ? TEXT("building") : TEXT("curb"),
		laneIndex);
	for (UScenarioTemplateFieldRowViewModel* fieldItem : fieldItems)
	{
		if (fieldItem)
		{
			fieldItem->SetPayloadPath(payloadPath);
		}
	}
	return fieldItems;
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::CreateCorridorSegmentFieldItems(
	const int32 segmentIndex,
	const FScenarioTemplateSegment& segment,
	const TArray<FString>& surfaceOptions)
{
	TArray<UScenarioTemplateFieldRowViewModel*> fieldItems;
	fieldItems.Reserve(4);

	fieldItems.Add(CreateFieldItem(
		TEXT("CorridorSegmentId"),
		TEXT("구간 이름"),
		segment.SegmentId,
		EScenarioEditorSidebarFieldInputType::Text,
		true));

	UScenarioTemplateFieldRowViewModel* segmentType = CreateFieldItem(
		TEXT("CorridorSegmentType"),
		TEXT("구간 유형"),
		SegmentTypeToString(segment.Type),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	segmentType->SetComboOptions({ TEXT("straight"), TEXT("narrowing"), TEXT("crosswalk"), TEXT("entrance") });
	segmentType->SetComboAllowsUnset(false, FString());
	fieldItems.Add(segmentType);

	const FString startMeters = FormatEditableNumber(segment.AlongRangeMeters.StartMeters);
	const FString endMeters = FormatEditableNumber(segment.AlongRangeMeters.EndMeters);
	UScenarioTemplateFieldRowViewModel* alongRange = CreateFieldItem(
		TEXT("CorridorSegmentAlongRange"),
		TEXT("적용 범위"),
		FString::Printf(TEXT("%s..%s"), *startMeters, *endMeters),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	alongRange->SetRangeValueText(startMeters, endMeters);
	alongRange->SetRangeInputEnabled(true);
	fieldItems.Add(alongRange);

	UScenarioTemplateFieldRowViewModel* replacedBy = CreateFieldItem(
		TEXT("CorridorSegmentReplacedBy"),
		TEXT("대체 표면"),
		FormatEditableStringValue(segment.ReplacedBySurfaceId),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	replacedBy->SetComboOptions(surfaceOptions);
	replacedBy->SetComboAllowsUnset(true, TEXT("(unset)"));
	fieldItems.Add(replacedBy);

	for (UScenarioTemplateFieldRowViewModel* fieldItem : fieldItems)
	{
		if (fieldItem)
		{
			fieldItem->SetPayloadPath(FString::FromInt(segmentIndex));
		}
	}
	return fieldItems;
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::CreateObstaclePlacementFieldItems(
	const int32 placementIndex,
	const FScenarioTemplateObstaclePlacement& placement)
{
	const bool bFixedPlacement = placement.Kind == EScenarioTemplateObstaclePlacementKind::Fixed;
	const bool bPatternPlacement = placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern;
	const bool bScatterPlacement = placement.Kind == EScenarioTemplateObstaclePlacementKind::Scatter;

	TArray<UScenarioTemplateFieldRowViewModel*> fieldItems;
	fieldItems.Reserve(18);

	fieldItems.Add(CreateFieldItem(
		TEXT("PlacementId"),
		TEXT("장애물명"),
		placement.PlacementId,
		EScenarioEditorSidebarFieldInputType::Text,
		true,
		false));
	UScenarioTemplateFieldRowViewModel* placementKind = CreateFieldItem(
		TEXT("PlacementKind"),
		TEXT("배치 방식"),
		ObstaclePlacementKindToString(placement.Kind),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	placementKind->SetComboOptions(GetObstaclePlacementKindOptions());
	placementKind->SetComboAllowsUnset(false, FString());
	fieldItems.Add(placementKind);
	UScenarioTemplateFieldRowViewModel* prop = CreateFieldItem(
		TEXT("PlacementProp"),
		TEXT("장애물 종류"),
		placement.PropId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		bFixedPlacement || bPatternPlacement,
		false,
		bFixedPlacement || bPatternPlacement);
	prop->SetComboOptions(GetObstaclePropIdOptions());
	prop->SetComboAllowsUnset(false, FString());
	fieldItems.Add(prop);
	UScenarioTemplateFieldRowViewModel* pattern = CreateFieldItem(
		TEXT("PlacementPattern"),
		TEXT("배치 패턴"),
		placement.PatternId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		bPatternPlacement,
		false,
		bPatternPlacement);
	pattern->SetComboOptions(GetObstaclePatternOptions());
	pattern->SetComboAllowsUnset(false, FString());
	fieldItems.Add(pattern);

	UScenarioTemplateFieldRowViewModel* segment = CreateFieldItem(
		TEXT("PlacementSegment"),
		TEXT("구간"),
		placement.At.SegmentId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		bFixedPlacement || bPatternPlacement,
		false,
		bFixedPlacement || bPatternPlacement);
	segment->SetComboOptions(CorridorSegmentIdOptions);
	segment->SetComboAllowsUnset(false, FString());
	fieldItems.Add(segment);

	UScenarioTemplateFieldRowViewModel* lane = CreateFieldItem(
		TEXT("PlacementLane"),
		TEXT("배치 영역"),
		placement.At.LaneId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		bFixedPlacement || bPatternPlacement,
		false,
		bFixedPlacement || bPatternPlacement);
	lane->SetComboOptions(GetLaneHintOptions());
	lane->SetComboAllowsUnset(true, TEXT("(unset)"));
	fieldItems.Add(lane);

	UScenarioTemplateFieldRowViewModel* along = CreateFieldItem(
		TEXT("PlacementAlong"),
		TEXT("진행 거리"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		bFixedPlacement || bPatternPlacement,
		false,
		bFixedPlacement || bPatternPlacement);
	ApplyNumberFieldValue(along, placement.At.AlongMeters);
	fieldItems.Add(along);

	UScenarioTemplateFieldRowViewModel* offset = CreateFieldItem(
		TEXT("PlacementOffset"),
		TEXT("좌우 위치"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		bFixedPlacement || bPatternPlacement,
		false,
		bFixedPlacement || bPatternPlacement);
	ApplyNumberFieldValue(offset, placement.At.OffsetMeters);
	fieldItems.Add(offset);

	fieldItems.Add(CreateFieldItem(
		TEXT("PlacementZoneSegments"),
		TEXT("허용 구간"),
		JoinStringList(placement.Zone.SegmentIds),
		EScenarioEditorSidebarFieldInputType::Text,
		bScatterPlacement,
		false,
		bScatterPlacement));
	fieldItems.Add(CreateFieldItem(
		TEXT("PlacementZoneLanes"),
		TEXT("허용 영역"),
		JoinStringList(placement.Zone.LaneIds),
		EScenarioEditorSidebarFieldInputType::Text,
		bScatterPlacement,
		false,
		bScatterPlacement));
	fieldItems.Add(CreateFieldItem(
		TEXT("PlacementPaletteCategories"),
		TEXT("후보 카테고리"),
		JoinStringList(placement.Palette.CategoryIds),
		EScenarioEditorSidebarFieldInputType::Text,
		bScatterPlacement,
		false,
		bScatterPlacement));
	fieldItems.Add(CreateFieldItem(
		TEXT("PlacementPaletteClasses"),
		TEXT("후보 클래스"),
		JoinStringList(placement.Palette.ClassIds),
		EScenarioEditorSidebarFieldInputType::Text,
		bScatterPlacement,
		false,
		bScatterPlacement));

	UScenarioTemplateFieldRowViewModel* count = CreateFieldItem(
		TEXT("PlacementCount"),
		TEXT("개수"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		bPatternPlacement || bScatterPlacement,
		false,
		bPatternPlacement || bScatterPlacement);
	ApplyIntegerFieldValue(count, placement.Count);
	fieldItems.Add(count);

	UScenarioTemplateFieldRowViewModel* spacing = CreateFieldItem(
		TEXT("PlacementSpacing"),
		TEXT("간격"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		bPatternPlacement,
		false,
		bPatternPlacement);
	ApplyNumberFieldValue(spacing, placement.SpacingMeters);
	fieldItems.Add(spacing);

	UScenarioTemplateFieldRowViewModel* gapWidth = CreateFieldItem(
		TEXT("PlacementGapWidth"),
		TEXT("통과 간격"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		bPatternPlacement,
		false,
		bPatternPlacement);
	ApplyNumberFieldValue(gapWidth, placement.GapWidthMeters);
	fieldItems.Add(gapWidth);

	UScenarioTemplateFieldRowViewModel* density = CreateFieldItem(
		TEXT("PlacementDensity"),
		TEXT("생성 밀도"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		bScatterPlacement,
		false,
		bScatterPlacement);
	ApplyNumberFieldValue(density, placement.DensityPer10Meters);
	fieldItems.Add(density);

	UScenarioTemplateFieldRowViewModel* yaw = CreateFieldItem(
		TEXT("PlacementYaw"),
		TEXT("회전"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(yaw, placement.YawDegrees);
	fieldItems.Add(yaw);

	UScenarioTemplateFieldRowViewModel* allowBlocking = CreateFieldItem(
		TEXT("PlacementAllowBlocking"),
		TEXT("통로 차단 허용"),
		placement.bAllowBlocking ? FString(TEXT("true")) : FString(TEXT("false")),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	allowBlocking->SetComboOptions(GetBooleanOptions());
	allowBlocking->SetComboAllowsUnset(false, FString());
	fieldItems.Add(allowBlocking);

	for (UScenarioTemplateFieldRowViewModel* fieldItem : fieldItems)
	{
		if (fieldItem)
		{
			fieldItem->SetPayloadPath(FString::FromInt(placementIndex));
		}
	}
	return fieldItems;
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::CreatePedestrianEncounterFieldItems(
	const int32 encounterIndex,
	const FScenarioTemplatePedestrianEncounter& encounter)
{
	TArray<UScenarioTemplateFieldRowViewModel*> fieldItems;
	fieldItems.Reserve(11);

	fieldItems.Add(CreateFieldItem(
		TEXT("EncounterId"),
		TEXT("상황 이름"),
		encounter.EncounterId,
		EScenarioEditorSidebarFieldInputType::Text,
		true));
	UScenarioTemplateFieldRowViewModel* type = CreateFieldItem(
		TEXT("EncounterType"),
		TEXT("상황 유형"),
		EncounterTypeToString(encounter.Type),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	type->SetComboOptions(GetPedestrianEncounterTypeOptions());
	type->SetComboAllowsUnset(false, FString());
	fieldItems.Add(type);

	UScenarioTemplateFieldRowViewModel* atSegment = CreateFieldItem(
		TEXT("EncounterAtSegment"),
		TEXT("발생 구간"),
		encounter.AtSegmentId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	atSegment->SetComboOptions(CorridorSegmentIdOptions);
	atSegment->SetComboAllowsUnset(false, FString());
	fieldItems.Add(atSegment);

	UScenarioTemplateFieldRowViewModel* persona = CreateFieldItem(
		TEXT("EncounterPersona"),
		TEXT("보행자 성향"),
		encounter.PersonaId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	persona->SetComboOptions(GetPedestrianPersonaOptions());
	persona->SetComboAllowsUnset(true, TEXT("(unset)"));
	fieldItems.Add(persona);

	UScenarioTemplateFieldRowViewModel* meetOffset = CreateFieldItem(
		TEXT("EncounterMeetOffset"),
		TEXT("만남 위치 보정"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(meetOffset, encounter.MeetOffsetMeters);
	fieldItems.Add(meetOffset);

	UScenarioTemplateFieldRowViewModel* cooperation = CreateFieldItem(
		TEXT("EncounterCooperation"),
		TEXT("양보 성향"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(cooperation, encounter.Overrides.Cooperation);
	fieldItems.Add(cooperation);

	UScenarioTemplateFieldRowViewModel* evasiveness = CreateFieldItem(
		TEXT("EncounterEvasiveness"),
		TEXT("회피 성향"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(evasiveness, encounter.Overrides.Evasiveness);
	fieldItems.Add(evasiveness);

	UScenarioTemplateFieldRowViewModel* personalSpace = CreateFieldItem(
		TEXT("EncounterPersonalSpace"),
		TEXT("개인 공간"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(personalSpace, encounter.Overrides.PersonalSpaceMeters);
	fieldItems.Add(personalSpace);

	UScenarioTemplateFieldRowViewModel* awarenessHorizon = CreateFieldItem(
		TEXT("EncounterAwarenessHorizon"),
		TEXT("예측 시간"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(awarenessHorizon, encounter.Overrides.AwarenessHorizonSeconds);
	fieldItems.Add(awarenessHorizon);

	UScenarioTemplateFieldRowViewModel* maxYieldWait = CreateFieldItem(
		TEXT("EncounterMaxYieldWait"),
		TEXT("대기 한계 시간"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(maxYieldWait, encounter.Overrides.MaxYieldWaitSeconds);
	fieldItems.Add(maxYieldWait);

	UScenarioTemplateFieldRowViewModel* sidestepDistance = CreateFieldItem(
		TEXT("EncounterSidestepDistance"),
		TEXT("회피 이동 거리"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true);
	ApplyNumberFieldValue(sidestepDistance, encounter.Overrides.SidestepDistanceMeters);
	fieldItems.Add(sidestepDistance);

	for (UScenarioTemplateFieldRowViewModel* fieldItem : fieldItems)
	{
		if (fieldItem)
		{
			fieldItem->SetPayloadPath(FString::FromInt(encounterIndex));
		}
	}
	return fieldItems;
}

UScenarioTemplateFieldRowViewModel* UScenarioTemplateSidebarViewModel::CreateFieldItem(
	const FString& id,
	const FString& label,
	const FString& value,
	const EScenarioEditorSidebarFieldInputType inputType,
	const bool bEditable,
	const bool bArrayControlsEnabled,
	const bool bVisible)
{
	UScenarioTemplateFieldRowViewModel* item = NewObject<UScenarioTemplateFieldRowViewModel>(this);
	if (item)
	{
		const FScenarioTemplateFieldSpec fieldSpec = ResolveScenarioTemplateFieldSpec(
			id,
			label,
			inputType,
			bEditable,
			bArrayControlsEnabled,
			bVisible);
		item->InitializeFieldRow(
			fieldSpec.Id,
			fieldSpec.Label,
			value,
			fieldSpec.InputType,
			fieldSpec.bEditable,
			fieldSpec.bArrayControlsEnabled,
			fieldSpec.bVisible);
	}
	return item;
}

void UScenarioTemplateSidebarViewModel::ApplyNumberFieldValue(
	UScenarioTemplateFieldRowViewModel* fieldItem,
	const FScenarioTemplateNumberValue& value)
{
	if (!fieldItem)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldItem->SetValueText(FString());
		fieldItem->SetRangeValueText(FString(), FString());
		fieldItem->SetRangeInputEnabled(false);
		return;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		fieldItem->SetValueText(FormatEditableNumber((value.MinValue + value.MaxValue) * 0.5));
		fieldItem->SetRangeValueText(FormatEditableNumber(value.MinValue), FormatEditableNumber(value.MaxValue));
		fieldItem->SetRangeInputEnabled(true);
		return;
	}
	fieldItem->SetValueText(FormatEditableNumber(value.FixedValue));
	fieldItem->SetRangeValueText(FormatEditableNumber(value.FixedValue), FormatEditableNumber(value.FixedValue));
	fieldItem->SetRangeInputEnabled(false);
}

void UScenarioTemplateSidebarViewModel::ApplyIntegerFieldValue(
	UScenarioTemplateFieldRowViewModel* fieldItem,
	const FScenarioTemplateIntegerValue& value)
{
	if (!fieldItem)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldItem->SetValueText(FString());
		fieldItem->SetRangeValueText(FString(), FString());
		fieldItem->SetRangeInputEnabled(false);
		return;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		fieldItem->SetValueText(FormatEditableInteger(FMath::RoundToInt((value.MinValue + value.MaxValue) * 0.5f)));
		fieldItem->SetRangeValueText(FormatEditableInteger(value.MinValue), FormatEditableInteger(value.MaxValue));
		fieldItem->SetRangeInputEnabled(true);
		return;
	}
	fieldItem->SetValueText(FormatEditableInteger(value.FixedValue));
	fieldItem->SetRangeValueText(FormatEditableInteger(value.FixedValue), FormatEditableInteger(value.FixedValue));
	fieldItem->SetRangeInputEnabled(false);
}

UScenarioTemplateFieldRowViewModel* UScenarioTemplateSidebarViewModel::FindFieldItemById(
	const TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>>& items,
	const FString& fieldId)
{
	for (UScenarioTemplateFieldRowViewModel* item : items)
	{
		if (item && item->GetItemId() == fieldId)
		{
			return item;
		}
	}
	return nullptr;
}

void UScenarioTemplateSidebarViewModel::CacheCorridorSegmentOptions(
	const FScenarioDocument& scenarioTemplate)
{
	CorridorSegmentIdOptions.Reset();
	for (const FScenarioTemplateSegment& segment : scenarioTemplate.Corridor.Segments)
	{
		if (!segment.SegmentId.IsEmpty())
		{
			CorridorSegmentIdOptions.AddUnique(segment.SegmentId);
		}
	}
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetObstaclePlacementKindOptions()
{
	return { TEXT("fixed"), TEXT("pattern"), TEXT("scatter") };
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetObstaclePropIdOptions() const
{
	TArray<FScenarioStaticObstaclePropEntry> propEntries;
	GetStaticObstaclePaletteEntries(propEntries);

	TArray<FString> propIds;
	propIds.Reserve(propEntries.Num());
	for (const FScenarioStaticObstaclePropEntry& propEntry : propEntries)
	{
		if (!propEntry.PropId.IsNone())
		{
			propIds.AddUnique(propEntry.PropId.ToString());
		}
	}
	return propIds;
}

FString UScenarioTemplateSidebarViewModel::StaticObstacleCategoryToId(
	const EScenarioStaticObstaclePropCategory category)
{
	switch (category)
	{
	case EScenarioStaticObstaclePropCategory::StreetFurniture:
		return TEXT("street_furniture");
	case EScenarioStaticObstaclePropCategory::TrafficControl:
		return TEXT("traffic_control");
	case EScenarioStaticObstaclePropCategory::DeliveryItem:
		return TEXT("delivery_item");
	case EScenarioStaticObstaclePropCategory::Utility:
		return TEXT("utility");
	case EScenarioStaticObstaclePropCategory::SurfaceObject:
		return TEXT("surface_object");
	case EScenarioStaticObstaclePropCategory::Unknown:
	default:
		return FString();
	}
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetObstacleCategoryIdOptions() const
{
	TArray<FScenarioStaticObstaclePropEntry> propEntries;
	GetStaticObstaclePaletteEntries(propEntries);

	TArray<FString> categoryIds;
	categoryIds.Reserve(propEntries.Num());
	for (const FScenarioStaticObstaclePropEntry& propEntry : propEntries)
	{
		const FString categoryId = StaticObstacleCategoryToId(propEntry.Category);
		if (!categoryId.IsEmpty())
		{
			categoryIds.AddUnique(categoryId);
		}
	}
	return categoryIds;
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetObstacleClassIdOptions() const
{
	TArray<FScenarioStaticObstaclePropEntry> propEntries;
	GetStaticObstaclePaletteEntries(propEntries);

	TArray<FString> classIds;
	classIds.Reserve(propEntries.Num());
	for (const FScenarioStaticObstaclePropEntry& propEntry : propEntries)
	{
		if (!propEntry.SemanticTypeId.IsNone())
		{
			classIds.AddUnique(propEntry.SemanticTypeId.ToString());
		}
	}
	return classIds;
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetObstaclePatternOptions()
{
	return { TEXT("gate"), TEXT("line"), TEXT("cluster") };
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetLaneHintOptions()
{
	return { TEXT("walkway"), TEXT("building_edge"), TEXT("center"), TEXT("curb_edge"), TEXT("across") };
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetPedestrianEncounterTypeOptions()
{
	return { TEXT("oncoming_pass"), TEXT("overtake"), TEXT("cross_path"), TEXT("standing_group") };
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetPedestrianPersonaOptions()
{
	return { TEXT("passive"), TEXT("normal"), TEXT("assertive"), TEXT("vulnerable") };
}

TArray<FString> UScenarioTemplateSidebarViewModel::GetBooleanOptions()
{
	return { TEXT("false"), TEXT("true") };
}

void UScenarioTemplateSidebarViewModel::AppendRobotAnchorFieldItems(
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>>& items,
	const FString& idPrefix,
	const FScenarioTemplateRobotAnchor& anchor)
{
	const bool bUsesCorridorPose = anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose;
	TArray<FString> anchorTypeOptions = { TEXT("entry"), TEXT("exit"), TEXT("corridor_pose") };
	TArray<FString> headingOptions = { TEXT("forward"), TEXT("backward"), TEXT("auto") };

	UScenarioTemplateFieldRowViewModel* type = CreateFieldItem(
		idPrefix + TEXT("Type"),
		TEXT("위치 기준"),
		RobotAnchorTypeToString(anchor.Type),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	type->SetComboOptions(anchorTypeOptions);
	type->SetComboAllowsUnset(false, FString());
	items.Add(type);

	UScenarioTemplateFieldRowViewModel* segment = CreateFieldItem(
		idPrefix + TEXT("Segment"),
		TEXT("구간"),
		anchor.SegmentId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true,
		false,
		bUsesCorridorPose);
	segment->SetComboOptions(CorridorSegmentIdOptions);
	segment->SetComboAllowsUnset(false, FString());
	items.Add(segment);

	UScenarioTemplateFieldRowViewModel* along = CreateFieldItem(
		idPrefix + TEXT("Along"),
		TEXT("진행 거리"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true,
		false,
		bUsesCorridorPose);
	ApplyNumberFieldValue(along, anchor.AlongMeters);
	items.Add(along);

	UScenarioTemplateFieldRowViewModel* offset = CreateFieldItem(
		idPrefix + TEXT("Offset"),
		TEXT("좌우 위치"),
		FString(),
		EScenarioEditorSidebarFieldInputType::Range,
		true,
		false,
		bUsesCorridorPose);
	ApplyNumberFieldValue(offset, anchor.OffsetMeters);
	items.Add(offset);

	UScenarioTemplateFieldRowViewModel* lane = CreateFieldItem(
		idPrefix + TEXT("Lane"),
		TEXT("이동 영역"),
		anchor.LaneId,
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true,
		false,
		bUsesCorridorPose);
	lane->SetComboOptions(GetLaneHintOptions());
	lane->SetComboAllowsUnset(true, TEXT("(unset)"));
	items.Add(lane);

	UScenarioTemplateFieldRowViewModel* heading = CreateFieldItem(
		idPrefix + TEXT("Heading"),
		TEXT("진행 방향"),
		RobotHeadingToString(anchor.Heading),
		EScenarioEditorSidebarFieldInputType::ComboBox,
		true);
	heading->SetComboOptions(headingOptions);
	heading->SetComboAllowsUnset(false, FString());
	items.Add(heading);
}

FString UScenarioTemplateSidebarViewModel::RobotAnchorTypeToString(
	const EScenarioTemplateRobotAnchorType type)
{
	switch (type)
	{
	case EScenarioTemplateRobotAnchorType::Entry:
		return TEXT("entry");
	case EScenarioTemplateRobotAnchorType::Exit:
		return TEXT("exit");
	case EScenarioTemplateRobotAnchorType::CorridorPose:
		return TEXT("corridor_pose");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioTemplateSidebarViewModel::RobotHeadingToString(
	const EScenarioTemplateRobotHeading heading)
{
	switch (heading)
	{
	case EScenarioTemplateRobotHeading::Forward:
		return TEXT("forward");
	case EScenarioTemplateRobotHeading::Backward:
		return TEXT("backward");
	case EScenarioTemplateRobotHeading::Auto:
		return TEXT("auto");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioTemplateSidebarViewModel::FormatNumberSummary(
	const FScenarioTemplateNumberValue& value,
	const FString& suffix)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%.2f..%.2f%s"), value.MinValue, value.MaxValue, *suffix);
	}

	return FString::Printf(TEXT("%.2f%s"), value.FixedValue, *suffix);
}

FString UScenarioTemplateSidebarViewModel::FormatRobotAnchorSummary(
	const FScenarioTemplateRobotAnchor& anchor)
{
	if (anchor.Type != EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		return FString::Printf(
			TEXT("%s | heading: %s"),
			*RobotAnchorTypeToString(anchor.Type),
			*RobotHeadingToString(anchor.Heading));
	}

	return FString::Printf(
		TEXT("corridor_pose | segment: %s | along: %s | offset: %s | lane: %s | heading: %s"),
		anchor.SegmentId.IsEmpty() ? TEXT("(unset)") : *anchor.SegmentId,
		*FormatNumberSummary(anchor.AlongMeters, TEXT("m")),
		*FormatNumberSummary(anchor.OffsetMeters, TEXT("m")),
		anchor.LaneId.IsEmpty() ? TEXT("(unset)") : *anchor.LaneId,
		*RobotHeadingToString(anchor.Heading));
}

bool UScenarioTemplateSidebarViewModel::TryGetCorridorLaneProfile(
	const EScenarioEditorCorridorSide side,
	TArray<FScenarioTemplateLaneRule>& outLanes,
	FString& outStatusText) const
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;
	outLanes = side == EScenarioEditorCorridorSide::Building
		? corridor.BuildingSide
		: corridor.CurbSide;
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryGetCorridorAxisPoints(
	TArray<FVector2D>& outPointsMeters,
	FString& outStatusText) const
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	outPointsMeters = scenarioTemplate.Corridor.Axis.PointsMeters;
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryGetCorridorSegments(
	TArray<FScenarioTemplateSegment>& outSegments,
	FString& outStatusText) const
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	outSegments = scenarioTemplate.Corridor.Segments;
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorWalkwayWidthValue(
	const FScenarioTemplateNumberValue& widthMeters,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetCorridorWalkwayWidthMeters(widthMeters, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Corridor edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorLaneProfile(
	const EScenarioEditorCorridorSide side,
	const TArray<FScenarioTemplateLaneRule>& lanes,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetCorridorSideLaneProfile(side, lanes, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Corridor edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorAxisPoints(
	const TArray<FVector2D>& pointsMeters,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetCorridorAxisPointsMeters(pointsMeters, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Corridor edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitCorridorSegments(
	const TArray<FScenarioTemplateSegment>& segments,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetCorridorSegments(segments, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Corridor edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

FScenarioTemplateLaneRule UScenarioTemplateSidebarViewModel::MakeDefaultLaneRule(
	const EScenarioEditorCorridorSide side,
	const TArray<FScenarioTemplateLaneRule>& existingLanes,
	const int32 neighborIndex)
{
	if (existingLanes.IsValidIndex(neighborIndex))
	{
		return existingLanes[neighborIndex];
	}
	if (!existingLanes.IsEmpty())
	{
		return existingLanes.Last();
	}

	FScenarioTemplateLaneRule lane;
	lane.SurfaceId = side == EScenarioEditorCorridorSide::Building
		? FString(TEXT("building"))
		: FString(TEXT("road"));
	lane.WidthMeters = MakeFixedTemplateNumberValue(0.4);
	return lane;
}

FVector2D UScenarioTemplateSidebarViewModel::MakeDefaultAxisPoint(
	const TArray<FVector2D>& existingPoints,
	const int32 neighborIndex)
{
	if (existingPoints.IsValidIndex(neighborIndex)
		&& existingPoints.IsValidIndex(neighborIndex + 1))
	{
		return (existingPoints[neighborIndex] + existingPoints[neighborIndex + 1]) * 0.5;
	}

	if (existingPoints.IsValidIndex(neighborIndex))
	{
		const FVector2D basePoint = existingPoints[neighborIndex];
		if (existingPoints.IsValidIndex(neighborIndex - 1))
		{
			const FVector2D direction = (basePoint - existingPoints[neighborIndex - 1]).GetSafeNormal();
			return basePoint + (direction.IsNearlyZero() ? FVector2D(1.0, 0.0) : direction);
		}
		return basePoint + FVector2D(1.0, 0.0);
	}

	if (!existingPoints.IsEmpty())
	{
		const FVector2D basePoint = existingPoints.Last();
		if (existingPoints.Num() >= 2)
		{
			const FVector2D direction = (basePoint - existingPoints[existingPoints.Num() - 2]).GetSafeNormal();
			return basePoint + (direction.IsNearlyZero() ? FVector2D(1.0, 0.0) : direction);
		}
		return basePoint + FVector2D(1.0, 0.0);
	}

	return FVector2D(1.0, 0.0);
}

FScenarioTemplateSegment UScenarioTemplateSidebarViewModel::MakeDefaultSegment(
	const TArray<FScenarioTemplateSegment>& existingSegments,
	const int32 neighborIndex) const
{
	TSet<FString> usedIds;
	for (const FScenarioTemplateSegment& segment : existingSegments)
	{
		if (!segment.SegmentId.IsEmpty())
		{
			usedIds.Add(segment.SegmentId);
		}
	}

	FString segmentId;
	for (int32 index = 1; index < 10000; ++index)
	{
		const FString candidateId = FString::Printf(TEXT("segment_%d"), index);
		if (!usedIds.Contains(candidateId))
		{
			segmentId = candidateId;
			break;
		}
	}
	if (segmentId.IsEmpty())
	{
		segmentId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	FScenarioTemplateSegment segment;
	if (existingSegments.IsValidIndex(neighborIndex))
	{
		segment = existingSegments[neighborIndex];
	}
	else if (!existingSegments.IsEmpty())
	{
		segment = existingSegments.Last();
	}
	else
	{
		FScenarioDocument scenarioTemplate;
		FString failureReason;
		const double axisLengthMeters = TryGetDraftScenario(scenarioTemplate, failureReason)
			? MeasureAxisLengthMeters(scenarioTemplate.Corridor.Axis.PointsMeters)
			: 1.0;
		segment.Type = EScenarioTemplateSegmentType::Straight;
		segment.AlongRangeMeters.StartMeters = 0.0;
		segment.AlongRangeMeters.EndMeters = FMath::Max(1.0, axisLengthMeters);
	}

	segment.SegmentId = segmentId;
	return segment;
}

bool UScenarioTemplateSidebarViewModel::TryParseMeters(const FText& text, double& outMeters)
{
	FString meterText = text.ToString().TrimStartAndEnd();
	meterText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
	meterText.TrimStartAndEndInline();
	return LexTryParseString(outMeters, *meterText);
}

bool UScenarioTemplateSidebarViewModel::TryParseSegmentType(
	const FText& text,
	EScenarioTemplateSegmentType& outType)
{
	const FString segmentTypeText = text.ToString().TrimStartAndEnd().ToLower();
	if (segmentTypeText == TEXT("straight"))
	{
		outType = EScenarioTemplateSegmentType::Straight;
		return true;
	}
	if (segmentTypeText == TEXT("narrowing"))
	{
		outType = EScenarioTemplateSegmentType::Narrowing;
		return true;
	}
	if (segmentTypeText == TEXT("crosswalk"))
	{
		outType = EScenarioTemplateSegmentType::Crosswalk;
		return true;
	}
	if (segmentTypeText == TEXT("entrance"))
	{
		outType = EScenarioTemplateSegmentType::Entrance;
		return true;
	}

	return false;
}

FString UScenarioTemplateSidebarViewModel::JoinDiagnostics(
	const TArray<FString>& diagnostics,
	const FString& fallbackText)
{
	return diagnostics.IsEmpty()
		? fallbackText
		: FString::Join(diagnostics, TEXT("\n"));
}

bool UScenarioTemplateSidebarViewModel::CommitRobotAnchorValue(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const FScenarioTemplateRobotAnchor& anchor,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	const bool bCommitted = target == EScenarioEditorSidebarRobotAnchorTarget::Start
		? SetDraftRobotStartAnchor(anchor, diagnostics)
		: SetDraftRobotGoalAnchor(anchor, diagnostics);
	if (!bCommitted)
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown robot anchor edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryParseRobotAnchorType(
	const FText& text,
	EScenarioTemplateRobotAnchorType& outType)
{
	const FString typeText = text.ToString().TrimStartAndEnd().ToLower();
	if (typeText == TEXT("entry"))
	{
		outType = EScenarioTemplateRobotAnchorType::Entry;
		return true;
	}
	if (typeText == TEXT("exit"))
	{
		outType = EScenarioTemplateRobotAnchorType::Exit;
		return true;
	}
	if (typeText == TEXT("corridor_pose"))
	{
		outType = EScenarioTemplateRobotAnchorType::CorridorPose;
		return true;
	}
	return false;
}

bool UScenarioTemplateSidebarViewModel::TryParseRobotHeading(
	const FText& text,
	EScenarioTemplateRobotHeading& outHeading)
{
	const FString headingText = text.ToString().TrimStartAndEnd().ToLower();
	if (headingText == TEXT("forward"))
	{
		outHeading = EScenarioTemplateRobotHeading::Forward;
		return true;
	}
	if (headingText == TEXT("backward"))
	{
		outHeading = EScenarioTemplateRobotHeading::Backward;
		return true;
	}
	if (headingText == TEXT("auto"))
	{
		outHeading = EScenarioTemplateRobotHeading::Auto;
		return true;
	}
	return false;
}

bool UScenarioTemplateSidebarViewModel::TryGetObstaclePlacements(
	TArray<FScenarioTemplateObstaclePlacement>& outPlacements,
	FString& outStatusText) const
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	outPlacements = scenarioTemplate.Obstacles.Placements;
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitObstacleMinClearWidthValue(
	const FScenarioTemplateNumberValue& widthMeters,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetObstacleMinClearWidthMeters(widthMeters, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Obstacle edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitObstaclePlacements(
	const TArray<FScenarioTemplateObstaclePlacement>& placements,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetObstaclePlacements(placements, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Obstacle edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

FScenarioTemplateObstaclePlacement UScenarioTemplateSidebarViewModel::MakeDefaultObstaclePlacement(
	const TArray<FScenarioTemplateObstaclePlacement>& existingPlacements,
	const int32 neighborIndex) const
{
	FScenarioTemplateObstaclePlacement placement;
	placement.Kind = EScenarioTemplateObstaclePlacementKind::Fixed;

	const FString baseId = TEXT("obstacle");
	for (int32 candidateIndex = 1; candidateIndex < 1000; ++candidateIndex)
	{
		const FString candidateId = FString::Printf(TEXT("%s_%03d"), *baseId, candidateIndex);
		const bool bExists = existingPlacements.ContainsByPredicate(
			[&candidateId](const FScenarioTemplateObstaclePlacement& existingPlacement)
			{
				return existingPlacement.PlacementId == candidateId;
			});
		if (!bExists)
		{
			placement.PlacementId = candidateId;
			break;
		}
	}

	if (existingPlacements.IsValidIndex(neighborIndex) && !existingPlacements[neighborIndex].PropId.IsEmpty())
	{
		placement.PropId = existingPlacements[neighborIndex].PropId;
	}
	else
	{
		TArray<FScenarioStaticObstaclePropEntry> propEntries;
		GetStaticObstaclePaletteEntries(propEntries);
		if (!propEntries.IsEmpty() && !propEntries[0].PropId.IsNone())
		{
			placement.PropId = propEntries[0].PropId.ToString();
		}
	}

	if (existingPlacements.IsValidIndex(neighborIndex) && !existingPlacements[neighborIndex].At.SegmentId.IsEmpty())
	{
		placement.At.SegmentId = existingPlacements[neighborIndex].At.SegmentId;
	}
	else
	{
		FScenarioDocument scenarioTemplate;
		FString failureReason;
		if (TryGetDraftScenario(scenarioTemplate, failureReason)
			&& !scenarioTemplate.Corridor.Segments.IsEmpty())
		{
			placement.At.SegmentId = scenarioTemplate.Corridor.Segments[0].SegmentId;
		}
	}
	placement.At.AlongMeters = MakeFixedTemplateNumberValue(0.0);
	placement.At.OffsetMeters = MakeFixedTemplateNumberValue(0.0);
	placement.At.LaneId = TEXT("walkway");
	placement.YawDegrees = MakeFixedTemplateNumberValue(0.0);
	placement.bAllowBlocking = false;
	return placement;
}

bool UScenarioTemplateSidebarViewModel::ResolveObstaclePlacementStringListField(
	FScenarioTemplateObstaclePlacement& placement,
	const EScenarioEditorSidebarObstaclePlacementField field,
	TArray<FString>*& outValues)
{
	outValues = nullptr;
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::ZoneSegments:
		outValues = &placement.Zone.SegmentIds;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::ZoneLanes:
		outValues = &placement.Zone.LaneIds;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteCategories:
		outValues = &placement.Palette.CategoryIds;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteClasses:
		outValues = &placement.Palette.ClassIds;
		return true;
	default:
		return false;
	}
}

FString UScenarioTemplateSidebarViewModel::MakeDefaultObstaclePlacementStringListItem(
	const EScenarioEditorSidebarObstaclePlacementField field,
	const TArray<FString>& existingValues) const
{
	TArray<FString> options;
	FString fallbackValue;
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::ZoneSegments:
		options = CorridorSegmentIdOptions;
		if (options.IsEmpty())
		{
			FScenarioDocument scenarioTemplate;
			FString failureReason;
			if (TryGetDraftScenario(scenarioTemplate, failureReason))
			{
				for (const FScenarioTemplateSegment& segment : scenarioTemplate.Corridor.Segments)
				{
					if (!segment.SegmentId.IsEmpty())
					{
						options.AddUnique(segment.SegmentId);
					}
				}
			}
		}
		break;
	case EScenarioEditorSidebarObstaclePlacementField::ZoneLanes:
		options = GetLaneHintOptions();
		fallbackValue = TEXT("walkway");
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteCategories:
		options = GetObstacleCategoryIdOptions();
		fallbackValue = TEXT("street_furniture");
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteClasses:
		options = GetObstacleClassIdOptions();
		fallbackValue = TEXT("obstacle");
		break;
	default:
		break;
	}

	for (const FString& option : options)
	{
		if (!option.IsEmpty() && !existingValues.Contains(option))
		{
			return option;
		}
	}
	if (!fallbackValue.IsEmpty() && !existingValues.Contains(fallbackValue))
	{
		return fallbackValue;
	}
	return options.IsEmpty() ? fallbackValue : FString();
}

FString UScenarioTemplateSidebarViewModel::MakeDefaultPedestrianSpawnSegmentId(
	const TArray<FString>& existingValues) const
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		return FString();
	}

	for (const FScenarioTemplateSegment& segment : scenarioTemplate.Corridor.Segments)
	{
		if (!segment.SegmentId.IsEmpty() && !existingValues.Contains(segment.SegmentId))
		{
			return segment.SegmentId;
		}
	}
	return FString();
}

bool UScenarioTemplateSidebarViewModel::TryGetPedestrianRules(
	FScenarioTemplatePedestrianRules& outPedestrianRules,
	FString& outStatusText) const
{
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		outStatusText = failureReason;
		return false;
	}

	outPedestrianRules = scenarioTemplate.Pedestrians;
	return true;
}

bool UScenarioTemplateSidebarViewModel::CommitPedestrianRules(
	const FScenarioTemplatePedestrianRules& pedestrianRules,
	FString& outStatusText)
{
	TArray<FString> diagnostics;
	if (!SetPedestrianRules(pedestrianRules, diagnostics))
	{
		outStatusText = JoinDiagnostics(diagnostics, TEXT("Unknown Pedestrian edit failure."));
		return false;
	}

	outStatusText.Reset();
	return true;
}

FScenarioTemplatePedestrianEncounter UScenarioTemplateSidebarViewModel::MakeDefaultPedestrianEncounter(
	const TArray<FScenarioTemplatePedestrianEncounter>& existingEncounters,
	const int32 neighborIndex) const
{
	FScenarioTemplatePedestrianEncounter encounter;
	encounter.Type = existingEncounters.IsValidIndex(neighborIndex)
		? existingEncounters[neighborIndex].Type
		: EScenarioTemplateEncounterType::CrossPath;
	encounter.PersonaId = existingEncounters.IsValidIndex(neighborIndex)
		&& !existingEncounters[neighborIndex].PersonaId.IsEmpty()
			? existingEncounters[neighborIndex].PersonaId
			: TEXT("normal");
	encounter.MeetOffsetMeters = MakeFixedTemplateNumberValue(0.0);

	for (int32 candidateIndex = 1; candidateIndex < 1000; ++candidateIndex)
	{
		const FString candidateId = FString::Printf(TEXT("encounter_%03d"), candidateIndex);
		const bool bExists = existingEncounters.ContainsByPredicate(
			[&candidateId](const FScenarioTemplatePedestrianEncounter& existingEncounter)
			{
				return existingEncounter.EncounterId == candidateId;
			});
		if (!bExists)
		{
			encounter.EncounterId = candidateId;
			break;
		}
	}

	if (existingEncounters.IsValidIndex(neighborIndex) && !existingEncounters[neighborIndex].AtSegmentId.IsEmpty())
	{
		encounter.AtSegmentId = existingEncounters[neighborIndex].AtSegmentId;
		return encounter;
	}

	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (TryGetDraftScenario(scenarioTemplate, failureReason)
		&& !scenarioTemplate.Corridor.Segments.IsEmpty())
	{
		encounter.AtSegmentId = scenarioTemplate.Corridor.Segments[0].SegmentId;
	}
	return encounter;
}

bool UScenarioTemplateSidebarViewModel::SetPlacementNumberField(
	FScenarioTemplateObstaclePlacement& placement,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FScenarioTemplateNumberValue& value)
{
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::Along:
		placement.At.AlongMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Offset:
		placement.At.OffsetMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Spacing:
		placement.SpacingMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::GapWidth:
		placement.GapWidthMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Density:
		placement.DensityPer10Meters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Yaw:
		placement.YawDegrees = value;
		return true;
	default:
		return false;
	}
}

bool UScenarioTemplateSidebarViewModel::SetPlacementIntegerField(
	FScenarioTemplateObstaclePlacement& placement,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FScenarioTemplateIntegerValue& value)
{
	if (field != EScenarioEditorSidebarObstaclePlacementField::Count)
	{
		return false;
	}

	placement.Count = value;
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryParsePlacementKind(
	const FText& text,
	EScenarioTemplateObstaclePlacementKind& outKind)
{
	const FString kindText = text.ToString().TrimStartAndEnd().ToLower();
	if (kindText == TEXT("fixed"))
	{
		outKind = EScenarioTemplateObstaclePlacementKind::Fixed;
		return true;
	}
	if (kindText == TEXT("pattern"))
	{
		outKind = EScenarioTemplateObstaclePlacementKind::Pattern;
		return true;
	}
	if (kindText == TEXT("scatter"))
	{
		outKind = EScenarioTemplateObstaclePlacementKind::Scatter;
		return true;
	}
	return false;
}

bool UScenarioTemplateSidebarViewModel::TryParseScalar(const FText& text, double& outValue)
{
	FString scalarText = text.ToString().TrimStartAndEnd();
	scalarText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
	scalarText.RemoveFromEnd(TEXT("deg"), ESearchCase::IgnoreCase);
	scalarText.TrimStartAndEndInline();
	return LexTryParseString(outValue, *scalarText) && FMath::IsFinite(outValue);
}

bool UScenarioTemplateSidebarViewModel::TryParseOptionalNumber(
	const FText& text,
	FScenarioTemplateNumberValue& outValue)
{
	const FString trimmedText = text.ToString().TrimStartAndEnd();
	if (trimmedText.IsEmpty())
	{
		outValue = MakeUnsetNumberValue();
		return true;
	}

	double parsedValue = 0.0;
	if (!TryParseScalar(text, parsedValue))
	{
		return false;
	}

	outValue = MakeFixedTemplateNumberValue(parsedValue);
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryParseOptionalNumberRange(
	const FText& minText,
	const FText& maxText,
	FScenarioTemplateNumberValue& outValue)
{
	const bool bMinEmpty = minText.ToString().TrimStartAndEnd().IsEmpty();
	const bool bMaxEmpty = maxText.ToString().TrimStartAndEnd().IsEmpty();
	if (bMinEmpty && bMaxEmpty)
	{
		outValue = MakeUnsetNumberValue();
		return true;
	}

	double minValue = 0.0;
	double maxValue = 0.0;
	if (!TryParseScalar(minText, minValue) || !TryParseScalar(maxText, maxValue))
	{
		return false;
	}

	outValue = MakeRangeTemplateNumberValue(minValue, maxValue);
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryParseOptionalInteger(
	const FText& text,
	FScenarioTemplateIntegerValue& outValue)
{
	const FString trimmedText = text.ToString().TrimStartAndEnd();
	if (trimmedText.IsEmpty())
	{
		outValue = MakeUnsetIntegerValue();
		return true;
	}

	int32 parsedValue = 0;
	if (!LexTryParseString(parsedValue, *trimmedText))
	{
		return false;
	}

	outValue = MakeFixedTemplateIntegerValue(parsedValue);
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryParseOptionalIntegerRange(
	const FText& minText,
	const FText& maxText,
	FScenarioTemplateIntegerValue& outValue)
{
	const bool bMinEmpty = minText.ToString().TrimStartAndEnd().IsEmpty();
	const bool bMaxEmpty = maxText.ToString().TrimStartAndEnd().IsEmpty();
	if (bMinEmpty && bMaxEmpty)
	{
		outValue = MakeUnsetIntegerValue();
		return true;
	}

	int32 minValue = 0;
	int32 maxValue = 0;
	const FString trimmedMinText = minText.ToString().TrimStartAndEnd();
	const FString trimmedMaxText = maxText.ToString().TrimStartAndEnd();
	if (!LexTryParseString(minValue, *trimmedMinText)
		|| !LexTryParseString(maxValue, *trimmedMaxText))
	{
		return false;
	}

	outValue = MakeRangeTemplateIntegerValue(minValue, maxValue);
	return true;
}

bool UScenarioTemplateSidebarViewModel::TryParseBool(const FText& text, bool& outValue)
{
	const FString boolText = text.ToString().TrimStartAndEnd().ToLower();
	if (boolText == TEXT("true") || boolText == TEXT("1") || boolText == TEXT("yes"))
	{
		outValue = true;
		return true;
	}
	if (boolText == TEXT("false") || boolText == TEXT("0") || boolText == TEXT("no"))
	{
		outValue = false;
		return true;
	}
	return false;
}

TArray<FString> UScenarioTemplateSidebarViewModel::ParseStringList(const FString& text)
{
	TArray<FString> values;
	text.ParseIntoArray(values, TEXT(","), true);
	for (FString& value : values)
	{
		value.TrimStartAndEndInline();
	}
	values.RemoveAll(
		[](const FString& value)
		{
			return value.IsEmpty();
		});
	return values;
}

FScenarioTemplateNumberValue UScenarioTemplateSidebarViewModel::MakeUnsetNumberValue()
{
	return FScenarioTemplateNumberValue();
}

FScenarioTemplateIntegerValue UScenarioTemplateSidebarViewModel::MakeUnsetIntegerValue()
{
	return FScenarioTemplateIntegerValue();
}

FString UScenarioTemplateSidebarViewModel::EncounterTypeToString(
	const EScenarioTemplateEncounterType type)
{
	switch (type)
	{
	case EScenarioTemplateEncounterType::OncomingPass:
		return TEXT("oncoming_pass");
	case EScenarioTemplateEncounterType::Overtake:
		return TEXT("overtake");
	case EScenarioTemplateEncounterType::CrossPath:
		return TEXT("cross_path");
	case EScenarioTemplateEncounterType::StandingGroup:
		return TEXT("standing_group");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioTemplateSidebarViewModel::ObstaclePlacementKindToString(
	const EScenarioTemplateObstaclePlacementKind kind)
{
	switch (kind)
	{
	case EScenarioTemplateObstaclePlacementKind::Fixed:
		return TEXT("fixed");
	case EScenarioTemplateObstaclePlacementKind::Pattern:
		return TEXT("pattern");
	case EScenarioTemplateObstaclePlacementKind::Scatter:
		return TEXT("scatter");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioTemplateSidebarViewModel::AxisTypeToString(
	const EScenarioCorridorAxisType type)
{
	switch (type)
	{
	case EScenarioCorridorAxisType::Polyline:
		return TEXT("polyline");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioTemplateSidebarViewModel::SegmentTypeToString(
	const EScenarioTemplateSegmentType type)
{
	switch (type)
	{
	case EScenarioTemplateSegmentType::Straight:
		return TEXT("straight");
	case EScenarioTemplateSegmentType::Narrowing:
		return TEXT("narrowing");
	case EScenarioTemplateSegmentType::Crosswalk:
		return TEXT("crosswalk");
	case EScenarioTemplateSegmentType::Entrance:
		return TEXT("entrance");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioTemplateSidebarViewModel::FormatEditableStringValue(
	const FScenarioTemplateStringValue& value)
{
	if (!value.bIsSet)
	{
		return FString();
	}
	if (value.Mode == EScenarioTemplateStringValueMode::Choices)
	{
		return value.Choices.IsEmpty() ? FString() : value.Choices[0];
	}

	return value.FixedValue;
}

double UScenarioTemplateSidebarViewModel::MeasureAxisLengthMeters(
	const TArray<FVector2D>& pointsMeters)
{
	double lengthMeters = 0.0;
	for (int32 index = 1; index < pointsMeters.Num(); ++index)
	{
		lengthMeters += FVector2D::Distance(pointsMeters[index - 1], pointsMeters[index]);
	}
	return lengthMeters;
}

FString UScenarioTemplateSidebarViewModel::JoinStringList(const TArray<FString>& values)
{
	return FString::Join(values, TEXT(", "));
}

FString UScenarioTemplateSidebarViewModel::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

FString UScenarioTemplateSidebarViewModel::FormatEditableInteger(const int32 value)
{
	return FString::FromInt(value);
}

UScenarioAuthoringSubsystem* UScenarioTemplateSidebarViewModel::ResolveAuthoringSubsystem() const
{
	return UiSubsystem ? UiSubsystem->ResolveAuthoringSubsystem() : nullptr;
}

TArray<UScenarioTemplateFieldRowViewModel*> UScenarioTemplateSidebarViewModel::CopyItems(
	const TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>>& source)
{
	TArray<UScenarioTemplateFieldRowViewModel*> result;
	result.Reserve(source.Num());
	for (UScenarioTemplateFieldRowViewModel* item : source)
	{
		if (item && item->IsFieldVisible())
		{
			result.Add(item);
		}
	}
	return result;
}
