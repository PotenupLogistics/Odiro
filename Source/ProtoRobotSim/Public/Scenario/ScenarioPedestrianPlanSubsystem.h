#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioPedestrianPlanTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioPedestrianPlanSubsystem.generated.h"

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioPedestrianPlanSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scenario|PedestrianPlan")
	void ClearPlans();

	bool BuildPlans(
		const FScenarioSimulationSetupSpec& setupSpec,
		const FScenarioPedestrianPlanBuildContext& buildContext,
		FScenarioPedestrianPlanBuildResult& outResult);

	UFUNCTION(BlueprintPure, Category = "Scenario|PedestrianPlan")
	bool HasPlan(const FString& instanceId) const;

	const FScenarioPedestrianPlan* FindPlan(const FString& instanceId) const;

	UFUNCTION(BlueprintPure, Category = "Scenario|PedestrianPlan")
	FScenarioPedestrianPlan GetPlanCopy(const FString& instanceId, bool& bFound) const;

private:
	UPROPERTY(Transient)
	TMap<FString, FScenarioPedestrianPlan> PlansByPedestrianId;
};
