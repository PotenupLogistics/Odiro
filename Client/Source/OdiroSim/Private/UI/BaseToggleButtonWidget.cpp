#include "UI/BaseToggleButtonWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseSwitchWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseToggleButtonWidget::SynchronizeBaseProperties()
{
	const bool bSwitchStyle = ToggleStyle == EBaseToggleButtonStyle::Switch;
	const bool bChecked = CheckState == ECheckBoxState::Checked;
	bSelected = !bSwitchStyle && bChecked;
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
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
		LabelTextBlock->SetJustification(bShowStateText ? ETextJustify::Left : ETextJustify::Center);
	}
	if (SwitchVisual)
	{
		SwitchVisual->SetCheckState(CheckState);
		SwitchVisual->SetDisabled(bDisabled);
	}
	if (!bSwitchStyle && CheckState == ECheckBoxState::Undetermined && tokens)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			tokens->SurfaceControlColor,
			tokens->AccentColor,
			tokens->Radius,
			tokens->BorderWidth);
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
		OnCheckStateChanged.Broadcast(this, CheckState);
	}
	Super::NativeOnClicked();
}
