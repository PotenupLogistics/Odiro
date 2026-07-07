#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"

void UScenarioTemplateFieldRowViewModel::InitializeFieldRow(
	const FString& fieldId,
	const FString& label,
	const FString& valueText,
	const EScenarioEditorSidebarFieldInputType inputType,
	const bool bInEditable,
	const bool bInArrayControlsEnabled,
	const bool bInVisible)
{
	InitializeItem(fieldId, label, FString(), FString());
	SetValueText(valueText);
	SetInputType(inputType);
	SetFieldEditable(bInEditable);
	SetArrayControlsEnabled(bInArrayControlsEnabled);
	SetFieldVisible(bInVisible);
}

void UScenarioTemplateFieldRowViewModel::SetValueText(const FString& text)
{
	UE_MVVM_SET_PROPERTY_VALUE(ValueText, text);
}

void UScenarioTemplateFieldRowViewModel::SetRangeValueText(
	const FString& minText,
	const FString& maxText)
{
	UE_MVVM_SET_PROPERTY_VALUE(MinValueText, minText);
	UE_MVVM_SET_PROPERTY_VALUE(MaxValueText, maxText);
}

void UScenarioTemplateFieldRowViewModel::SetComboOptions(const TArray<FString>& options)
{
	UE_MVVM_SET_PROPERTY_VALUE(ComboOptions, options);
}

void UScenarioTemplateFieldRowViewModel::SetInputType(
	const EScenarioEditorSidebarFieldInputType inInputType)
{
	UE_MVVM_SET_PROPERTY_VALUE(InputType, inInputType);
	UE_MVVM_SET_PROPERTY_VALUE(bMultilineValue, inInputType == EScenarioEditorSidebarFieldInputType::MultilineText);
	if (inInputType != EScenarioEditorSidebarFieldInputType::Range)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bRangeInputEnabled, false);
	}
}

void UScenarioTemplateFieldRowViewModel::SetFieldEditable(const bool bInEditable)
{
	UE_MVVM_SET_PROPERTY_VALUE(bEditable, bInEditable);
}

void UScenarioTemplateFieldRowViewModel::SetMultilineValue(const bool bInMultilineValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bMultilineValue, bInMultilineValue);
	UE_MVVM_SET_PROPERTY_VALUE(
		InputType,
		bInMultilineValue
			? EScenarioEditorSidebarFieldInputType::MultilineText
			: EScenarioEditorSidebarFieldInputType::Text);
	if (bInMultilineValue)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bRangeInputEnabled, false);
	}
}

void UScenarioTemplateFieldRowViewModel::SetRangeInputEnabled(const bool bInRangeInputEnabled)
{
	UE_MVVM_SET_PROPERTY_VALUE(
		bRangeInputEnabled,
		bInRangeInputEnabled && InputType == EScenarioEditorSidebarFieldInputType::Range);
}

void UScenarioTemplateFieldRowViewModel::SetArrayControlsEnabled(const bool bInArrayControlsEnabled)
{
	UE_MVVM_SET_PROPERTY_VALUE(bArrayControlsEnabled, bInArrayControlsEnabled);
}

void UScenarioTemplateFieldRowViewModel::SetComboAllowsUnset(
	const bool bInComboAllowsUnset,
	const FString& unsetDisplayText)
{
	UE_MVVM_SET_PROPERTY_VALUE(bComboAllowsUnset, bInComboAllowsUnset);
	UE_MVVM_SET_PROPERTY_VALUE(
		ComboUnsetDisplayText,
		unsetDisplayText.IsEmpty() ? FString(TEXT("(unset)")) : unsetDisplayText);
}

void UScenarioTemplateFieldRowViewModel::SetSliderSpec(
	FScenarioEditorSidebarFieldSliderSpec inSliderSpec)
{
	inSliderSpec.DisplayDecimals = FMath::Clamp(inSliderSpec.DisplayDecimals, 0, 6);
	if (!FMath::IsFinite(inSliderSpec.MinValue)
		|| !FMath::IsFinite(inSliderSpec.MaxValue)
		|| inSliderSpec.MinValue >= inSliderSpec.MaxValue)
	{
		inSliderSpec.bEnabled = false;
	}
	if (inSliderSpec.bInteger)
	{
		inSliderSpec.DisplayDecimals = 0;
	}
	UE_MVVM_SET_PROPERTY_VALUE(SliderSpec, inSliderSpec);
}

void UScenarioTemplateFieldRowViewModel::SetFieldVisible(const bool bInVisible)
{
	UE_MVVM_SET_PROPERTY_VALUE(bVisible, bInVisible);
}
