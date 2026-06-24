#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "FileListItemWidget.generated.h"

class UButton;
class UEditableTextBox;
class UOdiroListItemViewModel;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FFileListRenameRequestedNative,
	class UFileListItemWidget*,
	const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FFileListActionRequestedNative, class UFileListItemWidget*);

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UFileListItemWidget : public UOdiroCommonUserWidget
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
	// 공통 item ViewModel의 경로/표시 상태를 list row UI에 반영한다.
	void InitializeFromItemViewModel(
		UOdiroListItemViewModel* itemViewModel,
		FString primaryActionLabel = FString(TEXT("편집")),
		FString secondaryActionLabel = FString(TEXT("실행")),
		bool bInAllowRename = true,
		bool bInAllowPrimaryAction = true,
		bool bInAllowSecondaryAction = false);
	// 오른쪽 보조 버튼 자리에 클릭되지 않는 상태 text를 표시한다.
	void SetSecondaryActionDisplayOnly(const FString& displayText);
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

	// Row 표시 데이터를 제공하는 item ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UOdiroListItemViewModel> ItemViewModel;

	FString OriginalPath;
	bool bAllowRename = true;
	bool bAllowPrimaryAction = true;
	bool bAllowSecondaryAction = false;
	bool bSecondaryActionClickable = false;
};
