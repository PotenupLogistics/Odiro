// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Actor/DeliveryBot.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "DeliveryBot/Component/DeliveryBot_DriveComponent.h"
#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"
#include "DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBot, Log, All);

namespace
{
	TSharedRef<FJsonObject> MakeVectorJson(const FVector& vector)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetNumberField(TEXT("x"), vector.X);
		object->SetNumberField(TEXT("y"), vector.Y);
		object->SetNumberField(TEXT("z"), vector.Z);
		return object;
	}

	FString GetLidarModeString(EDeliveryBotLidarModeType modeType)
	{
		switch (modeType)
		{
		case EDeliveryBotLidarModeType::OneD:
			return TEXT("OneD");
		case EDeliveryBotLidarModeType::TwoD:
			return TEXT("TwoD");
		case EDeliveryBotLidarModeType::ThreeD:
			return TEXT("ThreeD");
		case EDeliveryBotLidarModeType::OneDAndTwoD:
			return TEXT("OneDAndTwoD");
		case EDeliveryBotLidarModeType::TwoDAndThreeD:
			return TEXT("TwoDAndThreeD");
		case EDeliveryBotLidarModeType::All:
			return TEXT("All");
		default:
			return TEXT("Unknown");
		}
	}

	void SetNameArrayField(TSharedRef<FJsonObject> object, const FString& fieldName, const TArray<FName>& names)
	{
		TArray<TSharedPtr<FJsonValue>> values;

		for (const FName& name : names)
		{
			values.Add(MakeShared<FJsonValueString>(name.ToString()));
		}

		object->SetArrayField(fieldName, values);
	}
}

ADeliveryBot::ADeliveryBot()
{
	PrimaryActorTick.bCanEverTick = true;

	DriveComponent = CreateDefaultSubobject<UDeliveryBot_DriveComponent>(TEXT("DriveComponent"));
	LidarSensorComponent = CreateDefaultSubobject<UDeliveryBot_LidarSensorComponent>(TEXT("LidarSensorComponent"));
	HttpPolicyComponent = CreateDefaultSubobject<UDeliveryBot_HttpPolicyComponent>(TEXT("HttpPolicyComponent"));
	PolicyControllerComponent = CreateDefaultSubobject<UDeliveryBot_PolicyControllerComponent>(TEXT("PolicyControllerComponent"));
	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));

	UChaosWheeledVehicleMovementComponent* wheeledMovement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	if (IsValid(DriveComponent))
	{
		DriveComponent->SetupVehicleMovement(wheeledMovement);
	}
}

void ADeliveryBot::BeginPlay()
{
	Super::BeginPlay();
	
	ApplySetupInfo();
	UpdateSensorSnapshot();

	if (UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent())
	{
		vehicleMovement->SetRequiresControllerForInputs(false);
		vehicleMovement->SetUseAutomaticGears(false);
		vehicleMovement->SetTargetGear(1, true);
	}
	
	if (IsValid(PolicyControllerComponent))
	{
		PolicyControllerComponent->InitializePolicyController(this, HttpPolicyComponent);
	}
	
}

void ADeliveryBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateSensorSnapshot();

	if (IsValid(PolicyControllerComponent))
	{
		PolicyControllerComponent->TickPolicy(DeltaTime);
	}
}

void ADeliveryBot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(PolicyControllerComponent))
	{
		PolicyControllerComponent->StopPolicyLoop();
	}

	Super::EndPlay(EndPlayReason);
}

void ADeliveryBot::DebugLogObservation(float deltaTime)
{
	DebugLogElapsedSeconds += deltaTime;
	if (DebugLogElapsedSeconds < 1.f)
		return;
	
	DebugLogElapsedSeconds = 0.f;
	const FDeliveryBotObservationInfo observation = BuildObservation();
	UE_LOG(
	LogDeliveryBot,
	Log,
	TEXT("Observation | PolicySeq: %d, SensorSeq: %d, Time: %.2f, Location: %s, Yaw: %.1f, Speed: %.2fkm/h, Rays: %d, Objects: %d, MaxSpeed: %.1f, ReverseSpeed: %.1f, LidarRange: %.1f"),
	observation.Sequence,
	observation.SensorSequence,
	observation.WorldTimeSeconds,
	*observation.RobotState.LocationCm.ToString(),
	observation.RobotState.YawDegree,
	observation.RobotState.SpeedKmh,
	observation.LidarScanInfo.RayInfos.Num(),
	observation.ObservedObjects.Num(),
	observation.VehicleSpec.MaxSpeedKmh,
	observation.VehicleSpec.MaxReverseSpeedKmh,
	observation.VehicleSpec.LidarScanRangeM);
	
	FString observationJson;
	if (BuildObservationJson(observation, observationJson))
	{
		UE_LOG(LogDeliveryBot, Log, TEXT("Observation JSON Length: %d"), observationJson.Len());
	}
}
void ADeliveryBot::InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo)
{
	SetupInfo = setupInfo;
	ApplySetupInfo();
	UpdateSensorSnapshot();
}

void ADeliveryBot::ApplyMoveCommand(const FDeliveryBotMoveCommandInfo& moveCommandInfo, float deltaTime)
{
	if (!IsValid(DriveComponent))
		return;
	
	UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (!IsValid(vehicleMovement))
		return;

	DriveComponent->ApplyMoveCommand(vehicleMovement, moveCommandInfo, deltaTime);
	LastMoveCommandInfo = moveCommandInfo;
	LastActionReason = TEXT("python_policy");
	bHasLastMoveCommand = true;
}

void ADeliveryBot::ApplyParkingStop()
{
	if (!IsValid(DriveComponent))
		return;

	UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (!IsValid(vehicleMovement))
		return;

	DriveComponent->ApplyParkingStop(vehicleMovement);
	LastMoveCommandInfo = FDeliveryBotMoveCommandInfo{};
	LastMoveCommandInfo.Brake = 1.f;
	LastMoveCommandInfo.bBrake = true;
	LastActionReason = TEXT("parking_stop");
	bHasLastMoveCommand = true;
}

void ADeliveryBot::ApplyRuntimeDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo)
{
	UE_LOG(
		LogDeliveryBot,
		Log,
		TEXT("Runtime drive config apply requested | MaxSpeed: %.2f, MaxReverseSpeed: %.2f"),
		driveConfigInfo.MaxSpeedKmh,
		driveConfigInfo.MaxReverseSpeedKmh
	);

	SetupInfo.ChaosDriveConfigInfo = driveConfigInfo;

	if (!IsValid(DriveComponent))
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Runtime drive config apply skipped. DriveComponent is invalid."));
		return;
	}

	UChaosWheeledVehicleMovementComponent* wheeledMovement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	DriveComponent->InitializeChaosDrive(wheeledMovement, SetupInfo.ChaosDriveConfigInfo);

	// DriveComponent에서 보정된 값을 다시 SetupInfo에 저장한다.
	// 이후 observation 검증과 config update JSON이 같은 기준을 쓰게 하기 위함.
	SetupInfo.ChaosDriveConfigInfo = DriveComponent->GetDriveConfigInfo();

	UE_LOG(
		LogDeliveryBot,
		Log,
		TEXT("Runtime drive config applied | MaxSpeed: %.2f, MaxReverseSpeed: %.2f"),
		SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh,
		SetupInfo.ChaosDriveConfigInfo.MaxReverseSpeedKmh
	);
}

void ADeliveryBot::ApplyCurrentSetupInfoToRuntimeComponents()
{
	ApplyRuntimeDriveConfigInfo(SetupInfo.ChaosDriveConfigInfo);

	if (IsValid(LidarSensorComponent))
	{
		LidarSensorComponent->InitializeLidar(SetupInfo.LidarSensorConfigInfo);
	}

	UpdateSensorSnapshot();

	UE_LOG(
		LogDeliveryBot,
		Log,
		TEXT("Current setup info applied to runtime components | MaxSpeed: %.2f, LidarRange: %.2f"),
		SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh,
		SetupInfo.LidarSensorConfigInfo.ScanRangeM
	);
}

void ADeliveryBot::SendCurrentRuntimeConfigUpdateToPolicyServerOnce()
{
	if (!IsValid(PolicyControllerComponent))
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Current runtime config update skipped. PolicyControllerComponent is invalid."));
		return;
	}

	const bool bRequestStarted = PolicyControllerComponent->SendCurrentRuntimeConfigUpdateToPolicyServerOnce();

	UE_LOG(
		LogDeliveryBot,
		Log,
		TEXT("Current runtime config update button completed | Started: %s"),
		bRequestStarted ? TEXT("true") : TEXT("false")
	);
}

void ADeliveryBot::UpdateSensorSnapshot()
{
	LastSensorSnapshot = FDeliveryBotSensorSnapshot{};

	if (!IsValid(LidarSensorComponent))
		return;

	LastSensorSnapshot.LidarScanInfo = LidarSensorComponent->ScanLidar();
	LastSensorSnapshot.DetectedObjects =
		LidarSensorComponent->BuildDetectedObjects(LastSensorSnapshot.LidarScanInfo);

	FDeliveryBotLidarDetectedObjectInfo frontObjectInfo;
	if (LidarSensorComponent->FindNearestFrontObject(LastSensorSnapshot.LidarScanInfo, frontObjectInfo))
	{
		LastSensorSnapshot.bHasFrontObject = true;
		LastSensorSnapshot.FrontObjectInfo = frontObjectInfo;
	}

	++SensorSnapshotSequence;
}

FDeliveryBotObservationInfo ADeliveryBot::BuildPolicyObservation()
{
	FDeliveryBotObservationInfo observation;
	FillObservation(observation);

	observation.Sequence = ++PolicyObservationSequence;

	return observation;
}

bool ADeliveryBot::GetSensorSnapshot(FDeliveryBotSensorSnapshot& outSnapshot) const
{
	outSnapshot = LastSensorSnapshot;
	return IsValid(LidarSensorComponent);
}

bool ADeliveryBot::GetLastMoveCommandInfo(FDeliveryBotMoveCommandInfo& outMoveCommandInfo, FString& outActionReason) const
{
	outMoveCommandInfo = LastMoveCommandInfo;
	outActionReason = LastActionReason;
	return bHasLastMoveCommand;
}

FDeliveryBotObservationInfo ADeliveryBot::BuildObservation() const
{
	FDeliveryBotObservationInfo observation;
	FillObservation(observation);

	// Debug/read-only 용도라 policy sequence를 증가시키지 않는다.
	observation.Sequence = PolicyObservationSequence;

	return observation;
}
TArray<FDeliveryBotLidarObservedObjectInfo> ADeliveryBot::BuildObservedObjectsForPolicy() const
{
	TArray<FDeliveryBotLidarObservedObjectInfo> result;

	for (const FDeliveryBotLidarDetectedObjectInfo& source : LastSensorSnapshot.DetectedObjects)
	{
		FDeliveryBotLidarObservedObjectInfo target;
		target.ActorName = source.ActorName;
		target.ActorTags = source.ActorTags;
		target.ClosestHitLocationCm = source.ClosestHitLocationCm;
		target.ClosestDistanceM = source.ClosestDistanceM;
		target.ClosestRayYawDegree = source.ClosestRayYawDegree;
		target.TotalHitRayCount = source.TotalHitRayCount;
		target.FrontHitRayCount = source.FrontHitRayCount;
		target.bInFront = source.bInFront;
		result.Add(target);
	}

	return result;
}


void ADeliveryBot::ApplySetupInfo()
{
	UChaosWheeledVehicleMovementComponent* wheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	if (IsValid(DriveComponent))
		DriveComponent->InitializeChaosDrive(wheeledMovement, SetupInfo.ChaosDriveConfigInfo);

	if (IsValid(LidarSensorComponent))
		LidarSensorComponent->InitializeLidar(SetupInfo.LidarSensorConfigInfo);
}



void ADeliveryBot::FillObservation(FDeliveryBotObservationInfo& observation) const
{
	observation.SensorSequence = SensorSnapshotSequence;

	if (const UWorld* world = GetWorld())
	{
		observation.WorldTimeSeconds = world->GetTimeSeconds();
	}

	observation.RobotState.LocationCm = GetActorLocation();
	observation.RobotState.YawDegree = GetActorRotation().Yaw;
	observation.RobotState.VelocityCmPerSecond = GetVelocity();
	observation.RobotState.SpeedKmh = GetVelocity().Size() * 0.036f;
	
	observation.VehicleSpec.MaxSpeedKmh = SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh;
	observation.VehicleSpec.MaxReverseSpeedKmh = SetupInfo.ChaosDriveConfigInfo.MaxReverseSpeedKmh;
	observation.VehicleSpec.RobotBoxExtentCm = RobotBoxExtentCm;
	observation.VehicleSpec.MinTurningRadiusCm = MinTurningRadiusCm;
	observation.VehicleSpec.LidarModeType = SetupInfo.LidarSensorConfigInfo.LidarModeType;
	observation.VehicleSpec.LidarScanRangeM = SetupInfo.LidarSensorConfigInfo.ScanRangeM;
	
	observation.LidarScanInfo = LastSensorSnapshot.LidarScanInfo;
	observation.ObservedObjects = BuildObservedObjectsForPolicy();
}

bool ADeliveryBot::BuildObservationJson(const FDeliveryBotObservationInfo& observation, FString& outJson) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();

	rootObject->SetNumberField(TEXT("sequence"), observation.Sequence);
	rootObject->SetNumberField(TEXT("sensorSequence"), observation.SensorSequence);
	rootObject->SetNumberField(TEXT("worldTimeSeconds"), observation.WorldTimeSeconds);

	TSharedRef<FJsonObject> robotObject = MakeShared<FJsonObject>();
	robotObject->SetNumberField(TEXT("x"), observation.RobotState.LocationCm.X);
	robotObject->SetNumberField(TEXT("y"), observation.RobotState.LocationCm.Y);
	robotObject->SetNumberField(TEXT("z"), observation.RobotState.LocationCm.Z);
	robotObject->SetNumberField(TEXT("yawDegree"), observation.RobotState.YawDegree);
	robotObject->SetNumberField(TEXT("speedKmh"), observation.RobotState.SpeedKmh);
	rootObject->SetObjectField(TEXT("robotState"), robotObject);

	TArray<TSharedPtr<FJsonValue>> objectValues;
	for (const FDeliveryBotLidarObservedObjectInfo& observedObject : observation.ObservedObjects)
	{
		TSharedRef<FJsonObject> objectJson = MakeShared<FJsonObject>();
		objectJson->SetStringField(TEXT("actorName"), observedObject.ActorName);
		SetNameArrayField(objectJson, TEXT("actorTags"), observedObject.ActorTags);
		objectJson->SetNumberField(TEXT("closestDistanceM"), observedObject.ClosestDistanceM);
		objectJson->SetNumberField(TEXT("closestRayYawDegree"), observedObject.ClosestRayYawDegree);
		objectJson->SetNumberField(TEXT("totalHitRayCount"), observedObject.TotalHitRayCount);
		objectJson->SetNumberField(TEXT("frontHitRayCount"), observedObject.FrontHitRayCount);
		objectJson->SetBoolField(TEXT("inFront"), observedObject.bInFront);
		objectValues.Add(MakeShared<FJsonValueObject>(objectJson));
	}
	rootObject->SetArrayField(TEXT("observedObjects"), objectValues);

	TArray<TSharedPtr<FJsonValue>> rayValues;
	for (const FDeliveryBotLidarRayInfo& rayInfo : observation.LidarScanInfo.RayInfos)
	{
		TSharedRef<FJsonObject> rayJson = MakeShared<FJsonObject>();
		rayJson->SetBoolField(TEXT("hit"), rayInfo.bHit);
		rayJson->SetNumberField(TEXT("rayIndex"), rayInfo.RayIndex);
		rayJson->SetNumberField(TEXT("rayYawDegree"), rayInfo.RayYawDegree);
		rayJson->SetNumberField(TEXT("distanceM"), rayInfo.DistanceM);
		rayJson->SetStringField(TEXT("actorName"), rayInfo.ActorName);
		SetNameArrayField(rayJson, TEXT("actorTags"), rayInfo.ActorTags);
		rayValues.Add(MakeShared<FJsonValueObject>(rayJson));
	}
	rootObject->SetArrayField(TEXT("lidarRays"), rayValues);

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&outJson);

	return FJsonSerializer::Serialize(rootObject, writer);
}




bool ADeliveryBot::BuildEpisodeStartJson(FString& outJson) const
{
	outJson.Empty();

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem) || !gridSubsystem->HasBuiltGrid())
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Episode start JSON build skipped. Grid is not built yet."));
		return false;
	}

	FString gridJson;
	if (!gridSubsystem->BuildGridJson(gridJson))
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Episode start JSON build failed. Grid JSON build failed."));
		return false;
	}

	TSharedPtr<FJsonObject> gridObject;
	const TSharedRef<TJsonReader<>> gridReader = TJsonReaderFactory<>::Create(gridJson);

	if (!FJsonSerializer::Deserialize(gridReader, gridObject) || !gridObject.IsValid())
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Episode start JSON build failed. Grid JSON parse failed."));
		return false;
	}

	const FDeliveryBotLocationSetupInfo& locationInfo = SetupInfo.LocationSetupInfo;
	const FDeliveryBotDriveConfigInfo& driveInfo = SetupInfo.ChaosDriveConfigInfo;
	const FDeliveryBotLidarSensorConfigInfo& lidarInfo = SetupInfo.LidarSensorConfigInfo;
	const FDeliveryBotPathFollowConfigInfo& motionInfo = SetupInfo.PathFollowConfigInfo;

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("episodeId"), TEXT("delivery_bot_episode"));
	rootObject->SetStringField(TEXT("robotInstanceId"), GetName());

	TSharedRef<FJsonObject> locationSpecObject = MakeShared<FJsonObject>();
	locationSpecObject->SetObjectField(TEXT("startLocationCm"), MakeVectorJson(locationInfo.StartLocationCm));
	locationSpecObject->SetObjectField(TEXT("goalLocationCm"), MakeVectorJson(locationInfo.GoalLocationCm));
	locationSpecObject->SetBoolField(TEXT("autoStartRoute"), locationInfo.bAutoStartRoute);
	rootObject->SetObjectField(TEXT("locationSpec"), locationSpecObject);

	TSharedRef<FJsonObject> driveSpecObject = MakeShared<FJsonObject>();
	driveSpecObject->SetNumberField(TEXT("maxSpeedKmh"), driveInfo.MaxSpeedKmh);
	driveSpecObject->SetNumberField(TEXT("maxReverseSpeedKmh"), driveInfo.MaxReverseSpeedKmh);
	driveSpecObject->SetNumberField(TEXT("slowdownSpeedRangeKmh"), driveInfo.SlowdownSpeedRangeKmh);
	driveSpecObject->SetNumberField(TEXT("stopBrakeInput"), driveInfo.StopBrakeInput);
	driveSpecObject->SetNumberField(TEXT("throttleInputRatePerSecond"), driveInfo.ThrottleInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("brakeInputRatePerSecond"), driveInfo.BrakeInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("steeringInputRatePerSecond"), driveInfo.SteeringInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("accelerationRateKmhPerSecond"), driveInfo.AccelerationRateKmhPerSecond);
	driveSpecObject->SetNumberField(TEXT("decelerationRateKmhPerSecond"), driveInfo.DecelerationRateKmhPerSecond);
	driveSpecObject->SetNumberField(TEXT("maxTorque"), driveInfo.MaxTorque);
	driveSpecObject->SetNumberField(TEXT("maxRPM"), driveInfo.MaxRPM);
	rootObject->SetObjectField(TEXT("driveSpec"), driveSpecObject);

	TSharedRef<FJsonObject> lidarSpecObject = MakeShared<FJsonObject>();
	lidarSpecObject->SetNumberField(TEXT("scanRangeM"), lidarInfo.ScanRangeM);
	lidarSpecObject->SetNumberField(TEXT("angleStepDegree"), lidarInfo.AngleStepDegree);
	lidarSpecObject->SetNumberField(TEXT("sensorHeightM"), lidarInfo.SensorHeightM);
	lidarSpecObject->SetNumberField(TEXT("frontHalfAngleDegree"), lidarInfo.FrontHalfAngleDegree);
	lidarSpecObject->SetBoolField(TEXT("storeMissedRays"), lidarInfo.bStoreMissedRays);
	lidarSpecObject->SetNumberField(TEXT("stopDistanceM"), lidarInfo.StopDistanceM);
	lidarSpecObject->SetNumberField(TEXT("slowDownDistanceM"), lidarInfo.SlowDownDistanceM);
	lidarSpecObject->SetStringField(TEXT("lidarModeType"), GetLidarModeString(lidarInfo.LidarModeType));
	SetNameArrayField(lidarSpecObject, TEXT("ignoreTags"), lidarInfo.IgnoreTags);
	rootObject->SetObjectField(TEXT("lidarSpec"), lidarSpecObject);

	TSharedRef<FJsonObject> motionControlSpecObject = MakeShared<FJsonObject>();
	motionControlSpecObject->SetBoolField(TEXT("drawDebug"), motionInfo.bDrawDebug);
	motionControlSpecObject->SetNumberField(TEXT("lookAheadDistanceM"), motionInfo.LookAheadDistanceM);
	motionControlSpecObject->SetNumberField(TEXT("pathPointAcceptanceDistanceM"), motionInfo.PathPointAcceptanceDistanceM);
	motionControlSpecObject->SetNumberField(TEXT("goalAcceptanceDistanceM"), motionInfo.GoalAcceptanceDistanceM);
	motionControlSpecObject->SetNumberField(TEXT("steeringSensitivity"), motionInfo.SteeringSensitivity);
	motionControlSpecObject->SetNumberField(TEXT("minTurnSpeedKmh"), motionInfo.MinTurnSpeedKmh);
	motionControlSpecObject->SetNumberField(TEXT("obstacleSlowSpeedKmh"), motionInfo.ObstacleSlowSpeedKmh);
	rootObject->SetObjectField(TEXT("motionControlSpec"), motionControlSpecObject);

	// 현재 Python 서버 호환용 필드. 서버 스키마 정리 후 제거해도 된다.
	TSharedRef<FJsonObject> startObject = MakeVectorJson(locationInfo.StartLocationCm);
	startObject->SetNumberField(TEXT("yawDegree"), GetActorRotation().Yaw);
	rootObject->SetObjectField(TEXT("start"), startObject);

	TSharedRef<FJsonObject> goalObject = MakeVectorJson(locationInfo.GoalLocationCm);
	goalObject->SetBoolField(TEXT("hasGoal"), locationInfo.bAutoStartRoute);
	rootObject->SetObjectField(TEXT("goal"), goalObject);

	TSharedRef<FJsonObject> controlSpecObject = MakeShared<FJsonObject>();
	controlSpecObject->SetStringField(TEXT("mode"), TEXT("TargetSpeed"));
	rootObject->SetObjectField(TEXT("controlSpec"), controlSpecObject);

	rootObject->SetObjectField(TEXT("grid"), gridObject);

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&outJson);

	return FJsonSerializer::Serialize(rootObject, writer);
}

bool ADeliveryBot::BuildEpisodeConfigUpdateJson(FString& outJson) const
{
	outJson.Empty();

	const FDeliveryBotDriveConfigInfo& driveInfo = SetupInfo.ChaosDriveConfigInfo;
	const FDeliveryBotLidarSensorConfigInfo& lidarInfo = SetupInfo.LidarSensorConfigInfo;
	const FDeliveryBotPathFollowConfigInfo& motionInfo = SetupInfo.PathFollowConfigInfo;

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();

	TSharedRef<FJsonObject> driveSpecObject = MakeShared<FJsonObject>();
	driveSpecObject->SetNumberField(TEXT("maxSpeedKmh"), driveInfo.MaxSpeedKmh);
	driveSpecObject->SetNumberField(TEXT("maxReverseSpeedKmh"), driveInfo.MaxReverseSpeedKmh);
	driveSpecObject->SetNumberField(TEXT("slowdownSpeedRangeKmh"), driveInfo.SlowdownSpeedRangeKmh);
	driveSpecObject->SetNumberField(TEXT("stopBrakeInput"), driveInfo.StopBrakeInput);
	driveSpecObject->SetNumberField(TEXT("throttleInputRatePerSecond"), driveInfo.ThrottleInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("brakeInputRatePerSecond"), driveInfo.BrakeInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("steeringInputRatePerSecond"), driveInfo.SteeringInputRatePerSecond);
	driveSpecObject->SetNumberField(TEXT("accelerationRateKmhPerSecond"), driveInfo.AccelerationRateKmhPerSecond);
	driveSpecObject->SetNumberField(TEXT("decelerationRateKmhPerSecond"), driveInfo.DecelerationRateKmhPerSecond);
	driveSpecObject->SetNumberField(TEXT("maxTorque"), driveInfo.MaxTorque);
	driveSpecObject->SetNumberField(TEXT("maxRPM"), driveInfo.MaxRPM);
	rootObject->SetObjectField(TEXT("driveSpec"), driveSpecObject);

	TSharedRef<FJsonObject> lidarSpecObject = MakeShared<FJsonObject>();
	lidarSpecObject->SetNumberField(TEXT("scanRangeM"), lidarInfo.ScanRangeM);
	lidarSpecObject->SetNumberField(TEXT("angleStepDegree"), lidarInfo.AngleStepDegree);
	lidarSpecObject->SetNumberField(TEXT("sensorHeightM"), lidarInfo.SensorHeightM);
	lidarSpecObject->SetNumberField(TEXT("frontHalfAngleDegree"), lidarInfo.FrontHalfAngleDegree);
	lidarSpecObject->SetBoolField(TEXT("storeMissedRays"), lidarInfo.bStoreMissedRays);
	lidarSpecObject->SetNumberField(TEXT("stopDistanceM"), lidarInfo.StopDistanceM);
	lidarSpecObject->SetNumberField(TEXT("slowDownDistanceM"), lidarInfo.SlowDownDistanceM);
	lidarSpecObject->SetStringField(TEXT("lidarModeType"), GetLidarModeString(lidarInfo.LidarModeType));
	SetNameArrayField(lidarSpecObject, TEXT("ignoreTags"), lidarInfo.IgnoreTags);
	rootObject->SetObjectField(TEXT("lidarSpec"), lidarSpecObject);

	TSharedRef<FJsonObject> motionControlSpecObject = MakeShared<FJsonObject>();
	motionControlSpecObject->SetBoolField(TEXT("drawDebug"), motionInfo.bDrawDebug);
	motionControlSpecObject->SetNumberField(TEXT("lookAheadDistanceM"), motionInfo.LookAheadDistanceM);
	motionControlSpecObject->SetNumberField(TEXT("pathPointAcceptanceDistanceM"), motionInfo.PathPointAcceptanceDistanceM);
	motionControlSpecObject->SetNumberField(TEXT("goalAcceptanceDistanceM"), motionInfo.GoalAcceptanceDistanceM);
	motionControlSpecObject->SetNumberField(TEXT("steeringSensitivity"), motionInfo.SteeringSensitivity);
	motionControlSpecObject->SetNumberField(TEXT("minTurnSpeedKmh"), motionInfo.MinTurnSpeedKmh);
	motionControlSpecObject->SetNumberField(TEXT("obstacleSlowSpeedKmh"), motionInfo.ObstacleSlowSpeedKmh);
	rootObject->SetObjectField(TEXT("motionControlSpec"), motionControlSpecObject);

	TSharedRef<FJsonObject> controlSpecObject = MakeShared<FJsonObject>();
	controlSpecObject->SetStringField(TEXT("mode"), TEXT("TargetSpeed"));
	rootObject->SetObjectField(TEXT("controlSpec"), controlSpecObject);

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&outJson);

	return FJsonSerializer::Serialize(rootObject, writer);
}

bool ADeliveryBot::SendPolicyObservationOnce()
{
	if (!IsValid(HttpPolicyComponent))
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Policy observation send skipped. HttpPolicyComponent is invalid."));
		return false;
	}

	if (HttpPolicyComponent->IsRequestInFlight())
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Policy observation send skipped. Previous policy request is still in flight."));
		return false;
	}
	
	const FDeliveryBotObservationInfo observation = BuildPolicyObservation();

	FString observationJson;
	if (!BuildObservationJson(observation, observationJson))
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Policy observation send skipped. Failed to build observation JSON."));
		return false;
	}

	const bool bRequestStarted = HttpPolicyComponent->SendObservationJson(observationJson);

	if (bRequestStarted)
	{
		UE_LOG(
			LogDeliveryBot,
			Log,
			TEXT("Policy observation request sent | PolicySeq: %d, SensorSeq: %d, JsonLength: %d"),
			observation.Sequence,
			observation.SensorSequence,
			observationJson.Len()
		);
	}
	else
	{
		UE_LOG(LogDeliveryBot, Warning, TEXT("Policy observation send failed. HTTP request did not start."));
	}

	return bRequestStarted;
}

float ADeliveryBot::GetMaxPolicySpeedKmh(EDeliveryBotMoveDirectionType moveDirectionType) const
{
	return moveDirectionType == EDeliveryBotMoveDirectionType::Reverse
		? SetupInfo.ChaosDriveConfigInfo.MaxReverseSpeedKmh
		: SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh;
}
