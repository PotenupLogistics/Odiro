#pragma once

#include "CoreMinimal.h"
#include "Shared/SimulationSetupTypes.h"
#include "UI/BaseWidget.h"
#include "RunListScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseSliderComboWidget;
class UBaseTextInputWidget;
class UBaseTextWidget;
class UExperimentConfigViewModel;
class UProjectExperimentRunRowWidget;
class UProjectWorkspaceViewModel;
class UVerticalBox;

class URunListScreenWidget;

// Run detail navigation request emitted by the experiment list surface.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRunListScreenRunDetailRequested,
	URunListScreenWidget*,
	Screen,
	const FString&,
	RunId);

// Platform experiment list and configuration screen.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URunListScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Binds command buttons and loads active project experiment data.
	virtual void NativeConstruct() override;

	// Releases command button and row bindings.
	virtual void NativeDestruct() override;

	// Refreshes experiment settings and run rows from ViewModels.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunList")
	void RefreshFromViewModels();

	// Saves settings and starts a new project run.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunList")
	bool StartNewRun();

	// Requests AI analysis for the selected run and asks root to show detail.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunList")
	bool RequestAnalysisForSelectedRun();

	// Asks root to show the currently selected run detail.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunList")
	bool OpenSelectedRunDetail();

	// Run detail navigation event.
	UPROPERTY(BlueprintAssignable, Category = "Platform|RunList|Events")
	FRunListScreenRunDetailRequested OnRunDetailRequested;

private:
	// Resolves and caches workspace ViewModel.
	UProjectWorkspaceViewModel* ResolveWorkspaceViewModel();

	// Resolves and caches experiment config ViewModel.
	UExperimentConfigViewModel* ResolveExperimentConfigViewModel();

	// Commits visible experiment setting inputs into the ViewModel and saves setting.json.
	bool SaveExperimentSettings();

	// Rebuilds run row widgets from the workspace ViewModel items.
	void RebuildRunRows();

	// Returns the configured run row class.
	TSubclassOf<UProjectExperimentRunRowWidget> ResolveRunRowWidgetClass() const;

	// Reads a run state from runs/<id>/status.json when available.
	static ESimulationRunState ResolveRunState(const FString& runDirectory);

	// Removes row widgets and native event subscriptions.
	void ClearRunRows();

	// Refresh button click handler.
	UFUNCTION()
	void HandleRefreshClicked(UBaseButtonWidget* button);

	// Run button click handler.
	UFUNCTION()
	void HandleRunClicked(UBaseButtonWidget* button);

	// Analysis button click handler.
	UFUNCTION()
	void HandleAnalyzeClicked(UBaseButtonWidget* button);

	// Detail button click handler.
	UFUNCTION()
	void HandleOpenDetailClicked(UBaseButtonWidget* button);

	// Row analysis click handler.
	void HandleRunRowAnalyzeRequested(UProjectExperimentRunRowWidget* rowWidget);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectWorkspaceViewModel> ProjectWorkspaceViewModel;

	// Experiment settings ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UExperimentConfigViewModel> ExperimentConfigViewModel;

	// Runtime run row widgets owned by this screen.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectExperimentRunRowWidget>> RunRows;

	// Run row Widget Blueprint class used for dynamic rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectExperimentRunRowWidget> RunRowWidgetClass;

	// Active project path display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> ProjectPathText;

	// Selected run id display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SelectedRunText;

	// Status and diagnostics display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> StatusText;

	// Fixed FPS input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> FixedFpsInput;

	// Fixed FPS slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> FixedFpsSliderCombo;

	// Episode count input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> EpisodeCountInput;

	// Base seed input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> BaseSeedInput;

	// Container for project run rows.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RunRowListBox;

	// Refresh command button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> RefreshButton;

	// Start run command button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> StartRunButton;

	// Analyze selected run command button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> AnalyzeRunButton;

	// Open selected run detail command button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> OpenRunDetailButton;
};
