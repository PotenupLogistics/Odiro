#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/StartupScreenViewModel.h"
#include "UI/BaseWidget.h"
#include "StartupScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextWidget;
class UPanelWidget;
class URecentProjectCardWidget;
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

	// OS folder picker로 기존 project root를 고른다.
	bool BrowseForExistingProjectFolder(FString& outFolder) const;

	// 내 프로젝트 열기 button click을 처리한다.
	UFUNCTION()
	void HandleOpenProjectClicked(UBaseButtonWidget* button);

	// 새 project 만들기 button click을 처리한다.
	UFUNCTION()
	void HandleCreateProjectClicked(UBaseButtonWidget* button);

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
};
