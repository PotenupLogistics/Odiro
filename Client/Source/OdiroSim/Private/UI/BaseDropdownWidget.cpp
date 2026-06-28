#include "UI/BaseDropdownWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseButtonWidget.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseDropdownWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const FBaseDropdownItem* selectedItem = FindDropdownItemById(Items, SelectedId);
	if (SelectedTextBlock)
	{
		SetTextBlockValue(SelectedTextBlock.Get(), selectedItem ? selectedItem->Label : FText::GetEmpty(), false);
		ApplyTextStyle(SelectedTextBlock.Get(), EBaseTextRole::Label);
		if (tokens && bDisabled)
		{
			ApplyTextColor(SelectedTextBlock.Get(), tokens->TextFaintColor);
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
		if (tokens)
		{
			CaretImage->SetColorAndOpacity(bDisabled ? tokens->TextFaintColor : tokens->TextSecondaryColor);
		}
	}
	const bool bUseEmbeddedOptions = UsesEmbeddedOptionList();
	if (OptionListSurface)
	{
		// One rounded panel wraps the whole option list; individual rows are
		// borderless so the list reads as a single surface.
		SetOptionalWidgetVisible(OptionListSurface.Get(), bOpen && bUseEmbeddedOptions, ESlateVisibility::Visible);
		if (tokens)
		{
			BaseWidgetPrivate::ApplyRoundedSurface(
				nullptr,
				OptionListSurface.Get(),
				tokens->SurfacePanelColor,
				tokens->LineFieldColor,
				tokens->Radius,
				tokens->BorderWidth);
		}
	}
	if (tokens)
	{
		const FLinearColor fillColor = bDisabled ? tokens->SurfaceChromeColor : tokens->SurfaceWellColor;
		const FLinearColor strokeColor = bDisabled ? tokens->LineSubtleColor : (bOpen ? tokens->AccentFocusColor : tokens->LineFieldColor);
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			tokens->Radius,
			tokens->BorderWidth);
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
	ActiveMenuWidget->BaseTokens = BaseTokens;
	ActiveMenuWidget->Size = Size;
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

	OptionIdByWidget.Reset();
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
		OptionIdByWidget.Add(TWeakObjectPtr<UBaseButtonWidget>(option), item.Id);
		OptionContainer->AddChild(option);
	}
	RefreshOptions();
}

void UBaseDropdownWidget::RefreshOptions()
{
	if (!OptionContainer)
	{
		return;
	}

	OptionIdByWidget.Reset();
	const int32 childCount = FMath::Min(OptionContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseButtonWidget* option = Cast<UBaseButtonWidget>(OptionContainer->GetChildAt(itemIndex));
		if (!option)
		{
			continue;
		}

		const FBaseDropdownItem& item = Items[itemIndex];
		option->SetLabel(item.Label);
		option->SetIcon(item.Icon);
		option->SetSelected(item.Id == SelectedId);
		option->SetDisabled(bDisabled || item.bDisabled);
		OptionIdByWidget.Add(TWeakObjectPtr<UBaseButtonWidget>(option), item.Id);
	}
}

void UBaseDropdownWidget::HandleOptionClicked(UBaseButtonWidget* button)
{
	const TWeakObjectPtr<UBaseButtonWidget> optionKey(button);
	if (const FName* itemId = OptionIdByWidget.Find(optionKey))
	{
		SelectItemById(*itemId);
	}
}

void UBaseDropdownWidget::HandlePopupSelectionChanged(UWidget* widget, const FName selectedId)
{
	(void)widget;
	SelectItemById(selectedId);
}
