#include "UI/BaseTextInputWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
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
}

void UBaseTextInputWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	TGuardValue<bool> synchronizingGuard(bSynchronizing, true);
	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const bool bHasError = !ErrorText.IsEmpty() || State == EBaseWidgetState::Error;
	const bool bEnabled = !bDisabled && State != EBaseWidgetState::Disabled;

	if (TextBox)
	{
		TextBox->SetHintText(PlaceholderText);
		TextBox->SetIsReadOnly(!bEnabled);
		TextBox->SetIsEnabled(bEnabled);
		// While in error, keep the user's raw text so an invalid value stays
		// visible. Reverting it here would let the focus-clear re-commit (fired
		// by ClearKeyboardFocusOnCommit on Enter) parse the reverted valid value
		// and silently clear the warning.
		if (Mode == EBaseTextInputMode::Number)
		{
			if (!bHasError)
			{
				TextBox->SetText(FormatNumberText(NumericValue, DisplayDecimals));
			}
		}
		else if (Mode == EBaseTextInputMode::Text)
		{
			TextBox->SetText(Text);
		}
		SetOptionalWidgetVisible(
			TextBox.Get(),
			Mode == EBaseTextInputMode::Text || Mode == EBaseTextInputMode::Number,
			ESlateVisibility::Visible);
	}
	if (MultiLineTextBox)
	{
		MultiLineTextBox->SetHintText(PlaceholderText);
		MultiLineTextBox->SetIsReadOnly(!bEnabled);
		MultiLineTextBox->SetIsEnabled(bEnabled);
		MultiLineTextBox->SetText(Text);
		SetOptionalWidgetVisible(MultiLineTextBox.Get(), Mode == EBaseTextInputMode::Multiline, ESlateVisibility::Visible);
	}
	if (LowerTextBox)
	{
		LowerTextBox->SetIsReadOnly(!bEnabled);
		LowerTextBox->SetIsEnabled(bEnabled);
		if (!bHasError)
		{
			LowerTextBox->SetText(FormatNumberText(LowerValue, DisplayDecimals));
		}
		SetOptionalWidgetVisible(LowerTextBox.Get(), Mode == EBaseTextInputMode::NumberRange, ESlateVisibility::Visible);
	}
	if (UpperTextBox)
	{
		UpperTextBox->SetIsReadOnly(!bEnabled);
		UpperTextBox->SetIsEnabled(bEnabled);
		if (!bHasError)
		{
			UpperTextBox->SetText(FormatNumberText(UpperValue, DisplayDecimals));
		}
		SetOptionalWidgetVisible(UpperTextBox.Get(), Mode == EBaseTextInputMode::NumberRange, ESlateVisibility::Visible);
	}
	if (RangeSeparatorTextBlock)
	{
		SetOptionalWidgetVisible(RangeSeparatorTextBlock.Get(), Mode == EBaseTextInputMode::NumberRange);
	}
	if (StepUpButton)
	{
		StepUpButton->SetIsEnabled(bEnabled);
		SetOptionalWidgetVisible(StepUpButton.Get(), Mode == EBaseTextInputMode::Number, ESlateVisibility::Visible);
	}
	if (StepDownButton)
	{
		StepDownButton->SetIsEnabled(bEnabled);
		SetOptionalWidgetVisible(StepDownButton.Get(), Mode == EBaseTextInputMode::Number, ESlateVisibility::Visible);
	}
	if (ErrorTextBlock)
	{
		SetTextBlockValue(ErrorTextBlock.Get(), ErrorText);
		ApplyTextStyle(ErrorTextBlock.Get(), EBaseTextRole::Caption);
		if (tokens)
		{
			ApplyTextColor(ErrorTextBlock.Get(), tokens->StatusDangerColor);
		}
	}

	if (tokens)
	{
		FLinearColor fillColor = tokens->SurfaceWellColor;
		FLinearColor strokeColor = tokens->LineFieldColor;
		if (!bEnabled)
		{
			fillColor = tokens->SurfaceChromeColor;
			strokeColor = tokens->LineSubtleColor;
		}
		else if (bHasError)
		{
			strokeColor = tokens->StatusDangerColor;
		}
		else if (bFocusActive || State == EBaseWidgetState::Selected)
		{
			strokeColor = tokens->AccentFocusColor;
		}
		else if (bHoverActive || State == EBaseWidgetState::Hovered)
		{
			strokeColor = tokens->LineFieldHoverColor;
		}

		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			tokens->Radius,
			tokens->BorderWidth);
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
	if (!bHoverActive)
	{
		bHoverActive = true;
		SynchronizeBaseProperties();
	}
}

void UBaseTextInputWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (bHoverActive)
	{
		bHoverActive = false;
		SynchronizeBaseProperties();
	}
}

void UBaseTextInputWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	if (!bFocusActive)
	{
		bFocusActive = true;
		SynchronizeBaseProperties();
	}
}

void UBaseTextInputWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	if (bFocusActive)
	{
		bFocusActive = false;
		SynchronizeBaseProperties();
	}
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
		StepUpButton->OnClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepUpClicked);
		StepUpButton->OnClicked.AddDynamic(this, &UBaseTextInputWidget::HandleStepUpClicked);
	}
	if (StepDownButton)
	{
		StepDownButton->OnClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepDownClicked);
		StepDownButton->OnClicked.AddDynamic(this, &UBaseTextInputWidget::HandleStepDownClicked);
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
		StepUpButton->OnClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepUpClicked);
	}
	if (StepDownButton)
	{
		StepDownButton->OnClicked.RemoveDynamic(this, &UBaseTextInputWidget::HandleStepDownClicked);
	}

	Super::NativeDestruct();
}

void UBaseTextInputWidget::SetInputMode(const EBaseTextInputMode inMode)
{
	Mode = inMode;
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

	if (Mode == EBaseTextInputMode::Multiline)
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
}

void UBaseTextInputWidget::SetErrorText(const FText inErrorText)
{
	ErrorText = inErrorText;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::ClearError()
{
	ErrorText = FText::GetEmpty();
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseTextInputWidget::HandleEditableTextCommitted(const FText& committedText, ETextCommit::Type commitMethod)
{
	(void)commitMethod;
	if (!bSynchronizing)
	{
		CommitText(committedText);
	}
}

void UBaseTextInputWidget::HandleEditableTextChanged(const FText& changedText)
{
	if (!bSynchronizing && Mode == EBaseTextInputMode::Text)
	{
		OnTextChanged.Broadcast(this, changedText);
	}
}

void UBaseTextInputWidget::HandleMultiLineTextCommitted(const FText& committedText, ETextCommit::Type commitMethod)
{
	(void)commitMethod;
	if (!bSynchronizing)
	{
		CommitText(committedText);
	}
}

void UBaseTextInputWidget::HandleMultiLineTextChanged(const FText& changedText)
{
	if (!bSynchronizing && Mode == EBaseTextInputMode::Multiline)
	{
		OnTextChanged.Broadcast(this, changedText);
	}
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

void UBaseTextInputWidget::HandleStepUpClicked()
{
	if (bDisabled || State == EBaseWidgetState::Disabled)
	{
		return;
	}

	SetNumericValue(NumericValue + Step);
	OnNumericValueCommitted.Broadcast(this, NumericValue);
}

void UBaseTextInputWidget::HandleStepDownClicked()
{
	if (bDisabled || State == EBaseWidgetState::Disabled)
	{
		return;
	}

	SetNumericValue(NumericValue - Step);
	OnNumericValueCommitted.Broadcast(this, NumericValue);
}
