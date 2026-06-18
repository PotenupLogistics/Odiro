#include "Platform/Widget/ExperimentResultIterationButton.h"

void UExperimentResultIterationButton::Configure(const FString& resultPath, const FString& episodeId)
{
	ResultPath = resultPath;
	EpisodeId = episodeId;

	OnClicked.RemoveDynamic(this, &UExperimentResultIterationButton::HandleClicked);
	OnClicked.AddDynamic(this, &UExperimentResultIterationButton::HandleClicked);
}

void UExperimentResultIterationButton::ReleaseSlateResources(const bool bReleaseChildren)
{
	OnClicked.RemoveDynamic(this, &UExperimentResultIterationButton::HandleClicked);
	OnIterationClicked.Clear();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UExperimentResultIterationButton::HandleClicked()
{
	OnIterationClicked.Broadcast(this);
}
