// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Actor/DeliveryBot.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "DeliveryBot/Component/DeliveryBot_DriveComponent.h"
#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"
#include "DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBot, Log, All);

ADeliveryBot::ADeliveryBot()
{
	PrimaryActorTick.bCanEverTick = true;

	DriveComponent = CreateDefaultSubobject<UDeliveryBot_DriveComponent>(TEXT("DriveComponent"));
	LidarSensorComponent = CreateDefaultSubobject<UDeliveryBot_LidarSensorComponent>(TEXT("LidarSensorComponent"));
	HttpPolicyComponent = CreateDefaultSubobject<UDeliveryBot_HttpPolicyComponent>(TEXT("HttpPolicyComponent"));
	PolicyControllerComponent = CreateDefaultSubobject<UDeliveryBot_PolicyControllerComponent>(TEXT("PolicyControllerComponent"));
}

void ADeliveryBot::BeginPlay()
{
	Super::BeginPlay();
	
	ApplySetupInfo();
	UpdateSensorSnapshot();
	
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

bool ADeliveryBot::BuildObservationJson(
	const FDeliveryBotObservationInfo& observation,
	FString& outJson) const
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

	TSharedRef<FJsonObject> vehicleObject = MakeShared<FJsonObject>();
	vehicleObject->SetNumberField(TEXT("maxSpeedKmh"), observation.VehicleSpec.MaxSpeedKmh);
	vehicleObject->SetNumberField(TEXT("maxReverseSpeedKmh"), observation.VehicleSpec.MaxReverseSpeedKmh);
	vehicleObject->SetNumberField(TEXT("lidarScanRangeM"), observation.VehicleSpec.LidarScanRangeM);
	rootObject->SetObjectField(TEXT("vehicleSpec"), vehicleObject);

	TArray<TSharedPtr<FJsonValue>> objectValues;
	for (const FDeliveryBotLidarObservedObjectInfo& observedObject : observation.ObservedObjects)
	{
		TSharedRef<FJsonObject> objectJson = MakeShared<FJsonObject>();
		objectJson->SetStringField(TEXT("actorName"), observedObject.ActorName);
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

		rayValues.Add(MakeShared<FJsonValueObject>(rayJson));
	}
	rootObject->SetArrayField(TEXT("lidarRays"), rayValues);

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&outJson);

	return FJsonSerializer::Serialize(rootObject, writer);
}

bool ADeliveryBot::SendPolicyObservationOnce()
{
	if (!IsValid(HttpPolicyComponent))
		return false;

	if (HttpPolicyComponent->IsRequestInFlight())
		return false;
	
	const FDeliveryBotObservationInfo observation = BuildPolicyObservation();

	FString observationJson;
	if (!BuildObservationJson(observation, observationJson))
		return false;

	return HttpPolicyComponent->SendObservationJson(observationJson);
}
