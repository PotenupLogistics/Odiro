#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"

#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"
#include "Scenario/Widget/ScenarioPlaceablePaletteItemWidget.h"

namespace SidebarWidgetHelpers = ScenarioEditorSidebarWidgetHelpers;

void UScenarioEditorSidebarObstaclePlacementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
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
	if (bHasCachedPlacement)
	{
		RefreshFieldItemsFromViewModel();
	}
	ConfigureFieldRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarObstaclePlacementWidget::RefreshFromPlacement(
	const FScenarioTemplateObstaclePlacement& placement)
{
	CachedPlacement = placement;
	bHasCachedPlacement = true;
	RefreshFieldItemsFromViewModel();
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

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsAddRequested()
{
	BroadcastStringListItemAction(
		OnStringListItemAddRequested,
		EScenarioEditorSidebarObstaclePlacementField::ZoneSegments,
		INDEX_NONE);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesAddRequested()
{
	BroadcastStringListItemAction(
		OnStringListItemAddRequested,
		EScenarioEditorSidebarObstaclePlacementField::ZoneLanes,
		INDEX_NONE);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesAddRequested()
{
	BroadcastStringListItemAction(
		OnStringListItemAddRequested,
		EScenarioEditorSidebarObstaclePlacementField::PaletteCategories,
		INDEX_NONE);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesAddRequested()
{
	BroadcastStringListItemAction(
		OnStringListItemAddRequested,
		EScenarioEditorSidebarObstaclePlacementField::PaletteClasses,
		INDEX_NONE);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentItemCommitted(
	const int32 itemIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastStringListItemText(
		EScenarioEditorSidebarObstaclePlacementField::ZoneSegments,
		itemIndex,
		text,
		commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLaneItemCommitted(
	const int32 itemIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastStringListItemText(
		EScenarioEditorSidebarObstaclePlacementField::ZoneLanes,
		itemIndex,
		text,
		commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoryItemCommitted(
	const int32 itemIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastStringListItemText(
		EScenarioEditorSidebarObstaclePlacementField::PaletteCategories,
		itemIndex,
		text,
		commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassItemCommitted(
	const int32 itemIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastStringListItemText(
		EScenarioEditorSidebarObstaclePlacementField::PaletteClasses,
		itemIndex,
		text,
		commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentItemRemoveRequested(const int32 itemIndex)
{
	BroadcastStringListItemAction(
		OnStringListItemRemoveRequested,
		EScenarioEditorSidebarObstaclePlacementField::ZoneSegments,
		itemIndex);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLaneItemRemoveRequested(const int32 itemIndex)
{
	BroadcastStringListItemAction(
		OnStringListItemRemoveRequested,
		EScenarioEditorSidebarObstaclePlacementField::ZoneLanes,
		itemIndex);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoryItemRemoveRequested(const int32 itemIndex)
{
	BroadcastStringListItemAction(
		OnStringListItemRemoveRequested,
		EScenarioEditorSidebarObstaclePlacementField::PaletteCategories,
		itemIndex);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassItemRemoveRequested(const int32 itemIndex)
{
	BroadcastStringListItemAction(
		OnStringListItemRemoveRequested,
		EScenarioEditorSidebarObstaclePlacementField::PaletteClasses,
		itemIndex);
}

void UScenarioEditorSidebarObstaclePlacementWidget::BindFieldRows()
{
	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->OnAddActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested);
		PlacementBlockWidget->OnAddActionRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested);
		PlacementBlockWidget->OnRemoveActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested);
		PlacementBlockWidget->OnRemoveActionRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested);
	}
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
		PlacementIdFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
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
		ZoneSegmentsFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsAddRequested);
		ZoneSegmentsFieldRow->OnAddItemRequested.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsAddRequested);
	}
	if (ZoneLanesFieldRow)
	{
		ZoneLanesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesCommitted);
		ZoneLanesFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesAddRequested);
		ZoneLanesFieldRow->OnAddItemRequested.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesAddRequested);
	}
	if (PaletteCategoriesFieldRow)
	{
		PaletteCategoriesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesCommitted);
		PaletteCategoriesFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesAddRequested);
		PaletteCategoriesFieldRow->OnAddItemRequested.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesAddRequested);
	}
	if (PaletteClassesFieldRow)
	{
		PaletteClassesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesCommitted);
		PaletteClassesFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesAddRequested);
		PaletteClassesFieldRow->OnAddItemRequested.AddDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesAddRequested);
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
	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->OnAddActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAddRequested);
		PlacementBlockWidget->OnRemoveActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleRemoveRequested);
	}
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
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
		ZoneSegmentsFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentsAddRequested);
	}
	if (ZoneLanesFieldRow)
	{
		ZoneLanesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesCommitted);
		ZoneLanesFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLanesAddRequested);
	}
	if (PaletteCategoriesFieldRow)
	{
		PaletteCategoriesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesCommitted);
		PaletteCategoriesFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoriesAddRequested);
	}
	if (PaletteClassesFieldRow)
	{
		PaletteClassesFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesCommitted);
		PaletteClassesFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassesAddRequested);
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
	ClearStringListRows(EScenarioEditorSidebarObstaclePlacementField::ZoneSegments);
	ClearStringListRows(EScenarioEditorSidebarObstaclePlacementField::ZoneLanes);
	ClearStringListRows(EScenarioEditorSidebarObstaclePlacementField::PaletteCategories);
	ClearStringListRows(EScenarioEditorSidebarObstaclePlacementField::PaletteClasses);
}

void UScenarioEditorSidebarObstaclePlacementWidget::ConfigureFieldRows()
{
	if (PlacementBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(PlacementBlockWidget.Get(), TextStyleCatalog, {
			bHasCachedPlacement && !CachedPlacement.PlacementId.IsEmpty()
				? CachedPlacement.PlacementId
				: FString::Printf(TEXT("배치된 장애물 %d"), PlacementIndex + 1),
			SidebarWidgetHelpers::MakeIndexedBlockPath(TEXT("root.obstacles.placements"), PlacementIndex),
			TEXT("세부"),
			false,
			true,
			false });
		PlacementBlockWidget->SetAddActionVisible(false);
		PlacementBlockWidget->SetRemoveActionVisible(true);
	}
	ApplyAssetHeaderSummary();

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
	for (TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>>* rows : {
		&ZoneSegmentItemRows,
		&ZoneLaneItemRows,
		&PaletteCategoryItemRows,
		&PaletteClassItemRows })
	{
		for (UScenarioEditorSidebarFieldRow* fieldRow : *rows)
		{
			if (fieldRow)
			{
				fieldRow->SetTextStyleCatalog(TextStyleCatalog);
			}
		}
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ApplyCachedPlacementToRows()
{
	if (!bHasCachedPlacement)
	{
		return;
	}
	if (CachedFieldItems.IsEmpty())
	{
		RefreshFieldItemsFromViewModel();
	}

	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetBlockMetadata(
			CachedPlacement.PlacementId.IsEmpty()
				? FString::Printf(TEXT("배치된 장애물 %d"), PlacementIndex + 1)
				: CachedPlacement.PlacementId,
			SidebarWidgetHelpers::MakeIndexedBlockPath(TEXT("root.obstacles.placements"), PlacementIndex),
			TEXT("세부"));
	}
	ApplyAssetHeaderSummary();
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementId")));
	}
	if (KindFieldRow)
	{
		KindFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementKind")));
	}
	if (PropFieldRow)
	{
		PropFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementProp")));
	}
	if (PatternFieldRow)
	{
		PatternFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementPattern")));
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementSegment")));
	}
	if (LaneFieldRow)
	{
		LaneFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementLane")));
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementAlong")));
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementOffset")));
	}
	if (ZoneSegmentsFieldRow)
	{
		UScenarioTemplateFieldRowViewModel* fieldItem = FindCachedFieldItem(TEXT("PlacementZoneSegments"));
		ZoneSegmentsFieldRow->InitializeFromItemViewModel(fieldItem);
		const UScenarioTemplateFieldRowViewModel* segmentItem = FindCachedFieldItem(TEXT("PlacementSegment"));
		RefreshStringListRows(
			EScenarioEditorSidebarObstaclePlacementField::ZoneSegments,
			ZoneSegmentsFieldRow.Get(),
			CachedPlacement.Zone.SegmentIds,
			segmentItem ? segmentItem->GetComboOptions() : TArray<FString>(),
			TEXT("구간"),
			fieldItem && fieldItem->IsFieldVisible());
	}
	if (ZoneLanesFieldRow)
	{
		UScenarioTemplateFieldRowViewModel* fieldItem = FindCachedFieldItem(TEXT("PlacementZoneLanes"));
		ZoneLanesFieldRow->InitializeFromItemViewModel(fieldItem);
		const UScenarioTemplateFieldRowViewModel* laneItem = FindCachedFieldItem(TEXT("PlacementLane"));
		RefreshStringListRows(
			EScenarioEditorSidebarObstaclePlacementField::ZoneLanes,
			ZoneLanesFieldRow.Get(),
			CachedPlacement.Zone.LaneIds,
			laneItem ? laneItem->GetComboOptions() : TArray<FString>(),
			TEXT("영역"),
			fieldItem && fieldItem->IsFieldVisible());
	}
	if (PaletteCategoriesFieldRow)
	{
		UScenarioTemplateFieldRowViewModel* fieldItem = FindCachedFieldItem(TEXT("PlacementPaletteCategories"));
		PaletteCategoriesFieldRow->InitializeFromItemViewModel(fieldItem);
		RefreshStringListRows(
			EScenarioEditorSidebarObstaclePlacementField::PaletteCategories,
			PaletteCategoriesFieldRow.Get(),
			CachedPlacement.Palette.CategoryIds,
			TArray<FString>(),
			TEXT("카테고리"),
			fieldItem && fieldItem->IsFieldVisible());
	}
	if (PaletteClassesFieldRow)
	{
		UScenarioTemplateFieldRowViewModel* fieldItem = FindCachedFieldItem(TEXT("PlacementPaletteClasses"));
		PaletteClassesFieldRow->InitializeFromItemViewModel(fieldItem);
		RefreshStringListRows(
			EScenarioEditorSidebarObstaclePlacementField::PaletteClasses,
			PaletteClassesFieldRow.Get(),
			CachedPlacement.Palette.ClassIds,
			TArray<FString>(),
			TEXT("클래스"),
			fieldItem && fieldItem->IsFieldVisible());
	}
	if (CountFieldRow)
	{
		CountFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementCount")));
	}
	if (SpacingFieldRow)
	{
		SpacingFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementSpacing")));
	}
	if (GapWidthFieldRow)
	{
		GapWidthFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementGapWidth")));
	}
	if (DensityFieldRow)
	{
		DensityFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementDensity")));
	}
	if (YawFieldRow)
	{
		YawFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementYaw")));
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementAllowBlocking")));
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ApplyAssetHeaderSummary()
{
	if (!PlacementBlockWidget)
	{
		return;
	}
	if (!bHasCachedPlacement)
	{
		PlacementBlockWidget->SetAssetHeaderSummary(
			FText::GetEmpty(),
			FText::GetEmpty(),
			TSoftObjectPtr<UTexture2D>(),
			false);
		return;
	}

	FText assetKindText = CachedPlacement.PropId.IsEmpty()
		? FText::FromString(TEXT("Obstacle asset"))
		: FText::FromString(CachedPlacement.PropId);
	TSoftObjectPtr<UTexture2D> thumbnailTexture;

	if (!CachedPlacement.PropId.IsEmpty())
	{
		if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
		{
			TArray<FScenarioStaticObstaclePropEntry> propEntries;
			templateSidebarViewModel->GetStaticObstaclePaletteEntries(propEntries);
			const FName propId(*CachedPlacement.PropId);
			for (const FScenarioStaticObstaclePropEntry& propEntry : propEntries)
			{
				if (propEntry.PropId == propId)
				{
					const FScenarioPaletteItemEntry paletteItem =
						UScenarioPlaceablePaletteItemWidget::MakeStaticObstaclePaletteItemEntry(propEntry);
					assetKindText = paletteItem.DisplayName;
					thumbnailTexture = paletteItem.ThumbnailTexture;
					break;
				}
			}
		}
	}

	const FString placementName = CachedPlacement.PlacementId.IsEmpty()
		? FString::Printf(TEXT("Obstacle %d"), PlacementIndex + 1)
		: CachedPlacement.PlacementId;
	PlacementBlockWidget->SetAssetHeaderSummary(
		assetKindText,
		FText::FromString(placementName),
		thumbnailTexture,
		true);
}

void UScenarioEditorSidebarObstaclePlacementWidget::RefreshFieldItemsFromViewModel()
{
	CachedFieldItems.Reset();
	if (!bHasCachedPlacement)
	{
		return;
	}

	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	for (UScenarioTemplateFieldRowViewModel* fieldItem :
		templateSidebarViewModel->CreateObstaclePlacementFieldItems(PlacementIndex, CachedPlacement))
	{
		CachedFieldItems.Add(fieldItem);
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

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarObstaclePlacementWidget::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

UScenarioTemplateFieldRowViewModel* UScenarioEditorSidebarObstaclePlacementWidget::FindCachedFieldItem(
	const FString& fieldId) const
{
	for (UScenarioTemplateFieldRowViewModel* fieldItem : CachedFieldItems)
	{
		if (fieldItem && fieldItem->GetItemId() == fieldId)
		{
			return fieldItem;
		}
	}
	return nullptr;
}

void UScenarioEditorSidebarObstaclePlacementWidget::RefreshStringListRows(
	const EScenarioEditorSidebarObstaclePlacementField field,
	UScenarioEditorSidebarFieldRow* collectionFieldRow,
	const TArray<FString>& values,
	const TArray<FString>& options,
	const FString& itemLabelPrefix,
	const bool bVisible)
{
	ClearStringListRows(field);
	if (!collectionFieldRow)
	{
		return;
	}

	collectionFieldRow->SetValueText(FString::FromInt(values.Num()));
	collectionFieldRow->SetEditable(false);
	collectionFieldRow->SetArrayControlsEnabled(false);
	collectionFieldRow->SetAddItemControlVisible(bVisible);
	collectionFieldRow->SetRemoveItemControlVisible(false);
	collectionFieldRow->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}

	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>>* rows = ResolveStringListRows(field);
	if (!rows)
	{
		return;
	}

	for (int32 itemIndex = 0; itemIndex < values.Num(); ++itemIndex)
	{
		if (UScenarioEditorSidebarFieldRow* fieldRow =
			AddStringListItemRow(field, collectionFieldRow, itemIndex, values[itemIndex], options, itemLabelPrefix))
		{
			rows->Add(fieldRow);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarObstaclePlacementWidget::AddStringListItemRow(
	const EScenarioEditorSidebarObstaclePlacementField field,
	UScenarioEditorSidebarFieldRow* collectionFieldRow,
	const int32 itemIndex,
	const FString& value,
	const TArray<FString>& options,
	const FString& itemLabelPrefix)
{
	if (!GetWorld() || !PlacementBlockWidget || !collectionFieldRow)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow =
		CreateWidget<UScenarioEditorSidebarFieldRow>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(WidgetClassCatalog));
	if (!fieldRow)
	{
		return nullptr;
	}

	fieldRow->SetTextStyleCatalog(TextStyleCatalog);
	fieldRow->SetFieldLabel(FString::Printf(TEXT("%s %d"), *itemLabelPrefix, itemIndex + 1));
	fieldRow->SetValueText(value);
	fieldRow->SetInputType(options.IsEmpty()
		? EScenarioEditorSidebarFieldInputType::Text
		: EScenarioEditorSidebarFieldInputType::ComboBox);
	fieldRow->SetComboOptions(options);
	fieldRow->SetComboAllowsUnset(false, FString());
	fieldRow->SetEditable(true);
	fieldRow->SetArrayControlsEnabled(false);
	fieldRow->SetAddItemControlVisible(false);
	fieldRow->SetRemoveItemControlVisible(true);
	fieldRow->SetActionContextIndex(itemIndex);

	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::ZoneSegments:
		fieldRow->OnIndexedValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentItemCommitted);
		fieldRow->OnIndexedRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneSegmentItemRemoveRequested);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::ZoneLanes:
		fieldRow->OnIndexedValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLaneItemCommitted);
		fieldRow->OnIndexedRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleZoneLaneItemRemoveRequested);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteCategories:
		fieldRow->OnIndexedValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoryItemCommitted);
		fieldRow->OnIndexedRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteCategoryItemRemoveRequested);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteClasses:
		fieldRow->OnIndexedValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassItemCommitted);
		fieldRow->OnIndexedRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePaletteClassItemRemoveRequested);
		break;
	default:
		break;
	}

	if (UVerticalBox* bodyBox = PlacementBlockWidget->GetBodyBox())
	{
		int32 anchorIndex = INDEX_NONE;
		for (int32 childIndex = 0; childIndex < bodyBox->GetChildrenCount(); ++childIndex)
		{
			if (bodyBox->GetChildAt(childIndex) == collectionFieldRow)
			{
				anchorIndex = childIndex;
				break;
			}
		}

		UPanelSlot* insertedSlot = anchorIndex == INDEX_NONE
			? bodyBox->AddChild(fieldRow)
			: bodyBox->InsertChildAt(anchorIndex + 1 + itemIndex, fieldRow);
		if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(insertedSlot))
		{
			verticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
			verticalSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	return fieldRow;
}

void UScenarioEditorSidebarObstaclePlacementWidget::ClearStringListRows(
	const EScenarioEditorSidebarObstaclePlacementField field)
{
	TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>>* rows = ResolveStringListRows(field);
	if (!rows)
	{
		return;
	}

	for (UScenarioEditorSidebarFieldRow* fieldRow : *rows)
	{
		if (fieldRow)
		{
			fieldRow->OnIndexedValueTextCommitted.RemoveAll(this);
			fieldRow->OnIndexedRemoveItemRequested.RemoveAll(this);
			fieldRow->RemoveFromParent();
		}
	}
	rows->Reset();
}

TArray<TObjectPtr<UScenarioEditorSidebarFieldRow>>*
UScenarioEditorSidebarObstaclePlacementWidget::ResolveStringListRows(
	const EScenarioEditorSidebarObstaclePlacementField field)
{
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::ZoneSegments:
		return &ZoneSegmentItemRows;
	case EScenarioEditorSidebarObstaclePlacementField::ZoneLanes:
		return &ZoneLaneItemRows;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteCategories:
		return &PaletteCategoryItemRows;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteClasses:
		return &PaletteClassItemRows;
	default:
		return nullptr;
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::BroadcastStringListItemText(
	const EScenarioEditorSidebarObstaclePlacementField field,
	const int32 itemIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnStringListItemTextCommitted.Broadcast(PlacementIndex, field, itemIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::BroadcastStringListItemAction(
	FScenarioEditorSidebarObstaclePlacementStringListItemActionRequested& action,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const int32 itemIndex)
{
	action.Broadcast(PlacementIndex, field, itemIndex);
}
