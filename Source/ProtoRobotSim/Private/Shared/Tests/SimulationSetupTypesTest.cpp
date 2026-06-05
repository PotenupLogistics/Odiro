#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/SimulationSetupTypes.h"

#include "Misc/AutomationTest.h"

namespace
{
	bool HasSimulationDiagnosticCode(
		const TArray<FEpisodeCompileDiagnostic>& diagnostics,
		const FString& code)
	{
		for (const FEpisodeCompileDiagnostic& diagnostic : diagnostics)
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
	"ProtoRobotSim.SimulationSetup.Json.ParseSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonParseSampleTest::RunTest(const FString& parameters)
{
	const FSimulationSetupParseResult result =
		FSimulationSetupJson::ParseFromFile(TEXT("Json/Input/SimulationSetupSample.json"));

	TestTrue(TEXT("sample parses"), result.bSuccess);
	TestEqual(TEXT("diagnostics"), result.Diagnostics.Num(), 0);
	TestEqual(TEXT("schema"), result.Setup.Schema, FString(TEXT("simulation_setup")));
	TestEqual(TEXT("version"), result.Setup.Version, 1);
	TestEqual(TEXT("map id"), result.Setup.MapId, FString(TEXT("EpisodeSimulationMap")));
	TestEqual(TEXT("run queue"), result.Setup.RunQueueJsonPath, FString(TEXT("Json/Input/EpisodeRunQueueSample.json")));
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
	FSimulationSetupJsonValidationTest,
	"ProtoRobotSim.SimulationSetup.Json.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationSetupJsonValidationTest::RunTest(const FString& parameters)
{
	const FString invalidJson =
		TEXT("{")
		TEXT("\"schema\":\"simulation_setup\",")
		TEXT("\"version\":1,")
		TEXT("\"map_id\":\"EpisodeSimulationMap\",")
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
	FSimulationSetupJsonMissingFileTest,
	"ProtoRobotSim.SimulationSetup.Json.MissingFile",
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
	"ProtoRobotSim.SimulationSetup.CommandLine.Parse",
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
	"ProtoRobotSim.SimulationSetup.Status.Write",
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
	status.LogPaths.Add(TEXT("Saved/AnalysisLogs/sample.jsonl"));

	FString json;
	TArray<FString> diagnostics;
	TestTrue(TEXT("status JSON writes"), FSimulationRunStatusJson::TryWriteStatusJson(status, json, diagnostics));
	TestEqual(TEXT("status diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("state field"), json.Contains(TEXT("\"state\": \"Running\"")));
	TestTrue(TEXT("report path"), json.Contains(TEXT("Json/Output/sample_report.json")));
	TestTrue(TEXT("log path"), json.Contains(TEXT("Saved/AnalysisLogs/sample.jsonl")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationRunStatusJsonReadTest,
	"ProtoRobotSim.SimulationSetup.Status.Read",
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
