#include "Platform/Widget/RunDetailScreenWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/Widget/ProjectAiSuggestionRowWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayCardWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayViewerWidget.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextWidget.h"

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
}

void URunDetailScreenWidget::NativeDestruct()
{
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
		TotalDurationText->SetText(FText::FromString(resultViewModel->GetTotalDurationLabel()));
	}
	if (DurationMetricSub)
	{
		DurationMetricSub->SetText(FText::Format(
			NSLOCTEXT("OdiroPlatform", "RunDetailAverageDurationSub", "평균 실행 시간 {0}"),
			FText::FromString(resultViewModel->GetAverageDurationLabel())));
	}
	if (SuccessRateText)
	{
		SuccessRateText->SetText(FText::FromString(resultViewModel->GetSuccessRateLabel()));
	}
	if (CollisionCountText)
	{
		CollisionCountText->SetText(FText::FromString(resultViewModel->GetCollisionCountLabel()));
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
		EpisodeReplayCardWrapBox->AddChildToWrapBox(cardWidget);
		EpisodeCards.Add(cardWidget);
	}
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

void URunDetailScreenWidget::HandleEpisodeReplayRequested(UProjectEpisodeReplayCardWidget* cardWidget)
{
	if (!ProjectEpisodeReplayViewer || !cardWidget || !cardWidget->IsReplayAvailable())
	{
		return;
	}

	ProjectEpisodeReplayViewer->OpenEpisodeReplay(cardWidget->GetEpisodeDirectory());
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
