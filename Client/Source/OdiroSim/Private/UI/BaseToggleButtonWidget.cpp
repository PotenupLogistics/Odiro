#include "UI/BaseToggleButtonWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseSwitchWidget.h"
#include "Components/Border.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

namespace
{
	void ApplyToggleOverlaySlotPadding(UWidget* widget, const FMargin& padding)
	{
		if (widget)
		{
			if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(widget->Slot))
			{
				overlaySlot->SetPadding(padding);
			}
		}
	}

	// Maps base horizontal alignment to label justification.
	ETextJustify::Type ToToggleTextJustify(const EBaseHorizontalContentAlign alignment)
	{
		switch (alignment)
		{
		case EBaseHorizontalContentAlign::Left:
			return ETextJustify::Left;
		case EBaseHorizontalContentAlign::Right:
			return ETextJustify::Right;
		case EBaseHorizontalContentAlign::Center:
		default:
			return ETextJustify::Center;
		}
	}
}

void UBaseToggleButtonWidget::SynchronizeBaseProperties()
{
	const bool bSwitchStyle = ToggleStyle == EBaseToggleButtonStyle::Switch;
	const bool bChecked = CheckState == ECheckBoxState::Checked;
	bSelected = !bSwitchStyle && bChecked;
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (!bSwitchStyle && sizes)
	{
		FBaseWidgetSizeConstraints effectiveSizeConstraints = SizeConstraints;
		if (sizes->ControlHeight > 0.0f && effectiveSizeConstraints.MinHeight <= 0.0f)
		{
			effectiveSizeConstraints.MinHeight = sizes->ControlHeight;
		}
		BaseWidgetPrivate::ApplySizeConstraints(RootSize.Get(), effectiveSizeConstraints);
		if (RootSizeBox.Get() != RootSize.Get())
		{
			BaseWidgetPrivate::ApplySizeConstraints(RootSizeBox.Get(), effectiveSizeConstraints);
		}
		if (SurfaceBorder)
		{
			const FMargin controlPadding(sizes->Space4, sizes->Space2);
			SurfaceBorder->SetPadding(controlPadding);
			ApplyToggleOverlaySlotPadding(ToggleContent.Get(), controlPadding);
		}
	}
	SetOptionalWidgetVisible(ButtonVisualRoot.Get(), !bSwitchStyle);
	SetOptionalWidgetVisible(SwitchVisualRoot.Get(), bSwitchStyle);
	SetOptionalWidgetVisible(SwitchVisual.Get(), bSwitchStyle, ESlateVisibility::HitTestInvisible);
	if (!ButtonVisualRoot)
	{
		SetOptionalWidgetVisible(BorderFrame.Get(), !bSwitchStyle);
		SetOptionalWidgetVisible(SurfaceBorder.Get(), !bSwitchStyle);
	}
	if (StateTextBlock)
	{
		if (CheckState == ECheckBoxState::Checked)
		{
			StateTextBlock->SetText(CheckedStateText);
		}
		else if (CheckState == ECheckBoxState::Undetermined)
		{
			StateTextBlock->SetText(UndeterminedStateText);
		}
		else
		{
			StateTextBlock->SetText(UncheckedStateText);
		}
		ApplyTextStyle(StateTextBlock.Get(), EBaseTextRole::Caption);
		SetOptionalWidgetVisible(StateTextBlock.Get(), !bSwitchStyle && bShowStateText);
	}
	if (LabelTextBlock && bSwitchStyle)
	{
		SetOptionalWidgetVisible(LabelTextBlock.Get(), false);
	}
	else if (LabelTextBlock)
	{
		LabelTextBlock->SetJustification(bShowStateText ? ETextJustify::Left : ToToggleTextJustify(ContentAlign));
	}
	if (SwitchVisual)
	{
		SwitchVisual->SetColorsOverride(ColorsOverride);
		SwitchVisual->SetSizesOverride(SizesOverride);
		SwitchVisual->SetCheckState(CheckState);
		SwitchVisual->SetDisabled(bDisabled);
	}
	if (!bSwitchStyle && CheckState == ECheckBoxState::Undetermined && colors && sizes)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			colors->SurfaceControlColor,
			colors->AccentColor,
			sizes->Radius,
			sizes->BorderWidth);
	}
}

void UBaseToggleButtonWidget::SetToggleStyle(const EBaseToggleButtonStyle inToggleStyle)
{
	ToggleStyle = inToggleStyle;
	SynchronizeBaseProperties();
}

void UBaseToggleButtonWidget::SetCheckState(const ECheckBoxState inCheckState)
{
	CheckState = inCheckState;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
}

void UBaseToggleButtonWidget::SetShowStateText(const bool bInShowStateText)
{
	bShowStateText = bInShowStateText;
	SynchronizeBaseProperties();
}

void UBaseToggleButtonWidget::NativeOnClicked()
{
	if (!IsDisabled())
	{
		const ECheckBoxState nextState = CheckState == ECheckBoxState::Checked
			? ECheckBoxState::Unchecked
			: ECheckBoxState::Checked;
		CheckState = nextState;
		SynchronizeBaseProperties();
		NotifyBaseVisualStateChanged();
		OnCheckStateChanged.Broadcast(this, CheckState);
	}
	Super::NativeOnClicked();
}
