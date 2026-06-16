#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "ScenarioPlaceableDetailsWidget.generated.h"

class USizeBox;
class UButton;
class UEditableText;
class UEditableTextBox;
class UScenarioPlaceableComponent;
class UTextBlock;
class UWidget;

// Transform field edited by one numeric row in the placeable details panel.
enum class EScenarioPlaceableDetailsTransformField : uint8
{
	LocationX,
	LocationY,
	LocationZ,
	RotationX,
	RotationY,
	RotationZ,
	ScaleX,
	ScaleY,
	ScaleZ
};

// Selection details panel for placeable-backed editor items.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioPlaceableDetailsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds optional child-widget events after UMG construction.
	virtual void NativeOnInitialized() override;
	// Acquires widget input mode while the details panel is visible.
	virtual void NativeConstruct() override;
	// Releases widget input mode when the details panel is removed.
	virtual void NativeDestruct() override;

	// Text color used for valid editable fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Details")
	FLinearColor NormalFieldTextColor = FLinearColor::White;

	// Text color used while a field is flashing a rejected value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Details")
	FLinearColor InvalidFieldTextColor = FLinearColor(1.0f, 0.08f, 0.04f, 1.0f);

	// Duration for temporary invalid-field highlighting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Details", meta = (ClampMin = "0.0"))
	float InvalidFieldFlashSeconds = 3.0f;

	// Optional button that enables instance-id editing.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UButton> EditInstanceIdButton;

	// Optional editable field for the selected placeable instance id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableText> InstanceIdEditableText;

	// Optional label for the selected placeable asset name.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UTextBlock> AssetNameTextBlock;

	// Optional container for transform-orientation controls.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<USizeBox> OrientationSizeBox;

	// Optional button that switches transform edits to world orientation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UButton> WorldOrientationButton;

	// Optional button that switches transform edits to relative orientation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UButton> RelativeOrientationButton;

	// Optional container for location edit rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<USizeBox> LocationSizeBox;

	// Optional numeric field for X location.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> LocationXTextBox;

	// Optional numeric field for Y location.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> LocationYTextBox;

	// Optional numeric field for Z location.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> LocationZTextBox;

	// Optional container for rotation edit rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<USizeBox> RotationSizeBox;

	// Optional numeric field for X rotation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> RotationXTextBox;

	// Optional numeric field for Y rotation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> RotationYTextBox;

	// Optional numeric field for Z rotation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> RotationZTextBox;

	// Optional container for scale edit rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<USizeBox> ScaleSizeBox;

	// Optional numeric field for X scale.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> ScaleXTextBox;

	// Optional numeric field for Y scale.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> ScaleYTextBox;

	// Optional numeric field for Z scale.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UEditableTextBox> ScaleZTextBox;

	// Optional button that deletes the selected placeable.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Details")
	TObjectPtr<UButton> DeleteButton;

	// Sets the placeable object whose properties are shown by this details panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Details")
	void SetSelectedPlaceable(UScenarioPlaceableComponent* placeableComponent);

	// Refreshes editable fields from the current selected placeable state.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Details")
	void RefreshFromSelectedPlaceable();

protected:
	// Handles the request to make the selected instance id editable.
	UFUNCTION()
	void HandleEditInstanceIdButtonClicked();

	// Handles switching transform editing to world orientation.
	UFUNCTION()
	void HandleWorldOrientationButtonClicked();

	// Handles switching transform editing to selected-object relative orientation.
	UFUNCTION()
	void HandleRelativeOrientationButtonClicked();

	// Handles delete requests for the selected placeable.
	UFUNCTION()
	void HandleDeleteButtonClicked();

	// Commits a pending selected placeable instance-id edit.
	UFUNCTION()
	void HandleInstanceIdCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending X location edit.
	UFUNCTION()
	void HandleLocationXCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending Y location edit.
	UFUNCTION()
	void HandleLocationYCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending Z location edit.
	UFUNCTION()
	void HandleLocationZCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending X rotation edit.
	UFUNCTION()
	void HandleRotationXCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending Y rotation edit.
	UFUNCTION()
	void HandleRotationYCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending Z rotation edit.
	UFUNCTION()
	void HandleRotationZCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending X scale edit.
	UFUNCTION()
	void HandleScaleXCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending Y scale edit.
	UFUNCTION()
	void HandleScaleYCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Commits a pending Z scale edit.
	UFUNCTION()
	void HandleScaleZCommitted(const FText& text, ETextCommit::Type commitMethod);

private:
	// Requests editor widget input mode while a details field may capture keyboard input.
	void RequestEditorWidgetInputMode();
	// Releases editor widget input mode requested by this details panel.
	void ReleaseEditorWidgetInputMode();
	// Commits one transform row after validating the complete transform block.
	bool CommitTransformField(
		UEditableTextBox* textBox,
		EScenarioPlaceableDetailsTransformField field,
		ETextCommit::Type commitMethod);
	// Reads the transform block and returns the first invalid field.
	bool TryReadTransformFields(FTransform& outTransform, UEditableTextBox*& outInvalidTextBox) const;
	// Parses one numeric field as a double.
	bool TryReadDoubleField(UEditableTextBox* textBox, double& outValue) const;
	// Checks whether the selected placeable allows editing the given transform row.
	bool IsTransformFieldEditable(
		const UScenarioPlaceableComponent* placeableComponent,
		EScenarioPlaceableDetailsTransformField field) const;
	// Applies editability and visibility rules for selected-placeable fields.
	void ApplyEditPermissions(const UScenarioPlaceableComponent* placeableComponent);
	// Applies orientation button state from the editor controller and selected object.
	void ApplyOrientationControls(const UScenarioPlaceableComponent* placeableComponent);
	// Updates a numeric field's editability without changing its value.
	void SetTextBoxEditable(UEditableTextBox* textBox, bool bEditable) const;
	// Temporarily highlights a numeric field that failed validation.
	void FlashInvalidField(UEditableTextBox* textBox);
	// Applies a foreground color to a numeric field.
	void SetTextBoxFieldColor(UEditableTextBox* textBox, const FLinearColor& color) const;
	// Temporarily highlights the instance-id field after validation failure.
	void FlashInvalidInstanceIdField();
	// Applies a foreground color to the instance-id field.
	void SetInstanceIdFieldColor(const FLinearColor& color);
	// Writes the selected instance id into the editable text field.
	void SetInstanceIdFieldText(const FString& instanceId);
	// Writes a transform into the bound numeric fields.
	void SetTransformFieldTexts(const FTransform& transform);
	// Writes one numeric field using the panel's fixed decimal format.
	void SetFieldText(UEditableTextBox* textBox, double value) const;

	// Currently selected placeable whose fields are displayed by the details panel.
	TWeakObjectPtr<UScenarioPlaceableComponent> SelectedPlaceableComponent;
	// Guards field-change handlers while the panel is refreshing from selection state.
	bool bRefreshingFields = false;
};
