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
	ContentAlign = EBaseHorizontalContentAlign::Left;
	const bool bCallerDisabled = bDisabled && !bItemForcesDisabled;
	bItemForcesDisabled = Item.bDisabled || Item.bSeparator;
	bDisabled = bCallerDisabled || bItemForcesDisabled;
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (Item.bDanger && colors)
	{
		if (LabelTextBlock)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(colors->StatusDangerColor));
		}
		if (ShortcutTextBlock)
		{
			ShortcutTextBlock->SetColorAndOpacity(FSlateColor(colors->StatusDangerColor));
		}
	}
	if (ShortcutTextBlock)
	{
		SetTextBlockValue(ShortcutTextBlock.Get(), Item.Shortcut);
		ApplyTextStyle(ShortcutTextBlock.Get(), EBaseTextRole::Caption);
		if (Item.bDanger && colors)
		{
			ShortcutTextBlock->SetColorAndOpacity(FSlateColor(colors->StatusDangerColor));
		}
	}
	SetOptionalWidgetVisible(SubMenuCaretImage.Get(), Item.bHasSubMenu);
	SetOptionalWidgetVisible(SeparatorLineWidget.Get(), Item.bSeparator);
	SetOptionalWidgetVisible(ItemContent.Get(), !Item.bSeparator);
	if (!Item.bSeparator && colors && sizes)
	{
		const EBaseWidgetState effectiveState = GetEffectiveState();
		FLinearColor fillColor = FLinearColor::Transparent;
		if (!bDisabled)
		{
			if (effectiveState == EBaseWidgetState::Pressed)
			{
				fillColor = colors->SurfaceHoverColor;
			}
			else if (effectiveState == EBaseWidgetState::Hovered || effectiveState == EBaseWidgetState::Selected || bSelected)
			{
				fillColor = colors->SurfaceControlHoverColor;
			}
		}
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			FLinearColor::Transparent,
			sizes->Radius,
			0.0f);
	}
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

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (colors && sizes)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			colors->SurfacePanelColor,
			colors->LineFieldColor,
			sizes->Radius,
			sizes->BorderWidth);
	}
	if (PlaceholderTextBlock)
	{
		SetTextBlockValue(PlaceholderTextBlock.Get(), PlaceholderText, false);
		ApplyTextStyle(PlaceholderTextBlock.Get(), EBaseTextRole::Caption);
		SetOptionalWidgetVisible(PlaceholderTextBlock.Get(), Items.IsEmpty());
		if (colors)
		{
			ApplyTextColor(PlaceholderTextBlock.Get(), colors->TextMutedColor);
		}
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

void UBaseContextMenuWidget::SetPlaceholderText(const FText inPlaceholderText)
{
	PlaceholderText = inPlaceholderText;
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
		itemWidget->SetColorsOverride(ColorsOverride);
		itemWidget->SetSizesOverride(SizesOverride);
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

		itemWidget->SetColorsOverride(ColorsOverride);
		itemWidget->SetSizesOverride(SizesOverride);
		itemWidget->SetItem(Items[itemIndex]);
	}
}

void UBaseContextMenuWidget::HandleGeneratedItemSelected(UWidget* widget, const FName itemId)
{
	(void)widget;
	OnItemSelected.Broadcast(this, itemId);
}

UBaseContextMenuAnchorWidget::UBaseContextMenuAnchorWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	FBaseContextMenuItem firstItem;
	firstItem.Id = TEXT("open");
	firstItem.Label = NSLOCTEXT("BaseContextMenuAnchorWidget", "DefaultOpenItem", "Open");

	FBaseContextMenuItem secondItem;
	secondItem.Id = TEXT("rename");
	secondItem.Label = NSLOCTEXT("BaseContextMenuAnchorWidget", "DefaultRenameItem", "Rename");
	secondItem.Shortcut = NSLOCTEXT("BaseContextMenuAnchorWidget", "DefaultRenameShortcut", "F2");

	Items = { firstItem, secondItem };
}

void UBaseContextMenuAnchorWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	if (!PlaceholderTextBlock)
	{
		return;
	}

	SetTextBlockValue(PlaceholderTextBlock.Get(), PlaceholderText, false);
	ApplyTextStyle(PlaceholderTextBlock.Get(), EBaseTextRole::Caption);
	if (const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors())
	{
		ApplyTextColor(PlaceholderTextBlock.Get(), colors->TextMutedColor);
	}
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
	ActiveMenuWidget->SetColorsOverride(ColorsOverride);
	ActiveMenuWidget->SetSizesOverride(SizesOverride);
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
