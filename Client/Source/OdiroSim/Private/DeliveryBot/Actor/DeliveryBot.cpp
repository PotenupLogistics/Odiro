
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

namespace
{
	// Returns the scenario-authored semantic id when the actor participates in scenario logging.
	FString ResolveScenarioTargetId(const AActor* actor)
	{
		if (!IsValid(actor))
		{
			return FString();
		}

		const UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>();
		return placeableComponent ? placeableComponent->InstanceId : FString();
	}
}

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

	UE_LOG(
		LogDeliveryBot,
		Log,
		TEXT("DeliveryBot policy auto start evaluated | Actor=%s AutoStartRoute=%s HasGoal=%s Start=%s Goal=%s"),
		*GetName(),
		SetupInfo.LocationSetupInfo.bAutoStartRoute ? TEXT("true") : TEXT("false"),
		SetupInfo.LocationSetupInfo.bHasGoal ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToString(),
		*SetupInfo.LocationSetupInfo.GoalLocationCm.ToString());

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

// 렌더 Tick에서 고정 시뮬레이션 루프만 진행한다.
void ADeliveryBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFixedSimulation(DeltaTime);
}

// 렌더 delta를 누적해서 고정 간격만큼 시뮬레이션을 진행한다.
void ADeliveryBot::UpdateFixedSimulation(float deltaTime)
{
	const float fixedTickIntervalSeconds = GetFixedTickIntervalSeconds();

	FixedTickElapsedSeconds += FMath::Max(deltaTime, 0.f);

	while (FixedTickElapsedSeconds >= fixedTickIntervalSeconds)
	{
		FixedTickElapsedSeconds -= fixedTickIntervalSeconds;
		StepFixedSimulation(fixedTickIntervalSeconds);
	}
}

// 한 번의 고정 틱에서 순서가 중요한 런타임 갱신을 처리한다.
void ADeliveryBot::StepFixedSimulation(float fixedDeltaSeconds)
{
	FixedSimulationTimeSeconds += fixedDeltaSeconds;
	
	UpdateFixedSensor(fixedDeltaSeconds);
	UpdateFixedPolicy(fixedDeltaSeconds);
	ApplyLatestMoveCommand(fixedDeltaSeconds);
	
	if (bLogPolicyObservationRequests)
	{
		DebugLogObservation(fixedDeltaSeconds);
	}
}

// 고정 틱 간격을 초 단위로 반환한다.
float ADeliveryBot::GetFixedTickIntervalSeconds() const
{
	const float fixedTickRateHz = FMath::Max(FixedTickRateHz, 1.f);
	return 1.f / fixedTickRateHz;
}

// 고정 틱 위에서 LiDAR scan rate에 맞춰 센서 snapshot을 갱신한다.
void ADeliveryBot::UpdateFixedSensor(float fixedDeltaSeconds)
{
	SensorElapsedSeconds += fixedDeltaSeconds;

	const float scanRateHz = FMath::Max(SetupInfo.LidarSensorConfigInfo.ScanRateHz, 0.1f);
	const float scanIntervalSeconds = 1.f / scanRateHz;

	if (SensorSnapshotSequence > 0 && SensorElapsedSeconds < scanIntervalSeconds)
		return;

	SensorElapsedSeconds -= scanIntervalSeconds;
	if (SensorElapsedSeconds < 0.f)
		SensorElapsedSeconds = 0.f;

	RefreshSensorSnapshot();
}

// 현재 LiDAR 센서 관측값을 LastSensorSnapshot에 저장한다.
void ADeliveryBot::RefreshSensorSnapshot()
{
	LastSensorSnapshot = FDeliveryBotSensorSnapshot{};
	LastSensorSnapshot.SimulationTimeSeconds = FixedSimulationTimeSeconds;

	if (!IsValid(LidarSensorComponent))
		return;

	const FDeliveryBotLidarScanInfo rawScanInfo = LidarSensorComponent->ScanLidar();
	LastSensorSnapshot.LidarScanInfo = rawScanInfo;
	LastSensorSnapshot.LidarScanInfo.SimulationTimeSeconds = FixedSimulationTimeSeconds;
	LastSensorSnapshot.DetectedObjects = LidarSensorComponent->BuildDetectedObjects(LastSensorSnapshot.LidarScanInfo);
	LastSensorSnapshot.bHasFrontObject = LidarSensorComponent->FindNearestFrontObject(
		LastSensorSnapshot.LidarScanInfo,
		LastSensorSnapshot.FrontObjectInfo);

	++SensorSnapshotSequence;
}

// 고정 틱마다 Python policy component의 start retry와 decide 누적 시간을 진행한다.
void ADeliveryBot::UpdateFixedPolicy(float fixedDeltaSeconds)
{
	if (!IsValid(HttpPolicyComponent))
		return;

	HttpPolicyComponent->UpdatePolicy(fixedDeltaSeconds);
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
	CollisionStopTargetId.Reset();
	CollisionStopTargetTags.Reset();
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
		CollisionStopTargetId = ResolveScenarioTargetId(otherActor);
		CollisionStopTargetTags = otherActor->Tags;
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

void ADeliveryBot::InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo)
{
	ResetCollisionStopState();
	SetupInfo = setupInfo;
	ApplySetupInfo();
}

void ADeliveryBot::ApplyMoveCommand(const FDeliveryBotMoveCommandInfo& moveCommandInfo, float deltaTime)
{
	(void)deltaTime;

	if (bCollisionStopActive)
	{
		ApplyParkingStop();
		return;
	}

	if (!bHasLastMoveCommand)
	{
		UE_LOG(
			LogDeliveryBot,
			Log,
			TEXT("DeliveryBot received first policy move command | Actor=%s TargetSpeed=%.2f Steering=%.3f Brake=%.3f Direction=%d"),
			*GetName(),
			moveCommandInfo.TargetSpeedKmh,
			moveCommandInfo.Steering,
			moveCommandInfo.Brake,
			static_cast<int32>(moveCommandInfo.MoveDirectionType));
	}

	LastMoveCommandInfo = moveCommandInfo;
	LastActionReason = TEXT("python_policy");
	bHasLastMoveCommand = true;
}

// 저장된 최신 이동 명령을 매 고정 틱마다 차량에 적용한다.
void ADeliveryBot::ApplyLatestMoveCommand(float fixedDeltaSeconds)
{
	if (bCollisionStopActive)
	{
		ApplyParkingStop();
		return;
	}

	if (!bHasLastMoveCommand || !IsValid(DriveComponent))
		return;

	UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (!IsValid(vehicleMovement))
		return;

	DriveComponent->ApplyMoveCommand(vehicleMovement, LastMoveCommandInfo, fixedDeltaSeconds);
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

// Runner가 정한 user project output episode id를 HTTP policy component에 전달한다.
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
		target.TargetId = source.TargetId;
		target.TargetTags = source.TargetTags;
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
	observation.RobotState.CollisionTargetId = CollisionStopTargetId;
	observation.RobotState.CollisionTargetTags = CollisionStopTargetTags;

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

// Python policy의 마지막 decide 결과를 반환한다.
FDeliveryBotPolicyDecisionResultInfo ADeliveryBot::GetLastPolicyDecisionResult() const
{
	if (!IsValid(HttpPolicyComponent))
		return FDeliveryBotPolicyDecisionResultInfo{};

	return HttpPolicyComponent->GetLastPolicyDecisionResult();
}

// Python policy가 마지막으로 반환한 capture refs를 복사한다.
void ADeliveryBot::GetLastPythonCaptureRefs(TArray<FDeliveryBotPythonCaptureRefInfo>& outCaptureRefs) const
{
	outCaptureRefs.Reset();

	if (!IsValid(HttpPolicyComponent))
		return;

	HttpPolicyComponent->GetLastPythonCaptureRefs(outCaptureRefs);
}
