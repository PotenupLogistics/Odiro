#if WITH_DEV_AUTOMATION_TESTS

#include "Episode/EpisodePedestrianPlanBuilder.h"
#include "Misc/AutomationTest.h"

namespace
{
	FEpisodeParamValue MakeStringParam(const FString& value)
	{
		FEpisodeParamValue param;
		param.Type = EEpisodeParamValueType::String;
		param.StringValue = value;
		return param;
	}

	FEpisodeParamValue MakeFloatParam(double value)
	{
		FEpisodeParamValue param;
		param.Type = EEpisodeParamValueType::Float;
		param.FloatValue = value;
		return param;
	}

	FEpisodeParamValue MakeVectorParam(const FVector& value)
	{
		FEpisodeParamValue param;
		param.Type = EEpisodeParamValueType::Vector;
		param.VectorValue = value;
		return param;
	}

	FEpisodeDynamicActorSpec MakePlannedPedestrianSpec()
	{
		FEpisodeDynamicActorSpec spec;
		spec.InstanceId = TEXT("ped_01");
		spec.AssetId = TEXT("adult_pedestrian");
		spec.Category = EEpisodeActorCategory::Pedestrian;
		spec.InitialTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector);
		spec.Properties.Add(TEXT("movement_model"), MakeStringParam(TEXT("planned_trajectory")));
		spec.Properties.Add(TEXT("planned_start_cm"), MakeVectorParam(FVector(0.0, 0.0, 0.0)));
		spec.Properties.Add(TEXT("planned_goal_cm"), MakeVectorParam(FVector(1000.0, 0.0, 0.0)));
		spec.Properties.Add(TEXT("speed_cm_per_second"), MakeFloatParam(100.0));
		return spec;
	}

	FEpisodeSimulationSetupSpec MakeSetupSpec()
	{
		FEpisodeSimulationSetupSpec setupSpec;
		setupSpec.EpisodeId = TEXT("pedestrian_plan_test");
		setupSpec.SpecHash = TEXT("source_hash");
		setupSpec.DynamicActors.Add(MakePlannedPedestrianSpec());
		return setupSpec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodePedestrianPlanBuilderDeterminismTest,
	"ProtoRobotSim.Episode.PedestrianPlan.BuilderDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodePedestrianPlanBuilderDeterminismTest::RunTest(const FString& Parameters)
{
	const FEpisodeSimulationSetupSpec setupSpec = MakeSetupSpec();

	FEpisodePedestrianPlanBuildContext context;
	context.SourceSpecHash = setupSpec.SpecHash;
	context.StaticObstacleClearanceCm = 80.0;

	FEpisodePedestrianPlanBuildResult firstResult;
	FEpisodePedestrianPlanBuildResult secondResult;

	TestTrue(TEXT("first build succeeds"), FEpisodePedestrianPlanBuilder::BuildPlans(setupSpec, context, firstResult));
	TestTrue(TEXT("second build succeeds"), FEpisodePedestrianPlanBuilder::BuildPlans(setupSpec, context, secondResult));
	TestEqual(TEXT("one plan built"), firstResult.Plans.Num(), 1);
	TestEqual(TEXT("one plan built again"), secondResult.Plans.Num(), 1);

	if (firstResult.Plans.Num() == 1 && secondResult.Plans.Num() == 1)
	{
		TestEqual(TEXT("plan hash is deterministic"), firstResult.Plans[0].PlanHash, secondResult.Plans[0].PlanHash);
		TestEqual(TEXT("straight path has two points"), firstResult.Plans[0].Points.Num(), 2);
		TestEqual(TEXT("nominal duration uses speed"), firstResult.Plans[0].NominalDurationSeconds, 10.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodePedestrianPlanBuilderObstacleDetourTest,
	"ProtoRobotSim.Episode.PedestrianPlan.BuilderObstacleDetour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodePedestrianPlanBuilderObstacleDetourTest::RunTest(const FString& Parameters)
{
	const FEpisodeSimulationSetupSpec setupSpec = MakeSetupSpec();

	FEpisodePedestrianPlanBuildContext context;
	context.SourceSpecHash = setupSpec.SpecHash;
	context.StaticObstacleClearanceCm = 80.0;

	FEpisodePedestrianObstacleFootprint obstacle;
	obstacle.InstanceId = TEXT("box_01");
	obstacle.AssetId = TEXT("test_box");
	obstacle.Center = FVector(500.0, 0.0, 0.0);
	obstacle.Extent = FVector(100.0, 100.0, 100.0);
	context.StaticObstacleFootprints.Add(obstacle);

	FEpisodePedestrianPlanBuildResult result;
	TestTrue(TEXT("build succeeds"), FEpisodePedestrianPlanBuilder::BuildPlans(setupSpec, context, result));
	TestEqual(TEXT("one plan built"), result.Plans.Num(), 1);

	if (result.Plans.Num() == 1)
	{
		const FEpisodePedestrianPlan& plan = result.Plans[0];
		TestTrue(TEXT("detour adds intermediate points"), plan.Points.Num() > 2);
		TestTrue(TEXT("plan hash includes footprint hash"), !plan.ResolvedFootprintHash.IsEmpty());
		TestEqual(TEXT("plan stores source hash"), plan.SourceSpecHash, setupSpec.SpecHash);
		TestEqual(TEXT("first point is start"), plan.Points[0].Location, FVector(0.0, 0.0, 0.0));
		TestEqual(TEXT("last point is goal"), plan.Points.Last().Location, FVector(1000.0, 0.0, 0.0));
	}

	return true;
}

#endif
