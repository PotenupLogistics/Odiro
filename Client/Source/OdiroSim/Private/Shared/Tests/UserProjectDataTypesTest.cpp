#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/UserProjectDataTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool HasUserProjectDiagnosticCode(
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

	bool SaveUserProjectTestFile(const FString& filePath, const FString& contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(filePath), true);
		return FFileHelper::SaveStringToFile(contents, *filePath);
	}

	FString MakeUserProjectDataTestRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/UserProjectData"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	bool WriteUserProjectDataSnapshot(const FString& projectPath, FUserProjectRunSnapshotPaths& outPaths)
	{
		outPaths = FUserProjectRunSnapshot::BuildPaths(projectPath, TEXT("000001"));
		IFileManager::Get().MakeDirectory(*outPaths.ReviewPath, true);
		IFileManager::Get().MakeDirectory(*outPaths.EpisodesPath, true);
		IFileManager::Get().MakeDirectory(*outPaths.PolicyPath, true);

		return SaveUserProjectTestFile(
				outPaths.SettingPath,
				TEXT("{")
				TEXT("\"schema\":\"project_setting\",")
				TEXT("\"version\":1,")
				TEXT("\"project_id\":\"automation_project\",")
				TEXT("\"sampling\":{\"base_seed\":1234,\"episode_count\":2,\"generator_version\":\"0.1.0\"},")
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":30,\"time_scale\":1.0,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.0,\"tip_over_angle_deg\":60,\"near_miss_distance_m\":0.5}")
				TEXT("}"))
			&& SaveUserProjectTestFile(
				outPaths.ProfilePath,
				TEXT("{")
				TEXT("\"schema\":\"simulation_profile\",")
				TEXT("\"version\":1,")
				TEXT("\"profile_id\":\"automation_profile\",")
				TEXT("\"display_name\":\"Automation\",")
				TEXT("\"description\":\"Automation profile\",")
				TEXT("\"robot\":{\"body\":{},\"drive\":{},\"lidar\":{}}")
				TEXT("}"))
			&& SaveUserProjectTestFile(
				outPaths.ScenarioPath,
				TEXT("{")
				TEXT("\"schema\":\"scenario\",")
				TEXT("\"version\":1,")
				TEXT("\"scenario_id\":\"automation_scenario\",")
				TEXT("\"intent\":\"Automation\",")
				TEXT("\"corridor\":{\"segments\":[{\"id\":\"main\",\"walkway_width_m\":{\"min\":2.5,\"max\":4.0}}]},")
				TEXT("\"obstacles\":{\"placements\":[{\"id\":\"obstacle_1\",\"prop\":{\"choices\":[\"bench\",\"cone\"]}}]},")
				TEXT("\"pedestrians\":{\"background\":{\"count\":{\"choices\":[0,1,2]}},\"encounters\":[]},")
				TEXT("\"robot\":{\"start\":{\"segment\":\"main\"},\"goal\":{\"segment\":\"main\"}}")
				TEXT("}"))
			&& SaveUserProjectTestFile(
				outPaths.PolicyEntrypointPath,
				TEXT("def create_policy():\n    return None\n"));
	}

	bool LoadUserProjectJsonObject(const FString& filePath, TSharedPtr<FJsonObject>& outObject)
	{
		FString json;
		if (!FFileHelper::LoadFileToString(json, *filePath))
		{
			return false;
		}

		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(json);
		return FJsonSerializer::Deserialize(reader, outObject) && outObject.IsValid();
	}

	FScenarioParamValue MakeUserProjectFloatParam(double value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Float;
		paramValue.FloatValue = value;
		return paramValue;
	}

	int32 CountJsonlLines(const FString& filePath)
	{
		FString contents;
		if (!FFileHelper::LoadFileToString(contents, *filePath))
		{
			return INDEX_NONE;
		}

		TArray<FString> lines;
		contents.ParseIntoArrayLines(lines, true);
		return lines.Num();
	}

	TSharedRef<FJsonObject> MakeActionTestRequestObject()
	{
		TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();
		requestObject->SetNumberField(TEXT("sequence"), 7);
		requestObject->SetNumberField(TEXT("runTimeSeconds"), 3.5);

		TSharedRef<FJsonObject> robotStateObject = MakeShared<FJsonObject>();
		robotStateObject->SetNumberField(TEXT("x"), 10.0);
		robotStateObject->SetNumberField(TEXT("y"), 20.0);
		robotStateObject->SetNumberField(TEXT("z"), 0.0);
		robotStateObject->SetNumberField(TEXT("yawDegree"), 90.0);
		robotStateObject->SetNumberField(TEXT("speedKmh"), 4.0);
		requestObject->SetObjectField(TEXT("robotState"), robotStateObject);

		TArray<TSharedPtr<FJsonValue>> lidarRayValues;
		TSharedRef<FJsonObject> leftRayObject = MakeShared<FJsonObject>();
		leftRayObject->SetBoolField(TEXT("hit"), false);
		leftRayObject->SetNumberField(TEXT("distanceM"), 10.0);
		leftRayObject->SetNumberField(TEXT("rayIndex"), 0);
		leftRayObject->SetNumberField(TEXT("rayYawDegree"), -45.0);
		lidarRayValues.Add(MakeShared<FJsonValueObject>(leftRayObject));
		TSharedRef<FJsonObject> rightRayObject = MakeShared<FJsonObject>();
		rightRayObject->SetBoolField(TEXT("hit"), true);
		rightRayObject->SetNumberField(TEXT("distanceM"), 4.0);
		rightRayObject->SetNumberField(TEXT("rayIndex"), 1);
		rightRayObject->SetNumberField(TEXT("rayYawDegree"), 45.0);
		lidarRayValues.Add(MakeShared<FJsonValueObject>(rightRayObject));
		requestObject->SetArrayField(TEXT("lidarRays"), lidarRayValues);

		TArray<TSharedPtr<FJsonValue>> observedObjectValues;
		TSharedRef<FJsonObject> observedObject = MakeShared<FJsonObject>();
		observedObject->SetStringField(TEXT("actorName"), TEXT("pedestrian_01"));
		observedObject->SetNumberField(TEXT("closestDistanceM"), 4.0);
		observedObjectValues.Add(MakeShared<FJsonValueObject>(observedObject));
		requestObject->SetArrayField(TEXT("observedObjects"), observedObjectValues);

		return requestObject;
	}

	TSharedRef<FJsonObject> MakeActionTestResponseObject()
	{
		TSharedRef<FJsonObject> responseObject = MakeShared<FJsonObject>();
		responseObject->SetStringField(TEXT("status"), TEXT("ok"));

		TSharedRef<FJsonObject> actionObject = MakeShared<FJsonObject>();
		actionObject->SetNumberField(TEXT("steering"), 0.25);
		actionObject->SetNumberField(TEXT("targetSpeedKmh"), 6.0);
		actionObject->SetNumberField(TEXT("brake"), 0.0);
		actionObject->SetStringField(TEXT("direction"), TEXT("Forward"));
		responseObject->SetObjectField(TEXT("action"), actionObject);

		TSharedRef<FJsonObject> debugObject = MakeShared<FJsonObject>();
		debugObject->SetNumberField(TEXT("pathIndex"), 3);
		responseObject->SetObjectField(TEXT("debug"), debugObject);
		return responseObject;
	}

	FEpisodeMeasurementLogTickRecord MakeTraceTestTickRecord()
	{
		FEpisodeMeasurementLogTickRecord tickRecord;
		tickRecord.TickIndex = 5;
		tickRecord.WorldTimeSeconds = 4.5;
		tickRecord.DeltaSeconds = 0.033;
		tickRecord.Robot.Id = TEXT("robot_01");
		tickRecord.Robot.Truth.PositionCm = FVector(100.0, 200.0, 0.0);
		tickRecord.Robot.Truth.RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };
		tickRecord.Robot.Truth.VelocityCmPerSecond = FVector(10.0, 0.0, 0.0);

		FEpisodeMeasurementLogActorState pedestrianState;
		pedestrianState.ActorIndex = 2;
		pedestrianState.PositionCm = FVector(150.0, 250.0, 0.0);
		pedestrianState.RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };
		pedestrianState.VelocityCmPerSecond = FVector(0.0, 20.0, 0.0);
		tickRecord.MovingActors.Add(pedestrianState);
		return tickRecord;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectDataRootContractTest,
	"OdiroSim.UserProjectData.RootContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectDataRootContractTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeUserProjectDataTestRoot();
	const FString settingPath = FPaths::Combine(projectPath, TEXT("setting.json"));
	const FString settingJson = TEXT("{\"schema\":\"project_setting\",\"version\":1,\"project_id\":\"automation\"}");

	TArray<FScenarioCompileDiagnostic> saveDiagnostics;
	TestTrue(
		TEXT("setting root saves"),
		FUserProjectDataJson::SaveRootJsonFile(settingPath, settingJson, TEXT("project_setting"), saveDiagnostics));
	TestEqual(TEXT("save diagnostics"), saveDiagnostics.Num(), 0);

	const FUserProjectJsonParseResult settingResult =
		FUserProjectDataJson::ValidateRootJsonFile(settingPath, TEXT("project_setting"));
	TestTrue(TEXT("setting root reads"), settingResult.bSuccess);
	TestEqual(TEXT("setting schema"), settingResult.Schema, FString(TEXT("project_setting")));

	const FUserProjectJsonParseResult invalidResult =
		FUserProjectDataJson::ValidateRootJsonString(settingJson, TEXT("scenario"));
	TestFalse(TEXT("invalid schema fails"), invalidResult.bSuccess);
	TestTrue(TEXT("invalid schema diagnostic"), HasUserProjectDiagnosticCode(invalidResult.Diagnostics, TEXT("invalid_schema")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectEpisodeScenarioWriteTest,
	"OdiroSim.UserProjectData.EpisodeScenario.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectEpisodeScenarioWriteTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeUserProjectDataTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write snapshot"), WriteUserProjectDataSnapshot(projectPath, paths));

	const FUserProjectRunSnapshotParseResult snapshotResult = FUserProjectRunSnapshot::Parse(projectPath, TEXT("000001"));
	TestTrue(TEXT("snapshot parses"), snapshotResult.bSuccess);
	TestEqual(TEXT("base seed"), snapshotResult.Setting.BaseSeed, static_cast<int64>(1234));
	TestEqual(TEXT("generator version"), snapshotResult.Setting.GeneratorVersion, FString(TEXT("0.1.0")));

	TArray<FUserProjectEpisodeScenarioWriteResult> writeResults;
	TArray<FScenarioCompileDiagnostic> writeDiagnostics;
	TestTrue(
		TEXT("episode scenarios write"),
		FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
			snapshotResult.Paths,
			snapshotResult.Setting,
			writeResults,
			writeDiagnostics));
	TestEqual(TEXT("write diagnostics"), writeDiagnostics.Num(), 0);
	TestEqual(TEXT("episode count"), writeResults.Num(), 2);
	TestEqual(TEXT("first episode id"), writeResults[0].EpisodeId, FString(TEXT("000001")));
	TestEqual(TEXT("first seed"), writeResults[0].Seed, static_cast<int64>(1234));
	TestFalse(TEXT("scenario hash populated"), writeResults[0].ScenarioHash.IsEmpty());

	const FUserProjectEpisodeScenarioParseResult parseResult =
		FUserProjectEpisodeScenarioJson::ParseFromFile(writeResults[0].ScenarioPath);
	TestTrue(TEXT("episode scenario parses"), parseResult.bSuccess);
	TestEqual(TEXT("parsed episode id"), parseResult.EpisodeId, FString(TEXT("000001")));
	TestEqual(TEXT("parsed seed"), parseResult.Seed, static_cast<int64>(1234));

	TSharedPtr<FJsonObject> episodeObject;
	TestTrue(TEXT("load episode json"), LoadUserProjectJsonObject(writeResults[0].ScenarioPath, episodeObject));
	const TSharedPtr<FJsonObject> paramsObject = episodeObject->GetObjectField(TEXT("params"));
	TestTrue(TEXT("range param recorded"), paramsObject->HasField(TEXT("$.corridor.segments[0].walkway_width_m")));
	TestTrue(TEXT("choice param recorded"), paramsObject->HasField(TEXT("$.obstacles.placements[0].prop")));

	const TSharedPtr<FJsonObject> semanticObject = episodeObject->GetObjectField(TEXT("semantic"));
	const TArray<TSharedPtr<FJsonValue>> segments =
		semanticObject->GetObjectField(TEXT("corridor"))->GetArrayField(TEXT("segments"));
	const TSharedPtr<FJsonValue> walkwayWidth =
		segments[0]->AsObject()->TryGetField(TEXT("walkway_width_m"));
	TestTrue(TEXT("random range materialized to number"), walkwayWidth.IsValid() && walkwayWidth->Type == EJson::Number);

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectRunOutputWriteTest,
	"OdiroSim.UserProjectData.RunOutput.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectRunOutputWriteTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeUserProjectDataTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write snapshot"), WriteUserProjectDataSnapshot(projectPath, paths));

	const FUserProjectRunSnapshotParseResult snapshotResult = FUserProjectRunSnapshot::Parse(projectPath, TEXT("000001"));
	TestTrue(TEXT("snapshot parses"), snapshotResult.bSuccess);

	TArray<FUserProjectEpisodeScenarioWriteResult> writeResults;
	TArray<FScenarioCompileDiagnostic> writeDiagnostics;
	TestTrue(
		TEXT("episode scenarios write"),
		FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
			snapshotResult.Paths,
			snapshotResult.Setting,
			writeResults,
			writeDiagnostics));
	if (writeResults.IsEmpty())
	{
		IFileManager::Get().DeleteDirectory(*projectPath, false, true);
		return false;
	}

	FEpisodeRunRecord runRecord;
	runRecord.RunId = TEXT("000001");
	runRecord.RunIndex = 0;
	runRecord.EpisodeId = writeResults[0].EpisodeId;
	runRecord.PairId = writeResults[0].EpisodeId;
	runRecord.EpisodeSetupJsonPath = writeResults[0].ScenarioPath;
	runRecord.DeliveryBotSetupJsonPath = snapshotResult.Paths.ProfilePath;
	runRecord.PolicySpecJsonPath = snapshotResult.Paths.PolicyEntrypointPath;
	runRecord.EpisodeSetupHash = writeResults[0].ScenarioHash;
	runRecord.DeliveryBotSetupHash = TEXT("crc32:profile");
	runRecord.PairHash = TEXT("crc32:policy");
	runRecord.bCompileSucceeded = true;
	runRecord.bEpisodeSetupCompileSucceeded = true;
	runRecord.bDeliveryBotSetupCompileSucceeded = true;
	runRecord.bSetupSucceeded = true;
	runRecord.bEvaluationCompleted = true;
	runRecord.bSuccess = true;
	runRecord.Outcome = EEpisodeEvaluationOutcome::Success;
	runRecord.TerminalReason = EEpisodeEvaluationTerminalReason::GoalReached;
	runRecord.DurationSeconds = 12.5;
	runRecord.EvaluationResult.EpisodeId = writeResults[0].EpisodeId;
	runRecord.EvaluationResult.bCompleted = true;
	runRecord.EvaluationResult.bSuccess = true;
	runRecord.EvaluationResult.Outcome = EEpisodeEvaluationOutcome::Success;
	runRecord.EvaluationResult.TerminalReason = EEpisodeEvaluationTerminalReason::GoalReached;
	runRecord.EvaluationResult.DurationSeconds = 12.5;
	runRecord.EvaluationResult.Metrics.Add(TEXT("score"), MakeUserProjectFloatParam(98.0));

	FEpisodeEvaluationEvent nearMissEvent;
	nearMissEvent.EventIndex = 0;
	nearMissEvent.ElapsedTimeSeconds = 4.25;
	nearMissEvent.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
	nearMissEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
	nearMissEvent.Message = TEXT("near miss");
	nearMissEvent.Properties.Add(TEXT("distance_m"), MakeUserProjectFloatParam(0.45));
	runRecord.EvaluationResult.Events.Add(nearMissEvent);

	TArray<FString> artifactDiagnostics;
	TestTrue(
		TEXT("episode artifacts write"),
		FUserProjectRunOutputJson::SaveEpisodeArtifacts(snapshotResult.Paths, runRecord, artifactDiagnostics));
	TestEqual(TEXT("artifact diagnostics"), artifactDiagnostics.Num(), 0);

	TArray<FEpisodeRunRecord> runRecords;
	runRecords.Add(runRecord);
	TArray<FString> summaryDiagnostics;
	TestTrue(
		TEXT("run summary writes"),
		FUserProjectRunOutputJson::SaveRunSummary(snapshotResult.Paths, runRecords, summaryDiagnostics));
	TestEqual(TEXT("summary diagnostics"), summaryDiagnostics.Num(), 0);

	const FString episodeDirectory = FUserProjectRunOutputJson::BuildEpisodeDirectory(snapshotResult.Paths, TEXT("000001"));
	const FString resultPath = FPaths::Combine(episodeDirectory, TEXT("result.json"));
	const FString eventsPath = FPaths::Combine(episodeDirectory, TEXT("events.jsonl"));
	TestTrue(TEXT("result exists"), FPaths::FileExists(resultPath));
	TestTrue(TEXT("events exists"), FPaths::FileExists(eventsPath));
	TestTrue(TEXT("actions exists"), FPaths::FileExists(FPaths::Combine(episodeDirectory, TEXT("actions.jsonl"))));
	TestTrue(TEXT("trace exists"), FPaths::FileExists(FPaths::Combine(episodeDirectory, TEXT("trace.jsonl"))));
	TestTrue(TEXT("summary exists"), FPaths::FileExists(snapshotResult.Paths.SummaryPath));
	TestEqual(TEXT("events include evaluation and terminal lines"), CountJsonlLines(eventsPath), 2);

	TSharedPtr<FJsonObject> resultObject;
	TestTrue(TEXT("load result json"), LoadUserProjectJsonObject(resultPath, resultObject));
	TestEqual(TEXT("result schema"), resultObject->GetStringField(TEXT("schema")), FString(TEXT("episode_result")));
	TestEqual(
		TEXT("result episode id"),
		resultObject->GetObjectField(TEXT("episode"))->GetStringField(TEXT("episode_id")),
		FString(TEXT("000001")));
	TestEqual(
		TEXT("event summary count"),
		static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("event_summary"))->GetNumberField(TEXT("event_count")))),
		2);

	FString eventsJson;
	TestTrue(TEXT("load events jsonl"), FFileHelper::LoadFileToString(eventsJson, *eventsPath));
	TestTrue(TEXT("events include terminal"), eventsJson.Contains(TEXT("\"event_type\":\"Terminal\"")));
	TestTrue(TEXT("events include terminal reason"), eventsJson.Contains(TEXT("\"reason\":\"GoalReached\"")));

	TSharedPtr<FJsonObject> summaryObject;
	TestTrue(TEXT("load summary json"), LoadUserProjectJsonObject(snapshotResult.Paths.SummaryPath, summaryObject));
	TestEqual(TEXT("summary schema"), summaryObject->GetStringField(TEXT("schema")), FString(TEXT("run_summary")));
	TestEqual(
		TEXT("result and summary policy hash match"),
		resultObject->GetObjectField(TEXT("run"))->GetStringField(TEXT("policy_snapshot_hash")),
		summaryObject->GetObjectField(TEXT("run"))->GetStringField(TEXT("policy_snapshot_hash")));
	const TArray<TSharedPtr<FJsonValue>> rows = summaryObject->GetArrayField(TEXT("rows"));
	TestEqual(TEXT("summary row count"), rows.Num(), 1);
	TestEqual(
		TEXT("summary scenario id"),
		rows[0]->AsObject()->GetStringField(TEXT("scenario_id")),
		FString(TEXT("automation_scenario")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectRobotActionWriteTest,
	"OdiroSim.UserProjectData.RobotAction.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectRobotActionWriteTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeUserProjectDataTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write snapshot"), WriteUserProjectDataSnapshot(projectPath, paths));

	TArray<FString> diagnostics;
	TestTrue(
		TEXT("append robot action"),
		FUserProjectRunOutputJson::AppendRobotActionRecord(
			paths,
			TEXT("000001"),
			MakeActionTestRequestObject(),
			MakeActionTestResponseObject(),
			true,
			200,
			FString(),
			diagnostics));
	TestEqual(TEXT("action diagnostics"), diagnostics.Num(), 0);

	const FString actionPath = FPaths::Combine(
		FUserProjectRunOutputJson::BuildEpisodeDirectory(paths, TEXT("000001")),
		TEXT("actions.jsonl"));
	TestTrue(TEXT("actions exists"), FPaths::FileExists(actionPath));
	TestEqual(TEXT("action line count"), CountJsonlLines(actionPath), 1);

	FString actionJsonl;
	TestTrue(TEXT("load action jsonl"), FFileHelper::LoadFileToString(actionJsonl, *actionPath));
	TestTrue(TEXT("action schema"), actionJsonl.Contains(TEXT("\"schema\":\"robot_action\"")));
	TestTrue(TEXT("action status"), actionJsonl.Contains(TEXT("\"status\":\"ok\"")));
	TestTrue(TEXT("action sequence"), actionJsonl.Contains(TEXT("\"sequence\":7")));
	TestTrue(TEXT("action angle"), actionJsonl.Contains(TEXT("\"front_half_angle_degree\":45")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectEpisodeTraceWriteTest,
	"OdiroSim.UserProjectData.EpisodeTrace.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectEpisodeTraceWriteTest::RunTest(const FString& parameters)
{
	const FString projectPath = MakeUserProjectDataTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write snapshot"), WriteUserProjectDataSnapshot(projectPath, paths));

	TArray<FString> diagnostics;
	TestTrue(
		TEXT("append trace"),
		FUserProjectRunOutputJson::AppendEpisodeTraceRecord(
			paths,
			TEXT("000001"),
			MakeTraceTestTickRecord(),
			0,
			diagnostics));
	TestEqual(TEXT("trace diagnostics"), diagnostics.Num(), 0);

	const FString tracePath = FPaths::Combine(
		FUserProjectRunOutputJson::BuildEpisodeDirectory(paths, TEXT("000001")),
		TEXT("trace.jsonl"));
	TestTrue(TEXT("trace exists"), FPaths::FileExists(tracePath));
	TestEqual(TEXT("trace line count"), CountJsonlLines(tracePath), 1);

	FString traceJsonl;
	TestTrue(TEXT("load trace jsonl"), FFileHelper::LoadFileToString(traceJsonl, *tracePath));
	TestTrue(TEXT("trace schema"), traceJsonl.Contains(TEXT("\"schema\":\"episode_trace\"")));
	TestTrue(TEXT("trace sample index"), traceJsonl.Contains(TEXT("\"sample_index\":0")));
	TestTrue(TEXT("trace robot id"), traceJsonl.Contains(TEXT("\"id\":\"robot_01\"")));
	TestTrue(TEXT("trace actors"), traceJsonl.Contains(TEXT("\"actors\"")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

#endif
