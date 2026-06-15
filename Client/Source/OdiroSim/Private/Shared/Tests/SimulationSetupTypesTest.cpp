#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/SimulationSetupTypes.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Scenario/ScenarioSimulationProfileAdapter.h"
#include "Scenario/ScenarioTemplateWorldSpecAdapter.h"

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
	TestTrue(TEXT("save report"), result.Setup.Report.bSaveEvaluationReportJson);
	TestEqual(TEXT("report output directory"), result.Setup.Report.OutputDirectory, FString(TEXT("Json/Output")));
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

	FString runQueueJson;
	TestTrue(
		TEXT("playable run queue loads"),
		FFileHelper::LoadFileToString(
			runQueueJson,
			*FSimulationSetupJson::ResolveProjectPath(TEXT("Json/Input/ScenarioRunQueuePlayable.json"))));

	TArray<FScenarioRunInput> runInputs;
	TArray<FString> runQueueDiagnostics;
	TestTrue(
		TEXT("playable run queue reads"),
		USimulatorLaunchSubsystem::TryReadScenarioRunQueueJson(runQueueJson, runInputs, runQueueDiagnostics));
	TestEqual(TEXT("playable run input count"), runInputs.Num(), 1);
	TestEqual(
		TEXT("playable scenario template path"),
		runInputs[0].ScenarioSourceJsonPath,
		FString(TEXT("Json/Input/ScenarioTemplates/FeatureProbeNoPedestrians.template.json")));
	TestEqual(
		TEXT("playable simulation profile path"),
		runInputs[0].SimulationProfileJsonPath,
		FString(TEXT("Json/Input/ScenarioTemplates/TemplateProfileForTest.json")));
	TestEqual(
		TEXT("playable policy spec path"),
		runInputs[0].PolicySpecJsonPath,
		FString(TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json")));

	const FScenarioSimulationProfileCompileResult profileResult =
		FScenarioSimulationProfileAdapter::CompileProfileFromJsonFile(runInputs[0].SimulationProfileJsonPath);
	TestTrue(TEXT("playable profile compiles"), profileResult.bSuccess);

	FScenarioTemplateSampleRequest sampleRequest =
		FScenarioTemplateWorldSpecAdapter::MakeDefaultSampleRequest(runInputs[0].ScenarioSourceJsonPath, runInputs[0].PairId);
	sampleRequest.ProfileRef = runInputs[0].SimulationProfileJsonPath;
	sampleRequest.ProfileHash = FScenarioSimulationProfileAdapter::MakeProfileFileHash(runInputs[0].SimulationProfileJsonPath);
	const FScenarioTemplateWorldSpecCompileResult scenarioResult =
		FScenarioTemplateWorldSpecAdapter::CompileScenarioWorldSpecFromTemplateFile(runInputs[0].ScenarioSourceJsonPath, sampleRequest);
	TestTrue(TEXT("playable scenario template compiles"), scenarioResult.bSuccess);

	const FScenarioPlaceableInstanceSpec* robotSpec = nullptr;
	for (const FScenarioPlaceableInstanceSpec& placeable : scenarioResult.CompileResult.WorldSpec.Placeables)
	{
		if (placeable.Category == EScenarioActorCategory::DeliveryBot)
		{
			robotSpec = &placeable;
			break;
		}
	}

	TestNotNull(TEXT("playable scenario has robot"), robotSpec);
	if (robotSpec)
	{
		TestFalse(TEXT("playable robot is not spawn-only"), robotSpec->DeliveryBot.bSpawnOnly);
		TestTrue(TEXT("playable robot has start"), robotSpec->DeliveryBot.bHasStartLocation);
		TestTrue(TEXT("playable robot has goal"), robotSpec->DeliveryBot.bHasGoalLocation);
		TestTrue(TEXT("playable route auto-starts"), robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute);
		TestNotEqual(
			TEXT("playable robot start and goal differ"),
			robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm,
			robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm);
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
	setup.Report.bSaveEvaluationReportJson = true;
	setup.Report.OutputDirectory = TEXT("Json/TestOutput");
	setup.Status.OutputPath = TEXT("Saved/SimulationRuns/test_status.json");

	FString json;
	TArray<FString> diagnostics;
	TestTrue(TEXT("setup JSON writes"), FSimulationSetupJson::TryWriteSetupJson(setup, json, diagnostics));
	TestEqual(TEXT("diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("run queue field"), json.Contains(TEXT("\"run_queue\"")));

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
		TEXT("report output directory"),
		setup.Report.OutputDirectory,
		FString(TEXT("Saved/SimulationRuns/run-001")));
	TestEqual(
		TEXT("status output path"),
		setup.Status.OutputPath,
		FString(TEXT("Saved/SimulationRuns/run-001/status.json")));

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
	"OdiroSim.SimulationSetup.CommandLine.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationCommandLineParseTest::RunTest(const FString& parameters)
{
	const FSimulationCommandLineParseResult simulatorResult =
		FSimulationCommandLine::Parse(TEXT("-unattended -Simulate=Json/Input/SimulationSetupSample.json -RunId=sample-run-001"));

	TestTrue(TEXT("simulator command succeeds"), simulatorResult.bSuccess);
	TestTrue(TEXT("simulate enabled"), simulatorResult.Options.bSimulate);
	TestEqual(
		TEXT("simulate setup file"),
		simulatorResult.Options.SimulationSetupFile,
		FString(TEXT("Json/Input/SimulationSetupSample.json")));
	TestEqual(TEXT("run id"), simulatorResult.Options.RunId, FString(TEXT("sample-run-001")));

	const FSimulationCommandLineParseResult nonSimulatorResult =
		FSimulationCommandLine::Parse(TEXT("-unattended -NoSplash"));
	TestTrue(TEXT("non-simulator command succeeds"), nonSimulatorResult.bSuccess);
	TestFalse(TEXT("non-simulator command does not simulate"), nonSimulatorResult.Options.bSimulate);

	const FSimulationCommandLineParseResult missingValueResult =
		FSimulationCommandLine::Parse(TEXT("-Simulate -RunId=sample-run-001"));
	TestFalse(TEXT("missing simulate value fails"), missingValueResult.bSuccess);
	TestTrue(
		TEXT("missing simulate value diagnostic"),
		HasSimulationDiagnosticCode(missingValueResult.Diagnostics, TEXT("missing_simulate_value")));

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
	status.ReportPaths.Add(TEXT("Json/Output/sample_report.json"));
	status.ReportPaths.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Json/Output/absolute_sample_report.json"))));
	status.LogPaths.Add(TEXT("Saved/AnalysisLogs/sample.jsonl"));

	FString json;
	TArray<FString> diagnostics;
	FString normalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeFilename(normalizedProjectDir);

	TestTrue(TEXT("status JSON writes"), FSimulationRunStatusJson::TryWriteStatusJson(status, json, diagnostics));
	TestEqual(TEXT("status diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("state field"), json.Contains(TEXT("\"state\": \"Running\"")));
	TestTrue(TEXT("report path"), json.Contains(TEXT("Json/Output/sample_report.json")));
	TestTrue(TEXT("absolute report path is written project-relative"), json.Contains(TEXT("Json/Output/absolute_sample_report.json")));
	TestFalse(TEXT("report path does not include project root"), json.Contains(normalizedProjectDir));
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
		TEXT("\"report_paths\":[\"Json/Output/sample_report.json\"],")
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
	TestEqual(TEXT("report count"), status.ReportPaths.Num(), 1);
	TestEqual(TEXT("log count"), status.LogPaths.Num(), 1);
	TestTrue(TEXT("nullable current pair"), status.CurrentPairId.IsEmpty());
	TestTrue(TEXT("nullable error"), status.Error.IsEmpty());

	return true;
}

#endif
