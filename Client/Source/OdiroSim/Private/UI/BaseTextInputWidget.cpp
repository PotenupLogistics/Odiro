#include "UI/BaseTextInputWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "InputCoreTypes.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

namespace
{
	// Formats a numeric value for display: fixed decimals when requested, else the
	// compact trailing-zero-trimmed default.
	FText FormatNumberText(const float value, const int32 decimals)
	{
		if (decimals < 0)
		{
			return MakeNumberText(value);
		}
		return FText::FromString(FString::Printf(TEXT("%.*f"), decimals, value));
	}

	// Resolves BaseTextInput's editable value font from explicit override or DA typography.
	FSlateFontInfo ResolveInputValueFont(const UBaseWidgetSizeCatalog* sizes, const float fontSizeOverride)
	{
		FSlateFontInfo valueFont;
		if (sizes)
		{
			valueFont = sizes->GetTypography(EBaseTextRole::Body).Font;
		}

		if (fontSizeOverride > 0.0f)
		{
			valueFont.Size = fontSizeOverride;
		}

		return valueFont;
	}

	// Applies resolved DA font metrics while preserving a WBP-authored font face when only size is known.
	FSlateFontInfo ResolveAppliedFont(const FSlateFontInfo& currentFont, const FSlateFontInfo& resolvedFont)
	{
		if (resolvedFont.Size <= 0.0f)
		{
			return currentFont;
		}

		FSlateFontInfo appliedFont = currentFont;
		if (resolvedFont.FontObject || resolvedFont.CompositeFont.IsValid())
		{
			appliedFont = resolvedFont;
		}
		else if (!resolvedFont.TypefaceFontName.IsNone())
		{
			appliedFont.TypefaceFontName = resolvedFont.TypefaceFontName;
		}

		appliedFont.Size = resolvedFont.Size;
		return appliedFont;
	}

	// Applies the resolved value style without replacing the WBP-authored field chrome.
	void ApplyEditableTextBoxValueStyle(
		UEditableTextBox* textBox,
		const FSlateFontInfo& valueFont,
		const FLinearColor& textColor,
		const bool bApplyTextColor,
		const bool bPreserveFocusedText)
	{
		if (!IsValid(textBox) || (valueFont.Size <= 0.0f && !bApplyTextColor))
		{
			return;
		}

		FEditableTextBoxStyle textBoxStyle = textBox->WidgetStyle;
		FTextBlockStyle textStyle = textBoxStyle.TextStyle;
		if (valueFont.Size > 0.0f)
		{
			const FSlateFontInfo font = ResolveAppliedFont(textStyle.Font, valueFont);
			textStyle.SetFont(font);
			textBoxStyle.SetFont(font);
		}
		if (bApplyTextColor)
		{
			const FSlateColor slateTextColor(textColor);
			textStyle.SetColorAndOpacity(slateTextColor);
			textBoxStyle
				.SetForegroundColor(slateTextColor)
				.SetReadOnlyForegroundColor(slateTextColor)
				.SetFocusedForegroundColor(slateTextColor);
		}
		textBoxStyle.SetTextStyle(textStyle);
		textBox->WidgetStyle = textBoxStyle;
		if (!bPreserveFocusedText)
		{
			textBox->SynchronizeProperties();
		}
		if (bApplyTextColor)
		{
			textBox->SetForegroundColor(textColor);
		}
	}

	// Returns true while focused edits should not be overwritten by visual synchronization.
	bool ShouldPreserveFocusedEditableText(const UEditableTextBox* textBox, const bool bMoveCaretToEndOnFocus)
	{
		return !bMoveCaretToEndOnFocus
			&& IsValid(textBox)
			&& textBox->HasKeyboardFocus();
	}

	// Applies the resolved multiline value style without replacing the WBP-authored field chrome.
	void ApplyMultiLineEditableTextBoxValueStyle(
		UMultiLineEditableTextBox* textBox,
		const FSlateFontInfo& valueFont,
		const FLinearColor& textColor,
		const bool bApplyTextColor)
	{
		if (!IsValid(textBox) || (valueFont.Size <= 0.0f && !bApplyTextColor))
		{
			return;
		}

		FEditableTextBoxStyle textBoxStyle = textBox->WidgetStyle;
		FTextBlockStyle textStyle = textBoxStyle.TextStyle;
		if (valueFont.Size > 0.0f)
		{
			const FSlateFontInfo font = ResolveAppliedFont(textStyle.Font, valueFont);
			textStyle.SetFont(font);
			textBoxStyle.SetFont(font);
		}
		if (bApplyTextColor)
		{
			const FSlateColor slateTextColor(textColor);
			textStyle.SetColorAndOpacity(slateTextColor);
			textBoxStyle
				.SetForegroundColor(slateTextColor)
				.SetReadOnlyForegroundColor(slateTextColor)
				.SetFocusedForegroundColor(slateTextColor);
		}
		textBoxStyle.SetTextStyle(textStyle);
		textBox->WidgetStyle = textBoxStyle;
		textBox->SynchronizeProperties();
		if (bApplyTextColor)
		{
			textBox->SetForegroundColor(textColor);
		}
	}

	// Applies the resolved value font so helper labels can keep their semantic color role.
	void ApplyTextBlockValueFont(UTextBlock* textBlock, const FSlateFontInfo& valueFont)
	{
		if (!IsValid(textBlock) || valueFont.Size <= 0.0f)
		{
			return;
		}

		textBlock->SetFont(ResolveAppliedFont(textBlock->GetFont(), valueFont));
	}
}

void UBaseTextInputWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	TGuardValue<bool> synchronizingGuard(bSynchronizing, true);
	const bool bHasError = !ErrorText.IsEmpty() || State == EBaseWidgetState::Error;
	const bool bEnabled = !bDisabled && State != EBaseWidgetState::Disabled;
	const bool bUsesWrappedText = UsesWrappedTextMode();
	const bool bUsesSingleLineText = Mode == EBaseTextInputMode::Text && !bUsesWrappedText;
	const bool bUsesNumber = Mode == EBaseTextInputMode::Number;
	const bool bUsesNumberRange = Mode == EBaseTextInputMode::NumberRange;
	const bool bShowsSingleLineField = bUsesSingleLineText || bUsesNumber;
	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	const FSlateFontInfo inputValueFont = ResolveInputValueFont(sizes, FontSizeOverride);
	const FLinearColor valueTextColor = colors
		? (bEnabled ? colors->GetTextColor(EBaseTextRole::Body) : colors->TextFaintColor)
		: FLinearColor();
	const FLinearColor auxiliaryTextColor = colors
		? (bEnabled ? colors->GetTextColor(EBaseTextRole::Caption) : colors->TextFaintColor)
		: FLinearColor();
	const FLinearColor unitTextColor = colors
		? (bEnabled ? (bOverrideUnitTextColor ? UnitTextColorOverride : colors->TextSecondaryColor) : colors->TextFaintColor)
		: FLinearColor();

	if (TextHorizontalBox)
	{
		SetOptionalWidgetVisible(TextHorizontalBox.Get(), bShowsSingleLineField, ESlateVisibility::SelfHitTestInvisible);
	}

	if (TextBox)
	{
		const bool bPreserveFocusedText = ShouldPreserveFocusedEditableText(TextBox.Get(), bMoveCaretToEndOnFocus);
		if (!bTextBoxDefaultJustificationCaptured)
		{
			TextBoxDefaultJustification = TextBox->GetJustification();
			bTextBoxDefaultJustificationCaptured = true;
		}
		if (!PlaceholderText.IsEmpty())
		{
			TextBox->SetHintText(PlaceholderText);
		}
		TextBox->SetIsReadOnly(!bEnabled);
		TextBox->SetIsEnabled(bEnabled);
		TextBox->SetIsCaretMovedWhenGainFocus(bMoveCaretToEndOnFocus);
		ApplyEditableTextBoxValueStyle(TextBox.Get(), inputValueFont, valueTextColor, colors != nullptr, bPreserveFocusedText);
		TextBox->SetJustification(bUsesNumber
			? ETextJustify::Right
			: TextBoxDefaultJustification.GetValue());
		// While in error, keep the user's raw text so an invalid value stays
		// visible. Reverting it here would let the focus-clear re-commit (fired
		// by ClearKeyboardFocusOnCommit on Enter) parse the reverted valid value
		// and silently clear the warning.
		if (bUsesNumber)
		{
			if (!bHasError && !bPreserveFocusedText)
			{
				TextBox->SetText(FormatNumberText(NumericValue, DisplayDecimals));
			}
		}
		else if (bUsesSingleLineText)
		{
			if (!bPreserveFocusedText)
			{
				TextBox->SetText(Text);
			}
		}
		SetOptionalWidgetVisible(TextBox.Get(), bShowsSingleLineField, ESlateVisibility::Visible);
	}
	if (MultiLineTextBox)
	{
		if (!PlaceholderText.IsEmpty())
		{
			MultiLineTextBox->SetHintText(PlaceholderText);
		}
		MultiLineTextBox->SetIsReadOnly(!bEnabled);
		MultiLineTextBox->SetIsEnabled(bEnabled);
		MultiLineTextBox->SetAutoWrapText(true);
		ApplyMultiLineEditableTextBoxValueStyle(MultiLineTextBox.Get(), inputValueFont, valueTextColor, colors != nullptr);
		MultiLineTextBox->SetText(Text);
		SetOptionalWidgetVisible(MultiLineTextBox.Get(), bUsesWrappedText, ESlateVisibility::Visible);
	}
	if (RangeHorizontalBox)
	{
		SetOptionalWidgetVisible(RangeHorizontalBox.Get(), bUsesNumberRange, ESlateVisibility::SelfHitTestInvisible);
	}
	if (LowerTextBox)
	{
		const bool bPreserveFocusedText = ShouldPreserveFocusedEditableText(LowerTextBox.Get(), bMoveCaretToEndOnFocus);
		LowerTextBox->SetIsReadOnly(!bEnabled);
		LowerTextBox->SetIsEnabled(bEnabled);
		LowerTextBox->SetIsCaretMovedWhenGainFocus(bMoveCaretToEndOnFocus);
		ApplyEditableTextBoxValueStyle(LowerTextBox.Get(), inputValueFont, valueTextColor, colors != nullptr, bPreserveFocusedText);
		if (!bHasError && !bPreserveFocusedText)
		{
			LowerTextBox->SetText(FormatNumberText(LowerValue, DisplayDecimals));
		}
		SetOptionalWidgetVisible(LowerTextBox.Get(), bUsesNumberRange, ESlateVisibility::Visible);
	}
	if (UpperTextBox)
	{
		const bool bPreserveFocusedText = ShouldPreserveFocusedEditableText(UpperTextBox.Get(), bMoveCaretToEndOnFocus);
		UpperTextBox->SetIsReadOnly(!bEnabled);
		UpperTextBox->SetIsEnabled(bEnabled);
		UpperTextBox->SetIsCaretMovedWhenGainFocus(bMoveCaretToEndOnFocus);
		ApplyEditableTextBoxValueStyle(UpperTextBox.Get(), inputValueFont, valueTextColor, colors != nullptr, bPreserveFocusedText);
		if (!bHasError && !bPreserveFocusedText)
		{
			UpperTextBox->SetText(FormatNumberText(UpperValue, DisplayDecimals));
		}
		SetOptionalWidgetVisible(UpperTextBox.Get(), bUsesNumberRange, ESlateVisibility::Visible);
	}
	if (RangeSeparatorTextBlock)
	{
		ApplyTextStyle(RangeSeparatorTextBlock.Get(), EBaseTextRole::Caption);
		ApplyTextBlockValueFont(RangeSeparatorTextBlock.Get(), inputValueFont);
		if (colors)
		{
			ApplyTextColor(RangeSeparatorTextBlock.Get(), auxiliaryTextColor);
		}
		SetOptionalWidgetVisible(RangeSeparatorTextBlock.Get(), bUsesNumberRange);
	}
	if (UnitTextBlock)
	{
		SetTextBlockValue(UnitTextBlock.Get(), UnitText, false);
		ApplyTextStyle(UnitTextBlock.Get(), EBaseTextRole::Caption);
		ApplyTextBlockValueFont(UnitTextBlock.Get(), inputValueFont);
		if (colors)
		{
			ApplyTextColor(UnitTextBlock.Get(), unitTextColor);
		}
		SetOptionalWidgetVisible(UnitTextBlock.Get(), bUsesNumber && !UnitText.IsEmpty());
	}
	if (StepperColumn)
	{
		SetOptionalWidgetVisible(StepperColumn.Get(), bUsesNumber, ESlateVisibility::Visible);
	}
	if (StepUpButton)
	{
		StepUpButton->SetDisabled(!bEnabled);
		SetOptionalWidgetVisible(StepUpButton.Get(), bUsesNumber, ESlateVisibility::Visible);
	}
	if (StepDownButton)
	{
		StepDownButton->SetDisabled(!bEnabled);
		SetOptionalWidgetVisible(StepDownButton.Get(), bUsesNumber, ESlateVisibility::Visible);
	}
	ApplyStepperIconOpacity(bEnabled);
	if (ErrorTextBlock)
	{
		SetTextBlockValue(ErrorTextBlock.Get(), ErrorText);
		ApplyTextStyle(ErrorTextBlock.Get(), EBaseTextRole::Caption);
		if (colors)
		{
			ApplyTextColor(ErrorTextBlock.Get(), colors->StatusDangerColor);
		}
	}

	if (SurfaceBorder)
	{
		SurfaceBorder->SetVerticalAlignment(bUsesWrappedText ? VAlign_Top : VAlign_Center);
	}
	if (colors && sizes)
	{
		FLinearColor fillColor = colors->SurfaceWellColor;
		FLinearColor strokeColor = colors->LineFieldColor;
		if (!bEnabled)
		{
			fillColor = colors->SurfaceChromeColor;
			strokeColor = colors->LineSubtleColor;
		}
		else if (bHasError)
		{
			strokeColor = colors->StatusDangerColor;
		}
		else if (bFocusActive || State == EBaseWidgetState::Selected)
		{
			strokeColor = colors->AccentFocusColor;
		}
		else if (bHoverActive || State == EBaseWidgetState::Hovered)
		{
			strokeColor = colors->LineFieldHoverColor;
		}

		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			sizes->Radius,
			sizes->BorderWidth);
	}
}

int32 UBaseTextInputWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseTextInputWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHoverActive = true;
	bStepperHoverActive = IsStepperInteractionHovered();
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(EBaseWidgetState::Hovered);
}

FReply UBaseTextInputWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply reply = Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	SetStepperHoverActive(IsStepperInteractionHovered());
	return reply;
}

FReply UBaseTextInputWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ShouldFocusTextBoxFromUnitClick(InMouseEvent))
	{
		return FReply::Handled().SetUserFocus(TextBox->TakeWidget(), EFocusCause::Mouse);
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UBaseTextInputWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHoverActive = false;
	bStepperHoverActive = false;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(State);
}

void UBaseTextInputWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	bFocusActive = true;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(EBaseWidgetState::Selected);
}

void UBaseTextInputWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bFocusActive = false;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(State);
}

void UBaseTextInputWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Hit-testable so pointer enter/leave reach this widget for the hover highlight.
	SetVisibility(ESlateVisibility::Visible);

	if (TextBox)
	{
		TextBox->OnTextChanged.RemoveDynamic(this, &UBaseTextInputWidget::HandleEditableTextChanged);
		TextBox->OnTextChanged.AddDynamic(this, &UBaseTextInputWidget::HandleEditableTextChanged);
		TextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleEditableTextCommitted);
		TextBox->OnTextCommitted.AddDynamic(this, &UBaseTextInputWidget::HandleEditableTextCommitted);
	}
	if (MultiLineTextBox)
	{
		MultiLineTextBox->OnTextChanged.RemoveDynamic(this, &UBaseTextInputWidget::HandleMultiLineTextChanged);
		MultiLineTextBox->OnTextChanged.AddDynamic(this, &UBaseTextInputWidget::HandleMultiLineTextChanged);
		MultiLineTextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleMultiLineTextCommitted);
		MultiLineTextBox->OnTextCommitted.AddDynamic(this, &UBaseTextInputWidget::HandleMultiLineTextCommitted);
	}
	if (LowerTextBox)
	{
		LowerTextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleLowerTextCommitted);
		LowerTextBox->OnTextCommitted.AddDynamic(this, &UBaseTextInputWidget::HandleLowerTextCommitted);
	}
	if (UpperTextBox)
	{
		UpperTextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleUpperTextCommitted);
		UpperTextBox->OnTextCommitted.AddDynamic(this, &UBaseTextInputWidget::HandleUpperTextCommitted);
	}
	if (StepUpButton)
	{
		StepUpButton->OnBaseClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepUpClicked);
		StepUpButton->OnBaseClicked.AddDynamic(this, &UBaseTextInputWidget::HandleStepUpClicked);
		StepUpButton->OnBaseHovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperHovered);
		StepUpButton->OnBaseHovered.AddDynamic(this, &UBaseTextInputWidget::HandleStepperHovered);
		StepUpButton->OnBaseUnhovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperUnhovered);
		StepUpButton->OnBaseUnhovered.AddDynamic(this, &UBaseTextInputWidget::HandleStepperUnhovered);
	}
	if (StepDownButton)
	{
		StepDownButton->OnBaseClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepDownClicked);
		StepDownButton->OnBaseClicked.AddDynamic(this, &UBaseTextInputWidget::HandleStepDownClicked);
		StepDownButton->OnBaseHovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperHovered);
		StepDownButton->OnBaseHovered.AddDynamic(this, &UBaseTextInputWidget::HandleStepperHovered);
		StepDownButton->OnBaseUnhovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperUnhovered);
		StepDownButton->OnBaseUnhovered.AddDynamic(this, &UBaseTextInputWidget::HandleStepperUnhovered);
	}
}

void UBaseTextInputWidget::NativeDestruct()
{
	if (TextBox)
	{
		TextBox->OnTextChanged.RemoveDynamic(this, &UBaseTextInputWidget::HandleEditableTextChanged);
		TextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleEditableTextCommitted);
	}
	if (MultiLineTextBox)
	{
		MultiLineTextBox->OnTextChanged.RemoveDynamic(this, &UBaseTextInputWidget::HandleMultiLineTextChanged);
		MultiLineTextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleMultiLineTextCommitted);
	}
	if (LowerTextBox)
	{
		LowerTextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleLowerTextCommitted);
	}
	if (UpperTextBox)
	{
		UpperTextBox->OnTextCommitted.RemoveDynamic(this, &UBaseTextInputWidget::HandleUpperTextCommitted);
	}
	if (StepUpButton)
	{
		StepUpButton->OnBaseClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepUpClicked);
		StepUpButton->OnBaseHovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperHovered);
		StepUpButton->OnBaseUnhovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperUnhovered);
	}
	if (StepDownButton)
	{
		StepDownButton->OnBaseClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepDownClicked);
		StepDownButton->OnBaseHovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperHovered);
		StepDownButton->OnBaseUnhovered.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepperUnhovered);
	}

	Super::NativeDestruct();
}

void UBaseTextInputWidget::SetInputMode(const EBaseTextInputMode inMode)
{
	if (inMode == EBaseTextInputMode::Multiline)
	{
		Mode = EBaseTextInputMode::Text;
		bTextWrap = true;
	}
	else
	{
		Mode = inMode;
	}
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetTextWrap(const bool bInTextWrap)
{
	bTextWrap = bInTextWrap;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetMoveCaretToEndOnFocus(const bool bInMoveCaretToEndOnFocus)
{
	bMoveCaretToEndOnFocus = bInMoveCaretToEndOnFocus;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetText(const FText inText)
{
	Text = inText;
	SynchronizeBaseProperties();
}

FText UBaseTextInputWidget::GetCurrentText() const
{
	if (Mode == EBaseTextInputMode::Number)
	{
		return TextBox ? TextBox->GetText() : FormatNumberText(NumericValue, DisplayDecimals);
	}

	if (Mode == EBaseTextInputMode::NumberRange)
	{
		const FString lowerText = LowerTextBox
			? LowerTextBox->GetText().ToString()
			: FormatNumberText(LowerValue, DisplayDecimals).ToString();
		const FString upperText = UpperTextBox
			? UpperTextBox->GetText().ToString()
			: FormatNumberText(UpperValue, DisplayDecimals).ToString();
		return FText::FromString(FString::Printf(TEXT("%s - %s"), *lowerText, *upperText));
	}

	if (UsesWrappedTextMode())
	{
		return MultiLineTextBox ? MultiLineTextBox->GetText() : Text;
	}

	return TextBox ? TextBox->GetText() : Text;
}

void UBaseTextInputWidget::SetPlaceholderText(const FText inPlaceholderText)
{
	PlaceholderText = inPlaceholderText;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetUnitText(const FText inUnitText)
{
	UnitText = inUnitText;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetUnitTextColorOverride(const FLinearColor inUnitTextColorOverride)
{
	UnitTextColorOverride = inUnitTextColorOverride;
	bOverrideUnitTextColor = true;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetOverrideUnitTextColor(const bool bInOverrideUnitTextColor)
{
	bOverrideUnitTextColor = bInOverrideUnitTextColor;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::ClearUnitTextColorOverride()
{
	bOverrideUnitTextColor = false;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetFontSizeOverride(const float inFontSizeOverride)
{
	FontSizeOverride = FMath::Max(inFontSizeOverride, 0.0f);
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetValueRange(const float inMinValue, const float inMaxValue)
{
	MinValue = inMinValue;
	MaxValue = inMaxValue;
	NormalizeMinMax(MinValue, MaxValue);
	SetNumericValue(NumericValue);
	SetRangeValue(LowerValue, UpperValue);
}

void UBaseTextInputWidget::SetNumericValue(const float inValue)
{
	NumericValue = FMath::Clamp(inValue, MinValue, MaxValue);
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetRangeValue(const float inLowerValue, const float inUpperValue)
{
	LowerValue = FMath::Clamp(inLowerValue, MinValue, MaxValue);
	UpperValue = FMath::Clamp(inUpperValue, MinValue, MaxValue);
	NormalizeRange(LowerValue, UpperValue);
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetStep(const float inStep)
{
	Step = FMath::Max(inStep, UE_SMALL_NUMBER);
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetDisplayDecimals(const int32 inDisplayDecimals)
{
	DisplayDecimals = FMath::Clamp(inDisplayDecimals, -1, 6);
	SynchronizeBaseProperties();
}

bool UBaseTextInputWidget::CommitText(const FText inText)
{
	if (bDisabled || State == EBaseWidgetState::Disabled)
	{
		return false;
	}

	if (Mode == EBaseTextInputMode::Number)
	{
		float parsedValue = 0.0f;
		if (!TryParseNumber(inText, parsedValue))
		{
			SetErrorText(InvalidNumberErrorText);
			return false;
		}
		ClearError();
		SetNumericValue(parsedValue);
		OnNumericValueCommitted.Broadcast(this, NumericValue);
		return true;
	}
	if (Mode == EBaseTextInputMode::NumberRange)
	{
		float parsedLower = 0.0f;
		float parsedUpper = 0.0f;
		if (!TryParseRange(inText, parsedLower, parsedUpper))
		{
			SetErrorText(InvalidRangeErrorText);
			return false;
		}
		ClearError();
		SetRangeValue(parsedLower, parsedUpper);
		OnRangeValueCommitted.Broadcast(this, LowerValue, UpperValue);
		return true;
	}

	ClearError();
	SetText(inText);
	OnTextCommitted.Broadcast(this, Text);
	return true;
}

bool UBaseTextInputWidget::CommitCurrentText()
{
	return CommitText(GetCurrentText());
}

void UBaseTextInputWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(State);
}

void UBaseTextInputWidget::SetErrorText(const FText inErrorText)
{
	ErrorText = inErrorText;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(ErrorText.IsEmpty() ? State : EBaseWidgetState::Error);
}

void UBaseTextInputWidget::ClearError()
{
	ErrorText = FText::GetEmpty();
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(State);
}

void UBaseTextInputWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : State);
}

void UBaseTextInputWidget::HandleEditableTextCommitted(const FText& committedText, ETextCommit::Type commitMethod)
{
	if (!bSynchronizing)
	{
		const bool bCommitted = CommitText(committedText);
		if (bCommitted && commitMethod == ETextCommit::OnEnter && Mode == EBaseTextInputMode::Text && !UsesWrappedTextMode())
		{
			OnTextSubmitted.Broadcast(this, Text);
		}
	}
}

void UBaseTextInputWidget::HandleEditableTextChanged(const FText& changedText)
{
	if (!bSynchronizing && Mode == EBaseTextInputMode::Text && !UsesWrappedTextMode())
	{
		OnTextChanged.Broadcast(this, changedText);
	}
}

void UBaseTextInputWidget::HandleMultiLineTextCommitted(const FText& committedText, ETextCommit::Type commitMethod)
{
	if (!bSynchronizing)
	{
		const bool bCommitted = CommitText(committedText);
		if (bCommitted && commitMethod == ETextCommit::OnEnter && UsesWrappedTextMode())
		{
			OnTextSubmitted.Broadcast(this, Text);
		}
	}
}

void UBaseTextInputWidget::HandleMultiLineTextChanged(const FText& changedText)
{
	if (!bSynchronizing && UsesWrappedTextMode())
	{
		OnTextChanged.Broadcast(this, changedText);
	}
}

bool UBaseTextInputWidget::UsesWrappedTextMode() const
{
	return Mode == EBaseTextInputMode::Multiline
		|| (Mode == EBaseTextInputMode::Text && bTextWrap);
}

void UBaseTextInputWidget::HandleLowerTextCommitted(const FText& committedText, ETextCommit::Type commitMethod)
{
	(void)commitMethod;
	if (bSynchronizing)
	{
		return;
	}

	float parsedValue = 0.0f;
	if (!TryParseNumber(committedText, parsedValue))
	{
		SetErrorText(InvalidNumberErrorText);
		return;
	}

	ClearError();
	SetRangeValue(parsedValue, UpperValue);
	OnRangeValueCommitted.Broadcast(this, LowerValue, UpperValue);
}

void UBaseTextInputWidget::HandleUpperTextCommitted(const FText& committedText, ETextCommit::Type commitMethod)
{
	(void)commitMethod;
	if (bSynchronizing)
	{
		return;
	}

	float parsedValue = 0.0f;
	if (!TryParseNumber(committedText, parsedValue))
	{
		SetErrorText(InvalidNumberErrorText);
		return;
	}

	ClearError();
	SetRangeValue(LowerValue, parsedValue);
	OnRangeValueCommitted.Broadcast(this, LowerValue, UpperValue);
}

void UBaseTextInputWidget::HandleStepUpClicked(UBaseButtonWidget* button)
{
	(void)button;
	if (bDisabled || State == EBaseWidgetState::Disabled)
	{
		return;
	}

	SetNumericValue(NumericValue + Step);
	OnNumericValueCommitted.Broadcast(this, NumericValue);
}

void UBaseTextInputWidget::HandleStepDownClicked(UBaseButtonWidget* button)
{
	(void)button;
	if (bDisabled || State == EBaseWidgetState::Disabled)
	{
		return;
	}

	SetNumericValue(NumericValue - Step);
	OnNumericValueCommitted.Broadcast(this, NumericValue);
}

void UBaseTextInputWidget::HandleStepperHovered(UBaseButtonWidget* button)
{
	(void)button;
	SetStepperHoverActive(true);
}

void UBaseTextInputWidget::HandleStepperUnhovered(UBaseButtonWidget* button)
{
	(void)button;
	SetStepperHoverActive(IsStepperInteractionHovered());
}

bool UBaseTextInputWidget::IsStepperInteractionHovered() const
{
	if (Mode != EBaseTextInputMode::Number)
	{
		return false;
	}

	return (StepperColumn && StepperColumn->IsHovered())
		|| (StepUpButton && StepUpButton->IsHovered())
		|| (StepDownButton && StepDownButton->IsHovered());
}

bool UBaseTextInputWidget::ShouldFocusTextBoxFromUnitClick(const FPointerEvent& InMouseEvent) const
{
	if (Mode != EBaseTextInputMode::Number
		|| bDisabled
		|| State == EBaseWidgetState::Disabled
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
		|| UnitText.IsEmpty()
		|| !TextBox
		|| !UnitTextBlock
		|| !TextBox->GetIsEnabled()
		|| !TextBox->IsVisible()
		|| !UnitTextBlock->IsVisible())
	{
		return false;
	}

	return UnitTextBlock->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
}

void UBaseTextInputWidget::SetStepperHoverActive(const bool bInStepperHoverActive)
{
	if (bStepperHoverActive == bInStepperHoverActive)
	{
		return;
	}

	bStepperHoverActive = bInStepperHoverActive;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::ApplyStepperIconOpacity(const bool bEnabled)
{
	const bool bShowsStepper = Mode == EBaseTextInputMode::Number;
	const float idleOpacity = FMath::Clamp(StepperIconIdleOpacity, 0.0f, 1.0f);
	const float hoveredOpacity = FMath::Clamp(StepperIconHoveredOpacity, 0.0f, 1.0f);
	const float iconOpacity = bShowsStepper
		? (bEnabled && bStepperHoverActive ? hoveredOpacity : idleOpacity)
		: hoveredOpacity;
	// Stepper buttons are icon-only in WBP_BaseTextInput, so their render opacity
	// acts as the icon emphasis without adding a button-specific color override.
	if (StepUpButton)
	{
		StepUpButton->SetRenderOpacity(iconOpacity);
	}
	if (StepDownButton)
	{
		StepDownButton->SetRenderOpacity(iconOpacity);
	}
}
