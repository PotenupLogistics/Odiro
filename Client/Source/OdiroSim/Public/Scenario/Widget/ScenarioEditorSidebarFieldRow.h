#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Types/SlateEnums.h"
#include "ScenarioEditorSidebarFieldRow.generated.h"

class UEditableTextBox;
class UButton;
class UComboBoxString;
class UHorizontalBox;
class UImage;
class UTexture2D;
class UMultiLineEditableTextBox;
class USizeBox;
class UTextBlock;
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
class ODIROSIM_API UScenarioEditorSidebarFieldRow : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds editable controls after UMG construction.
	virtual void NativeConstruct() override;
	// Releases editable controls before teardown.
	virtual void NativeDestruct() override;

	// Label displayed on the left side of the field row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString FieldLabel;

	// Current text value shown by either the editable or read-only value control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString ValueText;

	// Optional minimum text used when the field is edited as a range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString MinValueText;

	// Optional maximum text used when the field is edited as a range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString MaxValueText;

	// Option list used when this row is edited through a combo box.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TArray<FString> ComboOptions;

	// Preferred editor control shape for this row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	EScenarioEditorSidebarFieldInputType InputType = EScenarioEditorSidebarFieldInputType::Text;

	// Controls whether the row should expose the editable text box when one is bound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bEditable = true;

	// Controls whether editable values use the bounded multiline input instead of the single-line input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bMultilineValue = false;

	// Controls whether range-capable rows show min/max boxes instead of a single value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bRangeInputEnabled = false;

	// Controls whether this row exposes add/remove buttons for array-like fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bArrayControlsEnabled = false;

	// Controls whether this row exposes the add button independently from remove.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bAddItemControlVisible = false;

	// Controls whether this row exposes the remove button independently from add.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bRemoveItemControlVisible = false;

	// Controls whether combo-box input exposes an explicit unset option.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bComboAllowsUnset = false;

	// Repeated item index emitted by indexed row delegates.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	int32 ActionContextIndex = INDEX_NONE;

	// Display label used for an unset combo-box value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString ComboUnsetDisplayText = TEXT("(unset)");

	// Minimum height used by multiline inputs before content-driven expansion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template", meta = (ClampMin = "24.0"))
	float MultilineValueHeight = 96.0f;

	// Shared typography catalog used to style label and value controls.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional label text block bound by the UMG row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Optional separator text block, normally ":".
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> SeparatorTextBlock;

	// Optional read-only value text block for non-editable rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> ValueTextBlock;

	// Optional editable text box for editable rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UEditableTextBox> ValueEditableTextBox;

	// Optional combo box for fields constrained to a known option set.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UComboBoxString> ValueComboBox;

	// Optional fixed-size wrapper for editable multiline values.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<USizeBox> ValueMultiLineSizeBox;

	// Optional multiline editable text box for long string values.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UMultiLineEditableTextBox> ValueMultiLineEditableTextBox;

	// Optional horizontal range editor container used for min/max field values.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UHorizontalBox> ValueRangeBox;

	// Optional editable text box for the range minimum value.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UEditableTextBox> MinValueEditableTextBox;

	// Optional separator text shown between range min and max values.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> RangeSeparatorTextBlock;

	// Optional editable text box for the range maximum value.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UEditableTextBox> MaxValueEditableTextBox;

	// Optional button that toggles a range-capable field between single and min/max input.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UButton> RangeToggleButton;

	// Optional text used by the range toggle button.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> RangeToggleTextBlock;

	// Optional icon used by the range toggle button when the WBP owns icon binding.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UImage> RangeToggleIconImage;

	// Optional button that requests adding an array item near this row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UButton> AddItemButton;

	// Optional text used by the add item button.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> AddItemTextBlock;

	// Optional icon used by the add item button when C++ or WBP owns icon binding.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UImage> AddItemIconImage;

	// Optional button that requests removing an array item represented by this row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UButton> RemoveItemButton;

	// Optional text used by the remove item button.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> RemoveItemTextBlock;

	// Optional icon used by the remove item button when C++ or WBP owns icon binding.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UImage> RemoveItemIconImage;

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

	// Updates the row label and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetFieldLabel(const FString& label);

	// Updates the row value and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetValueText(const FString& text);

	// Updates the min/max value text used by range-capable rows.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRangeValueText(const FString& minText, const FString& maxText);

	// Updates the options available when this row uses combo-box input.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetComboOptions(const TArray<FString>& options);

	// Updates optional display names and thumbnails used by combo-box options.
	void SetComboOptionSummaries(
		const TMap<FString, FText>& optionDisplayTexts,
		const TMap<FString, TSoftObjectPtr<UTexture2D>>& optionThumbnailTextures);

	// Updates the preferred editor control shape for this row.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetInputType(EScenarioEditorSidebarFieldInputType inInputType);

	// Toggles editable versus read-only value presentation.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetEditable(bool bInEditable);

	// Toggles multiline editable presentation for long string values.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetMultilineValue(bool bInMultilineValue);

	// Toggles range-capable rows between single-value and min/max editing.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRangeInputEnabled(bool bInRangeInputEnabled);

	// Toggles add/remove controls for array-like fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetArrayControlsEnabled(bool bInArrayControlsEnabled);

	// Toggles only the add control for array-like fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetAddItemControlVisible(bool bInAddItemControlVisible);

	// Toggles only the remove control for array-like fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRemoveItemControlVisible(bool bInRemoveItemControlVisible);

	// Updates the repeated item context index emitted by indexed delegates.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetActionContextIndex(int32 inActionContextIndex);

	// Toggles the explicit unset option for combo-box fields.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetComboAllowsUnset(bool bInComboAllowsUnset, const FString& unsetDisplayText);

	// Updates the shared typography catalog reference used by this row.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Applies display/editor state from the field row ViewModel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void InitializeFromItemViewModel(UScenarioTemplateFieldRowViewModel* itemViewModel);

	// Returns the value currently displayed by the row.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetValueText() const;

	// Returns the minimum text currently displayed by the range editor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetMinValueText() const;

	// Returns the maximum text currently displayed by the range editor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetMaxValueText() const;

private:
	// Handles text commits from the optional editable value control.
	UFUNCTION()
	void HandleValueTextCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles committed selections from the optional combo box.
	UFUNCTION()
	void HandleValueComboSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	// Builds one combo option row with optional asset thumbnail metadata.
	UFUNCTION()
	UWidget* HandleGenerateComboOptionWidget(FString item);

	// Handles range minimum text commits.
	UFUNCTION()
	void HandleMinValueTextCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles range maximum text commits.
	UFUNCTION()
	void HandleMaxValueTextCommitted(const FText& text, ETextCommit::Type commitMethod);
	// Handles clicks on the range mode toggle.
	UFUNCTION()
	void HandleRangeToggleClicked();
	// Handles clicks on the add array item button.
	UFUNCTION()
	void HandleAddItemClicked();
	// Handles clicks on the remove array item button.
	UFUNCTION()
	void HandleRemoveItemClicked();

	// Binds editable control delegates owned by this row.
	void BindControls();
	// Releases editable control delegates owned by this row.
	void UnbindControls();
	// Applies shared sidebar field spacing to the optional WBP-owned controls.
	void ApplyVisualStyle();
	// Creates generated icon content for optional range/fixed buttons when WBP binding is absent.
	void EnsureRangeToggleIcon();
	// Creates generated icon content for optional row add/remove buttons when WBP binding is absent.
	void EnsureArrayActionIcons();
	// Applies the flat no-background visual style to row action buttons.
	void ApplyFlatButtonStyle(UButton* button) const;
	// Applies the range/fixed icon and flat visual state to the range toggle button.
	void ApplyRangeToggleButtonState() const;
	// Applies visibility and style state to row add/remove buttons.
	void ApplyArrayActionButtonState() const;
	// Applies stored label, value, and editability state to bound controls.
	void RefreshRow();
	// Returns whether the current type should show the multiline editor.
	bool UsesMultilineInput() const;
	// Returns whether the current type should show the combo-box editor.
	bool UsesComboInput() const;
	// Returns whether the current type can toggle to min/max input.
	bool IsRangeCapable() const;
	// Returns whether the row should currently show min/max input.
	bool UsesRangeInput() const;
	// Applies combo options and selected value to the bound combo box.
	void RefreshComboBoxOptions();
	// Returns the user-facing display text for one combo option.
	FText ResolveComboOptionDisplayText(const FString& option) const;
	// Returns the optional thumbnail texture for one combo option.
	TSoftObjectPtr<UTexture2D> ResolveComboOptionThumbnail(const FString& option) const;
	// Applies text to a bound text block.
	void SetTextBlockText(UTextBlock* textBlock, const FString& text) const;

	// Option value to display text map used by asset-backed combo rows.
	UPROPERTY(Transient)
	TMap<FString, FText> ComboOptionDisplayTextByValue;

	// Option value to thumbnail map used by asset-backed combo rows.
	UPROPERTY(Transient)
	TMap<FString, TSoftObjectPtr<UTexture2D>> ComboOptionThumbnailByValue;
};
