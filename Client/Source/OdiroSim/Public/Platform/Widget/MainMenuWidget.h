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
class USimulatorLaunchSubsystem;
class UScrollBox;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
struct FPlatformAnalysisAiResponse;

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

private:
	bool ValidateRequiredBindings() const;
	void BindControls();
	void ShowMainMenuSection(int32 sectionIndex);
	void SyncComboBoxSelection(UComboBoxString* targetComboBox, const FString& selectedItem);
	void LoadSelectedSetup();
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
	TSubclassOf<UFileListItemWidget> ResolveFileListItemWidgetClass() const;
	void HandleRunInfoChanged(const struct FSimulatorRunInfo& runInfo);
	void HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response);
	void UpdateStatusText(const FString& extraMessage = FString());
	void UpdateReportAndLogText();
	void SetDiagnosticsText(const FString& message);
	FString GetSelectedSetupPath() const;
	FString GetSelectedScenarioSetupPath() const;
	FString GetSelectedDeliveryBotSetupPath() const;
	FString GetSelectedPolicySpecPath() const;
	USimulatorLaunchSubsystem* GetSimulatorLaunchSubsystem() const;
	UScenarioEditorLaunchSubsystem* GetScenarioEditorLaunchSubsystem() const;
	UPlatformAnalysisAiSubsystem* GetPlatformAnalysisAiSubsystem() const;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> MainContentSwitcher;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActiveSectionTextBlock;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> ScenarioNavButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> PolicyNavButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> ExperimentConfigNavButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RunStatusNavButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> ExperimentResultNavButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UScrollBox> ScenarioListScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UScrollBox> PolicyListScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidget> ExperimentConfigSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidget> ExperimentConfigDetailSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UScrollBox> ExperimentConfigListScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidget> ExperimentResultSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UWidget> ExperimentResultDetailSectionBoxScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UScrollBox> ExperimentResultListScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UScrollBox> ExperimentResultIterationScrollBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> ExperimentConfigBackButton;

	UPROPERTY(Transient, meta = (BindWidget))
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

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> FixedStepFpsTextBox;

	UPROPERTY(Transient, meta = (BindWidget))
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

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UComboBoxString> ExperimentScenarioSetupComboBox;

	UPROPERTY(Transient, meta = (BindWidget))
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

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> NewSetupButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NewSetupButtonOverlay;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveFpsButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> SaveSetupButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenEditorButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> NewScenarioButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenPolicyTextEditorButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> NewPolicyButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SendToAiButton;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AiAnalysisTextBlock;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReportTextBlock;

	UPROPERTY(Transient, meta = (BindWidget))
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

	FString SelectedSetupPath;
	FString SelectedScenarioSetupPath;
	FString SelectedDeliveryBotSetupPath;
	FString SelectedPolicySpecJsonPath;
	FString SelectedExperimentResultRunDirectory;
	FString SelectedExperimentResultPath;
	bool bExperimentConfigDetailVisible = false;
	bool bExperimentResultDetailVisible = false;
};
