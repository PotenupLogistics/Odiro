#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioTemplateFieldRowViewModel.generated.h"

// Scenario Template sidebar field row가 표시할 field metadata와 editor state.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioTemplateFieldRowViewModel : public UOdiroListItemViewModel
{
	GENERATED_BODY()

public:
	// Field row 표시/입력 상태를 한 번에 초기화한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void InitializeFieldRow(
		const FString& fieldId,
		const FString& label,
		const FString& valueText,
		EScenarioEditorSidebarFieldInputType inputType,
		bool bInEditable,
		bool bInArrayControlsEnabled = false,
		bool bInVisible = true);

	// Row value text를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	FString GetValueText() const { return ValueText; }

	// Row value text를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetValueText(const FString& text);

	// Range min/max text를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	FString GetMinValueText() const { return MinValueText; }

	// Range max text를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	FString GetMaxValueText() const { return MaxValueText; }

	// Range min/max text를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetRangeValueText(const FString& minText, const FString& maxText);

	// ComboBox option 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	TArray<FString> GetComboOptions() const { return ComboOptions; }

	// ComboBox option 목록을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetComboOptions(const TArray<FString>& options);

	// Row 입력 control shape을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	EScenarioEditorSidebarFieldInputType GetInputType() const { return InputType; }

	// Row 입력 control shape을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetInputType(EScenarioEditorSidebarFieldInputType inInputType);

	// Row editable 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	bool IsFieldEditable() const { return bEditable; }

	// Row editable 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetFieldEditable(bool bInEditable);

	// Multiline editor 사용 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	bool UsesMultilineValue() const { return bMultilineValue; }

	// Multiline editor 사용 여부를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetMultilineValue(bool bInMultilineValue);

	// Range input 활성 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	bool IsRangeInputEnabled() const { return bRangeInputEnabled; }

	// Range input 활성 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetRangeInputEnabled(bool bInRangeInputEnabled);

	// Array add/remove control 노출 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	bool HasArrayControls() const { return bArrayControlsEnabled; }

	// Array add/remove control 노출 여부를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetArrayControlsEnabled(bool bInArrayControlsEnabled);

	// ComboBox unset option 노출 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	bool AllowsComboUnset() const { return bComboAllowsUnset; }

	// ComboBox unset display text를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	FString GetComboUnsetDisplayText() const { return ComboUnsetDisplayText; }

	// ComboBox unset option 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetComboAllowsUnset(bool bInComboAllowsUnset, const FString& unsetDisplayText);

	// Field row 표시 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	bool IsFieldVisible() const { return bVisible; }

	// Field row 표시 여부를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SetFieldVisible(bool bInVisible);

private:
	// Row value text.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	FString ValueText;

	// Range minimum text.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	FString MinValueText;

	// Range maximum text.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	FString MaxValueText;

	// ComboBox option 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	TArray<FString> ComboOptions;

	// Row 입력 control shape.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	EScenarioEditorSidebarFieldInputType InputType = EScenarioEditorSidebarFieldInputType::Text;

	// Row editable 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	bool bEditable = true;

	// Multiline editor 사용 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	bool bMultilineValue = false;

	// Range input 활성 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	bool bRangeInputEnabled = false;

	// Array add/remove control 노출 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	bool bArrayControlsEnabled = false;

	// ComboBox unset option 노출 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	bool bComboAllowsUnset = false;

	// ComboBox unset option display text.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	FString ComboUnsetDisplayText = TEXT("(unset)");

	// Field row visibility state owned by the sidebar ViewModel.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	bool bVisible = true;
};
