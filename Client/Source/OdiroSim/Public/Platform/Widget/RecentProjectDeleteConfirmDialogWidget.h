#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "RecentProjectDeleteConfirmDialogWidget.generated.h"

class UBaseButtonWidget;
class URecentProjectDeleteConfirmDialogWidget;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FRecentProjectDeleteConfirmDialogNative,
	URecentProjectDeleteConfirmDialogWidget*);

// Wires the WBP-authored recent-project delete confirmation UI to native events.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URecentProjectDeleteConfirmDialogWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Updates the project root path shown by the confirmation dialog.
	UFUNCTION(BlueprintCallable, Category = "Platform|Recent Projects")
	void SetDeleteTarget(const FString& projectPath);

	// Notifies the owner that the confirm button was clicked.
	FRecentProjectDeleteConfirmDialogNative OnConfirmed;

	// Notifies the owner that the cancel button was clicked.
	FRecentProjectDeleteConfirmDialogNative OnCanceled;

protected:
	// Binds WBP-owned button delegates.
	virtual void NativeConstruct() override;

	// Releases WBP-owned button delegates.
	virtual void NativeDestruct() override;

private:
	// Applies the current target path to the bound text widgets.
	void RefreshDeleteTargetTexts();

	// Converts the confirm button click to a native event.
	UFUNCTION()
	void HandleConfirmClicked(UBaseButtonWidget* button);

	// Converts the cancel button click to a native event.
	UFUNCTION()
	void HandleCancelClicked(UBaseButtonWidget* button);

	// WBP-owned button that accepts physical deletion.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> ConfirmButton;

	// WBP-owned button that cancels the delete confirmation.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> CancelButton;

	// WBP-owned text showing the target folder name.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProjectFolderNameText;

	// WBP-owned text showing the target absolute folder path.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProjectFolderPathText;

	// Normalized project root path currently shown by the dialog.
	UPROPERTY(Transient)
	FString DeleteTargetProjectPath;
};
