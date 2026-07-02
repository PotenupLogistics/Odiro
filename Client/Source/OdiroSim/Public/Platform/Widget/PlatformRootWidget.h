#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/WindowActionBarWidget.h"
#include "UI/BaseWidget.h"
#include "PlatformRootWidget.generated.h"

class UProjectOverviewScreenWidget;
class UProjectCreateScreenWidget;
class URunDetailScreenWidget;
class URunListScreenWidget;
class UScenarioEditorRootWidget;
class UStartupScreenWidget;
class UWidget;
class UWidgetSwitcher;
class UWindowStatusBarWidget;

// Platform root가 표시할 screen content 종류.
UENUM(BlueprintType)
enum class EPlatformRootScreen : uint8
{
	Startup,
	ProjectCreate,
	ProjectOverview,
	ScenarioEditor,
	RobotConfig,
	RunList,
	RunDetail
};

// Active root screen change notification used by editor-owned overlays.
DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformRootScreenChangedNative, EPlatformRootScreen);

// Window status bar와 현재 platform screen content를 조합하는 root widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UPlatformRootWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Startup screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowStartupScreen();

	// Project create screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowProjectCreateScreen();

	// Project overview screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowProjectOverviewScreen();

	// Scenario editor screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowScenarioEditorScreen();

	// Robot config screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowRobotConfigScreen();

	// Run list screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowRunListScreen();

	// Run detail screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void ShowRunDetailScreen(const FString& runId);

	// 지정 screen을 content 영역에 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|Root")
	void SetActiveScreen(EPlatformRootScreen screen);

	// 현재 표시 중인 screen 종류를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Root")
	EPlatformRootScreen GetActiveScreen() const { return ActiveScreen; }

	// Scenario editor controller가 등록할 수 있는 editor root를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|Root")
	UScenarioEditorRootWidget* GetScenarioEditorRootWidget() const;

	// Active screen change notification for native owners.
	FPlatformRootScreenChangedNative OnActiveScreenChangedNative;

protected:
	// Designer preview와 runtime에서 status bar 기본 표시를 맞춘다.
	virtual void NativePreConstruct() override;

	// Child screen event와 status bar event를 연결한다.
	virtual void NativeConstruct() override;

	// Child screen event와 status bar event 연결을 해제한다.
	virtual void NativeDestruct() override;

private:
	// Root child widget 이벤트를 연결한다.
	void BindControls();

	// Root child widget 이벤트 연결을 해제한다.
	void UnbindControls();

	// 현재 screen에 맞춰 status bar tab/action 표시를 갱신한다.
	void ConfigureStatusBarForActiveScreen();

	// Starts or stops the Robot tab preview according to the active root screen.
	void UpdateRobotPreviewActivation(EPlatformRootScreen previousScreen, EPlatformRootScreen nextScreen);

	// 현재 world가 active project workspace를 표시해야 하는지 반환한다.
	bool ShouldUseProjectWorkspaceDefault() const;

	// 현재 project session이 active project를 소유하는지 반환한다.
	bool HasActiveProject() const;

	// 현재 run detail result tab id를 만든다.
	FName BuildRunDetailTabId() const;

	// Result tab id에서 run id를 복원한다.
	static FString ExtractRunIdFromResultTabId(FName tabId);

	// Result tab에 표시할 run id 부분을 만든다.
	static FString MakeRunDetailResultTabDisplayId(const FString& runId);

	// Root UI가 mouse cursor를 소유하도록 player input mode를 적용한다.
	void ApplyRootInputMode();

	// Startup screen의 create project 요청을 screen 전환으로 처리한다.
	UFUNCTION()
	void HandleStartupCreateProjectRequested(UStartupScreenWidget* startupScreen);

	// Startup screen의 project open 성공을 workspace 진입으로 처리한다.
	UFUNCTION()
	void HandleStartupProjectOpened(UStartupScreenWidget* startupScreen, const FString& projectPath);

	// Project create screen의 cancel 요청을 screen 전환으로 처리한다.
	UFUNCTION()
	void HandleProjectCreateCancelRequested(UProjectCreateScreenWidget* projectCreateScreen);

	// Project create screen의 생성 완료를 startup screen 복귀로 처리한다.
	UFUNCTION()
	void HandleProjectCreated(UProjectCreateScreenWidget* projectCreateScreen, const FString& projectPath);

	// Overview guide의 scenario 요청을 screen 전환으로 처리한다.
	UFUNCTION()
	void HandleOverviewScenarioRequested(UProjectOverviewScreenWidget* overviewScreen);

	// Overview guide의 robot 요청을 screen 전환으로 처리한다.
	UFUNCTION()
	void HandleOverviewRobotRequested(UProjectOverviewScreenWidget* overviewScreen);

	// Overview guide의 experiment 요청을 screen 전환으로 처리한다.
	UFUNCTION()
	void HandleOverviewExperimentRequested(UProjectOverviewScreenWidget* overviewScreen);

	// Run list의 detail 요청을 screen 전환으로 처리한다.
	UFUNCTION()
	void HandleRunDetailRequested(URunListScreenWidget* runListScreen, const FString& runId);

	// Status bar tab 선택 요청을 screen 전환으로 처리한다.
	void HandleStatusBarTabSelected(FName tabId);

	// Status bar action을 현재 screen command로 전달한다.
	void HandleStatusBarActionRequested(FName actionId);

	// Result tab close 요청을 RunList 복귀로 처리한다.
	void HandleStatusBarResultTabCloseRequested(FName tabId);

	// WBP layout이 소유하는 custom window status bar.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWindowStatusBarWidget> WindowStatusBar;

	// WBP layout이 소유하는 screen content switcher.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ScreenContentSwitcher;

	// WBP layout이 소유하는 startup screen content.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UStartupScreenWidget> StartupScreen;

	// WBP layout이 소유하는 project create screen content.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UProjectCreateScreenWidget> ProjectCreateScreen;

	// WBP layout이 소유하는 project overview screen content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectOverviewScreenWidget> ProjectOverviewScreen;

	// WBP layout이 소유하는 scenario editor screen content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ScenarioEditorScreen;

	// WBP layout이 소유하는 robot config screen content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RobotConfigScreen;

	// WBP layout이 소유하는 run list screen content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<URunListScreenWidget> RunListScreen;

	// WBP layout이 소유하는 run detail screen content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<URunDetailScreenWidget> RunDetailScreen;

	// ProjectCreate screen에서 confirm slot에 적용할 WBP-authored action config.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Root|Actions", meta = (AllowPrivateAccess = "true"))
	FWindowActionButtonConfig ProjectCreateConfirmActionConfig;

	// Save/confirm action config.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Root|Actions", meta = (AllowPrivateAccess = "true"))
	FWindowActionButtonConfig SaveActionConfig;

	// Run action config.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Root|Actions", meta = (AllowPrivateAccess = "true"))
	FWindowActionButtonConfig RunActionConfig;

	// Analyze/detail action config.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Root|Actions", meta = (AllowPrivateAccess = "true"))
	FWindowActionButtonConfig AnalyzeActionConfig;

	// Back action config.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Root|Actions", meta = (AllowPrivateAccess = "true"))
	FWindowActionButtonConfig BackActionConfig;

	// Run id currently represented by RunDetail screen and result tab.
	UPROPERTY(Transient)
	FString ActiveRunDetailId;

	// 현재 content 영역에 표시 중인 screen.
	UPROPERTY(Transient)
	EPlatformRootScreen ActiveScreen = EPlatformRootScreen::Startup;
};
