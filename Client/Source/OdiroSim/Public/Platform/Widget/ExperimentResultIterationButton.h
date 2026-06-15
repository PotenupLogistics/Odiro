#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "ExperimentResultIterationButton.generated.h"

class UExperimentResultIterationButton;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FExperimentResultIterationClickedNative,
	UExperimentResultIterationButton*);

// Compact selector button for one episode_result entry in the experiment result detail page.
UCLASS()
class ODIROSIM_API UExperimentResultIterationButton : public UButton
{
	GENERATED_BODY()

public:
	FExperimentResultIterationClickedNative OnIterationClicked;

	// Stores the selected episode_result path and label index for click callbacks.
	void Configure(const FString& resultPath, int32 runIndex);

	FString GetResultPath() const { return ResultPath; }
	int32 GetRunIndex() const { return RunIndex; }

protected:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UFUNCTION()
	void HandleClicked();

	// Project-relative or absolute episode_result JSON path represented by this button.
	FString ResultPath;

	// Numeric label used by the compact result selector.
	int32 RunIndex = INDEX_NONE;
};
