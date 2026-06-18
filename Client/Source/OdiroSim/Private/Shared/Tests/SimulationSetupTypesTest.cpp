#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/SimulationSetupTypes.h"

#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Scenario/ScenarioCompiler.h"
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
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":45,\"time_scale\":1.0,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.0,\"tip_over_angle_deg\":60,\"near_miss_distance_m\":0.5}")
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
				TEXT("\"corridor\":{},")
				TEXT("\"obstacles\":{},")
				TEXT("\"pedestrians\":{},")
				TEXT("\"robot\":{}")
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

	const UScenarioCompiler* episodeCompiler = NewObject<UScenarioCompiler>();
	const FScenarioCompileResult episodeResult =
		episodeCompiler->CompileScenarioWorldSpecFromJsonFile(TEXT("Json/Input/ScenarioSetupPlayable.json"));
	TestTrue(TEXT("playable episode compiles"), episodeResult.bSuccess);

	const FScenarioPlaceableInstanceSpec* robotSpec = nullptr;
	for (const FScenarioPlaceableInstanceSpec& placeable : episodeResult.WorldSpec.Placeables)
	{
		if (placeable.Category == EScenarioActorCategory::DeliveryBot)
		{
			robotSpec = &placeable;
			break;
		}
	}

	TestNotNull(TEXT("playable episode has robot"), robotSpec);
	if (robotSpec)
	{
		TestFalse(TEXT("playable robot is not spawn-only"), robotSpec->DeliveryBot.bSpawnOnly);
		TestTrue(TEXT("playable robot has start"), robotSpec->DeliveryBot.bHasStartLocation);
		TestTrue(TEXT("playable robot has goal"), robotSpec->DeliveryBot.bHasGoalLocation);
		TestTrue(TEXT("playable route auto-starts"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute);
		TestEqual(TEXT("playable robot start x cm"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.X, -600.0);
		TestEqual(TEXT("playable robot start y cm"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.Y, 0.0);
		TestEqual(TEXT("playable robot goal x cm"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.X, 600.0);
		TestEqual(TEXT("playable robot goal y cm"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.Y, 0.0);
	}

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
	TestEqual(TEXT("episode count"), result.Setting.EpisodeCount, 3);
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
