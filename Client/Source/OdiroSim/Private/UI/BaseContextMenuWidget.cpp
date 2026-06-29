#include "UI/BaseContextMenuWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseContextMenuItemWidget::SynchronizeBaseProperties()
{
	Label = Item.Label;
	Icon = Item.Icon;
	Variant = EBaseWidgetVariant::Ghost;
	const bool bCallerDisabled = bDisabled && !bItemForcesDisabled;
	bItemForcesDisabled = Item.bDisabled || Item.bSeparator;
	bDisabled = bCallerDisabled || bItemForcesDisabled;
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (Item.bDanger && tokens)
	{
		if (LabelTextBlock)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(tokens->StatusDangerColor));
		}
		if (ShortcutTextBlock)
		{
			ShortcutTextBlock->SetColorAndOpacity(FSlateColor(tokens->StatusDangerColor));
		}
	}
	if (ShortcutTextBlock)
	{
		SetTextBlockValue(ShortcutTextBlock.Get(), Item.Shortcut);
		ApplyTextStyle(ShortcutTextBlock.Get(), EBaseTextRole::Caption);
		if (Item.bDanger && tokens)
		{
			ShortcutTextBlock->SetColorAndOpacity(FSlateColor(tokens->StatusDangerColor));
		}
	}
	SetOptionalWidgetVisible(SubMenuCaretImage.Get(), Item.bHasSubMenu);
	SetOptionalWidgetVisible(SeparatorLineWidget.Get(), Item.bSeparator);
	SetOptionalWidgetVisible(ItemContent.Get(), !Item.bSeparator);
	if (Item.bSeparator)
	{
		BaseWidgetPrivate::MakeBorderVisualTransparent(BorderFrame.Get());
		BaseWidgetPrivate::MakeBorderVisualTransparent(SurfaceBorder.Get());
	}
}

void UBaseContextMenuItemWidget::SetItem(const FBaseContextMenuItem& inItem)
{
	Item = inItem;
	SynchronizeBaseProperties();
}

void UBaseContextMenuItemWidget::NativeOnClicked()
{
	if (!Item.bSeparator && !Item.bDisabled)
	{
		OnItemSelected.Broadcast(this, Item.Id);
	}
	Super::NativeOnClicked();
}

void UBaseContextMenuWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (tokens)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			tokens->SurfacePanelColor,
			tokens->LineFieldColor,
			tokens->Radius,
			tokens->BorderWidth);
	}
	if (ItemContainer && ItemContainer->GetChildrenCount() != Items.Num())
	{
		RebuildItems();
		return;
	}
	RefreshItems();
}

int32 UBaseContextMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseContextMenuWidget::SetItems(const TArray<FBaseContextMenuItem>& inItems)
{
	Items = inItems;
	RebuildItems();
	SynchronizeBaseProperties();
}

void UBaseContextMenuWidget::RebuildItems()
{
	if (!ItemContainer || !ItemWidgetClass || !GetWorld())
	{
		return;
	}

	ItemContainer->ClearChildren();
	for (const FBaseContextMenuItem& item : Items)
	{
		UBaseContextMenuItemWidget* itemWidget = CreateWidget<UBaseContextMenuItemWidget>(GetWorld(), ItemWidgetClass);
		if (!itemWidget)
		{
			continue;
		}

		itemWidget->OnItemSelected.RemoveDynamic(this, &UBaseContextMenuWidget::HandleGeneratedItemSelected);
		itemWidget->OnItemSelected.AddDynamic(this, &UBaseContextMenuWidget::HandleGeneratedItemSelected);
		ItemContainer->AddChild(itemWidget);
	}
	RefreshItems();
}

void UBaseContextMenuWidget::RefreshItems()
{
	if (!ItemContainer)
	{
		return;
	}

	const int32 childCount = FMath::Min(ItemContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseContextMenuItemWidget* itemWidget = Cast<UBaseContextMenuItemWidget>(ItemContainer->GetChildAt(itemIndex));
		if (!itemWidget)
		{
			continue;
		}

		itemWidget->SetItem(Items[itemIndex]);
	}
}

void UBaseContextMenuWidget::HandleGeneratedItemSelected(UWidget* widget, const FName itemId)
{
	(void)widget;
	OnItemSelected.Broadcast(this, itemId);
}

void UBaseContextMenuAnchorWidget::SetItems(const TArray<FBaseContextMenuItem>& inItems)
{
	Items = inItems;
	if (ActiveMenuWidget)
	{
		ActiveMenuWidget->SetItems(Items);
	}
}

FReply UBaseContextMenuAnchorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	(void)InGeometry;
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OpenMenuAt(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	if (ActiveMenuWidget)
	{
		CloseMenu();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBaseContextMenuAnchorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UBaseContextMenuAnchorWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	CloseMenu();
	Super::NativeOnFocusLost(InFocusEvent);
}

void UBaseContextMenuAnchorWidget::NativeDestruct()
{
	CloseMenu();
	Super::NativeDestruct();
}

void UBaseContextMenuAnchorWidget::OpenMenuAt(const FVector2D& screenPosition)
{
	CloseMenu();

	if (!MenuWidgetClass)
	{
		return;
	}
	ActiveMenuWidget = CreateWidget<UBaseContextMenuWidget>(GetWorld(), MenuWidgetClass);
	if (!ActiveMenuWidget)
	{
		return;
	}

	ActiveMenuWidget->SetItems(Items);
	ActiveMenuWidget->OnItemSelected.RemoveDynamic(this, &UBaseContextMenuAnchorWidget::HandleMenuItemSelected);
	ActiveMenuWidget->OnItemSelected.AddDynamic(this, &UBaseContextMenuAnchorWidget::HandleMenuItemSelected);
	ActiveMenuWidget->AddToViewport(MenuZOrder);

	FVector2D pixelPosition = FVector2D::ZeroVector;
	FVector2D viewportPosition = FVector2D::ZeroVector;
	USlateBlueprintLibrary::AbsoluteToViewport(this, screenPosition, pixelPosition, viewportPosition);
	ActiveMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	ActiveMenuWidget->SetPositionInViewport(viewportPosition, false);
	SetKeyboardFocus();
}

void UBaseContextMenuAnchorWidget::CloseMenu()
{
	if (ActiveMenuWidget)
	{
		ActiveMenuWidget->RemoveFromParent();
		ActiveMenuWidget = nullptr;
	}
}

void UBaseContextMenuAnchorWidget::HandleMenuItemSelected(UWidget* widget, const FName itemId)
{
	(void)widget;
	OnItemSelected.Broadcast(this, itemId);
	CloseMenu();
}
