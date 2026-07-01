#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "RunDetailScreenWidget.generated.h"

class UBaseTextWidget;
class UBaseButtonWidget;
class UExperimentResultInsightViewModel;
class UExperimentResultSuggestionViewModel;
class UExperimentResultViewModel;
class UOdiroListItemViewModel;
class UProjectAiSuggestionRowWidget;
class UProjectEpisodeReplayCardWidget;
class UProjectEpisodeReplayViewerWidget;
class UProjectWorkspaceViewModel;
class UTextBlock;
class UVerticalBox;
class UWrapBox;
struct FPlatformAnalysisAiResponse;

// Platform project run detail screen with dashboard metrics and embedded replay.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URunDetailScreenWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Binds command controls and analysis completion refresh.
	virtual void NativeConstruct() override;

	// Releases replay card event bindings.
	virtual void NativeDestruct() override;

	// Loads and displays the requested run id.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunDetail")
	void ShowRun(const FString& runId);

	// Refreshes dashboard labels, episode cards, suggestions, and replay state.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunDetail")
	void RefreshFromViewModels();

	// Requests AI analysis for the active run.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunDetail")
	bool RequestAiAnalysis();

	// Clears replay playback state.
	UFUNCTION(BlueprintCallable, Category = "Platform|RunDetail")
	void ResetReplay();

	// Returns the run id currently shown by this screen.
	UFUNCTION(BlueprintPure, Category = "Platform|RunDetail")
	FString GetDisplayedRunId() const { return DisplayedRunId; }

private:
	// Resolves and caches workspace ViewModel.
	UProjectWorkspaceViewModel* ResolveWorkspaceViewModel();

	// Resolves and caches experiment result ViewModel.
	UExperimentResultViewModel* ResolveExperimentResultViewModel();

	// Rebuilds episode replay card widgets.
	void RebuildEpisodeCards();

	// Rebuilds AI analysis row widgets.
	void RebuildAnalysisRows();

	// Clears episode card widgets and event subscriptions.
	void ClearEpisodeCards();

	// Clears AI analysis row widgets.
	void ClearAnalysisRows();

	// Returns the configured episode card class.
	TSubclassOf<UProjectEpisodeReplayCardWidget> ResolveEpisodeCardWidgetClass() const;

	// Returns the configured suggestion row class.
	TSubclassOf<UProjectAiSuggestionRowWidget> ResolveSuggestionRowWidgetClass() const;

	// Adds an insight row to its dedicated container or the existing suggestion fallback.
	void AddInsightRow(const UExperimentResultInsightViewModel* insightItem);

	// Adds a suggestion row to the suggestion container.
	void AddSuggestionRow(const UExperimentResultSuggestionViewModel* suggestionItem);

	// Adds a warning row to its dedicated container or the existing suggestion fallback.
	void AddWarningRow(const UOdiroListItemViewModel* warningItem);

	// Creates one configured analysis row in a WBP-owned vertical container.
	void AddSuggestionRowToContainer(
		const UExperimentResultSuggestionViewModel* suggestionItem,
		UVerticalBox* container);

	// Opens replay when an episode card is clicked.
	void HandleEpisodeReplayRequested(UProjectEpisodeReplayCardWidget* cardWidget);

	// Analysis button click handler.
	UFUNCTION()
	void HandleRequestAiAnalysisClicked(UBaseButtonWidget* button);

	// Refreshes the panel after project-run analysis writes its review JSON.
	void HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UProjectWorkspaceViewModel> ProjectWorkspaceViewModel;

	// Run result ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UExperimentResultViewModel> ExperimentResultViewModel;

	// Runtime episode cards owned by this screen.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectEpisodeReplayCardWidget>> EpisodeCards;

	// Runtime AI analysis rows owned by this screen.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectAiSuggestionRowWidget>> AnalysisRows;

	// Run id currently shown by this detail screen.
	UPROPERTY(Transient)
	FString DisplayedRunId;

	// Episode card Widget Blueprint class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunDetail", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectEpisodeReplayCardWidget> EpisodeCardWidgetClass;

	// AI suggestion row Widget Blueprint class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunDetail", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectAiSuggestionRowWidget> SuggestionRowWidgetClass;

	// Run id title display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> RunIdText;

	// Run directory display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> RunDirectoryText;

	// Total duration metric display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> TotalDurationText;

	// Small total duration sublabel below average duration.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DurationMetricSub;

	// Success rate metric display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> SuccessRateText;

	// Collision count metric display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> CollisionCountText;

	// AI summary display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> AiSummaryText;

	// Detail status display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> StatusText;

	// Request AI analysis command button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> RequestAiAnalysisButton;

	// Episode replay card container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> EpisodeReplayCardWrapBox;

	// Suggestion row container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> AiSuggestionListBox;

	// Optional insight row container; falls back to the suggestion container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> AiInsightListBox;

	// Optional warning row container; falls back to the suggestion container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> AiWarningListBox;

	// Embedded replay viewer owned by a replay host widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectEpisodeReplayViewerWidget> ProjectEpisodeReplayViewer;
};
