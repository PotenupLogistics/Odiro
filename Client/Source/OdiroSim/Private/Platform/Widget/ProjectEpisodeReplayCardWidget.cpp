#include "Platform/Widget/ProjectEpisodeReplayCardWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Misc/Paths.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"

void UProjectEpisodeReplayCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ReplayCardButton)
	{
		ReplayCardButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayCardWidget::HandleReplayCardClicked);
		ReplayCardButton->OnClicked.AddDynamic(this, &UProjectEpisodeReplayCardWidget::HandleReplayCardClicked);
	}
}

void UProjectEpisodeReplayCardWidget::NativeDestruct()
{
	if (ReplayCardButton)
	{
		ReplayCardButton->OnClicked.RemoveDynamic(this, &UProjectEpisodeReplayCardWidget::HandleReplayCardClicked);
	}

	OnReplayRequested.Clear();
	Super::NativeDestruct();
}

void UProjectEpisodeReplayCardWidget::InitializeFromEpisodeViewModel(
	const UExperimentResultEpisodeViewModel* episodeItem)
{
	EpisodeId = episodeItem ? episodeItem->GetEpisodeId() : FString();
	EpisodeDirectory = episodeItem ? episodeItem->GetEpisodeDirectory() : FString();
	FPaths::NormalizeDirectoryName(EpisodeDirectory);
	bReplayAvailable = episodeItem && episodeItem->IsReplayAvailable();

	if (EpisodeIdText)
	{
		EpisodeIdText->SetText(FText::FromString(EpisodeId.IsEmpty() ? TEXT("--") : EpisodeId));
	}
	if (EpisodeStateText)
	{
		const FString outcomeLabel = episodeItem ? episodeItem->GetOutcomeLabel() : FString();
		EpisodeStateText->SetText(outcomeLabel.IsEmpty()
			? (episodeItem && episodeItem->IsSuccess()
				? NSLOCTEXT("OdiroPlatform", "EpisodeCardSuccess", "성공")
				: NSLOCTEXT("OdiroPlatform", "EpisodeCardFailed", "실패"))
			: FText::FromString(outcomeLabel));
	}
	if (EpisodeDurationText)
	{
		EpisodeDurationText->SetText(FText::FromString(episodeItem ? episodeItem->GetDurationLabel() : FString(TEXT("--"))));
	}
	if (ReplayAvailabilityText)
	{
		ReplayAvailabilityText->SetText(bReplayAvailable
			? NSLOCTEXT("OdiroPlatform", "EpisodeCardReplayReady", "Replay")
			: NSLOCTEXT("OdiroPlatform", "EpisodeCardReplayMissing", "No replay"));
	}
}

void UProjectEpisodeReplayCardWidget::HandleReplayCardClicked()
{
	OnReplayRequested.Broadcast(this);
}
