#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"

#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

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
	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PlacementBlockWidget->SetBlockMetadata(
			bHasCachedPlacement && !CachedPlacement.PlacementId.IsEmpty()
				? CachedPlacement.PlacementId
				: FString::Printf(TEXT("배치 규칙 %d"), PlacementIndex + 1),
			TEXT("root.obstacles.placements[]"),
			TEXT("세부"));
		PlacementBlockWidget->SetNested(true);
		PlacementBlockWidget->SetShowNormalOutline(false);
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
				? FString::Printf(TEXT("배치 규칙 %d"), PlacementIndex + 1)
				: CachedPlacement.PlacementId,
			TEXT("root.obstacles.placements[]"),
			TEXT("세부"));
	}
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
		ZoneSegmentsFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementZoneSegments")));
	}
	if (ZoneLanesFieldRow)
	{
		ZoneLanesFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementZoneLanes")));
	}
	if (PaletteCategoriesFieldRow)
	{
		PaletteCategoriesFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementPaletteCategories")));
	}
	if (PaletteClassesFieldRow)
	{
		PaletteClassesFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("PlacementPaletteClasses")));
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
