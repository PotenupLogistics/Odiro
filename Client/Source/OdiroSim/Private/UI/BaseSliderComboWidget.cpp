#include "UI/BaseSliderComboWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "UI/BaseSliderWidget.h"
#include "UI/BaseTextInputWidget.h"
#include "UI/BaseWidgetPrivate.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

using namespace BaseFormElementPrivate;

namespace
{
	// Applies the same operation to each non-null widget pointer in a fixed set.
	template <typename WidgetType, typename CallableType>
	void ForEachWidget(CallableType callable, WidgetType* first, WidgetType* second = nullptr, WidgetType* third = nullptr)
	{
		if (first)
		{
			callable(first);
		}
		if (second && second != first)
		{
			callable(second);
		}
		if (third && third != first && third != second)
		{
			callable(third);
		}
	}

	template <typename WidgetType>
	WidgetType* FindNamedWidget(UBaseSliderComboWidget* owner, const TCHAR* widgetName)
	{
		return owner ? Cast<WidgetType>(owner->GetWidgetFromName(FName(widgetName))) : nullptr;
	}

	// Returns whether any child size constraint should be forwarded to BaseTextInput.
	bool HasExplicitSizeConstraints(const FBaseWidgetSizeConstraints& constraints)
	{
		return constraints.MinWidth > 0.0f
			|| constraints.MinHeight > 0.0f
			|| constraints.MaxWidth > 0.0f
			|| constraints.MaxHeight > 0.0f;
	}

	// Merges non-zero overrides while keeping the child widget's WBP-authored size defaults.
	FBaseWidgetSizeConstraints MergeExplicitSizeConstraints(
		FBaseWidgetSizeConstraints baseConstraints,
		const FBaseWidgetSizeConstraints& overrideConstraints)
	{
		if (overrideConstraints.MinWidth > 0.0f)
		{
			baseConstraints.MinWidth = overrideConstraints.MinWidth;
		}
		if (overrideConstraints.MinHeight > 0.0f)
		{
			baseConstraints.MinHeight = overrideConstraints.MinHeight;
		}
		if (overrideConstraints.MaxWidth > 0.0f)
		{
			baseConstraints.MaxWidth = overrideConstraints.MaxWidth;
		}
		if (overrideConstraints.MaxHeight > 0.0f)
		{
			baseConstraints.MaxHeight = overrideConstraints.MaxHeight;
		}

		return BaseWidgetPrivate::NormalizeSizeConstraints(baseConstraints);
	}

	// Applies a combo-owned font-size override after the semantic base label style.
	void ApplyTextBlockFontSizeOverride(UTextBlock* textBlock, const float fontSizeOverride)
	{
		if (!IsValid(textBlock) || fontSizeOverride <= 0.0f)
		{
			return;
		}

		FSlateFontInfo font = textBlock->GetFont();
		font.Size = fontSizeOverride;
		textBlock->SetFont(font);
	}

	// Applies a combo-owned minimum label width without wrapping WBP layout in C++.
	void ApplyTextBlockMinWidthOverride(UTextBlock* textBlock, const float minWidthOverride)
	{
		if (!IsValid(textBlock) || minWidthOverride <= 0.0f)
		{
			return;
		}

		textBlock->SetMinDesiredWidth(minWidthOverride);
	}
}

void UBaseSliderComboWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	RefreshOptionalBindings();

	TGuardValue<bool> synchronizingGuard(bSynchronizing, true);
	const bool bCompact = ComboStyle == EBaseSliderComboStyle::Compact;
	SetOptionalWidgetVisible(CompactRoot.Get(), bCompact, ESlateVisibility::Visible);
	SetOptionalWidgetVisible(ModernRoot.Get(), !bCompact, ESlateVisibility::Visible);

	auto syncLabel = [this](UTextBlock* textBlock, const bool bVisible)
	{
		if (!textBlock)
		{
			return;
		}
		textBlock->SetText(Label);
		ApplyTextStyle(textBlock, EBaseTextRole::Label);
		ApplyTextBlockFontSizeOverride(textBlock, LabelFontSizeOverride);
		ApplyTextBlockMinWidthOverride(textBlock, LabelMinWidthOverride);
		SetOptionalWidgetVisible(textBlock, bVisible && bShowLabel);
	};
	syncLabel(LabelTextBlock.Get(), true);
	syncLabel(CompactLabelTextBlock.Get(), bCompact);
	syncLabel(ModernLabelTextBlock.Get(), !bCompact);

	ForEachWidget<UBaseSliderWidget>(
		[this](UBaseSliderWidget* slider)
		{
			SynchronizeSlider(slider);
		},
		Slider.Get(),
		bCompact ? CompactSlider.Get() : ModernSlider.Get());

	ForEachWidget<UBaseTextInputWidget>(
		[this, bCompact](UBaseTextInputWidget* input)
		{
			SynchronizeValueInput(input);
			const bool bVisible = bShowValueField && !bRangeMode
				&& (input == ValueInput.Get()
					|| (bCompact && input == CompactValueInputWidget.Get())
					|| (!bCompact && input == ModernValueInputWidget.Get()));
			SetOptionalWidgetVisible(input, bVisible, ESlateVisibility::Visible);
		},
		ValueInput.Get(),
		bCompact ? CompactValueInputWidget.Get() : ModernValueInputWidget.Get());

	ForEachWidget<UBaseTextInputWidget>(
		[this, bCompact](UBaseTextInputWidget* input)
		{
			SynchronizeRangeInput(input);
			const bool bVisible = bShowValueField && bRangeMode
				&& (input == RangeInput.Get()
					|| (bCompact && input == CompactRangeInputWidget.Get())
					|| (!bCompact && input == ModernRangeInputWidget.Get()));
			SetOptionalWidgetVisible(input, bVisible, ESlateVisibility::Visible);
		},
		RangeInput.Get(),
		bCompact ? CompactRangeInputWidget.Get() : ModernRangeInputWidget.Get());
}

void UBaseSliderComboWidget::SynchronizeSlider(UBaseSliderWidget* slider)
{
	if (!slider)
	{
		return;
	}

	slider->SetColorsOverride(ColorsOverride);
	slider->SetSizesOverride(SizesOverride);
	slider->SetValueRange(MinValue, MaxValue);
	slider->SetRangeMode(bRangeMode);
	if (bRangeMode)
	{
		slider->SetRangeValue(LowerValue, UpperValue);
	}
	else
	{
		slider->SetValue(Value);
	}
	slider->SetDisabled(bDisabled);
	SetOptionalWidgetVisible(slider, true, ESlateVisibility::Visible);
}

void UBaseSliderComboWidget::SynchronizeValueInput(UBaseTextInputWidget* input)
{
	if (!input)
	{
		return;
	}

	input->SetColorsOverride(ColorsOverride);
	input->SetSizesOverride(SizesOverride);
	if (TextInputFontSizeOverride > 0.0f)
	{
		input->SetFontSizeOverride(TextInputFontSizeOverride);
	}
	if (HasExplicitSizeConstraints(TextInputSizeConstraints))
	{
		input->SetSizeConstraints(MergeExplicitSizeConstraints(input->GetSizeConstraints(), TextInputSizeConstraints));
	}
	input->SetInputMode(EBaseTextInputMode::Number);
	input->SetDisplayDecimals(DisplayDecimals);
	input->SetValueRange(MinValue, MaxValue);
	input->SetNumericValue(Value);
	input->SetDisabled(bDisabled || !bShowValueField);
}

void UBaseSliderComboWidget::SynchronizeRangeInput(UBaseTextInputWidget* input)
{
	if (!input)
	{
		return;
	}

	input->SetColorsOverride(ColorsOverride);
	input->SetSizesOverride(SizesOverride);
	if (TextInputFontSizeOverride > 0.0f)
	{
		input->SetFontSizeOverride(TextInputFontSizeOverride);
	}
	if (HasExplicitSizeConstraints(TextInputSizeConstraints))
	{
		input->SetSizeConstraints(MergeExplicitSizeConstraints(input->GetSizeConstraints(), TextInputSizeConstraints));
	}
	input->SetInputMode(EBaseTextInputMode::NumberRange);
	input->SetDisplayDecimals(DisplayDecimals);
	input->SetValueRange(MinValue, MaxValue);
	input->SetRangeValue(LowerValue, UpperValue);
	input->SetDisabled(bDisabled || !bShowValueField);
}

void UBaseSliderComboWidget::RefreshOptionalBindings()
{
	CompactValueInputWidget = FindNamedWidget<UBaseTextInputWidget>(this, TEXT("CompactValueInput"));
	ModernValueInputWidget = FindNamedWidget<UBaseTextInputWidget>(this, TEXT("ModernValueInput"));
	CompactRangeInputWidget = FindNamedWidget<UBaseTextInputWidget>(this, TEXT("CompactRangeInput"));
	ModernRangeInputWidget = FindNamedWidget<UBaseTextInputWidget>(this, TEXT("ModernRangeInput"));
}

void UBaseSliderComboWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshOptionalBindings();

	ForEachWidget<UBaseSliderWidget>(
		[this](UBaseSliderWidget* slider)
		{
			slider->OnValueChanged.RemoveDynamic(this, &UBaseSliderComboWidget::HandleSliderValueChanged);
			slider->OnValueChanged.AddDynamic(this, &UBaseSliderComboWidget::HandleSliderValueChanged);
			slider->OnRangeValueChanged.RemoveDynamic(this, &UBaseSliderComboWidget::HandleSliderRangeValueChanged);
			slider->OnRangeValueChanged.AddDynamic(this, &UBaseSliderComboWidget::HandleSliderRangeValueChanged);
		},
		Slider.Get(),
		CompactSlider.Get(),
		ModernSlider.Get());

	ForEachWidget<UBaseTextInputWidget>(
		[this](UBaseTextInputWidget* input)
		{
			input->OnNumericValueCommitted.RemoveDynamic(this, &UBaseSliderComboWidget::HandleValueInputCommitted);
			input->OnNumericValueCommitted.AddDynamic(this, &UBaseSliderComboWidget::HandleValueInputCommitted);
		},
		ValueInput.Get(),
		CompactValueInputWidget.Get(),
		ModernValueInputWidget.Get());

	ForEachWidget<UBaseTextInputWidget>(
		[this](UBaseTextInputWidget* input)
		{
			input->OnRangeValueCommitted.RemoveDynamic(this, &UBaseSliderComboWidget::HandleRangeInputCommitted);
			input->OnRangeValueCommitted.AddDynamic(this, &UBaseSliderComboWidget::HandleRangeInputCommitted);
		},
		RangeInput.Get(),
		CompactRangeInputWidget.Get(),
		ModernRangeInputWidget.Get());
}

void UBaseSliderComboWidget::NativeDestruct()
{
	ForEachWidget<UBaseSliderWidget>(
		[this](UBaseSliderWidget* slider)
		{
			slider->OnValueChanged.RemoveDynamic(this, &UBaseSliderComboWidget::HandleSliderValueChanged);
			slider->OnRangeValueChanged.RemoveDynamic(this, &UBaseSliderComboWidget::HandleSliderRangeValueChanged);
		},
		Slider.Get(),
		CompactSlider.Get(),
		ModernSlider.Get());

	ForEachWidget<UBaseTextInputWidget>(
		[this](UBaseTextInputWidget* input)
		{
			input->OnNumericValueCommitted.RemoveDynamic(this, &UBaseSliderComboWidget::HandleValueInputCommitted);
		},
		ValueInput.Get(),
		CompactValueInputWidget.Get(),
		ModernValueInputWidget.Get());

	ForEachWidget<UBaseTextInputWidget>(
		[this](UBaseTextInputWidget* input)
		{
			input->OnRangeValueCommitted.RemoveDynamic(this, &UBaseSliderComboWidget::HandleRangeInputCommitted);
		},
		RangeInput.Get(),
		CompactRangeInputWidget.Get(),
		ModernRangeInputWidget.Get());

	Super::NativeDestruct();
}

void UBaseSliderComboWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetLabelFontSizeOverride(const float inLabelFontSizeOverride)
{
	LabelFontSizeOverride = FMath::Max(inLabelFontSizeOverride, 0.0f);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetLabelMinWidthOverride(const float inLabelMinWidthOverride)
{
	LabelMinWidthOverride = FMath::Max(inLabelMinWidthOverride, 0.0f);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetShowLabel(const bool bInShowLabel)
{
	bShowLabel = bInShowLabel;
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetShowValueField(const bool bInShowValueField)
{
	bShowValueField = bInShowValueField;
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetComboStyle(const EBaseSliderComboStyle inComboStyle)
{
	ComboStyle = inComboStyle;
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetRangeMode(const bool bInRangeMode)
{
	bRangeMode = bInRangeMode;
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetValueRange(const float inMinValue, const float inMaxValue)
{
	MinValue = inMinValue;
	MaxValue = inMaxValue;
	NormalizeMinMax(MinValue, MaxValue);
	SetValue(Value);
	SetRangeValue(LowerValue, UpperValue);
}

void UBaseSliderComboWidget::SetValue(const float inValue)
{
	Value = FMath::Clamp(inValue, MinValue, MaxValue);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetRangeValue(const float inLowerValue, const float inUpperValue)
{
	LowerValue = FMath::Clamp(inLowerValue, MinValue, MaxValue);
	UpperValue = FMath::Clamp(inUpperValue, MinValue, MaxValue);
	NormalizeRange(LowerValue, UpperValue);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetDisplayDecimals(const int32 inDisplayDecimals)
{
	DisplayDecimals = FMath::Clamp(inDisplayDecimals, -1, 6);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetTextInputFontSizeOverride(const float inTextInputFontSizeOverride)
{
	TextInputFontSizeOverride = FMath::Max(inTextInputFontSizeOverride, 0.0f);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetTextInputSizeConstraints(const FBaseWidgetSizeConstraints inTextInputSizeConstraints)
{
	TextInputSizeConstraints = BaseWidgetPrivate::NormalizeSizeConstraints(inTextInputSizeConstraints);
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseSliderComboWidget::HandleSliderValueChanged(UWidget* widget, const float inValue)
{
	(void)widget;
	if (bSynchronizing)
	{
		return;
	}

	Value = FMath::Clamp(inValue, MinValue, MaxValue);
	SynchronizeBaseProperties();
	OnValueChanged.Broadcast(this, Value);
}

void UBaseSliderComboWidget::HandleSliderRangeValueChanged(UWidget* widget, const float inLowerValue, const float inUpperValue)
{
	(void)widget;
	if (bSynchronizing)
	{
		return;
	}

	LowerValue = FMath::Clamp(inLowerValue, MinValue, MaxValue);
	UpperValue = FMath::Clamp(inUpperValue, MinValue, MaxValue);
	NormalizeRange(LowerValue, UpperValue);
	SynchronizeBaseProperties();
	OnRangeValueChanged.Broadcast(this, LowerValue, UpperValue);
}

void UBaseSliderComboWidget::HandleValueInputCommitted(UBaseTextInputWidget* inputWidget, const float inValue)
{
	(void)inputWidget;
	SetValue(inValue);
	OnValueChanged.Broadcast(this, Value);
}

void UBaseSliderComboWidget::HandleRangeInputCommitted(UBaseTextInputWidget* inputWidget, const float inLowerValue, const float inUpperValue)
{
	(void)inputWidget;
	SetRangeValue(inLowerValue, inUpperValue);
	OnRangeValueChanged.Broadcast(this, LowerValue, UpperValue);
}
