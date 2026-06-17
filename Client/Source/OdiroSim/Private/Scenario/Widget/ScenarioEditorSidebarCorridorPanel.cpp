#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorLaneWidget.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float CorridorPanelBlockPadding = 10.0f;

	FString JoinCorridorPanelDiagnostics(const TArray<FString>& diagnostics)
	{
		return diagnostics.IsEmpty()
			? FString(TEXT("Unknown Corridor edit failure."))
			: FString::Join(diagnostics, TEXT("\n"));
	}

	void AddCorridorPanelWidgetToBox(
		UVerticalBox* box,
		UWidget* widget,
		const FMargin& padding = FMargin())
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(padding);
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	UVerticalBox* AddCorridorPanelBlockWidget(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		TObjectPtr<UScenarioEditorSidebarBlockWidget>& blockWidget,
		const TCHAR* widgetName,
		const FString& name,
		const FString& path,
		const FString& badge,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog,
		const bool bHighlighted = false,
		const bool bNested = false,
		const bool bExpanded = true,
		const bool bShowNormalOutline = true)
	{
		if (!widgetTree || !parent)
		{
			return nullptr;
		}

		blockWidget = widgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
			UScenarioEditorSidebarBlockWidget::StaticClass(),
			FName(widgetName));
		if (!blockWidget)
		{
			return nullptr;
		}

		blockWidget->SetTextStyleCatalog(catalog);
		blockWidget->SetBlockMetadata(name, path, badge);
		blockWidget->SetSelected(bHighlighted);
		blockWidget->SetNested(bNested);
		blockWidget->SetExpanded(bExpanded);
		blockWidget->SetShowNormalOutline(bShowNormalOutline);
		AddCorridorPanelWidgetToBox(
			parent,
			blockWidget.Get(),
			FMargin(0.0f, 0.0f, 0.0f, CorridorPanelBlockPadding));
		return blockWidget->GetBodyBox();
	}

	void AddCorridorPanelFieldRow(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		TObjectPtr<UScenarioEditorSidebarFieldRow>& fieldRow,
		const TCHAR* widgetName)
	{
		if (!widgetTree || !parent)
		{
			return;
		}

		fieldRow = widgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
			UScenarioEditorSidebarFieldRow::StaticClass(),
			FName(widgetName));
		if (!fieldRow)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = parent->AddChildToVerticalBox(fieldRow.Get()))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarCorridorPanel::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarCorridorPanel::NativeConstruct()
{
	Super::NativeConstruct();
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

void UScenarioEditorSidebarCorridorPanel::RefreshFromDraft()
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarCorridorPanel::RefreshFromTemplate(
	const FScenarioTemplateDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;
	if (AxisTypeFieldRow)
	{
		AxisTypeFieldRow->SetValueText(AxisTypeToString(corridor.Axis.Type));
	}
	if (AxisPointsFieldRow)
	{
		AxisPointsFieldRow->SetValueText(FormatAxisPointsSummary(corridor.Axis.PointsMeters));
	}
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

void UScenarioEditorSidebarCorridorPanel::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedCorridorPanelRoot"));
	if (!rootBox)
	{
		return;
	}

	WidgetTree->RootWidget = rootBox;

	UVerticalBox* corridorBody = AddCorridorPanelBlockWidget(
		WidgetTree,
		rootBox,
		CorridorBlockWidget,
		TEXT("CorridorBlockWidget"),
		TEXT("corridor"),
		TEXT("root.corridor"),
		TEXT("Template"),
		TextStyleCatalog,
		true);
	UVerticalBox* axisBody = AddCorridorPanelBlockWidget(
		WidgetTree,
		corridorBody,
		AxisBlockWidget,
		TEXT("AxisBlockWidget"),
		TEXT("axis"),
		TEXT("root.corridor.axis"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddCorridorPanelFieldRow(WidgetTree, axisBody, AxisTypeFieldRow, TEXT("AxisTypeFieldRow"));
	AddCorridorPanelFieldRow(WidgetTree, axisBody, AxisPointsFieldRow, TEXT("AxisPointsFieldRow"));

	UVerticalBox* walkwayBody = AddCorridorPanelBlockWidget(
		WidgetTree,
		corridorBody,
		WalkwayWidthBlockWidget,
		TEXT("WalkwayWidthBlockWidget"),
		TEXT("walkway_width_m"),
		TEXT("root.corridor.walkway_width_m"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddCorridorPanelFieldRow(WidgetTree, walkwayBody, WalkwayWidthFieldRow, TEXT("WalkwayWidthFieldRow"));

	AddCorridorPanelBlockWidget(
		WidgetTree,
		corridorBody,
		BuildingSideBlockWidget,
		TEXT("BuildingSideBlockWidget"),
		TEXT("building_side"),
		TEXT("root.corridor.building_side[]"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddCorridorPanelBlockWidget(
		WidgetTree,
		corridorBody,
		CurbSideBlockWidget,
		TEXT("CurbSideBlockWidget"),
		TEXT("curb_side"),
		TEXT("root.corridor.curb_side[]"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddCorridorPanelBlockWidget(
		WidgetTree,
		corridorBody,
		SegmentsBlockWidget,
		TEXT("SegmentsBlockWidget"),
		TEXT("segments"),
		TEXT("root.corridor.segments[]"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);

	DiagnosticsTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DiagnosticsTextBlock"));
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetAutoWrapText(true);
		AddCorridorPanelWidgetToBox(rootBox, DiagnosticsTextBlock.Get());
	}
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
		AxisPointsFieldRow->SetFieldLabel(TEXT("points_m"));
		AxisPointsFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
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
		CurbSideCountFieldRow.Get() })
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

	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		DiagnosticsTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	if (WidgetTree)
	{
		UWidgetTextStyleCatalog::ApplyWidgetTreeTextStyles(WidgetTree, TextStyleCatalog);
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
			AddLaneWidget(side, laneIndex, lanes[laneIndex], sideBlockWidget))
		{
			laneWidgets.Add(laneWidget);
		}
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshSegmentRows(
	const TArray<FScenarioTemplateSegment>& segments) const
{
	if (!SegmentsBlockWidget)
	{
		return;
	}

	SegmentsBlockWidget->ClearBodyChildren();
	AddReadOnlyFieldRow(
		SegmentsBlockWidget.Get(),
		TEXT("count"),
		FString::FromInt(segments.Num()),
		EScenarioEditorSidebarFieldInputType::Integer);
	for (const FScenarioTemplateSegment& segment : segments)
	{
		UScenarioEditorSidebarBlockWidget* segmentBlock = AddSegmentBlock(SegmentsBlockWidget.Get(), segment);
		AddReadOnlyFieldRow(
			segmentBlock,
			TEXT("id"),
			segment.SegmentId.IsEmpty() ? FString(TEXT("(unset)")) : segment.SegmentId,
			EScenarioEditorSidebarFieldInputType::Text);
		AddReadOnlyFieldRow(
			segmentBlock,
			TEXT("type"),
			SegmentTypeToString(segment.Type),
			EScenarioEditorSidebarFieldInputType::EnumText);
		AddReadOnlyFieldRow(
			segmentBlock,
			TEXT("along_range_m"),
			FString::Printf(
				TEXT("%.2f..%.2fm"),
				segment.AlongRangeMeters.StartMeters,
				segment.AlongRangeMeters.EndMeters),
			EScenarioEditorSidebarFieldInputType::Range);
		AddReadOnlyFieldRow(
			segmentBlock,
			TEXT("replaced_by"),
			FormatStringValue(segment.ReplacedBySurfaceId),
			EScenarioEditorSidebarFieldInputType::Text);
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
			UScenarioEditorSidebarFieldRow::StaticClass());
	if (!fieldRow)
	{
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

UScenarioEditorSidebarBlockWidget* UScenarioEditorSidebarCorridorPanel::AddSegmentBlock(
	UScenarioEditorSidebarBlockWidget* parentBlockWidget,
	const FScenarioTemplateSegment& segment) const
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarBlockWidget* segmentBlock =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
			UScenarioEditorSidebarBlockWidget::StaticClass());
	if (!segmentBlock)
	{
		return nullptr;
	}

	segmentBlock->SetTextStyleCatalog(TextStyleCatalog);
	segmentBlock->SetBlockMetadata(
		segment.SegmentId.IsEmpty() ? FString(TEXT("(unnamed)")) : segment.SegmentId,
		TEXT("root.corridor.segments[]"),
		TEXT("Detail"));
	segmentBlock->SetNested(true);
	segmentBlock->SetShowNormalOutline(false);
	parentBlockWidget->AddBodyChild(segmentBlock);
	return segmentBlock;
}

UScenarioEditorSidebarCorridorLaneWidget* UScenarioEditorSidebarCorridorPanel::AddLaneWidget(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FScenarioTemplateLaneRule& lane,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorLaneWidget* laneWidget =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarCorridorLaneWidget>(
			UScenarioEditorSidebarCorridorLaneWidget::StaticClass());
	if (!laneWidget)
	{
		return nullptr;
	}

	laneWidget->SetTextStyleCatalog(TextStyleCatalog);
	laneWidget->SetLaneContext(side, laneIndex);
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

UScenarioAuthoringSubsystem* UScenarioEditorSidebarCorridorPanel::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

TArray<FScenarioTemplateLaneRule> UScenarioEditorSidebarCorridorPanel::GetDraftLaneProfile(
	const EScenarioEditorCorridorSide side) const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return {};
	}

	const FScenarioTemplateCorridor& corridor = authoringSubsystem->GetDraftScenarioTemplate().Corridor;
	return side == EScenarioEditorCorridorSide::Building
		? corridor.BuildingSide
		: corridor.CurbSide;
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

FString UScenarioEditorSidebarCorridorPanel::FormatNumberValue(
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

FString UScenarioEditorSidebarCorridorPanel::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

FString UScenarioEditorSidebarCorridorPanel::FormatStringValue(
	const FScenarioTemplateStringValue& value)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateStringValueMode::Choices)
	{
		return FString::Printf(TEXT("[%s]"), *FormatStringList(value.Choices));
	}

	return value.FixedValue.IsEmpty() ? FString(TEXT("(empty)")) : value.FixedValue;
}

FString UScenarioEditorSidebarCorridorPanel::FormatStringList(
	const TArray<FString>& values)
{
	return values.IsEmpty() ? FString(TEXT("(none)")) : FString::Join(values, TEXT(", "));
}

FString UScenarioEditorSidebarCorridorPanel::FormatLaneRule(
	const FScenarioTemplateLaneRule& lane)
{
	return FString::Printf(
		TEXT("%s | width: %s"),
		lane.SurfaceId.IsEmpty() ? TEXT("(unset)") : *lane.SurfaceId,
		*FormatNumberValue(lane.WidthMeters, TEXT("m")));
}

FString UScenarioEditorSidebarCorridorPanel::FormatAxisPointsSummary(
	const TArray<FVector2D>& pointsMeters)
{
	return FString::Printf(
		TEXT("%d point(s), %.2fm"),
		pointsMeters.Num(),
		MeasureAxisLengthMeters(pointsMeters));
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
