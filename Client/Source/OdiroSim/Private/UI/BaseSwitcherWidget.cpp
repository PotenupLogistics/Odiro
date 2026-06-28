#include "UI/BaseSwitcherWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseToggleButtonWidget.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"

using namespace BaseFormElementPrivate;

void UBaseSwitcherWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	if (SegmentContainer && SegmentContainer->GetChildrenCount() != Items.Num())
	{
		RebuildSegments();
		return;
	}
	RefreshSegments();
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
	RebuildSegments();
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

void UBaseSwitcherWidget::RebuildSegments()
{
	if (!SegmentContainer || !SegmentWidgetClass || !GetWorld())
	{
		return;
	}

	SegmentIdByWidget.Reset();
	SegmentContainer->ClearChildren();
	for (const FBaseSwitcherItem& item : Items)
	{
		UBaseToggleButtonWidget* segment = CreateWidget<UBaseToggleButtonWidget>(GetWorld(), SegmentWidgetClass);
		if (!segment)
		{
			continue;
		}

		segment->OnBaseClicked.RemoveDynamic(this, &UBaseSwitcherWidget::HandleSegmentClicked);
		segment->OnBaseClicked.AddDynamic(this, &UBaseSwitcherWidget::HandleSegmentClicked);
		SegmentIdByWidget.Add(TWeakObjectPtr<UBaseToggleButtonWidget>(segment), item.Id);
		SegmentContainer->AddChild(segment);
	}
	RefreshSegments();
}

void UBaseSwitcherWidget::RefreshSegments()
{
	if (!SegmentContainer)
	{
		return;
	}

	SegmentIdByWidget.Reset();
	const int32 childCount = FMath::Min(SegmentContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseToggleButtonWidget* segment = Cast<UBaseToggleButtonWidget>(SegmentContainer->GetChildAt(itemIndex));
		if (!segment)
		{
			continue;
		}

		const FBaseSwitcherItem& item = Items[itemIndex];
		segment->SetToggleStyle(EBaseToggleButtonStyle::Button);
		segment->SetShowStateText(false);
		// Ghost segments are borderless until selected, so only the active segment
		// shows the accent rounded surface inside the shared switcher frame.
		segment->SetVariant(EBaseWidgetVariant::Ghost);
		segment->SetLabel(item.Label);
		segment->SetIcon(item.Icon);
		segment->SetCheckState(item.Id == SelectedId ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
		segment->SetDisabled(bDisabled || item.bDisabled);
		SegmentIdByWidget.Add(TWeakObjectPtr<UBaseToggleButtonWidget>(segment), item.Id);
	}
}

void UBaseSwitcherWidget::HandleSegmentClicked(UBaseButtonWidget* button)
{
	if (UBaseToggleButtonWidget* segment = Cast<UBaseToggleButtonWidget>(button))
	{
		const TWeakObjectPtr<UBaseToggleButtonWidget> segmentKey(segment);
		if (const FName* itemId = SegmentIdByWidget.Find(segmentKey))
		{
			SelectItemById(*itemId);
		}
	}
}
