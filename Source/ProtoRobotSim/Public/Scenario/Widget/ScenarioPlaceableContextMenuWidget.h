#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "ScenarioPlaceableContextMenuWidget.generated.h"

class USizeBox;
class UButton;
class UEditableText;
class UEditableTextBox;
class UScenarioPlaceableComponent;
class UTextBlock;
class UWidget;

enum class EScenarioPlaceableContextMenuTransformField : uint8
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

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UScenarioPlaceableContextMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Context Menu")
	FLinearColor NormalFieldTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Context Menu")
	FLinearColor InvalidFieldTextColor = FLinearColor(1.0f, 0.08f, 0.04f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Context Menu", meta = (ClampMin = "0.0"))
	float InvalidFieldFlashSeconds = 3.0f;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UButton> EditInstanceIdButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableText> InstanceIdEditableText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UTextBlock> AssetNameTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<USizeBox> OrientationSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UButton> WorldOrientationButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UButton> RelativeOrientationButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<USizeBox> LocationSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> LocationXTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> LocationYTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> LocationZTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<USizeBox> RotationSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> RotationXTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> RotationYTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> RotationZTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<USizeBox> ScaleSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> ScaleXTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> ScaleYTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> ScaleZTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UButton> DeleteButton;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Context Menu")
	void SetSelectedPlaceable(UScenarioPlaceableComponent* placeableComponent);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Context Menu")
	void RefreshFromSelectedPlaceable();

protected:
	UFUNCTION()
	void HandleEditInstanceIdButtonClicked();

	UFUNCTION()
	void HandleWorldOrientationButtonClicked();

	UFUNCTION()
	void HandleRelativeOrientationButtonClicked();

	UFUNCTION()
	void HandleDeleteButtonClicked();

	UFUNCTION()
	void HandleInstanceIdCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleLocationXCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleLocationYCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleLocationZCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleRotationXCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleRotationYCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleRotationZCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleScaleXCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleScaleYCommitted(const FText& text, ETextCommit::Type commitMethod);

	UFUNCTION()
	void HandleScaleZCommitted(const FText& text, ETextCommit::Type commitMethod);

private:
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	bool CommitTransformField(
		UEditableTextBox* textBox,
		EScenarioPlaceableContextMenuTransformField field,
		ETextCommit::Type commitMethod);
	bool TryReadTransformFields(FTransform& outTransform, UEditableTextBox*& outInvalidTextBox) const;
	bool TryReadDoubleField(UEditableTextBox* textBox, double& outValue) const;
	bool IsTransformFieldEditable(
		const UScenarioPlaceableComponent* placeableComponent,
		EScenarioPlaceableContextMenuTransformField field) const;
	void ApplyEditPermissions(const UScenarioPlaceableComponent* placeableComponent);
	void ApplyOrientationControls(const UScenarioPlaceableComponent* placeableComponent);
	void SetTextBoxEditable(UEditableTextBox* textBox, bool bEditable) const;
	void FlashInvalidField(UEditableTextBox* textBox);
	void SetTextBoxFieldColor(UEditableTextBox* textBox, const FLinearColor& color) const;
	void FlashInvalidInstanceIdField();
	void SetInstanceIdFieldColor(const FLinearColor& color);
	void SetInstanceIdFieldText(const FString& instanceId);
	void SetTransformFieldTexts(const FTransform& transform);
	void SetFieldText(UEditableTextBox* textBox, double value) const;

	TWeakObjectPtr<UScenarioPlaceableComponent> SelectedPlaceableComponent;
	bool bRefreshingFields = false;
};
