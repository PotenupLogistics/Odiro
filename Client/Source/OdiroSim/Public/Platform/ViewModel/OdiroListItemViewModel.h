#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "OdiroListItemViewModel.generated.h"

// ListView/TileView row와 card WBP가 표시할 공통 item ViewModel.
UCLASS(BlueprintType)
class ODIROSIM_API UOdiroListItemViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// 목록 item의 기본 표시/식별 상태를 한 번에 설정한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void InitializeItem(
		const FString& itemId,
		const FString& title,
		const FString& subtitle,
		const FString& payloadPath);

	// item command와 선택 동기화에 쓰는 안정 id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	FString GetItemId() const { return ItemId; }

	// item id를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetItemId(const FString& itemId);

	// item 주 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	FString GetTitle() const { return Title; }

	// item 주 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetTitle(const FString& title);

	// item 보조 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	FString GetSubtitle() const { return Subtitle; }

	// item 보조 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetSubtitle(const FString& subtitle);

	// item이 가리키는 파일/폴더 payload 경로를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	FString GetPayloadPath() const { return PayloadPath; }

	// item이 가리키는 파일/폴더 payload 경로를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetPayloadPath(const FString& payloadPath);

	// item 선택 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	bool IsSelected() const { return bSelected; }

	// item 선택 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetSelected(bool bInSelected);

	// item command 활성 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|ViewModel")
	bool IsEnabled() const { return bEnabled; }

	// item command 활성 상태를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|ViewModel")
	void SetEnabled(bool bInEnabled);

private:
	// command routing에 쓰는 item id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	FString ItemId;

	// item title 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	FString Title;

	// item subtitle 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	FString Subtitle;

	// item이 나타내는 파일/폴더 절대 경로.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	FString PayloadPath;

	// item 선택 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;

	// item command 활성 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bEnabled = true;
};
