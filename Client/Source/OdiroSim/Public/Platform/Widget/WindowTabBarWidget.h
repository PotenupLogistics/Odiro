#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/BaseWidgetTypes.h"
#include "WindowTabBarWidget.generated.h"

class UBaseButtonWidget;
class UBaseTabWidget;
class UHorizontalBox;
class UTexture2D;
class UWidget;

// Tab 선택을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowTabSelectedNative, FName);

// Result tab close 요청을 native parent surface에 전달하는 event.
DECLARE_MULTICAST_DELEGATE_OneParam(FWindowResultTabCloseRequestedNative, FName);

// Tab 선택을 Blueprint parent surface에 전달하는 event.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindowTabSelected, FName, TabId);

// Window tab bar에 표시할 tab 구성.
USTRUCT(BlueprintType)
struct ODIROSIM_API FWindowTabConfig
{
	GENERATED_BODY()

	// Logical tab id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	FName TabId;

	// Tab label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	FText Label;

	// Tab 표시 여부.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	bool bVisible = true;

	// Optional icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Optional icon glyph shown when Icon is empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	FText IconGlyphText;

	// true일 때 dynamic result tab에 close affordance를 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	bool bClosable = false;

	// Optional C++ size constraints; zero values leave WBP-authored tab sizing unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	FBaseWidgetSizeConstraints SizeConstraints;
};

// WBP fixed tab layout에 필요한 optional override만 전달하는 구성.
USTRUCT(BlueprintType)
struct ODIROSIM_API FWindowFixedTabConfig
{
	GENERATED_BODY()

	// Optional C++ size constraints; zero values leave WBP-authored tab sizing unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar")
	FBaseWidgetSizeConstraints SizeConstraints;
};

// 고정 platform tab과 실험 결과 tab을 구성하는 standalone tab bar widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UWindowTabBarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Designer preview와 runtime에서 WBP tab 기본값을 맞춘다.
	virtual void NativePreConstruct() override;

	// Runtime tab delegate를 native event로 연결한다.
	virtual void NativeConstruct() override;

	// Runtime tab delegate 연결을 해제한다.
	virtual void NativeDestruct() override;

	// 좁은 status bar에서 tab label을 숨기고 icon만 남기는 compact mode를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar")
	void SetCompactMode(bool bInCompactMode);

	// 현재 tab bar가 icon-only compact mode인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar")
	bool IsCompactMode() const { return bCompactMode; }

	// 활성 tab id를 갱신하고 tab visual state에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar")
	void SetActiveTab(FName tabId);

	// 지정 tab의 표시 여부를 저장하고 WBP tab widget에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar")
	void SetTabVisible(FName tabId, bool bVisible);

	// 현재 활성 tab id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar")
	FName GetActiveTab() const;

	// Startup fixed tab의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar|Ids")
	static FName GetStartupTabId();

	// Overview fixed tab의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar|Ids")
	static FName GetOverviewTabId();

	// Scenario fixed tab의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar|Ids")
	static FName GetScenarioTabId();

	// Robot fixed tab의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar|Ids")
	static FName GetRobotTabId();

	// Experiment fixed tab의 stable logical id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Window|Tab Bar|Ids")
	static FName GetExperimentTabId();

	// 실험 결과 tab 목록을 교체한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar|Results")
	void SetResultTabs(const TArray<FWindowTabConfig>& tabs);

	// 실험 결과 tab 하나를 추가하거나 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar|Results")
	void AddOrUpdateResultTab(const FWindowTabConfig& tab);

	// 실험 결과 tab 하나를 제거한다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar|Results")
	void RemoveResultTab(FName tabId);

	// 실험 결과 tab 목록을 비운다.
	UFUNCTION(BlueprintCallable, Category = "Window|Tab Bar|Results")
	void ClearResultTabs();

	// Tab 선택 요청을 native owner에게 전달한다.
	FWindowTabSelectedNative OnTabSelectedNative;

	// Dynamic result tab close 요청을 native owner에게 전달한다.
	FWindowResultTabCloseRequestedNative OnResultTabCloseRequestedNative;

	// Tab 선택 요청을 Blueprint owner에게 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "Window|Tab Bar|Events")
	FWindowTabSelected OnTabSelected;

protected:
	// WBP가 tab 선택에 맞춰 추가 visual state를 갱신하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window|Tab Bar")
	void BP_OnTabSelected(FName tabId);

	// WBP가 compact mode에 맞춰 label visibility 등 visual state를 갱신하는 extension point.
	UFUNCTION(BlueprintImplementableEvent, Category = "Window|Tab Bar")
	void BP_OnCompactModeChanged(bool bInCompactMode);

	// 실험 결과 tab을 런타임 생성할 WBP class. WBP default로 설정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Window|Tab Bar|Results")
	TSubclassOf<UBaseTabWidget> ResultTabWidgetClass;

private:
	// 고정 tab dimension을 적용한다.
	void ConfigureFixedTab(UBaseTabWidget* tabWidget, const FWindowFixedTabConfig& config);

	// C++에서 명시한 tab size constraint만 적용한다.
	void ApplyTabDimensions(UBaseTabWidget* tabWidget, const FBaseWidgetSizeConstraints& constraints) const;

	// WBP fixed tab visibility 기본값을 설정한다.
	void ConfigureFixedTabs();

	// 실험 결과 tab widget 목록을 현재 config와 맞춘다.
	void RebuildResultTabs();

	// Button click delegate를 연결한다.
	void BindControls();

	// Button click delegate 연결을 해제한다.
	void UnbindControls();

	// 단일 tab widget에 기본 상태와 click delegate를 연결한다.
	void ConfigureTab(UBaseTabWidget* tabWidget, const FWindowTabConfig& config);

	// 단일 tab widget의 native delegate를 연결한다.
	void BindTab(UBaseTabWidget* tabWidget);

	// 단일 tab widget의 native delegate 연결을 해제한다.
	void UnbindTab(UBaseTabWidget* tabWidget);

	// Result tab close button delegate를 연결한다.
	void BindResultTabClose(UBaseTabWidget* tabWidget);

	// Result tab close button delegate 연결을 해제한다.
	void UnbindResultTabClose(UBaseTabWidget* tabWidget);

	// 모든 tab widget에 현재 active state를 반영한다.
	void ApplyTabStates();

	// Tab widget과 optional host wrapper에 표시 상태를 같이 반영한다.
	void ApplyTabVisibility(FName tabId, bool bVisible);

	// 저장된 tab visibility 값을 반환한다.
	bool IsTabVisible(FName tabId) const;

	// 지정 tab id에 맞는 tab widget을 반환한다.
	UBaseTabWidget* ResolveTabById(FName tabId) const;

	// Base tab click source를 logical tab id로 변환한다.
	FName ResolveTabIdByButton(const UBaseButtonWidget* tabWidget) const;

	// 지정 fixed tab id에 맞는 WBP tab host wrapper를 반환한다.
	UWidget* ResolveFixedTabHostById(FName tabId) const;

	// 고정 tab id인지 반환한다.
	bool IsFixedTabId(FName tabId) const;

	// 고정 또는 실험 결과 tab id인지 반환한다.
	bool IsKnownTabId(FName tabId) const;

	// Tab 선택 요청을 native/Blueprint event로 broadcast한다.
	void BroadcastTabSelected(FName tabId);

	// BaseTab 선택 요청을 logical tab id event로 변환한다.
	UFUNCTION()
	void HandleTabSelected(UBaseButtonWidget* tabWidget);

	// BaseTab hover 시작을 logical tab id state로 변환한다.
	UFUNCTION()
	void HandleTabHovered(UBaseButtonWidget* tabWidget);

	// BaseTab hover 종료를 logical tab id state로 변환한다.
	UFUNCTION()
	void HandleTabUnhovered(UBaseButtonWidget* tabWidget);

	// Result tab close 요청을 logical tab id event로 변환한다.
	void HandleResultTabCloseRequested(UBaseTabWidget* tabWidget);

	// WBP layout이 소유하는 startup tab.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTabWidget> StartupTab;

	// Startup tab의 안정적인 layout 폭을 소유하는 WBP wrapper.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StartupTabHost;

	// WBP layout이 소유하는 overview tab.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTabWidget> OverviewTab;

	// Overview tab의 안정적인 layout 폭을 소유하는 WBP wrapper.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OverviewTabHost;

	// WBP layout이 소유하는 scenario tab.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTabWidget> ScenarioTab;

	// Scenario tab의 안정적인 layout 폭을 소유하는 WBP wrapper.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ScenarioTabHost;

	// WBP layout이 소유하는 robot tab.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTabWidget> RobotTab;

	// Robot tab의 안정적인 layout 폭을 소유하는 WBP wrapper.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RobotTabHost;

	// WBP layout이 소유하는 experiment tab.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTabWidget> ExperimentTab;

	// Experiment tab의 안정적인 layout 폭을 소유하는 WBP wrapper.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExperimentTabHost;

	// Runtime experiment result tabs를 담는 WBP-owned container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ResultTabContainer;

	// 현재 선택된 logical tab id.
	FName ActiveTabId;

	// 현재 pointer hover 중인 logical tab id.
	FName HoveredTabId;

	// Startup fixed tab의 optional WBP-editable override.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar|Fixed Tabs", meta = (AllowPrivateAccess = "true"))
	FWindowFixedTabConfig StartupTabConfig;

	// Overview fixed tab의 optional WBP-editable override.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar|Fixed Tabs", meta = (AllowPrivateAccess = "true"))
	FWindowFixedTabConfig OverviewTabConfig;

	// Scenario fixed tab의 optional WBP-editable override.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar|Fixed Tabs", meta = (AllowPrivateAccess = "true"))
	FWindowFixedTabConfig ScenarioTabConfig;

	// Robot fixed tab의 optional WBP-editable override.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar|Fixed Tabs", meta = (AllowPrivateAccess = "true"))
	FWindowFixedTabConfig RobotTabConfig;

	// Experiment fixed tab의 optional WBP-editable override.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Tab Bar|Fixed Tabs", meta = (AllowPrivateAccess = "true"))
	FWindowFixedTabConfig ExperimentTabConfig;

	// 현재 tab bar의 responsive compact 상태.
	UPROPERTY(Transient)
	bool bCompactMode = false;

	// Tab id별 표시 상태 override.
	TMap<FName, bool> TabVisibilityById;

	// 실험 결과 tab id 순서.
	UPROPERTY(Transient)
	TArray<FName> ResultTabOrder;

	// 실험 결과 tab config.
	UPROPERTY(Transient)
	TMap<FName, FWindowTabConfig> ResultTabConfigsById;

	// 생성된 실험 결과 tab widget.
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBaseTabWidget>> ResultTabsById;
};
