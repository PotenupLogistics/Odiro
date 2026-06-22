#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorLaneWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorSegmentWidget.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace
{
	FString JoinCorridorPanelDiagnostics(const TArray<FString>& diagnostics)
	{
		return diagnostics.IsEmpty()
			? FString(TEXT("Unknown Corridor edit failure."))
			: FString::Join(diagnostics, TEXT("\n"));
	}
}

void UScenarioEditorSidebarCorridorPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFromDraft();
}

void UScenarioEditorSidebarCorridorPanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarCorridorPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorPanel::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarCorridorPanel::RefreshFromDraft()
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenario());
}

void UScenarioEditorSidebarCorridorPanel::RefreshFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;
	if (AxisTypeFieldRow)
	{
		AxisTypeFieldRow->SetValueText(AxisTypeToString(corridor.Axis.Type));
	}
	RefreshAxisPointRows(corridor.Axis.PointsMeters);
	if (WalkwayWidthFieldRow)
	{
		const FScenarioTemplateNumberValue& walkwayWidth = corridor.WalkwayWidthMeters;
		const double fixedDisplayValue = walkwayWidth.Mode == EScenarioTemplateNumberValueMode::Range
			? (walkwayWidth.MinValue + walkwayWidth.MaxValue) * 0.5
			: walkwayWidth.FixedValue;
		const double minDisplayValue = walkwayWidth.Mode == EScenarioTemplateNumberValueMode::Range
			? walkwayWidth.MinValue
			: fixedDisplayValue;
		const double maxDisplayValue = walkwayWidth.Mode == EScenarioTemplateNumberValueMode::Range
			? walkwayWidth.MaxValue
			: fixedDisplayValue;

		WalkwayWidthFieldRow->SetValueText(
			walkwayWidth.bIsSet ? FormatEditableNumber(fixedDisplayValue) : FString());
		WalkwayWidthFieldRow->SetRangeValueText(
			walkwayWidth.bIsSet ? FormatEditableNumber(minDisplayValue) : FString(),
			walkwayWidth.bIsSet ? FormatEditableNumber(maxDisplayValue) : FString());
		WalkwayWidthFieldRow->SetRangeInputEnabled(
			walkwayWidth.bIsSet
			&& walkwayWidth.Mode == EScenarioTemplateNumberValueMode::Range);
	}

	RefreshLaneProfileRows(
		EScenarioEditorCorridorSide::Building,
		BuildingSideBlockWidget.Get(),
		corridor.BuildingSide);
	RefreshLaneProfileRows(
		EScenarioEditorCorridorSide::Curb,
		CurbSideBlockWidget.Get(),
		corridor.CurbSide);
	RefreshSegmentRows(corridor.Segments);
	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitWalkwayWidthText(text);
}

void UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitWalkwayWidthRangeText(minText, maxText);
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitLaneSurfaceText(side, laneIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitLaneWidthText(side, laneIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitLaneWidthRangeText(side, laneIndex, minText, maxText);
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex)
{
	AddLaneAfter(side, laneIndex);
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex)
{
	RemoveLaneAt(side, laneIndex);
}

void UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountAddRequested()
{
	AddLaneAfter(EScenarioEditorCorridorSide::Building, INDEX_NONE);
}

void UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountRemoveRequested()
{
	RemoveLaneAt(EScenarioEditorCorridorSide::Building, GetDraftLaneProfile(EScenarioEditorCorridorSide::Building).Num() - 1);
}

void UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountAddRequested()
{
	AddLaneAfter(EScenarioEditorCorridorSide::Curb, INDEX_NONE);
}

void UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountRemoveRequested()
{
	RemoveLaneAt(EScenarioEditorCorridorSide::Curb, GetDraftLaneProfile(EScenarioEditorCorridorSide::Curb).Num() - 1);
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted(
	const int32 pointIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitAxisPointXText(pointIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted(
	const int32 pointIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitAxisPointYText(pointIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested(const int32 pointIndex)
{
	AddAxisPointAfter(pointIndex);
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested(const int32 pointIndex)
{
	RemoveAxisPointAt(pointIndex);
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountAddRequested()
{
	AddAxisPointAfter(INDEX_NONE);
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountRemoveRequested()
{
	RemoveAxisPointAt(GetDraftAxisPoints().Num() - 1);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted(
	const int32 segmentIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitSegmentIdText(segmentIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted(
	const int32 segmentIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitSegmentTypeText(segmentIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted(
	const int32 segmentIndex,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitSegmentAlongRangeText(segmentIndex, minText, maxText);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted(
	const int32 segmentIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitSegmentReplacedByText(segmentIndex, text);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested(const int32 segmentIndex)
{
	AddSegmentAfter(segmentIndex);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested(const int32 segmentIndex)
{
	RemoveSegmentAt(segmentIndex);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountAddRequested()
{
	AddSegmentAfter(INDEX_NONE);
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountRemoveRequested()
{
	RemoveSegmentAt(GetDraftSegments().Num() - 1);
}

void UScenarioEditorSidebarCorridorPanel::BindFieldRows()
{
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted);
		WalkwayWidthFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted);
		WalkwayWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted);
		WalkwayWidthFieldRow->OnRangeValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted);
	}
}

void UScenarioEditorSidebarCorridorPanel::UnbindFieldRows()
{
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted);
		WalkwayWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted);
	}
	if (BuildingSideCountFieldRow)
	{
		BuildingSideCountFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountAddRequested);
		BuildingSideCountFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountRemoveRequested);
	}
	if (CurbSideCountFieldRow)
	{
		CurbSideCountFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountAddRequested);
		CurbSideCountFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountRemoveRequested);
	}
	if (AxisPointsFieldRow)
	{
		AxisPointsFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountAddRequested);
		AxisPointsFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountRemoveRequested);
	}
	if (SegmentsCountFieldRow)
	{
		SegmentsCountFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountAddRequested);
		SegmentsCountFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : BuildingSideLaneWidgets)
	{
		if (!laneWidget)
		{
			continue;
		}

		laneWidget->OnSurfaceCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
		laneWidget->OnWidthCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
		laneWidget->OnWidthRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
		laneWidget->OnAddLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
		laneWidget->OnRemoveLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : CurbSideLaneWidgets)
	{
		if (!laneWidget)
		{
			continue;
		}

		laneWidget->OnSurfaceCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
		laneWidget->OnWidthCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
		laneWidget->OnWidthRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
		laneWidget->OnAddLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
		laneWidget->OnRemoveLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorPointWidget* pointWidget : AxisPointWidgets)
	{
		if (!pointWidget)
		{
			continue;
		}

		pointWidget->OnXCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted);
		pointWidget->OnYCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted);
		pointWidget->OnAddPointRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested);
		pointWidget->OnRemovePointRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget : SegmentWidgets)
	{
		if (!segmentWidget)
		{
			continue;
		}

		segmentWidget->OnIdCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted);
		segmentWidget->OnTypeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted);
		segmentWidget->OnAlongRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted);
		segmentWidget->OnReplacedByCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted);
		segmentWidget->OnAddSegmentRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested);
		segmentWidget->OnRemoveSegmentRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested);
	}
}

void UScenarioEditorSidebarCorridorPanel::ConfigureFieldRows()
{
	if (CorridorBlockWidget)
	{
		CorridorBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		CorridorBlockWidget->SetBlockMetadata(TEXT("corridor"), TEXT("root.corridor"), TEXT("Template"));
		CorridorBlockWidget->SetSelected(true);
		CorridorBlockWidget->SetNested(false);
	}
	if (AxisBlockWidget)
	{
		AxisBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		AxisBlockWidget->SetBlockMetadata(TEXT("axis"), TEXT("root.corridor.axis"), TEXT("Property"));
		AxisBlockWidget->SetNested(true);
		AxisBlockWidget->SetShowNormalOutline(false);
	}
	if (AxisPointsBlockWidget)
	{
		AxisPointsBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		AxisPointsBlockWidget->SetBlockMetadata(
			TEXT("points_m"),
			TEXT("root.corridor.axis.points_m[]"),
			TEXT("Property"));
		AxisPointsBlockWidget->SetNested(true);
		AxisPointsBlockWidget->SetShowNormalOutline(false);
	}
	if (WalkwayWidthBlockWidget)
	{
		WalkwayWidthBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		WalkwayWidthBlockWidget->SetBlockMetadata(
			TEXT("walkway_width_m"),
			TEXT("root.corridor.walkway_width_m"),
			TEXT("Property"));
		WalkwayWidthBlockWidget->SetNested(true);
		WalkwayWidthBlockWidget->SetShowNormalOutline(false);
	}
	if (BuildingSideBlockWidget)
	{
		BuildingSideBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		BuildingSideBlockWidget->SetBlockMetadata(
			TEXT("building_side"),
			TEXT("root.corridor.building_side[]"),
			TEXT("Property"));
		BuildingSideBlockWidget->SetNested(true);
		BuildingSideBlockWidget->SetShowNormalOutline(false);
	}
	if (CurbSideBlockWidget)
	{
		CurbSideBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		CurbSideBlockWidget->SetBlockMetadata(
			TEXT("curb_side"),
			TEXT("root.corridor.curb_side[]"),
			TEXT("Property"));
		CurbSideBlockWidget->SetNested(true);
		CurbSideBlockWidget->SetShowNormalOutline(false);
	}
	if (SegmentsBlockWidget)
	{
		SegmentsBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		SegmentsBlockWidget->SetBlockMetadata(
			TEXT("segments"),
			TEXT("root.corridor.segments[]"),
			TEXT("Property"));
		SegmentsBlockWidget->SetNested(true);
		SegmentsBlockWidget->SetShowNormalOutline(false);
	}
	if (AxisTypeFieldRow)
	{
		AxisTypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AxisTypeFieldRow->SetFieldLabel(TEXT("type"));
		AxisTypeFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::EnumText);
		AxisTypeFieldRow->SetEditable(false);
	}
	if (AxisPointsFieldRow)
	{
		AxisPointsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AxisPointsFieldRow->SetFieldLabel(TEXT("count"));
		AxisPointsFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Integer);
		AxisPointsFieldRow->SetEditable(false);
	}
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		WalkwayWidthFieldRow->SetFieldLabel(TEXT("value"));
		WalkwayWidthFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
		WalkwayWidthFieldRow->SetEditable(true);
	}

	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorPanel::ApplyTextStyles()
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		CorridorBlockWidget.Get(),
		AxisBlockWidget.Get(),
		AxisPointsBlockWidget.Get(),
		WalkwayWidthBlockWidget.Get(),
		BuildingSideBlockWidget.Get(),
		CurbSideBlockWidget.Get(),
		SegmentsBlockWidget.Get() })
	{
		if (blockWidget)
		{
			blockWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		AxisTypeFieldRow.Get(),
		AxisPointsFieldRow.Get(),
		WalkwayWidthFieldRow.Get(),
		BuildingSideCountFieldRow.Get(),
		CurbSideCountFieldRow.Get(),
		SegmentsCountFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : BuildingSideLaneWidgets)
	{
		if (laneWidget)
		{
			laneWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : CurbSideLaneWidgets)
	{
		if (laneWidget)
		{
			laneWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarCorridorPointWidget* pointWidget : AxisPointWidgets)
	{
		if (pointWidget)
		{
			pointWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget : SegmentWidgets)
	{
		if (segmentWidget)
		{
			segmentWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(DiagnosticsTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshAxisPointRows(
	const TArray<FVector2D>& pointsMeters)
{
	AxisPointWidgets.Reset();

	if (AxisPointsFieldRow)
	{
		AxisPointsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AxisPointsFieldRow->SetFieldLabel(TEXT("count"));
		AxisPointsFieldRow->SetValueText(FString::FromInt(pointsMeters.Num()));
		AxisPointsFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Integer);
		AxisPointsFieldRow->SetEditable(false);
		AxisPointsFieldRow->SetArrayControlsEnabled(true);
		AxisPointsFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountAddRequested);
		AxisPointsFieldRow->OnAddItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountAddRequested);
		AxisPointsFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountRemoveRequested);
		AxisPointsFieldRow->OnRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountRemoveRequested);
	}

	if (!AxisPointsBlockWidget)
	{
		return;
	}

	AxisPointsBlockWidget->ClearBodyChildren();
	if (AxisPointsFieldRow)
	{
		AxisPointsBlockWidget->AddBodyChild(AxisPointsFieldRow.Get());
	}

	for (int32 pointIndex = 0; pointIndex < pointsMeters.Num(); ++pointIndex)
	{
		if (UScenarioEditorSidebarCorridorPointWidget* pointWidget =
			AddAxisPointWidget(pointIndex, pointsMeters[pointIndex], AxisPointsBlockWidget.Get()))
		{
			AxisPointWidgets.Add(pointWidget);
		}
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshLaneProfileRows(
	const EScenarioEditorCorridorSide side,
	UScenarioEditorSidebarBlockWidget* sideBlockWidget,
	const TArray<FScenarioTemplateLaneRule>& lanes)
{
	if (!sideBlockWidget)
	{
		return;
	}

	TArray<TObjectPtr<UScenarioEditorSidebarCorridorLaneWidget>>& laneWidgets =
		side == EScenarioEditorCorridorSide::Building
			? BuildingSideLaneWidgets
			: CurbSideLaneWidgets;
	laneWidgets.Reset();

	sideBlockWidget->ClearBodyChildren();
	const TArray<FString> surfaceOptions = GetCorridorSurfaceIdOptions();
	UScenarioEditorSidebarFieldRow* countRow = AddReadOnlyFieldRow(
		sideBlockWidget,
		TEXT("count"),
		FString::FromInt(lanes.Num()),
		EScenarioEditorSidebarFieldInputType::Integer);
	if (countRow)
	{
		countRow->SetArrayControlsEnabled(true);
		if (side == EScenarioEditorCorridorSide::Building)
		{
			BuildingSideCountFieldRow = countRow;
			countRow->OnAddItemRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountAddRequested);
			countRow->OnAddItemRequested.AddDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountAddRequested);
			countRow->OnRemoveItemRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountRemoveRequested);
			countRow->OnRemoveItemRequested.AddDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountRemoveRequested);
		}
		else
		{
			CurbSideCountFieldRow = countRow;
			countRow->OnAddItemRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountAddRequested);
			countRow->OnAddItemRequested.AddDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountAddRequested);
			countRow->OnRemoveItemRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountRemoveRequested);
			countRow->OnRemoveItemRequested.AddDynamic(
				this,
				&UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountRemoveRequested);
		}
	}

	for (int32 laneIndex = 0; laneIndex < lanes.Num(); ++laneIndex)
	{
		if (UScenarioEditorSidebarCorridorLaneWidget* laneWidget =
			AddLaneWidget(side, laneIndex, lanes[laneIndex], surfaceOptions, sideBlockWidget))
		{
			laneWidgets.Add(laneWidget);
		}
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshSegmentRows(
	const TArray<FScenarioTemplateSegment>& segments)
{
	if (!SegmentsBlockWidget)
	{
		return;
	}

	SegmentWidgets.Reset();
	SegmentsBlockWidget->ClearBodyChildren();
	const TArray<FString> surfaceOptions = GetCorridorSurfaceIdOptions();
	UScenarioEditorSidebarFieldRow* countRow = AddReadOnlyFieldRow(
		SegmentsBlockWidget.Get(),
		TEXT("count"),
		FString::FromInt(segments.Num()),
		EScenarioEditorSidebarFieldInputType::Integer);
	if (countRow)
	{
		SegmentsCountFieldRow = countRow;
		countRow->SetArrayControlsEnabled(true);
		countRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountAddRequested);
		countRow->OnAddItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountAddRequested);
		countRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountRemoveRequested);
		countRow->OnRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountRemoveRequested);
	}

	for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
	{
		if (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget =
			AddSegmentWidget(segmentIndex, segments[segmentIndex], surfaceOptions, SegmentsBlockWidget.Get()))
		{
			SegmentWidgets.Add(segmentWidget);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarCorridorPanel::AddReadOnlyFieldRow(
	UScenarioEditorSidebarBlockWidget* parentBlockWidget,
	const FString& label,
	const FString& value,
	const EScenarioEditorSidebarFieldInputType inputType) const
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
			UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(WidgetClassCatalog));
	if (!fieldRow)
	{
		SetDiagnosticsText(TEXT("Scenario editor field row widget class is missing."));
		return nullptr;
	}

	fieldRow->SetTextStyleCatalog(TextStyleCatalog);
	fieldRow->SetFieldLabel(label);
	fieldRow->SetValueText(value);
	fieldRow->SetInputType(inputType);
	fieldRow->SetEditable(false);
	parentBlockWidget->AddBodyChild(fieldRow);
	return fieldRow;
}

UScenarioEditorSidebarCorridorPointWidget* UScenarioEditorSidebarCorridorPanel::AddAxisPointWidget(
	const int32 pointIndex,
	const FVector2D& pointMeters,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorPointWidget* pointWidget =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarCorridorPointWidget>(
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPointWidgetClass(WidgetClassCatalog));
	if (!pointWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor corridor point widget class is missing."));
		return nullptr;
	}

	pointWidget->SetTextStyleCatalog(TextStyleCatalog);
	pointWidget->SetPointIndex(pointIndex);
	pointWidget->RefreshFromPoint(pointMeters);
	pointWidget->OnXCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted);
	pointWidget->OnXCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted);
	pointWidget->OnYCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted);
	pointWidget->OnYCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted);
	pointWidget->OnAddPointRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested);
	pointWidget->OnAddPointRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested);
	pointWidget->OnRemovePointRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested);
	pointWidget->OnRemovePointRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested);
	parentBlockWidget->AddBodyChild(pointWidget);
	return pointWidget;
}

UScenarioEditorSidebarCorridorLaneWidget* UScenarioEditorSidebarCorridorPanel::AddLaneWidget(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FScenarioTemplateLaneRule& lane,
	const TArray<FString>& surfaceOptions,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorLaneWidget* laneWidget =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarCorridorLaneWidget>(
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorLaneWidgetClass(WidgetClassCatalog));
	if (!laneWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor corridor lane widget class is missing."));
		return nullptr;
	}

	laneWidget->SetTextStyleCatalog(TextStyleCatalog);
	laneWidget->SetLaneContext(side, laneIndex);
	laneWidget->SetSurfaceOptions(surfaceOptions);
	laneWidget->RefreshFromLane(lane);
	laneWidget->OnSurfaceCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
	laneWidget->OnSurfaceCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
	laneWidget->OnWidthCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
	laneWidget->OnWidthCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
	laneWidget->OnWidthRangeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
	laneWidget->OnWidthRangeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
	laneWidget->OnAddLaneRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
	laneWidget->OnAddLaneRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
	laneWidget->OnRemoveLaneRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	laneWidget->OnRemoveLaneRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	parentBlockWidget->AddBodyChild(laneWidget);
	return laneWidget;
}

UScenarioEditorSidebarCorridorSegmentWidget* UScenarioEditorSidebarCorridorPanel::AddSegmentWidget(
	const int32 segmentIndex,
	const FScenarioTemplateSegment& segment,
	const TArray<FString>& surfaceOptions,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarCorridorSegmentWidget>(
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorSegmentWidgetClass(WidgetClassCatalog));
	if (!segmentWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor corridor segment widget class is missing."));
		return nullptr;
	}

	segmentWidget->SetTextStyleCatalog(TextStyleCatalog);
	segmentWidget->SetSegmentIndex(segmentIndex);
	segmentWidget->SetSurfaceOptions(surfaceOptions);
	segmentWidget->RefreshFromSegment(segment);
	segmentWidget->OnIdCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted);
	segmentWidget->OnIdCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted);
	segmentWidget->OnTypeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted);
	segmentWidget->OnTypeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted);
	segmentWidget->OnAlongRangeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted);
	segmentWidget->OnAlongRangeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted);
	segmentWidget->OnReplacedByCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted);
	segmentWidget->OnReplacedByCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted);
	segmentWidget->OnAddSegmentRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested);
	segmentWidget->OnAddSegmentRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested);
	segmentWidget->OnRemoveSegmentRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested);
	segmentWidget->OnRemoveSegmentRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested);
	parentBlockWidget->AddBodyChild(segmentWidget);
	return segmentWidget;
}

UScenarioAuthoringSubsystem* UScenarioEditorSidebarCorridorPanel::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

TArray<FString> UScenarioEditorSidebarCorridorPanel::GetCorridorSurfaceIdOptions() const
{
	TArray<FScenarioCorridorSurfaceEntry> surfaceEntries;
	if (const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		authoringSubsystem->GetCorridorSurfaceEntries(surfaceEntries);
	}
	else
	{
		surfaceEntries = UScenarioCorridorSurfaceCatalog::MakeDefaultEntries();
	}

	TArray<FString> surfaceIds;
	TSet<FString> seenSurfaceIds;
	for (const FScenarioCorridorSurfaceEntry& surfaceEntry : surfaceEntries)
	{
		if (surfaceEntry.SurfaceId.IsNone())
		{
			continue;
		}

		const FString surfaceId = surfaceEntry.SurfaceId.ToString();
		if (surfaceId.IsEmpty() || seenSurfaceIds.Contains(surfaceId))
		{
			continue;
		}

		seenSurfaceIds.Add(surfaceId);
		surfaceIds.Add(surfaceId);
	}

	return surfaceIds;
}

TArray<FScenarioTemplateLaneRule> UScenarioEditorSidebarCorridorPanel::GetDraftLaneProfile(
	const EScenarioEditorCorridorSide side) const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return {};
	}

	const FScenarioTemplateCorridor& corridor = authoringSubsystem->GetDraftScenario().Corridor;
	return side == EScenarioEditorCorridorSide::Building
		? corridor.BuildingSide
		: corridor.CurbSide;
}

TArray<FVector2D> UScenarioEditorSidebarCorridorPanel::GetDraftAxisPoints() const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return {};
	}

	return authoringSubsystem->GetDraftScenario().Corridor.Axis.PointsMeters;
}

TArray<FScenarioTemplateSegment> UScenarioEditorSidebarCorridorPanel::GetDraftSegments() const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return {};
	}

	return authoringSubsystem->GetDraftScenario().Corridor.Segments;
}

void UScenarioEditorSidebarCorridorPanel::CommitWalkwayWidthText(const FText& text)
{
	double widthMeters = 0.0;
	if (!TryParseMeters(text, widthMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("walkway_width_m must be a number in meters."));
		return;
	}

	CommitWalkwayWidthValue(UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(widthMeters));
}

void UScenarioEditorSidebarCorridorPanel::CommitWalkwayWidthRangeText(
	const FText& minText,
	const FText& maxText)
{
	double minWidthMeters = 0.0;
	double maxWidthMeters = 0.0;
	if (!TryParseMeters(minText, minWidthMeters) || !TryParseMeters(maxText, maxWidthMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("walkway_width_m range must use numeric min/max meters."));
		return;
	}

	CommitWalkwayWidthValue(
		UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(minWidthMeters, maxWidthMeters));
}

void UScenarioEditorSidebarCorridorPanel::CommitWalkwayWidthValue(
	const FScenarioTemplateNumberValue& widthMeters)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetCorridorWalkwayWidthMeters(widthMeters, diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinCorridorPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarCorridorPanel::CommitLaneSurfaceText(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text)
{
	TArray<FScenarioTemplateLaneRule> lanes = GetDraftLaneProfile(side);
	if (!lanes.IsValidIndex(laneIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Lane index is no longer valid."));
		return;
	}

	lanes[laneIndex].SurfaceId = text.ToString().TrimStartAndEnd();
	CommitLaneProfile(side, lanes);
}

void UScenarioEditorSidebarCorridorPanel::CommitLaneWidthText(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text)
{
	double widthMeters = 0.0;
	if (!TryParseMeters(text, widthMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("lane width_m must be a number in meters."));
		return;
	}

	TArray<FScenarioTemplateLaneRule> lanes = GetDraftLaneProfile(side);
	if (!lanes.IsValidIndex(laneIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Lane index is no longer valid."));
		return;
	}

	lanes[laneIndex].WidthMeters =
		UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(widthMeters);
	CommitLaneProfile(side, lanes);
}

void UScenarioEditorSidebarCorridorPanel::CommitLaneWidthRangeText(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& minText,
	const FText& maxText)
{
	double minWidthMeters = 0.0;
	double maxWidthMeters = 0.0;
	if (!TryParseMeters(minText, minWidthMeters) || !TryParseMeters(maxText, maxWidthMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("lane width_m range must use numeric min/max meters."));
		return;
	}

	TArray<FScenarioTemplateLaneRule> lanes = GetDraftLaneProfile(side);
	if (!lanes.IsValidIndex(laneIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Lane index is no longer valid."));
		return;
	}

	lanes[laneIndex].WidthMeters =
		UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(minWidthMeters, maxWidthMeters);
	CommitLaneProfile(side, lanes);
}

void UScenarioEditorSidebarCorridorPanel::CommitLaneProfile(
	const EScenarioEditorCorridorSide side,
	const TArray<FScenarioTemplateLaneRule>& lanes)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetCorridorSideLaneProfile(side, lanes, diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinCorridorPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarCorridorPanel::AddLaneAfter(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex)
{
	TArray<FScenarioTemplateLaneRule> lanes = GetDraftLaneProfile(side);
	const int32 insertionIndex = lanes.IsValidIndex(laneIndex)
		? laneIndex + 1
		: lanes.Num();
	lanes.Insert(MakeDefaultLaneRule(side, lanes, laneIndex), insertionIndex);
	CommitLaneProfile(side, lanes);
}

void UScenarioEditorSidebarCorridorPanel::RemoveLaneAt(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex)
{
	TArray<FScenarioTemplateLaneRule> lanes = GetDraftLaneProfile(side);
	if (!lanes.IsValidIndex(laneIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Lane index is no longer valid."));
		return;
	}

	lanes.RemoveAt(laneIndex);
	CommitLaneProfile(side, lanes);
}

FScenarioTemplateLaneRule UScenarioEditorSidebarCorridorPanel::MakeDefaultLaneRule(
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
	lane.WidthMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.4);
	return lane;
}

void UScenarioEditorSidebarCorridorPanel::CommitAxisPointXText(
	const int32 pointIndex,
	const FText& text)
{
	double xMeters = 0.0;
	if (!TryParseMeters(text, xMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("axis point x must be a finite number in meters."));
		return;
	}

	TArray<FVector2D> pointsMeters = GetDraftAxisPoints();
	if (!pointsMeters.IsValidIndex(pointIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Axis point index is no longer valid."));
		return;
	}

	pointsMeters[pointIndex].X = xMeters;
	CommitAxisPoints(pointsMeters);
}

void UScenarioEditorSidebarCorridorPanel::CommitAxisPointYText(
	const int32 pointIndex,
	const FText& text)
{
	double yMeters = 0.0;
	if (!TryParseMeters(text, yMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("axis point y must be a finite number in meters."));
		return;
	}

	TArray<FVector2D> pointsMeters = GetDraftAxisPoints();
	if (!pointsMeters.IsValidIndex(pointIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Axis point index is no longer valid."));
		return;
	}

	pointsMeters[pointIndex].Y = yMeters;
	CommitAxisPoints(pointsMeters);
}

void UScenarioEditorSidebarCorridorPanel::CommitAxisPoints(
	const TArray<FVector2D>& pointsMeters)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetCorridorAxisPointsMeters(pointsMeters, diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinCorridorPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarCorridorPanel::AddAxisPointAfter(const int32 pointIndex)
{
	TArray<FVector2D> pointsMeters = GetDraftAxisPoints();
	const int32 insertionIndex = pointsMeters.IsValidIndex(pointIndex)
		? pointIndex + 1
		: pointsMeters.Num();
	pointsMeters.Insert(MakeDefaultAxisPoint(pointsMeters, pointIndex), insertionIndex);
	CommitAxisPoints(pointsMeters);
}

void UScenarioEditorSidebarCorridorPanel::RemoveAxisPointAt(const int32 pointIndex)
{
	TArray<FVector2D> pointsMeters = GetDraftAxisPoints();
	if (!pointsMeters.IsValidIndex(pointIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Axis point index is no longer valid."));
		return;
	}

	pointsMeters.RemoveAt(pointIndex);
	CommitAxisPoints(pointsMeters);
}

FVector2D UScenarioEditorSidebarCorridorPanel::MakeDefaultAxisPoint(
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

void UScenarioEditorSidebarCorridorPanel::CommitSegmentIdText(
	const int32 segmentIndex,
	const FText& text)
{
	TArray<FScenarioTemplateSegment> segments = GetDraftSegments();
	if (!segments.IsValidIndex(segmentIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Segment index is no longer valid."));
		return;
	}

	segments[segmentIndex].SegmentId = text.ToString().TrimStartAndEnd();
	CommitSegments(segments);
}

void UScenarioEditorSidebarCorridorPanel::CommitSegmentTypeText(
	const int32 segmentIndex,
	const FText& text)
{
	EScenarioTemplateSegmentType segmentType = EScenarioTemplateSegmentType::Straight;
	if (!TryParseSegmentType(text, segmentType))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("segment type must be straight, narrowing, crosswalk, or entrance."));
		return;
	}

	TArray<FScenarioTemplateSegment> segments = GetDraftSegments();
	if (!segments.IsValidIndex(segmentIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Segment index is no longer valid."));
		return;
	}

	segments[segmentIndex].Type = segmentType;
	CommitSegments(segments);
}

void UScenarioEditorSidebarCorridorPanel::CommitSegmentAlongRangeText(
	const int32 segmentIndex,
	const FText& minText,
	const FText& maxText)
{
	double startMeters = 0.0;
	double endMeters = 0.0;
	if (!TryParseMeters(minText, startMeters) || !TryParseMeters(maxText, endMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("segment along_range_m must use numeric start/end meters."));
		return;
	}

	TArray<FScenarioTemplateSegment> segments = GetDraftSegments();
	if (!segments.IsValidIndex(segmentIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Segment index is no longer valid."));
		return;
	}

	segments[segmentIndex].AlongRangeMeters.StartMeters = startMeters;
	segments[segmentIndex].AlongRangeMeters.EndMeters = endMeters;
	CommitSegments(segments);
}

void UScenarioEditorSidebarCorridorPanel::CommitSegmentReplacedByText(
	const int32 segmentIndex,
	const FText& text)
{
	TArray<FScenarioTemplateSegment> segments = GetDraftSegments();
	if (!segments.IsValidIndex(segmentIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Segment index is no longer valid."));
		return;
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
	CommitSegments(segments);
}

void UScenarioEditorSidebarCorridorPanel::CommitSegments(
	const TArray<FScenarioTemplateSegment>& segments)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetCorridorSegments(segments, diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinCorridorPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarCorridorPanel::AddSegmentAfter(const int32 segmentIndex)
{
	TArray<FScenarioTemplateSegment> segments = GetDraftSegments();
	const int32 insertionIndex = segments.IsValidIndex(segmentIndex)
		? segmentIndex + 1
		: segments.Num();
	segments.Insert(MakeDefaultSegment(segments, segmentIndex), insertionIndex);
	CommitSegments(segments);
}

void UScenarioEditorSidebarCorridorPanel::RemoveSegmentAt(const int32 segmentIndex)
{
	TArray<FScenarioTemplateSegment> segments = GetDraftSegments();
	if (!segments.IsValidIndex(segmentIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Segment index is no longer valid."));
		return;
	}

	segments.RemoveAt(segmentIndex);
	CommitSegments(segments);
}

FScenarioTemplateSegment UScenarioEditorSidebarCorridorPanel::MakeDefaultSegment(
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
		const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
		const double axisLengthMeters = authoringSubsystem
			? MeasureAxisLengthMeters(authoringSubsystem->GetDraftScenario().Corridor.Axis.PointsMeters)
			: 1.0;
		segment.Type = EScenarioTemplateSegmentType::Straight;
		segment.AlongRangeMeters.StartMeters = 0.0;
		segment.AlongRangeMeters.EndMeters = FMath::Max(1.0, axisLengthMeters);
	}

	segment.SegmentId = segmentId;
	return segment;
}

void UScenarioEditorSidebarCorridorPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}

bool UScenarioEditorSidebarCorridorPanel::TryParseMeters(const FText& text, double& outMeters)
{
	FString meterText = text.ToString().TrimStartAndEnd();
	meterText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
	meterText.TrimStartAndEndInline();
	return LexTryParseString(outMeters, *meterText);
}

bool UScenarioEditorSidebarCorridorPanel::TryParseSegmentType(
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

FString UScenarioEditorSidebarCorridorPanel::AxisTypeToString(
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

FString UScenarioEditorSidebarCorridorPanel::SegmentTypeToString(
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

FString UScenarioEditorSidebarCorridorPanel::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

double UScenarioEditorSidebarCorridorPanel::MeasureAxisLengthMeters(
	const TArray<FVector2D>& pointsMeters)
{
	double lengthMeters = 0.0;
	for (int32 index = 1; index < pointsMeters.Num(); ++index)
	{
		lengthMeters += FVector2D::Distance(pointsMeters[index - 1], pointsMeters[index]);
	}
	return lengthMeters;
}
