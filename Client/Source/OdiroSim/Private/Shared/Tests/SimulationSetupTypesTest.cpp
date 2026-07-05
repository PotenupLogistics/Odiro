#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/SimulationSetupTypes.h"

#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
	bool HasSimulationDiagnosticCode(
		const TArray<FScenarioCompileDiagnostic>& diagnostics,
		const FString& code)
	{
		for (const FScenarioCompileDiagnostic& diagnostic : diagnostics)
		{
			if (diagnostic.Code == code)
			{
				return true;
			}
		}

		return false;
	}

	bool SaveSimulationTestFile(const FString& filePath, const FString& contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(filePath), true);
		return FFileHelper::SaveStringToFile(contents, *filePath);
	}

	bool WriteValidUserProjectRunSnapshot(const FString& projectPath)
	{
		const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(projectPath, TEXT("000001"));
		IFileManager::Get().MakeDirectory(*paths.ReviewPath, true);
		IFileManager::Get().MakeDirectory(*paths.EpisodesPath, true);
		IFileManager::Get().MakeDirectory(*paths.PolicyPath, true);

		return SaveSimulationTestFile(
				paths.SettingPath,
				TEXT("{")
				TEXT("\"schema\":\"project_setting\",")
				TEXT("\"version\":1,")
				TEXT("\"project_id\":\"automation_project\",")
				TEXT("\"sampling\":{\"base_seed\":1000,\"episode_count\":3,\"generator_version\":\"0.1.0\"},")
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":45,\"time_scale\":1.5,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.25,\"tip_over_angle_deg\":45,\"near_miss_distance_m\":0.75,")
				TEXT("\"stuck_detection_window_s\":3.5,\"stuck_min_goal_progress_m\":0.2,\"stuck_speed_threshold_kmh\":0.36}")
				TEXT("}"))
			&& SaveSimulationTestFile(
				paths.ProfilePath,
				TEXT("{")
				TEXT("\"schema\":\"simulation_profile\",")
				TEXT("\"version\":1,")
				TEXT("\"profile_id\":\"automation_profile\",")
				TEXT("\"display_name\":\"Automation\",")
				TEXT("\"description\":\"Automation profile\",")
				TEXT("\"robot\":{}")
				TEXT("}"))
			&& SaveSimulationTestFile(
				paths.ScenarioPath,
				TEXT("{")
				TEXT("\"schema\":\"scenario\",")
				TEXT("\"version\":1,")
				TEXT("\"scenario_id\":\"automation_scenario\",")
				TEXT("\"intent\":\"Automation\",")
				TEXT("\"corridor\":{")
				TEXT("\"axis\":{\"type\":\"polyline\",\"points_m\":[[0.0,0.0],[10.0,0.0]]},")
				TEXT("\"walkway_width_m\":3.0,")
				TEXT("\"segments\":[{\"id\":\"main\",\"type\":\"straight\",\"along_range_m\":[0.0,10.0]}]")
				TEXT("},")
				TEXT("\"obstacles\":{},")
				TEXT("\"pedestrians\":{},")
				TEXT("\"robot\":{\"start\":{\"type\":\"entry\"},\"goal\":{\"type\":\"exit\"}}")
				TEXT("}"))
			&& SaveSimulationTestFile(
				paths.PolicyEntrypointPath,
				TEXT("def create_policy():\n    return None\n"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationSetupJsonParseSampleTest,
	"OdiroSim.SimulationSetup.Json.ParseSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonParseSampleTest::RunTest(const FString& parameters)
{
	const FSimulationSetupParseResult result =
		FSimulationSetupJson::ParseFromFile(TEXT("Json/Input/SimulationSetupSample.json"));

	TestTrue(TEXT("sample parses"), result.bSuccess);
	TestEqual(TEXT("diagnostics"), result.Diagnostics.Num(), 0);
	TestEqual(TEXT("schema"), result.Setup.Schema, FString(TEXT("simulation_setup")));
	TestEqual(TEXT("version"), result.Setup.Version, 1);
	TestEqual(TEXT("map id"), result.Setup.MapId, FString(TEXT("ScenarioSimulationMap")));
	TestEqual(TEXT("run queue"), result.Setup.RunQueueJsonPath, FString(TEXT("Json/Input/ScenarioRunQueueSample.json")));
	TestEqual(TEXT("fixed step fps"), result.Setup.FixedStep.Fps, 60);
	TestTrue(TEXT("measurement enabled"), result.Setup.MeasurementLog.bEnabled);
	TestEqual(TEXT("measurement output directory"), result.Setup.MeasurementLog.OutputDirectory, FString(TEXT("Saved/AnalysisLogs")));
	TestEqual(TEXT("measurement file prefix"), result.Setup.MeasurementLog.FilePrefix, FString(TEXT("MeasurementLog")));
	TestEqual(TEXT("flush interval ticks"), result.Setup.MeasurementLog.FlushIntervalTicks, 60);
	TestTrue(TEXT("flush on event"), result.Setup.MeasurementLog.bFlushOnEvent);
	TestEqual(TEXT("status output path"), result.Setup.Status.OutputPath, FString(TEXT("Saved/SimulationRuns/latest_status.json")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationSetupJsonPlayableContractTest,
	"OdiroSim.SimulationSetup.Json.PlayableContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonPlayableContractTest::RunTest(const FString& parameters)
{
	const FSimulationSetupParseResult setupResult =
		FSimulationSetupJson::ParseFromFile(TEXT("Json/Input/SimulationSetupPlayable.json"));

	TestTrue(TEXT("playable setup parses"), setupResult.bSuccess);
	TestEqual(TEXT("playable map id"), setupResult.Setup.MapId, FString(TEXT("ScenarioSimulationMap")));
	TestEqual(TEXT("playable run queue"), setupResult.Setup.RunQueueJsonPath, FString(TEXT("Json/Input/SimulationSetupPlayable_RunQueue.json")));

	const UDeliveryBotSetupCompiler* deliveryBotCompiler = NewObject<UDeliveryBotSetupCompiler>();
	const FDeliveryBotSetupCompileResult deliveryBotResult =
		deliveryBotCompiler->CompileDeliveryBotSetupFromJsonFile(TEXT("Json/Input/DeliveryBotSetupPlayable.json"));
	TestTrue(TEXT("playable policy compiles"), deliveryBotResult.bSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeliveryBotProfileJsonCompileContractTest,
	"OdiroSim.DeliveryBot.Profile.Json.CompileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeliveryBotProfileJsonCompileContractTest::RunTest(const FString& parameters)
{
	const FString profileJson =
		TEXT("{")
		TEXT("\"schema\":\"simulation_profile\",")
		TEXT("\"version\":1,")
		TEXT("\"profile_id\":\"profile_contract\",")
		TEXT("\"display_name\":\"Profile Contract\",")
		TEXT("\"description\":\"Profile compiler contract\",")
		TEXT("\"robot\":{")
		TEXT("\"body\":{\"length_m\":1.0,\"width_m\":0.44,\"height_m\":0.64,\"wheel_base_m\":0.7,\"turning_radius_m\":1.2},")
		TEXT("\"drive\":{\"max_speed_kmh\":7.0,\"max_reverse_kmh\":2.0,\"mass_kg\":48.0,\"accel_kmh_per_s\":1.2,\"decel_kmh_per_s\":0.9,\"reverse_accel_kmh_per_s\":0.8,\"steering_rate_per_s\":3.2,\"throttle_rate_per_s\":0.28,\"brake_rate_per_s\":0.35,\"stop_brake\":0.18,\"gear_switch_stop_kmh\":0.1,\"gear_switch_brake\":0.2,\"slowdown_range_kmh\":5.0,\"speed_tolerance_kmh\":0.5,\"speed_limit_brake\":0.06,\"use_handbrake\":false,\"max_torque\":220.0,\"max_rpm\":4000.0,\"idle_rpm\":600.0,\"engine_brake\":0.04,\"rev_up_moi\":5.0,\"rev_down_rate\":600.0},")
		TEXT("\"lidar\":{\"lidar_mode\":\"TwoD\",\"mode\":\"ThreeD\",\"draw_debug\":true,\"angle_step_degree\":3.0,\"height_m\":0.07,\"vertical_min_degree\":-25.0,\"vertical_max_degree\":25.0,\"vertical_step_degree\":5.0,\"scan_rate_hz\":5.0,\"range_m\":15.0,\"front_half_angle_degree\":50.0,\"stop_distance_m\":2.0,\"obstacle_warning_distance_m\":5.2,\"slow_down_distance_m\":8.0,\"store_missed_rays\":false,\"trace_channel\":\"world_dynamic\",\"ignore_tags\":[\"ShouldNotApply\"],\"observation_profile\":\"realtime_point_cloud\",\"point_cloud\":{\"capture_enabled\":true,\"capture_every_n_sensor_frames\":10,\"range_limit_m\":15.0,\"include_ground_points\":true,\"max_points\":4096}}")
		TEXT("}")
		TEXT("}");

	const UDeliveryBotSetupCompiler* deliveryBotCompiler = NewObject<UDeliveryBotSetupCompiler>();
	const FDeliveryBotSetupCompileResult result =
		deliveryBotCompiler->CompileDeliveryBotSetupFromJsonString(profileJson);

	TestTrue(TEXT("profile compiles"), result.bSuccess);
	TestEqual(TEXT("profile diagnostics"), result.Diagnostics.Num(), 0);

	const FDeliveryBotBodyConfigInfo& body = result.SetupInfo.BodyConfigInfo;
	TestTrue(TEXT("body config present"), body.bHasSetupBodyConfig);
	TestEqual(TEXT("body length"), body.LengthM, 1.0f);
	TestEqual(TEXT("body width"), body.WidthM, 0.44f);
	TestEqual(TEXT("body height"), body.HeightM, 0.64f);
	TestEqual(TEXT("wheel base"), body.WheelBaseM, 0.7f);
	TestEqual(TEXT("turning radius"), body.TurningRadiusM, 1.2f);

	const FDeliveryBotDriveConfigInfo& drive = result.SetupInfo.ChaosDriveConfigInfo;
	TestEqual(TEXT("drive max speed"), drive.MaxSpeedKmh, 7.0f);
	TestEqual(TEXT("drive max reverse speed"), drive.MaxReverseSpeedKmh, 2.0f);
	TestTrue(TEXT("drive mass configured"), drive.bHasMassKg);
	TestEqual(TEXT("drive mass"), drive.MassKg, 48.0f);
	TestEqual(TEXT("drive acceleration"), drive.AccelerationRateKmhPerSecond, 1.2f);
	TestEqual(TEXT("drive deceleration"), drive.DecelerationRateKmhPerSecond, 0.9f);
	TestEqual(TEXT("drive reverse acceleration"), drive.ReverseAccelerationRateKmhPerSecond, 0.8f);
	TestEqual(TEXT("drive gear switch stop"), drive.GearSwitchStopSpeedKmh, 0.1f);
	TestEqual(TEXT("drive gear switch brake"), drive.GearSwitchBrakeInput, 0.2f);
	TestEqual(TEXT("drive max rpm"), drive.MaxRPM, 4000.0f);

	const FDeliveryBotLidarSensorConfigInfo& lidar = result.SetupInfo.LidarSensorConfigInfo;
	TestEqual(TEXT("lidar mode canonical wins"), lidar.LidarModeType, EDeliveryBotLidarModeType::ThreeD);
	TestEqual(TEXT("lidar range"), lidar.ScanRangeM, 15.0f);
	TestEqual(TEXT("lidar angle step"), lidar.AngleStepDegree, 3.0f);
	TestFalse(TEXT("lidar store missed rays"), lidar.bStoreMissedRays);
	TestEqual(TEXT("lidar trace channel remains default"), static_cast<int32>(lidar.TraceChannel.GetValue()), static_cast<int32>(ECC_Visibility));
	TestEqual(TEXT("lidar ignore tag count remains default"), lidar.IgnoreTags.Num(), 1);
	if (!lidar.IgnoreTags.IsEmpty())
	{
		TestEqual(TEXT("lidar ignore tag remains default"), lidar.IgnoreTags[0], FName(TEXT("NoCollision")));
	}

	const FDeliveryBotPointCloudCaptureConfigInfo& pointCloud = result.SetupInfo.PointCloudCaptureConfigInfo;
	TestTrue(TEXT("point cloud config present"), pointCloud.bHasSetupPointCloudConfig);
	TestEqual(TEXT("point cloud profile"), pointCloud.ObservationProfile, FString(TEXT("realtime_point_cloud")));
	TestEqual(TEXT("point cloud range"), pointCloud.RangeLimitM, 15.0f);
	TestEqual(TEXT("point cloud max points"), pointCloud.MaxPoints, 4096);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeliveryBotProfileJsonCompileOusterOS1Test,
	"OdiroSim.DeliveryBot.Profile.Json.CompileOusterOS1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeliveryBotProfileJsonCompileOusterOS1Test::RunTest(const FString& parameters)
{
	const FString profileJson =
		TEXT("{")
		TEXT("\"schema\":\"simulation_profile\",")
		TEXT("\"version\":1,")
		TEXT("\"robot\":{")
		TEXT("\"lidar\":{\"lidar_mode\":\"ouster_os1\",\"range_m\":15.0,\"scan_rate_hz\":10.0}")
		TEXT("}")
		TEXT("}");

	const UDeliveryBotSetupCompiler* deliveryBotCompiler = NewObject<UDeliveryBotSetupCompiler>();
	const FDeliveryBotSetupCompileResult result =
		deliveryBotCompiler->CompileDeliveryBotSetupFromJsonString(profileJson);

	TestTrue(TEXT("OS1 profile compiles"), result.bSuccess);
	TestEqual(TEXT("OS1 diagnostics"), result.Diagnostics.Num(), 0);
	TestEqual(
		TEXT("OS1 lidar mode"),
		result.SetupInfo.LidarSensorConfigInfo.LidarModeType,
		EDeliveryBotLidarModeType::OusterOS1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationSetupJsonValidationTest,
	"OdiroSim.SimulationSetup.Json.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonValidationTest::RunTest(const FString& parameters)
{
	const FString invalidJson =
		TEXT("{")
		TEXT("\"schema\":\"simulation_setup\",")
		TEXT("\"version\":1,")
		TEXT("\"map_id\":\"ScenarioSimulationMap\",")
		TEXT("\"run_queue\":\"\",")
		TEXT("\"fixed_step\":{\"fps\":0},")
		TEXT("\"logging\":{\"flush_interval_ticks\":0},")
		TEXT("\"report\":{\"output_directory\":\"Json/Output\"},")
		TEXT("\"status\":{}")
		TEXT("}");

	const FSimulationSetupParseResult result = FSimulationSetupJson::ParseFromString(invalidJson);

	TestFalse(TEXT("invalid setup fails"), result.bSuccess);
	TestTrue(TEXT("empty run queue diagnostic"), HasSimulationDiagnosticCode(result.Diagnostics, TEXT("empty_run_queue")));
	TestTrue(TEXT("invalid fps diagnostic"), HasSimulationDiagnosticCode(result.Diagnostics, TEXT("invalid_fps")));
	TestTrue(TEXT("invalid flush interval diagnostic"), HasSimulationDiagnosticCode(result.Diagnostics, TEXT("invalid_flush_interval_ticks")));
	TestTrue(TEXT("missing status output path diagnostic"), HasSimulationDiagnosticCode(result.Diagnostics, TEXT("missing_output_path")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationSetupJsonWriteRoundTripTest,
	"OdiroSim.SimulationSetup.Json.WriteRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonWriteRoundTripTest::RunTest(const FString& parameters)
{
	FSimulationSetup setup;
	setup.MapId = TEXT("ScenarioSimulationMap");
	setup.RunQueueJsonPath = TEXT("Json/Input/ScenarioRunQueueSample.json");
	setup.FixedStep.Fps = 30;
	setup.MeasurementLog.bEnabled = true;
	setup.MeasurementLog.OutputDirectory = TEXT("Saved/TestLogs");
	setup.MeasurementLog.FilePrefix = TEXT("TestMeasurement");
	setup.MeasurementLog.FlushIntervalTicks = 10;
	setup.MeasurementLog.bFlushOnEvent = false;
	setup.Status.OutputPath = TEXT("Saved/SimulationRuns/test_status.json");

	FString json;
	TArray<FString> diagnostics;
	TestTrue(TEXT("setup JSON writes"), FSimulationSetupJson::TryWriteSetupJson(setup, json, diagnostics));
	TestEqual(TEXT("diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("run queue field"), json.Contains(TEXT("\"run_queue\"")));
	TestFalse(TEXT("legacy report field omitted"), json.Contains(TEXT("\"report\"")));

	const FSimulationSetupParseResult result = FSimulationSetupJson::ParseFromString(json);
	TestTrue(TEXT("written setup parses"), result.bSuccess);
	TestEqual(TEXT("written fps"), result.Setup.FixedStep.Fps, 30);
	TestEqual(TEXT("written log dir"), result.Setup.MeasurementLog.OutputDirectory, FString(TEXT("Saved/TestLogs")));
	TestFalse(TEXT("written flush on event"), result.Setup.MeasurementLog.bFlushOnEvent);
	TestEqual(TEXT("written status"), result.Setup.Status.OutputPath, FString(TEXT("Saved/SimulationRuns/test_status.json")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationSetupRunOutputPathsTest,
	"OdiroSim.SimulationSetup.Json.RunOutputPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupRunOutputPathsTest::RunTest(const FString& parameters)
{
	FSimulationSetup setup;
	FSimulationSetupJson::ApplyRunOutputPaths(setup, TEXT("run-001"));

	TestEqual(
		TEXT("run output directory"),
		FSimulationSetupJson::BuildRunOutputDirectory(TEXT("run-001")),
		FString(TEXT("Saved/SimulationRuns/run-001")));
	TestEqual(
		TEXT("run setup path"),
		FSimulationSetupJson::BuildRunSetupPath(TEXT("run-001")),
		FString(TEXT("Saved/SimulationRuns/run-001/simulation_setup.json")));
	TestEqual(
		TEXT("measurement output directory"),
		setup.MeasurementLog.OutputDirectory,
		FString(TEXT("Saved/SimulationRuns/run-001")));
	TestEqual(
		TEXT("status output path"),
		setup.Status.OutputPath,
		FString(TEXT("Saved/SimulationRuns/run-001/status.json")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectRunSnapshotParseTest,
	"OdiroSim.UserProjectRun.Snapshot.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectRunSnapshotParseTest::RunTest(const FString& parameters)
{
	const FString projectPath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/UserProjectRunSnapshot"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	TestTrue(TEXT("write project snapshot"), WriteValidUserProjectRunSnapshot(projectPath));

	const FUserProjectRunSnapshotParseResult result = FUserProjectRunSnapshot::Parse(projectPath, TEXT("000001"));

	TestTrue(TEXT("snapshot parses"), result.bSuccess);
	TestEqual(TEXT("run id"), result.Paths.RunId, FString(TEXT("000001")));
	TestEqual(TEXT("map id"), result.Setting.MapId, FString(TEXT("ScenarioSimulationMap")));
	TestEqual(TEXT("fixed fps"), result.Setting.FixedFps, 45);
	TestEqual(TEXT("time scale"), result.Setting.TimeScale, 1.5);
	TestEqual(TEXT("episode count"), result.Setting.EpisodeCount, 3);
	TestEqual(TEXT("goal acceptance cm"), result.Setting.EvaluationConfig.GoalAcceptanceRadiusCm, 125.0);
	TestEqual(TEXT("tip over degrees"), result.Setting.EvaluationConfig.TipOverAngleDegrees, 45.0);
	TestEqual(TEXT("near miss cm"), result.Setting.EvaluationConfig.NearMissDistanceCm, 75.0);
	TestEqual(TEXT("stuck window seconds"), result.Setting.EvaluationConfig.StuckDetectionWindowSeconds, 3.5);
	TestEqual(TEXT("stuck progress cm"), result.Setting.EvaluationConfig.StuckMinGoalProgressCm, 20.0);
	TestEqual(TEXT("stuck speed cm/s"), result.Setting.EvaluationConfig.StuckSpeedThresholdCmPerSecond, 10.0);
	TestTrue(TEXT("policy entrypoint path"), result.Paths.PolicyEntrypointPath.EndsWith(TEXT("snapshot/policy/__init__.py")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectRunSnapshotInvalidRunIdTest,
	"OdiroSim.UserProjectRun.Snapshot.InvalidRunId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectRunSnapshotInvalidRunIdTest::RunTest(const FString& parameters)
{
	const FUserProjectRunSnapshotParseResult result =
		FUserProjectRunSnapshot::Parse(TEXT("X:/Projects/DeliveryBotA"), TEXT("run-001"));

	TestFalse(TEXT("snapshot fails"), result.bSuccess);
	TestTrue(TEXT("invalid run id diagnostic"), HasSimulationDiagnosticCode(result.Diagnostics, TEXT("invalid_run_id")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationSetupJsonMissingFileTest,
	"OdiroSim.SimulationSetup.Json.MissingFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonMissingFileTest::RunTest(const FString& parameters)
{
	const FSimulationSetupParseResult result =
		FSimulationSetupJson::ParseFromFile(TEXT("Json/Input/DoesNotExist_SimulationSetup.json"));

	TestFalse(TEXT("missing file fails"), result.bSuccess);
	TestTrue(
		TEXT("missing file diagnostic"),
		HasSimulationDiagnosticCode(result.Diagnostics, TEXT("simulation_setup_file_missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationCommandLineParseTest,
	"OdiroSim.ProjectRun.CommandLine.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationCommandLineParseTest::RunTest(const FString& parameters)
{
	const FSimulationCommandLineParseResult legacySimulateResult =
		FSimulationCommandLine::Parse(TEXT("-unattended -Simulate=Json/Input/SimulationSetupSample.json -RunId=sample-run-001"));

	TestFalse(TEXT("legacy simulate command fails"), legacySimulateResult.bSuccess);
	TestTrue(
		TEXT("legacy simulate diagnostic"),
		HasSimulationDiagnosticCode(legacySimulateResult.Diagnostics, TEXT("unsupported_simulate_arg")));

	const FSimulationCommandLineParseResult projectRunResult =
		FSimulationCommandLine::Parse(TEXT("-unattended -OdiroProject=\"X:/Projects/DeliveryBotA\" -RunId=000001 -PolicyPort=18124"));

	TestTrue(TEXT("project run command succeeds"), projectRunResult.bSuccess);
	TestTrue(TEXT("project run enabled"), projectRunResult.Options.bProjectRun);
	TestEqual(
		TEXT("project path"),
		projectRunResult.Options.ProjectPath,
		FString(TEXT("X:/Projects/DeliveryBotA")));
	TestEqual(TEXT("project run id"), projectRunResult.Options.RunId, FString(TEXT("000001")));
	TestEqual(TEXT("project policy port"), projectRunResult.Options.PolicyPort, 18124);

	const FSimulationCommandLineParseResult invalidProjectRunIdResult =
		FSimulationCommandLine::Parse(TEXT("-OdiroProject=X:/Projects/DeliveryBotA -RunId=run-001"));
	TestFalse(TEXT("invalid project run id fails"), invalidProjectRunIdResult.bSuccess);
	TestTrue(
		TEXT("invalid run id diagnostic"),
		HasSimulationDiagnosticCode(invalidProjectRunIdResult.Diagnostics, TEXT("invalid_run_id")));

	const FSimulationCommandLineParseResult invalidPolicyPortResult =
		FSimulationCommandLine::Parse(TEXT("-OdiroProject=X:/Projects/DeliveryBotA -RunId=000001 -PolicyPort=0"));
	TestFalse(TEXT("invalid policy port fails"), invalidPolicyPortResult.bSuccess);
	TestTrue(
		TEXT("invalid policy port diagnostic"),
		HasSimulationDiagnosticCode(invalidPolicyPortResult.Diagnostics, TEXT("invalid_policy_port")));

	const FSimulationCommandLineParseResult nonSimulatorResult =
		FSimulationCommandLine::Parse(TEXT("-unattended -NoSplash"));
	TestTrue(TEXT("non-simulator command succeeds"), nonSimulatorResult.bSuccess);

	const FSimulationCommandLineParseResult bareSimulateResult =
		FSimulationCommandLine::Parse(TEXT("-Simulate -RunId=sample-run-001"));
	TestFalse(TEXT("bare simulate fails"), bareSimulateResult.bSuccess);
	TestTrue(
		TEXT("bare simulate diagnostic"),
		HasSimulationDiagnosticCode(bareSimulateResult.Diagnostics, TEXT("unsupported_simulate_arg")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationRunStatusJsonWriteTest,
	"OdiroSim.SimulationSetup.Status.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationRunStatusJsonWriteTest::RunTest(const FString& parameters)
{
	FSimulationRunStatus status;
	status.RunId = TEXT("sample-run-001");
	status.State = ESimulationRunState::Running;
	status.SetupPath = TEXT("Json/Input/SimulationSetupSample.json");
	status.UpdatedAt = TEXT("2026-06-05T00:00:00Z");
	status.CurrentPairId = TEXT("sample_0");
	status.CompletedRuns = 1;
	status.TotalRuns = 5;
	status.ResultPaths.Add(TEXT("runs/000001/episodes/000001/result.json"));
	status.ResultPaths.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("runs/000001/episodes/000002/result.json"))));
	status.LogPaths.Add(TEXT("Saved/AnalysisLogs/sample.jsonl"));

	FString json;
	TArray<FString> diagnostics;
	FString normalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeFilename(normalizedProjectDir);

	TestTrue(TEXT("status JSON writes"), FSimulationRunStatusJson::TryWriteStatusJson(status, json, diagnostics));
	TestEqual(TEXT("status diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("state field"), json.Contains(TEXT("\"state\": \"Running\"")));
	TestTrue(TEXT("result path"), json.Contains(TEXT("runs/000001/episodes/000001/result.json")));
	TestTrue(TEXT("absolute result path is written project-relative"), json.Contains(TEXT("runs/000001/episodes/000002/result.json")));
	TestFalse(TEXT("result path does not include project root"), json.Contains(normalizedProjectDir));
	TestTrue(TEXT("log path"), json.Contains(TEXT("Saved/AnalysisLogs/sample.jsonl")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationRunStatusJsonReadTest,
	"OdiroSim.SimulationSetup.Status.Read",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationRunStatusJsonReadTest::RunTest(const FString& parameters)
{
	const FString json =
		TEXT("{")
		TEXT("\"schema\":\"simulation_run_status\",")
		TEXT("\"version\":1,")
		TEXT("\"run_id\":\"sample-run-001\",")
		TEXT("\"state\":\"Completed\",")
		TEXT("\"setup_path\":\"Json/Input/SimulationSetupSample.json\",")
		TEXT("\"updated_at\":\"2026-06-05T00:00:00Z\",")
		TEXT("\"current_pair_id\":null,")
		TEXT("\"completed_runs\":5,")
		TEXT("\"total_runs\":5,")
		TEXT("\"result_paths\":[\"runs/000001/episodes/000001/result.json\"],")
		TEXT("\"log_paths\":[\"Saved/AnalysisLogs/sample.jsonl\"],")
		TEXT("\"error\":null")
		TEXT("}");

	FSimulationRunStatus status;
	TArray<FString> diagnostics;
	TestTrue(TEXT("status JSON reads"), FSimulationRunStatusJson::TryReadStatusJson(json, status, diagnostics));
	TestEqual(TEXT("diagnostics"), diagnostics.Num(), 0);
	TestEqual(TEXT("run id"), status.RunId, FString(TEXT("sample-run-001")));
	TestEqual(TEXT("state"), status.State, ESimulationRunState::Completed);
	TestEqual(TEXT("completed runs"), status.CompletedRuns, 5);
	TestEqual(TEXT("total runs"), status.TotalRuns, 5);
	TestEqual(TEXT("result count"), status.ResultPaths.Num(), 1);
	TestEqual(TEXT("log count"), status.LogPaths.Num(), 1);
	TestTrue(TEXT("nullable current pair"), status.CurrentPairId.IsEmpty());
	TestTrue(TEXT("nullable error"), status.Error.IsEmpty());

	return true;
}

#endif
