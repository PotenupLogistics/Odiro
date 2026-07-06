#include "Platform/Widget/RunDetailScreenWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Misc/Paths.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/Widget/ProjectAiSuggestionRowWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayCardWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayInterestRegionStripWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayViewerWidget.h"
#include "UI/BaseButtonWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogRunDetailScreenWidget, Log, All);

void URunDetailScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RequestAiAnalysisButton)
	{
		RequestAiAnalysisButton->OnBaseClicked.RemoveDynamic(
			this,
			&URunDetailScreenWidget::HandleRequestAiAnalysisClicked);
		RequestAiAnalysisButton->OnBaseClicked.AddDynamic(
			this,
			&URunDetailScreenWidget::HandleRequestAiAnalysisClicked);
	}

	if (UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this))
	{
		platformUiSubsystem->OnAnalysisCompleted.RemoveAll(this);
		platformUiSubsystem->OnAnalysisCompleted.AddUObject(
			this,
			&URunDetailScreenWidget::HandleAnalysisCompleted);
	}

	if (ProjectEpisodeReplayViewer)
	{
		ProjectEpisodeReplayViewer->OnReplayFullscreenChanged.RemoveAll(this);
		ProjectEpisodeReplayViewer->OnReplayFullscreenChanged.AddUObject(
			this,
			&URunDetailScreenWidget::HandleReplayFullscreenChanged);
	}

	RestoreReplayViewerToNormalHost();
	ResolveReplayInterestRegionStrip();
	ApplyReplayInterestRegionStripToViewer();
	SetReplayEpisodeNumberText(FString());
}

void URunDetailScreenWidget::NativeDestruct()
{
	if (ProjectEpisodeReplayViewer)
	{
		ProjectEpisodeReplayViewer->OnReplayFullscreenChanged.RemoveAll(this);
		RestoreReplayViewerToNormalHost();
		ProjectEpisodeReplayViewer->SetExternalReplayInterestRegionStrip(nullptr);
	}

	if (RequestAiAnalysisButton)
	{
		RequestAiAnalysisButton->OnBaseClicked.RemoveDynamic(
			this,
			&URunDetailScreenWidget::HandleRequestAiAnalysisClicked);
	}

	if (UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this))
	{
		platformUiSubsystem->OnAnalysisCompleted.RemoveAll(this);
	}

	ClearEpisodeCards();
	ClearAnalysisRows();
	Super::NativeDestruct();
}

void URunDetailScreenWidget::ShowRun(const FString& runId)
{
	DisplayedRunId = runId.TrimStartAndEnd();
	if (UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel())
	{
		if (!DisplayedRunId.IsEmpty())
		{
			workspaceViewModel->SelectRun(DisplayedRunId);
		}
		else
		{
			DisplayedRunId = workspaceViewModel->GetSelectedRunId();
		}
	}

	RefreshFromViewModels();
}

void URunDetailScreenWidget::RefreshFromViewModels()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	UExperimentResultViewModel* resultViewModel = ResolveExperimentResultViewModel();
	if (!workspaceViewModel || !resultViewModel)
	{
		if (StatusText)
		{
			StatusText->SetText(NSLOCTEXT("OdiroPlatform", "RunDetailViewModelMissing", "Run detail ViewModel 없음"));
		}
		return;
	}

	const FString selectedRunDirectory = workspaceViewModel->GetSelectedRunDirectory();
	const bool bLoaded = resultViewModel->LoadRunDirectory(selectedRunDirectory);
	const FProjectRunResultDashboardData dashboardData = resultViewModel->GetDashboardData();

	const auto formatOneDecimalMetricValue = [](const double value)
	{
		return FString::Printf(TEXT("%.1f"), value);
	};
	const auto formatPercentMetricValue = [](const int32 count, const int32 total)
	{
		return total > 0
			? FString::Printf(TEXT("%.0f"), 100.0 * static_cast<double>(count) / total)
			: FString(TEXT("-"));
	};
	const auto formatSecondsTimeMetricValue = [](const double seconds)
	{
		const int32 clampedSeconds = FMath::Max(0, FMath::RoundToInt(seconds));
		return FString::Printf(TEXT("%02d:%02d"), clampedSeconds / 60, clampedSeconds % 60);
	};
	const auto formatFractionMetricValue = [](const int32 count, const int32 total)
	{
		return total > 0
			? FString::Printf(TEXT("%d / %d"), count, total)
			: FString(TEXT("-"));
	};
	const auto isTimeoutEpisode = [](const FProjectRunEpisodeDashboardItem& episodeItem)
	{
		return episodeItem.TerminalReason.Equals(TEXT("Timeout"), ESearchCase::IgnoreCase)
			|| episodeItem.Outcome.Equals(TEXT("Timeout"), ESearchCase::IgnoreCase)
			|| episodeItem.Outcome.Equals(TEXT("TimedOut"), ESearchCase::IgnoreCase);
	};
	int32 timeoutEpisodeCount = 0;
	for (const FProjectRunEpisodeDashboardItem& episodeItem : dashboardData.Episodes)
	{
		if (isTimeoutEpisode(episodeItem))
		{
			++timeoutEpisodeCount;
		}
	}

	if (RunIdText)
	{
		RunIdText->SetText(FText::FromString(workspaceViewModel->GetSelectedRunId()));
	}
	if (RunDirectoryText)
	{
		RunDirectoryText->SetText(FText::FromString(selectedRunDirectory));
	}
	if (TotalDurationText)
	{
		TotalDurationText->SetText(FText::FromString(
			dashboardData.EpisodeCount > 0
				? formatOneDecimalMetricValue(dashboardData.TotalDurationSeconds / dashboardData.EpisodeCount)
				: FString(TEXT("-"))));
	}
	if (DurationMetricSub)
	{
		DurationMetricSub->SetText(FText::FromString(
			dashboardData.EpisodeCount > 0
				? formatSecondsTimeMetricValue(dashboardData.TotalDurationSeconds)
				: FString(TEXT("-"))));
	}
	if (SuccessRateText)
	{
		SuccessRateText->SetText(FText::FromString(formatPercentMetricValue(
			dashboardData.SuccessCount,
			dashboardData.EpisodeCount)));
	}
	if (SuccessMetricSub)
	{
		SuccessMetricSub->SetText(FText::FromString(formatFractionMetricValue(
			dashboardData.SuccessCount,
			dashboardData.EpisodeCount)));
	}
	if (CollisionCountText)
	{
		CollisionCountText->SetText(FText::FromString(
			dashboardData.EpisodeCount > 0
				? formatOneDecimalMetricValue(
					static_cast<double>(dashboardData.CollisionCount) / dashboardData.EpisodeCount)
				: FString(TEXT("-"))));
	}
	if (CollisionMetricSub)
	{
		CollisionMetricSub->SetText(FText::FromString(
			dashboardData.EpisodeCount > 0
				? FString::FromInt(dashboardData.CollisionCount)
				: FString(TEXT("-"))));
	}
	if (TimeoutMetricValue)
	{
		TimeoutMetricValue->SetText(FText::FromString(
			dashboardData.EpisodeCount > 0
				? FString::FromInt(timeoutEpisodeCount)
				: FString(TEXT("-"))));
	}
	if (TimeoutMetricSub)
	{
		TimeoutMetricSub->SetText(FText::FromString(formatFractionMetricValue(
			timeoutEpisodeCount,
			dashboardData.EpisodeCount)));
	}
	if (AiSummaryText)
	{
		AiSummaryText->SetText(FText::FromString(resultViewModel->GetAiSummaryText()));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(
			bLoaded ? workspaceViewModel->GetStatusText() : resultViewModel->GetDiagnosticsText()));
	}

	RebuildEpisodeCards();
	OpenInitialEpisodeReplay();
	RebuildAnalysisRows();
}

bool URunDetailScreenWidget::RequestAiAnalysis()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	UExperimentResultViewModel* resultViewModel = ResolveExperimentResultViewModel();
	if (!workspaceViewModel || !resultViewModel)
	{
		return false;
	}

	const FString requestedRunId = DisplayedRunId.IsEmpty()
		? workspaceViewModel->GetSelectedRunId()
		: DisplayedRunId;
	if (!requestedRunId.IsEmpty()
		&& !requestedRunId.Equals(workspaceViewModel->GetSelectedRunId(), ESearchCase::IgnoreCase))
	{
		workspaceViewModel->SelectRun(requestedRunId);
	}

	const bool bRequested = resultViewModel->RequestAiAnalysis(
		workspaceViewModel->GetActiveProjectPath(),
		requestedRunId);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(resultViewModel->GetDiagnosticsText()));
	}
	return bRequested;
}

void URunDetailScreenWidget::ResetReplay()
{
	if (ProjectEpisodeReplayViewer)
	{
		ProjectEpisodeReplayViewer->ResetReplay();
	}
	LoadedReplayRunId.Reset();
	LoadedReplayEpisodeDirectory.Reset();
	SetReplayEpisodeNumberText(FString());
}

UProjectWorkspaceViewModel* URunDetailScreenWidget::ResolveWorkspaceViewModel()
{
	if (ProjectWorkspaceViewModel)
	{
		return ProjectWorkspaceViewModel.Get();
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	ProjectWorkspaceViewModel = platformUiSubsystem ? platformUiSubsystem->GetProjectWorkspaceViewModel() : nullptr;
	return ProjectWorkspaceViewModel.Get();
}

UExperimentResultViewModel* URunDetailScreenWidget::ResolveExperimentResultViewModel()
{
	if (ExperimentResultViewModel)
	{
		return ExperimentResultViewModel.Get();
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	ExperimentResultViewModel = platformUiSubsystem ? platformUiSubsystem->GetExperimentResultViewModel() : nullptr;
	return ExperimentResultViewModel.Get();
}

void URunDetailScreenWidget::RebuildEpisodeCards()
{
	ClearEpisodeCards();

	UExperimentResultViewModel* resultViewModel = ResolveExperimentResultViewModel();
	const TSubclassOf<UProjectEpisodeReplayCardWidget> cardClass = ResolveEpisodeCardWidgetClass();
	if (!EpisodeReplayCardWrapBox || !resultViewModel || !cardClass)
	{
		return;
	}

	for (const UExperimentResultEpisodeViewModel* episodeItem : resultViewModel->GetEpisodeItems())
	{
		UProjectEpisodeReplayCardWidget* cardWidget =
			CreateWidget<UProjectEpisodeReplayCardWidget>(this, cardClass);
		if (!cardWidget)
		{
			continue;
		}

		cardWidget->InitializeFromEpisodeViewModel(episodeItem);
		cardWidget->OnReplayRequested.RemoveAll(this);
		cardWidget->OnReplayRequested.AddUObject(this, &URunDetailScreenWidget::HandleEpisodeReplayRequested);
		if (UHorizontalBoxSlot* cardSlot = EpisodeReplayCardWrapBox->AddChildToHorizontalBox(cardWidget))
		{
			cardSlot->SetPadding(EpisodeReplayCardPadding);
		}
		EpisodeCards.Add(cardWidget);
	}
}

// Opens the first replay-capable episode when this run does not already have a loaded replay.
void URunDetailScreenWidget::OpenInitialEpisodeReplay()
{
	if (!ProjectEpisodeReplayViewer)
	{
		return;
	}

	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	FString selectedRunId = workspaceViewModel
		? workspaceViewModel->GetSelectedRunId()
		: DisplayedRunId;
	selectedRunId = selectedRunId.TrimStartAndEnd();

	if (!selectedRunId.IsEmpty()
		&& LoadedReplayRunId.Equals(selectedRunId, ESearchCase::IgnoreCase)
		&& !LoadedReplayEpisodeDirectory.IsEmpty())
	{
		return;
	}

	for (UProjectEpisodeReplayCardWidget* cardWidget : EpisodeCards)
	{
		if (cardWidget
			&& cardWidget->IsReplayAvailable()
			&& OpenEpisodeReplayCard(cardWidget))
		{
			return;
		}
	}

	ResetReplay();
}

// Opens replay for one episode card and mirrors the selected episode header on success.
bool URunDetailScreenWidget::OpenEpisodeReplayCard(
	UProjectEpisodeReplayCardWidget* cardWidget)
{
	if (!ProjectEpisodeReplayViewer || !cardWidget || !cardWidget->IsReplayAvailable())
	{
		return false;
	}

	ApplyReplayInterestRegionStripToViewer();
	if (!ProjectEpisodeReplayViewer->OpenEpisodeReplay(cardWidget->GetEpisodeDirectory()))
	{
		return false;
	}

	SetReplayEpisodeNumberText(cardWidget->GetEpisodeId());

	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	LoadedReplayRunId = workspaceViewModel
		? workspaceViewModel->GetSelectedRunId()
		: DisplayedRunId;
	LoadedReplayRunId = LoadedReplayRunId.TrimStartAndEnd();
	LoadedReplayEpisodeDirectory = cardWidget->GetEpisodeDirectory();
	FPaths::NormalizeDirectoryName(LoadedReplayEpisodeDirectory);
	return true;
}

void URunDetailScreenWidget::RebuildAnalysisRows()
{
	ClearAnalysisRows();

	UExperimentResultViewModel* resultViewModel = ResolveExperimentResultViewModel();
	if (!resultViewModel)
	{
		return;
	}

	for (const UExperimentResultInsightViewModel* insightItem : resultViewModel->GetInsightItems())
	{
		AddInsightRow(insightItem);
	}
	for (const UExperimentResultSuggestionViewModel* suggestionItem : resultViewModel->GetSuggestionItems())
	{
		AddSuggestionRow(suggestionItem);
	}
	for (const UOdiroListItemViewModel* warningItem : resultViewModel->GetWarningItems())
	{
		AddWarningRow(warningItem);
	}
}

void URunDetailScreenWidget::ClearEpisodeCards()
{
	for (UProjectEpisodeReplayCardWidget* cardWidget : EpisodeCards)
	{
		if (cardWidget)
		{
			cardWidget->OnReplayRequested.RemoveAll(this);
			cardWidget->RemoveFromParent();
		}
	}
	EpisodeCards.Reset();
	if (EpisodeReplayCardWrapBox)
	{
		EpisodeReplayCardWrapBox->ClearChildren();
	}
}

void URunDetailScreenWidget::ClearAnalysisRows()
{
	for (UProjectAiSuggestionRowWidget* rowWidget : AnalysisRows)
	{
		if (rowWidget)
		{
			rowWidget->RemoveFromParent();
		}
	}
	AnalysisRows.Reset();
	if (AiInsightListBox)
	{
		AiInsightListBox->ClearChildren();
	}
	if (AiSuggestionListBox)
	{
		AiSuggestionListBox->ClearChildren();
	}
	if (AiWarningListBox)
	{
		AiWarningListBox->ClearChildren();
	}
}

TSubclassOf<UProjectEpisodeReplayCardWidget> URunDetailScreenWidget::ResolveEpisodeCardWidgetClass() const
{
	return EpisodeCardWidgetClass;
}

TSubclassOf<UProjectAiSuggestionRowWidget> URunDetailScreenWidget::ResolveSuggestionRowWidgetClass() const
{
	return SuggestionRowWidgetClass;
}

void URunDetailScreenWidget::AddInsightRow(const UExperimentResultInsightViewModel* insightItem)
{
	if (!insightItem)
	{
		return;
	}

	UExperimentResultSuggestionViewModel* rowItem = NewObject<UExperimentResultSuggestionViewModel>(this);
	if (!rowItem)
	{
		return;
	}

	rowItem->SetSeverity(insightItem->GetSeverity());
	rowItem->SetSeverityLabel(insightItem->GetSeverityLabel());
	rowItem->SetReason(insightItem->GetDescription());
	rowItem->SetTitle(insightItem->GetTitle());
	AddSuggestionRowToContainer(rowItem, AiInsightListBox ? AiInsightListBox.Get() : AiSuggestionListBox.Get());
}

void URunDetailScreenWidget::AddSuggestionRow(const UExperimentResultSuggestionViewModel* suggestionItem)
{
	AddSuggestionRowToContainer(suggestionItem, AiSuggestionListBox.Get());
}

void URunDetailScreenWidget::AddWarningRow(const UOdiroListItemViewModel* warningItem)
{
	if (!warningItem)
	{
		return;
	}

	UExperimentResultSuggestionViewModel* rowItem = NewObject<UExperimentResultSuggestionViewModel>(this);
	if (!rowItem)
	{
		return;
	}

	rowItem->SetReason(warningItem->GetTitle());
	rowItem->SetTitle(warningItem->GetSubtitle());
	AddSuggestionRowToContainer(rowItem, AiWarningListBox ? AiWarningListBox.Get() : AiSuggestionListBox.Get());
}

void URunDetailScreenWidget::AddSuggestionRowToContainer(
	const UExperimentResultSuggestionViewModel* suggestionItem,
	UVerticalBox* container)
{
	const TSubclassOf<UProjectAiSuggestionRowWidget> rowClass = ResolveSuggestionRowWidgetClass();
	if (!container || !rowClass || !suggestionItem)
	{
		return;
	}

	UProjectAiSuggestionRowWidget* rowWidget =
		CreateWidget<UProjectAiSuggestionRowWidget>(this, rowClass);
	if (!rowWidget)
	{
		return;
	}

	rowWidget->InitializeFromSuggestionViewModel(suggestionItem);
	container->AddChild(rowWidget);
	AnalysisRows.Add(rowWidget);
}

void URunDetailScreenWidget::SetReplayEpisodeNumberText(const FString& episodeId)
{
	UTextBlock* episodeNumberText = ReplayEpisodeNumber.Get();
	if (!episodeNumberText)
	{
		ReplayEpisodeNumber = Cast<UTextBlock>(GetWidgetFromName(TEXT("ReplayEpisodeNumber")));
		episodeNumberText = ReplayEpisodeNumber.Get();
	}
	if (!episodeNumberText)
	{
		return;
	}

	const FString trimmedEpisodeId = episodeId.TrimStartAndEnd();
	episodeNumberText->SetText(trimmedEpisodeId.IsEmpty()
		? FText::GetEmpty()
		: FText::FromString(trimmedEpisodeId));
}

void URunDetailScreenWidget::HandleEpisodeReplayRequested(UProjectEpisodeReplayCardWidget* cardWidget)
{
	OpenEpisodeReplayCard(cardWidget);
}

void URunDetailScreenWidget::HandleReplayFullscreenChanged(
	UProjectEpisodeReplayViewerWidget* replayViewer,
	bool bFullscreen)
{
	if (!IsValid(replayViewer) || replayViewer != ProjectEpisodeReplayViewer.Get())
	{
		return;
	}

	if (bFullscreen)
	{
		if (!ReplayFullscreenHost || !ReplayViewerSize || !ProjectEpisodeReplayViewer)
		{
			UE_LOG(
				LogRunDetailScreenWidget,
				Warning,
				TEXT("Replay fullscreen requested without complete host bindings. NormalHost=%s FullscreenHost=%s Viewer=%s"),
				*GetNameSafe(ReplayViewerSize.Get()),
				*GetNameSafe(ReplayFullscreenHost.Get()),
				*GetNameSafe(ProjectEpisodeReplayViewer.Get()));
			return;
		}

		AttachReplayViewerToHost(ReplayFullscreenHost.Get());
		ReplayFullscreenHost->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	RestoreReplayViewerToNormalHost();
}

void URunDetailScreenWidget::AttachReplayViewerToHost(USizeBox* targetHost)
{
	if (!targetHost || !ProjectEpisodeReplayViewer)
	{
		return;
	}

	if (targetHost->GetContent() == ProjectEpisodeReplayViewer.Get())
	{
		return;
	}

	if (UWidget* existingContent = targetHost->GetContent())
	{
		existingContent->RemoveFromParent();
	}

	ProjectEpisodeReplayViewer->RemoveFromParent();
	targetHost->SetContent(ProjectEpisodeReplayViewer.Get());
	ProjectEpisodeReplayViewer->RefreshReplayControlBindings();
	ApplyReplayInterestRegionStripToViewer();
}

void URunDetailScreenWidget::RestoreReplayViewerToNormalHost()
{
	if (ReplayViewerSize && ProjectEpisodeReplayViewer)
	{
		AttachReplayViewerToHost(ReplayViewerSize.Get());
	}

	if (ReplayFullscreenHost)
	{
		ReplayFullscreenHost->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URunDetailScreenWidget::ResolveReplayInterestRegionStrip()
{
	if (ReplayInterestRegionStrip)
	{
		return;
	}

	ReplayInterestRegionStrip =
		Cast<UProjectEpisodeReplayInterestRegionStripWidget>(
			GetWidgetFromName(TEXT("ReplayInterestRegionStrip")));

	if (!ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip =
			Cast<UProjectEpisodeReplayInterestRegionStripWidget>(
				GetWidgetFromName(TEXT("InterestRegionStrip")));
	}

	if (!ReplayInterestRegionStrip)
	{
		ReplayInterestRegionStrip =
			Cast<UProjectEpisodeReplayInterestRegionStripWidget>(
				GetWidgetFromName(TEXT("WBP_ReplayInterestRegionStrip")));
	}
}

void URunDetailScreenWidget::ApplyReplayInterestRegionStripToViewer()
{
	ResolveReplayInterestRegionStrip();
	if (ProjectEpisodeReplayViewer)
	{
		ProjectEpisodeReplayViewer->SetExternalReplayInterestRegionStrip(
			ReplayInterestRegionStrip.Get());
	}
}

void URunDetailScreenWidget::HandleRequestAiAnalysisClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == RequestAiAnalysisButton)
	{
		RequestAiAnalysis();
	}
}

void URunDetailScreenWidget::HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response)
{
	const FString completedRunId = response.RunId.TrimStartAndEnd();
	if (!completedRunId.IsEmpty())
	{
		UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
		const FString selectedRunId = workspaceViewModel ? workspaceViewModel->GetSelectedRunId() : FString();
		const FString currentRunId = DisplayedRunId.IsEmpty() ? selectedRunId : DisplayedRunId;
		if (!currentRunId.IsEmpty() && !completedRunId.Equals(currentRunId, ESearchCase::IgnoreCase))
		{
			return;
		}

		if (workspaceViewModel && selectedRunId.IsEmpty())
		{
			workspaceViewModel->SelectRun(completedRunId);
		}
	}

	RefreshFromViewModels();
}
