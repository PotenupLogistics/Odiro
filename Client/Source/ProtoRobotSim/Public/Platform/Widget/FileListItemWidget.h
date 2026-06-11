#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FileListItemWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FFileListRenameRequestedNative,
	class UFileListItemWidget*,
	const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FFileListActionRequestedNative, class UFileListItemWidget*);

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UFileListItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void InitializeItem(
		const FString& itemPath,
		FString primaryActionLabel = FString(TEXT("편집")),
		FString secondaryActionLabel = FString(TEXT("실행")),
		bool bInAllowRename = true,
		bool bInAllowPrimaryAction = true,
		bool bInAllowSecondaryAction = false);
	void InitializeDisplayItem(
		const FString& itemPath,
		const FString& displayText,
		FString primaryActionLabel = FString(TEXT("편집")),
		FString secondaryActionLabel = FString(TEXT("실행")),
		bool bInAllowRename = true,
		bool bInAllowPrimaryAction = true,
		bool bInAllowSecondaryAction = false);
	FString GetOriginalPath() const { return OriginalPath; }
	FString GetEditedPath() const;

	FFileListRenameRequestedNative OnRenameRequested;
	FFileListActionRequestedNative OnPrimaryActionRequested;
	FFileListActionRequestedNative OnSecondaryActionRequested;

protected:
	UFUNCTION()
	void HandlePathTextChanged(const FText& text);

	UFUNCTION()
	void HandleRenameClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleSecondaryActionClicked();

private:
	void RefreshButtonState();

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> PathTextBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RenameButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> PrimaryActionButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> SecondaryActionButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RenameButtonLabel;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PrimaryActionLabel;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> SecondaryActionLabel;

	FString OriginalPath;
	bool bAllowRename = true;
	bool bAllowPrimaryAction = true;
	bool bAllowSecondaryAction = false;
};
