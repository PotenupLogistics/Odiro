#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioPedestrianPlanBuilder.h"
#include "Misc/AutomationTest.h"

namespace
{
	FScenarioParamValue MakeStringParam(const FString& value)
	{
		FScenarioParamValue param;
		param.Type = EScenarioParamValueType::String;
		param.StringValue = value;
		return param;
	}

	FScenarioParamValue MakeFloatParam(double value)
	{
		FScenarioParamValue param;
		param.Type = EScenarioParamValueType::Float;
		param.FloatValue = value;
		return param;
	}

	FScenarioParamValue MakeVectorParam(const FVector& value)
	{
		FScenarioParamValue param;
		param.Type = EScenarioParamValueType::Vector;
		param.VectorValue = value;
		return param;
	}

	FScenarioDynamicActorSpec MakePlannedPedestrianSpec()
	{
		FScenarioDynamicActorSpec spec;
		spec.InstanceId = TEXT("ped_01");
		spec.AssetId = TEXT("adult_pedestrian");
		spec.Category = EScenarioActorCategory::Pedestrian;
		spec.InitialTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector);
		spec.Properties.Add(TEXT("movement_model"), MakeStringParam(TEXT("planned_trajectory")));
		spec.Properties.Add(TEXT("planned_start_cm"), MakeVectorParam(FVector(0.0, 0.0, 0.0)));
		spec.Properties.Add(TEXT("planned_goal_cm"), MakeVectorParam(FVector(1000.0, 0.0, 0.0)));
		spec.Properties.Add(TEXT("speed_cm_per_second"), MakeFloatParam(100.0));
		return spec;
	}

	FScenarioSimulationSetupSpec MakeSetupSpec()
	{
		FScenarioSimulationSetupSpec setupSpec;
		setupSpec.EpisodeId = TEXT("pedestrian_plan_test");
		setupSpec.SpecHash = TEXT("source_hash");
		setupSpec.DynamicActors.Add(MakePlannedPedestrianSpec());
		return setupSpec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioPedestrianPlanBuilderDeterminismTest,
	"OdiroSim.Scenario.PedestrianPlan.BuilderDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioPedestrianPlanBuilderDeterminismTest::RunTest(const FString& Parameters)
{
	const FScenarioSimulationSetupSpec setupSpec = MakeSetupSpec();

	FScenarioPedestrianPlanBuildContext context;
	context.SourceSpecHash = setupSpec.SpecHash;
	context.StaticObstacleClearanceCm = 80.0;

	FScenarioPedestrianPlanBuildResult firstResult;
	FScenarioPedestrianPlanBuildResult secondResult;

	TestTrue(TEXT("first build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(setupSpec, context, firstResult));
	TestTrue(TEXT("second build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(setupSpec, context, secondResult));
	TestEqual(TEXT("one plan built"), firstResult.Plans.Num(), 1);
	TestEqual(TEXT("one plan built again"), secondResult.Plans.Num(), 1);

	if (firstResult.Plans.Num() == 1 && secondResult.Plans.Num() == 1)
	{
		TestEqual(TEXT("plan hash is deterministic"), firstResult.Plans[0].PlanHash, secondResult.Plans[0].PlanHash);
		TestEqual(TEXT("behavior hash is deterministic"), firstResult.Plans[0].BehaviorHash, secondResult.Plans[0].BehaviorHash);
		TestEqual(TEXT("scenario hash is deterministic"), firstResult.Plans[0].PedestrianScenarioHash, secondResult.Plans[0].PedestrianScenarioHash);
		TestEqual(TEXT("straight path has two points"), firstResult.Plans[0].Points.Num(), 2);
		TestEqual(TEXT("nominal duration uses speed"), firstResult.Plans[0].NominalDurationSeconds, 10.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioPedestrianPlanBuilderObstacleDetourTest,
	"OdiroSim.Scenario.PedestrianPlan.BuilderObstacleDetour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioPedestrianPlanBuilderObstacleDetourTest::RunTest(const FString& Parameters)
{
	const FScenarioSimulationSetupSpec setupSpec = MakeSetupSpec();

	FScenarioPedestrianPlanBuildContext context;
	context.SourceSpecHash = setupSpec.SpecHash;
	context.StaticObstacleClearanceCm = 80.0;

	FScenarioPedestrianObstacleFootprint obstacle;
	obstacle.InstanceId = TEXT("box_01");
	obstacle.AssetId = TEXT("test_box");
	obstacle.Center = FVector(500.0, 0.0, 0.0);
	obstacle.Extent = FVector(100.0, 100.0, 100.0);
	context.StaticObstacleFootprints.Add(obstacle);

	FScenarioPedestrianPlanBuildResult result;
	TestTrue(TEXT("build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(setupSpec, context, result));
	TestEqual(TEXT("one plan built"), result.Plans.Num(), 1);

	if (result.Plans.Num() == 1)
	{
		const FScenarioPedestrianPlan& plan = result.Plans[0];
		TestTrue(TEXT("detour adds intermediate points"), plan.Points.Num() > 2);
		TestTrue(TEXT("plan hash includes footprint hash"), !plan.ResolvedFootprintHash.IsEmpty());
		TestEqual(TEXT("plan stores source hash"), plan.SourceSpecHash, setupSpec.SpecHash);
		TestEqual(TEXT("first point is start"), plan.Points[0].Location, FVector(0.0, 0.0, 0.0));
		TestEqual(TEXT("last point is goal"), plan.Points.Last().Location, FVector(1000.0, 0.0, 0.0));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioPedestrianPlanBuilderBehaviorParamsTest,
	"OdiroSim.Scenario.PedestrianPlan.BehaviorParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioPedestrianPlanBuilderBehaviorParamsTest::RunTest(const FString& Parameters)
{
	FScenarioSimulationSetupSpec defaultSetupSpec = MakeSetupSpec();
	FScenarioSimulationSetupSpec customSetupSpec = MakeSetupSpec();
	FScenarioDynamicActorSpec& customPedestrian = customSetupSpec.DynamicActors[0];
	customPedestrian.Properties.Add(TEXT("behavior_cooperation"), MakeFloatParam(0.9));
	customPedestrian.Properties.Add(TEXT("behavior_evasiveness"), MakeFloatParam(0.8));
	customPedestrian.Properties.Add(TEXT("behavior_personal_space_cm"), MakeFloatParam(120.0));
	customPedestrian.Properties.Add(TEXT("behavior_awareness_horizon_s"), MakeFloatParam(3.0));
	customPedestrian.Properties.Add(TEXT("behavior_max_yield_wait_s"), MakeFloatParam(2.0));
	customPedestrian.Properties.Add(TEXT("behavior_sidestep_distance_cm"), MakeFloatParam(90.0));

	FScenarioPedestrianPlanBuildContext context;
	context.SourceSpecHash = defaultSetupSpec.SpecHash;

	FScenarioPedestrianPlanBuildResult defaultResult;
	FScenarioPedestrianPlanBuildResult customResult;

	TestTrue(TEXT("default build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(defaultSetupSpec, context, defaultResult));
	TestTrue(TEXT("custom build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(customSetupSpec, context, customResult));
	TestEqual(TEXT("default one plan"), defaultResult.Plans.Num(), 1);
	TestEqual(TEXT("custom one plan"), customResult.Plans.Num(), 1);

	if (defaultResult.Plans.Num() == 1 && customResult.Plans.Num() == 1)
	{
		const FScenarioPedestrianPlan& defaultPlan = defaultResult.Plans[0];
		const FScenarioPedestrianPlan& customPlan = customResult.Plans[0];
		TestEqual(TEXT("baseline plan hash is unchanged by behavior"), defaultPlan.PlanHash, customPlan.PlanHash);
		TestTrue(TEXT("behavior hash changes"), defaultPlan.BehaviorHash != customPlan.BehaviorHash);
		TestTrue(TEXT("scenario hash changes"), defaultPlan.PedestrianScenarioHash != customPlan.PedestrianScenarioHash);
		TestEqual(TEXT("custom cooperation resolved"), customPlan.BehaviorParams.Cooperation, 0.9);
		TestEqual(TEXT("custom evasiveness resolved"), customPlan.BehaviorParams.Evasiveness, 0.8);
		TestEqual(TEXT("custom personal space resolved"), customPlan.BehaviorParams.PersonalSpaceCm, 120.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioPedestrianPlanBuilderPathCurveTest,
	"OdiroSim.Scenario.PedestrianPlan.PathCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioPedestrianPlanBuilderPathCurveTest::RunTest(const FString& Parameters)
{
	FScenarioSimulationSetupSpec straightSetupSpec = MakeSetupSpec();
	FScenarioSimulationSetupSpec curvedSetupSpec = MakeSetupSpec();
	FScenarioDynamicActorSpec& curvedPedestrian = curvedSetupSpec.DynamicActors[0];
	curvedPedestrian.Properties.Add(TEXT("path_curve_offset_cm"), MakeFloatParam(80.0));
	curvedPedestrian.Properties.Add(TEXT("path_curve_sample_spacing_cm"), MakeFloatParam(100.0));

	FScenarioPedestrianPlanBuildContext context;
	context.SourceSpecHash = straightSetupSpec.SpecHash;

	FScenarioPedestrianPlanBuildResult straightResult;
	FScenarioPedestrianPlanBuildResult curvedResult;

	TestTrue(TEXT("straight build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(straightSetupSpec, context, straightResult));
	TestTrue(TEXT("curved build succeeds"), FScenarioPedestrianPlanBuilder::BuildPlans(curvedSetupSpec, context, curvedResult));
	TestEqual(TEXT("straight one plan"), straightResult.Plans.Num(), 1);
	TestEqual(TEXT("curved one plan"), curvedResult.Plans.Num(), 1);

	if (straightResult.Plans.Num() == 1 && curvedResult.Plans.Num() == 1)
	{
		const FScenarioPedestrianPlan& straightPlan = straightResult.Plans[0];
		const FScenarioPedestrianPlan& curvedPlan = curvedResult.Plans[0];
		TestEqual(TEXT("straight path remains two points by default"), straightPlan.Points.Num(), 2);
		TestTrue(TEXT("curved path adds sampled points"), curvedPlan.Points.Num() > 2);
		TestEqual(TEXT("curved path preserves start"), curvedPlan.Points[0].Location, FVector(0.0, 0.0, 0.0));
		TestEqual(TEXT("curved path preserves goal"), curvedPlan.Points.Last().Location, FVector(1000.0, 0.0, 0.0));
		TestTrue(TEXT("curved path bends laterally"), FMath::Abs(curvedPlan.Points[curvedPlan.Points.Num() / 2].Location.Y) > KINDA_SMALL_NUMBER);
		TestTrue(TEXT("curve changes baseline plan hash"), straightPlan.PlanHash != curvedPlan.PlanHash);
		TestEqual(TEXT("curve offset is stored"), curvedPlan.PathShapeParams.CurveOffsetCm, 80.0);
		TestEqual(TEXT("curve sample spacing is stored"), curvedPlan.PathShapeParams.CurveSampleSpacingCm, 100.0);
	}

	return true;
}

#endif
