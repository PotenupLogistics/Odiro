#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioSampleWorldSpecAdapter.h"

#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/UserProjectDataTypes.h"

namespace
{
	bool SaveProjectScenarioSampleAdapterTestFile(const FString& filePath, const FString& contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(filePath), true);
		return FFileHelper::SaveStringToFile(contents, *filePath);
	}

	FString MakeProjectScenarioSampleAdapterTestRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/UserProjectScenarioSampleAdapter"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	bool WriteProjectScenarioSampleAdapterSnapshot(const FString& projectPath, FUserProjectRunSnapshotPaths& outPaths)
	{
		outPaths = FUserProjectRunSnapshot::BuildPaths(projectPath, TEXT("000001"));
		IFileManager::Get().MakeDirectory(*outPaths.ReviewPath, true);
		IFileManager::Get().MakeDirectory(*outPaths.EpisodesPath, true);
		IFileManager::Get().MakeDirectory(*outPaths.PolicyPath, true);

		return SaveProjectScenarioSampleAdapterTestFile(
				outPaths.SettingPath,
				TEXT("{")
				TEXT("\"schema\":\"project_setting\",")
				TEXT("\"version\":1,")
				TEXT("\"project_id\":\"adapter_project\",")
				TEXT("\"sampling\":{\"base_seed\":2000,\"episode_count\":1,\"generator_version\":\"0.1.0\"},")
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":30,\"time_scale\":1.0,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.0,\"tip_over_angle_deg\":60,\"near_miss_distance_m\":0.5}")
				TEXT("}"))
			&& SaveProjectScenarioSampleAdapterTestFile(
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
			&& SaveProjectScenarioSampleAdapterTestFile(
				outPaths.ScenarioPath,
				TEXT("{")
				TEXT("\"schema\":\"scenario\",")
				TEXT("\"version\":1,")
				TEXT("\"scenario_id\":\"adapter_sidewalk\",")
				TEXT("\"intent\":\"Adapter test\",")
				TEXT("\"corridor\":{")
				TEXT("\"axis\":{\"type\":\"polyline\",\"points_m\":[[0.0,0.0],[12.0,0.0]]},")
				TEXT("\"walkway_width_m\":3.0,")
				TEXT("\"segments\":[{\"id\":\"main\",\"type\":\"straight\",\"along_range_m\":[0.0,12.0]}],")
				TEXT("\"building_side\":[{\"surface\":\"wall\",\"width_m\":0.5}],")
				TEXT("\"curb_side\":[{\"surface\":\"road\",\"width_m\":4.0}]")
				TEXT("},")
				TEXT("\"obstacles\":{\"min_clear_width_m\":0.9,\"placements\":[{\"kind\":\"fixed\",\"id\":\"cone_01\",\"prop\":\"obstacle.road_cone_01\",\"at\":{\"segment\":\"main\",\"along_m\":6.0,\"offset_m\":0.75,\"lane\":\"curb_edge\"},\"yaw_deg\":10.0}]},")
				TEXT("\"pedestrians\":{\"background\":{\"count\":0},\"encounters\":[]},")
				TEXT("\"robot\":{\"start\":{\"type\":\"corridor_pose\",\"segment\":\"main\",\"along_m\":1.0,\"offset_m\":0.0,\"heading\":\"forward\"},\"goal\":{\"type\":\"corridor_pose\",\"segment\":\"main\",\"along_m\":11.0,\"offset_m\":0.0,\"heading\":\"forward\"}}")
				TEXT("}"))
			&& SaveProjectScenarioSampleAdapterTestFile(
				outPaths.PolicyEntrypointPath,
				TEXT("def create_policy():\n    return None\n"));
	}

	bool HasRuntimeCorridorSurface(const FScenarioRuntimeCorridorSpec& corridorSpec, const FString& surfaceId)
	{
		for (const FScenarioRuntimeCorridorLayoutEntry& layoutEntry : corridorSpec.Layout)
		{
			if (layoutEntry.Lanes.ContainsByPredicate(
					[&surfaceId](const FScenarioRuntimeCorridorLaneSpec& laneSpec)
					{
						return laneSpec.SurfaceId == surfaceId;
					}))
			{
				return true;
			}
		}

		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectScenarioSampleWorldSpecAdapterTest,
	"OdiroSim.UserProjectScenarioSample.WorldSpecAdapter.Valid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectScenarioSampleWorldSpecAdapterTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeProjectScenarioSampleAdapterTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write project snapshot"), WriteProjectScenarioSampleAdapterSnapshot(projectPath, paths));

	const FUserProjectRunSnapshotParseResult snapshotResult = FUserProjectRunSnapshot::Parse(projectPath, TEXT("000001"));
	TestTrue(TEXT("snapshot parses"), snapshotResult.bSuccess);
	TestEqual(TEXT("max duration parses"), snapshotResult.Setting.MaxDurationSeconds, 60.0);

	TArray<FUserProjectEpisodeScenarioWriteResult> writeResults;
	TArray<FScenarioCompileDiagnostic> writeDiagnostics;
	const bool bScenarioSampleWrites = FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
		snapshotResult.Paths,
		snapshotResult.Setting,
		writeResults,
		writeDiagnostics);
	TestTrue(TEXT("scenario sample writes"), bScenarioSampleWrites);
	TestEqual(TEXT("episode count"), writeResults.Num(), 1);
	if (!bScenarioSampleWrites || writeResults.IsEmpty())
	{
		IFileManager::Get().DeleteDirectory(*projectPath, false, true);
		return false;
	}

	const FScenarioSampleParseResult sampleParseResult =
		FScenarioSampleJson::ParseFromFile(writeResults[0].ScenarioPath);
	TestTrue(TEXT("scenario sample parses"), sampleParseResult.bSuccess);

	const FScenarioCompileResult compileResult =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(sampleParseResult.Document);
	TestTrue(TEXT("scenario sample adapts"), compileResult.bSuccess);
	TestEqual(TEXT("sample scenario id"), compileResult.WorldSpec.RunConfig.TemplateId, FString(TEXT("adapter_sidewalk_000001")));
	TestEqual(TEXT("seed"), compileResult.WorldSpec.RunConfig.BaseSeed, static_cast<int64>(2000));
	const FScenarioParamValue* timeLimitParam = compileResult.WorldSpec.RunConfig.Parameters.Find(TEXT("time_limit_s"));
	TestNotNull(TEXT("time limit param exists"), timeLimitParam);
	if (timeLimitParam != nullptr)
	{
		TestEqual(TEXT("time limit param type"), timeLimitParam->Type, EScenarioParamValueType::Float);
		TestEqual(TEXT("time limit value"), timeLimitParam->FloatValue, 60.0);
	}
	TestEqual(TEXT("runtime corridor count"), compileResult.WorldSpec.Corridors.Num(), 1);
	if (compileResult.WorldSpec.Corridors.IsEmpty())
	{
		IFileManager::Get().DeleteDirectory(*projectPath, false, true);
		return false;
	}

	const FScenarioRuntimeCorridorSpec& runtimeCorridor = compileResult.WorldSpec.Corridors[0];
	TestEqual(TEXT("generated ground region count"), compileResult.WorldSpec.GroundRegions.Num(), 0);
	TestTrue(TEXT("runtime sidewalk surface preserved"), HasRuntimeCorridorSurface(runtimeCorridor, TEXT("sidewalk")));
	TestTrue(TEXT("runtime wall surface preserved"), HasRuntimeCorridorSurface(runtimeCorridor, TEXT("wall")));
	TestTrue(TEXT("runtime road surface preserved"), HasRuntimeCorridorSurface(runtimeCorridor, TEXT("road")));
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
