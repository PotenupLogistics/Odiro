#include "Scenario/ScenarioPedestrianPlanSubsystem.h"

#include "Scenario/ScenarioPedestrianPlanBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioPedestrianPlan, Log, All);

void UScenarioPedestrianPlanSubsystem::ClearPlans()
{
	const int32 planCount = PlansByPedestrianId.Num();
	PlansByPedestrianId.Reset();

	if (planCount > 0)
	{
		UE_LOG(LogScenarioPedestrianPlan, Log, TEXT("보행자 plan 정리 완료 | Plans: %d"), planCount);
	}
}

bool UScenarioPedestrianPlanSubsystem::BuildPlans(
	const FScenarioSimulationSetupSpec& setupSpec,
	const FScenarioPedestrianPlanBuildContext& buildContext,
	FScenarioPedestrianPlanBuildResult& outResult)
{
	ClearPlans();

	const bool bBuilt = FScenarioPedestrianPlanBuilder::BuildPlans(setupSpec, buildContext, outResult);
	for (const FScenarioPedestrianPlan& plan : outResult.Plans)
	{
		if (!plan.InstanceId.IsEmpty())
		{
			PlansByPedestrianId.Add(plan.InstanceId, plan);
		}
	}

	for (const FString& diagnostic : outResult.Diagnostics)
	{
		UE_LOG(LogScenarioPedestrianPlan, Warning, TEXT("%s"), *diagnostic);
	}

	if (outResult.Plans.Num() > 0 || outResult.Diagnostics.Num() > 0)
	{
		UE_LOG(
			LogScenarioPedestrianPlan,
			Log,
			TEXT("보행자 plan 생성 완료 | Episode: %s, Success: %s, Plans: %d, Diagnostics: %d, FootprintHash: %s"),
			*setupSpec.EpisodeId,
			bBuilt ? TEXT("true") : TEXT("false"),
			outResult.Plans.Num(),
			outResult.Diagnostics.Num(),
			*outResult.ResolvedFootprintHash);
	}

	return bBuilt;
}

bool UScenarioPedestrianPlanSubsystem::HasPlan(const FString& instanceId) const
{
	return PlansByPedestrianId.Contains(instanceId);
}

const FScenarioPedestrianPlan* UScenarioPedestrianPlanSubsystem::FindPlan(const FString& instanceId) const
{
	return PlansByPedestrianId.Find(instanceId);
}

FScenarioPedestrianPlan UScenarioPedestrianPlanSubsystem::GetPlanCopy(const FString& instanceId, bool& bFound) const
{
	if (const FScenarioPedestrianPlan* plan = FindPlan(instanceId))
	{
		bFound = true;
		return *plan;
	}

	bFound = false;
	return FScenarioPedestrianPlan{};
}
