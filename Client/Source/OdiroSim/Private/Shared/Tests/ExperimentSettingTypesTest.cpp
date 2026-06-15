#if WITH_DEV_AUTOMATION_TESTS

// Covers experiment_setting parsing/materialization and child-process run status JSON.

#include "Shared/ExperimentSettingTypes.h"
#include "Shared/SimulationRunStatusTypes.h"

#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
	bool HasSchemaDiagnosticCode(
		const TArray<FScenarioSchemaDiagnostic>& diagnostics,
		const FString& code)
	{
		for (const FScenarioSchemaDiagnostic& diagnostic : diagnostics)
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
	FExperimentSettingJsonParseSampleTest,
	"OdiroSim.ExperimentSetting.Json.ParseSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExperimentSettingJsonParseSampleTest::RunTest(const FString& parameters)
{
	const FExperimentSettingParseResult result =
		FExperimentSettingJson::ParseFromFile(TEXT("Json/Experiments/FeatureProbeNoPedestrians/setting.json"));

	TestTrue(TEXT("experiment setting parses"), result.bSuccess);
	TestEqual(TEXT("diagnostics"), result.Diagnostics.Num(), 0);
	TestEqual(TEXT("schema"), result.Document.Schema, FString(TEXT("experiment_setting")));
	TestEqual(TEXT("version"), result.Document.Version, 1);
	TestEqual(TEXT("experiment id"), result.Document.ExperimentId, FString(TEXT("feature_probe_no_pedestrians")));
	TestEqual(
		TEXT("scenario template ref"),
		result.Document.Sampling.ScenarioTemplateRef,
		FString(TEXT("Json/Templates/Scenarios/FeatureProbeNoPedestrians.template.json")));
	TestEqual(
		TEXT("profile template ref"),
		result.Document.Sampling.ProfileTemplateRef,
		FString(TEXT("Json/Templates/Profiles/TemplateProfileForTest.json")));
	TestEqual(TEXT("sample count"), result.Document.Sampling.SampleCount, 1);
	TestEqual(TEXT("runtime map"), result.Document.Runtime.MapId, FString(TEXT("ScenarioSimulationMap")));
	TestEqual(TEXT("runtime fps"), result.Document.Runtime.FixedFps, 60);
	TestEqual(TEXT("goal radius"), result.Document.Evaluation.GoalAcceptanceRadiusMeters, 0.5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExperimentSettingJsonWriteRoundTripTest,
	"OdiroSim.ExperimentSetting.Json.WriteRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExperimentSettingJsonWriteRoundTripTest::RunTest(const FString& parameters)
{
	FExperimentSettingDocument document;
	document.ExperimentId = TEXT("roundtrip_experiment");
	document.DisplayName = TEXT("Roundtrip Experiment");
	document.Sampling.ScenarioTemplateRef = TEXT("Json/Templates/Scenarios/FeatureProbeNoPedestrians.template.json");
	document.Sampling.ProfileTemplateRef = TEXT("Json/Templates/Profiles/TemplateProfileForTest.json");
	document.Sampling.BaseSeed = 42;
	document.Sampling.SampleCount = 2;
	document.Runtime.MapId = TEXT("ScenarioSimulationMap");
	document.Runtime.FixedFps = 30;
	document.Runtime.TimeScale = 1.0;
	document.Evaluation.GoalAcceptanceRadiusMeters = 0.75;

	FString json;
	TArray<FScenarioSchemaDiagnostic> diagnostics;
	TestTrue(TEXT("experiment setting writes"), FExperimentSettingJson::TryWriteJson(document, json, diagnostics));
	TestEqual(TEXT("diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("experiment field"), json.Contains(TEXT("\"experiment_id\"")));
	TestTrue(TEXT("sampling field"), json.Contains(TEXT("\"sampling\"")));

	const FExperimentSettingParseResult parsed = FExperimentSettingJson::ParseFromString(json);
	TestTrue(TEXT("written setting parses"), parsed.bSuccess);
	TestEqual(TEXT("written sample count"), parsed.Document.Sampling.SampleCount, 2);
	TestEqual(TEXT("written fixed fps"), parsed.Document.Runtime.FixedFps, 30);
	TestEqual(TEXT("written goal radius"), parsed.Document.Evaluation.GoalAcceptanceRadiusMeters, 0.75);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExperimentSettingBuildRunInputsTest,
	"OdiroSim.ExperimentSetting.Json.BuildRunInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExperimentSettingBuildRunInputsTest::RunTest(const FString& parameters)
{
	const FString experimentRef = FString::Printf(
		TEXT("Saved/Automation/OdiroSim/Experiments/%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FExperimentSettingDocument document;
	document.ExperimentId = TEXT("automation_experiment");
	document.Sampling.ScenarioTemplateRef = TEXT("Json/Templates/Scenarios/FeatureProbeNoPedestrians.template.json");
	document.Sampling.ProfileTemplateRef = TEXT("Json/Templates/Profiles/TemplateProfileForTest.json");
	document.Sampling.BaseSeed = 260615001;
	document.Sampling.SampleCount = 1;
	document.Runtime.MapId = TEXT("ScenarioSimulationMap");
	document.Runtime.FixedFps = 60;

	TArray<FScenarioSchemaDiagnostic> diagnostics;
	TestTrue(
		TEXT("experiment setting saves"),
		FExperimentSettingJson::SaveToFile(
			document,
			FExperimentSettingJson::BuildExperimentSettingPath(experimentRef),
			diagnostics));
	TestEqual(TEXT("save diagnostics"), diagnostics.Num(), 0);

	FExperimentSampleSelection selection;
	selection.Kind = EExperimentSampleSelectionKind::All;
	const FExperimentRunInputBuildResult result =
		FExperimentSettingJson::BuildRunInputsFromExperiment(experimentRef, selection);
	TestTrue(TEXT("run inputs build"), result.bSuccess);
	TestEqual(TEXT("build diagnostics"), result.Diagnostics.Num(), 0);
	TestEqual(TEXT("run input count"), result.RunInputs.Num(), 1);
	TestEqual(TEXT("sample path count"), result.ScenarioSampleJsonPaths.Num(), 1);

	const FString expectedProfilePath = FExperimentSettingJson::BuildExperimentProfilePath(experimentRef);
	FString expectedSamplePath = FPaths::Combine(
		FExperimentSettingJson::BuildExperimentScenariosDirectory(experimentRef),
		TEXT("000001.json"));
	FPaths::NormalizeFilename(expectedSamplePath);
	if (result.RunInputs.Num() > 0)
	{
		TestEqual(TEXT("run input pair id"), result.RunInputs[0].PairId, FString(TEXT("000001")));
		TestEqual(TEXT("run input profile"), result.RunInputs[0].SimulationProfileJsonPath, expectedProfilePath);
		TestEqual(TEXT("run input sample"), result.RunInputs[0].ScenarioSourceJsonPath, expectedSamplePath);
	}
	TestTrue(
		TEXT("profile materialized"),
		FPaths::FileExists(FExperimentSettingJson::ResolveProjectPath(expectedProfilePath)));
	TestTrue(
		TEXT("scenario sample materialized"),
		FPaths::FileExists(FExperimentSettingJson::ResolveProjectPath(expectedSamplePath)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExperimentRunCommandLineParseTest,
	"OdiroSim.ExperimentSetting.CommandLine.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExperimentRunCommandLineParseTest::RunTest(const FString& parameters)
{
	const FExperimentRunCommandLineParseResult experimentResult =
		FExperimentRunCommandLine::Parse(
			TEXT("-unattended -Experiment=Json/Experiments/FeatureProbeNoPedestrians -RunId=run-001 -SampleIds=000001,000002"));

	TestTrue(TEXT("experiment command succeeds"), experimentResult.bSuccess);
	TestTrue(TEXT("experiment run enabled"), experimentResult.Options.bExperimentRun);
	TestEqual(
		TEXT("experiment ref"),
		experimentResult.Options.Request.ExperimentRef,
		FString(TEXT("Json/Experiments/FeatureProbeNoPedestrians")));
	TestEqual(TEXT("run id"), experimentResult.Options.Request.RunId, FString(TEXT("run-001")));
	TestEqual(
		TEXT("explicit sample selection"),
		static_cast<int32>(experimentResult.Options.Request.SampleSelection.Kind),
		static_cast<int32>(EExperimentSampleSelectionKind::ExplicitIds));
	TestEqual(TEXT("sample id count"), experimentResult.Options.Request.SampleSelection.SampleIds.Num(), 2);
	if (experimentResult.Options.Request.SampleSelection.SampleIds.Num() == 2)
	{
		TestEqual(
			TEXT("first sample id"),
			experimentResult.Options.Request.SampleSelection.SampleIds[0],
			FString(TEXT("000001")));
		TestEqual(
			TEXT("second sample id"),
			experimentResult.Options.Request.SampleSelection.SampleIds[1],
			FString(TEXT("000002")));
	}

	const FExperimentRunCommandLineParseResult nonExperimentResult =
		FExperimentRunCommandLine::Parse(TEXT("-unattended -NoSplash"));
	TestTrue(TEXT("non-experiment command succeeds"), nonExperimentResult.bSuccess);
	TestFalse(TEXT("non-experiment command is not a run"), nonExperimentResult.Options.bExperimentRun);

	const FExperimentRunCommandLineParseResult missingValueResult =
		FExperimentRunCommandLine::Parse(TEXT("-Experiment -RunId=run-001"));
	TestFalse(TEXT("missing experiment value fails"), missingValueResult.bSuccess);
	TestTrue(
		TEXT("missing experiment value diagnostic"),
		HasSchemaDiagnosticCode(missingValueResult.Diagnostics, TEXT("missing_experiment_value")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExperimentRunOutputPathsTest,
	"OdiroSim.ExperimentSetting.Json.RunOutputPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExperimentRunOutputPathsTest::RunTest(const FString& parameters)
{
	const FString experimentRef = TEXT("Json/Experiments/FeatureProbeNoPedestrians");

	TestEqual(
		TEXT("run output directory"),
		FExperimentSettingJson::BuildExperimentRunDirectory(experimentRef, TEXT("run-001")),
		FString(TEXT("Json/Experiments/FeatureProbeNoPedestrians/runs/run-001")));
	TestEqual(
		TEXT("run status path"),
		FExperimentSettingJson::BuildExperimentRunStatusPath(experimentRef, TEXT("run-001")),
		FString(TEXT("Json/Experiments/FeatureProbeNoPedestrians/runs/run-001/status.json")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationRunStatusJsonWriteTest,
	"OdiroSim.SimulationRunStatus.Json.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationRunStatusJsonWriteTest::RunTest(const FString& parameters)
{
	FSimulationRunStatus status;
	status.RunId = TEXT("sample-run-001");
	status.State = ESimulationRunState::Running;
	status.SetupPath = TEXT("Json/Experiments/FeatureProbeNoPedestrians");
	status.UpdatedAt = TEXT("2026-06-05T00:00:00Z");
	status.CurrentPairId = TEXT("sample_0");
	status.CompletedRuns = 1;
	status.TotalRuns = 5;
	status.ResultPaths.Add(TEXT("Json/Output/sample_report.json"));
	status.ResultPaths.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(
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
	TestTrue(TEXT("result path"), json.Contains(TEXT("Json/Output/sample_report.json")));
	TestTrue(TEXT("absolute result path is written project-relative"), json.Contains(TEXT("Json/Output/absolute_sample_report.json")));
	TestFalse(TEXT("result path does not include project root"), json.Contains(normalizedProjectDir));
	TestTrue(TEXT("log path"), json.Contains(TEXT("Saved/AnalysisLogs/sample.jsonl")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulationRunStatusJsonReadTest,
	"OdiroSim.SimulationRunStatus.Json.Read",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulationRunStatusJsonReadTest::RunTest(const FString& parameters)
{
	const FString json =
		TEXT("{")
		TEXT("\"schema\":\"simulation_run_status\",")
		TEXT("\"version\":1,")
		TEXT("\"run_id\":\"sample-run-001\",")
		TEXT("\"state\":\"Completed\",")
		TEXT("\"setup_path\":\"Json/Experiments/FeatureProbeNoPedestrians\",")
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
	TestEqual(TEXT("result count"), status.ResultPaths.Num(), 1);
	TestEqual(TEXT("log count"), status.LogPaths.Num(), 1);
	TestTrue(TEXT("nullable current pair"), status.CurrentPairId.IsEmpty());
	TestTrue(TEXT("nullable error"), status.Error.IsEmpty());

	return true;
}

#endif
