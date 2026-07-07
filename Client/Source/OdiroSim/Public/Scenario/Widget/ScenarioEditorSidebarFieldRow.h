#pragma once

#include "CoreMinimal.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "UI/BaseWidget.h"
#include "Types/SlateEnums.h"
#include "ScenarioEditorSidebarFieldRow.generated.h"

class UBaseButtonWidget;
class UBaseDropdownWidget;
class UBaseSliderWidget;
class UBaseTextInputWidget;
class UTexture2D;
class UTextBlock;
class UWidget;
class UWidgetTextStyleCatalog;
class UScenarioTemplateFieldRowViewModel;

// Broadcasts when a Scenario Template field row commits an editable text value.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FScenarioEditorSidebarFieldRowTextCommitted,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts when a range field commits either min or max text.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FScenarioEditorSidebarFieldRowRangeCommitted,
	const FText&,
	MinText,
	const FText&,
	MaxText,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts when an array-capable row requests a structural edit.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FScenarioEditorSidebarFieldRowActionRequested);

// Broadcasts committed text with the row-owned repeated item index.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FScenarioEditorSidebarFieldRowIndexedTextCommitted,
	int32,
	ItemIndex,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Broadcasts an array action with the row-owned repeated item index.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarFieldRowIndexedActionRequested,
	int32,
	ItemIndex);

// Leaf property row for project scenario sidebar fields such as "scenario_id : value".
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarFieldRow : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Sets default icon assets used by BaseButton-backed row actions.
	UScenarioEditorSidebarFieldRow(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Binds Base child widget delegates after UMG construction.
	virtual void NativeConstruct() override;
	// Releases Base child widget delegates before teardown.
	virtual void NativeDestruct() override;
	// Applies exposed row properties to bound WBP-owned Base child widgets.
	virtual void SynchronizeBaseProperties() override;

	// Updates the row label and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetFieldLabel(const FString& label);

	// Returns the row label.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetFieldLabel() const { return FieldLabel; }

	// Updates the row value and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetValueText(const FString& text);

	// Returns the value currently displayed by the row.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetValueText() const;

	// Updates the min/max value text used by range-capable rows.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRangeValueText(const FString& minText, const FString& maxText);

	// Returns the minimum text currently displayed by the range editor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetMinValueText() const;

	// Returns the maximum text currently displayed by the range editor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetMaxValueText() const;

	// Updates the options available when this row uses dropdown input.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetComboOptions(const TArray<FString>& options);

	// Returns the options available when this row uses dropdown input.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	TArray<FString> GetComboOptions() const { return ComboOptions; }

	// Updates optional display names and thumbnails used by dropdown options.
	void SetComboOptionSummaries(
		const TMap<FString, FText>& optionDisplayTexts,
		const TMap<FString, TSoftObjectPtr<UTexture2D>>& optionThumbnailTextures);

	// Updates the preferred editor control shape for this row.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetInputType(EScenarioEditorSidebarFieldInputType inInputType);

	// Returns the preferred editor control shape for this row.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	EScenarioEditorSidebarFieldInputType GetInputType() const { return InputType; }

	// Toggles editable versus read-only value presentation.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetEditable(bool bInEditable);

	// Returns whether the row currently exposes editable controls.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool IsEditable() const { return bEditable; }

	// Toggles multiline editable presentation for long string values.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetMultilineValue(bool bInMultilineValue);

	// Returns whether text mode should use multiline wrapping.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool IsMultilineValue() const { return bMultilineValue; }

	// Toggles range-capable rows between single-value and min/max editing.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRangeInputEnabled(bool bInRangeInputEnabled);

	// Returns whether the row is currently using min/max range input.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool IsRangeInputEnabled() const { return bRangeInputEnabled; }

	// Toggles add/remove controls for array-like fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetArrayControlsEnabled(bool bInArrayControlsEnabled);

	// Returns whether add/remove controls are enabled as a group.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool HasArrayControls() const { return bArrayControlsEnabled; }

	// Toggles only the add control for array-like fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetAddItemControlVisible(bool bInAddItemControlVisible);

	// Returns whether the add-item control is visible.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool IsAddItemControlVisible() const { return bAddItemControlVisible; }

	// Toggles only the remove control for array-like fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRemoveItemControlVisible(bool bInRemoveItemControlVisible);

	// Returns whether the remove-item control is visible.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool IsRemoveItemControlVisible() const { return bRemoveItemControlVisible; }

	// Updates the repeated item context index emitted by indexed delegates.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetActionContextIndex(int32 inActionContextIndex);

	// Returns the repeated item context index emitted by indexed delegates.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	int32 GetActionContextIndex() const { return ActionContextIndex; }

	// Toggles the explicit unset option for combo-box fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetComboAllowsUnset(bool bInComboAllowsUnset, const FString& unsetDisplayText);

	// Returns whether dropdown input exposes an explicit unset option.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	bool AllowsComboUnset() const { return bComboAllowsUnset; }

	// Returns the display label used for an unset dropdown value.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetComboUnsetDisplayText() const { return ComboUnsetDisplayText; }

	// Updates explicit BaseSlider bounds for numeric and range rows.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetSliderSpec(FScenarioEditorSidebarFieldSliderSpec inSliderSpec);

	// Returns explicit BaseSlider bounds for numeric and range rows.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldSliderSpec GetSliderSpec() const { return SliderSpec; }

	// Updates the shared typography catalog reference kept for legacy callers.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Returns the legacy typography catalog reference.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> GetTextStyleCatalog() const { return TextStyleCatalog; }

	// Applies display/editor state from the field row ViewModel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void InitializeFromItemViewModel(UScenarioTemplateFieldRowViewModel* itemViewModel);

	// Emits committed text from the editable value control.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowTextCommitted OnValueTextCommitted;

	// Emits committed min/max text from the editable range controls.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowRangeCommitted OnRangeValueTextCommitted;

	// Emits when the row requests adding an array item.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowActionRequested OnAddItemRequested;

	// Emits when the row requests removing an array item.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowActionRequested OnRemoveItemRequested;

	// Emits committed text with the row action context index.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowIndexedTextCommitted OnIndexedValueTextCommitted;

	// Emits add requests with the row action context index.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowIndexedActionRequested OnIndexedAddItemRequested;

	// Emits remove requests with the row action context index.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowIndexedActionRequested OnIndexedRemoveItemRequested;

protected:
	// Label displayed on the left side of the field row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetFieldLabel", Setter = "SetFieldLabel", BlueprintGetter = "GetFieldLabel", BlueprintSetter = "SetFieldLabel", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	FString FieldLabel;

	// Current text value shown by either the editable or read-only value control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValueText", Setter = "SetValueText", BlueprintGetter = "GetValueText", BlueprintSetter = "SetValueText", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	FString ValueText;

	// Optional minimum text used when the field is edited as a range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetMinValueText", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	FString MinValueText;

	// Optional maximum text used when the field is edited as a range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetMaxValueText", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	FString MaxValueText;

	// Option list used when this row is edited through a dropdown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetComboOptions", Setter = "SetComboOptions", BlueprintGetter = "GetComboOptions", BlueprintSetter = "SetComboOptions", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	TArray<FString> ComboOptions;

	// Preferred editor control shape for this row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetInputType", Setter = "SetInputType", BlueprintGetter = "GetInputType", BlueprintSetter = "SetInputType", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	EScenarioEditorSidebarFieldInputType InputType = EScenarioEditorSidebarFieldInputType::Text;

	// Controls whether the row should expose editable controls when bound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsEditable", Setter = "SetEditable", BlueprintGetter = "IsEditable", BlueprintSetter = "SetEditable", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bEditable = true;

	// Controls whether editable text values use multiline wrapping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsMultilineValue", Setter = "SetMultilineValue", BlueprintGetter = "IsMultilineValue", BlueprintSetter = "SetMultilineValue", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bMultilineValue = false;

	// Controls whether range-capable rows show min/max boxes instead of a single value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsRangeInputEnabled", Setter = "SetRangeInputEnabled", BlueprintGetter = "IsRangeInputEnabled", BlueprintSetter = "SetRangeInputEnabled", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bRangeInputEnabled = false;

	// Controls whether this row exposes add/remove buttons for array-like fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "HasArrayControls", Setter = "SetArrayControlsEnabled", BlueprintGetter = "HasArrayControls", BlueprintSetter = "SetArrayControlsEnabled", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bArrayControlsEnabled = false;

	// Controls whether this row exposes the add button independently from remove.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAddItemControlVisible", Setter = "SetAddItemControlVisible", BlueprintGetter = "IsAddItemControlVisible", BlueprintSetter = "SetAddItemControlVisible", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bAddItemControlVisible = false;

	// Controls whether this row exposes the remove button independently from add.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsRemoveItemControlVisible", Setter = "SetRemoveItemControlVisible", BlueprintGetter = "IsRemoveItemControlVisible", BlueprintSetter = "SetRemoveItemControlVisible", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bRemoveItemControlVisible = false;

	// Controls whether dropdown input exposes an explicit unset option.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "AllowsComboUnset", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	bool bComboAllowsUnset = false;

	// Repeated item index emitted by indexed row delegates.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetActionContextIndex", Setter = "SetActionContextIndex", BlueprintGetter = "GetActionContextIndex", BlueprintSetter = "SetActionContextIndex", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	int32 ActionContextIndex = INDEX_NONE;

	// Display label used for an unset dropdown value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetComboUnsetDisplayText", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	FString ComboUnsetDisplayText = TEXT("(unset)");

	// Minimum height used by multiline inputs before content-driven expansion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template", meta = (ClampMin = "24.0", ExposeOnSpawn = "true"))
	float MultilineValueHeight = 96.0f;

	// Explicit BaseSlider bounds for numeric and range rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSliderSpec", Setter = "SetSliderSpec", BlueprintGetter = "GetSliderSpec", BlueprintSetter = "SetSliderSpec", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	FScenarioEditorSidebarFieldSliderSpec SliderSpec;

	// Shared typography catalog kept as a compatibility bridge for existing setup code.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTextStyleCatalog", Setter = "SetTextStyleCatalog", BlueprintGetter = "GetTextStyleCatalog", BlueprintSetter = "SetTextStyleCatalog", Category = "Scenario|Editor|Template", meta = (ExposeOnSpawn = "true"))
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Icon shown when a range-capable row is currently using min/max input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (ExposeOnSpawn = "true"))
	TSoftObjectPtr<UTexture2D> RangeInputIconTexture;

	// Icon shown when a range-capable row is currently using single-value input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (ExposeOnSpawn = "true"))
	TSoftObjectPtr<UTexture2D> FixedInputIconTexture;

	// Icon shown by the optional add-item control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (ExposeOnSpawn = "true"))
	TSoftObjectPtr<UTexture2D> AddItemIconTexture;

	// Icon shown by the optional remove-item control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (ExposeOnSpawn = "true"))
	TSoftObjectPtr<UTexture2D> RemoveItemIconTexture;

	// Icon size passed to BaseButton action buttons; values <= 0 use BaseButton defaults.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (ClampMin = "0.0", ExposeOnSpawn = "true"))
	float GeneratedActionIconSize = 0.0f;

	// Optional label text widget bound by the UMG row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Optional separator text widget, normally ":".
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> SeparatorTextBlock;

	// Optional BaseTextInput used for read-only, single-line, multiline, and range values.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UBaseTextInputWidget> ValueEditableTextBox;

	// Optional BaseDropdown for fields constrained to a known option set.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UBaseDropdownWidget> ValueComboBox;

	// Optional BaseButton that toggles between single and min/max input.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UBaseButtonWidget> RangeToggleButton;

	// Optional BaseButton that requests adding an array item near this row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UBaseButtonWidget> AddItemButton;

	// Optional BaseButton that requests removing an array item represented by this row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UBaseButtonWidget> RemoveItemButton;

	// Optional BaseSlider shown only when explicit slider bounds are configured.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UBaseSliderWidget> ValueSlider;

private:
	// Handles text commits from the optional editable value control.
	UFUNCTION()
	void HandleValueTextCommitted(UBaseTextInputWidget* widget, const FText& text);

	// Handles committed selections from the optional dropdown.
	UFUNCTION()
	void HandleDropdownSelectionChanged(UWidget* widget, FName selectedId);

	// Handles committed lower/upper values from the range variant of the value input.
	UFUNCTION()
	void HandleRangeValueCommitted(UBaseTextInputWidget* widget, float lowerValue, float upperValue);

	// Handles clicks on the range mode toggle.
	UFUNCTION()
	void HandleRangeToggleClicked(UBaseButtonWidget* button);

	// Handles clicks on the add array item button.
	UFUNCTION()
	void HandleAddItemClicked(UBaseButtonWidget* button);

	// Handles clicks on the remove array item button.
	UFUNCTION()
	void HandleRemoveItemClicked(UBaseButtonWidget* button);

	// Handles single-value slider edits.
	UFUNCTION()
	void HandleSliderValueChanged(UWidget* widget, float value);

	// Handles range slider edits.
	UFUNCTION()
	void HandleSliderRangeValueChanged(UWidget* widget, float lowerValue, float upperValue);

	// Binds editable control delegates owned by this row.
	void BindControls();
	// Releases editable control delegates owned by this row.
	void UnbindControls();
	// Returns whether the current type should show the multiline editor.
	bool UsesMultilineInput() const;
	// Returns whether the current type should show the dropdown editor.
	bool UsesComboInput() const;
	// Returns whether the current type can toggle to min/max input.
	bool IsRangeCapable() const;
	// Returns whether the row should currently show min/max input.
	bool UsesRangeInput() const;
	// Returns whether the current type can use BaseSlider.
	bool IsSliderCapable() const;
	// Returns whether the BaseSlider should currently be visible.
	bool ShouldShowSlider() const;
	// Applies dropdown options and selected value to the bound dropdown.
	void RefreshDropdownOptions();
	// Applies scalar/range values to the bound BaseSlider.
	void RefreshSlider();
	// Emits a value commit through the legacy row delegate surface.
	void BroadcastValueCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Returns the user-facing display text for one dropdown option.
	FText ResolveComboOptionDisplayText(const FString& option) const;
	// Returns the optional thumbnail texture for one dropdown option.
	UTexture2D* ResolveComboOptionThumbnail(const FString& option);
	// Resolves a soft texture once and reuses it for later row synchronizations.
	UTexture2D* ResolveCachedTexture(const TSoftObjectPtr<UTexture2D>& textureReference);
	// Applies runtime text to a WBP-owned TextBlock child.
	void SetTextBlockText(UTextBlock* textWidget, const FString& text) const;
	// Applies text-mode state to a BaseTextInput child.
	void SetTextInputState(UBaseTextInputWidget* inputWidget, const FString& text, bool bVisible, bool bTextWrap);
	// Applies number-range state to the primary BaseTextInput child.
	void SetRangeTextInputState(UBaseTextInputWidget* inputWidget, bool bVisible);
	// Applies icon and visibility state to a BaseButton child.
	void SetActionButtonState(UBaseButtonWidget* button, bool bVisible, UTexture2D* icon, const FText& fallbackGlyph) const;
	// Applies multiline height while preserving the WBP-authored primary input size baseline.
	void SetValueInputSizeConstraints(UBaseTextInputWidget* inputWidget, bool bTextWrap);
	// Normalizes invalid slider specs to disabled state.
	static FScenarioEditorSidebarFieldSliderSpec NormalizeSliderSpec(FScenarioEditorSidebarFieldSliderSpec inSliderSpec);

	// Option value to display text map used by asset-backed dropdown rows.
	UPROPERTY(Transient)
	TMap<FString, FText> ComboOptionDisplayTextByValue;

	// Option value to thumbnail map used by asset-backed dropdown rows.
	UPROPERTY(Transient)
	TMap<FString, TSoftObjectPtr<UTexture2D>> ComboOptionThumbnailByValue;

	// Soft texture cache used by row action icons and dropdown thumbnails.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> CachedTexturesByPath;

	// Prevents child events from echoing property synchronization writes.
	UPROPERTY(Transient)
	bool bSynchronizing = false;

	// WBP-authored primary text-input size constraints restored outside multiline mode.
	UPROPERTY(Transient)
	FBaseWidgetSizeConstraints ValueInputBaseSizeConstraints;

	// Whether the primary text-input size baseline has been captured.
	UPROPERTY(Transient)
	bool bValueInputBaseSizeConstraintsCaptured = false;
};
