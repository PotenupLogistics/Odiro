#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Components/ScenarioPathFollowerComponent.h"
#include "Scenario/Components/ScenarioPedestrianRuntimeComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Scenario/Editor/ScenarioCorridorHandleActor.h"
#include "Scenario/Editor/ScenarioCorridorPreviewActor.h"
#include "Scenario/ScenarioCompiler.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Scenario/ScenarioTemplateSampler.h"
#include "Shared/ScenarioTemplateJson.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioAuthoring, Log, All);

namespace
{
	const FString SpeedMpsKey(TEXT("speed_mps"));
	const FString SpeedCmPerSecondKey(TEXT("speed_cm_per_second"));
	const FString InitialDistanceMKey(TEXT("initial_distance_m"));
	const FString InitialDistanceCmKey(TEXT("initial_distance_cm"));
	const FString AutoStartKey(TEXT("auto_start"));
	const FString MovementModelKey(TEXT("movement_model"));
	const FString PlannedStartCmKey(TEXT("planned_start_cm"));
	const FString PlannedGoalCmKey(TEXT("planned_goal_cm"));
	const FString DefaultRobotInstanceId(TEXT("robot_01"));
	const FString DefaultRobotAssetId(TEXT("delivery_bot"));
	const FString RobotStartMarkerInstanceId(TEXT("robot_start_point"));
	const FString RobotGoalMarkerInstanceId(TEXT("robot_goal_point"));
	const FString RobotStartMarkerAssetId(TEXT("start_point"));
	const FString RobotGoalMarkerAssetId(TEXT("goal_point"));
	const FVector DefaultRobotStartLocationCm(-600.0, 0.0, 0.0);
	const FVector DefaultRobotGoalLocationCm(600.0, 0.0, 0.0);
	const FString CorridorVertexHandleIdPrefix(TEXT("corridor_vertex_"));
	const FString CorridorSegmentHandleIdPrefix(TEXT("corridor_segment_"));
	const double CorridorVertexHandleHeightCm = 32.0;
	const double CorridorSegmentHandleHeightCm = 18.0;
	const double CorridorVertexHandleScale = 0.28;
	// Static obstacle 배치는 Corridor preview surface와 같은 curb-side 하강값을 사용.
	const double CurbSideSurfaceZOffsetCm = -15.0;

	FScenarioParamValue MakeStringParamValue(const FString& value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::String;
		paramValue.StringValue = value;
		return paramValue;
	}

	FScenarioParamValue MakeBoolParamValue(bool value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Bool;
		paramValue.BoolValue = value;
		return paramValue;
	}
}

UScenarioAuthoringSubsystem::UScenarioAuthoringSubsystem()
{
	StaticObstacleClass = AScenarioStaticObstacle::StaticClass();
	PedestrianClass = AScenarioPedestrian::StaticClass();
	StaticObstaclePropCatalog = UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference();
	CorridorSurfaceCatalog = UScenarioCorridorSurfaceCatalog::MakeDefaultCatalogReference();

	static ConstructorHelpers::FClassFinder<AScenarioPedestrian> pedestrianBlueprintClass(
		TEXT("/Game/Blueprints/Scenario/BP_ScenarioPedestrian"));
	if (pedestrianBlueprintClass.Succeeded())
	{
		PedestrianClass = pedestrianBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> startPointBlueprintClass(TEXT("/Game/Blueprints/Scenario/BP_StartPoint"));
	if (startPointBlueprintClass.Succeeded())
	{
		StartPointClass = startPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> goalPointBlueprintClass(TEXT("/Game/Blueprints/Scenario/BP_GoalPoint"));
	if (goalPointBlueprintClass.Succeeded())
	{
		GoalPointClass = goalPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> pedestrianVisualizationBlueprintClass(
		TEXT("/Game/Models/Placeable/StaticMeshes/BP_PlaceablePedestrian"));
	if (pedestrianVisualizationBlueprintClass.Succeeded())
	{
		PedestrianVisualizationActorClass = pedestrianVisualizationBlueprintClass.Class;
	}
	else
	{
		PedestrianVisualizationActorClass = PedestrianClass.Get();
		UE_LOG(
			LogScenarioAuthoring,
			Warning,
			TEXT("Pedestrian visualization actor class was not found. Falling back to pedestrian class: %s"),
			PedestrianVisualizationActorClass
				? *PedestrianVisualizationActorClass->GetPathName()
				: TEXT("<null>"));
	}
}

void UScenarioAuthoringSubsystem::Deinitialize()
{
	ClearDraft();
	Super::Deinitialize();
}

void UScenarioAuthoringSubsystem::ClearDraft()
{
	ClearEditorView();
	DraftScenarioTemplate = FScenarioTemplateDocument();
	DraftGroundRegions.Reset();
	DraftPedestrianSpecs.Reset();
	SourceScenarioTemplateJsonPath.Reset();
	bDirty = false;
	NextStaticObstacleIndex = 1;
	NextPedestrianIndex = 1;
	NextGroundRegionIndex = 1;
}

void UScenarioAuthoringSubsystem::NewDraft()
{
	ClearDraft();
	InitializeDraftDefaults();
	TArray<FString> diagnostics;
	if (!RebuildEditorViewFromDraft(diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogScenarioAuthoring, Warning, TEXT("New draft editor view rebuild failed | %s"), *diagnostic);
		}
	}
}

bool UScenarioAuthoringSubsystem::LoadScenarioSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	const FString trimmedJsonFilePath = jsonFilePath.TrimStartAndEnd();
	if (trimmedJsonFilePath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioSetup JSON file path is empty."));
		return false;
	}

	outResolvedJsonFilePath = ResolveScenarioSetupLoadPath(trimmedJsonFilePath);

	const FScenarioTemplateParseResult parseResult = FScenarioTemplateJson::ParseFromFile(outResolvedJsonFilePath);
	AppendSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("ScenarioTemplate JSON import failed validation."));
		return false;
	}

	DraftScenarioTemplate = parseResult.Document;
	DraftGroundRegions.Reset();
	DraftPedestrianSpecs.Reset();
	SourceScenarioTemplateJsonPath = outResolvedJsonFilePath;
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UScenarioAuthoringSubsystem::LoadScenarioSetupJsonString(
	const FString& jsonString,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (jsonString.IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioSetup JSON string is empty."));
		return false;
	}

	const FScenarioTemplateParseResult parseResult = FScenarioTemplateJson::ParseFromString(jsonString);
	AppendSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("ScenarioTemplate JSON import failed validation."));
		return false;
	}

	DraftScenarioTemplate = parseResult.Document;
	DraftGroundRegions.Reset();
	DraftPedestrianSpecs.Reset();
	SourceScenarioTemplateJsonPath.Reset();
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UScenarioAuthoringSubsystem::ImportCompiledWorldSpec(
	const FScenarioWorldSpec& worldSpec,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	ImportWorldSpecAsScenarioTemplate(worldSpec);
	SourceScenarioTemplateJsonPath.Reset();
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UScenarioAuthoringSubsystem::RefreshEditorPreviewFromDraft(TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
		return RebuildEditorViewFromDraft(outDiagnostics);
	}

	return RefreshGeneratedEditorPreviewActorsFromDraft(outDiagnostics);
}

void UScenarioAuthoringSubsystem::GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();

	const UScenarioStaticObstaclePropCatalog* propCatalog = GetStaticObstaclePropCatalog();
	if (!propCatalog) return;

	outEntries = propCatalog->GetEntries();
}

bool UScenarioAuthoringSubsystem::TryGetStaticObstaclePropEntry(
	FName propId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	return TryFindStaticObstacleProp(propId, outPropEntry);
}

void UScenarioAuthoringSubsystem::GetCorridorSurfaceEntries(TArray<FScenarioCorridorSurfaceEntry>& outEntries) const
{
	outEntries.Reset();

	TSet<FName> seenSurfaceIds;
	if (const UScenarioCorridorSurfaceCatalog* surfaceCatalog = GetCorridorSurfaceCatalog())
	{
		for (const FScenarioCorridorSurfaceEntry& entry : surfaceCatalog->GetEntries())
		{
			if (!entry.SurfaceId.IsNone() && !seenSurfaceIds.Contains(entry.SurfaceId))
			{
				FScenarioCorridorSurfaceEntry resolvedEntry;
				if (surfaceCatalog->FindSurfaceEntryById(entry.SurfaceId, resolvedEntry))
				{
					outEntries.Add(resolvedEntry);
				}
				else
				{
					outEntries.Add(entry);
				}
				seenSurfaceIds.Add(entry.SurfaceId);
			}
		}
	}

	for (const FScenarioCorridorSurfaceEntry& entry : UScenarioCorridorSurfaceCatalog::MakeDefaultEntries())
	{
		if (!entry.SurfaceId.IsNone() && !seenSurfaceIds.Contains(entry.SurfaceId))
		{
			outEntries.Add(entry);
			seenSurfaceIds.Add(entry.SurfaceId);
		}
	}
}

bool UScenarioAuthoringSubsystem::TryGetCorridorSurfaceEntry(
	FName surfaceId,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry) const
{
	return TryFindCorridorSurfaceEntry(surfaceId, outSurfaceEntry);
}

FScenarioTemplateNumberValue UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(double value)
{
	return MakeFixedTemplateNumber(value);
}

FScenarioTemplateNumberValue UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(double minValue, double maxValue)
{
	return MakeRangeTemplateNumber(minValue, maxValue);
}

FScenarioTemplateIntegerValue UScenarioAuthoringSubsystem::MakeRangeTemplateIntegerValue(
	const int32 minValue,
	const int32 maxValue)
{
	return MakeRangeTemplateInteger(minValue, maxValue);
}

FScenarioTemplateIntegerValue UScenarioAuthoringSubsystem::MakeFixedTemplateIntegerValue(const int32 value)
{
	return MakeFixedTemplateInteger(value);
}

double UScenarioAuthoringSubsystem::GetDraftCorridorAxisLengthMeters() const
{
	return MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);
}

bool UScenarioAuthoringSubsystem::SetDraftTemplateId(
	const FString& templateId,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	const FString normalizedTemplateId = templateId.TrimStartAndEnd();
	if (normalizedTemplateId.IsEmpty())
	{
		outDiagnostics.Add(TEXT("scenario_template template_id must not be empty."));
		return false;
	}

	if (!IsDraftScenarioTemplateEmpty() && DraftScenarioTemplate.TemplateId == normalizedTemplateId)
	{
		return true;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	DraftScenarioTemplate.TemplateId = normalizedTemplateId;
	return CommitTemplateMetadataDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetDraftIntent(
	const FString& intent,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	const FString normalizedIntent = intent.TrimStartAndEnd();
	if (normalizedIntent.IsEmpty())
	{
		outDiagnostics.Add(TEXT("scenario_template intent must not be empty."));
		return false;
	}

	if (!IsDraftScenarioTemplateEmpty() && DraftScenarioTemplate.Intent == normalizedIntent)
	{
		return true;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	DraftScenarioTemplate.Intent = normalizedIntent;
	return CommitTemplateMetadataDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetCorridorAxisPointsMeters(
	const TArray<FVector2D>& pointsMeters,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	FString failureReason;
	if (!AreCorridorAxisPointsValid(pointsMeters, failureReason))
	{
		outDiagnostics.Add(failureReason);
		return false;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const double oldLengthMeters = MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);
	DraftScenarioTemplate.Corridor.Axis.Type = EScenarioCorridorAxisType::Polyline;
	DraftScenarioTemplate.Corridor.Axis.PointsMeters = pointsMeters;
	const double newLengthMeters = MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);

	RescaleCorridorSegmentsForAxisLength(oldLengthMeters, newLengthMeters);
	RescaleCorridorAlongReferences(oldLengthMeters, newLengthMeters);
	RepairCorridorReferenceSegmentIds();
	return CommitCorridorDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetCorridorWalkwayWidthMeters(
	const FScenarioTemplateNumberValue& widthMeters,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (!IsPositiveTemplateNumber(widthMeters))
	{
		outDiagnostics.Add(TEXT("Corridor walkway width must be a positive fixed value or positive min/max range."));
		return false;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	DraftScenarioTemplate.Corridor.WalkwayWidthMeters = widthMeters;
	return CommitCorridorDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetCorridorSideLaneProfile(
	EScenarioEditorCorridorSide side,
	const TArray<FScenarioTemplateLaneRule>& lanes,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	const FString path = side == EScenarioEditorCorridorSide::Building
		? TEXT("$.corridor.building_side")
		: TEXT("$.corridor.curb_side");
	if (!ValidateCorridorLaneProfile(lanes, path, outDiagnostics))
	{
		return false;
	}

	TArray<FScenarioTemplateLaneRule> normalizedLanes = lanes;
	for (FScenarioTemplateLaneRule& lane : normalizedLanes)
	{
		lane.SurfaceId = lane.SurfaceId.TrimStartAndEnd();
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	if (side == EScenarioEditorCorridorSide::Building)
	{
		DraftScenarioTemplate.Corridor.BuildingSide = normalizedLanes;
	}
	else
	{
		DraftScenarioTemplate.Corridor.CurbSide = normalizedLanes;
	}

	return CommitCorridorDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetCorridorSegments(
	const TArray<FScenarioTemplateSegment>& segments,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const double axisLengthMeters = MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);
	TArray<FScenarioTemplateSegment> normalizedSegments = segments;
	for (FScenarioTemplateSegment& segment : normalizedSegments)
	{
		segment.SegmentId = segment.SegmentId.TrimStartAndEnd();
		if (segment.ReplacedBySurfaceId.bIsSet)
		{
			if (segment.ReplacedBySurfaceId.Mode == EScenarioTemplateStringValueMode::Choices)
			{
				for (FString& choice : segment.ReplacedBySurfaceId.Choices)
				{
					choice = choice.TrimStartAndEnd();
				}
			}
			else
			{
				segment.ReplacedBySurfaceId.FixedValue = segment.ReplacedBySurfaceId.FixedValue.TrimStartAndEnd();
			}
		}
	}

	if (!ValidateCorridorSegments(normalizedSegments, axisLengthMeters, outDiagnostics))
	{
		DraftScenarioTemplate = previousTemplate;
		bDirty = bPreviousDirty;
		return false;
	}

	DraftScenarioTemplate.Corridor.Segments = normalizedSegments;
	RepairCorridorReferenceSegmentIds();
	return CommitCorridorDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetObstacleMinClearWidthMeters(
	const FScenarioTemplateNumberValue& widthMeters,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (!IsPositiveTemplateNumber(widthMeters))
	{
		outDiagnostics.Add(TEXT("Obstacle min_clear_width_m must be a positive fixed value or positive min/max range."));
		return false;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	DraftScenarioTemplate.Obstacles.MinClearWidthMeters = widthMeters;
	return CommitObstacleDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::SetObstaclePlacements(
	const TArray<FScenarioTemplateObstaclePlacement>& placements,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	TArray<FScenarioTemplateObstaclePlacement> normalizedPlacements = placements;
	for (FScenarioTemplateObstaclePlacement& placement : normalizedPlacements)
	{
		placement.PlacementId = placement.PlacementId.TrimStartAndEnd();
		placement.PropId = placement.PropId.TrimStartAndEnd();
		placement.PatternId = placement.PatternId.TrimStartAndEnd();
		placement.At.SegmentId = placement.At.SegmentId.TrimStartAndEnd();
		placement.At.LaneId = placement.At.LaneId.TrimStartAndEnd();
		for (FString& segmentId : placement.Zone.SegmentIds)
		{
			segmentId = segmentId.TrimStartAndEnd();
		}
		for (FString& laneId : placement.Zone.LaneIds)
		{
			laneId = laneId.TrimStartAndEnd();
		}
		for (FString& categoryId : placement.Palette.CategoryIds)
		{
			categoryId = categoryId.TrimStartAndEnd();
		}
		for (FString& classId : placement.Palette.ClassIds)
		{
			classId = classId.TrimStartAndEnd();
		}
	}

	if (!ValidateObstaclePlacements(normalizedPlacements, outDiagnostics))
	{
		return false;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	DraftScenarioTemplate.Obstacles.Placements = normalizedPlacements;
	RepairCorridorReferenceSegmentIds();
	return CommitObstacleDraftEdit(previousTemplate, bPreviousDirty, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::UpdateCorridorVertexHandleTransform(
	const FString& handleId,
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	int32 vertexIndex = INDEX_NONE;
	if (!TryParseCorridorVertexHandleId(handleId, vertexIndex))
	{
		outFailureReason = TEXT("Invalid corridor vertex handle id.");
		return false;
	}

	TArray<FVector2D> pointsMeters = DraftScenarioTemplate.Corridor.Axis.PointsMeters;
	if (!pointsMeters.IsValidIndex(vertexIndex))
	{
		outFailureReason = FString::Printf(TEXT("Corridor vertex handle index %d is out of range."), vertexIndex);
		return false;
	}

	const FVector locationCm = transform.GetLocation();
	pointsMeters[vertexIndex] = FVector2D(locationCm.X * CentimetersToMeters, locationCm.Y * CentimetersToMeters);
	return ApplyCorridorAxisPointsEdit(pointsMeters, false, outFailureReason);
}

bool UScenarioAuthoringSubsystem::UpdateCorridorSegmentHandleTransform(
	const FString& handleId,
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	int32 segmentIndex = INDEX_NONE;
	if (!TryParseCorridorSegmentHandleId(handleId, segmentIndex))
	{
		outFailureReason = TEXT("Invalid corridor segment handle id.");
		return false;
	}

	TArray<FVector2D> pointsMeters = DraftScenarioTemplate.Corridor.Axis.PointsMeters;
	if (!pointsMeters.IsValidIndex(segmentIndex) || !pointsMeters.IsValidIndex(segmentIndex + 1))
	{
		outFailureReason = FString::Printf(TEXT("Corridor segment handle index %d is out of range."), segmentIndex);
		return false;
	}

	const FTransform currentHandleTransform = MakeCorridorSegmentHandleTransform(
		pointsMeters[segmentIndex],
		pointsMeters[segmentIndex + 1]);
	const FVector currentLocationCm = currentHandleTransform.GetLocation();
	const FVector requestedLocationCm = transform.GetLocation();
	const FVector2D centerMeters(currentLocationCm.X * CentimetersToMeters, currentLocationCm.Y * CentimetersToMeters);
	const FVector2D deltaMeters(
		(requestedLocationCm.X - currentLocationCm.X) * CentimetersToMeters,
		(requestedLocationCm.Y - currentLocationCm.Y) * CentimetersToMeters);
	const double currentYawDegrees = currentHandleTransform.GetRotation().Rotator().Yaw;
	const double requestedYawDegrees = transform.GetRotation().Rotator().Yaw;
	const double deltaYawRadians = FMath::DegreesToRadians(
		FMath::FindDeltaAngleDegrees(currentYawDegrees, requestedYawDegrees));
	const double cosYaw = FMath::Cos(deltaYawRadians);
	const double sinYaw = FMath::Sin(deltaYawRadians);

	auto transformPoint = [centerMeters, deltaMeters, cosYaw, sinYaw](const FVector2D& pointMeters)
	{
		const FVector2D localPoint = pointMeters - centerMeters;
		const FVector2D rotatedPoint(
			localPoint.X * cosYaw - localPoint.Y * sinYaw,
			localPoint.X * sinYaw + localPoint.Y * cosYaw);
		return centerMeters + rotatedPoint + deltaMeters;
	};

	pointsMeters[segmentIndex] = transformPoint(pointsMeters[segmentIndex]);
	pointsMeters[segmentIndex + 1] = transformPoint(pointsMeters[segmentIndex + 1]);
	return ApplyCorridorAxisPointsEdit(pointsMeters, false, outFailureReason);
}

void UScenarioAuthoringSubsystem::GetAuthoredStaticObstacleActors(TArray<AScenarioStaticObstacle*>& outActors) const
{
	outActors.Reset();
	outActors.Reserve(StaticObstacleActors.Num());

	for (const TPair<FString, TObjectPtr<AScenarioStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (AScenarioStaticObstacle* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}
}

void UScenarioAuthoringSubsystem::GetEditorPlacementIgnoredActors(TArray<AActor*>& outActors) const
{
	outActors.Reset();
	outActors.Reserve(StaticObstacleActors.Num() + PedestrianActors.Num() + RouteMarkerActors.Num());

	for (const TPair<FString, TObjectPtr<AScenarioStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (AActor* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}

	for (const TPair<FString, TObjectPtr<AActor>>& pair : PedestrianActors)
	{
		if (AActor* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}

	for (const TObjectPtr<AActor>& markerActor : RouteMarkerActors)
	{
		if (AActor* actor = markerActor.Get())
		{
			outActors.Add(actor);
		}
	}

	if (AActor* actor = CorridorPreviewActor.Get())
	{
		outActors.Add(actor);
	}
}

bool UScenarioAuthoringSubsystem::CanPlaceStaticObstacle(
	FName propId,
	const FTransform& transform,
	FString& outFailureReason) const
{
	return CanPlaceStaticObstacleInternal(propId, transform, FString(), outFailureReason);
}

FTransform UScenarioAuthoringSubsystem::ResolveStaticObstaclePlacementTransform(const FTransform& transform) const
{
	FTransform resolvedTransform = transform;
	FVector locationCm = transform.GetLocation();
	double surfaceZOffsetCm = 0.0;
	if (TryResolveCorridorSurfaceZOffsetCm(locationCm, surfaceZOffsetCm))
	{
		locationCm.Z = surfaceZOffsetCm;
		resolvedTransform.SetLocation(locationCm);
	}
	return resolvedTransform;
}

bool UScenarioAuthoringSubsystem::CanPlaceEditorGroundActor(
	const FTransform& transform,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	const double locationZ = transform.GetLocation().Z;
	if (locationZ < -KINDA_SMALL_NUMBER)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be 0.00 cm or higher. Current Z: %.2f."),
			locationZ);
		return false;
	}
	if (locationZ > StaticObstacleGroundZToleranceCm)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be %.2f cm or lower. Current Z: %.2f."),
			StaticObstacleGroundZToleranceCm,
			locationZ);
		return false;
	}

	return true;
}

bool UScenarioAuthoringSubsystem::CanUpdateStaticObstacleTransform(
	const FString& instanceId,
	const FTransform& transform,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	if (instanceId.IsEmpty())
	{
		outFailureReason = TEXT("Static obstacle instance id is empty.");
		return false;
	}

	const FScenarioTemplateObstaclePlacement* placement = FindStaticObstaclePlacementByInstanceId(instanceId);
	if (!placement)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle spec '%s' was not found."), *instanceId);
		return false;
	}

	if (!FindStaticObstacleRecordByInstanceId(instanceId))
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle record '%s' was not found."), *instanceId);
		return false;
	}

	const TObjectPtr<AScenarioStaticObstacle>* actorPtr = StaticObstacleActors.Find(instanceId);
	if (!actorPtr || !actorPtr->Get())
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle actor '%s' was not found."), *instanceId);
		return false;
	}

	return CanPlaceStaticObstacleInternal(FName(*placement->PropId), transform, instanceId, outFailureReason);
}

bool UScenarioAuthoringSubsystem::CanPlaceStaticObstacleInternal(
	FName propId,
	const FTransform& transform,
	const FString& ignoredInstanceId,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	FScenarioStaticObstaclePropEntry candidateProp;
	if (!TryFindStaticObstacleProp(propId, candidateProp))
	{
		outFailureReason = FString::Printf(TEXT("Unknown static obstacle prop '%s'."), *propId.ToString());
		return false;
	}

	const FVector2D candidateHalfExtent = ComputePlacementHalfExtent2D(candidateProp);
	const FTransform candidateTransform = ResolveStaticObstaclePlacementTransform(transform);
	const FVector candidateLocation = candidateTransform.GetLocation();
	double surfaceZOffsetCm = 0.0;
	if (!TryResolveCorridorSurfaceZOffsetCm(transform.GetLocation(), surfaceZOffsetCm)
		&& (candidateLocation.Z < -KINDA_SMALL_NUMBER || candidateLocation.Z > StaticObstacleGroundZToleranceCm))
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be between 0.00 cm and %.2f cm. Current Z: %.2f."),
			StaticObstacleGroundZToleranceCm,
			candidateLocation.Z);
		return false;
	}

	for (const FScenarioAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (!ignoredInstanceId.IsEmpty() && record.InstanceId == ignoredInstanceId)
		{
			continue;
		}

		if (StaticObstacleFootprintsOverlap(candidateLocation, candidateHalfExtent, record))
		{
			outFailureReason = FString::Printf(
				TEXT("Overlaps static obstacle '%s'."), *record.InstanceId);
			return false;
		}
	}

	return true;
}

bool UScenarioAuthoringSubsystem::AddStaticObstacle(
	FName propId,
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec)
{
	AScenarioStaticObstacle* spawnedActor = nullptr;
	return AddStaticObstacleInternal(propId, transform, outSpec, spawnedActor);
}

bool UScenarioAuthoringSubsystem::AddPedestrian(
	FName archetypeId,
	const FTransform& transform,
	FScenarioDynamicActorSpec& outSpec,
	AActor*& outActor,
	FString& outFailureReason)
{
	outSpec = FScenarioDynamicActorSpec();
	outActor = nullptr;
	outFailureReason.Reset();

	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString instanceId = GeneratePedestrianInstanceId();
	outSpec = MakePedestrianSpec(instanceId, archetypeId, transform);

	if (!SpawnEditorPedestrianActor(outSpec, outActor, outFailureReason))
	{
		return false;
	}

	DraftPedestrianSpecs.Add(outSpec);
	AddPedestrianViewRecord(outSpec, outActor);
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Added pedestrian | InstanceId: %s | AssetId: %s | Location: %s"),
		*outSpec.InstanceId,
		*outSpec.AssetId,
		*transform.GetLocation().ToCompactString());

	return true;
}

bool UScenarioAuthoringSubsystem::SetRobotStartLocation(
	FName assetId,
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec,
	AActor*& outMarker,
	FString& outFailureReason)
{
	outSpec = FScenarioPlaceableInstanceSpec();
	outMarker = nullptr;
	outFailureReason.Reset();

	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	outMarker = SpawnOrReplaceRouteMarker(
		RobotStartMarkerActor,
		StartPointClass,
		transform,
		EScenarioPlaceableAuthoringRole::RobotStartMarker,
		outFailureReason);
	if (!outMarker)
	{
		return false;
	}

	DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(transform.GetLocation());
	outSpec = MakeDeliveryBotSpecFromTemplateRobot();
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Set robot start | InstanceId: %s | Location: %s"),
		*outSpec.InstanceId,
		*transform.GetLocation().ToCompactString());

	return true;
}

bool UScenarioAuthoringSubsystem::SetRobotGoalLocation(
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec,
	AActor*& outMarker,
	FString& outFailureReason)
{
	outSpec = FScenarioPlaceableInstanceSpec();
	outMarker = nullptr;
	outFailureReason.Reset();

	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		outFailureReason = TEXT("Robot start point must be placed before a goal point.");
		return false;
	}

	outMarker = SpawnOrReplaceRouteMarker(
		RobotGoalMarkerActor,
		GoalPointClass,
		transform,
		EScenarioPlaceableAuthoringRole::RobotGoalMarker,
		outFailureReason);
	if (!outMarker)
	{
		return false;
	}

	DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(transform.GetLocation());
	outSpec = MakeDeliveryBotSpecFromTemplateRobot();
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Set robot goal | InstanceId: %s | Location: %s"),
		*outSpec.InstanceId,
		*transform.GetLocation().ToCompactString());

	return true;
}

bool UScenarioAuthoringSubsystem::UpdateStaticObstacleTransform(
	const FString& instanceId,
	const FTransform& transform,
	FString& outFailureReason)
{
	const FTransform resolvedTransform = ResolveStaticObstaclePlacementTransform(transform);
	if (!CanUpdateStaticObstacleTransform(instanceId, resolvedTransform, outFailureReason))
	{
		return false;
	}

	FScenarioTemplateObstaclePlacement* placement = FindStaticObstaclePlacementByInstanceId(instanceId);
	FScenarioAuthoringStaticObstacleRecord* record = FindStaticObstacleRecordByInstanceId(instanceId);
	if (!placement || !record)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle '%s' is not editable."), *instanceId);
		return false;
	}

	TObjectPtr<AScenarioStaticObstacle>* actorPtr = StaticObstacleActors.Find(instanceId);
	AScenarioStaticObstacle* actor = actorPtr ? actorPtr->Get() : nullptr;
	if (!actor)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle actor '%s' was not found."), *instanceId);
		return false;
	}

	const FName propId(*placement->PropId);
	const bool bAllowBlocking = placement->bAllowBlocking;
	*placement = MakeStaticObstaclePlacement(instanceId, propId, resolvedTransform);
	placement->bAllowBlocking = bAllowBlocking;
	record->Transform = resolvedTransform;
	actor->SetActorTransform(resolvedTransform, false, nullptr, ETeleportType::TeleportPhysics);

	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Verbose,
		TEXT("Updated static obstacle transform | InstanceId: %s, Location: %s, Yaw: %.2f"),
		*instanceId,
		*resolvedTransform.GetLocation().ToCompactString(),
		resolvedTransform.Rotator().Yaw);

	return true;
}

bool UScenarioAuthoringSubsystem::UpdateRobotStartPointTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		outFailureReason = TEXT("Robot route points are not initialized.");
		return false;
	}
	if (!IsValid(RobotStartMarkerActor))
	{
		outFailureReason = TEXT("Robot start marker actor was not found.");
		return false;
	}

	const FVector location = transform.GetLocation();
	DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(location);
	RobotStartMarkerActor->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);

	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::UpdateRobotGoalPointTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		outFailureReason = TEXT("Robot route points are not initialized.");
		return false;
	}
	if (!IsValid(RobotGoalMarkerActor))
	{
		outFailureReason = TEXT("Robot goal marker actor was not found.");
		return false;
	}

	const FVector location = transform.GetLocation();
	DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(location);
	RobotGoalMarkerActor->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);

	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::RenameStaticObstacleInstanceId(
	const FString& oldInstanceId,
	const FString& newInstanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	const FString trimmedNewInstanceId = newInstanceId.TrimStartAndEnd();
	if (oldInstanceId.IsEmpty())
	{
		outFailureReason = TEXT("Static obstacle instance id is empty.");
		return false;
	}
	if (trimmedNewInstanceId.IsEmpty())
	{
		outFailureReason = TEXT("New static obstacle instance id is empty.");
		return false;
	}
	if (oldInstanceId == trimmedNewInstanceId)
	{
		return true;
	}
	if (ContainsInstanceId(trimmedNewInstanceId))
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle instance id '%s' already exists."), *trimmedNewInstanceId);
		return false;
	}

	FScenarioTemplateObstaclePlacement* placement = FindStaticObstaclePlacementByInstanceId(oldInstanceId);
	FScenarioAuthoringStaticObstacleRecord* record = FindStaticObstacleRecordByInstanceId(oldInstanceId);
	if (!placement || !record)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle '%s' is not editable."), *oldInstanceId);
		return false;
	}

	TObjectPtr<AScenarioStaticObstacle>* actorPtr = StaticObstacleActors.Find(oldInstanceId);
	if (!actorPtr || !actorPtr->Get())
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle actor '%s' was not found."), *oldInstanceId);
		return false;
	}
	AScenarioStaticObstacle* actor = actorPtr->Get();

	placement->PlacementId = trimmedNewInstanceId;
	record->InstanceId = trimmedNewInstanceId;
	StaticObstacleActors.Remove(oldInstanceId);
	TObjectPtr<AScenarioStaticObstacle> renamedActorPtr = actor;
	StaticObstacleActors.Add(trimmedNewInstanceId, renamedActorPtr);

	if (UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = trimmedNewInstanceId;
	}

	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Renamed static obstacle instance | OldInstanceId: %s | NewInstanceId: %s"),
		*oldInstanceId,
		*trimmedNewInstanceId);

	return true;
}

bool UScenarioAuthoringSubsystem::RemoveStaticObstacle(
	const FString& instanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	if (instanceId.IsEmpty())
	{
		outFailureReason = TEXT("Static obstacle instance id is empty.");
		return false;
	}

	const int32 removedSpecCount = DraftScenarioTemplate.Obstacles.Placements.RemoveAll(
		[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
		{
			return placement.PlacementId == instanceId;
		});
	if (removedSpecCount <= 0)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle spec '%s' was not found."), *instanceId);
		return false;
	}

	StaticObstacleRecords.RemoveAll(
		[&instanceId](const FScenarioAuthoringStaticObstacleRecord& record)
		{
			return record.InstanceId == instanceId;
		});

	TObjectPtr<AScenarioStaticObstacle> actorPtr;
	StaticObstacleActors.RemoveAndCopyValue(instanceId, actorPtr);
	if (AScenarioStaticObstacle* actor = actorPtr.Get())
	{
		actor->Destroy();
	}

	bDirty = true;

	UE_LOG(LogScenarioAuthoring, Log, TEXT("Removed static obstacle | InstanceId: %s"), *instanceId);
	return true;
}

bool UScenarioAuthoringSubsystem::AddGroundRegion(
	EScenarioGroundRegionType regionType,
	const FVector& centerCm,
	const FVector2D& sizeCm,
	double yawDegrees,
	FScenarioGroundRegionSpec& outSpec,
	FString& outFailureReason)
{
	outSpec = FScenarioGroundRegionSpec();
	outFailureReason.Reset();

	if (sizeCm.X <= KINDA_SMALL_NUMBER || sizeCm.Y <= KINDA_SMALL_NUMBER)
	{
		outFailureReason = TEXT("Ground region size must be positive.");
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString regionId = GenerateGroundRegionId();
	outSpec = MakeGroundRegionSpec(regionId, regionType, centerCm, sizeCm, yawDegrees);

	AScenarioGroundRegion* spawnedActor = nullptr;
	if (!SpawnEditorGroundRegionActor(outSpec, spawnedActor, outFailureReason))
	{
		outSpec = FScenarioGroundRegionSpec();
		return false;
	}

	DraftGroundRegions.Add(outSpec);
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Added ground region | RegionId: %s | Type: %s | Center: %s | Size: %s"),
		*outSpec.RegionId,
		*GroundRegionTypeToString(outSpec.RegionType),
		*centerCm.ToCompactString(),
		*sizeCm.ToString());

	return true;
}

bool UScenarioAuthoringSubsystem::UpdateGroundRegionTransform(
	const FString& regionId,
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	if (regionId.IsEmpty())
	{
		outFailureReason = TEXT("Ground region id is empty.");
		return false;
	}

	FScenarioGroundRegionSpec* regionSpec = DraftGroundRegions.FindByPredicate(
		[&regionId](const FScenarioGroundRegionSpec& spec)
		{
			return spec.RegionId == regionId;
		});
	if (!regionSpec)
	{
		outFailureReason = FString::Printf(TEXT("Ground region spec '%s' was not found."), *regionId);
		return false;
	}

	// 이동(Center)과 yaw 회전만 반영하고 Size는 보존함.
	regionSpec->Center = transform.GetLocation();
	regionSpec->YawDegrees = transform.GetRotation().Rotator().Yaw;

	if (const TObjectPtr<AScenarioGroundRegion>* actorPtr = GroundRegionActors.Find(regionId))
	{
		if (AScenarioGroundRegion* actor = actorPtr->Get())
		{
			actor->ConfigureRegion(*regionSpec);
		}
	}

	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::RemoveGroundRegion(
	const FString& regionId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	if (regionId.IsEmpty())
	{
		outFailureReason = TEXT("Ground region id is empty.");
		return false;
	}

	const int32 removedSpecCount = DraftGroundRegions.RemoveAll(
		[&regionId](const FScenarioGroundRegionSpec& spec)
		{
			return spec.RegionId == regionId;
		});
	if (removedSpecCount <= 0)
	{
		outFailureReason = FString::Printf(TEXT("Ground region spec '%s' was not found."), *regionId);
		return false;
	}

	TObjectPtr<AScenarioGroundRegion> actorPtr;
	GroundRegionActors.RemoveAndCopyValue(regionId, actorPtr);
	if (AScenarioGroundRegion* actor = actorPtr.Get())
	{
		actor->Destroy();
	}

	bDirty = true;

	UE_LOG(LogScenarioAuthoring, Log, TEXT("Removed ground region | RegionId: %s"), *regionId);
	return true;
}

FScenarioGroundRegionSpec UScenarioAuthoringSubsystem::MakeGroundRegionSpec(
	const FString& regionId,
	EScenarioGroundRegionType regionType,
	const FVector& centerCm,
	const FVector2D& sizeCm,
	double yawDegrees) const
{
	FScenarioGroundRegionSpec spec;
	spec.RegionId = regionId;
	spec.RegionType = regionType;
	spec.ShapeType = EScenarioGroundShapeType::Rectangle;
	spec.Center = centerCm;
	spec.Size = sizeCm;
	spec.YawDegrees = yawDegrees;

	switch (regionType)
	{
	case EScenarioGroundRegionType::Walkable:
		spec.TraversabilityScore = 1.0;
		break;
	case EScenarioGroundRegionType::Penalty:
		spec.TraversabilityScore = 0.5;
		break;
	case EScenarioGroundRegionType::Blocked:
		spec.TraversabilityScore = 0.0;
		break;
	default:
		break;
	}

	return spec;
}

FString UScenarioAuthoringSubsystem::GenerateGroundRegionId()
{
	FString regionId;
	do
	{
		regionId = FString::Printf(TEXT("region_%03d"), NextGroundRegionIndex++);
	}
	while (ContainsGroundRegionId(regionId));

	return regionId;
}

bool UScenarioAuthoringSubsystem::ContainsGroundRegionId(const FString& regionId) const
{
	for (const FScenarioGroundRegionSpec& spec : DraftGroundRegions)
	{
		if (spec.RegionId == regionId)
		{
			return true;
		}
	}

	return false;
}

bool UScenarioAuthoringSubsystem::SpawnEditorGroundRegionActor(
	const FScenarioGroundRegionSpec& spec,
	AScenarioGroundRegion*& outActor,
	FString& outFailureReason)
{
	outActor = nullptr;
	outFailureReason.Reset();

	UWorld* world = GetWorld();
	if (!world)
	{
		outFailureReason = TEXT("World is unavailable.");
		return false;
	}

	if (spec.RegionId.IsEmpty())
	{
		outFailureReason = TEXT("RegionId is empty.");
		return false;
	}

	TSubclassOf<AScenarioGroundRegion> spawnClass = GroundRegionClass;
	if (!spawnClass)
	{
		spawnClass = AScenarioGroundRegion::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioGroundRegion* regionActor = world->SpawnActor<AScenarioGroundRegion>(
		spawnClass,
		FTransform::Identity,
		spawnParams);
	if (!regionActor)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return false;
	}

	regionActor->ConfigureRegion(spec);
	GroundRegionActors.Add(spec.RegionId, regionActor);
	outActor = regionActor;
	return true;
}

bool UScenarioAuthoringSubsystem::AddStaticObstacleInternal(
	FName propId,
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec,
	AScenarioStaticObstacle*& outActor)
{
	outActor = nullptr;
	outSpec = FScenarioPlaceableInstanceSpec();

	const FTransform resolvedTransform = ResolveStaticObstaclePlacementTransform(transform);
	FString failureReason;
	if (!CanPlaceStaticObstacle(propId, resolvedTransform, failureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString instanceId = GenerateStaticObstacleInstanceId();
	outSpec = MakeStaticObstacleSpec(instanceId, propId, resolvedTransform);
	const FScenarioTemplateObstaclePlacement placement = MakeStaticObstaclePlacement(instanceId, propId, resolvedTransform);

	if (!SpawnEditorStaticObstacleActor(outSpec, outActor, failureReason))
	{
		return false;
	}

	FScenarioStaticObstaclePropEntry propEntry;
	TryFindStaticObstacleProp(propId, propEntry);
	AddStaticObstacleViewRecord(outSpec, propEntry, outActor);
	DraftScenarioTemplate.Obstacles.Placements.Add(placement);
	bDirty = true;
	return true;
}

TArray<FScenarioPlaceableInstanceSpec> UScenarioAuthoringSubsystem::GetAuthoredStaticObstacleSpecs() const
{
	TArray<FScenarioPlaceableInstanceSpec> staticObstacleSpecs;
	for (const FScenarioTemplateObstaclePlacement& placement : DraftScenarioTemplate.Obstacles.Placements)
	{
		if (placement.Kind != EScenarioTemplateObstaclePlacementKind::Fixed || placement.PropId.IsEmpty())
		{
			continue;
		}
		staticObstacleSpecs.Add(MakeStaticObstacleSpecFromPlacement(placement));
	}

	return staticObstacleSpecs;
}

bool UScenarioAuthoringSubsystem::ExportScenarioSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();
	if (!ValidateSingleRobotRouteSpecForExport(outDiagnostics))
	{
		return false;
	}

	TArray<FScenarioSchemaDiagnostic> schemaDiagnostics;
	const bool bWritten = FScenarioTemplateJson::TryWriteJson(DraftScenarioTemplate, outJsonString, schemaDiagnostics);
	AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);
	return bWritten;
}

bool UScenarioAuthoringSubsystem::ExportAndValidateScenarioSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	if (!ExportScenarioSetupJsonString(outJsonString, outDiagnostics))
	{
		return false;
	}

	FScenarioTemplateParseResult parseResult = FScenarioTemplateJson::ParseFromString(outJsonString);
	AppendSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("Exported ScenarioTemplate JSON failed validation."));
	}

	return parseResult.bSuccess;
}

bool UScenarioAuthoringSubsystem::SaveScenarioSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (jsonFilePath.IsEmpty())
	{
		outResolvedJsonFilePath.Reset();
		outDiagnostics.Add(TEXT("ScenarioSetup JSON file path is empty."));
		return false;
	}

	outResolvedJsonFilePath = ResolveProjectRelativePath(jsonFilePath);

	FString jsonString;
	if (!ExportAndValidateScenarioSetupJsonString(jsonString, outDiagnostics))
	{
		return false;
	}

	const FString directory = FPaths::GetPath(outResolvedJsonFilePath);
	if (!directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*directory, true);
	}

	if (!FFileHelper::SaveStringToFile(jsonString, *outResolvedJsonFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Failed to save ScenarioSetup JSON to '%s'."), *outResolvedJsonFilePath));
		return false;
	}

	SourceScenarioTemplateJsonPath = outResolvedJsonFilePath;
	bDirty = false;
	return true;
}

FString UScenarioAuthoringSubsystem::ResolveProjectRelativePath(const FString& filePath)
{
	if (FPaths::IsRelative(filePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), filePath);
	}

	return filePath;
}

FString UScenarioAuthoringSubsystem::ResolveScenarioSetupLoadPath(const FString& filePath) const
{
	FString normalizedPath = filePath;
	normalizedPath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedPath);

	if (FPaths::GetExtension(normalizedPath).IsEmpty())
	{
		normalizedPath = FPaths::SetExtension(normalizedPath, TEXT("json"));
	}

	if (!FPaths::IsRelative(normalizedPath))
	{
		return normalizedPath;
	}

	if (FPaths::GetPath(normalizedPath).IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), ScenarioSetupInputDirectory, normalizedPath));
	}

	return ResolveProjectRelativePath(normalizedPath);
}

FString UScenarioAuthoringSubsystem::CompileSeverityToString(EScenarioCompileDiagnosticSeverity severity)
{
	switch (severity)
	{
	case EScenarioCompileDiagnosticSeverity::Info:
		return TEXT("Info");
	case EScenarioCompileDiagnosticSeverity::Warning:
		return TEXT("Warning");
	case EScenarioCompileDiagnosticSeverity::Error:
		return TEXT("Error");
	default:
		return TEXT("Unknown");
	}
}

void UScenarioAuthoringSubsystem::AppendCompileDiagnostics(
	const FScenarioCompileResult& compileResult,
	TArray<FString>& outDiagnostics)
{
	for (const FScenarioCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("[%s] %s: %s"),
			*CompileSeverityToString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

FString UScenarioAuthoringSubsystem::GroundRegionTypeToString(EScenarioGroundRegionType regionType)
{
	switch (regionType)
	{
	case EScenarioGroundRegionType::Walkable:
		return TEXT("walkable");
	case EScenarioGroundRegionType::Penalty:
		return TEXT("penalty");
	case EScenarioGroundRegionType::Blocked:
		return TEXT("blocked");
	default:
		return TEXT("walkable");
	}
}

FString UScenarioAuthoringSubsystem::GroundShapeTypeToString(EScenarioGroundShapeType shapeType)
{
	switch (shapeType)
	{
	case EScenarioGroundShapeType::Rectangle:
		return TEXT("rectangle");
	case EScenarioGroundShapeType::ConvexPolygon:
		return TEXT("convex_polygon");
	default:
		return TEXT("rectangle");
	}
}

TArray<TSharedPtr<FJsonValue>> UScenarioAuthoringSubsystem::MakeXyArrayMeters(const FVector& locationCm)
{
	TArray<TSharedPtr<FJsonValue>> xyValues;
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.X * CentimetersToMeters));
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.Y * CentimetersToMeters));
	return xyValues;
}

TArray<TSharedPtr<FJsonValue>> UScenarioAuthoringSubsystem::MakeSizeArrayMeters(const FVector2D& sizeCm)
{
	TArray<TSharedPtr<FJsonValue>> sizeValues;
	sizeValues.Add(MakeShared<FJsonValueNumber>(sizeCm.X * CentimetersToMeters));
	sizeValues.Add(MakeShared<FJsonValueNumber>(sizeCm.Y * CentimetersToMeters));
	return sizeValues;
}

TSharedPtr<FJsonObject> UScenarioAuthoringSubsystem::MakePropertiesObject(const TMap<FString, FScenarioParamValue>& properties)
{
	return MakeFilteredPropertiesObject(properties, TSet<FString>());
}

TSharedPtr<FJsonObject> UScenarioAuthoringSubsystem::MakeFilteredPropertiesObject(
	const TMap<FString, FScenarioParamValue>& properties,
	const TSet<FString>& excludedKeys)
{
	TSharedRef<FJsonObject> propertiesObject = MakeShared<FJsonObject>();
	int32 serializedCount = 0;
	for (const TPair<FString, FScenarioParamValue>& pair : properties)
	{
		if (excludedKeys.Contains(pair.Key))
		{
			continue;
		}

		propertiesObject->SetField(pair.Key, MakeParamJsonValue(pair.Value));
		++serializedCount;
	}

	if (serializedCount <= 0)
	{
		return nullptr;
	}

	return propertiesObject;
}

TSharedPtr<FJsonValue> UScenarioAuthoringSubsystem::MakeParamJsonValue(const FScenarioParamValue& paramValue)
{
	switch (paramValue.Type)
	{
	case EScenarioParamValueType::Bool:
		return MakeShared<FJsonValueBoolean>(paramValue.BoolValue);
	case EScenarioParamValueType::Integer:
		return MakeShared<FJsonValueNumber>(paramValue.IntegerValue);
	case EScenarioParamValueType::Float:
		return MakeShared<FJsonValueNumber>(paramValue.FloatValue);
	case EScenarioParamValueType::String:
		return MakeShared<FJsonValueString>(paramValue.StringValue);
	case EScenarioParamValueType::Vector:
	{
		TArray<TSharedPtr<FJsonValue>> vectorValues;
		vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.X));
		vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.Y));
		vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.Z));
		return MakeShared<FJsonValueArray>(vectorValues);
	}
	default:
		return MakeShared<FJsonValueNull>();
	}
}

bool UScenarioAuthoringSubsystem::TryGetFloatProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	double& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue) return false;

	if (paramValue->Type == EScenarioParamValueType::Float)
	{
		outValue = paramValue->FloatValue;
		return true;
	}

	if (paramValue->Type == EScenarioParamValueType::Integer)
	{
		outValue = static_cast<double>(paramValue->IntegerValue);
		return true;
	}

	return false;
}

bool UScenarioAuthoringSubsystem::TryGetBoolProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	bool& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::Bool) return false;

	outValue = paramValue->BoolValue;
	return true;
}

bool UScenarioAuthoringSubsystem::TryGetStringProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	FString& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::String) return false;

	outValue = paramValue->StringValue;
	return true;
}

void UScenarioAuthoringSubsystem::AppendSchemaDiagnostics(
	const TArray<FScenarioSchemaDiagnostic>& schemaDiagnostics,
	TArray<FString>& outDiagnostics)
{
	for (const FScenarioSchemaDiagnostic& diagnostic : schemaDiagnostics)
	{
		FString severity = TEXT("Info");
		switch (diagnostic.Severity)
		{
		case EScenarioSchemaDiagnosticSeverity::Warning:
			severity = TEXT("Warning");
			break;
		case EScenarioSchemaDiagnosticSeverity::Repair:
			severity = TEXT("Repair");
			break;
		case EScenarioSchemaDiagnosticSeverity::Error:
			severity = TEXT("Error");
			break;
		case EScenarioSchemaDiagnosticSeverity::Info:
		default:
			break;
		}

		const FString pathSuffix = diagnostic.Path.IsEmpty()
			? FString()
			: FString::Printf(TEXT(" | %s"), *diagnostic.Path);
		outDiagnostics.Add(FString::Printf(
			TEXT("[%s] %s: %s%s"),
			*severity,
			*diagnostic.Code,
			*diagnostic.Message,
			*pathSuffix));
	}
}

FScenarioTemplateNumberValue UScenarioAuthoringSubsystem::MakeFixedTemplateNumber(double value)
{
	FScenarioTemplateNumberValue numberValue;
	numberValue.bIsSet = true;
	numberValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
	numberValue.FixedValue = value;
	return numberValue;
}

FScenarioTemplateNumberValue UScenarioAuthoringSubsystem::MakeRangeTemplateNumber(double minValue, double maxValue)
{
	FScenarioTemplateNumberValue numberValue;
	numberValue.bIsSet = true;
	numberValue.Mode = EScenarioTemplateNumberValueMode::Range;
	numberValue.MinValue = FMath::Min(minValue, maxValue);
	numberValue.MaxValue = FMath::Max(minValue, maxValue);
	return numberValue;
}

FScenarioTemplateIntegerValue UScenarioAuthoringSubsystem::MakeFixedTemplateInteger(int32 value)
{
	FScenarioTemplateIntegerValue integerValue;
	integerValue.bIsSet = true;
	integerValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
	integerValue.FixedValue = value;
	return integerValue;
}

FScenarioTemplateIntegerValue UScenarioAuthoringSubsystem::MakeRangeTemplateInteger(
	const int32 minValue,
	const int32 maxValue)
{
	FScenarioTemplateIntegerValue integerValue;
	integerValue.bIsSet = true;
	integerValue.Mode = EScenarioTemplateNumberValueMode::Range;
	integerValue.MinValue = FMath::Min(minValue, maxValue);
	integerValue.MaxValue = FMath::Max(minValue, maxValue);
	return integerValue;
}

double UScenarioAuthoringSubsystem::GetFixedTemplateNumber(
	const FScenarioTemplateNumberValue& value,
	double defaultValue)
{
	if (!value.bIsSet)
	{
		return defaultValue;
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return (value.MinValue + value.MaxValue) * 0.5;
	}

	return value.FixedValue;
}

bool UScenarioAuthoringSubsystem::IsPositiveTemplateNumber(const FScenarioTemplateNumberValue& value)
{
	if (!value.bIsSet)
	{
		return false;
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FMath::IsFinite(value.MinValue)
			&& FMath::IsFinite(value.MaxValue)
			&& value.MinValue > KINDA_SMALL_NUMBER
			&& value.MaxValue > KINDA_SMALL_NUMBER;
	}

	return FMath::IsFinite(value.FixedValue) && value.FixedValue > KINDA_SMALL_NUMBER;
}

bool UScenarioAuthoringSubsystem::IsValidOptionalTemplateNumber(const FScenarioTemplateNumberValue& value)
{
	if (!value.bIsSet)
	{
		return true;
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FMath::IsFinite(value.MinValue)
			&& FMath::IsFinite(value.MaxValue)
			&& value.MinValue <= value.MaxValue;
	}

	return FMath::IsFinite(value.FixedValue);
}

bool UScenarioAuthoringSubsystem::IsValidOptionalTemplateInteger(const FScenarioTemplateIntegerValue& value)
{
	if (!value.bIsSet)
	{
		return true;
	}

	return value.Mode != EScenarioTemplateNumberValueMode::Range || value.MinValue <= value.MaxValue;
}

bool UScenarioAuthoringSubsystem::IsNonNegativeTemplateInteger(const FScenarioTemplateIntegerValue& value)
{
	if (!IsValidOptionalTemplateInteger(value))
	{
		return false;
	}
	if (!value.bIsSet)
	{
		return true;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return value.MinValue >= 0 && value.MaxValue >= 0;
	}
	return value.FixedValue >= 0;
}

double UScenarioAuthoringSubsystem::MeasureCorridorAxisLengthMeters(const TArray<FVector2D>& pointsMeters)
{
	double lengthMeters = 0.0;
	for (int32 index = 0; index < pointsMeters.Num() - 1; ++index)
	{
		lengthMeters += (pointsMeters[index + 1] - pointsMeters[index]).Size();
	}

	return lengthMeters;
}

bool UScenarioAuthoringSubsystem::AreCorridorAxisPointsValid(
	const TArray<FVector2D>& pointsMeters,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (pointsMeters.Num() < 2)
	{
		outFailureReason = TEXT("Corridor axis must contain at least two points.");
		return false;
	}

	for (int32 index = 0; index < pointsMeters.Num(); ++index)
	{
		const FVector2D& pointMeters = pointsMeters[index];
		if (!FMath::IsFinite(pointMeters.X) || !FMath::IsFinite(pointMeters.Y))
		{
			outFailureReason = FString::Printf(TEXT("Corridor axis point %d must be finite."), index);
			return false;
		}
	}

	if (MeasureCorridorAxisLengthMeters(pointsMeters) <= KINDA_SMALL_NUMBER)
	{
		outFailureReason = TEXT("Corridor axis length must be positive.");
		return false;
	}

	return true;
}

FString UScenarioAuthoringSubsystem::MakeCorridorVertexHandleId(int32 vertexIndex)
{
	return FString::Printf(TEXT("%s%03d"), *CorridorVertexHandleIdPrefix, vertexIndex);
}

FString UScenarioAuthoringSubsystem::MakeCorridorSegmentHandleId(int32 segmentIndex)
{
	return FString::Printf(TEXT("%s%03d"), *CorridorSegmentHandleIdPrefix, segmentIndex);
}

bool UScenarioAuthoringSubsystem::TryParseCorridorVertexHandleId(const FString& handleId, int32& outVertexIndex)
{
	outVertexIndex = INDEX_NONE;
	if (!handleId.StartsWith(CorridorVertexHandleIdPrefix))
	{
		return false;
	}

	const FString indexText = handleId.RightChop(CorridorVertexHandleIdPrefix.Len());
	if (indexText.IsEmpty() || !indexText.IsNumeric())
	{
		return false;
	}

	outVertexIndex = FCString::Atoi(*indexText);
	return outVertexIndex >= 0;
}

bool UScenarioAuthoringSubsystem::TryParseCorridorSegmentHandleId(const FString& handleId, int32& outSegmentIndex)
{
	outSegmentIndex = INDEX_NONE;
	if (!handleId.StartsWith(CorridorSegmentHandleIdPrefix))
	{
		return false;
	}

	const FString indexText = handleId.RightChop(CorridorSegmentHandleIdPrefix.Len());
	if (indexText.IsEmpty() || !indexText.IsNumeric())
	{
		return false;
	}

	outSegmentIndex = FCString::Atoi(*indexText);
	return outSegmentIndex >= 0;
}

FTransform UScenarioAuthoringSubsystem::MakeCorridorVertexHandleTransform(const FVector2D& pointMeters)
{
	return FTransform(
		FRotator::ZeroRotator,
		FVector(pointMeters.X / CentimetersToMeters, pointMeters.Y / CentimetersToMeters, CorridorVertexHandleHeightCm),
		FVector(CorridorVertexHandleScale));
}

FTransform UScenarioAuthoringSubsystem::MakeCorridorSegmentHandleTransform(
	const FVector2D& startMeters,
	const FVector2D& endMeters)
{
	const FVector2D segmentVectorMeters = endMeters - startMeters;
	const FVector2D midpointMeters = (startMeters + endMeters) * 0.5;
	const double yawDegrees = FMath::RadiansToDegrees(FMath::Atan2(segmentVectorMeters.Y, segmentVectorMeters.X));
	return FTransform(
		FRotator(0.0, yawDegrees, 0.0),
		FVector(midpointMeters.X / CentimetersToMeters, midpointMeters.Y / CentimetersToMeters, CorridorSegmentHandleHeightCm),
		FVector::OneVector);
}

bool UScenarioAuthoringSubsystem::CommitCorridorDraftEdit(
	const FScenarioTemplateDocument& previousTemplate,
	bool bPreviousDirty,
	TArray<FString>& outDiagnostics)
{
	TArray<FScenarioSchemaDiagnostic> schemaDiagnostics;
	if (!FScenarioTemplateJson::ValidateDocument(DraftScenarioTemplate, schemaDiagnostics))
	{
		AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);
		DraftScenarioTemplate = previousTemplate;
		bDirty = bPreviousDirty;
		return false;
	}
	AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);

	bDirty = true;
	if (RebuildEditorViewFromDraft(outDiagnostics))
	{
		return true;
	}

	DraftScenarioTemplate = previousTemplate;
	bDirty = bPreviousDirty;

	TArray<FString> rollbackDiagnostics;
	RebuildEditorViewFromDraft(rollbackDiagnostics);
	bDirty = bPreviousDirty;
	outDiagnostics.Add(TEXT("Corridor edit was rejected because the editor preview could not be rebuilt."));
	return false;
}

bool UScenarioAuthoringSubsystem::CommitObstacleDraftEdit(
	const FScenarioTemplateDocument& previousTemplate,
	bool bPreviousDirty,
	TArray<FString>& outDiagnostics)
{
	TArray<FScenarioSchemaDiagnostic> schemaDiagnostics;
	if (!FScenarioTemplateJson::ValidateDocument(DraftScenarioTemplate, schemaDiagnostics))
	{
		AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);
		DraftScenarioTemplate = previousTemplate;
		bDirty = bPreviousDirty;
		return false;
	}
	AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);

	bDirty = true;
	if (RefreshGeneratedEditorPreviewActorsFromDraft(outDiagnostics))
	{
		return true;
	}

	DraftScenarioTemplate = previousTemplate;
	bDirty = bPreviousDirty;

	TArray<FString> rollbackDiagnostics;
	RefreshGeneratedEditorPreviewActorsFromDraft(rollbackDiagnostics);
	SyncCorridorHandleActors();
	bDirty = bPreviousDirty;
	outDiagnostics.Add(TEXT("Obstacle edit was rejected because the editor preview could not be refreshed."));
	return false;
}

bool UScenarioAuthoringSubsystem::CommitTemplateMetadataDraftEdit(
	const FScenarioTemplateDocument& previousTemplate,
	bool bPreviousDirty,
	TArray<FString>& outDiagnostics)
{
	TArray<FScenarioSchemaDiagnostic> schemaDiagnostics;
	if (!FScenarioTemplateJson::ValidateDocument(DraftScenarioTemplate, schemaDiagnostics))
	{
		AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);
		DraftScenarioTemplate = previousTemplate;
		bDirty = bPreviousDirty;
		return false;
	}

	AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);
	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::ValidateCorridorLaneProfile(
	const TArray<FScenarioTemplateLaneRule>& lanes,
	const FString& path,
	TArray<FString>& outDiagnostics) const
{
	for (int32 index = 0; index < lanes.Num(); ++index)
	{
		const FScenarioTemplateLaneRule& lane = lanes[index];
		const FString surfacePath = FString::Printf(TEXT("%s[%d].surface"), *path, index);
		if (!ValidateCorridorSurfaceId(lane.SurfaceId, surfacePath, outDiagnostics))
		{
			return false;
		}
		if (!IsPositiveTemplateNumber(lane.WidthMeters))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s[%d].width_m must be a positive fixed value or positive min/max range."), *path, index));
			return false;
		}
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateCorridorSegments(
	const TArray<FScenarioTemplateSegment>& segments,
	double axisLengthMeters,
	TArray<FString>& outDiagnostics) const
{
	if (segments.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Corridor must contain at least one segment."));
		return false;
	}

	TSet<FString> segmentIds;
	for (int32 index = 0; index < segments.Num(); ++index)
	{
		const FScenarioTemplateSegment& segment = segments[index];
		const FString segmentId = segment.SegmentId.TrimStartAndEnd();
		if (segmentId.IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("corridor.segments[%d].id must not be empty."), index));
			return false;
		}
		if (segmentIds.Contains(segmentId))
		{
			outDiagnostics.Add(FString::Printf(TEXT("Duplicate corridor segment id '%s'."), *segmentId));
			return false;
		}
		segmentIds.Add(segmentId);

		const FString replacedByPath = FString::Printf(TEXT("corridor.segments[%d].replaced_by"), index);
		if (!ValidateCorridorSurfaceValue(segment.ReplacedBySurfaceId, replacedByPath, outDiagnostics))
		{
			return false;
		}

		const double startMeters = segment.AlongRangeMeters.StartMeters;
		const double endMeters = segment.AlongRangeMeters.EndMeters;
		if (!FMath::IsFinite(startMeters) || !FMath::IsFinite(endMeters))
		{
			outDiagnostics.Add(FString::Printf(TEXT("corridor.segments[%d].along_range_m must be finite."), index));
			return false;
		}
		if (startMeters < -KINDA_SMALL_NUMBER || endMeters > axisLengthMeters + KINDA_SMALL_NUMBER)
		{
			outDiagnostics.Add(FString::Printf(TEXT("corridor.segments[%d].along_range_m must stay within the corridor axis length."), index));
			return false;
		}
		if (endMeters <= startMeters + KINDA_SMALL_NUMBER)
		{
			outDiagnostics.Add(FString::Printf(TEXT("corridor.segments[%d].along_range_m must have positive length."), index));
			return false;
		}
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateCorridorSurfaceId(
	const FString& surfaceId,
	const FString& path,
	TArray<FString>& outDiagnostics) const
{
	const FString normalizedSurfaceId = surfaceId.TrimStartAndEnd();
	if (normalizedSurfaceId.IsEmpty())
	{
		outDiagnostics.Add(FString::Printf(TEXT("%s must not be empty."), *path));
		return false;
	}

	FScenarioCorridorSurfaceEntry surfaceEntry;
	if (!TryFindCorridorSurfaceEntry(FName(*normalizedSurfaceId), surfaceEntry))
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("%s references unknown Corridor surface '%s'."),
			*path,
			*normalizedSurfaceId));
		return false;
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateCorridorSurfaceValue(
	const FScenarioTemplateStringValue& value,
	const FString& path,
	TArray<FString>& outDiagnostics) const
{
	if (!value.bIsSet)
	{
		return true;
	}

	if (value.Mode == EScenarioTemplateStringValueMode::Choices)
	{
		if (value.Choices.IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s choices must not be empty."), *path));
			return false;
		}

		for (int32 choiceIndex = 0; choiceIndex < value.Choices.Num(); ++choiceIndex)
		{
			const FString choicePath = FString::Printf(TEXT("%s.choices[%d]"), *path, choiceIndex);
			if (!ValidateCorridorSurfaceId(value.Choices[choiceIndex], choicePath, outDiagnostics))
			{
				return false;
			}
		}
		return true;
	}

	return ValidateCorridorSurfaceId(value.FixedValue, path, outDiagnostics);
}

bool UScenarioAuthoringSubsystem::ValidateObstaclePlacements(
	const TArray<FScenarioTemplateObstaclePlacement>& placements,
	TArray<FString>& outDiagnostics) const
{
	auto validateRequiredNumber =
		[this, &outDiagnostics](const FScenarioTemplateNumberValue& value, const FString& path)
	{
		if (!value.bIsSet || !IsValidOptionalTemplateNumber(value))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("%s must be a finite fixed number or finite min/max range."),
				*path));
			return false;
		}
		return true;
	};

	auto validateOptionalNumber =
		[this, &outDiagnostics](const FScenarioTemplateNumberValue& value, const FString& path)
	{
		if (!IsValidOptionalTemplateNumber(value))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("%s must be unset, a finite fixed number, or a finite min/max range."),
				*path));
			return false;
		}
		return true;
	};

	auto validateNonNegativeNumber =
		[this, &outDiagnostics, &validateOptionalNumber](const FScenarioTemplateNumberValue& value, const FString& path)
	{
		if (!validateOptionalNumber(value, path))
		{
			return false;
		}
		if (!value.bIsSet)
		{
			return true;
		}
		if (value.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			if (value.MinValue < 0.0 || value.MaxValue < 0.0)
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s must not contain negative values."), *path));
				return false;
			}
			return true;
		}
		if (value.FixedValue < 0.0)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s must not be negative."), *path));
			return false;
		}
		return true;
	};

	TSet<FString> placementIds;
	for (int32 index = 0; index < placements.Num(); ++index)
	{
		const FScenarioTemplateObstaclePlacement& placement = placements[index];
		const FString placementPath = FString::Printf(TEXT("obstacles.placements[%d]"), index);
		if (placement.PlacementId.IsEmpty())
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s.id must not be empty."), *placementPath));
			return false;
		}
		if (placementIds.Contains(placement.PlacementId))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Duplicate obstacle placement id '%s'."),
				*placement.PlacementId));
			return false;
		}
		placementIds.Add(placement.PlacementId);

		if (!IsNonNegativeTemplateInteger(placement.Count))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s.count must be unset or a non-negative fixed/range integer."), *placementPath));
			return false;
		}
		if (!validateNonNegativeNumber(placement.SpacingMeters, FString::Printf(TEXT("%s.spacing_m"), *placementPath))
			|| !validateNonNegativeNumber(placement.GapWidthMeters, FString::Printf(TEXT("%s.gap_width_m"), *placementPath))
			|| !validateNonNegativeNumber(placement.DensityPer10Meters, FString::Printf(TEXT("%s.density_per_10m"), *placementPath))
			|| !validateOptionalNumber(placement.YawDegrees, FString::Printf(TEXT("%s.yaw_deg"), *placementPath)))
		{
			return false;
		}

		if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Fixed
			|| placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern)
		{
			if (placement.PropId.IsEmpty())
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s.prop is required for fixed and pattern placement."), *placementPath));
				return false;
			}

			FScenarioStaticObstaclePropEntry propEntry;
			if (!TryFindStaticObstacleProp(FName(*placement.PropId), propEntry))
			{
				outDiagnostics.Add(FString::Printf(
					TEXT("%s.prop references unknown static obstacle prop '%s'."),
					*placementPath,
					*placement.PropId));
				return false;
			}

			if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern && placement.PatternId.IsEmpty())
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s.pattern is required for pattern placement."), *placementPath));
				return false;
			}
			if (!ValidateCorridorSegmentReference(
				placement.At.SegmentId,
				FString::Printf(TEXT("%s.at.segment"), *placementPath),
				outDiagnostics))
			{
				return false;
			}
			if (!validateRequiredNumber(placement.At.AlongMeters, FString::Printf(TEXT("%s.at.along_m"), *placementPath))
				|| !validateRequiredNumber(placement.At.OffsetMeters, FString::Printf(TEXT("%s.at.offset_m"), *placementPath)))
			{
				return false;
			}
		}
		else if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Scatter)
		{
			if (placement.Zone.SegmentIds.IsEmpty())
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s.zone.segments must contain at least one segment id."), *placementPath));
				return false;
			}
			TSet<FString> zoneSegmentIds;
			for (int32 segmentIndex = 0; segmentIndex < placement.Zone.SegmentIds.Num(); ++segmentIndex)
			{
				const FString path = FString::Printf(TEXT("%s.zone.segments[%d]"), *placementPath, segmentIndex);
				const FString& segmentId = placement.Zone.SegmentIds[segmentIndex];
				if (!ValidateCorridorSegmentReference(segmentId, path, outDiagnostics))
				{
					return false;
				}
				if (zoneSegmentIds.Contains(segmentId))
				{
					outDiagnostics.Add(FString::Printf(TEXT("Duplicate scatter zone segment '%s'."), *segmentId));
					return false;
				}
				zoneSegmentIds.Add(segmentId);
			}
			if (placement.Zone.LaneIds.IsEmpty())
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s.zone.lanes must contain at least one lane id."), *placementPath));
				return false;
			}
			if (!placement.DensityPer10Meters.bIsSet)
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s.density_per_10m is required for scatter placement."), *placementPath));
				return false;
			}
			if (placement.Palette.CategoryIds.IsEmpty() && placement.Palette.ClassIds.IsEmpty())
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s.palette must contain categories or classes for scatter placement."), *placementPath));
				return false;
			}
		}
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateCorridorSegmentReference(
	const FString& segmentId,
	const FString& path,
	TArray<FString>& outDiagnostics) const
{
	const FString normalizedSegmentId = segmentId.TrimStartAndEnd();
	if (normalizedSegmentId.IsEmpty())
	{
		outDiagnostics.Add(FString::Printf(TEXT("%s must reference a Corridor segment."), *path));
		return false;
	}

	const bool bFoundSegment = DraftScenarioTemplate.Corridor.Segments.ContainsByPredicate(
		[&normalizedSegmentId](const FScenarioTemplateSegment& segment)
		{
			return segment.SegmentId == normalizedSegmentId;
		});
	if (!bFoundSegment)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("%s references unknown Corridor segment '%s'."),
			*path,
			*normalizedSegmentId));
		return false;
	}

	return true;
}

void UScenarioAuthoringSubsystem::RescaleCorridorAlongReferences(double oldLengthMeters, double newLengthMeters)
{
	if (oldLengthMeters <= KINDA_SMALL_NUMBER || newLengthMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const double scale = newLengthMeters / oldLengthMeters;
	auto scaleNumber = [scale](FScenarioTemplateNumberValue& numberValue)
	{
		if (!numberValue.bIsSet)
		{
			return;
		}

		if (numberValue.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			numberValue.MinValue *= scale;
			numberValue.MaxValue *= scale;
			return;
		}

		numberValue.FixedValue *= scale;
	};

	if (DraftScenarioTemplate.Robot.Start.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		scaleNumber(DraftScenarioTemplate.Robot.Start.AlongMeters);
	}
	if (DraftScenarioTemplate.Robot.Goal.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		scaleNumber(DraftScenarioTemplate.Robot.Goal.AlongMeters);
	}

	for (FScenarioTemplateObstaclePlacement& placement : DraftScenarioTemplate.Obstacles.Placements)
	{
		if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Fixed
			|| placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern)
		{
			scaleNumber(placement.At.AlongMeters);
		}
	}
}

void UScenarioAuthoringSubsystem::RescaleCorridorSegmentsForAxisLength(
	double oldLengthMeters,
	double newLengthMeters)
{
	if (newLengthMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (DraftScenarioTemplate.Corridor.Segments.IsEmpty())
	{
		FScenarioTemplateSegment mainSegment;
		mainSegment.SegmentId = TEXT("main");
		mainSegment.Type = EScenarioTemplateSegmentType::Straight;
		mainSegment.AlongRangeMeters.StartMeters = 0.0;
		mainSegment.AlongRangeMeters.EndMeters = newLengthMeters;
		DraftScenarioTemplate.Corridor.Segments.Add(mainSegment);
		return;
	}

	double basisLengthMeters = oldLengthMeters;
	if (basisLengthMeters <= KINDA_SMALL_NUMBER)
	{
		for (const FScenarioTemplateSegment& segment : DraftScenarioTemplate.Corridor.Segments)
		{
			basisLengthMeters = FMath::Max(basisLengthMeters, segment.AlongRangeMeters.EndMeters);
		}
	}

	const double scale = basisLengthMeters > KINDA_SMALL_NUMBER
		? newLengthMeters / basisLengthMeters
		: 1.0;
	for (FScenarioTemplateSegment& segment : DraftScenarioTemplate.Corridor.Segments)
	{
		segment.AlongRangeMeters.StartMeters =
			FMath::Clamp(segment.AlongRangeMeters.StartMeters * scale, 0.0, newLengthMeters);
		segment.AlongRangeMeters.EndMeters =
			FMath::Clamp(segment.AlongRangeMeters.EndMeters * scale, 0.0, newLengthMeters);
		if (segment.AlongRangeMeters.EndMeters <= segment.AlongRangeMeters.StartMeters + KINDA_SMALL_NUMBER)
		{
			segment.AlongRangeMeters.EndMeters = FMath::Min(newLengthMeters, segment.AlongRangeMeters.StartMeters + 0.01);
		}
	}
}

void UScenarioAuthoringSubsystem::RepairCorridorReferenceSegmentIds()
{
	auto repairAnchor = [this](FScenarioTemplateRobotAnchor& anchor)
	{
		if (anchor.Type != EScenarioTemplateRobotAnchorType::CorridorPose)
		{
			return;
		}

		const double alongMeters = GetFixedTemplateNumber(anchor.AlongMeters, 0.0);
		anchor.SegmentId = FindCorridorSegmentIdForAlongMeters(alongMeters);
	};

	repairAnchor(DraftScenarioTemplate.Robot.Start);
	repairAnchor(DraftScenarioTemplate.Robot.Goal);

	for (FScenarioTemplateObstaclePlacement& placement : DraftScenarioTemplate.Obstacles.Placements)
	{
		if (placement.Kind != EScenarioTemplateObstaclePlacementKind::Fixed
			&& placement.Kind != EScenarioTemplateObstaclePlacementKind::Pattern)
		{
			continue;
		}

		const double alongMeters = GetFixedTemplateNumber(placement.At.AlongMeters, 0.0);
		placement.At.SegmentId = FindCorridorSegmentIdForAlongMeters(alongMeters);
	}
}

bool UScenarioAuthoringSubsystem::ApplyCorridorAxisPointsEdit(
	const TArray<FVector2D>& pointsMeters,
	bool bRebuildAllPreviewActors,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	FString validationFailureReason;
	if (!AreCorridorAxisPointsValid(pointsMeters, validationFailureReason))
	{
		outFailureReason = validationFailureReason;
		return false;
	}

	const FScenarioTemplateDocument previousTemplate = DraftScenarioTemplate;
	const bool bPreviousDirty = bDirty;
	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const double oldLengthMeters = MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);
	DraftScenarioTemplate.Corridor.Axis.Type = EScenarioCorridorAxisType::Polyline;
	DraftScenarioTemplate.Corridor.Axis.PointsMeters = pointsMeters;
	const double newLengthMeters = MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);

	RescaleCorridorSegmentsForAxisLength(oldLengthMeters, newLengthMeters);
	RescaleCorridorAlongReferences(oldLengthMeters, newLengthMeters);
	RepairCorridorReferenceSegmentIds();

	TArray<FScenarioSchemaDiagnostic> schemaDiagnostics;
	TArray<FString> diagnostics;
	if (!FScenarioTemplateJson::ValidateDocument(DraftScenarioTemplate, schemaDiagnostics))
	{
		AppendSchemaDiagnostics(schemaDiagnostics, diagnostics);
		DraftScenarioTemplate = previousTemplate;
		bDirty = bPreviousDirty;
		outFailureReason = diagnostics.IsEmpty()
			? TEXT("Corridor axis edit failed schema validation.")
			: FString::Join(diagnostics, TEXT(" "));
		return false;
	}

	bDirty = true;
	bool bPreviewRefreshed = false;
	if (bRebuildAllPreviewActors)
	{
		TArray<FString> rebuildDiagnostics;
		bPreviewRefreshed = RebuildEditorViewFromDraft(rebuildDiagnostics);
		diagnostics.Append(rebuildDiagnostics);
	}
	else
	{
		TArray<FString> refreshDiagnostics;
		bPreviewRefreshed = RefreshGeneratedEditorPreviewActorsFromDraft(refreshDiagnostics);
		diagnostics.Append(refreshDiagnostics);
		if (bPreviewRefreshed)
		{
			SyncCorridorHandleActors();
		}
	}

	if (bPreviewRefreshed)
	{
		return true;
	}

	DraftScenarioTemplate = previousTemplate;
	bDirty = bPreviousDirty;
	if (bRebuildAllPreviewActors)
	{
		TArray<FString> rollbackDiagnostics;
		RebuildEditorViewFromDraft(rollbackDiagnostics);
	}
	else
	{
		TArray<FString> rollbackDiagnostics;
		RefreshGeneratedEditorPreviewActorsFromDraft(rollbackDiagnostics);
		SyncCorridorHandleActors();
	}
	bDirty = bPreviousDirty;
	outFailureReason = diagnostics.IsEmpty()
		? TEXT("Corridor axis edit was rejected because the editor preview could not be refreshed.")
		: FString::Join(diagnostics, TEXT(" "));
	return false;
}

FString UScenarioAuthoringSubsystem::FindCorridorSegmentIdForAlongMeters(double alongMeters) const
{
	const TArray<FScenarioTemplateSegment>& segments = DraftScenarioTemplate.Corridor.Segments;
	if (segments.IsEmpty())
	{
		return TEXT("main");
	}

	const FScenarioTemplateSegment* nearestSegment = &segments[0];
	double nearestDistanceMeters = TNumericLimits<double>::Max();
	for (const FScenarioTemplateSegment& segment : segments)
	{
		if (alongMeters >= segment.AlongRangeMeters.StartMeters - KINDA_SMALL_NUMBER
			&& alongMeters <= segment.AlongRangeMeters.EndMeters + KINDA_SMALL_NUMBER)
		{
			return segment.SegmentId;
		}

		const double distanceMeters = FMath::Min(
			FMath::Abs(alongMeters - segment.AlongRangeMeters.StartMeters),
			FMath::Abs(alongMeters - segment.AlongRangeMeters.EndMeters));
		if (distanceMeters < nearestDistanceMeters)
		{
			nearestDistanceMeters = distanceMeters;
			nearestSegment = &segment;
		}
	}

	return nearestSegment ? nearestSegment->SegmentId : FString(TEXT("main"));
}

bool UScenarioAuthoringSubsystem::TryProjectLocationToCorridor(
	const FVector& locationCm,
	double& outAlongMeters,
	double& outOffsetMeters,
	FString& outSegmentId) const
{
	outAlongMeters = 0.0;
	outOffsetMeters = 0.0;
	outSegmentId.Reset();

	const TArray<FVector2D>& pointsMeters = DraftScenarioTemplate.Corridor.Axis.PointsMeters;
	if (pointsMeters.Num() < 2)
	{
		return false;
	}

	const FVector2D locationMeters(locationCm.X * CentimetersToMeters, locationCm.Y * CentimetersToMeters);
	double cumulativeLengthMeters = 0.0;
	double bestDistanceSquared = TNumericLimits<double>::Max();
	bool bFoundSegment = false;

	for (int32 index = 0; index < pointsMeters.Num() - 1; ++index)
	{
		const FVector2D segmentStart = pointsMeters[index];
		const FVector2D segmentEnd = pointsMeters[index + 1];
		const FVector2D segmentVector = segmentEnd - segmentStart;
		const double segmentLengthSquared = segmentVector.SizeSquared();
		const double segmentLengthMeters = FMath::Sqrt(segmentLengthSquared);
		if (segmentLengthMeters <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const double projectedAlpha = FMath::Clamp(
			FVector2D::DotProduct(locationMeters - segmentStart, segmentVector) / segmentLengthSquared,
			0.0,
			1.0);
		const FVector2D projectedPoint = segmentStart + (segmentVector * projectedAlpha);
		const double distanceSquared = (locationMeters - projectedPoint).SizeSquared();
		if (distanceSquared < bestDistanceSquared)
		{
			const FVector2D direction = segmentVector / segmentLengthMeters;
			const FVector2D normal(-direction.Y, direction.X);
			bestDistanceSquared = distanceSquared;
			outAlongMeters = cumulativeLengthMeters + (segmentLengthMeters * projectedAlpha);
			outOffsetMeters = FVector2D::DotProduct(locationMeters - projectedPoint, normal);
			bFoundSegment = true;
		}

		cumulativeLengthMeters += segmentLengthMeters;
	}

	if (!bFoundSegment)
	{
		return false;
	}

	outSegmentId = FindCorridorSegmentIdForAlongMeters(outAlongMeters);
	return true;
}

bool UScenarioAuthoringSubsystem::TryResolveCorridorPoseMeters(
	double alongMeters,
	double offsetMeters,
	FVector2D& outPointMeters,
	double& outYawDegrees) const
{
	outPointMeters = FVector2D::ZeroVector;
	outYawDegrees = 0.0;

	const TArray<FVector2D>& pointsMeters = DraftScenarioTemplate.Corridor.Axis.PointsMeters;
	if (pointsMeters.Num() < 2)
	{
		return false;
	}

	double remainingMeters = FMath::Max(alongMeters, 0.0);
	FVector2D direction = pointsMeters[1] - pointsMeters[0];
	FVector2D pointMeters = pointsMeters[0];

	for (int32 index = 0; index < pointsMeters.Num() - 1; ++index)
	{
		const FVector2D segmentStart = pointsMeters[index];
		const FVector2D segmentEnd = pointsMeters[index + 1];
		const FVector2D segmentVector = segmentEnd - segmentStart;
		const double segmentLengthMeters = segmentVector.Size();
		if (segmentLengthMeters <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		direction = segmentVector / segmentLengthMeters;
		if (remainingMeters <= segmentLengthMeters || index == pointsMeters.Num() - 2)
		{
			const double segmentDistanceMeters = FMath::Clamp(remainingMeters, 0.0, segmentLengthMeters);
			pointMeters = segmentStart + (direction * segmentDistanceMeters);
			break;
		}

		remainingMeters -= segmentLengthMeters;
	}

	const FVector2D normal(-direction.Y, direction.X);
	outPointMeters = pointMeters + (normal * offsetMeters);
	outYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(direction.Y, direction.X));
	return true;
}

double UScenarioAuthoringSubsystem::ResolveCorridorSurfaceZOffsetCm(double offsetMeters) const
{
	const double walkwayWidthMeters = GetFixedTemplateNumber(DraftScenarioTemplate.Corridor.WalkwayWidthMeters, 3.0);
	const double halfWalkwayWidthMeters = FMath::Max(walkwayWidthMeters, 0.0) * 0.5;
	if (offsetMeters <= halfWalkwayWidthMeters + KINDA_SMALL_NUMBER)
	{
		return 0.0;
	}

	double curbSideWidthMeters = 0.0;
	for (const FScenarioTemplateLaneRule& laneRule : DraftScenarioTemplate.Corridor.CurbSide)
	{
		curbSideWidthMeters += FMath::Max(GetFixedTemplateNumber(laneRule.WidthMeters, 0.0), 0.0);
	}

	return curbSideWidthMeters > KINDA_SMALL_NUMBER ? CurbSideSurfaceZOffsetCm : 0.0;
}

bool UScenarioAuthoringSubsystem::TryResolveCorridorSurfaceZOffsetCm(
	const FVector& locationCm,
	double& outSurfaceZOffsetCm) const
{
	outSurfaceZOffsetCm = 0.0;

	double alongMeters = 0.0;
	double offsetMeters = 0.0;
	FString segmentId;
	if (!TryProjectLocationToCorridor(locationCm, alongMeters, offsetMeters, segmentId))
	{
		return false;
	}

	outSurfaceZOffsetCm = ResolveCorridorSurfaceZOffsetCm(offsetMeters);
	return true;
}

UScenarioCompiler* UScenarioAuthoringSubsystem::CreateScenarioCompiler() const
{
	UScenarioCompiler* compiler = NewObject<UScenarioCompiler>();
	if (!compiler) return nullptr;

	compiler->StaticObstaclePropCatalog = StaticObstaclePropCatalog;
	return compiler;
}

const UScenarioStaticObstaclePropCatalog* UScenarioAuthoringSubsystem::GetStaticObstaclePropCatalog() const
{
	const UScenarioStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog))
	{
		UE_LOG(
			LogScenarioAuthoring,
			Warning,
			TEXT("Scenario static obstacle prop catalog is not configured or failed to load: %s"),
			*StaticObstaclePropCatalog.ToSoftObjectPath().ToString());
		return nullptr;
	}

	return propCatalog;
}

bool UScenarioAuthoringSubsystem::TryFindStaticObstacleProp(
	FName propId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	if (propId.IsNone()) return false;

	const UScenarioStaticObstaclePropCatalog* propCatalog = GetStaticObstaclePropCatalog();
	if (!propCatalog) return false;

	return propCatalog->FindPropEntryById(propId, outPropEntry);
}

const UScenarioCorridorSurfaceCatalog* UScenarioAuthoringSubsystem::GetCorridorSurfaceCatalog() const
{
	const UScenarioCorridorSurfaceCatalog* surfaceCatalog = CorridorSurfaceCatalog.LoadSynchronous();
	if (!IsValid(surfaceCatalog))
	{
		UE_LOG(
			LogScenarioAuthoring,
			Verbose,
			TEXT("Corridor surface catalog is unavailable; using built-in fallback entries. Path: %s"),
			*CorridorSurfaceCatalog.ToSoftObjectPath().ToString());
		return nullptr;
	}

	return surfaceCatalog;
}

bool UScenarioAuthoringSubsystem::TryFindCorridorSurfaceEntry(
	FName surfaceId,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry) const
{
	outSurfaceEntry = FScenarioCorridorSurfaceEntry();
	if (surfaceId.IsNone())
	{
		return false;
	}

	if (const UScenarioCorridorSurfaceCatalog* surfaceCatalog = GetCorridorSurfaceCatalog())
	{
		if (surfaceCatalog->FindSurfaceEntryById(surfaceId, outSurfaceEntry))
		{
			return true;
		}
	}

	return UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(surfaceId, outSurfaceEntry);
}

double UScenarioAuthoringSubsystem::ComputePlacementRadius2D(const FScenarioStaticObstaclePropEntry& propEntry) const
{
	if (propEntry.SafetyRadius > 0.0)
	{
		return propEntry.SafetyRadius;
	}

	return FMath::Sqrt(FMath::Square(propEntry.FallbackBoxExtent.X) + FMath::Square(propEntry.FallbackBoxExtent.Y));
}

FVector2D UScenarioAuthoringSubsystem::ComputePlacementHalfExtent2D(
	const FScenarioStaticObstaclePropEntry& propEntry) const
{
	const FVector2D halfExtent(
		FMath::Max(propEntry.FallbackBoxExtent.X, 0.0),
		FMath::Max(propEntry.FallbackBoxExtent.Y, 0.0));

	if (halfExtent.X > KINDA_SMALL_NUMBER || halfExtent.Y > KINDA_SMALL_NUMBER)
	{
		return halfExtent;
	}

	const double fallbackRadius = ComputePlacementRadius2D(propEntry);
	return FVector2D(fallbackRadius, fallbackRadius);
}

bool UScenarioAuthoringSubsystem::StaticObstacleFootprintsOverlap(
	const FVector& candidateLocation,
	const FVector2D& candidateHalfExtent,
	const FScenarioAuthoringStaticObstacleRecord& record) const
{
	const FVector recordLocation = record.Transform.GetLocation();
	FVector2D recordHalfExtent = record.PlacementHalfExtent2D;
	if (recordHalfExtent.X <= KINDA_SMALL_NUMBER && recordHalfExtent.Y <= KINDA_SMALL_NUMBER)
	{
		recordHalfExtent = FVector2D(record.PlacementRadius2D, record.PlacementRadius2D);
	}

	const double allowedDeltaX =
		candidateHalfExtent.X + recordHalfExtent.X + StaticObstacleFootprintClearanceCm;
	const double allowedDeltaY =
		candidateHalfExtent.Y + recordHalfExtent.Y + StaticObstacleFootprintClearanceCm;
	const double deltaX = FMath::Abs(candidateLocation.X - recordLocation.X);
	const double deltaY = FMath::Abs(candidateLocation.Y - recordLocation.Y);

	return deltaX < allowedDeltaX && deltaY < allowedDeltaY;
}

FString UScenarioAuthoringSubsystem::GenerateStaticObstacleInstanceId()
{
	FString instanceId;
	do
	{
		instanceId = FString::Printf(TEXT("obstacle_%03d"), NextStaticObstacleIndex++);
	}
	while (ContainsInstanceId(instanceId));

	return instanceId;
}

FString UScenarioAuthoringSubsystem::GeneratePedestrianInstanceId()
{
	FString instanceId;
	do
	{
		instanceId = FString::Printf(TEXT("ped_%03d"), NextPedestrianIndex++);
	}
	while (ContainsInstanceId(instanceId));

	return instanceId;
}

bool UScenarioAuthoringSubsystem::ContainsInstanceId(const FString& instanceId) const
{
	if (DraftScenarioTemplate.Obstacles.Placements.ContainsByPredicate(
			[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
			{
				return placement.PlacementId == instanceId;
			}))
	{
		return true;
	}

	for (const FScenarioDynamicActorSpec& spec : DraftPedestrianSpecs)
	{
		if (spec.InstanceId == instanceId)
		{
			return true;
		}
	}

	return false;
}

bool UScenarioAuthoringSubsystem::IsDraftScenarioTemplateEmpty() const
{
	return DraftScenarioTemplate.TemplateId.IsEmpty()
		&& DraftScenarioTemplate.Intent.IsEmpty()
		&& DraftScenarioTemplate.Corridor.Axis.PointsMeters.IsEmpty()
		&& DraftScenarioTemplate.Corridor.Segments.IsEmpty()
		&& DraftScenarioTemplate.Obstacles.Placements.IsEmpty()
		&& DraftGroundRegions.IsEmpty()
		&& DraftPedestrianSpecs.IsEmpty();
}

void UScenarioAuthoringSubsystem::InitializeDraftDefaults()
{
	DraftScenarioTemplate = FScenarioTemplateDocument();
	DraftScenarioTemplate.TemplateId = ScenarioId;
	DraftScenarioTemplate.Intent = TEXT("Editor authored scenario template.");
	DraftScenarioTemplate.Corridor.Axis.Type = EScenarioCorridorAxisType::Polyline;
	DraftScenarioTemplate.Corridor.Axis.PointsMeters =
	{
		FVector2D(DefaultRobotStartLocationCm.X * CentimetersToMeters, DefaultRobotStartLocationCm.Y * CentimetersToMeters),
		FVector2D(DefaultRobotGoalLocationCm.X * CentimetersToMeters, DefaultRobotGoalLocationCm.Y * CentimetersToMeters)
	};
	DraftScenarioTemplate.Corridor.WalkwayWidthMeters = MakeFixedTemplateNumber(3.0);

	FScenarioTemplateSegment mainSegment;
	mainSegment.SegmentId = TEXT("main");
	mainSegment.Type = EScenarioTemplateSegmentType::Straight;
	mainSegment.AlongRangeMeters.StartMeters = 0.0;
	mainSegment.AlongRangeMeters.EndMeters =
		MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);
	DraftScenarioTemplate.Corridor.Segments.Add(mainSegment);

	FScenarioTemplateLaneRule buildingLane;
	buildingLane.SurfaceId = TEXT("building");
	buildingLane.WidthMeters = MakeFixedTemplateNumber(1.0);
	DraftScenarioTemplate.Corridor.BuildingSide.Add(buildingLane);

	FScenarioTemplateLaneRule curbLane;
	curbLane.SurfaceId = TEXT("road");
	curbLane.WidthMeters = MakeFixedTemplateNumber(1.0);
	DraftScenarioTemplate.Corridor.CurbSide.Add(curbLane);

	DraftScenarioTemplate.Obstacles.MinClearWidthMeters = MakeFixedTemplateNumber(1.0);
	DraftScenarioTemplate.Pedestrians.Background.Count = MakeFixedTemplateInteger(0);
	DraftScenarioTemplate.Pedestrians.Background.SpeedMetersPerSecond = MakeFixedTemplateNumber(1.2);
	DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(DefaultRobotStartLocationCm);
	DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(DefaultRobotGoalLocationCm);
}

bool UScenarioAuthoringSubsystem::EnsureSingleRobotRouteSpec(
	TArray<FString>& outDiagnostics,
	bool& bOutDraftChanged)
{
	bOutDraftChanged = false;

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
		bOutDraftChanged = true;
		outDiagnostics.Add(TEXT("Robot route was missing; default StartPoint and GoalPoint were added."));
	}

	if (DraftScenarioTemplate.TemplateId.IsEmpty())
	{
		DraftScenarioTemplate.TemplateId = ScenarioId;
		bOutDraftChanged = true;
	}

	if (DraftScenarioTemplate.Intent.IsEmpty())
	{
		DraftScenarioTemplate.Intent = TEXT("Editor authored scenario template.");
		bOutDraftChanged = true;
	}

	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() < 2)
	{
		DraftScenarioTemplate.Corridor.Axis.PointsMeters =
		{
			FVector2D(DefaultRobotStartLocationCm.X * CentimetersToMeters, DefaultRobotStartLocationCm.Y * CentimetersToMeters),
			FVector2D(DefaultRobotGoalLocationCm.X * CentimetersToMeters, DefaultRobotGoalLocationCm.Y * CentimetersToMeters)
		};
		bOutDraftChanged = true;
	}

	if (DraftScenarioTemplate.Corridor.Segments.IsEmpty())
	{
		FScenarioTemplateSegment mainSegment;
		mainSegment.SegmentId = TEXT("main");
		mainSegment.Type = EScenarioTemplateSegmentType::Straight;
		mainSegment.AlongRangeMeters.StartMeters = 0.0;
		mainSegment.AlongRangeMeters.EndMeters =
			MeasureCorridorAxisLengthMeters(DraftScenarioTemplate.Corridor.Axis.PointsMeters);
		DraftScenarioTemplate.Corridor.Segments.Add(mainSegment);
		bOutDraftChanged = true;
	}

	if (!DraftScenarioTemplate.Corridor.WalkwayWidthMeters.bIsSet)
	{
		DraftScenarioTemplate.Corridor.WalkwayWidthMeters = MakeFixedTemplateNumber(3.0);
		bOutDraftChanged = true;
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateSingleRobotRouteSpecForExport(TArray<FString>& outDiagnostics) const
{
	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() < 2)
	{
		outDiagnostics.Add(TEXT("Robot route axis must contain at least two points."));
		return false;
	}

	return true;
}

FScenarioWorldSpec UScenarioAuthoringSubsystem::BuildDraftWorldSpecForPreview(TArray<FString>* outDiagnostics) const
{
	FScenarioWorldSpec fallbackWorldSpec = BuildCompatibilityDraftWorldSpecForPreview();

	FScenarioTemplateSampleRequest sampleRequest;
	sampleRequest.SampleId = TEXT("editor_preview");
	sampleRequest.ScenarioId = DraftScenarioTemplate.TemplateId.IsEmpty()
		? ScenarioId
		: DraftScenarioTemplate.TemplateId;
	sampleRequest.Seed = BaseSeed + IterationIndex;
	sampleRequest.TemplateRef = SourceScenarioTemplateJsonPath.IsEmpty()
		? TEXT("editor_draft.template.json")
		: SourceScenarioTemplateJsonPath;
	sampleRequest.TemplateHash = FString::Printf(
		TEXT("editor_preview:%08x"),
		FCrc::StrCrc32(*(sampleRequest.TemplateRef + DraftScenarioTemplate.TemplateId)));
	sampleRequest.ProfileRef = TEXT("editor_preview_profile");
	sampleRequest.ProfileHash = TEXT("editor_preview_profile_hash");
	sampleRequest.SettingRef = TEXT("editor_preview_setting");
	sampleRequest.SettingHash = TEXT("editor_preview_setting_hash");
	sampleRequest.GeneratorVersion = FScenarioTemplateSampler::GeneratorVersion;

	const FScenarioTemplateSampleResult sampleResult =
		FScenarioTemplateSampler::GenerateSample(DraftScenarioTemplate, sampleRequest);
	if (!sampleResult.bSuccess)
	{
		if (outDiagnostics)
		{
			AppendSchemaDiagnostics(sampleResult.Diagnostics, *outDiagnostics);
			outDiagnostics->Add(TEXT("ScenarioTemplate sampler preview failed; using compatibility preview projection."));
		}
		return fallbackWorldSpec;
	}

	FScenarioCompileResult compileResult =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(sampleResult.Document);
	if (!compileResult.bSuccess)
	{
		if (outDiagnostics)
		{
			AppendCompileDiagnostics(compileResult, *outDiagnostics);
			outDiagnostics->Add(TEXT("ScenarioSample world spec preview failed; using compatibility preview projection."));
		}
		return fallbackWorldSpec;
	}

	if (outDiagnostics)
	{
		AppendSchemaDiagnostics(sampleResult.Diagnostics, *outDiagnostics);
		AppendCompileDiagnostics(compileResult, *outDiagnostics);
	}

	FScenarioWorldSpec worldSpec = compileResult.WorldSpec;
	ApplyEditorPreviewRunConfig(worldSpec);
	worldSpec.GroundRegions.Append(DraftGroundRegions);
	worldSpec.DynamicActors.Append(DraftPedestrianSpecs);
	return worldSpec;
}

FScenarioWorldSpec UScenarioAuthoringSubsystem::BuildCompatibilityDraftWorldSpecForPreview() const
{
	FScenarioWorldSpec worldSpec;
	ApplyEditorPreviewRunConfig(worldSpec);

	worldSpec.Placeables.Add(MakeDeliveryBotSpecFromTemplateRobot());
	for (const FScenarioTemplateObstaclePlacement& placement : DraftScenarioTemplate.Obstacles.Placements)
	{
		if (placement.Kind != EScenarioTemplateObstaclePlacementKind::Fixed || placement.PropId.IsEmpty())
		{
			continue;
		}
		worldSpec.Placeables.Add(MakeStaticObstacleSpecFromPlacement(placement));
	}
	worldSpec.GroundRegions = DraftGroundRegions;
	worldSpec.DynamicActors = DraftPedestrianSpecs;
	return worldSpec;
}

void UScenarioAuthoringSubsystem::ApplyEditorPreviewRunConfig(FScenarioWorldSpec& worldSpec) const
{
	worldSpec.RunConfig.TemplateId = DraftScenarioTemplate.TemplateId.IsEmpty() ? ScenarioId : DraftScenarioTemplate.TemplateId;
	worldSpec.RunConfig.TemplateVersion = DraftScenarioTemplate.Version > 0 ? DraftScenarioTemplate.Version : FScenarioTemplateJson::SupportedVersion;
	worldSpec.RunConfig.GeneratorVersion = FScenarioTemplateJson::SupportedVersion;
	worldSpec.RunConfig.BaseSeed = BaseSeed;
	worldSpec.RunConfig.IterationIndex = IterationIndex;

	FScenarioParamValue timeLimitParam;
	timeLimitParam.Type = EScenarioParamValueType::Float;
	timeLimitParam.FloatValue = TimeLimitSeconds;
	worldSpec.RunConfig.Parameters.Add(TEXT("time_limit_s"), timeLimitParam);

	worldSpec.Seeds.WorldSeed = BaseSeed;
	worldSpec.Seeds.LayoutSeed = BaseSeed + 101;
	worldSpec.Seeds.StaticObstacleSeed = BaseSeed + 202;
	worldSpec.Seeds.DynamicActorSeed = BaseSeed + 303;
	worldSpec.Seeds.EventSeed = BaseSeed + 404;
	worldSpec.Seeds.PolicySeed = BaseSeed + 505;
}

FScenarioTemplateRobotAnchor UScenarioAuthoringSubsystem::MakeRobotAnchorFromLocationCm(const FVector& locationCm) const
{
	FScenarioTemplateRobotAnchor anchor;
	anchor.Type = EScenarioTemplateRobotAnchorType::CorridorPose;
	double alongMeters = locationCm.X * CentimetersToMeters;
	double offsetMeters = locationCm.Y * CentimetersToMeters;
	FString segmentId = DraftScenarioTemplate.Corridor.Segments.IsEmpty()
		? TEXT("main")
		: DraftScenarioTemplate.Corridor.Segments[0].SegmentId;
	TryProjectLocationToCorridor(locationCm, alongMeters, offsetMeters, segmentId);
	anchor.SegmentId = segmentId;
	anchor.AlongMeters = MakeFixedTemplateNumber(alongMeters);
	anchor.OffsetMeters = MakeFixedTemplateNumber(offsetMeters);
	anchor.LaneId = TEXT("walkway");
	anchor.Heading = EScenarioTemplateRobotHeading::Forward;
	return anchor;
}

FVector UScenarioAuthoringSubsystem::ResolveRobotAnchorLocationCm(
	const FScenarioTemplateRobotAnchor& anchor,
	bool bGoalAnchor) const
{
	if (anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		FVector2D pointMeters;
		double yawDegrees = 0.0;
		if (TryResolveCorridorPoseMeters(
				GetFixedTemplateNumber(anchor.AlongMeters, bGoalAnchor ? DefaultRobotGoalLocationCm.X * CentimetersToMeters : DefaultRobotStartLocationCm.X * CentimetersToMeters),
				GetFixedTemplateNumber(anchor.OffsetMeters, bGoalAnchor ? DefaultRobotGoalLocationCm.Y * CentimetersToMeters : DefaultRobotStartLocationCm.Y * CentimetersToMeters),
				pointMeters,
				yawDegrees))
		{
			return FVector(pointMeters.X / CentimetersToMeters, pointMeters.Y / CentimetersToMeters, 0.0);
		}

		return FVector(
			GetFixedTemplateNumber(anchor.AlongMeters, bGoalAnchor ? DefaultRobotGoalLocationCm.X * CentimetersToMeters : DefaultRobotStartLocationCm.X * CentimetersToMeters) / CentimetersToMeters,
			GetFixedTemplateNumber(anchor.OffsetMeters, bGoalAnchor ? DefaultRobotGoalLocationCm.Y * CentimetersToMeters : DefaultRobotStartLocationCm.Y * CentimetersToMeters) / CentimetersToMeters,
			0.0);
	}

	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() >= 2)
	{
		const FVector2D axisPointMeters = anchor.Type == EScenarioTemplateRobotAnchorType::Exit
			? DraftScenarioTemplate.Corridor.Axis.PointsMeters.Last()
			: DraftScenarioTemplate.Corridor.Axis.PointsMeters[0];
		return FVector(
			axisPointMeters.X / CentimetersToMeters,
			axisPointMeters.Y / CentimetersToMeters,
			0.0);
	}

	return bGoalAnchor ? DefaultRobotGoalLocationCm : DefaultRobotStartLocationCm;
}

FScenarioTemplateObstaclePlacement UScenarioAuthoringSubsystem::MakeStaticObstaclePlacement(
	const FString& placementId,
	FName propId,
	const FTransform& transform) const
{
	FScenarioTemplateObstaclePlacement placement;
	placement.PlacementId = placementId;
	placement.Kind = EScenarioTemplateObstaclePlacementKind::Fixed;
	placement.PropId = propId.ToString();
	double alongMeters = transform.GetLocation().X * CentimetersToMeters;
	double offsetMeters = transform.GetLocation().Y * CentimetersToMeters;
	FString segmentId = DraftScenarioTemplate.Corridor.Segments.IsEmpty()
		? TEXT("main")
		: DraftScenarioTemplate.Corridor.Segments[0].SegmentId;
	TryProjectLocationToCorridor(transform.GetLocation(), alongMeters, offsetMeters, segmentId);
	placement.At.SegmentId = segmentId;
	placement.At.AlongMeters = MakeFixedTemplateNumber(alongMeters);
	placement.At.OffsetMeters = MakeFixedTemplateNumber(offsetMeters);
	placement.At.LaneId = TEXT("walkway");
	FVector2D pointMeters;
	double axisYawDegrees = 0.0;
	TryResolveCorridorPoseMeters(alongMeters, offsetMeters, pointMeters, axisYawDegrees);
	placement.YawDegrees = MakeFixedTemplateNumber(FRotator::ClampAxis(transform.Rotator().Yaw - axisYawDegrees));
	return placement;
}

FScenarioPlaceableInstanceSpec UScenarioAuthoringSubsystem::MakeStaticObstacleSpecFromPlacement(
	const FScenarioTemplateObstaclePlacement& placement) const
{
	FScenarioPlaceableInstanceSpec spec;
	spec.InstanceId = placement.PlacementId;
	spec.AssetId = placement.PropId;
	spec.Category = EScenarioActorCategory::StaticObstacle;
	const double alongMeters = GetFixedTemplateNumber(placement.At.AlongMeters, 0.0);
	const double offsetMeters = GetFixedTemplateNumber(placement.At.OffsetMeters, 0.0);
	const double localYawDegrees = GetFixedTemplateNumber(placement.YawDegrees, 0.0);
	FVector2D pointMeters;
	double axisYawDegrees = 0.0;
	FVector locationCm(alongMeters / CentimetersToMeters, offsetMeters / CentimetersToMeters, 0.0);
	if (TryResolveCorridorPoseMeters(alongMeters, offsetMeters, pointMeters, axisYawDegrees))
	{
		locationCm = FVector(
			pointMeters.X / CentimetersToMeters,
			pointMeters.Y / CentimetersToMeters,
			ResolveCorridorSurfaceZOffsetCm(offsetMeters));
	}
	spec.Transform = FTransform(FRotator(0.0, FRotator::ClampAxis(axisYawDegrees + localYawDegrees), 0.0), locationCm);
	return spec;
}

FScenarioPlaceableInstanceSpec UScenarioAuthoringSubsystem::MakeDeliveryBotSpecFromTemplateRobot() const
{
	const FVector startLocationCm = ResolveRobotAnchorLocationCm(DraftScenarioTemplate.Robot.Start, false);
	const FVector goalLocationCm = ResolveRobotAnchorLocationCm(DraftScenarioTemplate.Robot.Goal, true);

	FScenarioPlaceableInstanceSpec robotSpec;
	robotSpec.InstanceId = DefaultRobotInstanceId;
	robotSpec.AssetId = DefaultRobotAssetId;
	robotSpec.Category = EScenarioActorCategory::DeliveryBot;
	robotSpec.Transform = FTransform(FRotator::ZeroRotator, startLocationCm);
	robotSpec.DeliveryBot.bSpawnOnly = false;
	robotSpec.DeliveryBot.bHasStartLocation = true;
	robotSpec.DeliveryBot.bHasGoalLocation = true;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = startLocationCm;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = goalLocationCm;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
	return robotSpec;
}

FScenarioTemplateObstaclePlacement* UScenarioAuthoringSubsystem::FindStaticObstaclePlacementByInstanceId(
	const FString& instanceId)
{
	return DraftScenarioTemplate.Obstacles.Placements.FindByPredicate(
		[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
		{
			return placement.PlacementId == instanceId;
		});
}

const FScenarioTemplateObstaclePlacement* UScenarioAuthoringSubsystem::FindStaticObstaclePlacementByInstanceId(
	const FString& instanceId) const
{
	return DraftScenarioTemplate.Obstacles.Placements.FindByPredicate(
		[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
		{
			return placement.PlacementId == instanceId;
		});
}

void UScenarioAuthoringSubsystem::ImportWorldSpecAsScenarioTemplate(const FScenarioWorldSpec& worldSpec)
{
	InitializeDraftDefaults();
	DraftScenarioTemplate.TemplateId = worldSpec.RunConfig.TemplateId.IsEmpty() ? ScenarioId : worldSpec.RunConfig.TemplateId;
	DraftScenarioTemplate.Version = worldSpec.RunConfig.TemplateVersion > 0 ? worldSpec.RunConfig.TemplateVersion : FScenarioTemplateJson::SupportedVersion;
	DraftScenarioTemplate.Obstacles.Placements.Reset();
	DraftGroundRegions = worldSpec.GroundRegions;
	DraftPedestrianSpecs = worldSpec.DynamicActors;

	for (const FScenarioPlaceableInstanceSpec& spec : worldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(spec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm);
			if (spec.DeliveryBot.bHasGoalLocation)
			{
				DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(spec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm);
			}
			continue;
		}

		if (spec.Category == EScenarioActorCategory::StaticObstacle)
		{
			DraftScenarioTemplate.Obstacles.Placements.Add(
				MakeStaticObstaclePlacement(spec.InstanceId, FName(*spec.AssetId), spec.Transform));
		}
	}
}

void UScenarioAuthoringSubsystem::ClearGeneratedEditorPreviewActors()
{
	for (const TObjectPtr<AActor>& markerActor : RouteMarkerActors)
	{
		if (IsValid(markerActor))
		{
			markerActor->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AScenarioStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AActor>>& pair : PedestrianActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AScenarioGroundRegion>>& pair : GroundRegionActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	if (IsValid(CorridorPreviewActor))
	{
		CorridorPreviewActor->Destroy();
	}
	CorridorPreviewActor = nullptr;
	RouteMarkerActors.Reset();
	RobotStartMarkerActor = nullptr;
	RobotGoalMarkerActor = nullptr;
	StaticObstacleRecords.Reset();
	StaticObstacleActors.Reset();
	PedestrianActors.Reset();
	GroundRegionActors.Reset();
	NextStaticObstacleIndex = 1;
	NextPedestrianIndex = 1;
	NextGroundRegionIndex = 1;
}

void UScenarioAuthoringSubsystem::ClearCorridorHandleActors()
{
	for (const TPair<FString, TObjectPtr<AScenarioCorridorHandleActor>>& pair : CorridorHandleActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	CorridorHandleActors.Reset();
}

void UScenarioAuthoringSubsystem::ClearEditorView()
{
	ClearGeneratedEditorPreviewActors();
	ClearCorridorHandleActors();
}

bool UScenarioAuthoringSubsystem::RebuildEditorViewFromDraft(TArray<FString>& outDiagnostics)
{
	ClearEditorView();
	bool bSucceeded = true;
	if (!SpawnCorridorHandleActors(outDiagnostics))
	{
		bSucceeded = false;
	}

	if (!RefreshGeneratedEditorPreviewActorsFromDraft(outDiagnostics))
	{
		bSucceeded = false;
	}

	return bSucceeded;
}

bool UScenarioAuthoringSubsystem::RefreshGeneratedEditorPreviewActorsFromDraft(TArray<FString>& outDiagnostics)
{
	bool bDraftChanged = false;
	if (!EnsureSingleRobotRouteSpec(outDiagnostics, bDraftChanged))
	{
		return false;
	}
	if (bDraftChanged)
	{
		bDirty = true;
	}

	ClearGeneratedEditorPreviewActors();
	const bool bHasSplineCorridorPreview = SpawnCorridorPreviewActor(outDiagnostics);
	const FScenarioWorldSpec previewWorldSpec = BuildDraftWorldSpecForPreview(&outDiagnostics);

	bool bSucceeded = true;
	for (const FScenarioPlaceableInstanceSpec& spec : previewWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			if (!SpawnRobotRouteMarkers(spec, outDiagnostics))
			{
				bSucceeded = false;
			}
			continue;
		}

		if (spec.Category != EScenarioActorCategory::StaticObstacle)
		{
			continue;
		}

		FScenarioPlaceableInstanceSpec resolvedSpec = spec;
		resolvedSpec.Transform = ResolveStaticObstaclePlacementTransform(spec.Transform);

		AScenarioStaticObstacle* spawnedActor = nullptr;
		FString failureReason;
		if (!SpawnEditorStaticObstacleActor(resolvedSpec, spawnedActor, failureReason))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Failed to create editor view for static obstacle '%s': %s"),
				*spec.InstanceId,
				*failureReason));
			bSucceeded = false;
			continue;
		}

		FScenarioStaticObstaclePropEntry propEntry;
		if (TryFindStaticObstacleProp(FName(*resolvedSpec.AssetId), propEntry))
		{
			AddStaticObstacleViewRecord(resolvedSpec, propEntry, spawnedActor);
		}
	}

	for (const FScenarioDynamicActorSpec& spec : previewWorldSpec.DynamicActors)
	{
		if (spec.Category != EScenarioActorCategory::Pedestrian)
		{
			continue;
		}

		AActor* spawnedActor = nullptr;
		FString failureReason;
		if (!SpawnEditorPedestrianActor(spec, spawnedActor, failureReason))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Failed to create editor view for pedestrian '%s': %s"),
				*spec.InstanceId,
				*failureReason));
			bSucceeded = false;
			continue;
		}

		AddPedestrianViewRecord(spec, spawnedActor);
	}

	for (const FScenarioGroundRegionSpec& regionSpec : previewWorldSpec.GroundRegions)
	{
		if (bHasSplineCorridorPreview && !ContainsGroundRegionId(regionSpec.RegionId))
		{
			continue;
		}

		AScenarioGroundRegion* spawnedRegion = nullptr;
		FString failureReason;
		if (!SpawnEditorGroundRegionActor(regionSpec, spawnedRegion, failureReason))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Failed to create editor view for ground region '%s': %s"),
				*regionSpec.RegionId,
				*failureReason));
			bSucceeded = false;
		}
	}

	return bSucceeded;
}

bool UScenarioAuthoringSubsystem::SpawnCorridorPreviewActor(TArray<FString>& outDiagnostics)
{
	UWorld* world = GetWorld();
	if (!world)
	{
		outDiagnostics.Add(TEXT("World is unavailable; corridor spline preview was not spawned."));
		return false;
	}

	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() < 2)
	{
		return false;
	}

	FString failureReason;
	if (!AreCorridorAxisPointsValid(DraftScenarioTemplate.Corridor.Axis.PointsMeters, failureReason))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Corridor spline preview skipped: %s"), *failureReason));
		return false;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CorridorPreviewActor = world->SpawnActor<AScenarioCorridorPreviewActor>(
		AScenarioCorridorPreviewActor::StaticClass(),
		FTransform::Identity,
		spawnParams);
	if (!CorridorPreviewActor)
	{
		outDiagnostics.Add(TEXT("Failed to spawn corridor spline preview actor."));
		return false;
	}

	CorridorPreviewActor->SurfaceCatalog = CorridorSurfaceCatalog;
	CorridorPreviewActor->ConfigureFromCorridor(DraftScenarioTemplate.Corridor);
	return CorridorPreviewActor->HasRenderableCorridor();
}

bool UScenarioAuthoringSubsystem::SpawnCorridorHandleActors(TArray<FString>& outDiagnostics)
{
	ClearCorridorHandleActors();

	UWorld* world = GetWorld();
	if (!world)
	{
		outDiagnostics.Add(TEXT("World is unavailable; corridor handles were not spawned."));
		return false;
	}

	const TArray<FVector2D>& pointsMeters = DraftScenarioTemplate.Corridor.Axis.PointsMeters;
	if (pointsMeters.Num() < 2)
	{
		return true;
	}

	bool bSucceeded = true;
	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 vertexIndex = 0; vertexIndex < pointsMeters.Num(); ++vertexIndex)
	{
		const FString handleId = MakeCorridorVertexHandleId(vertexIndex);
		AScenarioCorridorHandleActor* handleActor = world->SpawnActor<AScenarioCorridorHandleActor>(
			AScenarioCorridorHandleActor::StaticClass(),
			MakeCorridorVertexHandleTransform(pointsMeters[vertexIndex]),
			spawnParams);
		if (!handleActor)
		{
			outDiagnostics.Add(FString::Printf(TEXT("Failed to spawn corridor vertex handle '%s'."), *handleId));
			bSucceeded = false;
			continue;
		}

		handleActor->ConfigureVertexHandle(vertexIndex, handleId, MakeCorridorVertexHandleTransform(pointsMeters[vertexIndex]));
		CorridorHandleActors.Add(handleId, handleActor);
	}

	for (int32 segmentIndex = 0; segmentIndex < pointsMeters.Num() - 1; ++segmentIndex)
	{
		const FString handleId = MakeCorridorSegmentHandleId(segmentIndex);
		AScenarioCorridorHandleActor* handleActor = world->SpawnActor<AScenarioCorridorHandleActor>(
			AScenarioCorridorHandleActor::StaticClass(),
			MakeCorridorSegmentHandleTransform(pointsMeters[segmentIndex], pointsMeters[segmentIndex + 1]),
			spawnParams);
		if (!handleActor)
		{
			outDiagnostics.Add(FString::Printf(TEXT("Failed to spawn corridor segment handle '%s'."), *handleId));
			bSucceeded = false;
			continue;
		}

		handleActor->ConfigureSegmentHandle(
			segmentIndex,
			handleId,
			MakeCorridorSegmentHandleTransform(pointsMeters[segmentIndex], pointsMeters[segmentIndex + 1]),
			(pointsMeters[segmentIndex + 1] - pointsMeters[segmentIndex]).Size() / CentimetersToMeters);
		CorridorHandleActors.Add(handleId, handleActor);
	}

	return bSucceeded;
}

void UScenarioAuthoringSubsystem::SyncCorridorHandleActors()
{
	const TArray<FVector2D>& pointsMeters = DraftScenarioTemplate.Corridor.Axis.PointsMeters;
	TSet<FString> expectedHandleIds;

	for (int32 vertexIndex = 0; vertexIndex < pointsMeters.Num(); ++vertexIndex)
	{
		const FString handleId = MakeCorridorVertexHandleId(vertexIndex);
		expectedHandleIds.Add(handleId);
		if (TObjectPtr<AScenarioCorridorHandleActor>* handleActor = CorridorHandleActors.Find(handleId))
		{
			if (IsValid(*handleActor))
			{
				(*handleActor)->ConfigureVertexHandle(
					vertexIndex,
					handleId,
					MakeCorridorVertexHandleTransform(pointsMeters[vertexIndex]));
			}
		}
	}

	for (int32 segmentIndex = 0; segmentIndex < pointsMeters.Num() - 1; ++segmentIndex)
	{
		const FString handleId = MakeCorridorSegmentHandleId(segmentIndex);
		expectedHandleIds.Add(handleId);
		if (TObjectPtr<AScenarioCorridorHandleActor>* handleActor = CorridorHandleActors.Find(handleId))
		{
			if (IsValid(*handleActor))
			{
				(*handleActor)->ConfigureSegmentHandle(
					segmentIndex,
					handleId,
					MakeCorridorSegmentHandleTransform(pointsMeters[segmentIndex], pointsMeters[segmentIndex + 1]),
					(pointsMeters[segmentIndex + 1] - pointsMeters[segmentIndex]).Size() / CentimetersToMeters);
			}
		}
	}

	for (auto iterator = CorridorHandleActors.CreateIterator(); iterator; ++iterator)
	{
		AScenarioCorridorHandleActor* handleActor = iterator.Value().Get();
		if (!expectedHandleIds.Contains(iterator.Key()) || !IsValid(handleActor))
		{
			if (IsValid(handleActor))
			{
				handleActor->Destroy();
			}
			iterator.RemoveCurrent();
		}
	}
}

bool UScenarioAuthoringSubsystem::SpawnRobotRouteMarkers(
	const FScenarioPlaceableInstanceSpec& spec,
	TArray<FString>& outDiagnostics)
{
	if (!StartPointClass)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("StartPointClass is not set; robot start marker was not spawned for '%s'."),
			*spec.InstanceId));
		return false;
	}

	AActor* startMarker = SpawnEditorMarkerActor(StartPointClass, FTransform(spec.Transform));
	if (!startMarker)
	{
		outDiagnostics.Add(FString::Printf(TEXT("Failed to spawn robot start marker for '%s'."), *spec.InstanceId));
		return false;
	}
	FString markerFailureReason;
	if (!ConfigureRobotRouteMarkerActor(
		startMarker,
		EScenarioPlaceableAuthoringRole::RobotStartMarker,
		markerFailureReason))
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Failed to configure robot start marker for '%s': %s"),
			*spec.InstanceId,
			*markerFailureReason));
		startMarker->Destroy();
		return false;
	}

	RobotStartMarkerActor = startMarker;
	RouteMarkerActors.Add(startMarker);
	const auto cleanupStartMarker = [this, startMarker]()
	{
		RouteMarkerActors.RemoveAll(
			[startMarker](const TObjectPtr<AActor>& markerActor)
			{
				return !IsValid(markerActor) || markerActor.Get() == startMarker;
			});

		if (RobotStartMarkerActor.Get() == startMarker)
		{
			RobotStartMarkerActor = nullptr;
		}
		if (IsValid(startMarker))
		{
			startMarker->Destroy();
		}
	};

	if (!spec.DeliveryBot.bHasGoalLocation)
	{
		outDiagnostics.Add(FString::Printf(TEXT("Robot goal marker is missing for '%s'."), *spec.InstanceId));
		cleanupStartMarker();
		return false;
	}

	if (!GoalPointClass)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("GoalPointClass is not set; robot goal marker was not spawned for '%s'."),
			*spec.InstanceId));
		cleanupStartMarker();
		return false;
	}

	const FTransform goalTransform(FRotator::ZeroRotator, spec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm);
	AActor* goalMarker = SpawnEditorMarkerActor(GoalPointClass, goalTransform);
	if (!goalMarker)
	{
		outDiagnostics.Add(FString::Printf(TEXT("Failed to spawn robot goal marker for '%s'."), *spec.InstanceId));
		cleanupStartMarker();
		return false;
	}
	if (!ConfigureRobotRouteMarkerActor(
		goalMarker,
		EScenarioPlaceableAuthoringRole::RobotGoalMarker,
		markerFailureReason))
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Failed to configure robot goal marker for '%s': %s"),
			*spec.InstanceId,
			*markerFailureReason));
		goalMarker->Destroy();
		cleanupStartMarker();
		return false;
	}

	RobotGoalMarkerActor = goalMarker;
	RouteMarkerActors.Add(goalMarker);
	return true;
}

AActor* UScenarioAuthoringSubsystem::SpawnEditorMarkerActor(
	TSubclassOf<AActor> markerClass,
	const FTransform& transform)
{
	UWorld* world = GetWorld();
	if (!world || !markerClass) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return world->SpawnActor<AActor>(markerClass, transform, spawnParams);
}

AActor* UScenarioAuthoringSubsystem::SpawnOrReplaceRouteMarker(
	TObjectPtr<AActor>& markerActor,
	TSubclassOf<AActor> markerClass,
	const FTransform& transform,
	EScenarioPlaceableAuthoringRole markerRole,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!markerClass)
	{
		outFailureReason = TEXT("Marker actor class is not set.");
		return nullptr;
	}

	AActor* spawnedMarker = SpawnEditorMarkerActor(markerClass, transform);
	if (!spawnedMarker)
	{
		outFailureReason = TEXT("Failed to spawn marker actor.");
		return nullptr;
	}
	if (!ConfigureRobotRouteMarkerActor(spawnedMarker, markerRole, outFailureReason))
	{
		spawnedMarker->Destroy();
		return nullptr;
	}

	RouteMarkerActors.RemoveAll(
		[&markerActor](const TObjectPtr<AActor>& existingMarker)
		{
			return !IsValid(existingMarker) || existingMarker.Get() == markerActor.Get();
		});

	if (IsValid(markerActor))
	{
		markerActor->Destroy();
	}

	markerActor = spawnedMarker;
	RouteMarkerActors.Add(markerActor);
	return markerActor.Get();
}

bool UScenarioAuthoringSubsystem::ConfigureRobotRouteMarkerActor(
	AActor* markerActor,
	EScenarioPlaceableAuthoringRole markerRole,
	FString& outFailureReason) const
{
	outFailureReason.Reset();
	if (!markerActor)
	{
		outFailureReason = TEXT("Marker actor is null.");
		return false;
	}

	UScenarioPlaceableComponent* placeableComponent =
		markerActor->FindComponentByClass<UScenarioPlaceableComponent>();
	if (!placeableComponent)
	{
		const FName componentName = MakeUniqueObjectName(
			markerActor,
			UScenarioPlaceableComponent::StaticClass(),
			TEXT("RouteMarkerPlaceableComponent"));
		placeableComponent = NewObject<UScenarioPlaceableComponent>(markerActor, componentName);
		if (!placeableComponent)
		{
			outFailureReason = TEXT("Failed to create route marker placeable component.");
			return false;
		}

		markerActor->AddInstanceComponent(placeableComponent);
		placeableComponent->RegisterComponent();
	}

	const bool bStartMarker = markerRole == EScenarioPlaceableAuthoringRole::RobotStartMarker;
	placeableComponent->InstanceId = bStartMarker ? RobotStartMarkerInstanceId : RobotGoalMarkerInstanceId;
	placeableComponent->AssetId = bStartMarker ? RobotStartMarkerAssetId : RobotGoalMarkerAssetId;
	placeableComponent->Category = EScenarioActorCategory::DeliveryBot;
	placeableComponent->AuthoringRole = markerRole;
	placeableComponent->bAuthoringSelectable = true;
	placeableComponent->bAuthoringRenamable = false;
	placeableComponent->bAuthoringDeletable = false;
	placeableComponent->bAuthoringAllowLocationEdit = true;
	placeableComponent->bAuthoringAllowRotationEdit = false;
	placeableComponent->bAuthoringAllowScaleEdit = false;

	USphereComponent* selectionComponent = nullptr;
	TArray<USphereComponent*> sphereComponents;
	markerActor->GetComponents(sphereComponents);
	const FName selectionComponentTag(TEXT("ScenarioRouteMarkerSelection"));
	for (USphereComponent* sphereComponent : sphereComponents)
	{
		if (sphereComponent && sphereComponent->ComponentTags.Contains(selectionComponentTag))
		{
			selectionComponent = sphereComponent;
			break;
		}
	}
	if (!selectionComponent)
	{
		const FName componentName = MakeUniqueObjectName(
			markerActor,
			USphereComponent::StaticClass(),
			TEXT("RouteMarkerSelectionComponent"));
		selectionComponent = NewObject<USphereComponent>(markerActor, componentName);
		if (selectionComponent)
		{
			markerActor->AddInstanceComponent(selectionComponent);
			if (USceneComponent* rootComponent = markerActor->GetRootComponent())
			{
				selectionComponent->SetupAttachment(rootComponent);
			}
			selectionComponent->RegisterComponent();
		}
	}
	if (!selectionComponent)
	{
		outFailureReason = TEXT("Failed to create route marker selection component.");
		return false;
	}
	if (selectionComponent)
	{
		selectionComponent->ComponentTags.AddUnique(selectionComponentTag);
		selectionComponent->SetSphereRadius(75.0f, true);
		selectionComponent->SetRelativeLocation(FVector::ZeroVector);
		selectionComponent->SetHiddenInGame(true);
		selectionComponent->SetVisibility(false);
		selectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		selectionComponent->SetCollisionObjectType(ECC_WorldDynamic);
		selectionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		selectionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		selectionComponent->SetGenerateOverlapEvents(false);
	}
	return true;
}

bool UScenarioAuthoringSubsystem::SpawnEditorStaticObstacleActor(
	const FScenarioPlaceableInstanceSpec& spec,
	AScenarioStaticObstacle*& outActor,
	FString& outFailureReason)
{
	outActor = nullptr;
	outFailureReason.Reset();

	UWorld* world = GetWorld();
	if (!world)
	{
		outFailureReason = TEXT("World is unavailable.");
		return false;
	}

	if (spec.InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("InstanceId is empty.");
		return false;
	}

	if (spec.AssetId.IsEmpty())
	{
		outFailureReason = TEXT("AssetId is empty.");
		return false;
	}

	TSubclassOf<AScenarioStaticObstacle> spawnClass = StaticObstacleClass;
	if (!spawnClass)
	{
		spawnClass = AScenarioStaticObstacle::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioStaticObstacle* staticObstacle = world->SpawnActor<AScenarioStaticObstacle>(
		spawnClass,
		spec.Transform,
		spawnParams);
	if (!staticObstacle)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return false;
	}

	FScenarioStaticObstaclePropEntry propEntry;
	if (!TryFindStaticObstacleProp(FName(*spec.AssetId), propEntry))
	{
		outFailureReason = FString::Printf(TEXT("Unknown prop '%s'."), *spec.AssetId);
		staticObstacle->Destroy();
		return false;
	}

	if (!staticObstacle->ApplyPropEntry(propEntry))
	{
		outFailureReason = FString::Printf(TEXT("Failed to apply prop '%s'."), *spec.AssetId);
		staticObstacle->Destroy();
		return false;
	}

	ConfigureAuthoredStaticObstacleActor(staticObstacle, spec);
	outActor = staticObstacle;
	return true;
}

bool UScenarioAuthoringSubsystem::SpawnEditorPedestrianActor(
	const FScenarioDynamicActorSpec& spec,
	AActor*& outActor,
	FString& outFailureReason)
{
	outActor = nullptr;
	outFailureReason.Reset();

	UWorld* world = GetWorld();
	if (!world)
	{
		outFailureReason = TEXT("World is unavailable.");
		return false;
	}

	if (spec.InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("InstanceId is empty.");
		return false;
	}

	TSubclassOf<AActor> spawnClass = PedestrianVisualizationActorClass;
	if (!spawnClass)
	{
		spawnClass = PedestrianClass.Get();
	}
	if (!spawnClass)
	{
		spawnClass = AScenarioPedestrian::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* pedestrian = world->SpawnActor<AActor>(
		spawnClass,
		spec.InitialTransform,
		spawnParams);
	if (!pedestrian)
	{
		outFailureReason = FString::Printf(
			TEXT("SpawnActor failed for pedestrian class '%s'."),
			*spawnClass->GetPathName());
		return false;
	}

	if (AScenarioPedestrian* episodePedestrian = Cast<AScenarioPedestrian>(pedestrian))
	{
		if (episodePedestrian->PathFollowerComponent)
		{
			episodePedestrian->PathFollowerComponent->bAutoStart = false;
			episodePedestrian->PathFollowerComponent->StopFollowing();
		}
		if (episodePedestrian->PedestrianRuntimeComponent)
		{
			episodePedestrian->PedestrianRuntimeComponent->bAutoStart = false;
			episodePedestrian->PedestrianRuntimeComponent->bEnableRobotReaction = false;
			episodePedestrian->PedestrianRuntimeComponent->StopFollowing();
		}
	}

	if (UScenarioPlaceableComponent* placeableComponent = pedestrian->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = spec.InstanceId;
		placeableComponent->AssetId = spec.AssetId;
		placeableComponent->Category = EScenarioActorCategory::Pedestrian;
		placeableComponent->bAuthoringSelectable = false;
	}
	else
	{
		UE_LOG(
			LogScenarioAuthoring,
			Verbose,
			TEXT("Spawned pedestrian editor actor has no ScenarioPlaceableComponent | Class: %s | InstanceId: %s"),
			*spawnClass->GetPathName(),
			*spec.InstanceId);
	}

	outActor = pedestrian;
	return true;
}

void UScenarioAuthoringSubsystem::AddStaticObstacleViewRecord(
	const FScenarioPlaceableInstanceSpec& spec,
	const FScenarioStaticObstaclePropEntry& propEntry,
	AScenarioStaticObstacle* actor)
{
	FScenarioAuthoringStaticObstacleRecord record;
	record.InstanceId = spec.InstanceId;
	record.PropId = FName(*spec.AssetId);
	record.Transform = spec.Transform;
	record.PlacementRadius2D = ComputePlacementRadius2D(propEntry);
	record.PlacementHalfExtent2D = ComputePlacementHalfExtent2D(propEntry);

	StaticObstacleRecords.Add(record);
	StaticObstacleActors.Add(spec.InstanceId, actor);
}

void UScenarioAuthoringSubsystem::AddPedestrianViewRecord(const FScenarioDynamicActorSpec& spec, AActor* actor)
{
	if (!actor || spec.InstanceId.IsEmpty())
	{
		return;
	}

	PedestrianActors.Add(spec.InstanceId, actor);
}

FScenarioPlaceableInstanceSpec UScenarioAuthoringSubsystem::MakeStaticObstacleSpec(
	const FString& instanceId,
	FName propId,
	const FTransform& transform) const
{
	FScenarioPlaceableInstanceSpec spec;
	spec.InstanceId = instanceId;
	spec.AssetId = propId.ToString();
	spec.Category = EScenarioActorCategory::StaticObstacle;
	spec.Transform = transform;
	return spec;
}

FScenarioDynamicActorSpec UScenarioAuthoringSubsystem::MakePedestrianSpec(
	const FString& instanceId,
	FName archetypeId,
	const FTransform& transform) const
{
	FScenarioDynamicActorSpec spec;
	spec.InstanceId = instanceId;
	spec.AssetId = archetypeId.IsNone() ? TEXT("adult_pedestrian") : archetypeId.ToString();
	spec.Category = EScenarioActorCategory::Pedestrian;
	spec.InitialTransform = transform;
	spec.Properties.Add(MovementModelKey, MakeStringParamValue(TEXT("static_placement")));
	spec.Properties.Add(AutoStartKey, MakeBoolParamValue(false));
	return spec;
}

FScenarioAuthoringStaticObstacleRecord* UScenarioAuthoringSubsystem::FindStaticObstacleRecordByInstanceId(
	const FString& instanceId)
{
	for (FScenarioAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (record.InstanceId == instanceId)
		{
			return &record;
		}
	}

	return nullptr;
}

const FScenarioAuthoringStaticObstacleRecord* UScenarioAuthoringSubsystem::FindStaticObstacleRecordByInstanceId(
	const FString& instanceId) const
{
	for (const FScenarioAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (record.InstanceId == instanceId)
		{
			return &record;
		}
	}

	return nullptr;
}

void UScenarioAuthoringSubsystem::ConfigureAuthoredStaticObstacleActor(
	AScenarioStaticObstacle* actor,
	const FScenarioPlaceableInstanceSpec& spec) const
{
	if (!actor) return;

	if (UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = spec.InstanceId;
		placeableComponent->AssetId = spec.AssetId;
		placeableComponent->Category = spec.Category;
		placeableComponent->AuthoringRole = EScenarioPlaceableAuthoringRole::Generic;
		placeableComponent->bAuthoringSelectable = true;
		placeableComponent->bAuthoringRenamable = true;
		placeableComponent->bAuthoringDeletable = true;
		placeableComponent->bAuthoringAllowLocationEdit = true;
		placeableComponent->bAuthoringAllowRotationEdit = true;
		placeableComponent->bAuthoringAllowScaleEdit = true;
	}
}
