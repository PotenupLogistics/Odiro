#include "Platform/Widget/ExperimentResultIterationButton.h"

void UExperimentResultIterationButton::Configure(const FString& resultPath, const int32 runIndex)
{
	ResultPath = resultPath;
	RunIndex = runIndex;

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
