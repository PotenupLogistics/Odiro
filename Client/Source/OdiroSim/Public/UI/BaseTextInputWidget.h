#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "Types/SlateEnums.h"
#include "BaseTextInputWidget.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UTextBlock;

// Base-token styled text, number, number-range, and multiline input component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTextInputWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies text, numeric values, state, and error visuals to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the active input mode.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetInputMode(EBaseTextInputMode inMode);

	// Returns the active input mode.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	EBaseTextInputMode GetInputMode() const { return Mode; }

	// Updates the string value used by text and multiline modes.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetText(FText inText);

	// Returns the string value used by text and multiline modes.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	FText GetText() const { return Text; }

	// Returns the current visible edit value, including uncommitted child field text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	FText GetCurrentText() const;

	// Updates the placeholder text used by editable controls.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetPlaceholderText(FText inPlaceholderText);

	// Returns the placeholder text used by editable controls.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	FText GetPlaceholderText() const { return PlaceholderText; }

	// Updates the accepted numeric value range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetValueRange(float inMinValue, float inMaxValue);

	// Updates the current numeric value and clamps it to the accepted range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetNumericValue(float inValue);

	// Returns the current numeric value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	float GetNumericValue() const { return NumericValue; }

	// Updates the current lower and upper range values while preserving lower <= upper.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetRangeValue(float inLowerValue, float inUpperValue);

	// Returns the current lower range value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	float GetLowerValue() const { return LowerValue; }

	// Returns the current upper range value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	float GetUpperValue() const { return UpperValue; }

	// Updates the decimal places used when displaying numeric values (-1 = compact).
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetDisplayDecimals(int32 inDisplayDecimals);

	// Returns the decimal places used when displaying numeric values.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	int32 GetDisplayDecimals() const { return DisplayDecimals; }

	// Updates the number step used by stepper buttons.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetStep(float inStep);

	// Returns the number step used by stepper buttons.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	float GetStep() const { return Step; }

	// Parses and commits text as the current mode's user input.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	bool CommitText(FText inText);

	// Parses and commits the current visible edit value.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	bool CommitCurrentText();

	// Updates the semantic visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the semantic visual state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates the current error text; non-empty text forces error styling.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetErrorText(FText inErrorText);

	// Clears the current error text.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void ClearError();

	// Returns the current error text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	FText GetErrorText() const { return ErrorText; }

	// Updates whether input is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text Input")
	void SetDisabled(bool bInDisabled);

	// Returns whether input is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text Input")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after text or multiline input commits.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Text Input|Events")
	FBaseTextInputTextEvent OnTextCommitted;

	// Broadcasts while text or multiline input changes before commit.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Text Input|Events")
	FBaseTextInputTextChangedEvent OnTextChanged;

	// Broadcasts after number input commits.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Text Input|Events")
	FBaseTextInputNumberEvent OnNumericValueCommitted;

	// Broadcasts after range input commits.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Text Input|Events")
	FBaseTextInputRangeEvent OnRangeValueCommitted;

protected:
	// Feeds rounded input material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Drives the rounded hover highlight from pointer enter/leave events.
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// Drives the rounded focus highlight when this widget or a child field enters
	// or leaves the focus path (fires for the inner editable's focus too).
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	// Binds optional editable child events after WBP construction.
	virtual void NativeConstruct() override;

	// Unbinds optional editable child events before destruction.
	virtual void NativeDestruct() override;

	// Handles single-line text commits from the bound editable text box.
	UFUNCTION()
	void HandleEditableTextCommitted(const FText& committedText, ETextCommit::Type commitMethod);

	// Handles single-line text edits from the bound editable text box.
	UFUNCTION()
	void HandleEditableTextChanged(const FText& changedText);

	// Handles multiline text commits from the bound editable text box.
	UFUNCTION()
	void HandleMultiLineTextCommitted(const FText& committedText, ETextCommit::Type commitMethod);

	// Handles multiline text edits from the bound editable text box.
	UFUNCTION()
	void HandleMultiLineTextChanged(const FText& changedText);

	// Handles lower range field commits.
	UFUNCTION()
	void HandleLowerTextCommitted(const FText& committedText, ETextCommit::Type commitMethod);

	// Handles upper range field commits.
	UFUNCTION()
	void HandleUpperTextCommitted(const FText& committedText, ETextCommit::Type commitMethod);

	// Handles the positive stepper action.
	UFUNCTION()
	void HandleStepUpClicked();

	// Handles the negative stepper action.
	UFUNCTION()
	void HandleStepDownClicked();

	// Input behavior mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetInputMode", Setter = "SetInputMode", BlueprintGetter = "GetInputMode", BlueprintSetter = "SetInputMode", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	EBaseTextInputMode Mode = EBaseTextInputMode::Text;

	// Text value for text-like modes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetText", Setter = "SetText", BlueprintGetter = "GetText", BlueprintSetter = "SetText", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	FText Text;

	// Placeholder text for editable controls.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetPlaceholderText", Setter = "SetPlaceholderText", BlueprintGetter = "GetPlaceholderText", BlueprintSetter = "SetPlaceholderText", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	FText PlaceholderText;

	// Minimum accepted numeric value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	float MinValue = 0.0f;

	// Maximum accepted numeric value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	float MaxValue = 100.0f;

	// Current numeric value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetNumericValue", Setter = "SetNumericValue", BlueprintGetter = "GetNumericValue", BlueprintSetter = "SetNumericValue", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	float NumericValue = 0.0f;

	// Current lower range value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLowerValue", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	float LowerValue = 0.0f;

	// Current upper range value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetUpperValue", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	float UpperValue = 100.0f;

	// Amount applied by the optional stepper buttons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetStep", Setter = "SetStep", BlueprintGetter = "GetStep", BlueprintSetter = "SetStep", Category = "UI|Base Text Input", meta = (ClampMin = "0.0001", ExposeOnSpawn = "true"))
	float Step = 1.0f;

	// Decimal places for displayed numeric values; -1 keeps the compact format.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetDisplayDecimals", Setter = "SetDisplayDecimals", BlueprintGetter = "GetDisplayDecimals", BlueprintSetter = "SetDisplayDecimals", Category = "UI|Base Text Input", meta = (ClampMin = "-1", ClampMax = "6", ExposeOnSpawn = "true"))
	int32 DisplayDecimals = -1;

	// Semantic visual state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Validation message rendered below the input surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetErrorText", Setter = "SetErrorText", BlueprintGetter = "GetErrorText", BlueprintSetter = "SetErrorText", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	FText ErrorText;

	// Error message used when a numeric text commit cannot be parsed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Text Input|Validation", meta = (ExposeOnSpawn = "true"))
	FText InvalidNumberErrorText = NSLOCTEXT("BaseTextInputWidget", "InvalidNumberErrorText", "Invalid number");

	// Error message used when a numeric range text commit cannot be parsed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Text Input|Validation", meta = (ExposeOnSpawn = "true"))
	FText InvalidRangeErrorText = NSLOCTEXT("BaseTextInputWidget", "InvalidRangeErrorText", "Invalid range");

	// Disabled input state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Text Input", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Prevents child event handlers from reacting to property synchronization.
	UPROPERTY(Transient)
	bool bSynchronizing = false;

	// Cached hover state driving the runtime border highlight.
	UPROPERTY(Transient)
	bool bHoverActive = false;

	// Cached keyboard-focus state driving the runtime border highlight.
	UPROPERTY(Transient)
	bool bFocusActive = false;

	// Rounded input surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Single-line and number editable field owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> TextBox;

	// Multiline editable field owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UMultiLineEditableTextBox> MultiLineTextBox;

	// Lower range editable field owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> LowerTextBox;

	// Upper range editable field owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> UpperTextBox;

	// Optional visual separator between range fields.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RangeSeparatorTextBlock;

	// Optional number increment button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StepUpButton;

	// Optional number decrement button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StepDownButton;

	// Optional error text block under the input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorTextBlock;
};
