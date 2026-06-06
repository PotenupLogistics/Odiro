#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "EpisodePlaceableContextMenuWidget.generated.h"

class UButton;
class UEditableText;
class UEditableTextBox;
class UEpisodePlaceableComponent;
class UTextBlock;
class UWidget;

enum class EEpisodePlaceableContextMenuTransformField : uint8
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
class PROTOROBOTSIM_API UEpisodePlaceableContextMenuWidget : public UUserWidget
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
	TObjectPtr<UEditableTextBox> LocationXTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> LocationYTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> LocationZTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> RotationXTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> RotationYTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> RotationZTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> ScaleXTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> ScaleYTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UEditableTextBox> ScaleZTextBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Context Menu")
	TObjectPtr<UButton> DeleteButton;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Context Menu")
	void SetSelectedPlaceable(UEpisodePlaceableComponent* placeableComponent);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Context Menu")
	void RefreshFromSelectedPlaceable();

protected:
	UFUNCTION()
	void HandleEditInstanceIdButtonClicked();

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
		EEpisodePlaceableContextMenuTransformField field,
		ETextCommit::Type commitMethod);
	bool TryReadTransformFields(FTransform& outTransform, UEditableTextBox*& outInvalidTextBox) const;
	bool TryReadDoubleField(UEditableTextBox* textBox, double& outValue) const;
	void FlashInvalidField(UEditableTextBox* textBox);
	void SetTextBoxFieldColor(UEditableTextBox* textBox, const FLinearColor& color) const;
	void FlashInvalidInstanceIdField();
	void SetInstanceIdFieldColor(const FLinearColor& color);
	void SetInstanceIdFieldText(const FString& instanceId);
	void SetTransformFieldTexts(const FTransform& transform);
	void SetFieldText(UEditableTextBox* textBox, double value) const;

	TWeakObjectPtr<UEpisodePlaceableComponent> SelectedPlaceableComponent;
	bool bRefreshingFields = false;
};
