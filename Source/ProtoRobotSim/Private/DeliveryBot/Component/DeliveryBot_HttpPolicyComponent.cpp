#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DrawDebugHelpers.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Subsystem/DeliveryBotPythonProcessSubsystem.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotHttpPolicy, Log, All);

namespace
{

	// JSON object field를 안전하게 가져온다.
	bool TryGetJsonObjectField(const FJsonObject& jsonObject, const FString& fieldName, TSharedPtr<FJsonObject>& outObject)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::Object)
			return false;

		outObject = jsonValue->AsObject();
		return outObject.IsValid();
	}

	// JSON array field를 안전하게 가져온다.
	bool TryGetJsonArrayField(const FJsonObject& jsonObject, const FString& fieldName, TArray<TSharedPtr<FJsonValue>>& outArray)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::Array)
			return false;

		outArray = jsonValue->AsArray();
		return true;
	}

	// FName 태그 배열을 JSON string 배열로 변환한다.
	TArray<TSharedPtr<FJsonValue>> MakeJsonStringArrayFromNames(const TArray<FName>& names)
	{
		TArray<TSharedPtr<FJsonValue>> jsonValues;
		jsonValues.Reserve(names.Num());

		for (const FName& name : names)
		{
			jsonValues.Add(MakeShared<FJsonValueString>(name.ToString()));
		}

		return jsonValues;
	}
}

// 컴포넌트 Tick은 끄고 DeliveryBot Tick에서 명시적으로 갱신한다.
UDeliveryBot_HttpPolicyComponent::UDeliveryBot_HttpPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// BeginPlay에서 호출되어 scenario start를 예약한다.
void UDeliveryBot_HttpPolicyComponent::RequestStartScenario()
{
	if (bScenarioStarted || bStartRequestInFlight)
		return;

	ResetScenarioState(false);

	bStartRequested = true;
	TryStartScenario();
}

// start 전에는 재시도하고 start 후에는 decide를 반복 호출한다.
void UDeliveryBot_HttpPolicyComponent::UpdatePolicy(float deltaTime)
{
	if (bEndRequestInFlight)
		return;

	if (bStartRequested && !bScenarioStarted)
	{
		StartRetryElapsedSeconds += deltaTime;
		if (StartRetryElapsedSeconds >= StartRetryIntervalSeconds)
		{
			StartRetryElapsedSeconds = 0.f;
			TryStartScenario();
		}
		return;
	}

	if (!bScenarioStarted)
		return;

	DecideElapsedSeconds += deltaTime;
	if (DecideElapsedSeconds >= DecideIntervalSeconds)
	{
		const float decisionDeltaTime = DecideElapsedSeconds;
		DecideElapsedSeconds = 0.f;
		RequestDecision(decisionDeltaTime);
	}
}

// Python 서버에 /scenario/start 요청을 보낸다.
bool UDeliveryBot_HttpPolicyComponent::TryStartScenario()
{
	if (bScenarioStarted || bStartRequestInFlight)
		return true;

	FString payload;
	if (!BuildStartPayload(payload))
		return false;

	bStartRequestInFlight = true;

	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/start"),
		payload,
		[this](FHttpResponsePtr response, bool bSucceeded)
		{
			bStartRequestInFlight = false;

			// /scenario/start envelope 응답의 response.status를 확인한다.
			// /scenario/start envelope 응답의 response.status를 확인한다.
			if (!bSucceeded || !IsPythonResponseOk(response))
			{
				const int32 responseCode = response.IsValid() ? response->GetResponseCode() : 0;
				const FString responseBody = response.IsValid() ? response->GetContentAsString() : FString();

				UE_LOG(
					LogDeliveryBotHttpPolicy,
					Warning,
					TEXT("Python scenario start failed. Succeeded=%s, Code=%d, Body=%s"),
					bSucceeded ? TEXT("true") : TEXT("false"),
					responseCode,
					*responseBody);

				return;
			}

			bScenarioStarted = true;
			UE_LOG(LogDeliveryBotHttpPolicy, Log, TEXT("Python scenario started."));
		});

	if (!bRequestStarted)
	{
		bStartRequestInFlight = false;
	}

	return bRequestStarted;
}


// Python 서버에 /scenario/decide 요청을 보내고 action을 차량에 적용한다.
bool UDeliveryBot_HttpPolicyComponent::RequestDecision(float deltaTime)
{
	if (!bScenarioStarted || bDecisionRequestInFlight)
		return false;

	FString payload;
	if (!BuildDecidePayload(payload))
		return false;

	bDecisionRequestInFlight = true;

	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/decide"),
		payload,
		[this, deltaTime](FHttpResponsePtr response, bool bSucceeded)
		{
			bDecisionRequestInFlight = false;

			FDeliveryBotMoveCommandInfo moveCommand;
			if (!bSucceeded || !TryParseMoveCommand(response, moveCommand))
			{
				if (ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner()))
				{
					deliveryBot->ApplyParkingStop();
				}
				return;
			}

			if (ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner()))
			{
				deliveryBot->ApplyMoveCommand(moveCommand, deltaTime);
			}
		});

	if (!bRequestStarted)
	{
		bDecisionRequestInFlight = false;
	}

	return bRequestStarted;
}


// 현재 GameInstance에서 Python process subsystem을 가져온다.
UDeliveryBotPythonProcessSubsystem* UDeliveryBot_HttpPolicyComponent::GetPythonProcessSubsystem() const
{
	AActor* owner = GetOwner();
	if (!IsValid(owner))
		return nullptr;

	UGameInstance* gameInstance = owner->GetGameInstance();
	if (!IsValid(gameInstance))
		return nullptr;

	return gameInstance->GetSubsystem<UDeliveryBotPythonProcessSubsystem>();
}

// Python 서버에 POST 요청을 보낸다.
bool UDeliveryBot_HttpPolicyComponent::SendPostRequest(const FString& endpoint, const FString& payload, TFunction<void(FHttpResponsePtr, bool)> onComplete)
{
	const UDeliveryBotPythonProcessSubsystem* pythonProcessSubsystem = GetPythonProcessSubsystem();
	if (!IsValid(pythonProcessSubsystem) || !pythonProcessSubsystem->IsReady())
		return false;

	const FString normalizedEndpoint = endpoint.StartsWith(TEXT("/")) ? endpoint : TEXT("/") + endpoint;
	const FString url = pythonProcessSubsystem->GetBaseUrl() + normalizedEndpoint;

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(payload);

	request->OnProcessRequestComplete().BindWeakLambda(this, [onComplete = MoveTemp(onComplete)](FHttpRequestPtr, FHttpResponsePtr response,
				bool bWasSuccessful) mutable{onComplete(response, bWasSuccessful);});

	return request->ProcessRequest();
}

// request 객체를 Python message envelope으로 감싼다.
bool UDeliveryBot_HttpPolicyComponent::BuildMessagePayload(const FString& messageType, const TSharedRef<FJsonObject>& requestObject, FString& outPayload) const
{
	outPayload.Reset();

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), TEXT("delivery_bot_python_message"));
	rootObject->SetNumberField(TEXT("version"), 1);
	rootObject->SetStringField(TEXT("type"), messageType);
	rootObject->SetObjectField(TEXT("request"), requestObject);

	TSharedRef<FJsonObject> responseObject = MakeShared<FJsonObject>();
	responseObject->SetStringField(TEXT("status"), TEXT("pending"));
	responseObject->SetField(TEXT("action"), MakeShared<FJsonValueNull>());
	responseObject->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
	responseObject->SetObjectField(TEXT("debug"), MakeShared<FJsonObject>());
	rootObject->SetObjectField(TEXT("response"), responseObject);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outPayload);
	return FJsonSerializer::Serialize(rootObject, writer);
}

// FVector를 Python 서버 location JSON 객체로 변환한다.
TSharedRef<FJsonObject> UDeliveryBot_HttpPolicyComponent::BuildLocationObject(const FVector& location, float yawDegree) const
{
	TSharedRef<FJsonObject> locationObject = MakeShared<FJsonObject>();

	locationObject->SetNumberField(TEXT("x"), location.X);
	locationObject->SetNumberField(TEXT("y"), location.Y);
	locationObject->SetNumberField(TEXT("z"), location.Z);
	locationObject->SetNumberField(TEXT("yawDegree"), yawDegree);

	return locationObject;
}

// GridSubsystem JSON에서 Python 서버가 받는 필드만 추려 grid 객체를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildPythonGridObject(TSharedPtr<FJsonObject>& outGridObject) const
{
	outGridObject.Reset();

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem) || !gridSubsystem->HasBuiltGrid())
		return false;

	FString gridJson;
	if (!gridSubsystem->BuildGridJson(gridJson))
		return false;

	TSharedPtr<FJsonObject> sourceGridObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(gridJson);
	if (!FJsonSerializer::Deserialize(reader, sourceGridObject) || !sourceGridObject.IsValid())
		return false;

	outGridObject = MakeShared<FJsonObject>();

	double numberValue = 0.0;

	sourceGridObject->TryGetNumberField(TEXT("gridSizeX"), numberValue);
	outGridObject->SetNumberField(TEXT("gridSizeX"), numberValue);

	sourceGridObject->TryGetNumberField(TEXT("gridSizeY"), numberValue);
	outGridObject->SetNumberField(TEXT("gridSizeY"), numberValue);

	sourceGridObject->TryGetNumberField(TEXT("cellSizeCm"), numberValue);
	outGridObject->SetNumberField(TEXT("cellSizeCm"), numberValue);

	sourceGridObject->TryGetNumberField(TEXT("cellCount"), numberValue);
	outGridObject->SetNumberField(TEXT("cellCount"), numberValue);

	TSharedPtr<FJsonObject> originObject;
	if (!TryGetJsonObjectField(*sourceGridObject, TEXT("originCm"), originObject))
		return false;

	outGridObject->SetObjectField(TEXT("originCm"), originObject);

	TArray<TSharedPtr<FJsonValue>> sourceCellValues;
	if (!TryGetJsonArrayField(*sourceGridObject, TEXT("cells"), sourceCellValues))
		return false;

	TArray<TSharedPtr<FJsonValue>> targetCellValues;
	targetCellValues.Reserve(sourceCellValues.Num());

	for (const TSharedPtr<FJsonValue>& sourceCellValue : sourceCellValues)
	{
		const TSharedPtr<FJsonObject> sourceCellObject = sourceCellValue.IsValid() ? sourceCellValue->AsObject() : nullptr;
		if (!sourceCellObject.IsValid())
			continue;

		TSharedRef<FJsonObject> targetCellObject = MakeShared<FJsonObject>();

		sourceCellObject->TryGetNumberField(TEXT("x"), numberValue);
		targetCellObject->SetNumberField(TEXT("x"), numberValue);

		sourceCellObject->TryGetNumberField(TEXT("y"), numberValue);
		targetCellObject->SetNumberField(TEXT("y"), numberValue);

		FString areaType;
		sourceCellObject->TryGetStringField(TEXT("areaType"), areaType);
		targetCellObject->SetStringField(TEXT("areaType"), areaType);

		sourceCellObject->TryGetNumberField(TEXT("cost"), numberValue);
		targetCellObject->SetNumberField(TEXT("cost"), numberValue);

		bool bBlocked = false;
		sourceCellObject->TryGetBoolField(TEXT("blocked"), bBlocked);
		targetCellObject->SetBoolField(TEXT("blocked"), bBlocked);

		FString sourceCollisionProfile;
		sourceCellObject->TryGetStringField(TEXT("sourceCollisionProfile"), sourceCollisionProfile);
		targetCellObject->SetStringField(TEXT("sourceCollisionProfile"), sourceCollisionProfile);

		targetCellValues.Add(MakeShared<FJsonValueObject>(targetCellObject));
	}

	outGridObject->SetArrayField(TEXT("cells"), targetCellValues);
	return true;
}

// /scenario/start 요청 envelope body를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildStartPayload(FString& outPayload)
{
	outPayload.Reset();

	ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner());
	if (!IsValid(deliveryBot))
		return false;

	TSharedPtr<FJsonObject> gridObject;
	if (!BuildPythonGridObject(gridObject) || !gridObject.IsValid())
		return false;

	if (EpisodeId.IsEmpty())
	{
		EpisodeId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	}

	RobotInstanceId = deliveryBot->GetName();

	const FDeliveryBotSetupInfo& setupInfo = deliveryBot->GetSetupInfo();
	const FDeliveryBotObservationInfo observation = deliveryBot->BuildObservation();

	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();
	requestObject->SetStringField(TEXT("experimentId"), TEXT("unreal_runtime"));
	requestObject->SetStringField(TEXT("episodeId"), EpisodeId);
	requestObject->SetStringField(TEXT("robotInstanceId"), RobotInstanceId);

	requestObject->SetObjectField(
		TEXT("start"),
		BuildLocationObject(deliveryBot->GetActorLocation(), deliveryBot->GetActorRotation().Yaw));

	TSharedRef<FJsonObject> goalObject = MakeShared<FJsonObject>();
	goalObject->SetBoolField(TEXT("hasGoal"), setupInfo.LocationSetupInfo.bHasGoal);
	goalObject->SetNumberField(TEXT("x"), setupInfo.LocationSetupInfo.GoalLocationCm.X);
	goalObject->SetNumberField(TEXT("y"), setupInfo.LocationSetupInfo.GoalLocationCm.Y);
	goalObject->SetNumberField(TEXT("z"), setupInfo.LocationSetupInfo.GoalLocationCm.Z);
	goalObject->SetNumberField(TEXT("acceptanceRadiusCm"), setupInfo.PathFollowConfigInfo.GoalAcceptanceDistanceM * 100.f);
	requestObject->SetObjectField(TEXT("goal"), goalObject);

	requestObject->SetObjectField(TEXT("grid"), gridObject);

	TSharedRef<FJsonObject> vehicleSpecObject = MakeShared<FJsonObject>();
	vehicleSpecObject->SetNumberField(TEXT("maxSpeedKmh"), observation.VehicleSpec.MaxSpeedKmh);
	vehicleSpecObject->SetNumberField(TEXT("maxReverseSpeedKmh"), observation.VehicleSpec.MaxReverseSpeedKmh);
	requestObject->SetObjectField(TEXT("vehicleSpec"), vehicleSpecObject);

	TSharedRef<FJsonObject> lidarSpecObject = MakeShared<FJsonObject>();
	lidarSpecObject->SetNumberField(TEXT("scanRangeM"), observation.VehicleSpec.LidarScanRangeM);
	lidarSpecObject->SetNumberField(TEXT("angleStepDegree"), setupInfo.LidarSensorConfigInfo.AngleStepDegree);
	lidarSpecObject->SetNumberField(TEXT("sensorHeightM"), setupInfo.LidarSensorConfigInfo.SensorHeightM);
	lidarSpecObject->SetNumberField(TEXT("frontHalfAngleDegree"), setupInfo.LidarSensorConfigInfo.FrontHalfAngleDegree);
	lidarSpecObject->SetNumberField(TEXT("stopDistanceM"), setupInfo.LidarSensorConfigInfo.StopDistanceM);
	lidarSpecObject->SetNumberField(TEXT("nearMissDistanceM"), setupInfo.LidarSensorConfigInfo.NearMissDistanceM);
	lidarSpecObject->SetNumberField(TEXT("slowDownDistanceM"), setupInfo.LidarSensorConfigInfo.SlowDownDistanceM);
	lidarSpecObject->SetNumberField(TEXT("collisionStopHalfAngleDegree"), setupInfo.LidarSensorConfigInfo.CollisionStopHalfAngleDegree);
	lidarSpecObject->SetNumberField(TEXT("collisionStopDistanceM"), setupInfo.LidarSensorConfigInfo.CollisionStopDistanceM);
	requestObject->SetObjectField(TEXT("lidarSpec"), lidarSpecObject);

	const float policySoftStopBrakeInput = FMath::Clamp(setupInfo.ChaosDriveConfigInfo.StopBrakeInput, 0.0f, 0.35f);
	const float policyEmergencyBrakeInput = FMath::Clamp(FMath::Max(policySoftStopBrakeInput, 0.45f), 0.0f, 0.6f);

	TSharedRef<FJsonObject> controlSpecObject = MakeShared<FJsonObject>();
	controlSpecObject->SetNumberField(TEXT("targetSpeedKmh"), setupInfo.PathFollowConfigInfo.TargetSpeedKmh);
	controlSpecObject->SetNumberField(TEXT("lookAheadDistanceM"), setupInfo.PathFollowConfigInfo.LookAheadDistanceM);
	controlSpecObject->SetNumberField(TEXT("pathPointAcceptanceDistanceM"), setupInfo.PathFollowConfigInfo.PathPointAcceptanceDistanceM);
	controlSpecObject->SetNumberField(TEXT("steeringSensitivity"), setupInfo.PathFollowConfigInfo.SteeringSensitivity);
	controlSpecObject->SetNumberField(TEXT("steeringFullScaleDegree"), setupInfo.PathFollowConfigInfo.SteeringFullScaleDegree);
	controlSpecObject->SetNumberField(TEXT("maxSteering"), setupInfo.PathFollowConfigInfo.MaxSteering);
	controlSpecObject->SetNumberField(TEXT("maxSteeringDelta"), setupInfo.PathFollowConfigInfo.MaxSteeringDelta);
	controlSpecObject->SetNumberField(TEXT("minTurnSpeedKmh"), setupInfo.PathFollowConfigInfo.MinTurnSpeedKmh);
	controlSpecObject->SetNumberField(TEXT("obstacleSlowSpeedKmh"), setupInfo.PathFollowConfigInfo.ObstacleSlowSpeedKmh);
	controlSpecObject->SetNumberField(TEXT("nearMissDistanceM"), setupInfo.LidarSensorConfigInfo.NearMissDistanceM);
	controlSpecObject->SetNumberField(TEXT("collisionStopHalfAngleDegree"), setupInfo.LidarSensorConfigInfo.CollisionStopHalfAngleDegree);
	controlSpecObject->SetNumberField(TEXT("collisionStopDistanceM"), setupInfo.LidarSensorConfigInfo.CollisionStopDistanceM);
	controlSpecObject->SetNumberField(TEXT("softStopBrakeInput"), policySoftStopBrakeInput);
	controlSpecObject->SetNumberField(TEXT("emergencyBrakeInput"), policyEmergencyBrakeInput);
	controlSpecObject->SetNumberField(TEXT("recoverySpeedKmh"), setupInfo.ChaosDriveConfigInfo.MaxReverseSpeedKmh * 0.4f);
	requestObject->SetObjectField(TEXT("controlSpec"), controlSpecObject);

	return BuildMessagePayload(TEXT("scenario_start"), requestObject, outPayload);
}

// /scenario/decide 요청 envelope body를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildDecidePayload(FString& outPayload)
{
	outPayload.Reset();

	ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner());
	if (!IsValid(deliveryBot))
		return false;

	const FDeliveryBotObservationInfo observation = deliveryBot->BuildPolicyObservation();
	LastDecisionSequence = observation.Sequence;

	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();
	requestObject->SetNumberField(TEXT("sequence"), observation.Sequence);
	requestObject->SetNumberField(TEXT("runTimeSeconds"), observation.WorldTimeSeconds);

	TSharedRef<FJsonObject> robotStateObject = MakeShared<FJsonObject>();
	robotStateObject->SetNumberField(TEXT("x"), observation.RobotState.LocationCm.X);
	robotStateObject->SetNumberField(TEXT("y"), observation.RobotState.LocationCm.Y);
	robotStateObject->SetNumberField(TEXT("z"), observation.RobotState.LocationCm.Z);
	robotStateObject->SetNumberField(TEXT("yawDegree"), observation.RobotState.YawDegree);
	robotStateObject->SetNumberField(TEXT("speedKmh"), observation.RobotState.SpeedKmh);
	requestObject->SetObjectField(TEXT("robotState"), robotStateObject);

	TArray<TSharedPtr<FJsonValue>> lidarRayValues;
	lidarRayValues.Reserve(observation.LidarScanInfo.RayInfos.Num());

	for (const FDeliveryBotLidarRayInfo& rayInfo : observation.LidarScanInfo.RayInfos)
	{
		TSharedRef<FJsonObject> rayObject = MakeShared<FJsonObject>();
		rayObject->SetBoolField(TEXT("hit"), rayInfo.bHit);
		rayObject->SetNumberField(TEXT("distanceM"), rayInfo.DistanceM);
		rayObject->SetNumberField(TEXT("rayIndex"), rayInfo.RayIndex);
		rayObject->SetNumberField(TEXT("rayYawDegree"), rayInfo.RayYawDegree);
		rayObject->SetStringField(TEXT("actorName"), rayInfo.ActorName);
		rayObject->SetArrayField(TEXT("actorTags"), MakeJsonStringArrayFromNames(rayInfo.ActorTags));

		lidarRayValues.Add(MakeShared<FJsonValueObject>(rayObject));
	}

	requestObject->SetArrayField(TEXT("lidarRays"), lidarRayValues);

	TArray<TSharedPtr<FJsonValue>> observedObjectValues;
	observedObjectValues.Reserve(observation.ObservedObjects.Num());

	for (const FDeliveryBotLidarObservedObjectInfo& objectInfo : observation.ObservedObjects)
	{
		TSharedRef<FJsonObject> objectJson = MakeShared<FJsonObject>();
		objectJson->SetStringField(TEXT("actorName"), objectInfo.ActorName);
		objectJson->SetArrayField(TEXT("actorTags"), MakeJsonStringArrayFromNames(objectInfo.ActorTags));
		objectJson->SetNumberField(TEXT("closestDistanceM"), objectInfo.ClosestDistanceM);
		objectJson->SetNumberField(TEXT("closestRayYawDegree"), objectInfo.ClosestRayYawDegree);
		objectJson->SetNumberField(TEXT("totalHitRayCount"), objectInfo.TotalHitRayCount);
		objectJson->SetNumberField(TEXT("frontHitRayCount"), objectInfo.FrontHitRayCount);
		objectJson->SetBoolField(TEXT("inFront"), objectInfo.bInFront);

		observedObjectValues.Add(MakeShared<FJsonValueObject>(objectJson));
	}

	requestObject->SetArrayField(TEXT("observedObjects"), observedObjectValues);

	return BuildMessagePayload(TEXT("scenario_decide"), requestObject, outPayload);
}

// envelope 응답에서 response 객체를 가져온다.
bool UDeliveryBot_HttpPolicyComponent::TryGetPythonResponseObject(const FHttpResponsePtr& response, TSharedPtr<FJsonObject>& outResponseObject) const
{
	outResponseObject.Reset();

	if (!response.IsValid() || response->GetResponseCode() < 200 || response->GetResponseCode() >= 300)
		return false;

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		return false;

	return TryGetJsonObjectField(*rootObject, TEXT("response"), outResponseObject);
}

// envelope 응답의 response.status가 ok인지 확인한다.
bool UDeliveryBot_HttpPolicyComponent::IsPythonResponseOk(const FHttpResponsePtr& response) const
{
	TSharedPtr<FJsonObject> responseObject;
	if (!TryGetPythonResponseObject(response, responseObject))
		return false;

	FString status;
	return responseObject->TryGetStringField(TEXT("status"), status)
		&& status.Equals(TEXT("ok"), ESearchCase::IgnoreCase);
}

// /scenario/decide envelope 응답의 response.action을 이동 명령으로 변환한다.
bool UDeliveryBot_HttpPolicyComponent::TryParseMoveCommand(
	const FHttpResponsePtr& response,
	FDeliveryBotMoveCommandInfo& outMoveCommand) const
{
	outMoveCommand = FDeliveryBotMoveCommandInfo{};

	TSharedPtr<FJsonObject> responseObject;
	if (!TryGetPythonResponseObject(response, responseObject))
		return false;

	FString status;
	if (!responseObject->TryGetStringField(TEXT("status"), status) || !status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
		return false;

	DrawPythonPathDebug(responseObject);

	TSharedPtr<FJsonObject> actionObject;
	if (!TryGetJsonObjectField(*responseObject, TEXT("action"), actionObject))
		return false;

	double steering = 0.0;
	double targetSpeedKmh = 0.0;
	double brake = 0.0;
	FString direction = TEXT("Forward");

	actionObject->TryGetNumberField(TEXT("steering"), steering);
	actionObject->TryGetNumberField(TEXT("targetSpeedKmh"), targetSpeedKmh);
	actionObject->TryGetNumberField(TEXT("brake"), brake);
	actionObject->TryGetStringField(TEXT("direction"), direction);

	outMoveCommand.Steering = FMath::Clamp(static_cast<float>(steering), -1.f, 1.f);
	outMoveCommand.TargetSpeedKmh = FMath::Max(static_cast<float>(targetSpeedKmh), 0.f);
	outMoveCommand.Brake = FMath::Clamp(static_cast<float>(brake), 0.f, 1.f);
	outMoveCommand.bBrake = outMoveCommand.Brake > KINDA_SMALL_NUMBER;

	outMoveCommand.MoveDirectionType = direction.Equals(TEXT("Reverse"), ESearchCase::IgnoreCase)
		? EDeliveryBotMoveDirectionType::Reverse
		: EDeliveryBotMoveDirectionType::Forward;

	return true;
}

// Python path debug 좌표를 경로선, 현재 인덱스, 실제 추종 목표점으로 그린다.
void UDeliveryBot_HttpPolicyComponent::DrawPythonPathDebug(const TSharedPtr<FJsonObject>& responseObject) const
{
	if (!bDrawPythonPathDebug || !responseObject.IsValid())
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	TSharedPtr<FJsonObject> debugObject;
	if (!TryGetJsonObjectField(*responseObject, TEXT("debug"), debugObject))
		return;

	const TArray<TSharedPtr<FJsonValue>>* pathValues = nullptr;
	if (!debugObject->TryGetArrayField(TEXT("pathWorldPoints"), pathValues) || pathValues == nullptr || pathValues->Num() < 2)
		return;

	TArray<FVector> pathPoints;
	pathPoints.Reserve(pathValues->Num());

	for (const TSharedPtr<FJsonValue>& pointValue : *pathValues)
	{
		FVector locationCm;
		if (!TryParsePythonPathDebugPoint(pointValue, locationCm))
			continue;

		locationCm.Z += PythonPathDebugHeightCm;
		pathPoints.Add(locationCm);
	}

	for (int32 index = 1; index < pathPoints.Num(); ++index)
	{
		DrawDebugLine(
			world,
			pathPoints[index - 1],
			pathPoints[index],
			FColor::Cyan,
			false,
			DecideIntervalSeconds * 2.f,
			0,
			PythonPathDebugLineThickness);
	}

	double pathIndex = 0.0;
	if (debugObject->TryGetNumberField(TEXT("pathIndex"), pathIndex))
	{
		const int32 currentIndex = FMath::Clamp(static_cast<int32>(pathIndex), 0, pathPoints.Num() - 1);
		DrawDebugSphere(
			world,
			pathPoints[currentIndex],
			18.f,
			12,
			FColor::Yellow,
			false,
			DecideIntervalSeconds * 2.f,
			0,
			2.f);
	}

	TSharedPtr<FJsonObject> targetPointObject;
	if (TryGetJsonObjectField(*debugObject, TEXT("targetWorldPoint"), targetPointObject))
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;

		if (targetPointObject->TryGetNumberField(TEXT("x"), x) &&
			targetPointObject->TryGetNumberField(TEXT("y"), y))
		{
			targetPointObject->TryGetNumberField(TEXT("z"), z);

			FVector targetLocationCm(
				static_cast<float>(x),
				static_cast<float>(y),
				static_cast<float>(z) + PythonPathDebugHeightCm + 20.f);

			DrawDebugSphere(
				world,
				targetLocationCm,
				24.f,
				12,
				FColor::Green,
				false,
				DecideIntervalSeconds * 2.f,
				0,
				3.f);

			if (const AActor* owner = GetOwner(); IsValid(owner))
			{
				DrawDebugLine(
					world,
					owner->GetActorLocation() + FVector(0.f, 0.f, 45.f),
					targetLocationCm,
					FColor::Green,
					false,
					DecideIntervalSeconds * 2.f,
					0,
					3.f);
			}
		}
	}
}

// path debug point JSON을 FVector로 변환한다.
bool UDeliveryBot_HttpPolicyComponent::TryParsePythonPathDebugPoint(
	const TSharedPtr<FJsonValue>& pointValue,
	FVector& outLocationCm) const
{
	outLocationCm = FVector::ZeroVector;

	if (!pointValue.IsValid() || pointValue->Type != EJson::Object)
		return false;

	const TSharedPtr<FJsonObject> pointObject = pointValue->AsObject();
	if (!pointObject.IsValid())
		return false;

	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	if (!pointObject->TryGetNumberField(TEXT("x"), x) || !pointObject->TryGetNumberField(TEXT("y"), y))
		return false;

	pointObject->TryGetNumberField(TEXT("z"), z);

	outLocationCm = FVector(x, y, z);
	return true;
}

// /scenario/end 요청 envelope body를 만든다.
bool UDeliveryBot_HttpPolicyComponent::BuildEndPayload(const FString& status, FString& outPayload) const
{
	outPayload.Reset();

	if (EpisodeId.IsEmpty() || RobotInstanceId.IsEmpty())
		return false;

	TSharedRef<FJsonObject> requestObject = MakeShared<FJsonObject>();

	requestObject->SetStringField(TEXT("experimentId"), TEXT("unreal_runtime"));
	requestObject->SetStringField(TEXT("episodeId"), EpisodeId);
	requestObject->SetStringField(TEXT("robotInstanceId"), RobotInstanceId);
	requestObject->SetNumberField(TEXT("sequence"), LastDecisionSequence);
	requestObject->SetStringField(TEXT("status"), status);

	TSharedRef<FJsonObject> metricsObject = MakeShared<FJsonObject>();
	requestObject->SetObjectField(TEXT("metrics"), metricsObject);

	TSharedRef<FJsonObject> debugObject = MakeShared<FJsonObject>();
	debugObject->SetStringField(TEXT("endSource"), TEXT("UScenarioEvaluationSubsystem"));
	requestObject->SetObjectField(TEXT("debug"), debugObject);

	return BuildMessagePayload(TEXT("scenario_end"), requestObject, outPayload);
}

// scenario 진행 상태를 초기화한다.
void UDeliveryBot_HttpPolicyComponent::ResetScenarioState(bool bKeepLastResult)
{
	EpisodeId.Reset();
	RobotInstanceId.Reset();

	LastDecisionSequence = 0;
	StartRetryElapsedSeconds = 0.f;
	DecideElapsedSeconds = 0.f;

	bStartRequested = false;
	bScenarioStarted = false;
	bStartRequestInFlight = false;
	bDecisionRequestInFlight = false;
	bEndRequestInFlight = false;

	if (!bKeepLastResult)
	{
		LastScenarioResultJson.Reset();
	}
}

// 목표 도착 시 Python 서버에 /scenario/end 요청을 보내고 결과 JSON을 저장한다.
void UDeliveryBot_HttpPolicyComponent::EndScenario(const FString& status)
{
	if (!bScenarioStarted || bEndRequestInFlight)
		return;

	FString payload;
	if (!BuildEndPayload(status, payload))
		return;

	bEndRequestInFlight = true;

	// /scenario/end 응답을 저장하고 scenario 상태를 종료 상태로 초기화한다.
	const bool bRequestStarted = SendPostRequest(
		TEXT("/scenario/end"),
		payload,
		[this](FHttpResponsePtr response, bool bSucceeded)
		{
			bEndRequestInFlight = false;

			LastScenarioResultJson = response.IsValid() ? response->GetContentAsString() : FString();

			ResetScenarioState(true);

			// /scenario/end envelope 응답의 response.status를 확인한다.
			if (!bSucceeded || !IsPythonResponseOk(response))
			{
				UE_LOG(LogDeliveryBotHttpPolicy, Warning, TEXT("Python scenario end failed."));
				return;
			}

			UE_LOG(LogDeliveryBotHttpPolicy, Log, TEXT("Python scenario result saved. Length=%d"), LastScenarioResultJson.Len());
		});

	if (!bRequestStarted)
	{
		bEndRequestInFlight = false;
	}
}
