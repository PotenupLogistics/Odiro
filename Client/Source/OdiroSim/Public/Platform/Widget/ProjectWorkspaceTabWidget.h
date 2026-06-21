#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWorkspaceTabWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FProjectWorkspaceTabNative, class UProjectWorkspaceTabWidget*);

// Project workspace 상단 tab의 상태와 click event를 C++ 로직에 연결하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectWorkspaceTabWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Tab이 대표하는 stable id를 설정한다.
	void SetTabId(const FName& tabId);

	// Tab label을 표시 위젯에 반영한다.
	void SetTabLabel(const FText& label);

	// Tab 활성 상태를 저장하고 WBP visual state event를 호출한다.
	void SetTabActive(bool bInActive);

	// Tab close 가능 상태를 저장하고 close control 표시 상태를 갱신한다.
	void SetTabClosable(bool bInClosable);

	// Tab 표시 여부를 갱신한다.
	void SetTabVisible(bool bInVisible);

	// Tab이 대표하는 stable id를 반환한다.
	FName GetTabId() const { return TabId; }

	// Tab 선택 요청을 parent widget에 알린다.
	FProjectWorkspaceTabNative OnSelectedRequested;

	// Tab 닫기 요청을 parent widget에 알린다.
	FProjectWorkspaceTabNative OnCloseRequested;

protected:
	// WBP가 active/closable state에 맞는 visual을 적용하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "ProjectWorkspaceTab")
	void BP_OnTabStateChanged(bool bIsActive, bool bIsClosable);

	// Main tab button click을 선택 요청으로 변환한다.
	UFUNCTION()
	void HandleTabButtonClicked();

	// Close button click을 닫기 요청으로 변환한다.
	UFUNCTION()
	void HandleCloseButtonClicked();

private:
	// Tab 전체 click target. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabButton;

	// Tab label text. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TabLabelText;

	// Optional close click target. Visual layout은 WBP가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// Optional active-state indicator owned by WBP layout.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActiveIndicator;

	// 이 tab의 stable logical id.
	FName TabId;

	// 이 tab의 현재 active 상태.
	bool bActive = false;

	// 이 tab의 현재 closable 상태.
	bool bClosable = false;
};
