#include "UI/BaseTabWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/BaseWidgetPrivate.h"

namespace
{
	// Maps BaseButton content alignment to the overlay slot that owns the tab label group.
	EHorizontalAlignment ResolveTabContentAlignment(const EBaseHorizontalContentAlign contentAlign)
	{
		switch (contentAlign)
		{
		case EBaseHorizontalContentAlign::Left:
			return HAlign_Left;
		case EBaseHorizontalContentAlign::Right:
			return HAlign_Right;
		case EBaseHorizontalContentAlign::Center:
		default:
			return HAlign_Center;
		}
	}
}

void UBaseTabWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshCloseVisibility();
	RefreshDividerMetrics();
}

void UBaseTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnBaseClicked.RemoveDynamic(this, &UBaseTabWidget::HandleCloseClicked);
		CloseButton->OnBaseClicked.AddDynamic(this, &UBaseTabWidget::HandleCloseClicked);
	}
	RefreshCloseVisibility();
	RefreshTabContentLayout();
	RefreshDividerMetrics(true);
}

void UBaseTabWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnBaseClicked.RemoveDynamic(this, &UBaseTabWidget::HandleCloseClicked);
	}
	OnCloseRequestedNative.Clear();

	Super::NativeDestruct();
}

void UBaseTabWidget::SynchronizeBaseProperties()
{
	const bool bRequestedDisabled = bDisabled || State == EBaseWidgetState::Disabled;
	const EBaseWidgetState requestedState = State;

	Super::SynchronizeBaseProperties();
	RefreshTabContentLayout();
	RefreshDividerMetrics();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (!colors || !sizes)
	{
		return;
	}

	const EBaseWidgetState effectiveState = GetEffectiveState();
	const bool bEnabled = !bRequestedDisabled;
	const bool bActive = bSelected
		|| requestedState == EBaseWidgetState::Selected
		|| effectiveState == EBaseWidgetState::Selected;
	FLinearColor surfaceColor = FLinearColor::Transparent;
	FLinearColor frameColor = FLinearColor::Transparent;
	float borderWidth = 0.0f;

	if (bActive)
	{
		surfaceColor = effectiveState == EBaseWidgetState::Pressed
			? colors->SurfaceControlActiveColor
			: colors->SurfacePanelColor;
		frameColor = colors->LineSubtleColor;
		borderWidth = sizes->BorderWidth;
	}
	else if ((effectiveState == EBaseWidgetState::Hovered || effectiveState == EBaseWidgetState::Pressed) && bEnabled)
	{
		surfaceColor = colors->SurfaceHoverSoftColor;
	}

	BaseWidgetPrivate::ApplyTopRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		sizes->Radius,
		borderWidth);

	const FLinearColor labelColor = !bEnabled
		? colors->TextFaintColor
		: (bActive ? colors->TextStrongColor : colors->TextSecondaryColor);
	if (LabelTextBlock)
	{
		LabelTextBlock->SetColorAndOpacity(FSlateColor(labelColor));
	}
	if (IconImage)
	{
		IconImage->SetColorAndOpacity(labelColor);
	}
	if (IconGlyph)
	{
		IconGlyph->SetColorAndOpacity(FSlateColor(labelColor));
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshCloseVisibility();
}

void UBaseTabWidget::SetDividerSize(const float inDividerWidth, const float inDividerHeight)
{
	DividerWidth = FMath::Max(0.0f, inDividerWidth);
	DividerHeight = FMath::Max(0.0f, inDividerHeight);
	RefreshDividerMetrics(true);
}

void UBaseTabWidget::SetDividerEdgesVisible(
	const bool bInShowLeftDivider,
	const bool bInShowRightDivider)
{
	if (bShowLeftDivider == bInShowLeftDivider && bShowRightDivider == bInShowRightDivider)
	{
		return;
	}

	bShowLeftDivider = bInShowLeftDivider;
	bShowRightDivider = bInShowRightDivider;
	RefreshDividerMetrics();
}

void UBaseTabWidget::SetClosable(const bool bInClosable)
{
	bClosable = bInClosable;
	RefreshCloseVisibility();
}

void UBaseTabWidget::HandleCloseClicked(UBaseButtonWidget* button)
{
	if (!IsValid(button) || button != CloseButton)
	{
		return;
	}

	OnCloseRequestedNative.Broadcast(this);
}

void UBaseTabWidget::RefreshCloseVisibility() const
{
	if (CloseButton)
	{
		if (UOverlaySlot* closeButtonSlot = Cast<UOverlaySlot>(CloseButton->Slot))
		{
			if (closeButtonSlot->GetHorizontalAlignment() != HAlign_Right)
			{
				closeButtonSlot->SetHorizontalAlignment(HAlign_Right);
			}
			if (closeButtonSlot->GetVerticalAlignment() != VAlign_Center)
			{
				closeButtonSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		const bool bShouldDisableCloseButton = !bClosable;
		if (CloseButton->IsDisabled() != bShouldDisableCloseButton)
		{
			CloseButton->SetDisabled(bShouldDisableCloseButton);
		}

		const ESlateVisibility desiredVisibility = bClosable
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed;
		if (CloseButton->GetVisibility() != desiredVisibility)
		{
			CloseButton->SetVisibility(desiredVisibility);
		}
	}
}

void UBaseTabWidget::RefreshTabContentLayout() const
{
	if (SurfaceBorder && SurfaceBorder->GetHorizontalAlignment() != HAlign_Fill)
	{
		SurfaceBorder->SetHorizontalAlignment(HAlign_Fill);
	}
	if (SurfaceBorder && SurfaceBorder->GetVerticalAlignment() != VAlign_Fill)
	{
		SurfaceBorder->SetVerticalAlignment(VAlign_Fill);
	}

	UOverlaySlot* contentStackSlot = ContentStack ? Cast<UOverlaySlot>(ContentStack->Slot) : nullptr;
	if (contentStackSlot)
	{
		const EHorizontalAlignment desiredContentAlignment = ResolveTabContentAlignment(ContentAlign);
		if (contentStackSlot->GetHorizontalAlignment() != desiredContentAlignment)
		{
			contentStackSlot->SetHorizontalAlignment(desiredContentAlignment);
		}
		if (contentStackSlot->GetVerticalAlignment() != VAlign_Fill)
		{
			contentStackSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void UBaseTabWidget::RefreshDividerMetrics(const bool bUpdateSlotLayout) const
{
	const float width = FMath::Max(0.0f, DividerWidth);
	const float height = FMath::Max(0.0f, DividerHeight);
	const FVector2D dividerSize(width, height);
	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();

	auto applyDividerMetrics = [colors, dividerSize, width, height, bUpdateSlotLayout](
		UImage* divider,
		const EHorizontalAlignment horizontalAlignment,
		const bool bShowDivider)
	{
		if (!IsValid(divider))
		{
			return;
		}

		const ESlateVisibility desiredVisibility =
			bShowDivider && width > KINDA_SMALL_NUMBER && height > KINDA_SMALL_NUMBER
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed;
		if (divider->GetVisibility() != desiredVisibility)
		{
			divider->SetVisibility(desiredVisibility);
		}

		FSlateBrush brush = divider->GetBrush();
		if (!brush.ImageSize.Equals(dividerSize, KINDA_SMALL_NUMBER))
		{
			brush.ImageSize = dividerSize;
			divider->SetBrush(brush);
		}
		if (colors)
		{
			const FLinearColor desiredColor = colors->LineDividerColor;
			if (!divider->GetColorAndOpacity().Equals(desiredColor))
			{
				divider->SetColorAndOpacity(desiredColor);
			}
		}

		const float edgeDirection = horizontalAlignment == HAlign_Left ? -1.0f : 1.0f;
		const FVector2D desiredTranslation(edgeDirection * width * 0.5f, 0.0f);
		if (!divider->GetRenderTransform().Translation.Equals(desiredTranslation, KINDA_SMALL_NUMBER))
		{
			divider->SetRenderTranslation(desiredTranslation);
		}

		if (bUpdateSlotLayout)
		{
			if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(divider->Slot))
			{
				if (overlaySlot->GetHorizontalAlignment() != horizontalAlignment)
				{
					overlaySlot->SetHorizontalAlignment(horizontalAlignment);
				}
				if (overlaySlot->GetVerticalAlignment() != VAlign_Center)
				{
					overlaySlot->SetVerticalAlignment(VAlign_Center);
				}
				const FMargin slotPadding = overlaySlot->GetPadding();
				if (!FMath::IsNearlyZero(slotPadding.Left)
					|| !FMath::IsNearlyZero(slotPadding.Top)
					|| !FMath::IsNearlyZero(slotPadding.Right)
					|| !FMath::IsNearlyZero(slotPadding.Bottom))
				{
					overlaySlot->SetPadding(FMargin());
				}
			}
		}
	};

	applyDividerMetrics(LeftDivider.Get(), HAlign_Left, bShowLeftDivider);
	applyDividerMetrics(RightDivider.Get(), HAlign_Right, bShowRightDivider);
}
