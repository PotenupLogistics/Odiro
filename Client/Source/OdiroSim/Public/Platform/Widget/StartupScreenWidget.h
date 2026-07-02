#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/StartupScreenViewModel.h"
#include "UI/BaseWidget.h"
#include "StartupScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextWidget;
class UPanelWidget;
class URecentProjectCardWidget;
class UScrollBox;
class USpacer;
class UStartupScreenWidget;
class UStartupScreenViewModel;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStartupScreenCreateProjectRequested, UStartupScreenWidget*, StartupScreen);

// Project open 성공을 root shell에 전달하는 event.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FStartupScreenProjectOpened,
	UStartupScreenWidget*,
	StartupScreen,
	const FString&,
	ProjectPath);

// Master Widget 내부에 배치되는 project 선택 content panel.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UStartupScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// 외부 Master Widget이 소유한 ViewModel을 주입한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void SetViewModel(UStartupScreenViewModel* viewModel);

	// 현재 연결된 StartupScreen ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|StartupScreen")
	UStartupScreenViewModel* GetViewModel() const { return StartupScreenViewModel; }

	// ViewModel 상태를 다시 읽어 카드/진단 표시를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	void RefreshFromViewModel();

	// 현재 선택된 project를 연다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	bool OpenSelectedProject();

	// 지정 project root를 검증하고 연다.
	UFUNCTION(BlueprintCallable, Category = "Platform|StartupScreen")
	bool OpenProjectPath(const FString& projectPath);

	// 새 project 생성 flow가 필요함을 Master Widget에 알린다.
	UPROPERTY(BlueprintAssignable, Category = "Platform|StartupScreen|Events")
	FStartupScreenCreateProjectRequested OnCreateProjectRequested;

	// Project open 성공을 Master Widget에 알린다.
	UPROPERTY(BlueprintAssignable, Category = "Platform|StartupScreen|Events")
	FStartupScreenProjectOpened OnProjectOpened;

protected:
	// Designer preview에서 runtime-owned preview 상태를 갱신한다.
	virtual void NativePreConstruct() override;

	// Runtime binding과 내부 ViewModel을 초기화한다.
	virtual void NativeConstruct() override;

	// Viewport 크기 변화에 맞춰 startup panel scroll padding을 갱신한다.
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;

	// Runtime binding을 해제한다.
	virtual void NativeDestruct() override;

private:
	// 내부 ViewModel이 없으면 생성하고 초기화한다.
	UStartupScreenViewModel* EnsureStartupScreenViewModel();

	// WBP에서 수정 가능한 오류 상황별 진단 문구를 ViewModel에 주입한다.
	void ApplyDiagnosticMessagesToViewModel();

	// 버튼 click delegate를 연결한다.
	void BindControls();

	// 버튼 click delegate를 해제한다.
	void UnbindControls();

	// 최근 project 카드 목록을 다시 생성한다.
	void RefreshRecentProjectCards();

	// 카드 class 설정을 해석한다.
	TSubclassOf<URecentProjectCardWidget> ResolveRecentProjectCardWidgetClass() const;

	// Startup panel이 여유 공간에서는 중앙에 있고 작은 viewport에서는 scroll 범위가 되도록 padding을 조정한다.
	void UpdateStartupPanelScrollPadding(const FVector2D& screenSize);

	// WBP-authored startup panel slot 값을 runtime 보정 기준으로 저장한다.
	bool CaptureStartupPanelAuthoredLayout();

	// OS folder picker로 기존 project root를 고른다.
	bool BrowseForExistingProjectFolder(FString& outFolder) const;

	// 내 프로젝트 열기 button click을 처리한다.
	UFUNCTION()
	void HandleOpenProjectClicked(UBaseButtonWidget* button);

	// 새 project 만들기 button click을 처리한다.
	UFUNCTION()
	void HandleCreateProjectClicked(UBaseButtonWidget* button);

	// 화면 하단 가로 scrollbar 입력을 실제 panel scroll viewport에 반영한다.
	UFUNCTION()
	void HandleStartupPanelStickyHorizontalScrolled(float currentOffset);

	// 실제 panel scroll viewport의 가로 offset 변화를 화면 하단 scrollbar에 반영한다.
	UFUNCTION()
	void HandleStartupPanelContentHorizontalScrolled(float currentOffset);

	// 화면 오른쪽 세로 scrollbar 입력을 실제 panel scroll viewport에 반영한다.
	UFUNCTION()
	void HandleStartupPanelStickyVerticalScrolled(float currentOffset);

	// 실제 panel scroll viewport의 세로 offset 변화를 화면 오른쪽 scrollbar에 반영한다.
	UFUNCTION()
	void HandleStartupPanelContentVerticalScrolled(float currentOffset);

	// 최근 project card click을 project open 요청으로 처리한다.
	void HandleRecentProjectCardSelected(URecentProjectCardWidget* cardWidget);

	// 최근 project 카드가 들어갈 WBP-owned panel.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> RecentProjectCardPanel;

	// 최근 project가 없을 때 표시할 WBP-owned empty state.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RecentProjectsEmptyState;

	// ViewModel diagnostics를 표시할 WBP-owned text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> DiagnosticsText;

	// Startup panel 세로 scroll을 content 영역 오른쪽에 고정하는 WBP-owned scroll box.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> StartupPanelVerticalScrollBox;

	// Startup panel 가로 overflow를 처리하는 WBP-owned scroll box.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> StartupPanelHorizontalScrollBox;

	// 고정 크기 startup panel surface.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StartupPanelSurface;

	// 작은 viewport에서 화면 하단에 고정되는 WBP-owned optional 가로 scrollbar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> StartupPanelStickyHorizontalScrollBox;

	// WBP-owned optional 가로 scrollbar의 scroll range를 만드는 spacer.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USpacer> StartupPanelStickyHorizontalScrollSpacer;

	// 작은 viewport에서 화면 오른쪽에 고정되는 WBP-owned optional 세로 scrollbar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> StartupPanelStickyVerticalScrollBox;

	// WBP-owned optional 세로 scrollbar의 scroll range를 만드는 spacer.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USpacer> StartupPanelStickyVerticalScrollSpacer;

	// 기존 project folder picker를 여는 WBP-owned button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBaseButtonWidget> OpenProjectButton;

	// 새 project flow 요청을 보내는 WBP-owned button.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBaseButtonWidget> CreateProjectButton;

	// 반복 생성할 최근 project card Widget Blueprint class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URecentProjectCardWidget> RecentProjectCardWidgetClass;

	// WBP에서 수정 가능한 오류 상황별 진단 문구.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Diagnostics", meta = (AllowPrivateAccess = "true"))
	FStartupScreenDiagnosticMessages DiagnosticMessages;

	// Existing project picker dialog title.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|StartupScreen|Dialog", meta = (AllowPrivateAccess = "true"))
	FText BrowseDialogTitle;

	// 이 screen이 소비하는 독립 StartupScreen ViewModel.
	UPROPERTY(Transient)
	TObjectPtr<UStartupScreenViewModel> StartupScreenViewModel;

	// 현재 생성된 최근 project card 인스턴스.
	UPROPERTY(Transient)
	TArray<TObjectPtr<URecentProjectCardWidget>> RecentProjectCards;

	// WBP-authored StartupPanelSurface slot padding.
	UPROPERTY(Transient)
	FMargin StartupPanelSurfaceBasePadding;

	// 마지막으로 적용한 startup panel padding 계산 입력.
	UPROPERTY(Transient)
	FVector2D CachedStartupPanelPaddingInput = FVector2D::ZeroVector;

	// 마지막으로 적용한 startup panel desired size.
	UPROPERTY(Transient)
	FVector2D CachedStartupPanelDesiredSize = FVector2D::ZeroVector;

	// 마지막으로 적용한 startup panel 가로 scroll 필요 상태.
	UPROPERTY(Transient)
	uint8 bCachedStartupPanelNeedsHorizontalScroll : 1 = false;

	// 마지막으로 적용한 startup panel 세로 scroll 필요 상태.
	UPROPERTY(Transient)
	uint8 bCachedStartupPanelNeedsVerticalScroll : 1 = false;

	// StartupPanelSurfaceBasePadding capture 완료 여부.
	UPROPERTY(Transient)
	uint8 bHasStartupPanelSurfaceBasePadding : 1 = false;

	// 가로 scroll offset 동기화 중 재진입을 막는다.
	UPROPERTY(Transient)
	bool bSyncingStartupPanelHorizontalScroll = false;

	// 세로 scroll offset 동기화 중 재진입을 막는다.
	UPROPERTY(Transient)
	bool bSyncingStartupPanelVerticalScroll = false;
};
