#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/UserProjectEpisodeScenarioWorldSpecAdapter.h"

#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Shared/UserProjectDataTypes.h"

namespace
{
	bool SaveProjectEpisodeAdapterTestFile(const FString& filePath, const FString& contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(filePath), true);
		return FFileHelper::SaveStringToFile(contents, *filePath);
	}

	FString MakeProjectEpisodeAdapterTestRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/UserProjectEpisodeAdapter"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	bool WriteProjectEpisodeAdapterSnapshot(const FString& projectPath, FUserProjectRunSnapshotPaths& outPaths)
	{
		outPaths = FUserProjectRunSnapshot::BuildPaths(projectPath, TEXT("000001"));
		IFileManager::Get().MakeDirectory(*outPaths.ReviewPath, true);
		IFileManager::Get().MakeDirectory(*outPaths.EpisodesPath, true);
		IFileManager::Get().MakeDirectory(*outPaths.PolicyPath, true);

		return SaveProjectEpisodeAdapterTestFile(
				outPaths.SettingPath,
				TEXT("{")
				TEXT("\"schema\":\"project_setting\",")
				TEXT("\"version\":1,")
				TEXT("\"project_id\":\"adapter_project\",")
				TEXT("\"sampling\":{\"base_seed\":2000,\"episode_count\":1,\"generator_version\":\"0.1.0\"},")
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":30,\"time_scale\":1.0,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.0,\"tip_over_angle_deg\":60,\"near_miss_distance_m\":0.5}")
				TEXT("}"))
			&& SaveProjectEpisodeAdapterTestFile(
				outPaths.ProfilePath,
				TEXT("{")
				TEXT("\"schema\":\"simulation_profile\",")
				TEXT("\"version\":1,")
				TEXT("\"profile_id\":\"adapter_profile\",")
				TEXT("\"display_name\":\"Adapter\",")
				TEXT("\"description\":\"Adapter profile\",")
				TEXT("\"robot\":{")
				TEXT("\"body\":{\"length_m\":1.0,\"width_m\":0.44,\"height_m\":0.64,\"wheel_base_m\":0.7,\"turning_radius_m\":1.2},")
				TEXT("\"drive\":{\"max_speed_kmh\":7.0,\"slowdown_range_kmh\":5.0,\"speed_tolerance_kmh\":0.5,\"stop_brake\":0.18,\"throttle_rate_per_s\":0.28,\"brake_rate_per_s\":0.35,\"steering_rate_per_s\":3.2,\"accel_kmh_per_s\":1.2,\"decel_kmh_per_s\":0.9,\"use_handbrake\":false,\"max_torque\":220.0,\"max_rpm\":4000.0,\"idle_rpm\":600.0,\"engine_brake\":0.04,\"rev_up_moi\":5.0,\"rev_down_rate\":600.0},")
				TEXT("\"lidar\":{\"mode\":\"front_2d\",\"range_m\":6.0,\"angle_step_degree\":5.0,\"height_m\":0.07,\"store_missed_rays\":false}")
				TEXT("}")
				TEXT("}"))
			&& SaveProjectEpisodeAdapterTestFile(
				outPaths.ScenarioPath,
				TEXT("{")
				TEXT("\"schema\":\"scenario\",")
				TEXT("\"version\":1,")
				TEXT("\"scenario_id\":\"adapter_sidewalk\",")
				TEXT("\"intent\":\"Adapter test\",")
				TEXT("\"corridor\":{")
				TEXT("\"segments\":[{\"id\":\"main\",\"kind\":\"sidewalk\",\"length_m\":12.0,\"walkway_width_m\":3.0,\"along_range_m\":[0.0,12.0]}],")
				TEXT("\"building_side\":[{\"surface\":\"wall\",\"width_m\":0.5}],")
				TEXT("\"curb_side\":[{\"surface\":\"road\",\"width_m\":4.0}]")
				TEXT("},")
				TEXT("\"obstacles\":{\"min_clear_width_m\":0.9,\"placements\":[{\"kind\":\"fixed\",\"id\":\"cone_01\",\"prop\":\"obstacle.road_cone_01\",\"at\":{\"segment\":\"main\",\"along_m\":6.0,\"offset_m\":0.75,\"lane\":\"curb_edge\"},\"yaw_deg\":10.0}]},")
				TEXT("\"pedestrians\":{\"background\":{\"count\":0},\"encounters\":[{\"id\":\"oncoming_01\",\"type\":\"oncoming_pass\",\"at\":\"main\",\"meet_offset_m\":0.0,\"persona\":\"normal\"}]},")
				TEXT("\"robot\":{\"start\":{\"segment\":\"main\",\"along_m\":1.0,\"offset_m\":0.0,\"yaw_deg\":0.0},\"goal\":{\"segment\":\"main\",\"along_m\":11.0,\"offset_m\":0.0}}")
				TEXT("}"))
			&& SaveProjectEpisodeAdapterTestFile(
				outPaths.PolicyEntrypointPath,
				TEXT("def create_policy():\n    return None\n"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectEpisodeScenarioWorldSpecAdapterTest,
	"OdiroSim.UserProjectEpisodeScenario.WorldSpecAdapter.Valid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectEpisodeScenarioWorldSpecAdapterTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeProjectEpisodeAdapterTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write project snapshot"), WriteProjectEpisodeAdapterSnapshot(projectPath, paths));

	const FUserProjectRunSnapshotParseResult snapshotResult = FUserProjectRunSnapshot::Parse(projectPath, TEXT("000001"));
	TestTrue(TEXT("snapshot parses"), snapshotResult.bSuccess);

	TArray<FUserProjectEpisodeScenarioWriteResult> writeResults;
	TArray<FScenarioCompileDiagnostic> writeDiagnostics;
	TestTrue(
		TEXT("episode scenario writes"),
		FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
			snapshotResult.Paths,
			snapshotResult.Setting,
			writeResults,
			writeDiagnostics));
	TestEqual(TEXT("episode count"), writeResults.Num(), 1);

	TestTrue(
		TEXT("episode scenario schema is detected"),
		FUserProjectEpisodeScenarioWorldSpecAdapter::IsEpisodeScenarioFile(writeResults[0].ScenarioPath));

	const FScenarioCompileResult compileResult =
		FUserProjectEpisodeScenarioWorldSpecAdapter::CompileScenarioWorldSpecFromEpisodeScenarioFile(writeResults[0].ScenarioPath);
	TestTrue(TEXT("episode scenario adapts"), compileResult.bSuccess);
	TestEqual(TEXT("episode id"), compileResult.WorldSpec.RunConfig.TemplateId, FString(TEXT("000001")));
	const FScenarioParamValue* scenarioIdParam = compileResult.WorldSpec.RunConfig.Parameters.Find(TEXT("scenario_id"));
	TestNotNull(TEXT("scenario id param"), scenarioIdParam);
	if (scenarioIdParam)
	{
		TestEqual(TEXT("scenario id"), scenarioIdParam->StringValue, FString(TEXT("adapter_sidewalk")));
	}
	TestEqual(TEXT("seed"), compileResult.WorldSpec.RunConfig.BaseSeed, static_cast<int64>(2000));
	TestTrue(TEXT("ground regions generated"), compileResult.WorldSpec.GroundRegions.Num() >= 3);
	TestEqual(TEXT("dynamic actor count"), compileResult.WorldSpec.DynamicActors.Num(), 1);
	TestTrue(TEXT("spec hash populated"), !compileResult.WorldSpec.SpecHash.IsEmpty());

	const FScenarioPlaceableInstanceSpec* robotSpec = compileResult.WorldSpec.Placeables.FindByPredicate(
		[](const FScenarioPlaceableInstanceSpec& spec)
		{
			return spec.Category == EScenarioActorCategory::DeliveryBot;
		});
	TestNotNull(TEXT("robot spec"), robotSpec);
	if (robotSpec)
	{
		TestEqual(TEXT("robot start x cm"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.X, 100.0);
		TestEqual(TEXT("robot goal x cm"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.X, 1100.0);
		TestTrue(TEXT("robot has route"), robotSpec->DeliveryBot.bHasStartLocation && robotSpec->DeliveryBot.bHasGoalLocation);
		TestTrue(TEXT("robot setup has goal"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bHasGoal);
	}

	const FScenarioPlaceableInstanceSpec* obstacleSpec = compileResult.WorldSpec.Placeables.FindByPredicate(
		[](const FScenarioPlaceableInstanceSpec& spec)
		{
			return spec.Category == EScenarioActorCategory::StaticObstacle;
		});
	TestNotNull(TEXT("static obstacle spec"), obstacleSpec);
	if (obstacleSpec)
	{
		TestEqual(TEXT("static obstacle prop"), obstacleSpec->AssetId, FString(TEXT("obstacle.road_cone_01")));
	}

	const UDeliveryBotSetupCompiler* deliveryBotCompiler = NewObject<UDeliveryBotSetupCompiler>();
	const FDeliveryBotSetupCompileResult profileResult =
		deliveryBotCompiler->CompileDeliveryBotSetupFromJsonFile(snapshotResult.Paths.ProfilePath);
	TestTrue(TEXT("profile compiles as delivery bot setup"), profileResult.bSuccess);
	TestEqual(TEXT("profile drive alias"), profileResult.SetupInfo.ChaosDriveConfigInfo.SlowdownSpeedRangeKmh, 5.0f);
	TestEqual(TEXT("profile lidar alias"), profileResult.SetupInfo.LidarSensorConfigInfo.ScanRangeM, 6.0f);

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

#endif
