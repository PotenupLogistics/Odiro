#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "Platform/Widget/WindowActionBarWidget.h"
#include "Platform/Widget/WindowTabBarWidget.h"
#include "WindowStatusBarWidget.generated.h"

class UWindowsControlWidget;

// Window status bar tab 선택을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowStatusBarTabSelectedNative, FName);

// Window status bar action button 요청을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE(FWindowStatusBarActionNative);

// Window status bar action id 요청을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowStatusBarActionRequestedNative, FName);

// Window status bar result tab close 요청을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowStatusBarResultTabCloseRequestedNative, FName);

// Window status bar tab 선택을 Blueprint parent surface에 전달하는 event.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindowStatusBarTabSelected, FName, TabId);

// Window status bar action button 요청을 Blueprint parent surface에 전달하는 event.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWindowStatusBarActionRequested);

// Window status bar action id 요청을 Blueprint parent surface에 전달하는 event.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindowStatusBarActionIdRequested, FName, ActionId);

// Status bar가 제공하는 호환 action slot.
UENUM(BlueprintType)
enum class EWindowStatusBarActionSlot : uint8
{
	Confirm,
	Run
};

// Window title/status bar 조합 WBP의 tab, action, window control event를 연결하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UWindowStatusBarWidget : public UOdiroCommonUserWidget
{
	GENERATED_BODY()

public:
	// 기본 action 설정을 만든다.
	UWindowStatusBarWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Designer preview와 runtime에서 child bar 기본 상태를 맞춘다.
	virtual void NativePreConstruct() override;

	// Runtime child bar delegate를 native event로 연결한다.
	virtual void NativeConstruct() override;

	// Runtime child bar delegate 연결을 해제한다.
	virtual void NativeDestruct() override;

	// 활성 tab id를 갱신하고 tab visual state에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar")
	void SetActiveTab(FName tabId);

	// 지정 tab의 표시 여부를 저장하고 tab bar에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar")
	void SetTabVisible(FName tabId, bool bVisible);

	// 전체 tab bar 표시 여부를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar")
	void SetTabBarVisible(bool bVisible);

	// 현재 활성 tab id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window Status Bar")
	FName GetActiveTab() const;

	// Confirm action slot의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window Status Bar|Action|Ids")
	static FName GetConfirmActionId();

	// Run action slot의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window Status Bar|Action|Ids")
	static FName GetRunActionId();

	// 지정 action slot의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window Status Bar|Action|Ids")
	static FName GetActionIdForSlot(EWindowStatusBarActionSlot actionSlot);

	// 실험 결과 tab 목록을 교체한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Tabs")
	void SetResultTabs(const TArray<FWindowTabConfig>& tabs);

	// Action button 목록을 현재 screen 설정으로 교체한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Action")
	void SetActionButtons(const TArray<FWindowActionButtonConfig>& actions);

	// 오른쪽 action button slot을 화면별 설정으로 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Action")
	void SetActionButtonConfig(EWindowStatusBarActionSlot actionSlot, const FWindowActionButtonConfig& config);

	// Confirm action button slot 설정을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Action")
	void SetConfirmActionButtonConfig(const FWindowActionButtonConfig& config);

	// Run action button slot 설정을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Action")
	void SetRunActionButtonConfig(const FWindowActionButtonConfig& config);

	// 오른쪽 action button slot 표시 여부를 빠르게 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Action")
	void SetActionButtonVisible(EWindowStatusBarActionSlot actionSlot, bool bVisible);

	// 오른쪽 action button slot을 기본값으로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "Window Status Bar|Action")
	void ResetActionButtonConfigs();

	// Tab 선택 요청을 native owner에게 전달한다.
	FWindowStatusBarTabSelectedNative OnTabSelectedNative;

	// Confirm action button click을 native owner에게 전달한다.
	FWindowStatusBarActionNative OnConfirmActionRequestedNative;

	// Run action button click을 native owner에게 전달한다.
	FWindowStatusBarActionNative OnRunActionRequestedNative;

	// Any action button click을 native owner에게 전달한다.
	FWindowStatusBarActionRequestedNative OnActionRequestedNative;

	// Dynamic result tab close 요청을 native owner에게 전달한다.
	FWindowStatusBarResultTabCloseRequestedNative OnResultTabCloseRequestedNative;

	// Tab 선택 요청을 Blueprint owner에게 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "Window Status Bar|Events")
	FWindowStatusBarTabSelected OnTabSelected;

	// Confirm action button click을 Blueprint owner에게 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "Window Status Bar|Events")
	FWindowStatusBarActionRequested OnConfirmActionRequested;

	// Run action button click을 Blueprint owner에게 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "Window Status Bar|Events")
	FWindowStatusBarActionRequested OnRunActionRequested;

	// Any action button click을 Blueprint owner에게 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "Window Status Bar|Events")
	FWindowStatusBarActionIdRequested OnActionRequested;

protected:
	// WBP가 tab 선택에 맞춰 추가 visual state를 갱신하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window Status Bar")
	void BP_OnTabSelected(FName tabId);

	// WBP가 confirm action click에 반응하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window Status Bar")
	void BP_OnConfirmActionRequested();

	// WBP가 run action click에 반응하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window Status Bar")
	void BP_OnRunActionRequested();

	// WBP가 action id click에 반응하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window Status Bar")
	void BP_OnActionRequested(FName actionId);

private:
	// Child bar에 저장 action config를 적용한다.
	void ConfigureActionButtons();

	// Child bar delegate를 연결한다.
	void BindControls();

	// Child bar delegate 연결을 해제한다.
	void UnbindControls();

	// 지정 action slot의 저장 config를 반환한다.
	FWindowActionButtonConfig& ResolveActionConfigBySlot(EWindowStatusBarActionSlot actionSlot);

	// 지정 action slot의 저장 config를 반환한다.
	const FWindowActionButtonConfig& ResolveActionConfigBySlot(EWindowStatusBarActionSlot actionSlot) const;

	// 지정 action slot의 logical id를 반환한다.
	FName ResolveActionIdBySlot(EWindowStatusBarActionSlot actionSlot) const;

	// Tab bar 선택 요청을 status bar event로 변환한다.
	void HandleTabBarTabSelected(FName tabId);

	// Result tab close 요청을 status bar event로 변환한다.
	void HandleTabBarResultTabCloseRequested(FName tabId);

	// Action bar click을 status bar event로 변환한다.
	void HandleActionBarActionRequested(FName actionId);

	// WBP layout이 소유하는 standalone tab bar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWindowTabBarWidget> TabBar;

	// WBP layout이 소유하는 standalone action bar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWindowActionBarWidget> ActionBar;

	// WBP layout이 소유하는 native Windows control button group.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWindowsControlWidget> WindowControls;

	// Confirm action button의 현재 화면별 설정.
	FWindowActionButtonConfig ConfirmActionConfig;

	// Run action button의 현재 화면별 설정.
	FWindowActionButtonConfig RunActionConfig;
};
