#include "UI/BaseCheckBoxWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

namespace
{
	// Applies the checked mark color to common WBP-authored decorative mark types.
	void TintCheckMark(UWidget* markWidget, const FLinearColor& color)
	{
		if (UTextBlock* textBlock = Cast<UTextBlock>(markWidget))
		{
			textBlock->SetColorAndOpacity(FSlateColor(color));
			return;
		}
		if (UImage* image = Cast<UImage>(markWidget))
		{
			image->SetColorAndOpacity(color);
			return;
		}
		if (UBorder* border = Cast<UBorder>(markWidget))
		{
			BaseWidgetPrivate::ApplyBorderBrushTint(border, color);
		}
	}
}

void UBaseCheckBoxWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	const bool bEnabled = !bDisabled;
	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		LabelTextBlock->SetVisibility(LabelTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
		if (colors && !bEnabled)
		{
			ApplyTextColor(LabelTextBlock.Get(), colors->TextFaintColor);
		}
	}
	SetOptionalWidgetVisible(CheckMarkWidget.Get(), CheckState == ECheckBoxState::Checked);
	SetOptionalWidgetVisible(IndeterminateMarkWidget.Get(), CheckState == ECheckBoxState::Undetermined);
	if (colors)
	{
		TintCheckMark(CheckMarkWidget.Get(), colors->TextStrongColor);
		TintCheckMark(IndeterminateMarkWidget.Get(), colors->TextStrongColor);
	}
	if (colors && sizes)
	{
		const bool bCheckedLike = IsCheckedLikeState(CheckState);
		const FLinearColor fillColor = !bEnabled
			? colors->SurfaceChromeColor
			: (bCheckedLike ? colors->AccentColor : colors->SurfaceWellColor);
		const FLinearColor strokeColor = !bEnabled
			? colors->LineSubtleColor
			: (bCheckedLike ? colors->AccentColor : colors->LineFieldColor);
		BaseWidgetPrivate::ApplyRoundedSurface(
			nullptr,
			BoxSurfaceBorder.Get(),
			fillColor,
			strokeColor,
			sizes->Radius,
			sizes->BorderWidth);
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

void UBaseCheckBoxWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseCheckBoxWidget::SetCheckState(const ECheckBoxState inCheckState)
{
	CheckState = inCheckState;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(CheckState == ECheckBoxState::Unchecked
		? EBaseWidgetState::Default
		: EBaseWidgetState::Selected);
}

void UBaseCheckBoxWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : EBaseWidgetState::Default);
}

UBaseCheckBoxGroupWidget::UBaseCheckBoxGroupWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	FBaseCheckBoxGroupItem firstItem;
	firstItem.Id = TEXT("ExampleA");
	firstItem.Label = NSLOCTEXT("BaseCheckBoxGroupWidget", "DefaultFirstItem", "First option");
	firstItem.CheckState = ECheckBoxState::Checked;

	FBaseCheckBoxGroupItem secondItem;
	secondItem.Id = TEXT("ExampleB");
	secondItem.Label = NSLOCTEXT("BaseCheckBoxGroupWidget", "DefaultSecondItem", "Second option");
	secondItem.CheckState = ECheckBoxState::Unchecked;

	Items = { firstItem, secondItem };
}

void UBaseCheckBoxGroupWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	BaseWidgetPrivate::ApplySlotVerticalAlignment(ItemContainer.Get(), ContentVAlign);
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

void UBaseCheckBoxGroupWidget::SetContentVAlign(const EBaseVerticalContentAlign inContentVAlign)
{
	ContentVAlign = inContentVAlign;
	SynchronizeBaseProperties();
}

void UBaseCheckBoxGroupWidget::NativeDestruct()
{
	UnbindGeneratedItems();
	ItemIdsByChildIndex.Reset();
	Super::NativeDestruct();
}

void UBaseCheckBoxGroupWidget::RebuildItems()
{
	if (!ItemContainer || !ItemWidgetClass || !GetWorld())
	{
		return;
	}

	UnbindGeneratedItems();
	ItemIdsByChildIndex.Reset();
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
		itemWidget->SetColorsOverride(ColorsOverride);
		itemWidget->SetSizesOverride(SizesOverride);
		ItemContainer->AddChild(itemWidget);
		ItemIdsByChildIndex.Add(item.Id);
	}
	RefreshItems();
}

void UBaseCheckBoxGroupWidget::RefreshItems()
{
	if (!ItemContainer)
	{
		return;
	}

	ItemIdsByChildIndex.Reset();
	ItemIdsByChildIndex.SetNum(ItemContainer->GetChildrenCount());
	const int32 childCount = FMath::Min(ItemContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseCheckBoxWidget* itemWidget = Cast<UBaseCheckBoxWidget>(ItemContainer->GetChildAt(itemIndex));
		if (!itemWidget)
		{
			continue;
		}

		const FBaseCheckBoxGroupItem& item = Items[itemIndex];
		itemWidget->SetColorsOverride(ColorsOverride);
		itemWidget->SetSizesOverride(SizesOverride);
		itemWidget->SetLabel(item.Label);
		itemWidget->SetCheckState(item.CheckState);
		itemWidget->SetDisabled(item.bDisabled);
		ItemIdsByChildIndex[itemIndex] = item.Id;
	}
}

void UBaseCheckBoxGroupWidget::UnbindGeneratedItems()
{
	if (!ItemContainer)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < ItemContainer->GetChildrenCount(); ++childIndex)
	{
		if (UBaseCheckBoxWidget* itemWidget = Cast<UBaseCheckBoxWidget>(ItemContainer->GetChildAt(childIndex)))
		{
			itemWidget->OnCheckStateChanged.RemoveDynamic(this, &UBaseCheckBoxGroupWidget::HandleItemWidgetCheckStateChanged);
		}
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
	UBaseCheckBoxWidget* checkBoxWidget = Cast<UBaseCheckBoxWidget>(widget);
	if (!ItemContainer || !checkBoxWidget)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < ItemContainer->GetChildrenCount(); ++childIndex)
	{
		if (ItemContainer->GetChildAt(childIndex) == checkBoxWidget
			&& ItemIdsByChildIndex.IsValidIndex(childIndex)
			&& !ItemIdsByChildIndex[childIndex].IsNone())
		{
			SetItemCheckState(ItemIdsByChildIndex[childIndex], inCheckState);
			return;
		}
	}
}
