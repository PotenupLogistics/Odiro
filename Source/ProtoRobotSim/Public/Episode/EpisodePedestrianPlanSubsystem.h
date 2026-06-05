#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodePedestrianPlanTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodePedestrianPlanSubsystem.generated.h"

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodePedestrianPlanSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Episode|PedestrianPlan")
	void ClearPlans();

	bool BuildPlans(
		const FEpisodeSimulationSetupSpec& setupSpec,
		const FEpisodePedestrianPlanBuildContext& buildContext,
		FEpisodePedestrianPlanBuildResult& outResult);

	UFUNCTION(BlueprintPure, Category = "Episode|PedestrianPlan")
	bool HasPlan(const FString& instanceId) const;

	const FEpisodePedestrianPlan* FindPlan(const FString& instanceId) const;

	UFUNCTION(BlueprintPure, Category = "Episode|PedestrianPlan")
	FEpisodePedestrianPlan GetPlanCopy(const FString& instanceId, bool& bFound) const;

private:
	UPROPERTY(Transient)
	TMap<FString, FEpisodePedestrianPlan> PlansByPedestrianId;
};
