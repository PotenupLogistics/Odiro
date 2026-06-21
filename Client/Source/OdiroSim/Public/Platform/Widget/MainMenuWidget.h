#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UScenarioEditorLaunchSubsystem;
class UExperimentResultIterationButton;
class UFileListItemWidget;
class UPlatformAnalysisAiSubsystem;
class UProjectWorkspaceTabWidget;
class UProjectSessionSubsystem;
class UScenarioEditorRootWidget;
class USimulatorLaunchSubsystem;
class UScrollBox;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
struct FPlatformAnalysisAiResponse;
struct FSimulationSetup;

// ScenarioEditorMap에서 UMG Blueprint layout과 platform workflow event handler를 연결하는 widget.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Simulation")
	void RefreshSetupOptions();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Simulation")
	void RefreshFromSubsystem();

	// Project mode prototype에서 선택한 run id를 반환한다.
	FString GetProjectRunIdForPrototype() const;

	// WBP_MainMenu가 포함한 scenario editor root widget을 반환한다.
	UScenarioEditorRootWidget* GetScenarioEditorRootWidget() const { return ScenarioEditorRootWidget.Get(); }

	// 현재 project 입력에서 run snapshot을 생성하고 선택한다.
	bool CreateProjectRunForPrototype(
		FString& outRunId,
		TArray<FString>& outDiagnostics,
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

protected:
	UFUNCTION()
	void HandleSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleNewSetupClicked();

	UFUNCTION()
	void HandleSaveFpsClicked();

	UFUNCTION()
	void HandleSaveSetupClicked();

	UFUNCTION()
	void HandleOpenEditorClicked();

	UFUNCTION()
	void HandleNewScenarioClicked();

	void HandleScenarioRenameRequested(UFileListItemWidget* itemWidget, const FString& requestedPath);
	void HandleScenarioEditRequested(UFileListItemWidget* itemWidget);
	void HandlePolicyRenameRequested(UFileListItemWidget* itemWidget, const FString& requestedPath);
	void HandlePolicyEditRequested(UFileListItemWidget* itemWidget);
	void HandleExperimentConfigRenameRequested(UFileListItemWidget* itemWidget, const FString& requestedPath);
	void HandleExperimentConfigEditRequested(UFileListItemWidget* itemWidget);
	void HandleExperimentConfigPlayRequested(UFileListItemWidget* itemWidget);
	void HandleExperimentResultDetailsRequested(UFileListItemWidget* itemWidget);
	void HandleExperimentResultIterationButtonClicked(UExperimentResultIterationButton* buttonWidget);

	UFUNCTION()
	void HandleOpenPolicyTextEditorClicked();

	UFUNCTION()
	void HandleNewPolicyClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleShowScenarioClicked();

	UFUNCTION()
	void HandleShowPolicyClicked();

	UFUNCTION()
	void HandleShowExperimentConfigClicked();

	UFUNCTION()
	void HandleShowRunStatusClicked();

	UFUNCTION()
	void HandleShowExperimentResultClicked();

	UFUNCTION()
	void HandleExperimentResultBackClicked();

	UFUNCTION()
	void HandleExperimentConfigBackClicked();

	UFUNCTION()
	void HandleScenarioSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandlePolicyDeliveryBotSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandleExperimentScenarioSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandleExperimentDeliveryBotSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	void HandlePolicySpecSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	UFUNCTION()
	UWidget* HandleGenerateComboBoxItem(FString item);

	UFUNCTION()
	void HandleSendToAiClicked();

	// Project workspace에서 StartupMap으로 돌아간다.
	UFUNCTION()
	void HandleProjectHomeClicked();

	// Project workspace의 시나리오 편집 tab을 표시한다.
	UFUNCTION()
	void HandleShowProjectScenarioTabClicked();

	// Project workspace의 실험 현황 tab을 표시한다.
	UFUNCTION()
	void HandleShowProjectExperimentStatusTabClicked();

	// Project experiment 설정 tab을 연다.
	UFUNCTION()
	void HandleConfigureExperimentClicked();

	// Project experiment를 현재 저장된 설정으로 바로 실행한다.
	UFUNCTION()
	void HandleRunExperimentClicked();

	// Project experiment 설정을 저장하고 simulator 실행을 시작한다.
	UFUNCTION()
	void HandleCreateExperimentFromConfigClicked();

	// Project experiment 설정을 저장하고 실험 현황으로 돌아간다.
	UFUNCTION()
	void HandleSaveExperimentConfigClicked();

	// Project experiment 설정 패널을 닫는다.
	UFUNCTION()
	void HandleCancelExperimentConfigClicked();

private:
	// Project workspace content switcher가 표시하는 logical tab.
	enum class EProjectWorkspaceTabType : uint8
	{
		ScenarioEdit,
		ExperimentStatus,
		ExperimentConfig,
		ExperimentResultDetail,
	};

	bool ValidateRequiredBindings() const;
	void BindControls();
	// WBP_MainMenu가 소유한 project mode control을 C++ event handler에 연결한다.
	void BindProjectModeControls();
	void ShowMainMenuSection(int32 sectionIndex);
	// Project workspace 화면을 표시한다.
	void ShowProjectWorkspaceScreen();
	// Project workspace 내부 tab을 표시한다.
	void ShowProjectWorkspaceTab(EProjectWorkspaceTabType tabType);
	// Project workspace 임시 tab을 열고 해당 tab으로 이동한다.
	void OpenTransientProjectTab(EProjectWorkspaceTabType tabType, const FText& label);
	// Project workspace 임시 tab을 닫고 필요하면 실험 현황으로 복귀한다.
	void CloseTransientProjectTab(EProjectWorkspaceTabType tabType);
	// 현재 Project workspace tab에 맞춰 tab widget state를 갱신한다.
	void ApplyProjectWorkspaceTabState(EProjectWorkspaceTabType activeTabType);
	void HandleProjectWorkspaceTabSelected(UProjectWorkspaceTabWidget* tabWidget);
	void HandleProjectWorkspaceTabCloseRequested(UProjectWorkspaceTabWidget* tabWidget);
	void SyncComboBoxSelection(UComboBoxString* targetComboBox, const FString& selectedItem);
	void LoadSelectedSetup();
	// 선택된 user project의 현재 run 선택을 최신 상태로 맞춘다.
	void RefreshProjectRunSelection();
	// Project experiment 설정 패널 표시 상태를 바꾼다.
	void ShowProjectExperimentConfigPanel(bool bVisible);
	// Project experiment run 생성을 검증하고 simulator 실행을 시작한다.
	bool StartProjectExperimentRun(
		TArray<FString>& outDiagnostics,
		FString& outRunId);
	// user project setting.json 값을 Project experiment 설정 패널에 채운다.
	bool LoadProjectExperimentSettingIntoPanel(TArray<FString>& outDiagnostics);
	// Project experiment 설정 패널 값을 user project setting.json에 저장한다.
	bool SaveProjectExperimentSettingFromPanel(TArray<FString>& outDiagnostics);
	// Project experiment 설정 패널 warning text를 갱신한다.
	void SetProjectExperimentConfigWarningText(const FString& message);
	void RefreshScenarioList();
	void RefreshPolicyList();
	void RefreshExperimentConfigList();
	void RefreshExperimentResultList();
	void RefreshExperimentResultIterationList();
	void ApplyNewSetupDefaults(const FString& setupPath);
	void SetExperimentConfigDetailVisible(bool bVisible);
	void SetExperimentResultDetailVisible(bool bVisible);
	void SetSelectedSetupPath(const FString& setupPath);
	void SetSelectedScenarioSetupPath(const FString& scenarioSetupPath);
	void SetSelectedDeliveryBotSetupPath(const FString& deliveryBotSetupPath);
	void SetSelectedPolicySpecPath(const FString& policySpecPath);
	void SetSelectedExperimentResultRunDirectory(const FString& runDirectory);
	void SetSelectedExperimentResultPath(const FString& reportPath);
	void ClearExperimentResultIterationWidgets();
	bool CreateScenarioFileFromTemplate(const FString& scenarioSetupPath);
	bool OpenScenarioInEditor(const FString& scenarioSetupPath);
	bool BuildSimulationSetupFromControls(
		const FSimulationSetup& baseSetup,
		const FString& runQueuePath,
		FSimulationSetup& outSetup,
		TArray<FString>& outDiagnostics) const;
	TSubclassOf<UFileListItemWidget> ResolveFileListItemWidgetClass() const;
	void HandleRunInfoChanged(const struct FSimulatorRunInfo& runInfo);
	void HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response);
	void UpdateStatusText(const FString& extraMessage = FString());
	void UpdateReportAndLogText();
	void SetDiagnosticsText(const FString& message);
	// Project path가 입력된 동안 project mode가 legacy UI 흐름보다 우선한다.
	bool IsProjectModeSelected() const;
	// 경로가 현재 선택된 user project의 run directory인지 확인한다.
	bool IsProjectRunDirectory(const FString& runDirectory) const;
	FString GetSelectedSetupPath() const;
	FString GetSelectedScenarioSetupPath() const;
	FString GetSelectedDeliveryBotSetupPath() const;
	FString GetSelectedPolicySpecPath() const;
	// 선택된 user project root absolute path를 반환한다.
	FString GetSelectedProjectPath() const;
	// 선택된 user project의 editable scenario.json path를 반환한다.
	FString GetSelectedProjectScenarioPath() const;
	// 선택된 project run id를 반환한다.
	FString GetSelectedProjectRunId() const;
	// 선택된 project run directory absolute path를 반환한다.
	FString GetSelectedProjectRunDirectory() const;
	// 선택된 project run id와 결과 preview root를 동기화한다.
	void SetSelectedProjectRunId(const FString& runId);
	// Active project session이 있어 workspace가 project mode로 동작할 수 있는지 반환한다.
	bool IsProjectOpened() const;
	UProjectSessionSubsystem* GetProjectSessionSubsystem() const;
	USimulatorLaunchSubsystem* GetSimulatorLaunchSubsystem() const;
	UScenarioEditorLaunchSubsystem* GetScenarioEditorLaunchSubsystem() const;
	UPlatformAnalysisAiSubsystem* GetPlatformAnalysisAiSubsystem() const;
	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();
	UWidget* ResolveInputModeFocusWidget() const;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> MainContentSwitcher;

	// ScenarioEditorMap에서 WBP_MainMenu 내부에 포함되는 editor root UI.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScenarioEditorRootWidget> ScenarioEditorRootWidget;

	// MainMenu controls 영역만 editor UI input focus로 요청하기 위한 optional panel.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MainMenuInputModePanel;

	// MainMenu의 실제 root workspace 화면.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProjectWorkspaceScreen;

	// Project 작업 화면의 상단 tab content를 전환한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ProjectWorkspaceSwitcher;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActiveSectionTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ScenarioNavButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PolicyNavButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExperimentConfigNavButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RunStatusNavButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExperimentResultNavButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScenarioListScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> PolicyListScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExperimentConfigSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExperimentConfigDetailSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ExperimentConfigListScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExperimentResultSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExperimentResultDetailSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ExperimentResultListScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ExperimentResultIterationScrollBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExperimentConfigBackButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExperimentResultBackButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ScenarioSetupComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ScenarioSetupPathTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> PolicyDeliveryBotSetupComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> SetupComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExperimentConfigSectionTitleText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SetupLabel;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> MapIdTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RunIdTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> FixedStepFpsTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RunCountTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> MeasurementLogEnabledCheckBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> MeasurementOutputDirectoryTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> MeasurementFilePrefixTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> FlushIntervalTicksTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ReportOutputDirectoryTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> StatusOutputPathTextBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ExperimentScenarioSetupComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> DeliveryBotSetupComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> PolicySpecComboBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PairRow;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RunSettingsRow;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoadButtonOverlay;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadButtonLabel;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewSetupButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NewSetupButtonOverlay;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveFpsButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveSetupButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenEditorButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewScenarioButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenPolicyTextEditorButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewPolicyButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SendToAiButton;

	// Project workspace에서 StartupMap으로 돌아가는 home button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> HomeButton;

	// Project workspace 시나리오 tab widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectWorkspaceTabWidget> ScenarioEditTab;

	// Project workspace 실험 현황 tab widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectWorkspaceTabWidget> ExperimentStatusTab;

	// Project workspace 실험 설정 임시 tab widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectWorkspaceTabWidget> ExperimentConfigTab;

	// Project workspace 분석 상세 임시 tab widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectWorkspaceTabWidget> ExperimentResultDetailTab;

	// Project experiment 설정 tab을 여는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfigureExperimentButton;

	// Project experiment를 현재 설정으로 바로 실행하는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RunExperimentButton;

	// Project experiment 설정 editor panel. UI layout은 WBP_MainMenu가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProjectExperimentConfigPanel;

	// Project experiment target map id 입력 control.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectExperimentMapIdTextBox;

	// Project experiment fixed FPS 입력 control.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectExperimentFixedFpsTextBox;

	// Project experiment episode count 입력 control.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectExperimentEpisodeCountTextBox;

	// Project experiment base seed 입력 control.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectExperimentBaseSeedTextBox;

	// Project experiment 설정 저장 후 simulator 실행을 시작하는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateExperimentConfigButton;

	// Project experiment 설정만 저장하는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveExperimentConfigButton;

	// Project experiment 설정 editor를 닫는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelExperimentConfigButton;

	// Project experiment 설정 validation 메시지.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProjectExperimentConfigWarningText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AiAnalysisTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReportTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LogPreviewTextBlock;

	FString CurrentPreviewReportPath;
	FString CurrentPreviewLogPath;
	FTimerHandle RefreshTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "MainMenu|List")
	TSubclassOf<UFileListItemWidget> FileListItemWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFileListItemWidget>> ScenarioListItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFileListItemWidget>> PolicyListItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFileListItemWidget>> ExperimentConfigListItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFileListItemWidget>> ExperimentResultListItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UExperimentResultIterationButton>> ExperimentResultIterationButtons;

	// Scenario editor controller에 input mode를 요청할 때 사용한 focus widget.
	UPROPERTY(Transient)
	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;

	FString SelectedSetupPath;
	FString SelectedScenarioSetupPath;
	FString SelectedDeliveryBotSetupPath;
	FString SelectedPolicySpecJsonPath;
	FString SelectedExperimentResultRunDirectory;
	FString SelectedExperimentResultPath;
	// 선택된 user project run id cache.
	FString SelectedProjectRunId;
	bool bExperimentConfigDetailVisible = false;
	// Project experiment 설정 임시 tab이 현재 표시 중인지 나타낸다.
	bool bProjectExperimentConfigTabOpen = false;
	// Project experiment 분석 상세 임시 tab이 현재 표시 중인지 나타낸다.
	bool bProjectExperimentResultDetailTabOpen = false;
	// Project workspace switcher의 현재 logical tab.
	EProjectWorkspaceTabType ActiveProjectWorkspaceTab = EProjectWorkspaceTabType::ScenarioEdit;
};
