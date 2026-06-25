#include "Platform/Widget/ProjectEpisodeReplayCardWidget.h"

#include "Components/Button.h"
#include "Misc/Paths.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"

void UProjectEpisodeReplayCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!ReplayCardButton)
	{
		ReplayCardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("ReplayCardButton"))));
	}

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
}

void UProjectEpisodeReplayCardWidget::HandleReplayCardClicked()
{
	OnReplayRequested.Broadcast(this);
}
