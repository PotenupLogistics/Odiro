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
				TEXT("\"corridor\":{")
				TEXT("\"axis\":{\"type\":\"polyline\",\"points_m\":[[0.0,0.0],[12.0,0.0]]},")
				TEXT("\"walkway_width_m\":{\"min\":2.5,\"max\":4.0},")
				TEXT("\"building_side\":[{\"surface\":\"wall\",\"width_m\":0.5}],")
				TEXT("\"curb_side\":[{\"surface\":\"road\",\"width_m\":4.0}],")
				TEXT("\"segments\":[{\"id\":\"main\",\"type\":\"straight\",\"along_range_m\":[0.0,12.0]}]")
				TEXT("},")
				TEXT("\"obstacles\":{\"min_clear_width_m\":0.9,\"placements\":[{\"kind\":\"fixed\",\"id\":\"obstacle_1\",\"prop\":\"obstacle.bench_01\",\"at\":{\"segment\":\"main\",\"along_m\":{\"min\":4.0,\"max\":6.0},\"offset_m\":0.25,\"lane\":\"walkway\"},\"yaw_deg\":{\"min\":0.0,\"max\":10.0},\"allow_blocking\":false}]},")
				TEXT("\"pedestrians\":{\"background\":{\"count\":0},\"encounters\":[]},")
				TEXT("\"robot\":{\"start\":{\"type\":\"corridor_pose\",\"segment\":\"main\",\"along_m\":1.0,\"offset_m\":0.0,\"heading\":\"forward\"},\"goal\":{\"type\":\"corridor_pose\",\"segment\":\"main\",\"along_m\":11.0,\"offset_m\":0.0,\"heading\":\"forward\"}}")
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

	bool TryGetJsonObjectFieldForTest(
		const TSharedPtr<FJsonObject>& object,
		const TCHAR* fieldName,
		TSharedPtr<FJsonObject>& outObject)
	{
		outObject.Reset();
		if (!object.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonValue> value = object->TryGetField(fieldName);
		if (!value.IsValid() || value->Type != EJson::Object)
		{
			return false;
		}

		outObject = value->AsObject();
		return outObject.IsValid();
	}

	FScenarioParamValue MakeUserProjectFloatParam(double value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Float;
		paramValue.FloatValue = value;
		return paramValue;
	}

	// events.jsonl 계약 매핑 검증에 쓰는 typed event snapshot 문자열을 만든다.
	FScenarioParamValue MakeUserProjectIntegerParam(int32 value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Integer;
		paramValue.IntegerValue = value;
		return paramValue;
	}

	FScenarioParamValue MakeUserProjectStringParam(const FString& value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::String;
		paramValue.StringValue = value;
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

	bool LoadUserProjectJsonlObjects(const FString& filePath, TArray<TSharedPtr<FJsonObject>>& outObjects)
	{
		outObjects.Reset();

		FString contents;
		if (!FFileHelper::LoadFileToString(contents, *filePath))
		{
			return false;
		}

		TArray<FString> lines;
		contents.ParseIntoArrayLines(lines, true);
		for (const FString& line : lines)
		{
			TSharedPtr<FJsonObject> object;
			const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(line);
			if (!FJsonSerializer::Deserialize(reader, object) || !object.IsValid())
			{
				return false;
			}
			outObjects.Add(object);
		}

		return true;
	}

	int32 CountEventTypeForTest(
		const TArray<TSharedPtr<FJsonObject>>& eventLines,
		const FString& eventType)
	{
		int32 count = 0;
		for (const TSharedPtr<FJsonObject>& eventLine : eventLines)
		{
			FString actualEventType;
			if (eventLine.IsValid()
				&& eventLine->TryGetStringField(TEXT("event_type"), actualEventType)
				&& actualEventType == eventType)
			{
				++count;
			}
		}
		return count;
	}

	TSharedRef<FJsonObject> MakeActionTestRequestObject()
	{
		TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();
		requestObject->SetNumberField(TEXT("sequence"), 7);
		requestObject->SetNumberField(TEXT("runTimeSeconds"), 3.5);
		requestObject->SetNumberField(TEXT("sensorSequence"), 11);
		requestObject->SetNumberField(TEXT("sensorTimeSeconds"), 3.45);

		TSharedRef<FJsonObject> robotStateObject = MakeShared<FJsonObject>();
		robotStateObject->SetNumberField(TEXT("x"), 1000.0);
		robotStateObject->SetNumberField(TEXT("y"), 2000.0);
		robotStateObject->SetNumberField(TEXT("z"), 50.0);
		robotStateObject->SetNumberField(TEXT("yawDegree"), 90.0);
		robotStateObject->SetNumberField(TEXT("speedKmh"), 4.0);
		robotStateObject->SetBoolField(TEXT("bColliding"), true);
		robotStateObject->SetStringField(TEXT("collisionActorName"), TEXT("runtime_collision_actor"));
		robotStateObject->SetArrayField(
			TEXT("collisionActorTags"),
			{ MakeShared<FJsonValueString>(TEXT("RuntimeCollisionTag")) });
		robotStateObject->SetStringField(TEXT("collisionTargetId"), TEXT("obstacle_01"));
		robotStateObject->SetArrayField(
			TEXT("collisionTargetTags"),
			{ MakeShared<FJsonValueString>(TEXT("ScenarioObstacleTag")) });
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

		TSharedRef<FJsonObject> lidarObject = MakeShared<FJsonObject>();
		lidarObject->SetStringField(TEXT("mode"), TEXT("TwoDAndThreeD"));
		TArray<TSharedPtr<FJsonValue>> lidarRay1DValues;
		TSharedRef<FJsonObject> ray1DObject = MakeShared<FJsonObject>();
		ray1DObject->SetBoolField(TEXT("hit"), false);
		ray1DObject->SetNumberField(TEXT("distanceM"), 10.0);
		ray1DObject->SetNumberField(TEXT("rayIndex"), 0);
		lidarRay1DValues.Add(MakeShared<FJsonValueObject>(ray1DObject));
		lidarObject->SetArrayField(TEXT("rays1d"), lidarRay1DValues);

		TArray<TSharedPtr<FJsonValue>> lidarRay2DValues;
		TSharedRef<FJsonObject> ray2DObject = MakeShared<FJsonObject>();
		ray2DObject->SetBoolField(TEXT("hit"), true);
		ray2DObject->SetNumberField(TEXT("distanceM"), 4.0);
		ray2DObject->SetNumberField(TEXT("rayIndex"), 1);
		ray2DObject->SetNumberField(TEXT("yawDegree"), 45.0);
		ray2DObject->SetStringField(TEXT("actorName"), TEXT("RuntimeObstacleActor"));
		ray2DObject->SetStringField(TEXT("targetId"), TEXT("obstacle_01"));
		ray2DObject->SetArrayField(
			TEXT("actorTags"),
			{ MakeShared<FJsonValueString>(TEXT("RuntimeObstacleTag")) });
		ray2DObject->SetArrayField(
			TEXT("targetTags"),
			{ MakeShared<FJsonValueString>(TEXT("ScenarioObstacleTag")) });
		lidarRay2DValues.Add(MakeShared<FJsonValueObject>(ray2DObject));
		lidarObject->SetArrayField(TEXT("rays2d"), lidarRay2DValues);

		TArray<TSharedPtr<FJsonValue>> lidarRay3DValues;
		TSharedRef<FJsonObject> ray3DObject = MakeShared<FJsonObject>();
		ray3DObject->SetBoolField(TEXT("hit"), true);
		ray3DObject->SetNumberField(TEXT("distanceM"), 4.2);
		ray3DObject->SetNumberField(TEXT("rayIndex"), 2);
		ray3DObject->SetNumberField(TEXT("yawDegree"), 45.0);
		ray3DObject->SetNumberField(TEXT("pitchDegree"), -5.0);
		TSharedRef<FJsonObject> hitLocationObject = MakeShared<FJsonObject>();
		hitLocationObject->SetNumberField(TEXT("x"), 400.0);
		hitLocationObject->SetNumberField(TEXT("y"), 0.0);
		hitLocationObject->SetNumberField(TEXT("z"), 50.0);
		ray3DObject->SetObjectField(TEXT("hitLocationCm"), hitLocationObject);
		ray3DObject->SetStringField(TEXT("actorName"), TEXT("RuntimeObstacleActor"));
		ray3DObject->SetStringField(TEXT("targetId"), TEXT("obstacle_01"));
		ray3DObject->SetArrayField(
			TEXT("actorTags"),
			{ MakeShared<FJsonValueString>(TEXT("RuntimeObstacleTag")) });
		ray3DObject->SetArrayField(
			TEXT("targetTags"),
			{ MakeShared<FJsonValueString>(TEXT("ScenarioObstacleTag")) });
		lidarRay3DValues.Add(MakeShared<FJsonValueObject>(ray3DObject));
		lidarObject->SetArrayField(TEXT("rays3d"), lidarRay3DValues);
		requestObject->SetObjectField(TEXT("lidar"), lidarObject);

		TArray<TSharedPtr<FJsonValue>> observedObjectValues;
		TSharedRef<FJsonObject> observedObject = MakeShared<FJsonObject>();
		observedObject->SetStringField(TEXT("actorName"), TEXT("pedestrian_01"));
		observedObject->SetStringField(TEXT("targetId"), TEXT("pedestrian_01"));
		observedObject->SetArrayField(
			TEXT("actorTags"),
			{ MakeShared<FJsonValueString>(TEXT("RuntimePedestrianTag")) });
		observedObject->SetArrayField(
			TEXT("targetTags"),
			{ MakeShared<FJsonValueString>(TEXT("ScenarioPedestrianTag")) });
		observedObject->SetBoolField(TEXT("hasBounds"), true);
		TSharedRef<FJsonObject> boundsOriginObject = MakeShared<FJsonObject>();
		boundsOriginObject->SetNumberField(TEXT("x"), 380.0);
		boundsOriginObject->SetNumberField(TEXT("y"), 0.0);
		boundsOriginObject->SetNumberField(TEXT("z"), 50.0);
		observedObject->SetObjectField(TEXT("boundsOriginCm"), boundsOriginObject);
		TSharedRef<FJsonObject> boundsExtentObject = MakeShared<FJsonObject>();
		boundsExtentObject->SetNumberField(TEXT("x"), 30.0);
		boundsExtentObject->SetNumberField(TEXT("y"), 20.0);
		boundsExtentObject->SetNumberField(TEXT("z"), 40.0);
		observedObject->SetObjectField(TEXT("boundsExtentCm"), boundsExtentObject);
		observedObject->SetObjectField(TEXT("closestHitLocationCm"), hitLocationObject);
		observedObject->SetNumberField(TEXT("closestDistanceM"), 4.0);
		observedObject->SetNumberField(TEXT("closestRayYawDegree"), 45.0);
		observedObject->SetNumberField(TEXT("totalHitRayCount"), 2);
		observedObject->SetNumberField(TEXT("frontHitRayCount"), 1);
		observedObject->SetBoolField(TEXT("inFront"), true);
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
		debugObject->SetStringField(TEXT("selectedPolicy"), TEXT("PathFollower"));
		debugObject->SetStringField(TEXT("reason"), TEXT("follow_path"));
		debugObject->SetStringField(TEXT("pathStatus"), TEXT("valid"));
		debugObject->SetNumberField(TEXT("pathLength"), 2);
		debugObject->SetNumberField(TEXT("targetPathIndex"), 1);
		TSharedRef<FJsonObject> targetWorldPointObject = MakeShared<FJsonObject>();
		targetWorldPointObject->SetNumberField(TEXT("x"), 400.0);
		targetWorldPointObject->SetNumberField(TEXT("y"), 0.0);
		targetWorldPointObject->SetNumberField(TEXT("z"), 50.0);
		debugObject->SetObjectField(TEXT("targetWorldPoint"), targetWorldPointObject);
		TArray<TSharedPtr<FJsonValue>> pathWorldPointValues;
		TSharedRef<FJsonObject> firstPathPointObject = MakeShared<FJsonObject>();
		firstPathPointObject->SetNumberField(TEXT("x"), 300.0);
		firstPathPointObject->SetNumberField(TEXT("y"), 0.0);
		firstPathPointObject->SetNumberField(TEXT("z"), 50.0);
		pathWorldPointValues.Add(MakeShared<FJsonValueObject>(firstPathPointObject));
		pathWorldPointValues.Add(MakeShared<FJsonValueObject>(targetWorldPointObject));
		debugObject->SetArrayField(TEXT("pathWorldPoints"), pathWorldPointValues);
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
	const bool bEpisodeScenariosWrite = FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
			snapshotResult.Paths,
			snapshotResult.Setting,
			writeResults,
			writeDiagnostics);
	TestTrue(TEXT("scenario samples write"), bEpisodeScenariosWrite);
	TestEqual(TEXT("write diagnostics"), writeDiagnostics.Num(), 0);
	TestEqual(TEXT("episode count"), writeResults.Num(), 2);
	if (!bEpisodeScenariosWrite || writeResults.IsEmpty())
	{
		IFileManager::Get().DeleteDirectory(*projectPath, false, true);
		return false;
	}
	TestEqual(TEXT("first episode id"), writeResults[0].EpisodeId, FString(TEXT("000001")));
	TestEqual(TEXT("first seed"), writeResults[0].Seed, static_cast<int64>(1234));
	TestFalse(TEXT("scenario hash populated"), writeResults[0].ScenarioHash.IsEmpty());

	const FUserProjectEpisodeScenarioParseResult parseResult =
		FUserProjectEpisodeScenarioJson::ParseFromFile(writeResults[0].ScenarioPath);
	TestTrue(TEXT("scenario sample parses"), parseResult.bSuccess);
	TestEqual(TEXT("parsed episode id"), parseResult.EpisodeId, FString(TEXT("000001")));
	TestEqual(TEXT("parsed seed"), parseResult.Seed, static_cast<int64>(1234));

	TSharedPtr<FJsonObject> episodeObject;
	TestTrue(TEXT("load episode json"), LoadUserProjectJsonObject(writeResults[0].ScenarioPath, episodeObject));
	if (!episodeObject.IsValid())
	{
		IFileManager::Get().DeleteDirectory(*projectPath, false, true);
		return false;
	}
	TestEqual(TEXT("scenario sample schema"), episodeObject->GetStringField(TEXT("schema")), FString(TEXT("scenario_sample")));
	TestEqual(
		TEXT("sample seed"),
		static_cast<int64>(episodeObject->GetObjectField(TEXT("sample"))->GetObjectField(TEXT("source"))->GetNumberField(TEXT("seed"))),
		static_cast<int64>(1234));

	const TSharedPtr<FJsonObject> sampledScenarioObject = episodeObject->GetObjectField(TEXT("scenario"));
	const TSharedPtr<FJsonObject> paramsObject = sampledScenarioObject->GetObjectField(TEXT("params"));
	TestTrue(TEXT("walkway range param recorded"), paramsObject->HasField(TEXT("corridor.walkway_width_m")));
	TestTrue(TEXT("obstacle along range param recorded"), paramsObject->HasField(TEXT("obstacles.obstacle_1.at.along_m")));
	TestTrue(TEXT("obstacle yaw range param recorded"), paramsObject->HasField(TEXT("obstacles.obstacle_1.yaw_deg")));

	const TSharedPtr<FJsonObject> semanticObject = sampledScenarioObject->GetObjectField(TEXT("semantic"));
	TestTrue(TEXT("route axis generated"), semanticObject->HasField(TEXT("route_axis")));
	TestFalse(TEXT("layout generated"), semanticObject->GetArrayField(TEXT("layout")).IsEmpty());
	TestFalse(TEXT("static obstacle generated"), semanticObject->GetArrayField(TEXT("static_obstacles")).IsEmpty());
	TestTrue(TEXT("robot generated"), semanticObject->HasField(TEXT("robot")));

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
		TEXT("scenario samples write"),
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
	runRecord.EpisodeScenarioJsonPath = writeResults[0].ScenarioPath;
	runRecord.ProfileJsonPath = snapshotResult.Paths.ProfilePath;
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
	runRecord.EvaluationResult.Metrics.Add(TEXT("near_miss_count"), MakeUserProjectFloatParam(0.0));
	runRecord.EvaluationResult.Metrics.Add(TEXT("distance_to_goal_m"), MakeUserProjectFloatParam(0.25));
	runRecord.EvaluationResult.Metrics.Add(TEXT("goal_threshold_m"), MakeUserProjectFloatParam(0.5));

	FEpisodeEvaluationEvent nearMissEvent;
	nearMissEvent.EventIndex = 0;
	nearMissEvent.ElapsedTimeSeconds = 4.25;
	nearMissEvent.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
	nearMissEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
	nearMissEvent.TargetInstanceId = TEXT("pedestrian_001");
	nearMissEvent.Message = TEXT("near miss");
	nearMissEvent.Properties.Add(TEXT("distance_m"), MakeUserProjectFloatParam(0.45));
	runRecord.EvaluationResult.Events.Add(nearMissEvent);

	FEpisodeEvaluationEvent staticCollisionEvent;
	staticCollisionEvent.EventIndex = 1;
	staticCollisionEvent.ElapsedTimeSeconds = 4.75;
	staticCollisionEvent.EventType = EEpisodeEvaluationEventType::StaticObstacleCollision;
	staticCollisionEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
	staticCollisionEvent.TargetInstanceId = TEXT("obstacle_001");
	staticCollisionEvent.Message = TEXT("static collision");
	staticCollisionEvent.Properties.Add(TEXT("target_actor"), MakeUserProjectStringParam(TEXT("BenchActor_001")));
	runRecord.EvaluationResult.Events.Add(staticCollisionEvent);

	FEpisodeEvaluationEvent penaltyRegionEvent;
	penaltyRegionEvent.EventIndex = 2;
	penaltyRegionEvent.ElapsedTimeSeconds = 4.9;
	penaltyRegionEvent.EventType = EEpisodeEvaluationEventType::PenaltyRegionViolation;
	penaltyRegionEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
	penaltyRegionEvent.TargetInstanceId = TEXT("penalty_region_001");
	penaltyRegionEvent.Message = TEXT("penalty region violation");
	penaltyRegionEvent.Properties.Add(TEXT("start_time_s"), MakeUserProjectFloatParam(4.0));
	penaltyRegionEvent.Properties.Add(TEXT("duration_s"), MakeUserProjectFloatParam(0.9));
	runRecord.EvaluationResult.Events.Add(penaltyRegionEvent);

	FEpisodeEvaluationEvent repathEvent;
	repathEvent.EventIndex = 3;
	repathEvent.ElapsedTimeSeconds = 5.0;
	repathEvent.EventType = EEpisodeEvaluationEventType::DeliveryBotRepath;
	repathEvent.Severity = EEpisodeEvaluationEventSeverity::Info;
	repathEvent.Message = TEXT("dynamic_repath_ready");
	repathEvent.Properties.Add(TEXT("policy_sequence"), MakeUserProjectIntegerParam(7));
	repathEvent.Properties.Add(TEXT("policy_event_code"), MakeUserProjectStringParam(TEXT("repath")));
	repathEvent.Properties.Add(TEXT("policy_reason"), MakeUserProjectStringParam(TEXT("dynamic_repath_ready")));
	runRecord.EvaluationResult.Events.Add(repathEvent);

	FEpisodeEvaluationEvent pathfindFailEvent;
	pathfindFailEvent.EventIndex = 4;
	pathfindFailEvent.ElapsedTimeSeconds = 6.0;
	pathfindFailEvent.EventType = EEpisodeEvaluationEventType::DeliveryBotPolicyFailure;
	pathfindFailEvent.Severity = EEpisodeEvaluationEventSeverity::Failure;
	pathfindFailEvent.Message = TEXT("path not found");
	pathfindFailEvent.Properties.Add(TEXT("policy_sequence"), MakeUserProjectIntegerParam(8));
	pathfindFailEvent.Properties.Add(TEXT("error_code"), MakeUserProjectStringParam(TEXT("PATH_NOT_FOUND")));
	pathfindFailEvent.Properties.Add(TEXT("error_message"), MakeUserProjectStringParam(TEXT("no valid path")));
	runRecord.EvaluationResult.Events.Add(pathfindFailEvent);

	FEpisodeEvaluationEvent policyDecisionErrorEvent;
	policyDecisionErrorEvent.EventIndex = 5;
	policyDecisionErrorEvent.ElapsedTimeSeconds = 7.0;
	policyDecisionErrorEvent.EventType = EEpisodeEvaluationEventType::DeliveryBotPolicyServerFailure;
	policyDecisionErrorEvent.Severity = EEpisodeEvaluationEventSeverity::Failure;
	policyDecisionErrorEvent.Message = TEXT("request failed");
	policyDecisionErrorEvent.Properties.Add(TEXT("policy_sequence"), MakeUserProjectIntegerParam(9));
	policyDecisionErrorEvent.Properties.Add(TEXT("error_code"), MakeUserProjectStringParam(TEXT("PYTHON_REQUEST_FAILED")));
	policyDecisionErrorEvent.Properties.Add(TEXT("error_message"), MakeUserProjectStringParam(TEXT("Python decide HTTP request failed.")));
	runRecord.EvaluationResult.Events.Add(policyDecisionErrorEvent);

	FEpisodeEvaluationEvent stuckEvent;
	stuckEvent.EventIndex = 6;
	stuckEvent.ElapsedTimeSeconds = 8.0;
	stuckEvent.EventType = EEpisodeEvaluationEventType::DeliveryBotSimulationFailure;
	stuckEvent.Severity = EEpisodeEvaluationEventSeverity::Failure;
	stuckEvent.Message = TEXT("robot stuck");
	stuckEvent.Properties.Add(TEXT("delivery_bot_failure_type"), MakeUserProjectStringParam(TEXT("Stuck")));
	runRecord.EvaluationResult.Events.Add(stuckEvent);

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
	TestEqual(TEXT("events include evaluation and terminal lines"), CountJsonlLines(eventsPath), 8);

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
		8);
	TestEqual(
		TEXT("event summary total"),
		static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("event_summary"))->GetNumberField(TEXT("total")))),
		8);
	TestEqual(
		TEXT("summary terminal event index"),
		static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("summary"))->GetNumberField(TEXT("terminal_event_index")))),
		7);
	TestEqual(
		TEXT("event summary terminal event index"),
		static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("event_summary"))->GetNumberField(TEXT("terminal_event_index")))),
		7);
	const TSharedPtr<FJsonObject> eventSummaryByType = resultObject->GetObjectField(TEXT("event_summary"))->GetObjectField(TEXT("by_type"));
	TestEqual(TEXT("near miss event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("PedestrianNearMiss")))), 1);
	TestEqual(TEXT("static collision event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("StaticObstacleCollision")))), 1);
	TestEqual(TEXT("penalty region event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("PenaltyRegionViolation")))), 1);
	TestEqual(TEXT("repath event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("Repath")))), 1);
	TestEqual(TEXT("pathfind fail event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("PathfindFail")))), 1);
	TestEqual(TEXT("policy decision error event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("PolicyDecisionError")))), 1);
	TestEqual(TEXT("stuck event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("Stuck")))), 1);
	TestEqual(TEXT("goal reached event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("GoalReached")))), 1);
	const TSharedPtr<FJsonObject> eventSummaryBySource = resultObject->GetObjectField(TEXT("event_summary"))->GetObjectField(TEXT("by_source"));
	TestEqual(TEXT("evaluation source event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryBySource->GetNumberField(TEXT("EvaluationSubsystem")))), 5);
	TestEqual(TEXT("python source event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryBySource->GetNumberField(TEXT("PythonPolicy")))), 2);
	TestEqual(TEXT("policy runtime source event summary"), static_cast<int32>(FMath::RoundToInt(eventSummaryBySource->GetNumberField(TEXT("PolicyRuntime")))), 1);

	FString eventsJson;
	TestTrue(TEXT("load events jsonl"), FFileHelper::LoadFileToString(eventsJson, *eventsPath));
	TestTrue(TEXT("events include goal reached terminal"), eventsJson.Contains(TEXT("\"event_type\":\"GoalReached\"")));
	TestTrue(TEXT("events include terminal reason"), eventsJson.Contains(TEXT("\"reason\":\"GoalReached\"")));
	TestTrue(TEXT("events include terminal duration snapshot"), eventsJson.Contains(TEXT("\"duration_s\":12.5")));
	TestTrue(TEXT("events include goal distance snapshot"), eventsJson.Contains(TEXT("\"distance_to_goal_m\":0.25")));
	TestTrue(TEXT("events include goal threshold snapshot"), eventsJson.Contains(TEXT("\"goal_threshold_m\":0.5")));
	TestTrue(TEXT("events include repath"), eventsJson.Contains(TEXT("\"event_type\":\"Repath\"")));
	TestTrue(TEXT("events include python policy source"), eventsJson.Contains(TEXT("\"source\":\"PythonPolicy\"")));
	TestTrue(TEXT("events include pathfind fail"), eventsJson.Contains(TEXT("\"event_type\":\"PathfindFail\"")));
	TestTrue(TEXT("events include policy decision error"), eventsJson.Contains(TEXT("\"event_type\":\"PolicyDecisionError\"")));
	TestTrue(TEXT("events include policy runtime source"), eventsJson.Contains(TEXT("\"source\":\"PolicyRuntime\"")));
	TestTrue(TEXT("events include stuck"), eventsJson.Contains(TEXT("\"event_type\":\"Stuck\"")));
	TestFalse(TEXT("events omit internal repath type"), eventsJson.Contains(TEXT("\"event_type\":\"DeliveryBotRepath\"")));
	TArray<TSharedPtr<FJsonObject>> eventLines;
	TestTrue(TEXT("parse events jsonl"), LoadUserProjectJsonlObjects(eventsPath, eventLines));
	TestEqual(TEXT("events parsed line count"), eventLines.Num(), 8);
	if (eventLines.Num() == 8 && eventLines[7].IsValid())
	{
		const TSharedPtr<FJsonValue> nearMissActionSequence = eventLines[0]->TryGetField(TEXT("action_sequence"));
		TestTrue(
			TEXT("near miss action sequence is null"),
			nearMissActionSequence.IsValid() && nearMissActionSequence->Type == EJson::Null);
		TSharedPtr<FJsonObject> nearMissProperties;
		TestTrue(TEXT("near miss properties object"), TryGetJsonObjectFieldForTest(eventLines[0], TEXT("properties"), nearMissProperties));
		if (nearMissProperties.IsValid())
		{
			TestEqual(TEXT("near miss target id fallback"), nearMissProperties->GetStringField(TEXT("target_id")), FString(TEXT("pedestrian_001")));
		}
		TSharedPtr<FJsonObject> staticCollisionProperties;
		TestTrue(TEXT("static collision properties object"), TryGetJsonObjectFieldForTest(eventLines[1], TEXT("properties"), staticCollisionProperties));
		if (staticCollisionProperties.IsValid())
		{
			TestEqual(TEXT("static collision target id fallback"), staticCollisionProperties->GetStringField(TEXT("target_id")), FString(TEXT("obstacle_001")));
		}
		TSharedPtr<FJsonObject> penaltyRegionProperties;
		TestTrue(TEXT("penalty region properties object"), TryGetJsonObjectFieldForTest(eventLines[2], TEXT("properties"), penaltyRegionProperties));
		if (penaltyRegionProperties.IsValid())
		{
			TestEqual(TEXT("penalty region id fallback"), penaltyRegionProperties->GetStringField(TEXT("region_id")), FString(TEXT("penalty_region_001")));
		}
		TestEqual(TEXT("repath action sequence"), static_cast<int32>(eventLines[3]->GetNumberField(TEXT("action_sequence"))), 7);
		TestEqual(TEXT("pathfind fail action sequence"), static_cast<int32>(eventLines[4]->GetNumberField(TEXT("action_sequence"))), 8);
		TestEqual(TEXT("policy decision error action sequence"), static_cast<int32>(eventLines[5]->GetNumberField(TEXT("action_sequence"))), 9);
		const TSharedPtr<FJsonValue> stuckActionSequence = eventLines[6]->TryGetField(TEXT("action_sequence"));
		TestTrue(
			TEXT("stuck action sequence is null"),
			stuckActionSequence.IsValid() && stuckActionSequence->Type == EJson::Null);
		const TSharedPtr<FJsonValue> terminalActionSequence = eventLines[7]->TryGetField(TEXT("action_sequence"));
		TestTrue(
			TEXT("terminal action sequence is null"),
			terminalActionSequence.IsValid() && terminalActionSequence->Type == EJson::Null);
		TestEqual(TEXT("terminal line index"), static_cast<int32>(eventLines[7]->GetNumberField(TEXT("event_index"))), 7);
		TestEqual(TEXT("terminal line type"), eventLines[7]->GetStringField(TEXT("event_type")), FString(TEXT("GoalReached")));
	}

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
		FString(TEXT("automation_scenario_000001")));
	TestTrue(
		TEXT("summary scenario params copied"),
		rows[0]->AsObject()->GetObjectField(TEXT("scenario_params"))->HasField(TEXT("corridor.walkway_width_m")));
	TestTrue(
		TEXT("summary scenario semantic copied"),
		rows[0]->AsObject()->GetObjectField(TEXT("scenario_semantic"))->HasField(TEXT("route_axis")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUserProjectRunOutputTerminalEventReuseTest,
	"OdiroSim.UserProjectData.RunOutput.TerminalEventReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUserProjectRunOutputTerminalEventReuseTest::RunTest(const FString& parameters)
{
	(void)parameters;

	const FString projectPath = MakeUserProjectDataTestRoot();
	FUserProjectRunSnapshotPaths paths;
	TestTrue(TEXT("write snapshot"), WriteUserProjectDataSnapshot(projectPath, paths));

	const FUserProjectRunSnapshotParseResult snapshotResult = FUserProjectRunSnapshot::Parse(projectPath, TEXT("000001"));
	TestTrue(TEXT("snapshot parses"), snapshotResult.bSuccess);

	TArray<FUserProjectEpisodeScenarioWriteResult> writeResults;
	TArray<FScenarioCompileDiagnostic> writeDiagnostics;
	TestTrue(
		TEXT("scenario samples write"),
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
	runRecord.EpisodeScenarioJsonPath = writeResults[0].ScenarioPath;
	runRecord.ProfileJsonPath = snapshotResult.Paths.ProfilePath;
	runRecord.PolicySpecJsonPath = snapshotResult.Paths.PolicyEntrypointPath;
	runRecord.EpisodeSetupHash = writeResults[0].ScenarioHash;
	runRecord.DeliveryBotSetupHash = TEXT("crc32:profile");
	runRecord.PairHash = TEXT("crc32:policy");
	runRecord.bCompileSucceeded = true;
	runRecord.bEpisodeSetupCompileSucceeded = true;
	runRecord.bDeliveryBotSetupCompileSucceeded = true;
	runRecord.bSetupSucceeded = true;
	runRecord.bEvaluationCompleted = true;
	runRecord.bSuccess = false;
	runRecord.Outcome = EEpisodeEvaluationOutcome::Failure;
	runRecord.TerminalReason = EEpisodeEvaluationTerminalReason::Timeout;
	runRecord.DurationSeconds = 10.0;
	runRecord.EvaluationResult.EpisodeId = writeResults[0].EpisodeId;
	runRecord.EvaluationResult.bCompleted = true;
	runRecord.EvaluationResult.bSuccess = false;
	runRecord.EvaluationResult.Outcome = EEpisodeEvaluationOutcome::Failure;
	runRecord.EvaluationResult.TerminalReason = EEpisodeEvaluationTerminalReason::Timeout;
	runRecord.EvaluationResult.DurationSeconds = 10.0;
	runRecord.EvaluationResult.Metrics.Add(TEXT("duration_s"), MakeUserProjectFloatParam(10.0));
	runRecord.EvaluationResult.Metrics.Add(TEXT("max_duration_s"), MakeUserProjectFloatParam(10.0));
	runRecord.EvaluationResult.Metrics.Add(TEXT("distance_to_goal_m"), MakeUserProjectFloatParam(2.5));

	FEpisodeEvaluationEvent stuckEvent;
	stuckEvent.EventIndex = 0;
	stuckEvent.ElapsedTimeSeconds = 5.0;
	stuckEvent.EventType = EEpisodeEvaluationEventType::DeliveryBotSimulationFailure;
	stuckEvent.Severity = EEpisodeEvaluationEventSeverity::Warning;
	stuckEvent.Message = TEXT("robot stuck");
	stuckEvent.Properties.Add(TEXT("delivery_bot_failure_type"), MakeUserProjectStringParam(TEXT("Stuck")));
	stuckEvent.Properties.Add(TEXT("duration_s"), MakeUserProjectFloatParam(5.0));
	stuckEvent.Properties.Add(TEXT("distance_to_goal_m"), MakeUserProjectFloatParam(2.5));
	runRecord.EvaluationResult.Events.Add(stuckEvent);

	FEpisodeEvaluationEvent timeoutEvent;
	timeoutEvent.EventIndex = 1;
	timeoutEvent.ElapsedTimeSeconds = 10.0;
	timeoutEvent.EventType = EEpisodeEvaluationEventType::Timeout;
	timeoutEvent.Severity = EEpisodeEvaluationEventSeverity::Failure;
	timeoutEvent.Message = TEXT("time limit exceeded");
	timeoutEvent.Properties.Add(TEXT("duration_s"), MakeUserProjectFloatParam(10.0));
	timeoutEvent.Properties.Add(TEXT("max_duration_s"), MakeUserProjectFloatParam(10.0));
	timeoutEvent.Properties.Add(TEXT("distance_to_goal_m"), MakeUserProjectFloatParam(2.5));
	runRecord.EvaluationResult.Events.Add(timeoutEvent);

	TArray<FString> artifactDiagnostics;
	TestTrue(
		TEXT("episode artifacts write"),
		FUserProjectRunOutputJson::SaveEpisodeArtifacts(snapshotResult.Paths, runRecord, artifactDiagnostics));
	TestEqual(TEXT("artifact diagnostics"), artifactDiagnostics.Num(), 0);

	const FString episodeDirectory = FUserProjectRunOutputJson::BuildEpisodeDirectory(snapshotResult.Paths, TEXT("000001"));
	const FString resultPath = FPaths::Combine(episodeDirectory, TEXT("result.json"));
	const FString eventsPath = FPaths::Combine(episodeDirectory, TEXT("events.jsonl"));

	TestEqual(TEXT("events reuse existing terminal line"), CountJsonlLines(eventsPath), 2);

	TSharedPtr<FJsonObject> resultObject;
	TestTrue(TEXT("load result json"), LoadUserProjectJsonObject(resultPath, resultObject));
	if (resultObject.IsValid())
	{
		TestEqual(
			TEXT("summary terminal event index reuses timeout"),
			static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("summary"))->GetNumberField(TEXT("terminal_event_index")))),
			1);
		TestEqual(
			TEXT("event summary count reuses timeout"),
			static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("event_summary"))->GetNumberField(TEXT("event_count")))),
			2);
		TestEqual(
			TEXT("event summary total reuses timeout"),
			static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("event_summary"))->GetNumberField(TEXT("total")))),
			2);
		TestEqual(
			TEXT("event summary terminal event index reuses timeout"),
			static_cast<int32>(FMath::RoundToInt(resultObject->GetObjectField(TEXT("event_summary"))->GetNumberField(TEXT("terminal_event_index")))),
			1);

		const TSharedPtr<FJsonObject> eventSummaryByType = resultObject->GetObjectField(TEXT("event_summary"))->GetObjectField(TEXT("by_type"));
		TestEqual(TEXT("stuck count"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("Stuck")))), 1);
		TestEqual(TEXT("timeout count"), static_cast<int32>(FMath::RoundToInt(eventSummaryByType->GetNumberField(TEXT("Timeout")))), 1);

		const TSharedPtr<FJsonObject> eventSummaryBySource = resultObject->GetObjectField(TEXT("event_summary"))->GetObjectField(TEXT("by_source"));
		TestEqual(TEXT("evaluation source count"), static_cast<int32>(FMath::RoundToInt(eventSummaryBySource->GetNumberField(TEXT("EvaluationSubsystem")))), 2);
	}

	TArray<TSharedPtr<FJsonObject>> eventLines;
	TestTrue(TEXT("parse events jsonl"), LoadUserProjectJsonlObjects(eventsPath, eventLines));
	TestEqual(TEXT("events parsed line count"), eventLines.Num(), 2);
	TestEqual(TEXT("one stuck event"), CountEventTypeForTest(eventLines, TEXT("Stuck")), 1);
	TestEqual(TEXT("one timeout event"), CountEventTypeForTest(eventLines, TEXT("Timeout")), 1);
	if (eventLines.Num() == 2 && eventLines[1].IsValid())
	{
		TestEqual(TEXT("terminal timeout event index"), static_cast<int32>(eventLines[1]->GetNumberField(TEXT("event_index"))), 1);
		TestEqual(TEXT("terminal timeout event type"), eventLines[1]->GetStringField(TEXT("event_type")), FString(TEXT("Timeout")));
		TestEqual(TEXT("terminal timeout reason"), eventLines[1]->GetStringField(TEXT("reason")), FString(TEXT("Timeout")));
	}

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
	TestFalse(TEXT("no legacy lidar_rays"), actionJsonl.Contains(TEXT("lidar_rays")));
	TestFalse(TEXT("no runtime actorName"), actionJsonl.Contains(TEXT("actorName")));
	TestFalse(TEXT("no runtime actorTags"), actionJsonl.Contains(TEXT("actorTags")));
	TestFalse(TEXT("no camel targetSpeedKmh"), actionJsonl.Contains(TEXT("targetSpeedKmh")));
	TestFalse(TEXT("no camel selectedPolicy"), actionJsonl.Contains(TEXT("selectedPolicy")));

	TSharedPtr<FJsonObject> actionLineObject;
	TestTrue(TEXT("parse action jsonl"), LoadUserProjectJsonObject(actionPath, actionLineObject));
	TestTrue(TEXT("action line object valid"), actionLineObject.IsValid());
	if (actionLineObject.IsValid())
	{
		FString stringValue;
		double numberValue = 0.0;
		bool boolValue = false;

		TestTrue(TEXT("action schema field"), actionLineObject->TryGetStringField(TEXT("schema"), stringValue));
		TestEqual(TEXT("action schema"), stringValue, FString(TEXT("robot_action")));
		TestTrue(TEXT("action status field"), actionLineObject->TryGetStringField(TEXT("status"), stringValue));
		TestEqual(TEXT("action status"), stringValue, FString(TEXT("ok")));
		TestTrue(TEXT("action sequence field"), actionLineObject->TryGetNumberField(TEXT("sequence"), numberValue));
		TestEqual(TEXT("action sequence"), static_cast<int32>(numberValue), 7);
		TestTrue(TEXT("action sensor sequence field"), actionLineObject->TryGetNumberField(TEXT("sensor_sequence"), numberValue));
		TestEqual(TEXT("action sensor sequence"), static_cast<int32>(numberValue), 11);
		TestTrue(TEXT("action angle field"), actionLineObject->TryGetNumberField(TEXT("front_half_angle_degree"), numberValue));
		TestEqual(TEXT("action angle"), static_cast<int32>(numberValue), 45);

		TSharedPtr<FJsonObject> lidarObject;
		TestTrue(TEXT("action lidar object"), TryGetJsonObjectFieldForTest(actionLineObject, TEXT("lidar"), lidarObject));
		if (lidarObject.IsValid())
		{
			TestTrue(TEXT("action lidar mode field"), lidarObject->TryGetStringField(TEXT("mode"), stringValue));
			TestEqual(TEXT("action lidar mode"), stringValue, FString(TEXT("TwoDAndThreeD")));

			const TArray<TSharedPtr<FJsonValue>>* rayValues = nullptr;
			TestTrue(TEXT("action lidar rays_2d field"), lidarObject->TryGetArrayField(TEXT("rays_2d"), rayValues));
			TestTrue(TEXT("action lidar rays_2d nonempty"), rayValues && rayValues->Num() == 1);
			if (rayValues && rayValues->Num() == 1 && (*rayValues)[0].IsValid() && (*rayValues)[0]->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> rayObject = (*rayValues)[0]->AsObject();
			TestTrue(TEXT("ray target id field"), rayObject->TryGetStringField(TEXT("target_id"), stringValue));
			TestEqual(TEXT("ray target id"), stringValue, FString(TEXT("obstacle_01")));
			const TArray<TSharedPtr<FJsonValue>>* targetTags = nullptr;
			TestTrue(TEXT("ray target tags field"), rayObject->TryGetArrayField(TEXT("target_tags"), targetTags));
			TestTrue(
				TEXT("ray target tag value"),
				targetTags && targetTags->Num() == 1 && (*targetTags)[0].IsValid()
					&& (*targetTags)[0]->AsString() == TEXT("ScenarioObstacleTag"));
			TestTrue(TEXT("ray yaw field"), rayObject->TryGetNumberField(TEXT("yaw_degree"), numberValue));
			TestEqual(TEXT("ray yaw"), static_cast<int32>(numberValue), 45);
			}

			TSharedPtr<FJsonObject> selectionObject;
			TestTrue(TEXT("policy ray selection object"), TryGetJsonObjectFieldForTest(lidarObject, TEXT("policy_ray_selection"), selectionObject));
			if (selectionObject.IsValid())
			{
				TestTrue(TEXT("policy ray selection mode field"), selectionObject->TryGetStringField(TEXT("mode"), stringValue));
				TestEqual(TEXT("policy ray selection mode"), stringValue, FString(TEXT("2d")));
				TestTrue(TEXT("policy ray selection source field"), selectionObject->TryGetStringField(TEXT("source"), stringValue));
				TestEqual(TEXT("policy ray selection source"), stringValue, FString(TEXT("lidar.rays_2d")));
				TestTrue(TEXT("policy ray count field"), selectionObject->TryGetNumberField(TEXT("ray_count"), numberValue));
				TestEqual(TEXT("policy ray count"), static_cast<int32>(numberValue), 1);
			}
		}

		TSharedPtr<FJsonObject> robotStateObject;
		TestTrue(TEXT("action robot state object"), TryGetJsonObjectFieldForTest(actionLineObject, TEXT("robot_state"), robotStateObject));
		if (robotStateObject.IsValid())
		{
			TestTrue(TEXT("robot state x field"), robotStateObject->TryGetNumberField(TEXT("x"), numberValue));
			TestEqual(TEXT("robot state x meters"), numberValue, 10.0);
			TestTrue(TEXT("robot state colliding field"), robotStateObject->TryGetBoolField(TEXT("colliding"), boolValue));
			TestTrue(TEXT("robot state colliding"), boolValue);
			TestTrue(TEXT("collision target id field"), robotStateObject->TryGetStringField(TEXT("collision_target_id"), stringValue));
			TestEqual(TEXT("collision target id"), stringValue, FString(TEXT("obstacle_01")));
			const TArray<TSharedPtr<FJsonValue>>* collisionTargetTags = nullptr;
			TestTrue(TEXT("collision target tags field"), robotStateObject->TryGetArrayField(TEXT("collision_target_tags"), collisionTargetTags));
			TestTrue(
				TEXT("collision target tag value"),
				collisionTargetTags && collisionTargetTags->Num() == 1 && (*collisionTargetTags)[0].IsValid()
					&& (*collisionTargetTags)[0]->AsString() == TEXT("ScenarioObstacleTag"));
		}

		TSharedPtr<FJsonObject> actionObject;
		TestTrue(TEXT("action command object"), TryGetJsonObjectFieldForTest(actionLineObject, TEXT("action"), actionObject));
		if (actionObject.IsValid())
		{
			TestTrue(TEXT("target speed field"), actionObject->TryGetNumberField(TEXT("target_speed_kmh"), numberValue));
			TestEqual(TEXT("target speed"), numberValue, 6.0);
		}

		TSharedPtr<FJsonObject> decisionObject;
		TestTrue(TEXT("decision object"), TryGetJsonObjectFieldForTest(actionLineObject, TEXT("decision"), decisionObject));
		if (decisionObject.IsValid())
		{
			TestTrue(TEXT("decision selected policy field"), decisionObject->TryGetStringField(TEXT("selected_policy"), stringValue));
			TestEqual(TEXT("decision selected policy"), stringValue, FString(TEXT("PathFollower")));
			TestTrue(TEXT("decision reason field"), decisionObject->TryGetStringField(TEXT("reason"), stringValue));
			TestEqual(TEXT("decision reason"), stringValue, FString(TEXT("follow_path")));
		}

		TSharedPtr<FJsonObject> pathObject;
		TestTrue(TEXT("path object"), TryGetJsonObjectFieldForTest(actionLineObject, TEXT("path"), pathObject));
		if (pathObject.IsValid())
		{
			TestTrue(TEXT("path status field"), pathObject->TryGetStringField(TEXT("path_status"), stringValue));
			TestEqual(TEXT("path status"), stringValue, FString(TEXT("valid")));
			TSharedPtr<FJsonObject> targetPointObject;
			TestTrue(TEXT("path target point object"), TryGetJsonObjectFieldForTest(pathObject, TEXT("target_world_point"), targetPointObject));
			if (targetPointObject.IsValid())
			{
				TestTrue(TEXT("path target point x field"), targetPointObject->TryGetNumberField(TEXT("x"), numberValue));
				TestEqual(TEXT("path target point x meters"), numberValue, 4.0);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* observedValues = nullptr;
		TestTrue(TEXT("observed objects field"), actionLineObject->TryGetArrayField(TEXT("observed_objects"), observedValues));
		TestTrue(TEXT("observed objects nonempty"), observedValues && observedValues->Num() == 1);
		if (observedValues && observedValues->Num() == 1 && (*observedValues)[0].IsValid() && (*observedValues)[0]->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> observedObject = (*observedValues)[0]->AsObject();
			TestTrue(TEXT("observed target id field"), observedObject->TryGetStringField(TEXT("target_id"), stringValue));
			TestEqual(TEXT("observed target id"), stringValue, FString(TEXT("pedestrian_01")));
			const TArray<TSharedPtr<FJsonValue>>* observedTargetTags = nullptr;
			TestTrue(TEXT("observed target tags field"), observedObject->TryGetArrayField(TEXT("target_tags"), observedTargetTags));
			TestTrue(
				TEXT("observed target tag value"),
				observedTargetTags && observedTargetTags->Num() == 1 && (*observedTargetTags)[0].IsValid()
					&& (*observedTargetTags)[0]->AsString() == TEXT("ScenarioPedestrianTag"));
		}
	}

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
