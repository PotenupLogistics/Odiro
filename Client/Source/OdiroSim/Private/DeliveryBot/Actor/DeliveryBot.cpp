
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DeliveryBot/Component/DeliveryBot_DriveComponent.h"
#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Components/PrimitiveComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBot, Log, All);

ADeliveryBot::ADeliveryBot()
{
	PrimaryActorTick.bCanEverTick = true;

	DriveComponent = CreateDefaultSubobject<UDeliveryBot_DriveComponent>(TEXT("DriveComponent"));
	LidarSensorComponent = CreateDefaultSubobject<UDeliveryBot_LidarSensorComponent>(TEXT("LidarSensorComponent"));
	HttpPolicyComponent = CreateDefaultSubobject<UDeliveryBot_HttpPolicyComponent>(TEXT("HttpPolicyComponent"));
	PlaceableComponent = CreateDefaultSubobject<UScenarioPlaceableComponent>(TEXT("PlaceableComponent"));

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

	if (UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent())
	{
		vehicleMovement->SetRequiresControllerForInputs(false);
		vehicleMovement->SetUseAutomaticGears(false);
		vehicleMovement->SetTargetGear(1, true);
	}

	BindCollisionStopHitDelegates();

	// BeginPlay에서 Python 서버에 scenario start를 요청한다.
	if (SetupInfo.LocationSetupInfo.bAutoStartRoute && IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->RequestStartScenario();
	}
	else if (!SetupInfo.LocationSetupInfo.bAutoStartRoute)
	{
		UE_LOG(LogDeliveryBot, Log, TEXT("DeliveryBot policy auto start skipped. AutoStartRoute is false."));
	}

}

// 매 Tick마다 센서 관측을 갱신하고 Python decide 요청을 갱신한다.
void ADeliveryBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshSensorSnapshot();

	if (bLogPolicyObservationRequests)
	{
		DebugLogObservation(DeltaTime);
	}

	if (IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->UpdatePolicy(DeltaTime);
	}
}

void ADeliveryBot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ADeliveryBot::BindCollisionStopHitDelegates()
{
	TArray<UPrimitiveComponent*> primitiveComponents;
	GetComponents<UPrimitiveComponent>(primitiveComponents);

	for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
	{
		if (!IsValid(primitiveComponent))
		{
			continue;
		}

		primitiveComponent->OnComponentHit.RemoveDynamic(this, &ADeliveryBot::HandleCollisionStopHit);
		primitiveComponent->OnComponentHit.AddDynamic(this, &ADeliveryBot::HandleCollisionStopHit);
		primitiveComponent->SetNotifyRigidBodyCollision(true);
	}
}

bool ADeliveryBot::IsCollisionStopActor(const AActor* otherActor) const
{
	if (!IsValid(otherActor) || otherActor == this)
	{
		return false;
	}

	if (Cast<AScenarioStaticObstacle>(otherActor) || Cast<AScenarioPedestrian>(otherActor))
	{
		return true;
	}

	if (const AScenarioGroundRegion* groundRegion = Cast<AScenarioGroundRegion>(otherActor))
	{
		return groundRegion->RegionSpec.RegionType == EScenarioGroundRegionType::Blocked;
	}

	for (const FName& tag : otherActor->Tags)
	{
		if (tag.ToString().StartsWith(TEXT("ObjectType.")))
		{
			return true;
		}
	}

	return false;
}

void ADeliveryBot::ResetCollisionStopState()
{
	bCollisionStopActive = false;
	CollisionStopActorName.Reset();
	CollisionStopActorTags.Reset();
}

void ADeliveryBot::HandleCollisionStopHit(
	UPrimitiveComponent* hitComponent,
	AActor* otherActor,
	UPrimitiveComponent* otherComp,
	FVector normalImpulse,
	const FHitResult& hit)
{
	(void)hitComponent;
	(void)otherComp;
	(void)normalImpulse;
	(void)hit;

	if (!IsCollisionStopActor(otherActor))
	{
		return;
	}

	if (!bCollisionStopActive)
	{
		CollisionStopActorName = otherActor->GetName();
		CollisionStopActorTags = otherActor->Tags;
		UE_LOG(LogDeliveryBot, Warning, TEXT("Collision stop locked | Actor=%s"), *CollisionStopActorName);
	}

	bCollisionStopActive = true;
	ApplyParkingStop();
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
}

// 현재 LiDAR 센서 관측값을 LastSensorSnapshot에 저장한다.
void ADeliveryBot::RefreshSensorSnapshot()
{
	LastSensorSnapshot = FDeliveryBotSensorSnapshot{};

	if (!IsValid(LidarSensorComponent))
		return;

	LastSensorSnapshot.LidarScanInfo = LidarSensorComponent->ScanLidar();
	LastSensorSnapshot.DetectedObjects = LidarSensorComponent->BuildDetectedObjects(LastSensorSnapshot.LidarScanInfo);
	LastSensorSnapshot.bHasFrontObject = LidarSensorComponent->FindNearestFrontObject(
		LastSensorSnapshot.LidarScanInfo,
		LastSensorSnapshot.FrontObjectInfo);

	++SensorSnapshotSequence;
}
void ADeliveryBot::InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo)
{
	ResetCollisionStopState();
	SetupInfo = setupInfo;
	ApplySetupInfo();
}

void ADeliveryBot::ApplyMoveCommand(const FDeliveryBotMoveCommandInfo& moveCommandInfo, float deltaTime)
{
	if (bCollisionStopActive)
	{
		ApplyParkingStop();
		return;
	}

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

	UChaosWheeledVehicleMovementComponent* wheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

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


	UE_LOG(
		LogDeliveryBot,
		Log,
		TEXT("Current setup info applied to runtime components | MaxSpeed: %.2f, LidarRange: %.2f"),
		SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh,
		SetupInfo.LidarSensorConfigInfo.ScanRangeM
	);
}

// 기존 Runner 경로에서 들어온 policy start 요청을 Python scenario start로 연결한다.
bool ADeliveryBot::StartPolicyRunWithPolicySpecFileName(const FString& policySpecFileName)
{
	(void)policySpecFileName;

	if (!IsValid(HttpPolicyComponent))
		return false;

	ResetCollisionStopState();
	HttpPolicyComponent->RequestStartScenario();
	return true;
}

void ADeliveryBot::ConfigureProjectActionLogging(const FString& projectOutputEpisodeId)
{
	if (IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->ConfigureProjectActionLogging(projectOutputEpisodeId);
	}
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
		target.bHasBounds = source.bHasBounds;
		target.BoundsOriginCm = source.BoundsOriginCm;
		target.BoundsExtentCm = source.BoundsExtentCm;
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
	observation.RobotState.bColliding = bCollisionStopActive;
	observation.RobotState.CollisionActorName = CollisionStopActorName;
	observation.RobotState.CollisionActorTags = CollisionStopActorTags;

	observation.VehicleSpec.MaxSpeedKmh = SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh;
	observation.VehicleSpec.MaxReverseSpeedKmh = SetupInfo.ChaosDriveConfigInfo.MaxReverseSpeedKmh;
	observation.VehicleSpec.RobotBoxExtentCm = RobotBoxExtentCm;
	observation.VehicleSpec.WheelBaseCm = WheelBaseCm;
	observation.VehicleSpec.MinTurningRadiusCm = MinTurningRadiusCm;
	observation.VehicleSpec.LidarModeType = SetupInfo.LidarSensorConfigInfo.LidarModeType;
	observation.VehicleSpec.LidarScanRangeM = SetupInfo.LidarSensorConfigInfo.ScanRangeM;

	observation.LidarScanInfo = LastSensorSnapshot.LidarScanInfo;
	observation.ObservedObjects = BuildObservedObjectsForPolicy();
}

// 평가 시스템이 목표 도착을 알려주면 Python 서버에 scenario end를 요청한다.
void ADeliveryBot::NotifyGoalReachedByEvaluation()
{
	ApplyParkingStop();

	if (!IsValid(HttpPolicyComponent))
		return;

	HttpPolicyComponent->EndScenario(TEXT("goal_reached"));
}

// Python 서버에서 받은 마지막 scenario result JSON을 반환한다.
FString ADeliveryBot::GetLastPythonScenarioResultJson() const
{
	if (!IsValid(HttpPolicyComponent))
		return FString();

	return HttpPolicyComponent->GetLastScenarioResultJson();
}
