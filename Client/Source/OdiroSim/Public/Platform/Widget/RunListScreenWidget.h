#pragma once

#include "CoreMinimal.h"
#include "Shared/SimulationSetupTypes.h"
#include "UI/BaseWidget.h"
#include "RunListScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextInputWidget;
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

	// Run button click handler.
	UFUNCTION()
	void HandleRunClicked(UBaseButtonWidget* button);

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

	// Fixed FPS input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> FixedFpsInput;

	// Runtime time scale input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> TimeScaleInput;

	// Runtime max duration seconds input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> MaxDurationInput;

	// Episode count input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> EpisodeCountInput;

	// Base seed input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> BaseSeedInput;

	// Tip-over angle degrees input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> TipOverAngleInput;

	// Near-miss distance input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> NearMissDistanceInput;

	// Goal acceptance radius input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> GoalAcceptanceRadiusInput;

	// Container for project run rows.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RunRowListBox;

	// Start run command button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> StartRunButton;
};
