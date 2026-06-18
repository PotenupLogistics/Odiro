#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "ExperimentResultIterationButton.generated.h"

class UExperimentResultIterationButton;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FExperimentResultIterationClickedNative,
	UExperimentResultIterationButton*);

// Compact selector button for one episode result in the project-run detail page.
UCLASS()
class ODIROSIM_API UExperimentResultIterationButton : public UButton
{
	GENERATED_BODY()

public:
	// Broadcasts the clicked button so the owner can read its episode result identity.
	FExperimentResultIterationClickedNative OnIterationClicked;

	// Stores the episode result identity represented by this selector.
	void Configure(const FString& resultPath, const FString& episodeId);

	// Result JSON path for the represented episode.
	FString GetResultPath() const { return ResultPath; }

	// Six-digit episode id for the represented episode.
	FString GetEpisodeId() const { return EpisodeId; }

protected:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UFUNCTION()
	void HandleClicked();

	// Result JSON path selected by this compact button.
	FString ResultPath;

	// Six-digit episode id displayed by this compact button.
	FString EpisodeId;
};
