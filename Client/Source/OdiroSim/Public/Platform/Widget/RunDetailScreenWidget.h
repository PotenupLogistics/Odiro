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
class UProjectEpisodeReplayInterestRegionStripWidget;
class UProjectEpisodeReplayViewerWidget;
class UProjectWorkspaceViewModel;
class USizeBox;
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

	// Opens the first replay-capable episode when this run does not already have a loaded replay.
	void OpenInitialEpisodeReplay();

	// Opens replay for one episode card and mirrors the selected episode header on success.
	bool OpenEpisodeReplayCard(UProjectEpisodeReplayCardWidget* cardWidget);

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

	// Moves the embedded replay viewer when its fullscreen state changes.
	void HandleReplayFullscreenChanged(
		UProjectEpisodeReplayViewerWidget* replayViewer,
		bool bFullscreen);

	// Attaches the replay viewer to the authored normal or fullscreen host.
	void AttachReplayViewerToHost(USizeBox* targetHost);

	// Restores the replay viewer to its authored normal host.
	void RestoreReplayViewerToNormalHost();

	// Finds the RunDetail-owned replay interest-region strip by supported WBP names.
	void ResolveReplayInterestRegionStrip();

	// Connects the RunDetail-owned interest strip to the embedded replay viewer.
	void ApplyReplayInterestRegionStripToViewer();

	// Updates the RunDetail header with the selected replay episode id.
	void SetReplayEpisodeNumberText(const FString& episodeId);

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

	// Run id whose replay is currently loaded in the embedded viewer.
	UPROPERTY(Transient)
	FString LoadedReplayRunId;

	// Episode directory currently loaded in the embedded replay viewer.
	UPROPERTY(Transient)
	FString LoadedReplayEpisodeDirectory;

	// Episode card Widget Blueprint class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunDetail", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectEpisodeReplayCardWidget> EpisodeCardWidgetClass;

	// AI suggestion row Widget Blueprint class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunDetail", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectAiSuggestionRowWidget> SuggestionRowWidgetClass;

	// Runtime WrapBox slot spacing for episode replay cards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RunDetail|Layout", meta = (AllowPrivateAccess = "true"))
	FMargin EpisodeReplayCardPadding = FMargin(0.0f, 0.0f, 8.0f, 8.0f);

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

	// Success rate metric sublabel display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SuccessMetricSub;

	// Collision count metric display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTextWidget> CollisionCountText;

	// Collision count metric sublabel display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CollisionMetricSub;

	// Timeout count metric display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeoutMetricValue;

	// Timeout count metric sublabel display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeoutMetricSub;

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

	// Normal replay viewer host inside the run detail content column.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> ReplayViewerSize;

	// Fullscreen replay host layered above run detail content.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> ReplayFullscreenHost;

	// Selected replay episode id label owned by the RunDetail header.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayEpisodeNumber;

	// Embedded replay viewer owned by a replay host widget.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectEpisodeReplayViewerWidget> ProjectEpisodeReplayViewer;

	// Parent-owned replay interest strip placed below the embedded replay panel.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectEpisodeReplayInterestRegionStripWidget> ReplayInterestRegionStrip;
};
