#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EpisodeEvaluationReportJson.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ParseReportJson(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	FScenarioParamValue MakeReportStringParam(const FString& Value)
	{
		FScenarioParamValue Param;
		Param.Type = EScenarioParamValueType::String;
		Param.StringValue = Value;
		return Param;
	}

	FScenarioParamValue MakeReportFloatParam(double Value)
	{
		FScenarioParamValue Param;
		Param.Type = EScenarioParamValueType::Float;
		Param.FloatValue = Value;
		return Param;
	}

	FScenarioParamValue MakeReportVectorParam(const FVector& Value)
	{
		FScenarioParamValue Param;
		Param.Type = EScenarioParamValueType::Vector;
		Param.VectorValue = Value;
		return Param;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEpisodeEvaluationReportJsonSerializationTest,
	"OdiroSim.EpisodeEvaluationReport.JsonSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEpisodeEvaluationReportJsonSerializationTest::RunTest(const FString& Parameters)
{
	FEpisodeRunRecord Record;
	Record.RunId = TEXT("episode_run_0004");
	Record.RunIndex = 4;
	Record.EpisodeId = TEXT("sensor_route_layout_004");
	Record.PairId = TEXT("sample_4");
	Record.ScenarioSourceJsonPath = TEXT("Json/Input/ScenarioTemplates/FeatureProbeNoPedestrians.template.json");
	Record.ScenarioSourceHash = TEXT("252738887");
	Record.SimulationProfileJsonPath = TEXT("Json/Input/ScenarioTemplates/TemplateProfileForTest.json");
	Record.SimulationProfileHash = TEXT("109204312");
	Record.PairHash = TEXT("83029122");
	Record.bCompileSucceeded = true;
	Record.bScenarioSourceCompileSucceeded = true;
	Record.bSimulationProfileCompileSucceeded = true;
	Record.bSetupSucceeded = true;
	Record.bEvaluationCompleted = true;
	Record.bSuccess = false;
	Record.Outcome = EEpisodeEvaluationOutcome::Failure;
	Record.TerminalReason = EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed;
	Record.DurationSeconds = 38.6;

	Record.EvaluationResult.EpisodeId = Record.EpisodeId;
	Record.EvaluationResult.bCompleted = true;
	Record.EvaluationResult.bSuccess = false;
	Record.EvaluationResult.Outcome = Record.Outcome;
	Record.EvaluationResult.TerminalReason = Record.TerminalReason;
	Record.EvaluationResult.DurationSeconds = Record.DurationSeconds;
	Record.EvaluationResult.Metrics.Add(TEXT("score"), MakeReportFloatParam(-7.0));
	Record.EvaluationResult.Metrics.Add(TEXT("duration_s"), MakeReportFloatParam(38.6));
	Record.EvaluationResult.Metrics.Add(TEXT("delivery_bot_failure_type"), MakeReportStringParam(TEXT("Stuck")));
	Record.EvaluationResult.Metrics.Add(TEXT("delivery_bot_failure_location_cm"), MakeReportVectorParam(FVector(380.0, 140.0, 50.0)));
	Record.EvaluationResult.Metrics.Add(TEXT("delivery_bot_failure_time_seconds"), MakeReportFloatParam(38.6));
	Record.EvaluationResult.Metrics.Add(TEXT("delivery_bot_failure_speed_kmh"), MakeReportFloatParam(0.1));
	Record.EvaluationResult.Metrics.Add(TEXT("policy_request_id"), MakeReportStringParam(TEXT("hidden_request")));

	FEpisodeEvaluationEvent NearMissEvent;
	NearMissEvent.EventIndex = 0;
	NearMissEvent.ElapsedTimeSeconds = 12.4;
	NearMissEvent.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
	NearMissEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
	NearMissEvent.SubjectInstanceId = TEXT("robot_01");
	NearMissEvent.TargetInstanceId = TEXT("ped_01");
	NearMissEvent.Location = FVector(120.0, 70.0, 0.0);
	NearMissEvent.Value = -3.0;
	NearMissEvent.Message = TEXT("Pedestrian near miss");
	NearMissEvent.Properties.Add(TEXT("duration_s"), MakeReportFloatParam(0.8));
	NearMissEvent.Properties.Add(TEXT("min_distance_m"), MakeReportFloatParam(0.42));
	Record.EvaluationResult.Events.Add(NearMissEvent);

	FEpisodeEvaluationEvent FailureEvent;
	FailureEvent.EventIndex = 1;
	FailureEvent.ElapsedTimeSeconds = 38.6;
	FailureEvent.EventType = EEpisodeEvaluationEventType::DeliveryBotSimulationFailure;
	FailureEvent.Severity = EEpisodeEvaluationEventSeverity::Failure;
	FailureEvent.SubjectInstanceId = TEXT("robot_01");
	FailureEvent.Location = FVector(380.0, 140.0, 50.0);
	FailureEvent.Message = TEXT("DeliveryBot remained below movement threshold.");
	FailureEvent.Properties.Add(TEXT("delivery_bot_failure_type"), MakeReportStringParam(TEXT("Stuck")));
	FailureEvent.Properties.Add(TEXT("delivery_bot_failure_speed_kmh"), MakeReportFloatParam(0.1));
	FailureEvent.Properties.Add(TEXT("delivery_bot_failure_target_actor_name"), MakeReportStringParam(FString()));
	FailureEvent.Properties.Add(TEXT("policy_request_id"), MakeReportStringParam(TEXT("hidden_request")));
	Record.EvaluationResult.Events.Add(FailureEvent);

	FString Json;
	TArray<FString> Diagnostics;
	TestTrue(TEXT("report serializes"), FEpisodeEvaluationReportJson::TryWriteReportJson(Record, Json, Diagnostics));
	TestEqual(TEXT("report diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RootObject;
	TestTrue(TEXT("report parses"), ParseReportJson(Json, RootObject));
	if (!RootObject.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("schema"), RootObject->GetStringField(TEXT("schema")), FString(TEXT("episode_evaluation_report")));

	const TSharedPtr<FJsonObject> RunObject = RootObject->GetObjectField(TEXT("run"));
	TestTrue(TEXT("run has scenario source"), RunObject->HasField(TEXT("scenario_source")));
	TestTrue(TEXT("run has simulation profile"), RunObject->HasField(TEXT("simulation_profile")));
	TestFalse(TEXT("run omits legacy episode setup"), RunObject->HasField(TEXT("episode_setup")));
	TestFalse(TEXT("run omits legacy delivery setup"), RunObject->HasField(TEXT("delivery_bot_setup")));

	const TSharedPtr<FJsonObject> PipelineObject = RootObject->GetObjectField(TEXT("pipeline"));
	TestTrue(TEXT("pipeline has scenario source compile"), PipelineObject->HasField(TEXT("scenario_source_compiled")));
	TestTrue(TEXT("pipeline has simulation profile compile"), PipelineObject->HasField(TEXT("simulation_profile_compiled")));
	TestFalse(TEXT("pipeline omits legacy episode setup compile"), PipelineObject->HasField(TEXT("episode_setup_compiled")));
	TestFalse(TEXT("pipeline omits legacy delivery setup compile"), PipelineObject->HasField(TEXT("delivery_bot_setup_compiled")));

	const TSharedPtr<FJsonObject> SummaryObject = RootObject->GetObjectField(TEXT("summary"));
	TestTrue(TEXT("usable for llm tuning"), SummaryObject->GetBoolField(TEXT("usable_for_llm_tuning")));

	const TSharedPtr<FJsonObject> MetricsObject = RootObject->GetObjectField(TEXT("metrics"));
	TestFalse(TEXT("metrics omit raw cm location"), MetricsObject->HasField(TEXT("delivery_bot_failure_location_cm")));
	TestFalse(TEXT("metrics omit policy request id"), MetricsObject->HasField(TEXT("policy_request_id")));
	TestTrue(TEXT("metrics include xy_m"), MetricsObject->HasField(TEXT("delivery_bot_failure_xy_m")));
	TestTrue(TEXT("metrics include time_s"), MetricsObject->HasField(TEXT("delivery_bot_failure_time_s")));
	const TArray<TSharedPtr<FJsonValue>>& FailureLocation = MetricsObject->GetArrayField(TEXT("delivery_bot_failure_xy_m"));
	TestTrue(TEXT("failure x m"), FMath::IsNearlyEqual(FailureLocation[0]->AsNumber(), 3.8));
	TestTrue(TEXT("failure y m"), FMath::IsNearlyEqual(FailureLocation[1]->AsNumber(), 1.4));

	const TSharedPtr<FJsonObject> EventSummaryObject = RootObject->GetObjectField(TEXT("event_summary"));
	const TSharedPtr<FJsonObject> ByTypeObject = EventSummaryObject->GetObjectField(TEXT("by_type"));
	TestEqual(TEXT("delivery bot failure count"), static_cast<int32>(ByTypeObject->GetNumberField(TEXT("DeliveryBotSimulationFailure"))), 1);
	TestEqual(TEXT("first failure index"), static_cast<int32>(EventSummaryObject->GetNumberField(TEXT("first_failure_event_index"))), 1);

	const TArray<TSharedPtr<FJsonValue>>& Events = RootObject->GetArrayField(TEXT("events"));
	TestEqual(TEXT("event count"), Events.Num(), 2);
	const TSharedPtr<FJsonObject> NearMissObject = Events[0]->AsObject();
	const TSharedPtr<FJsonObject> NearMissProperties = NearMissObject->GetObjectField(TEXT("properties"));
	TestTrue(TEXT("near miss score delta"), FMath::IsNearlyEqual(NearMissProperties->GetNumberField(TEXT("score_delta")), -3.0));

	const TSharedPtr<FJsonObject> FailureObject = Events[1]->AsObject();
	const TSharedPtr<FJsonObject> FailureProperties = FailureObject->GetObjectField(TEXT("properties"));
	TestEqual(TEXT("failure type"), FailureProperties->GetStringField(TEXT("failure_type")), FString(TEXT("Stuck")));
	TestFalse(TEXT("failure properties omit policy request id"), FailureProperties->HasField(TEXT("policy_request_id")));

	return true;
}

#endif
