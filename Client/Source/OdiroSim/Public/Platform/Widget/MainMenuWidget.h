#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UHorizontalBox;
class UScenarioEditorLaunchSubsystem;
class UExperimentResultIterationButton;
class UFileListItemWidget;
class UPlatformAnalysisAiSubsystem;
class UProjectTemplateCardWidget;
class USimulatorLaunchSubsystem;
class UScrollBox;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
struct FPlatformAnalysisAiResponse;
struct FSimulationSetup;

// MainMenuMap에서 UMG Blueprint layout과 platform event handler를 연결하는 widget
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

	// Project mode prototype path를 설정한다. Blueprint binding이 없을 때도 테스트와 임시 UI가 같은 상태를 쓴다.
	void SetProjectPathForPrototype(const FString& projectPath);

	// Project mode prototype path를 반환한다.
	FString GetProjectPathForPrototype() const;

	// Project mode prototype에서 선택한 run id를 반환한다.
	FString GetProjectRunIdForPrototype() const;

	// ProjectOpenScreen의 template card 선택을 반영한다.
	void SelectProjectTemplateForProjectOpen(const FString& templateId);

	// 선택한 template로 project를 생성한다.
	bool CreateSelectedProjectForPrototype(
		TArray<FString>& outDiagnostics,
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

	// 현재 project path를 검증한다.
	bool ValidateSelectedProjectForPrototype(
		TArray<FString>& outDiagnostics,
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem = nullptr);

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

	// ProjectOpenScreen 입력 변경에 따라 생성/불러오기 동작 가능 여부를 갱신한다.
	UFUNCTION()
	void HandleProjectOpenInputChanged(const FText& text);

	// 입력된 경로의 user project를 검증하고 작업 화면을 연다.
	UFUNCTION()
	void HandleOpenProjectClicked();

	// 현재 입력값으로 user project를 생성한다.
	UFUNCTION()
	void HandleCreateProjectClicked();

	// Project workspace의 시나리오 편집 tab을 표시한다.
	UFUNCTION()
	void HandleShowProjectScenarioTabClicked();

	// Project workspace의 실험 현황 tab을 표시한다.
	UFUNCTION()
	void HandleShowProjectExperimentStatusTabClicked();

	// Scenario editor map으로 전환한다.
	UFUNCTION()
	void HandleOpenProjectScenarioEditorClicked();

	// 새 실험 설정 editor를 연다.
	UFUNCTION()
	void HandleAddExperimentClicked();

	// Project experiment 설정을 저장하고 simulator 실행을 시작한다.
	UFUNCTION()
	void HandleCreateExperimentFromConfigClicked();

	// Project experiment 설정 패널을 닫는다.
	UFUNCTION()
	void HandleCancelExperimentConfigClicked();

private:
	bool ValidateRequiredBindings() const;
	void BindControls();
	// WBP_MainMenu가 소유한 project mode control을 C++ event handler에 연결한다.
	void BindProjectModeControls();
	void ShowMainMenuSection(int32 sectionIndex);
	// Project opening 화면을 표시한다.
	void ShowProjectOpenScreen();
	// Project workspace 화면을 표시한다.
	void ShowProjectWorkspaceScreen();
	// Project workspace 내부 tab을 표시한다.
	void ShowProjectWorkspaceTab(int32 tabIndex);
	// 현재 Project workspace tab에 맞춰 tab button 색을 갱신한다.
	void ApplyProjectWorkspaceTabStyle(int32 activeTabIndex);
	void SyncComboBoxSelection(UComboBoxString* targetComboBox, const FString& selectedItem);
	void LoadSelectedSetup();
	// Project path 입력칸의 기본값을 채운다.
	void InitializeProjectPathInputs();
	// 마지막 ProjectOpenScreen 입력값을 user settings에서 불러온다.
	void LoadProjectOpenOptions();
	// 현재 ProjectOpenScreen 입력값을 user settings에 저장한다.
	void SaveProjectOpenOptions();
	// ProjectOpenScreen widget 값을 내부 선택 cache에 반영한다.
	void CacheProjectOpenOptionsFromWidgets();
	// Static project template 목록을 card UI에 반영한다.
	void RefreshProjectTemplateOptions();
	// ProjectOpenScreen의 생성/불러오기 button 활성 상태를 입력 경로 기준으로 갱신한다.
	void RefreshProjectOpenActions();
	// ProjectOpenScreen template card의 선택 state를 갱신한다.
	void RefreshProjectTemplateCardStates();
	// ProjectOpenScreen 옆 warning text를 갱신한다.
	void SetProjectOpenWarningText(const FString& message);
	// 선택된 user project의 현재 run 선택을 최신 상태로 맞춘다.
	void RefreshProjectRunSelection();
	// Project experiment 설정 패널 표시 상태를 바꾼다.
	void ShowProjectExperimentConfigPanel(bool bVisible);
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
	// Project template card item Widget Blueprint class를 반환한다.
	TSubclassOf<UProjectTemplateCardWidget> ResolveProjectTemplateCardWidgetClass() const;
	// Project template card item의 선택 요청을 현재 선택값에 반영한다.
	void HandleProjectTemplateCardSelected(UProjectTemplateCardWidget* cardWidget);
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
	// 선택된 user project parent directory absolute path를 반환한다.
	FString GetSelectedProjectParentFolder() const;
	// 선택된 user project directory name을 반환한다.
	FString GetSelectedProjectName() const;
	// 선택된 user project root absolute path를 반환한다.
	FString GetSelectedProjectPath() const;
	// 선택된 user project의 editable scenario.json path를 반환한다.
	FString GetSelectedProjectScenarioPath() const;
	// 선택된 project template id를 반환한다.
	FString GetSelectedProjectTemplateId() const;
	// 선택된 project run id를 반환한다.
	FString GetSelectedProjectRunId() const;
	// 선택된 project run directory absolute path를 반환한다.
	FString GetSelectedProjectRunDirectory() const;
	// 선택된 project run id와 결과 preview root를 동기화한다.
	void SetSelectedProjectRunId(const FString& runId);
	// 입력 경로의 project가 workspace로 열린 상태인지 반환한다.
	bool IsProjectOpened() const;
	USimulatorLaunchSubsystem* GetSimulatorLaunchSubsystem() const;
	UScenarioEditorLaunchSubsystem* GetScenarioEditorLaunchSubsystem() const;
	UPlatformAnalysisAiSubsystem* GetPlatformAnalysisAiSubsystem() const;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> MainContentSwitcher;

	// Project 열기 화면과 project 작업 화면을 전환한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ProjectContentSwitcher;

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

	// User project parent directory 입력 control. UI layout은 WBP_MainMenu가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectParentFolderTextBox;

	// User project directory name 입력 control. UI layout은 WBP_MainMenu가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ProjectNameTextBox;

	// Static project template card들이 들어가는 container. UI layout은 WBP_MainMenu가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ProjectTemplateCardBox;

	// User project 생성 button. UI layout은 WBP_MainMenu가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateProjectButton;

	// Existing user project를 여는 button. UI layout은 WBP_MainMenu가 소유한다.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenProjectButton;

	// Project open validation 실패를 불러오기 button 옆에 표시하는 text.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProjectOpenWarningText;

	// Project workspace 시나리오 편집 tab button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ScenarioEditTabButton;

	// Project workspace 실험 현황 tab button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExperimentStatusTabButton;

	// Scenario editor map으로 전환하는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenScenarioEditorMapButton;

	// 새 실험 run snapshot을 추가하는 button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> AddExperimentButton;

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

	// Project template card item Widget Blueprint class.
	UPROPERTY(EditDefaultsOnly, Category = "MainMenu|Project")
	TSubclassOf<UProjectTemplateCardWidget> ProjectTemplateCardWidgetClass;

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

	// Runtime에 생성한 project template card item widgets.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTemplateCardWidget>> ProjectTemplateCards;

	FString SelectedSetupPath;
	FString SelectedScenarioSetupPath;
	FString SelectedDeliveryBotSetupPath;
	FString SelectedPolicySpecJsonPath;
	FString SelectedExperimentResultRunDirectory;
	FString SelectedExperimentResultPath;
	// 선택된 user project parent directory absolute path cache.
	FString SelectedProjectParentFolder;
	// 선택된 user project directory name cache.
	FString SelectedProjectName;
	// 선택된 static project template id cache.
	FString SelectedProjectTemplateId;
	// 선택된 user project run id cache.
	FString SelectedProjectRunId;
	// Project path 검증 또는 생성이 끝나 workspace 화면을 표시할 수 있는 상태.
	bool bProjectWorkspaceOpen = false;
	bool bExperimentConfigDetailVisible = false;
	bool bExperimentResultDetailVisible = false;
};
