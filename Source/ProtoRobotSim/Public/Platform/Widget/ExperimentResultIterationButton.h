#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "ExperimentResultIterationButton.generated.h"

class UExperimentResultIterationButton;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FExperimentResultIterationClickedNative,
	UExperimentResultIterationButton*);

// Compact selector button for one evaluation-report iteration in the experiment result detail page.
UCLASS()
class PROTOROBOTSIM_API UExperimentResultIterationButton : public UButton
{
	GENERATED_BODY()

public:
	FExperimentResultIterationClickedNative OnIterationClicked;

	void Configure(const FString& reportPath, int32 runIndex);

	FString GetReportPath() const { return ReportPath; }
	int32 GetRunIndex() const { return RunIndex; }

protected:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UFUNCTION()
	void HandleClicked();

	FString ReportPath;
	int32 RunIndex = INDEX_NONE;
};
