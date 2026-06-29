#include "UI/BaseCheckBoxWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseCheckBoxWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	TGuardValue<bool> synchronizingGuard(bSynchronizing, true);
	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const bool bEnabled = !bDisabled;
	if (NativeCheckBox)
	{
		NativeCheckBox->SetCheckedState(CheckState);
		NativeCheckBox->SetIsEnabled(bEnabled);
		// The custom box/marks render the state, so the native checkbox is hidden
		// entirely (no duplicate box); row clicks drive the toggle instead.
		NativeCheckBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LabelTextBlock)
	{
		SetTextBlockValue(LabelTextBlock.Get(), Label, false);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		if (tokens && !bEnabled)
		{
			ApplyTextColor(LabelTextBlock.Get(), tokens->TextFaintColor);
		}
	}
	SetOptionalWidgetVisible(CheckMarkWidget.Get(), CheckState == ECheckBoxState::Checked);
	SetOptionalWidgetVisible(IndeterminateMarkWidget.Get(), CheckState == ECheckBoxState::Undetermined);
	if (tokens)
	{
		const bool bCheckedLike = IsCheckedLikeState(CheckState);
		const FLinearColor fillColor = !bEnabled
			? tokens->SurfaceChromeColor
			: (bCheckedLike ? tokens->AccentColor : tokens->SurfaceWellColor);
		const FLinearColor strokeColor = !bEnabled
			? tokens->LineSubtleColor
			: (bCheckedLike ? tokens->AccentColor : tokens->LineFieldColor);
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			BoxSurfaceBorder.Get(),
			fillColor,
			strokeColor,
			tokens->Radius,
			tokens->BorderWidth);
	}
}

int32 UBaseCheckBoxWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const FVector2D boxSize = BoxSurfaceBorder
		? BoxSurfaceBorder->GetCachedGeometry().GetLocalSize()
		: AllottedGeometry.GetLocalSize();
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(BoxSurfaceBorder.Get(), boxSize);
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseCheckBoxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Hit-testable so the whole row receives the toggle click.
	SetVisibility(ESlateVisibility::Visible);

	if (NativeCheckBox)
	{
		NativeCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UBaseCheckBoxWidget::HandleNativeCheckStateChanged);
		NativeCheckBox->OnCheckStateChanged.AddDynamic(this, &UBaseCheckBoxWidget::HandleNativeCheckStateChanged);
	}
}

FReply UBaseCheckBoxWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDisabled && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const ECheckBoxState nextState = CheckState == ECheckBoxState::Checked
			? ECheckBoxState::Unchecked
			: ECheckBoxState::Checked;
		SetCheckState(nextState);
		OnCheckStateChanged.Broadcast(this, CheckState);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBaseCheckBoxWidget::NativeDestruct()
{
	if (NativeCheckBox)
	{
		NativeCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UBaseCheckBoxWidget::HandleNativeCheckStateChanged);
	}

	Super::NativeDestruct();
}

void UBaseCheckBoxWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseCheckBoxWidget::SetCheckState(const ECheckBoxState inCheckState)
{
	CheckState = inCheckState;
	SynchronizeBaseProperties();
}

void UBaseCheckBoxWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseCheckBoxWidget::HandleNativeCheckStateChanged(const bool bIsChecked)
{
	if (bSynchronizing)
	{
		return;
	}

	CheckState = bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	SynchronizeBaseProperties();
	OnCheckStateChanged.Broadcast(this, CheckState);
}

void UBaseCheckBoxGroupWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	if (ItemContainer && ItemContainer->GetChildrenCount() != Items.Num())
	{
		RebuildItems();
		return;
	}
	RefreshItems();
}

void UBaseCheckBoxGroupWidget::SetItems(const TArray<FBaseCheckBoxGroupItem>& inItems)
{
	Items = inItems;
	RefreshParentStates();
	RebuildItems();
	SynchronizeBaseProperties();
}

bool UBaseCheckBoxGroupWidget::SetItemCheckState(const FName itemId, const ECheckBoxState inCheckState)
{
	const int32 itemIndex = FindCheckBoxItemIndexById(Items, itemId);
	if (itemIndex == INDEX_NONE)
	{
		return false;
	}

	const ECheckBoxState normalizedState = inCheckState == ECheckBoxState::Checked
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
	Items[itemIndex].CheckState = normalizedState;

	TArray<FName> stack;
	stack.Add(itemId);
	while (!stack.IsEmpty())
	{
		const FName parentId = stack.Pop(EAllowShrinking::No);
		for (FBaseCheckBoxGroupItem& item : Items)
		{
			if (item.ParentId == parentId)
			{
				item.CheckState = normalizedState;
				stack.Add(item.Id);
			}
		}
	}

	RefreshParentStates();
	SynchronizeBaseProperties();
	OnItemCheckStateChanged.Broadcast(this, itemId, Items[itemIndex].CheckState);
	return true;
}

ECheckBoxState UBaseCheckBoxGroupWidget::GetItemCheckState(const FName itemId) const
{
	const int32 itemIndex = FindCheckBoxItemIndexById(Items, itemId);
	return itemIndex == INDEX_NONE ? ECheckBoxState::Unchecked : Items[itemIndex].CheckState;
}

void UBaseCheckBoxGroupWidget::RebuildItems()
{
	if (!ItemContainer || !ItemWidgetClass || !GetWorld())
	{
		return;
	}

	ItemIdByWidget.Reset();
	ItemContainer->ClearChildren();
	for (const FBaseCheckBoxGroupItem& item : Items)
	{
		UBaseCheckBoxWidget* itemWidget = CreateWidget<UBaseCheckBoxWidget>(GetWorld(), ItemWidgetClass);
		if (!itemWidget)
		{
			continue;
		}

		itemWidget->OnCheckStateChanged.RemoveDynamic(this, &UBaseCheckBoxGroupWidget::HandleItemWidgetCheckStateChanged);
		itemWidget->OnCheckStateChanged.AddDynamic(this, &UBaseCheckBoxGroupWidget::HandleItemWidgetCheckStateChanged);
		ItemIdByWidget.Add(TWeakObjectPtr<UBaseCheckBoxWidget>(itemWidget), item.Id);
		ItemContainer->AddChild(itemWidget);
	}
	RefreshItems();
}

void UBaseCheckBoxGroupWidget::RefreshItems()
{
	if (!ItemContainer)
	{
		return;
	}

	ItemIdByWidget.Reset();
	const int32 childCount = FMath::Min(ItemContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseCheckBoxWidget* itemWidget = Cast<UBaseCheckBoxWidget>(ItemContainer->GetChildAt(itemIndex));
		if (!itemWidget)
		{
			continue;
		}

		const FBaseCheckBoxGroupItem& item = Items[itemIndex];
		itemWidget->SetLabel(item.Label);
		itemWidget->SetCheckState(item.CheckState);
		itemWidget->SetDisabled(item.bDisabled);
		ItemIdByWidget.Add(TWeakObjectPtr<UBaseCheckBoxWidget>(itemWidget), item.Id);
	}
}

void UBaseCheckBoxGroupWidget::RefreshParentStates()
{
	for (int32 passIndex = 0; passIndex < Items.Num(); ++passIndex)
	{
		bool bChanged = false;
		for (FBaseCheckBoxGroupItem& parentItem : Items)
		{
			TArray<ECheckBoxState> childStates;
			for (const FBaseCheckBoxGroupItem& possibleChild : Items)
			{
				if (possibleChild.ParentId == parentItem.Id)
				{
					childStates.Add(possibleChild.CheckState);
				}
			}

			if (childStates.IsEmpty())
			{
				if (parentItem.CheckState == ECheckBoxState::Undetermined)
				{
					parentItem.CheckState = ECheckBoxState::Unchecked;
					bChanged = true;
				}
				continue;
			}

			bool bAllChecked = true;
			bool bAllUnchecked = true;
			for (const ECheckBoxState childState : childStates)
			{
				bAllChecked &= childState == ECheckBoxState::Checked;
				bAllUnchecked &= childState == ECheckBoxState::Unchecked;
			}

			const ECheckBoxState newState = bAllChecked
				? ECheckBoxState::Checked
				: (bAllUnchecked ? ECheckBoxState::Unchecked : ECheckBoxState::Undetermined);
			if (parentItem.CheckState != newState)
			{
				parentItem.CheckState = newState;
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			break;
		}
	}
}

void UBaseCheckBoxGroupWidget::HandleItemWidgetCheckStateChanged(UWidget* widget, const ECheckBoxState inCheckState)
{
	if (UBaseCheckBoxWidget* checkBoxWidget = Cast<UBaseCheckBoxWidget>(widget))
	{
		const TWeakObjectPtr<UBaseCheckBoxWidget> itemKey(checkBoxWidget);
		if (const FName* itemId = ItemIdByWidget.Find(itemKey))
		{
			SetItemCheckState(*itemId, inCheckState);
		}
	}
}
