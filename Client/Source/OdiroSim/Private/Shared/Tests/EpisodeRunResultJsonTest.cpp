#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EpisodeRunResultJson.h"

#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Shared/ExperimentSettingTypes.h"

namespace
{
	FScenarioParamValue MakeRunResultTestFloatParam(double Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::Float;
		ParamValue.FloatValue = Value;
		return ParamValue;
	}

	FScenarioParamValue MakeRunResultTestStringParam(const FString& Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::String;
		ParamValue.StringValue = Value;
		return ParamValue;
	}

	bool BuildRunResultTestInput(FScenarioRunInput& OutRunInput)
	{
		const FString ExperimentRef = FString::Printf(
			TEXT("Saved/Automation/OdiroSim/RunResult/%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));

		FExperimentSettingDocument Document;
		Document.ExperimentId = TEXT("run_result_test");
		Document.Sampling.ScenarioTemplateRef = TEXT("Json/Templates/Scenarios/FeatureProbeNoPedestrians.template.json");
		Document.Sampling.ProfileTemplateRef = TEXT("Json/Templates/Profiles/TemplateProfileForTest.json");
		Document.Sampling.BaseSeed = 260615001;
		Document.Sampling.SampleCount = 1;
		Document.Runtime.MapId = TEXT("ScenarioSimulationMap");
		Document.Runtime.FixedFps = 60;

		TArray<FScenarioSchemaDiagnostic> Diagnostics;
		if (!FExperimentSettingJson::SaveToFile(
				Document,
				FExperimentSettingJson::BuildExperimentSettingPath(ExperimentRef),
				Diagnostics))
		{
			return false;
		}

		FExperimentSampleSelection Selection;
		Selection.Kind = EExperimentSampleSelectionKind::All;
		const FExperimentRunInputBuildResult BuildResult =
			FExperimentSettingJson::BuildRunInputsFromExperiment(ExperimentRef, Selection);
		if (!BuildResult.bSuccess || BuildResult.RunInputs.Num() != 1)
		{
			return false;
		}

		OutRunInput = BuildResult.RunInputs[0];
		return true;
	}

	FEpisodeRunRecord MakeRunResultTestRecord(const FScenarioRunInput& RunInput)
	{
		FEpisodeRunRecord Record;
		Record.RunId = TEXT("run-result-test");
		Record.RunIndex = 0;
		Record.EpisodeId = TEXT("feature_probe_no_pedestrians_000001");
		Record.PairId = RunInput.PairId;
		Record.SourceJsonPath = RunInput.ScenarioSourceJsonPath;
		Record.ScenarioSourceJsonPath = RunInput.ScenarioSourceJsonPath;
		Record.SimulationProfileJsonPath = RunInput.SimulationProfileJsonPath;
		Record.SpecHash = TEXT("hash:world");
		Record.ScenarioSourceHash = TEXT("hash:world");
		Record.SimulationProfileHash = TEXT("hash:profile");
		Record.PairHash = TEXT("hash:pair");
		Record.bCompileSucceeded = true;
		Record.bScenarioSourceCompileSucceeded = true;
		Record.bSimulationProfileCompileSucceeded = true;
		Record.bSetupSucceeded = true;
		Record.bEvaluationCompleted = true;
		Record.bSuccess = true;
		Record.Outcome = EEpisodeEvaluationOutcome::Success;
		Record.TerminalReason = EEpisodeEvaluationTerminalReason::GoalReached;
		Record.DurationSeconds = 12.5;

		Record.EvaluationResult.EpisodeId = Record.EpisodeId;
		Record.EvaluationResult.bCompleted = true;
		Record.EvaluationResult.bSuccess = true;
		Record.EvaluationResult.Outcome = Record.Outcome;
		Record.EvaluationResult.TerminalReason = Record.TerminalReason;
		Record.EvaluationResult.DurationSeconds = Record.DurationSeconds;
		Record.EvaluationResult.Metrics.Add(TEXT("goal_reached"), MakeRunResultTestFloatParam(1.0));
		Record.EvaluationResult.Metrics.Add(TEXT("goal_distance_m"), MakeRunResultTestFloatParam(0.1));
		Record.EvaluationResult.Metrics.Add(TEXT("score"), MakeRunResultTestFloatParam(42.0));

		FEpisodeEvaluationEvent NearMissEvent;
		NearMissEvent.EventIndex = 0;
		NearMissEvent.ElapsedTimeSeconds = 3.25;
		NearMissEvent.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
		NearMissEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
		NearMissEvent.SubjectInstanceId = TEXT("robot_01");
		NearMissEvent.TargetInstanceId = TEXT("ped_01");
		NearMissEvent.Message = TEXT("Robot passed close to a pedestrian.");
		NearMissEvent.Properties.Add(TEXT("pedestrian_id"), MakeRunResultTestStringParam(TEXT("ped_01")));
		NearMissEvent.Properties.Add(TEXT("min_distance_m"), MakeRunResultTestFloatParam(0.32));
		Record.EvaluationResult.Events.Add(NearMissEvent);

		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeRunResultJsonWriteTest,
	"OdiroSim.EpisodeRunResult.Json.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeRunResultJsonWriteTest::RunTest(const FString& Parameters)
{
	FScenarioRunInput RunInput;
	TestTrue(TEXT("run input builds"), BuildRunResultTestInput(RunInput));
	if (RunInput.ScenarioSourceJsonPath.IsEmpty())
	{
		return false;
	}

	const FEpisodeRunRecord Record = MakeRunResultTestRecord(RunInput);

	FString ResultJson;
	TArray<FString> ResultDiagnostics;
	TestTrue(
		TEXT("episode result writes"),
		FEpisodeRunResultJson::TryWriteEpisodeResultJson(Record, ResultJson, ResultDiagnostics));
	TestEqual(TEXT("result diagnostics"), ResultDiagnostics.Num(), 0);
	TestTrue(TEXT("result schema"), ResultJson.Contains(TEXT("\"schema\": \"episode_result\"")));
	TestTrue(TEXT("result has sample id"), ResultJson.Contains(TEXT("\"sample_id\": \"000001\"")));
	TestTrue(TEXT("result has terminal event index"), ResultJson.Contains(TEXT("\"terminal_event_index\": 1")));
	TestFalse(TEXT("result omits legacy score"), ResultJson.Contains(TEXT("\"score\"")));

	FString EventsJsonl;
	TArray<FString> EventDiagnostics;
	TestTrue(
		TEXT("episode events write"),
		FEpisodeRunResultJson::TryWriteEpisodeEventsJsonl(Record, EventsJsonl, EventDiagnostics));
	TestEqual(TEXT("event diagnostics"), EventDiagnostics.Num(), 0);
	TestTrue(TEXT("events contain near miss"), EventsJsonl.Contains(TEXT("\"event_type\":\"PedestrianNearMiss\"")));
	TestTrue(TEXT("events contain synthetic goal reached"), EventsJsonl.Contains(TEXT("\"event_type\":\"GoalReached\"")));
	TestFalse(TEXT("events omit severity"), EventsJsonl.Contains(TEXT("\"severity\"")));

	FString SummaryJson;
	TArray<FString> SummaryDiagnostics;
	TArray<FEpisodeRunRecord> Records;
	Records.Add(Record);
	TestTrue(
		TEXT("run summary writes"),
		FEpisodeRunResultJson::TryWriteRunSummaryJson(Records, SummaryJson, SummaryDiagnostics));
	TestEqual(TEXT("summary diagnostics"), SummaryDiagnostics.Num(), 0);
	TestTrue(TEXT("summary schema"), SummaryJson.Contains(TEXT("\"schema\": \"run_summary\"")));
	TestTrue(TEXT("summary rows"), SummaryJson.Contains(TEXT("\"rows\"")));
	TestTrue(TEXT("summary has scenario semantic"), SummaryJson.Contains(TEXT("\"scenario_semantic\"")));

	return true;
}

#endif
