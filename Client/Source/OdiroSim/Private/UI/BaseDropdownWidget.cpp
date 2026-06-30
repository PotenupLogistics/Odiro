#include "UI/BaseDropdownWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseButtonWidget.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

namespace
{
	void ApplyDropdownOverlaySlotPadding(UWidget* widget, const FMargin& padding)
	{
		if (widget)
		{
			if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(widget->Slot))
			{
				overlaySlot->SetPadding(padding);
			}
		}
	}
}

void UBaseDropdownWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	FBaseWidgetSizeConstraints effectiveSizeConstraints = SizeConstraints;
	if (effectiveSizeConstraints.MinWidth <= 0.0f)
	{
		effectiveSizeConstraints.MinWidth = 80.0f;
	}
	if (sizes && sizes->ControlHeight > 0.0f && effectiveSizeConstraints.MinHeight <= 0.0f)
	{
		effectiveSizeConstraints.MinHeight = sizes->ControlHeight;
	}
	BaseWidgetPrivate::ApplySizeConstraints(RootSize.Get(), effectiveSizeConstraints);
	if (RootSizeBox.Get() != RootSize.Get())
	{
		BaseWidgetPrivate::ApplySizeConstraints(RootSizeBox.Get(), effectiveSizeConstraints);
	}

	const FBaseDropdownItem* selectedItem = FindDropdownItemById(Items, SelectedId);
	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	if (SelectedTextBlock)
	{
		const FText selectedText = selectedItem ? selectedItem->Label : PlaceholderText;
		if (!selectedText.IsEmpty())
		{
			SetTextBlockValue(SelectedTextBlock.Get(), selectedText, false);
		}
		ApplyTextStyle(SelectedTextBlock.Get(), EBaseTextRole::Label);
		if (colors)
		{
			ApplyTextColor(SelectedTextBlock.Get(), bDisabled
				? colors->TextFaintColor
				: (selectedItem ? colors->TextPrimaryColor : colors->TextMutedColor));
		}
	}
	if (SelectedIconImage)
	{
		SetImageTexture(SelectedIconImage.Get(), selectedItem ? selectedItem->Icon.Get() : nullptr);
	}
	if (CaretImage)
	{
		SetOptionalWidgetVisible(CaretImage.Get(), true);
		CaretImage->SetRenderTransformAngle(bOpen ? 180.0f : 0.0f);
		if (colors)
		{
			CaretImage->SetColorAndOpacity(bDisabled ? colors->TextFaintColor : colors->TextSecondaryColor);
		}
	}
	const bool bUseEmbeddedOptions = UsesEmbeddedOptionList();
	const bool bShowEmbeddedOptions = bUseEmbeddedOptions && (bOpen || IsDesignTime());
	if (OptionListSurface)
	{
		// One rounded panel wraps the whole option list; individual rows are
		// borderless so the list reads as a single surface.
		SetOptionalWidgetVisible(OptionListSurface.Get(), bShowEmbeddedOptions, ESlateVisibility::Visible);
		if (colors && sizes)
		{
			BaseWidgetPrivate::ApplyRoundedSurface(
				nullptr,
				OptionListSurface.Get(),
				colors->SurfacePanelColor,
				colors->LineFieldColor,
				sizes->Radius,
				sizes->BorderWidth);
		}
	}
	if (EmptyOptionsTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(EmptyOptionsTextBlock.Get(), EmptyOptionsText);
		ApplyTextStyle(EmptyOptionsTextBlock.Get(), EBaseTextRole::Caption);
		if (colors)
		{
			ApplyTextColor(EmptyOptionsTextBlock.Get(), colors->TextMutedColor);
		}
		SetOptionalWidgetVisible(EmptyOptionsTextBlock.Get(), Items.IsEmpty() && bShowEmbeddedOptions);
	}
	if (colors && sizes)
	{
		if (SurfaceBorder)
		{
			const FMargin controlPadding(sizes->Space4, sizes->Space2);
			SurfaceBorder->SetPadding(controlPadding);
			ApplyDropdownOverlaySlotPadding(ClosedContent.Get(), controlPadding);
		}
		const FLinearColor fillColor = bDisabled ? colors->SurfaceChromeColor : colors->SurfaceWellColor;
		const FLinearColor strokeColor = bDisabled
			? colors->LineSubtleColor
			: (bOpen ? colors->AccentFocusColor : colors->LineFieldColor);
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			sizes->Radius,
			sizes->BorderWidth);
	}

	if (OptionContainer && OptionContainer->GetChildrenCount() != Items.Num())
	{
		RebuildOptions();
		return;
	}
	RefreshOptions();
}

int32 UBaseDropdownWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	if (OptionListSurface)
	{
		BaseWidgetPrivate::UpdateRoundedSurfaceSize(OptionListSurface.Get(), OptionListSurface->GetCachedGeometry().GetLocalSize());
	}
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseDropdownWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UBaseDropdownWidget::NativeDestruct()
{
	CloseOptionList();
	UnbindGeneratedOptions();
	OptionIdsByChildIndex.Reset();
	Super::NativeDestruct();
}

FReply UBaseDropdownWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDisabled && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bOpen = !bOpen;
		if (bOpen && !UsesEmbeddedOptionList())
		{
			OpenOptionListAt(InGeometry);
		}
		else
		{
			CloseOptionList();
		}
		SynchronizeBaseProperties();
		NotifyBaseVisualStateChanged(bOpen ? EBaseWidgetState::Selected : EBaseWidgetState::Default);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBaseDropdownWidget::SetItems(const TArray<FBaseDropdownItem>& inItems)
{
	Items = inItems;
	if (const FBaseDropdownItem* selectedItem = FindDropdownItemById(Items, SelectedId))
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
	if (ActiveMenuWidget)
	{
		ActiveMenuWidget->SetItems(Items);
	}
	RebuildOptions();
	SynchronizeBaseProperties();
}

void UBaseDropdownWidget::SetPlaceholderText(const FText inPlaceholderText)
{
	PlaceholderText = inPlaceholderText;
	SynchronizeBaseProperties();
}

void UBaseDropdownWidget::SetEmptyOptionsText(const FText inEmptyOptionsText)
{
	EmptyOptionsText = inEmptyOptionsText;
	SynchronizeBaseProperties();
}

bool UBaseDropdownWidget::SelectItemById(const FName itemId)
{
	if (bDisabled)
	{
		return false;
	}

	const FBaseDropdownItem* item = FindDropdownItemById(Items, itemId);
	if (!item || item->bDisabled)
	{
		return false;
	}

	const bool bChanged = SelectedId != itemId;
	SelectedId = itemId;
	SetOpen(false);
	if (bChanged)
	{
		OnSelectionChanged.Broadcast(this, SelectedId);
	}
	return true;
}

void UBaseDropdownWidget::SetOpen(const bool bInOpen)
{
	bOpen = bInOpen && !bDisabled;
	if (bOpen && !UsesEmbeddedOptionList())
	{
		OpenOptionListAt(GetCachedGeometry());
	}
	else
	{
		CloseOptionList();
	}
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bOpen ? EBaseWidgetState::Selected : EBaseWidgetState::Default);
}

void UBaseDropdownWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	if (bDisabled)
	{
		SetOpen(false);
		return;
	}
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : EBaseWidgetState::Default);
}

bool UBaseDropdownWidget::UsesEmbeddedOptionList() const
{
	return bPopupInstance || !MenuWidgetClass;
}

void UBaseDropdownWidget::OpenOptionListAt(const FGeometry& anchorGeometry)
{
	CloseOptionList();

	if (IsDesignTime() || !MenuWidgetClass || !GetWorld())
	{
		return;
	}

	ActiveMenuWidget = CreateWidget<UBaseDropdownWidget>(GetWorld(), MenuWidgetClass);
	if (!ActiveMenuWidget)
	{
		return;
	}

	ActiveMenuWidget->bPopupInstance = true;
	ActiveMenuWidget->Items = Items;
	ActiveMenuWidget->SelectedId = SelectedId;
	ActiveMenuWidget->bOpen = true;
	ActiveMenuWidget->bDisabled = bDisabled;
	ActiveMenuWidget->OptionWidgetClass = OptionWidgetClass;
	ActiveMenuWidget->SetColorsOverride(ColorsOverride);
	ActiveMenuWidget->SetSizesOverride(SizesOverride);
	ActiveMenuWidget->OnSelectionChanged.RemoveDynamic(this, &UBaseDropdownWidget::HandlePopupSelectionChanged);
	ActiveMenuWidget->OnSelectionChanged.AddDynamic(this, &UBaseDropdownWidget::HandlePopupSelectionChanged);
	ActiveMenuWidget->AddToViewport(MenuZOrder);

	FVector2D pixelPosition = FVector2D::ZeroVector;
	FVector2D viewportPosition = FVector2D::ZeroVector;
	const FVector2D anchorBottomLeft = anchorGeometry.LocalToAbsolute(
		FVector2D(0.0f, anchorGeometry.GetLocalSize().Y));
	USlateBlueprintLibrary::AbsoluteToViewport(this, anchorBottomLeft, pixelPosition, viewportPosition);
	ActiveMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	ActiveMenuWidget->SetPositionInViewport(viewportPosition, false);
	ActiveMenuWidget->SynchronizeBaseProperties();
	SetKeyboardFocus();
}

void UBaseDropdownWidget::CloseOptionList()
{
	if (ActiveMenuWidget)
	{
		ActiveMenuWidget->OnSelectionChanged.RemoveDynamic(this, &UBaseDropdownWidget::HandlePopupSelectionChanged);
		ActiveMenuWidget->RemoveFromParent();
		ActiveMenuWidget = nullptr;
	}
}

void UBaseDropdownWidget::RebuildOptions()
{
	if (!OptionContainer || !OptionWidgetClass || !GetWorld())
	{
		return;
	}

	UnbindGeneratedOptions();
	OptionIdsByChildIndex.Reset();
	OptionContainer->ClearChildren();
	for (const FBaseDropdownItem& item : Items)
	{
		UBaseButtonWidget* option = CreateWidget<UBaseButtonWidget>(GetWorld(), OptionWidgetClass);
		if (!option)
		{
			continue;
		}

		option->OnBaseClicked.RemoveDynamic(this, &UBaseDropdownWidget::HandleOptionClicked);
		option->OnBaseClicked.AddDynamic(this, &UBaseDropdownWidget::HandleOptionClicked);
		option->SetColorsOverride(ColorsOverride);
		option->SetSizesOverride(SizesOverride);
		OptionContainer->AddChild(option);
		OptionIdsByChildIndex.Add(item.Id);
	}
	RefreshOptions();
}

void UBaseDropdownWidget::RefreshOptions()
{
	if (!OptionContainer)
	{
		return;
	}

	OptionIdsByChildIndex.Reset();
	OptionIdsByChildIndex.SetNum(OptionContainer->GetChildrenCount());
	const int32 childCount = FMath::Min(OptionContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseButtonWidget* option = Cast<UBaseButtonWidget>(OptionContainer->GetChildAt(itemIndex));
		if (!option)
		{
			continue;
		}

		const FBaseDropdownItem& item = Items[itemIndex];
		option->SetColorsOverride(ColorsOverride);
		option->SetSizesOverride(SizesOverride);
		option->SetLabel(item.Label);
		option->SetIcon(item.Icon);
		option->SetSelected(item.Id == SelectedId);
		option->SetDisabled(bDisabled || item.bDisabled);
		OptionIdsByChildIndex[itemIndex] = item.Id;
	}
}

void UBaseDropdownWidget::UnbindGeneratedOptions()
{
	if (!OptionContainer)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < OptionContainer->GetChildrenCount(); ++childIndex)
	{
		if (UBaseButtonWidget* option = Cast<UBaseButtonWidget>(OptionContainer->GetChildAt(childIndex)))
		{
			option->OnBaseClicked.RemoveDynamic(this, &UBaseDropdownWidget::HandleOptionClicked);
		}
	}
}

void UBaseDropdownWidget::HandleOptionClicked(UBaseButtonWidget* button)
{
	if (!OptionContainer || !button)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < OptionContainer->GetChildrenCount(); ++childIndex)
	{
		if (OptionContainer->GetChildAt(childIndex) == button
			&& OptionIdsByChildIndex.IsValidIndex(childIndex)
			&& !OptionIdsByChildIndex[childIndex].IsNone())
		{
			SelectItemById(OptionIdsByChildIndex[childIndex]);
			return;
		}
	}
}

void UBaseDropdownWidget::HandlePopupSelectionChanged(UWidget* widget, const FName selectedId)
{
	(void)widget;
	SelectItemById(selectedId);
}
