#include "UI/BaseSwitcherWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseToggleButtonWidget.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

namespace
{
	const TCHAR* IconSegmentClassPath = TEXT("/Game/Widgets/Common/WBP_BaseButton.WBP_BaseButton_C");
}

void UBaseSwitcherWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	const TArray<FBaseSwitcherItem> renderedItems = BuildRenderedItems();
	if (SegmentContainer
		&& (SegmentContainer->GetChildrenCount() != renderedItems.Num()
			|| SegmentIdsByChildIndex.Num() != renderedItems.Num()))
	{
		RebuildSegments(renderedItems);
		return;
	}
	RefreshSegments(renderedItems);
}

void UBaseSwitcherWidget::SetItems(const TArray<FBaseSwitcherItem>& inItems)
{
	Items = inItems;
	if (const FBaseSwitcherItem* selectedItem = FindSwitcherItemById(Items, SelectedId))
	{
		if (selectedItem->bDisabled)
		{
			SelectedId = NAME_None;
		}
	}
	else
	{
		SelectedId = NAME_None;
	}
	RebuildSegments(BuildRenderedItems());
	SynchronizeBaseProperties();
}

bool UBaseSwitcherWidget::SelectItemById(const FName itemId)
{
	if (bDisabled)
	{
		return false;
	}

	const FBaseSwitcherItem* item = FindSwitcherItemById(Items, itemId);
	if (!item || item->bDisabled)
	{
		return false;
	}

	const bool bChanged = SelectedId != itemId;
	SelectedId = itemId;
	SynchronizeBaseProperties();
	if (bChanged)
	{
		OnSelectionChanged.Broadcast(this, SelectedId);
	}
	return true;
}

void UBaseSwitcherWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseSwitcherWidget::NativeDestruct()
{
	UnbindGeneratedSegments();
	SegmentIdsByChildIndex.Reset();
	Super::NativeDestruct();
}

TArray<FBaseSwitcherItem> UBaseSwitcherWidget::BuildRenderedItems() const
{
	if (!Items.IsEmpty() || !IsDesignTime())
	{
		return Items;
	}

	FBaseSwitcherItem leftItem;
	leftItem.Id = TEXT("left");
	leftItem.Label = NSLOCTEXT("BaseSwitcherWidget", "DesignerPreviewLeft", "Left");

	FBaseSwitcherItem centerItem;
	centerItem.Id = TEXT("center");
	centerItem.Label = NSLOCTEXT("BaseSwitcherWidget", "DesignerPreviewCenter", "Center");

	FBaseSwitcherItem rightItem;
	rightItem.Id = TEXT("right");
	rightItem.Label = NSLOCTEXT("BaseSwitcherWidget", "DesignerPreviewRight", "Right");

	return { leftItem, centerItem, rightItem };
}

TSubclassOf<UBaseButtonWidget> UBaseSwitcherWidget::ResolveSegmentWidgetClass(const bool bNeedsIcon) const
{
	if (bNeedsIcon)
	{
		if (UClass* iconSegmentClass = LoadClass<UBaseButtonWidget>(nullptr, IconSegmentClassPath))
		{
			return iconSegmentClass;
		}
	}
	return SegmentWidgetClass;
}

void UBaseSwitcherWidget::RebuildSegments(const TArray<FBaseSwitcherItem>& renderedItems)
{
	if (!SegmentContainer || !GetWorld())
	{
		return;
	}

	UnbindGeneratedSegments();
	SegmentIdsByChildIndex.Reset();
	SegmentContainer->ClearChildren();
	for (const FBaseSwitcherItem& item : renderedItems)
	{
		const TSubclassOf<UBaseButtonWidget> resolvedSegmentClass = ResolveSegmentWidgetClass(item.Icon != nullptr);
		if (!resolvedSegmentClass)
		{
			continue;
		}

		UBaseButtonWidget* segment = CreateWidget<UBaseButtonWidget>(GetWorld(), resolvedSegmentClass);
		if (!segment)
		{
			continue;
		}

		segment->OnBaseClicked.RemoveDynamic(this, &UBaseSwitcherWidget::HandleSegmentClicked);
		segment->OnBaseClicked.AddDynamic(this, &UBaseSwitcherWidget::HandleSegmentClicked);
		SegmentContainer->AddChild(segment);
		SegmentIdsByChildIndex.Add(item.Id);
		BaseWidgetPrivate::ApplySlotFill(segment, 1.0f);
	}
	RefreshSegments(renderedItems);
}

void UBaseSwitcherWidget::RefreshSegments(const TArray<FBaseSwitcherItem>& renderedItems)
{
	if (!SegmentContainer)
	{
		return;
	}

	SegmentIdsByChildIndex.Reset();
	SegmentIdsByChildIndex.SetNum(SegmentContainer->GetChildrenCount());
	const FName effectiveSelectedId = SelectedId.IsNone() && IsDesignTime() && !renderedItems.IsEmpty()
		? renderedItems[0].Id
		: SelectedId;
	const int32 childCount = FMath::Min(SegmentContainer->GetChildrenCount(), renderedItems.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseButtonWidget* segment = Cast<UBaseButtonWidget>(SegmentContainer->GetChildAt(itemIndex));
		if (!segment)
		{
			continue;
		}

		const FBaseSwitcherItem& item = renderedItems[itemIndex];
		segment->SetContentAlign(EBaseHorizontalContentAlign::Center);
		segment->SetColorsOverride(ColorsOverride);
		segment->SetSizesOverride(SizesOverride);
		// Ghost segments are borderless until selected, so only the active segment
		// shows the accent rounded surface inside the shared switcher frame.
		segment->SetVariant(EBaseWidgetVariant::Ghost);
		segment->SetLabel(item.Label);
		segment->SetIcon(item.Icon);
		if (UBaseToggleButtonWidget* toggleSegment = Cast<UBaseToggleButtonWidget>(segment))
		{
			toggleSegment->SetToggleStyle(EBaseToggleButtonStyle::Button);
			toggleSegment->SetShowStateText(false);
			toggleSegment->SetCheckState(item.Id == effectiveSelectedId ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
		}
		else
		{
			segment->SetSelected(item.Id == effectiveSelectedId);
		}
		segment->SetDisabled(bDisabled || item.bDisabled);
		SegmentIdsByChildIndex[itemIndex] = item.Id;
	}
}

void UBaseSwitcherWidget::UnbindGeneratedSegments()
{
	if (!SegmentContainer)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < SegmentContainer->GetChildrenCount(); ++childIndex)
	{
		if (UBaseButtonWidget* segment = Cast<UBaseButtonWidget>(SegmentContainer->GetChildAt(childIndex)))
		{
			segment->OnBaseClicked.RemoveDynamic(this, &UBaseSwitcherWidget::HandleSegmentClicked);
		}
	}
}

void UBaseSwitcherWidget::HandleSegmentClicked(UBaseButtonWidget* button)
{
	if (!SegmentContainer || !button)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < SegmentContainer->GetChildrenCount(); ++childIndex)
	{
		if (SegmentContainer->GetChildAt(childIndex) == button
			&& SegmentIdsByChildIndex.IsValidIndex(childIndex)
			&& !SegmentIdsByChildIndex[childIndex].IsNone())
		{
			SelectItemById(SegmentIdsByChildIndex[childIndex]);
			return;
		}
	}
}
