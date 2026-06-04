#include "Episode/EpisodePedestrianPlanSubsystem.h"

#include "Episode/EpisodePedestrianPlanBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodePedestrianPlan, Log, All);

void UEpisodePedestrianPlanSubsystem::ClearPlans()
{
	const int32 planCount = PlansByPedestrianId.Num();
	PlansByPedestrianId.Reset();

	if (planCount > 0)
	{
		UE_LOG(LogEpisodePedestrianPlan, Log, TEXT("보행자 plan 정리 완료 | Plans: %d"), planCount);
	}
}

bool UEpisodePedestrianPlanSubsystem::BuildPlans(
	const FEpisodeSimulationSetupSpec& setupSpec,
	const FEpisodePedestrianPlanBuildContext& buildContext,
	FEpisodePedestrianPlanBuildResult& outResult)
{
	ClearPlans();

	const bool bBuilt = FEpisodePedestrianPlanBuilder::BuildPlans(setupSpec, buildContext, outResult);
	for (const FEpisodePedestrianPlan& plan : outResult.Plans)
	{
		if (!plan.InstanceId.IsEmpty())
		{
			PlansByPedestrianId.Add(plan.InstanceId, plan);
		}
	}

	for (const FString& diagnostic : outResult.Diagnostics)
	{
		UE_LOG(LogEpisodePedestrianPlan, Warning, TEXT("%s"), *diagnostic);
	}

	if (outResult.Plans.Num() > 0 || outResult.Diagnostics.Num() > 0)
	{
		UE_LOG(
			LogEpisodePedestrianPlan,
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

bool UEpisodePedestrianPlanSubsystem::HasPlan(const FString& instanceId) const
{
	return PlansByPedestrianId.Contains(instanceId);
}

const FEpisodePedestrianPlan* UEpisodePedestrianPlanSubsystem::FindPlan(const FString& instanceId) const
{
	return PlansByPedestrianId.Find(instanceId);
}

FEpisodePedestrianPlan UEpisodePedestrianPlanSubsystem::GetPlanCopy(const FString& instanceId, bool& bFound) const
{
	if (const FEpisodePedestrianPlan* plan = FindPlan(instanceId))
	{
		bFound = true;
		return *plan;
	}

	bFound = false;
	return FEpisodePedestrianPlan{};
}
