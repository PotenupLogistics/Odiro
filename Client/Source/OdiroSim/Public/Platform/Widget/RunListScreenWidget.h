#pragma once

#include "CoreMinimal.h"
#include "Shared/SimulationSetupTypes.h"
#include "TimerManager.h"
#include "UI/BaseWidget.h"
#include "RunListScreenWidget.generated.h"

class UBaseButtonWidget;
class UBaseTextInputWidget;
class UExperimentConfigViewModel;
class UProjectExperimentRunRowWidget;
class UProjectWorkspaceViewModel;
class UTextBlock;
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

	// Refreshes only the run result list, preserving visible experiment setting edits.
	void RefreshRunResults();

	// Starts periodic run result polling while this screen is constructed.
	void StartRunResultsPolling();

	// Stops periodic run result polling before row widgets are released.
	void StopRunResultsPolling();

	// Timer callback that refreshes run result artifacts from disk.
	void HandleRunResultsPollingTick();

	// Commits visible experiment setting inputs into the ViewModel and saves setting.json.
	bool SaveExperimentSettings();

	// Rebuilds run row widgets from the workspace ViewModel items.
	void RebuildRunRows(bool bForceRebuild = false);

	// Returns the configured run row class.
	TSubclassOf<UProjectExperimentRunRowWidget> ResolveRunRowWidgetClass() const;

	// Resolves run state through the Platform file adapter.
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

	// Timer handle for active run result list polling.
	FTimerHandle RunResultsPollingTimerHandle;

	// Last rendered run result signature used to skip redundant row rebuilds.
	FString RenderedRunResultsSignature;

	// Run row Widget Blueprint class used for dynamic rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectExperimentRunRowWidget> RunRowWidgetClass;

	// Seconds between run result list refreshes while this screen is visible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float RunResultsPollingIntervalSeconds = 2.5f;

	// Text used when a run metric has not been loaded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText EmptyRunMetricText = NSLOCTEXT("RunListScreen", "EmptyRunMetricText", "-");

	// Format used for run success rate. Tokens: {Percent}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText SuccessRateFormat = NSLOCTEXT("RunListScreen", "SuccessRateFormat", "{Percent}%");

	// Decimal places used for run success rate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "6", UIMin = "0", UIMax = "6"))
	int32 SuccessRateDisplayDecimals = 0;

	// Format used for total run duration. Tokens: {Seconds}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText TotalDurationFormat = NSLOCTEXT("RunListScreen", "TotalDurationFormat", "{Seconds} s");

	// Decimal places used for total run duration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "6", UIMin = "0", UIMax = "6"))
	int32 TotalDurationDisplayDecimals = 1;

	// Format used for running progress count. Tokens: {Completed}, {Total}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressCountFormat = NSLOCTEXT("RunListScreen", "ProgressCountFormat", "{Completed}/{Total}");

	// Progress label for error state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressErrorText = NSLOCTEXT("RunListScreen", "ProgressStatusError", "오류");

	// Progress label for active run preparation before simulator running status is confirmed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressStartingText = NSLOCTEXT("RunListScreen", "ProgressStatusStarting", "준비");

	// Progress label for canceled state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressCanceledText = NSLOCTEXT("RunListScreen", "ProgressStatusCanceled", "중단");

	// Progress label for completed state when count text is unavailable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressCompletedText = NSLOCTEXT("RunListScreen", "ProgressStatusCompletedFallback", "완료");

	// Progress label for failed state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressFailedText = NSLOCTEXT("RunListScreen", "ProgressStatusFailedFallback", "실패");

	// Progress label for pending state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Text", meta = (AllowPrivateAccess = "true"))
	FText ProgressPendingText = NSLOCTEXT("RunListScreen", "ProgressStatusPending", "대기");

	// Validation message for integer fields. Tokens: {Field}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Validation", meta = (AllowPrivateAccess = "true"))
	FText IntegerValidationErrorFormat = NSLOCTEXT("RunListScreen", "IntegerValidationErrorFormat", "{Field}은 정수여야 합니다.");

	// Validation message for numeric fields. Tokens: {Field}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Validation", meta = (AllowPrivateAccess = "true"))
	FText NumberValidationErrorFormat = NSLOCTEXT("RunListScreen", "NumberValidationErrorFormat", "{Field}은 숫자여야 합니다.");

	// Validation message for values outside the WBP-authored input range. Tokens: {Field}, {Min}, {Max}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunList|Validation", meta = (AllowPrivateAccess = "true"))
	FText RangeValidationErrorFormat = NSLOCTEXT("RunListScreen", "RangeValidationErrorFormat", "{Field}은 {Min}~{Max} 범위여야 합니다.");

	// Fixed FPS label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FixedFpsLabel;

	// Fixed FPS input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> FixedFpsInput;

	// Runtime time scale label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeScaleLabel;

	// Runtime time scale input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> TimeScaleInput;

	// Runtime max duration seconds label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxDurationLabel;

	// Runtime max duration seconds input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> MaxDurationInput;

	// Episode count label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EpisodeCountLabel;

	// Episode count input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> EpisodeCountInput;

	// Base seed label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BaseSeedLabel;

	// Base seed input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> BaseSeedInput;

	// Tip-over angle degrees label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TipOverAngleLabel;

	// Tip-over angle degrees input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> TipOverAngleInput;

	// Near-miss distance label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NearMissDistanceLabel;

	// Near-miss distance input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextInputWidget> NearMissDistanceInput;

	// Goal acceptance radius label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GoalAcceptanceRadiusLabel;

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
