#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

namespace
{
	constexpr float ObstaclePlacementWidgetPadding = 6.0f;

	void AddObstaclePlacementWidgetToBox(UVerticalBox* box, UWidget* widget)
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ObstaclePlacementWidgetPadding));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	void AddObstaclePlacementFieldRow(
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
		AddObstaclePlacementWidgetToBox(parent, fieldRow.Get());
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarObstaclePlacementWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarObstaclePlacementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	ApplyCachedPlacementToRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetPlacementIndex(const int32 inPlacementIndex)
{
	PlacementIndex = inPlacementIndex;
	ConfigureFieldRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarObstaclePlacementWidget::RefreshFromPlacement(
	const FScenarioTemplateObstaclePlacement& placement)
{
	CachedPlacement = placement;
	bHasCachedPlacement = true;
	ConfigureFieldRows();
	ApplyCachedPlacementToRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::PlacementId, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleKindCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Kind, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Prop, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePatternCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Pattern, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Segment, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleLaneCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Lane, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Along, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::Along, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Offset, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::Offset, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::ZoneSegments, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::ZoneLanes, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::PaletteCategories, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::PaletteClasses, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleCountCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Count, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleCountRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::Count, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Spacing, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::Spacing, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::GapWidth, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::GapWidth, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Density, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::Density, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleYawCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::Yaw, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleYawRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarObstaclePlacementField::Yaw, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarObstaclePlacementField::AllowBlocking, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested()
{
	OnAddRequested.Broadcast(PlacementIndex);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested()
{
	OnRemoveRequested.Broadcast(PlacementIndex);
}

void UScenarioEditorSidebarObstaclePlacementWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	PlacementBlockWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
		UScenarioEditorSidebarBlockWidget::StaticClass(),
		TEXT("PlacementBlockWidget"));
	if (!PlacementBlockWidget)
	{
		return;
	}

	WidgetTree->RootWidget = PlacementBlockWidget;
	PlacementBlockWidget->SetNested(true);
	PlacementBlockWidget->SetShowNormalOutline(false);

	UVerticalBox* placementBody = PlacementBlockWidget->GetBodyBox();
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PlacementIdFieldRow, TEXT("PlacementIdFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, KindFieldRow, TEXT("KindFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PropFieldRow, TEXT("PropFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PatternFieldRow, TEXT("PatternFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, SegmentFieldRow, TEXT("SegmentFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, LaneFieldRow, TEXT("LaneFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, AlongFieldRow, TEXT("AlongFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, OffsetFieldRow, TEXT("OffsetFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, ZoneSegmentsFieldRow, TEXT("ZoneSegmentsFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, ZoneLanesFieldRow, TEXT("ZoneLanesFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PaletteCategoriesFieldRow, TEXT("PaletteCategoriesFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PaletteClassesFieldRow, TEXT("PaletteClassesFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, CountFieldRow, TEXT("CountFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, SpacingFieldRow, TEXT("SpacingFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, GapWidthFieldRow, TEXT("GapWidthFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, DensityFieldRow, TEXT("DensityFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, YawFieldRow, TEXT("YawFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, AllowBlockingFieldRow, TEXT("AllowBlockingFieldRow"));
}

void UScenarioEditorSidebarObstaclePlacementWidget::BindFieldRows()
{
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
		PlacementIdFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
		PlacementIdFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested);
		PlacementIdFieldRow->OnAddItemRequested.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested);
		PlacementIdFieldRow->OnRemoveItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested);
		PlacementIdFieldRow->OnRemoveItemRequested.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested);
	}
	if (KindFieldRow)
	{
		KindFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleKindCommitted);
		KindFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleKindCommitted);
	}
	if (PropFieldRow)
	{
		PropFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted);
		PropFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted);
	}
	if (PatternFieldRow)
	{
		PatternFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePatternCommitted);
		PatternFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePatternCommitted);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted);
		SegmentFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted);
	}
	if (LaneFieldRow)
	{
		LaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleLaneCommitted);
		LaneFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleLaneCommitted);
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted);
		AlongFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted);
		AlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongRangeCommitted);
		AlongFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongRangeCommitted);
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted);
		OffsetFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted);
		OffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetRangeCommitted);
		OffsetFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetRangeCommitted);
	}
	if (ZoneSegmentsFieldRow)
	{
		ZoneSegmentsFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsCommitted);
		ZoneSegmentsFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsCommitted);
	}
	if (ZoneLanesFieldRow)
	{
		ZoneLanesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesCommitted);
		ZoneLanesFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesCommitted);
	}
	if (PaletteCategoriesFieldRow)
	{
		PaletteCategoriesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesCommitted);
		PaletteCategoriesFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesCommitted);
	}
	if (PaletteClassesFieldRow)
	{
		PaletteClassesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesCommitted);
		PaletteClassesFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesCommitted);
	}
	if (CountFieldRow)
	{
		CountFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleCountCommitted);
		CountFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleCountCommitted);
		CountFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleCountRangeCommitted);
		CountFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleCountRangeCommitted);
	}
	if (SpacingFieldRow)
	{
		SpacingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingCommitted);
		SpacingFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingCommitted);
		SpacingFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingRangeCommitted);
		SpacingFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingRangeCommitted);
	}
	if (GapWidthFieldRow)
	{
		GapWidthFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthCommitted);
		GapWidthFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthCommitted);
		GapWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthRangeCommitted);
		GapWidthFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthRangeCommitted);
	}
	if (DensityFieldRow)
	{
		DensityFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityCommitted);
		DensityFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityCommitted);
		DensityFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityRangeCommitted);
		DensityFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityRangeCommitted);
	}
	if (YawFieldRow)
	{
		YawFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleYawCommitted);
		YawFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleYawCommitted);
		YawFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleYawRangeCommitted);
		YawFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleYawRangeCommitted);
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted);
		AllowBlockingFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted);
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::UnbindFieldRows()
{
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
		PlacementIdFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested);
		PlacementIdFieldRow->OnRemoveItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested);
	}
	if (KindFieldRow)
	{
		KindFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleKindCommitted);
	}
	if (PropFieldRow)
	{
		PropFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted);
	}
	if (PatternFieldRow)
	{
		PatternFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePatternCommitted);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted);
	}
	if (LaneFieldRow)
	{
		LaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleLaneCommitted);
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted);
		AlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongRangeCommitted);
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted);
		OffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetRangeCommitted);
	}
	if (ZoneSegmentsFieldRow)
	{
		ZoneSegmentsFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsCommitted);
	}
	if (ZoneLanesFieldRow)
	{
		ZoneLanesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesCommitted);
	}
	if (PaletteCategoriesFieldRow)
	{
		PaletteCategoriesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesCommitted);
	}
	if (PaletteClassesFieldRow)
	{
		PaletteClassesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesCommitted);
	}
	if (CountFieldRow)
	{
		CountFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleCountCommitted);
		CountFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleCountRangeCommitted);
	}
	if (SpacingFieldRow)
	{
		SpacingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingCommitted);
		SpacingFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleSpacingRangeCommitted);
	}
	if (GapWidthFieldRow)
	{
		GapWidthFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthCommitted);
		GapWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleGapWidthRangeCommitted);
	}
	if (DensityFieldRow)
	{
		DensityFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityCommitted);
		DensityFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleDensityRangeCommitted);
	}
	if (YawFieldRow)
	{
		YawFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleYawCommitted);
		YawFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleYawRangeCommitted);
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted);
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ConfigureFieldRows()
{
	const EScenarioTemplateObstaclePlacementKind kind = bHasCachedPlacement
		? CachedPlacement.Kind
		: EScenarioTemplateObstaclePlacementKind::Fixed;
	const bool bFixedPlacement = kind == EScenarioTemplateObstaclePlacementKind::Fixed;
	const bool bPatternPlacement = kind == EScenarioTemplateObstaclePlacementKind::Pattern;
	const bool bScatterPlacement = kind == EScenarioTemplateObstaclePlacementKind::Scatter;

	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PlacementBlockWidget->SetBlockMetadata(
			bHasCachedPlacement && !CachedPlacement.PlacementId.IsEmpty()
				? CachedPlacement.PlacementId
				: FString::Printf(TEXT("placement[%d]"), PlacementIndex),
			TEXT("root.obstacles.placements[]"),
			TEXT("Detail"));
		PlacementBlockWidget->SetNested(true);
		PlacementBlockWidget->SetShowNormalOutline(false);
	}

	auto configureRow = [this](
		UScenarioEditorSidebarFieldRow* fieldRow,
		const FString& label,
		const EScenarioEditorSidebarFieldInputType inputType,
		const bool bVisible,
		const bool bArrayControlsEnabled = false)
	{
		if (!fieldRow)
		{
			return;
		}

		fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		fieldRow->SetFieldLabel(label);
		fieldRow->SetInputType(inputType);
		fieldRow->SetEditable(bVisible);
		fieldRow->SetArrayControlsEnabled(bArrayControlsEnabled);
		fieldRow->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	configureRow(PlacementIdFieldRow.Get(), TEXT("id"), EScenarioEditorSidebarFieldInputType::Text, true, true);
	configureRow(KindFieldRow.Get(), TEXT("kind"), EScenarioEditorSidebarFieldInputType::EnumText, true);
	configureRow(PropFieldRow.Get(), TEXT("prop"), EScenarioEditorSidebarFieldInputType::Text, bFixedPlacement || bPatternPlacement);
	configureRow(PatternFieldRow.Get(), TEXT("pattern"), EScenarioEditorSidebarFieldInputType::Text, bPatternPlacement);
	configureRow(SegmentFieldRow.Get(), TEXT("at.segment"), EScenarioEditorSidebarFieldInputType::Text, bFixedPlacement || bPatternPlacement);
	configureRow(LaneFieldRow.Get(), TEXT("at.lane"), EScenarioEditorSidebarFieldInputType::Text, bFixedPlacement || bPatternPlacement);
	configureRow(
		AlongFieldRow.Get(),
		TEXT("at.along_m"),
		EScenarioEditorSidebarFieldInputType::Range,
		bFixedPlacement || bPatternPlacement);
	configureRow(
		OffsetFieldRow.Get(),
		TEXT("at.offset_m"),
		EScenarioEditorSidebarFieldInputType::Range,
		bFixedPlacement || bPatternPlacement);
	configureRow(ZoneSegmentsFieldRow.Get(), TEXT("zone.segments"), EScenarioEditorSidebarFieldInputType::Text, bScatterPlacement);
	configureRow(ZoneLanesFieldRow.Get(), TEXT("zone.lanes"), EScenarioEditorSidebarFieldInputType::Text, bScatterPlacement);
	configureRow(PaletteCategoriesFieldRow.Get(), TEXT("palette.categories"), EScenarioEditorSidebarFieldInputType::Text, bScatterPlacement);
	configureRow(PaletteClassesFieldRow.Get(), TEXT("palette.classes"), EScenarioEditorSidebarFieldInputType::Text, bScatterPlacement);
	configureRow(CountFieldRow.Get(), TEXT("count"), EScenarioEditorSidebarFieldInputType::Range, bPatternPlacement || bScatterPlacement);
	configureRow(SpacingFieldRow.Get(), TEXT("spacing_m"), EScenarioEditorSidebarFieldInputType::Range, bPatternPlacement);
	configureRow(GapWidthFieldRow.Get(), TEXT("gap_width_m"), EScenarioEditorSidebarFieldInputType::Range, bPatternPlacement);
	configureRow(DensityFieldRow.Get(), TEXT("density_per_10m"), EScenarioEditorSidebarFieldInputType::Range, bScatterPlacement);
	configureRow(YawFieldRow.Get(), TEXT("yaw_deg"), EScenarioEditorSidebarFieldInputType::Range, true);
	configureRow(AllowBlockingFieldRow.Get(), TEXT("allow_blocking"), EScenarioEditorSidebarFieldInputType::EnumText, true);
}

void UScenarioEditorSidebarObstaclePlacementWidget::ApplyCachedPlacementToRows()
{
	if (!bHasCachedPlacement)
	{
		return;
	}

	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetBlockMetadata(
			CachedPlacement.PlacementId.IsEmpty()
				? FString::Printf(TEXT("placement[%d]"), PlacementIndex)
				: CachedPlacement.PlacementId,
			TEXT("root.obstacles.placements[]"),
			TEXT("Detail"));
	}
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->SetValueText(CachedPlacement.PlacementId);
	}
	if (KindFieldRow)
	{
		KindFieldRow->SetValueText(PlacementKindToString(CachedPlacement.Kind));
	}
	if (PropFieldRow)
	{
		PropFieldRow->SetValueText(CachedPlacement.PropId);
	}
	if (PatternFieldRow)
	{
		PatternFieldRow->SetValueText(CachedPlacement.PatternId);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->SetValueText(CachedPlacement.At.SegmentId);
	}
	if (LaneFieldRow)
	{
		LaneFieldRow->SetValueText(CachedPlacement.At.LaneId);
	}
	SetNumberRowValue(AlongFieldRow.Get(), CachedPlacement.At.AlongMeters);
	SetNumberRowValue(OffsetFieldRow.Get(), CachedPlacement.At.OffsetMeters);
	if (ZoneSegmentsFieldRow)
	{
		ZoneSegmentsFieldRow->SetValueText(JoinStringList(CachedPlacement.Zone.SegmentIds));
	}
	if (ZoneLanesFieldRow)
	{
		ZoneLanesFieldRow->SetValueText(JoinStringList(CachedPlacement.Zone.LaneIds));
	}
	if (PaletteCategoriesFieldRow)
	{
		PaletteCategoriesFieldRow->SetValueText(JoinStringList(CachedPlacement.Palette.CategoryIds));
	}
	if (PaletteClassesFieldRow)
	{
		PaletteClassesFieldRow->SetValueText(JoinStringList(CachedPlacement.Palette.ClassIds));
	}
	SetIntegerRowValue(CountFieldRow.Get(), CachedPlacement.Count);
	SetNumberRowValue(SpacingFieldRow.Get(), CachedPlacement.SpacingMeters);
	SetNumberRowValue(GapWidthFieldRow.Get(), CachedPlacement.GapWidthMeters);
	SetNumberRowValue(DensityFieldRow.Get(), CachedPlacement.DensityPer10Meters);
	SetNumberRowValue(YawFieldRow.Get(), CachedPlacement.YawDegrees);
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->SetValueText(CachedPlacement.bAllowBlocking ? TEXT("true") : TEXT("false"));
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ApplyTextStyles()
{
	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		PlacementIdFieldRow.Get(),
		KindFieldRow.Get(),
		PropFieldRow.Get(),
		PatternFieldRow.Get(),
		SegmentFieldRow.Get(),
		LaneFieldRow.Get(),
		AlongFieldRow.Get(),
		OffsetFieldRow.Get(),
		ZoneSegmentsFieldRow.Get(),
		ZoneLanesFieldRow.Get(),
		PaletteCategoriesFieldRow.Get(),
		PaletteClassesFieldRow.Get(),
		CountFieldRow.Get(),
		SpacingFieldRow.Get(),
		GapWidthFieldRow.Get(),
		DensityFieldRow.Get(),
		YawFieldRow.Get(),
		AllowBlockingFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::BroadcastText(
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnFieldTextCommitted.Broadcast(PlacementIndex, field, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::BroadcastRange(
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	OnFieldRangeCommitted.Broadcast(PlacementIndex, field, minText, maxText, commitMethod);
}

FString UScenarioEditorSidebarObstaclePlacementWidget::PlacementKindToString(
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

FString UScenarioEditorSidebarObstaclePlacementWidget::JoinStringList(const TArray<FString>& values)
{
	return FString::Join(values, TEXT(", "));
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetNumberRowValue(
	UScenarioEditorSidebarFieldRow* fieldRow,
	const FScenarioTemplateNumberValue& value)
{
	if (!fieldRow)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldRow->SetValueText(FString());
		fieldRow->SetRangeValueText(FString(), FString());
		fieldRow->SetRangeInputEnabled(false);
		return;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		fieldRow->SetValueText(FormatEditableNumber((value.MinValue + value.MaxValue) * 0.5));
		fieldRow->SetRangeValueText(FormatEditableNumber(value.MinValue), FormatEditableNumber(value.MaxValue));
		fieldRow->SetRangeInputEnabled(true);
		return;
	}
	fieldRow->SetValueText(FormatEditableNumber(value.FixedValue));
	fieldRow->SetRangeValueText(FormatEditableNumber(value.FixedValue), FormatEditableNumber(value.FixedValue));
	fieldRow->SetRangeInputEnabled(false);
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetIntegerRowValue(
	UScenarioEditorSidebarFieldRow* fieldRow,
	const FScenarioTemplateIntegerValue& value)
{
	if (!fieldRow)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldRow->SetValueText(FString());
		fieldRow->SetRangeValueText(FString(), FString());
		fieldRow->SetRangeInputEnabled(false);
		return;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		fieldRow->SetValueText(FormatEditableInteger(FMath::RoundToInt((value.MinValue + value.MaxValue) * 0.5f)));
		fieldRow->SetRangeValueText(FormatEditableInteger(value.MinValue), FormatEditableInteger(value.MaxValue));
		fieldRow->SetRangeInputEnabled(true);
		return;
	}
	fieldRow->SetValueText(FormatEditableInteger(value.FixedValue));
	fieldRow->SetRangeValueText(FormatEditableInteger(value.FixedValue), FormatEditableInteger(value.FixedValue));
	fieldRow->SetRangeInputEnabled(false);
}

FString UScenarioEditorSidebarObstaclePlacementWidget::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

FString UScenarioEditorSidebarObstaclePlacementWidget::FormatEditableInteger(const int32 value)
{
	return FString::FromInt(value);
}
