#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseSliderComboWidget.generated.h"

class UBaseSliderWidget;
class UBaseTextInputWidget;
class UTextBlock;
class UWidget;

// Label, slider, and numeric input composition for form rows.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseSliderComboWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies composition style, label, slider, and input state to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the combo label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetLabel(FText inLabel);

	// Returns the combo label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	FText GetLabel() const { return Label; }

	// Updates the optional label font-size override; 0 keeps the resolved base label style.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetLabelFontSizeOverride(float inLabelFontSizeOverride);

	// Returns the optional label font-size override.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	float GetLabelFontSizeOverride() const { return LabelFontSizeOverride; }

	// Updates the optional label minimum width; 0 keeps the WBP-authored label width.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetLabelMinWidthOverride(float inLabelMinWidthOverride);

	// Returns the optional label minimum width.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	float GetLabelMinWidthOverride() const { return LabelMinWidthOverride; }

	// Updates whether the label should be visible.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetShowLabel(bool bInShowLabel);

	// Returns whether the label should be visible.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	bool ShouldShowLabel() const { return bShowLabel; }

	// Updates whether the numeric input field should be visible.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetShowValueField(bool bInShowValueField);

	// Returns whether the numeric input field should be visible.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	bool ShouldShowValueField() const { return bShowValueField; }

	// Updates the visual composition style.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetComboStyle(EBaseSliderComboStyle inComboStyle);

	// Returns the visual composition style.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	EBaseSliderComboStyle GetComboStyle() const { return ComboStyle; }

	// Updates whether this combo uses lower and upper slider handles.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetRangeMode(bool bInRangeMode);

	// Returns whether this combo uses lower and upper slider handles.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	bool IsRangeMode() const { return bRangeMode; }

	// Updates the accepted value range.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetValueRange(float inMinValue, float inMaxValue);

	// Updates the current single slider value.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetValue(float inValue);

	// Returns the current single slider value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	float GetValue() const { return Value; }

	// Updates the current range slider values while preserving lower <= upper.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetRangeValue(float inLowerValue, float inUpperValue);

	// Returns the current lower range value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	float GetLowerValue() const { return LowerValue; }

	// Returns the current upper range value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	float GetUpperValue() const { return UpperValue; }

	// Updates the decimal places used by numeric input fields.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetDisplayDecimals(int32 inDisplayDecimals);

	// Returns the decimal places used by numeric input fields.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	int32 GetDisplayDecimals() const { return DisplayDecimals; }

	// Updates the optional child TextInput font-size override; 0 keeps the child TextInput default.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetTextInputFontSizeOverride(float inTextInputFontSizeOverride);

	// Returns the optional child TextInput font-size override.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	float GetTextInputFontSizeOverride() const { return TextInputFontSizeOverride; }

	// Updates optional child TextInput size constraints; zero fields keep the WBP-authored input size.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetTextInputSizeConstraints(FBaseWidgetSizeConstraints inTextInputSizeConstraints);

	// Returns optional child TextInput size constraints.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	FBaseWidgetSizeConstraints GetTextInputSizeConstraints() const { return TextInputSizeConstraints; }

	// Updates whether slider and input are disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Slider Combo")
	void SetDisabled(bool bInDisabled);

	// Returns whether slider and input are disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Slider Combo")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after the single slider value changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseSliderValueEvent OnValueChanged;

	// Broadcasts after the range slider values change.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseSliderRangeEvent OnRangeValueChanged;

protected:
	// Binds optional slider and field child events after WBP construction.
	virtual void NativeConstruct() override;

	// Unbinds optional slider and field child events before destruction.
	virtual void NativeDestruct() override;

	// Handles child slider single-value changes.
	UFUNCTION()
	void HandleSliderValueChanged(UWidget* widget, float inValue);

	// Handles child slider range changes.
	UFUNCTION()
	void HandleSliderRangeValueChanged(UWidget* widget, float inLowerValue, float inUpperValue);

	// Handles commits from visible single-value fields.
	UFUNCTION()
	void HandleValueInputCommitted(UBaseTextInputWidget* inputWidget, float inValue);

	// Handles commits from visible range fields.
	UFUNCTION()
	void HandleRangeInputCommitted(UBaseTextInputWidget* inputWidget, float inLowerValue, float inUpperValue);

	// Configures one slider child from current combo state.
	void SynchronizeSlider(UBaseSliderWidget* slider);

	// Configures one single-value input child from current combo state.
	void SynchronizeValueInput(UBaseTextInputWidget* input);

	// Configures one range input child from current combo state.
	void SynchronizeRangeInput(UBaseTextInputWidget* input);

	// Refreshes optional child references that intentionally keep their WBP-authored widget names.
	void RefreshOptionalBindings();

	// Label shown by the combo when enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Label = NSLOCTEXT("BaseSliderComboWidget", "DefaultLabel", "Amount");

	// Optional label font size in Slate units; 0 keeps the resolved base label style.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabelFontSizeOverride", Setter = "SetLabelFontSizeOverride", BlueprintGetter = "GetLabelFontSizeOverride", BlueprintSetter = "SetLabelFontSizeOverride", Category = "UI|Style", meta = (DisplayName = "Label Font Size Override", ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float LabelFontSizeOverride = 0.0f;

	// Optional label minimum desired width in Slate units; 0 keeps the WBP-authored label width.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabelMinWidthOverride", Setter = "SetLabelMinWidthOverride", BlueprintGetter = "GetLabelMinWidthOverride", BlueprintSetter = "SetLabelMinWidthOverride", Category = "UI|Layout", meta = (DisplayName = "Label Min Width Override", ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float LabelMinWidthOverride = 0.0f;

	// Whether label text should be rendered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "ShouldShowLabel", Setter = "SetShowLabel", BlueprintGetter = "ShouldShowLabel", BlueprintSetter = "SetShowLabel", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	bool bShowLabel = true;

	// Whether a numeric input should be rendered next to the slider.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "ShouldShowValueField", Setter = "SetShowValueField", BlueprintGetter = "ShouldShowValueField", BlueprintSetter = "SetShowValueField", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	bool bShowValueField = true;

	// Visual arrangement for label, slider, and input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetComboStyle", Setter = "SetComboStyle", BlueprintGetter = "GetComboStyle", BlueprintSetter = "SetComboStyle", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	EBaseSliderComboStyle ComboStyle = EBaseSliderComboStyle::Compact;

	// Whether lower and upper handles are active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsRangeMode", Setter = "SetRangeMode", BlueprintGetter = "IsRangeMode", BlueprintSetter = "SetRangeMode", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bRangeMode = false;

	// Minimum accepted slider value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Range", meta = (ExposeOnSpawn = "true"))
	float MinValue = 0.0f;

	// Maximum accepted slider value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Range", meta = (ExposeOnSpawn = "true"))
	float MaxValue = 100.0f;

	// Current single slider value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValue", Setter = "SetValue", BlueprintGetter = "GetValue", BlueprintSetter = "SetValue", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	float Value = 50.0f;

	// Current lower range value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLowerValue", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	float LowerValue = 25.0f;

	// Current upper range value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetUpperValue", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	float UpperValue = 75.0f;

	// Decimal places for displayed numeric values; -1 keeps the compact default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetDisplayDecimals", Setter = "SetDisplayDecimals", BlueprintGetter = "GetDisplayDecimals", BlueprintSetter = "SetDisplayDecimals", Category = "UI|Contents", meta = (ClampMin = "-1", ClampMax = "6", ExposeOnSpawn = "true"))
	int32 DisplayDecimals = -1;

	// Optional TextInput value font size in Slate units; 0 keeps the child TextInput default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTextInputFontSizeOverride", Setter = "SetTextInputFontSizeOverride", BlueprintGetter = "GetTextInputFontSizeOverride", BlueprintSetter = "SetTextInputFontSizeOverride", Category = "UI|Style", meta = (DisplayName = "Text Input Font Size Override", ClampMin = "0.0", UIMin = "0.0", ExposeOnSpawn = "true"))
	float TextInputFontSizeOverride = 0.0f;

	// Optional desired-size constraints forwarded to the child BaseTextInput widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTextInputSizeConstraints", Setter = "SetTextInputSizeConstraints", BlueprintGetter = "GetTextInputSizeConstraints", BlueprintSetter = "SetTextInputSizeConstraints", Category = "UI|Layout", meta = (DisplayName = "Text Input Size Constraints", ExposeOnSpawn = "true"))
	FBaseWidgetSizeConstraints TextInputSizeConstraints;

	// Disabled combo state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Prevents child events from echoing synchronization writes.
	UPROPERTY(Transient)
	bool bSynchronizing = false;

	// Optional compact one-line root owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CompactRoot;

	// Optional modern two-line root owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ModernRoot;

	// Generic label text owned by simple Widget Blueprints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Compact label text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompactLabelTextBlock;

	// Modern label text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModernLabelTextBlock;

	// Generic slider child owned by simple Widget Blueprints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> Slider;

	// Compact slider child owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> CompactSlider;

	// Modern slider child owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> ModernSlider;

	// Generic single-value input owned by simple Widget Blueprints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> ValueInput;

	// Compact single-value input resolved from the Widget Blueprint child named CompactValueInput.
	UPROPERTY(Transient)
	TObjectPtr<UBaseTextInputWidget> CompactValueInputWidget;

	// Modern single-value input resolved from the Widget Blueprint child named ModernValueInput.
	UPROPERTY(Transient)
	TObjectPtr<UBaseTextInputWidget> ModernValueInputWidget;

	// Generic range input owned by simple Widget Blueprints.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> RangeInput;

	// Compact range input resolved from the Widget Blueprint child named CompactRangeInput.
	UPROPERTY(Transient)
	TObjectPtr<UBaseTextInputWidget> CompactRangeInputWidget;

	// Modern range input resolved from the Widget Blueprint child named ModernRangeInput.
	UPROPERTY(Transient)
	TObjectPtr<UBaseTextInputWidget> ModernRangeInputWidget;
};
