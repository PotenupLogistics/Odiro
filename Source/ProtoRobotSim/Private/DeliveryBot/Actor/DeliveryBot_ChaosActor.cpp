#include "DeliveryBot/Actor/DeliveryBot_ChaosActor.h"

#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"
#include "DeliveryBot/Component/DeliveryBot_DriveComponent.h"
#include "DeliveryBot/Component/DeliveryBot_GlobalPathComponent.h"
#include "DeliveryBot/Component/DeliveryBot_PathFollowComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "DrawDebugHelpers.h"
#include "DeliveryBot/Component/DeliveryBot_PolicyJudgmentComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"


namespace
{
	FString GetDeliveryBotPolicyActionName(EDeliveryBotPolicyActionType actionType)
	{
		const UEnum* enumPtr = StaticEnum<EDeliveryBotPolicyActionType>();
		if (!IsValid(enumPtr))
			return TEXT("Unknown");
		return enumPtr->GetNameStringByValue(static_cast<int64>(actionType));
	}
}



ADeliveryBot_ChaosActor::ADeliveryBot_ChaosActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	ChaosDriveComponent = CreateDefaultSubobject<UDeliveryBot_DriveComponent>(TEXT("ChaosDriveComponent"));
	GlobalPathComponent = CreateDefaultSubobject<UDeliveryBot_GlobalPathComponent>(TEXT("GlobalPathComponent"));
	PathFollowComponent = CreateDefaultSubobject<UDeliveryBot_PathFollowComponent>(TEXT("PathFollowComponent"));
	LidarSensorComponent = CreateDefaultSubobject<UDeliveryBot_LidarSensorComponent>(TEXT("LidarSensorComponent"));
	PolicyJudgmentComponent = CreateDefaultSubobject<UDeliveryBot_PolicyJudgmentComponent>(TEXT("PolicyJudgmentComponent"));
	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	
	UChaosWheeledVehicleMovementComponent* wheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	if (IsValid(ChaosDriveComponent))
		ChaosDriveComponent->SetupVehicleMovement(wheeledMovement);
	
}
void ADeliveryBot_ChaosActor::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* world = GetWorld())
	{
		FlushPersistentDebugLines(world);
	}

	if (IsValid(PolicyJudgmentComponent))
	{
		PolicyJudgmentComponent->OnPolicyFailed.RemoveDynamic(this, &ADeliveryBot_ChaosActor::HandlePolicyFailed);
		PolicyJudgmentComponent->OnPolicyFailed.AddDynamic(this, &ADeliveryBot_ChaosActor::HandlePolicyFailed);
	}

	ApplySetupInfo();

	UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();

	if (!IsValid(vehicleMovement))
		return;

	vehicleMovement->SetRequiresControllerForInputs(false);
	vehicleMovement->SetUseAutomaticGears(false);
	vehicleMovement->SetTargetGear(1, true);

	SetActorLocation(SetupInfo.LocationSetupInfo.StartLocationCm, false, nullptr, ETeleportType::TeleportPhysics);

	if (SetupInfo.LocationSetupInfo.bAutoStartRoute)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ADeliveryBot_ChaosActor::BuildGlobalPathAndStartFollow);
	}
}

void ADeliveryBot_ChaosActor::InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo)
{
	SetupInfo = setupInfo;

	bSimulationFailed = false;
	LastSimulationFailureInfo = FDeliveryBotSimulationFailureInfo{};
	TipOverStartTimeSeconds = -1.f;

	ApplySetupInfo();
}

void ADeliveryBot_ChaosActor::SetDrawDebugEnabled(bool bEnabled)
{
	bDrawDebug = bEnabled;

	if (!bDrawDebug)
	{
		if (UWorld* world = GetWorld())
		{
			// 모든 DebugLine 삭제 함수
			FlushPersistentDebugLines(world);
		}
	}
	ApplySetupInfo();
}

bool ADeliveryBot_ChaosActor::GetMeasurementSnapshot(FDeliveryBotMeasurementSnapshot& OutSnapshot) const
{
	OutSnapshot = FDeliveryBotMeasurementSnapshot{};
	OutSnapshot.LidarScanInfo = LastLidarScanInfo;
	for (const FDeliveryBotLidarRayInfo& RayInfo : LastLidarScanInfo.RayInfos)
	{
		if (RayInfo.bHit)
		{
			++OutSnapshot.LidarHitCount;
		}
	}
	OutSnapshot.bHasFrontObject = IsValid(LidarSensorComponent)
		&& LidarSensorComponent->FindNearestFrontObject(LastLidarScanInfo, OutSnapshot.FrontObjectInfo);
	OutSnapshot.MoveCommandInfo = LastMoveCommandInfo;
	OutSnapshot.bHasMoveCommand = bHasLastMoveCommand;
	OutSnapshot.ActionReason = LastActionReason;
	return true;
}

void ADeliveryBot_ChaosActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawDebug != bLastAppliedDrawDebug)
	{
		ApplySetupInfo();
	}

	if (bSimulationFailed)
	{
		ApplyFailureStopCommand(DeltaSeconds);
		return;
	}

	CheckTipOverFailure();

	if (bSimulationFailed)
	{
		ApplyFailureStopCommand(DeltaSeconds);
		return;
	}

	UpdateLidarScan();
	DebugFrontLidarObject();

	ApplyPathFollowMoveCommand(DeltaSeconds);
}

void ADeliveryBot_ChaosActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupSimulationRuntimeState();

	Super::EndPlay(EndPlayReason);
}

void ADeliveryBot_ChaosActor::ApplySetupInfo()
{
	UChaosWheeledVehicleMovementComponent* wheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	if (IsValid(ChaosDriveComponent))
		ChaosDriveComponent->InitializeChaosDrive(wheeledMovement, SetupInfo.ChaosDriveConfigInfo);

	if (IsValid(PathFollowComponent))
	{
		FDeliveryBotPathFollowConfigInfo pathFollowConfigInfo = SetupInfo.PathFollowConfigInfo;
		pathFollowConfigInfo.bDrawDebug = bDrawDebug;
		PathFollowComponent->InitializePathFollow(pathFollowConfigInfo);
	}

	if (IsValid(LidarSensorComponent))
	{
		FDeliveryBotLidarSensorConfigInfo lidarSensorConfigInfo = SetupInfo.LidarSensorConfigInfo;
		lidarSensorConfigInfo.bDrawDebug = bDrawDebug;
		LidarSensorComponent->InitializeLidar(lidarSensorConfigInfo);
	}

	if (IsValid(GlobalPathComponent))
	{
		GlobalPathComponent->SetDrawDebugEnabled(bDrawDebug);
	}

	if (UWorld* world = GetWorld())
		if (UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>())
			gridSubsystem->SetDrawDebugEnabled(bDrawDebug);

	bLastAppliedDrawDebug = bDrawDebug;
}

void ADeliveryBot_ChaosActor::BuildGlobalPathAndStartFollow()
{
	if (!IsValid(GlobalPathComponent) || !IsValid(PathFollowComponent))
	{
		FailSimulation(BuildSimulationFailureInfo(
			EDeliveryBotSimulationFailureType::PathFindingFailed,
			TEXT("Path component is invalid")));
		return;
	}

	// const bool bSuccess
	// {
	// 	GlobalPathComponent->BuildPathByAStar(
	// 		SetupInfo.LocationSetupInfo.StartLocationCm,
	// 		SetupInfo.LocationSetupInfo.GoalLocationCm)
	// };
	const bool bSuccess
	{
		GlobalPathComponent->BuildPath(
			SetupInfo.LocationSetupInfo.StartLocationCm,
			SetupInfo.LocationSetupInfo.GoalLocationCm,
			SetupInfo.NavigationConfigInfo)
	};
	
	if (!bSuccess)
	{
		FailSimulation(BuildSimulationFailureInfo(
			EDeliveryBotSimulationFailureType::PathFindingFailed,
			TEXT("DeliveryBot path build failed")));
		return;
	}
	
	AlignRotationToPathStart();
	PathFollowComponent->SetPathPointInfos(GlobalPathComponent->GetGlobalPathPointInfos());
}


void ADeliveryBot_ChaosActor::ApplyPathFollowMoveCommand(float deltaTime)
{
	if (!IsValid(PathFollowComponent) || !IsValid(ChaosDriveComponent))
		return;

	UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (!IsValid(vehicleMovement))
		return;

	// FDeliveryBotMoveCommandInfo moveCommandInfo = PathFollowComponent->BuildMoveCommand(deltaTime);
	FDeliveryBotMoveCommandInfo moveCommandInfo = PathFollowComponent->BuildDriveCommand(deltaTime,	SetupInfo.NavigationConfigInfo);

	FString actionReason{ TEXT("path_follow") };

	FDeliveryBotLidarDetectedObjectInfo frontObjectInfo;
	const bool bHasFrontObject =
		IsValid(LidarSensorComponent) &&	LidarSensorComponent->FindNearestFrontObject(LastLidarScanInfo, frontObjectInfo);

	if (IsValid(PolicyJudgmentComponent))
	{
		const FDeliveryBotPolicyContextInfo contextInfo = BuildPolicyContextInfo(bHasFrontObject, frontObjectInfo);
		const FDeliveryBotPolicyDecisionInfo decisionInfo = PolicyJudgmentComponent->EvaluatePolicy(contextInfo);

		actionReason = ApplyPolicyDecisionToMoveCommand(moveCommandInfo, decisionInfo, frontObjectInfo, deltaTime);
	}

	if (!bHasFrontObject || frontObjectInfo.ClosestFrontDistanceM > SetupInfo.LidarSensorConfigInfo.SlowDownDistanceM)
	{
		if (!IsInRepathMoveGraceTime())
		{
			ClearLidarDynamicBlockedCells();
		}
	}

	LastMoveCommandInfo = moveCommandInfo;
	LastActionReason = actionReason;
	bHasLastMoveCommand = true;
	ChaosDriveComponent->ApplyMoveCommand(vehicleMovement, moveCommandInfo, deltaTime);
}

void ADeliveryBot_ChaosActor::ApplyStopCommand(FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	moveCommandInfo.TargetSpeedKmh = 0.f;
	moveCommandInfo.Steering = 0.f;
	moveCommandInfo.Brake = 0.f;
	moveCommandInfo.bBrake = true;
}

bool ADeliveryBot_ChaosActor::TryRequestRepathByFrontObject(const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo)
{
	UWorld* world = GetWorld();
	if (!IsValid(world) || !IsValid(GlobalPathComponent) || !IsValid(PathFollowComponent))
		return false;

	if (world->GetTimeSeconds() - LastRepathRequestTimeSeconds < RepathCooldownSeconds)
		return false;

	AActor* detectedActor = frontObjectInfo.DetectedActor.Get();
	if (!IsValid(detectedActor))
		return false;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();

	if (!IsValid(gridSubsystem))
		return false;

	LastRepathRequestTimeSeconds = world->GetTimeSeconds();

	gridSubsystem->ClearDynamicBlockedCells();
	bHasLidarDynamicBlockedCells = false;
	const int32 lidarBlockedActorCount = SetLidarDetectedActorsAsDynamicBlocked(gridSubsystem, detectedActor);
	bHasLidarDynamicBlockedCells = lidarBlockedActorCount > 0;

	const bool bSuccess =
	GlobalPathComponent->BuildPath(
		GetActorLocation(),
		SetupInfo.LocationSetupInfo.GoalLocationCm,
		SetupInfo.NavigationConfigInfo
	);

	// const bool bSuccess =
	// 	GlobalPathComponent->BuildPathByAStar(
	// 		GetActorLocation(),
	// 		SetupInfo.LocationSetupInfo.GoalLocationCm
	// 	);

	if (!bSuccess)
	{
		UE_LOG(	LogTemp,Warning,TEXT("DeliveryBot Repath failed | FrontObject: %s, LidarBlockedActors: %d"),*detectedActor->GetName(),	lidarBlockedActorCount);
		return false;
	}

	PathFollowComponent->SetPathPointInfos(GlobalPathComponent->GetGlobalPathPointInfos());
	LastSuccessfulRepathTimeSeconds = world->GetTimeSeconds();

	UE_LOG(
	LogTemp,
	Warning,
	TEXT("DeliveryBot Repath success | FrontObject: %s, LidarBlockedActors: %d, PathCount: %d"),
	*detectedActor->GetName(),
	lidarBlockedActorCount,
	GlobalPathComponent->GetGlobalPath().Num());

	return true;
}

// 재경로 찾은 후 잠깐 동안 찾은 길로 움직이도록 해주는 함수(똑같은 장애물보고 바로 다시 재경로 찾지 않도록) 
bool ADeliveryBot_ChaosActor::IsInRepathMoveGraceTime() const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	return world->GetTimeSeconds() - LastSuccessfulRepathTimeSeconds <= RepathMoveGraceSeconds;
}

void ADeliveryBot_ChaosActor::ClearLidarDynamicBlockedCells()
{
	if (!bHasLidarDynamicBlockedCells)
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();

	if (!IsValid(gridSubsystem))
		return;

	gridSubsystem->ClearDynamicBlockedCells();
	bHasLidarDynamicBlockedCells = false;
}

void ADeliveryBot_ChaosActor::AlignRotationToPathStart()
{
	if (!IsValid(GlobalPathComponent))
		return;

	const TArray<FDeliveryBotPathPointInfo>& pathPointInfos =
		GlobalPathComponent->GetGlobalPathPointInfos();

	if (pathPointInfos.Num() == 0)
		return;

	const float startHeadingDegree =
		FMath::RadiansToDegrees(pathPointInfos[0].HeadingRadian);

	SetActorRotation(
		FRotator(0.f, startHeadingDegree, 0.f),
		ETeleportType::TeleportPhysics);
}

void ADeliveryBot_ChaosActor::UpdateLidarScan()
{
	if (!IsValid(LidarSensorComponent))
		return;

	LastLidarScanInfo = LidarSensorComponent->ScanLidar2D();
}

void ADeliveryBot_ChaosActor::DebugFrontLidarObject() const
{
	if (!IsValid(LidarSensorComponent))
		return;

	FDeliveryBotLidarDetectedObjectInfo frontObjectInfo;

	if (!LidarSensorComponent->FindNearestFrontObject(LastLidarScanInfo, frontObjectInfo))
		return;
	// UE_LOG(
	// 	LogTemp,
	// 	Warning,
	// 	TEXT("FrontObject: %s, FrontDistanceM: %.2f, FrontYaw: %.1f, FrontRays: %d, TotalRays: %d"),
	// 	*frontObjectInfo.ActorName,
	// 	frontObjectInfo.ClosestFrontDistanceM,
	// 	frontObjectInfo.ClosestFrontRayYawDegree,
	// 	frontObjectInfo.FrontHitRayCount,
	// 	frontObjectInfo.TotalHitRayCount
	// );
}

// 임시 장애물 표시
int32 ADeliveryBot_ChaosActor::SetLidarDetectedActorsAsDynamicBlocked(UDeliveryBot_GridSubsystem* gridSubsystem,AActor* requiredFrontActor) const
{
	if (!IsValid(gridSubsystem) || !IsValid(LidarSensorComponent))
		return 0;

	TSet<AActor*> blockedActors;

	if (IsValid(requiredFrontActor) && requiredFrontActor != this)
	{
		blockedActors.Add(requiredFrontActor);
	}

	const TArray<FDeliveryBotLidarDetectedObjectInfo> detectedObjectInfos = LidarSensorComponent->BuildDetectedObjects(LastLidarScanInfo);

	for (const FDeliveryBotLidarDetectedObjectInfo& detectedObjectInfo : detectedObjectInfos)
	{
		AActor* detectedActor = detectedObjectInfo.DetectedActor.Get();

		if (!IsValid(detectedActor) || detectedActor == this)
			continue;

		blockedActors.Add(detectedActor);
	}

	for (AActor* blockedActor : blockedActors)
	{
		gridSubsystem->SetDynamicBlockedByActorBounds(blockedActor);
	}

	return blockedActors.Num();
}

void ADeliveryBot_ChaosActor::ApplyFrontObstacleSlowDown(FDeliveryBotMoveCommandInfo& moveCommandInfo, const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo) const
{
	const float stopDistanceM = FMath::Max(SetupInfo.LidarSensorConfigInfo.StopDistanceM, 0.f);

	const float slowDownDistanceM = FMath::Max(SetupInfo.LidarSensorConfigInfo.SlowDownDistanceM,stopDistanceM + 0.1f);

	const float slowSpeedKmh = FMath::Max(SetupInfo.PathFollowConfigInfo.ObstacleSlowSpeedKmh,0.f);

	const float distanceM = frontObjectInfo.ClosestFrontDistanceM;

	float limitedSpeedKmh = slowSpeedKmh;

	if (distanceM > stopDistanceM && distanceM <= slowDownDistanceM)
	{
		const float distanceAlpha = FMath::Clamp((distanceM - stopDistanceM) / (slowDownDistanceM - stopDistanceM),0.f,1.f);

		limitedSpeedKmh = FMath::Lerp(slowSpeedKmh,moveCommandInfo.TargetSpeedKmh, distanceAlpha);
	}

	moveCommandInfo.TargetSpeedKmh = FMath::Min(moveCommandInfo.TargetSpeedKmh,limitedSpeedKmh);

	moveCommandInfo.Brake = 0.f;
	moveCommandInfo.bBrake = false;
}

FDeliveryBotPolicyContextInfo ADeliveryBot_ChaosActor::BuildPolicyContextInfo(bool bHasFrontObject, const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo) const
{
	FDeliveryBotPolicyContextInfo contextInfo;

	contextInfo.bHasFrontObject = bHasFrontObject;
	contextInfo.FrontObjectDistanceM = bHasFrontObject ? frontObjectInfo.ClosestFrontDistanceM : 0.f;

	contextInfo.StopDistanceM = FMath::Max(SetupInfo.LidarSensorConfigInfo.StopDistanceM,0.f);

	contextInfo.SlowDownDistanceM = FMath::Max(SetupInfo.LidarSensorConfigInfo.SlowDownDistanceM,contextInfo.StopDistanceM + 0.1f);

	contextInfo.MaxSpeedKmh = SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh;

	const UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (IsValid(vehicleMovement))
	{
		contextInfo.CurrentSpeedKmh = FMath::Abs(vehicleMovement->GetForwardSpeed()) * 0.036f;
	}

	contextInfo.bCanRepath = bUseFrontObstacleRepath;
	contextInfo.bInRepathMoveGraceTime = IsInRepathMoveGraceTime();

	return contextInfo;
}

FString ADeliveryBot_ChaosActor::ApplyPolicyDecisionToMoveCommand(
	FDeliveryBotMoveCommandInfo& moveCommandInfo,const FDeliveryBotPolicyDecisionInfo& decisionInfo,
	const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo,	float deltaTime)
{
	if (bLogPolicyDecision && decisionInfo.ActionType != EDeliveryBotPolicyActionType::None)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("DeliveryBot Policy | Action: %s, FromRemote: %s, Reason: %s"),
			*GetDeliveryBotPolicyActionName(decisionInfo.ActionType),
			decisionInfo.bFromRemoteApi ? TEXT("true") : TEXT("false"),
			*decisionInfo.Reason
		);
	}

	switch (decisionInfo.ActionType)
	{
		case EDeliveryBotPolicyActionType::SlowDown:
		{
			ApplyFrontObstacleSlowDown(moveCommandInfo, frontObjectInfo);
			return decisionInfo.Reason.Contains(TEXT("Repath grace"))
				? FString(TEXT("repath_grace"))
				: FString(TEXT("slowdown"));
		}

		case EDeliveryBotPolicyActionType::Stop:
		{
			ApplyStopCommand(moveCommandInfo);
			return TEXT("stop");
		}

		case EDeliveryBotPolicyActionType::Repath:
		{
			if (TryRequestRepathByFrontObject(frontObjectInfo))
			{
				moveCommandInfo = PathFollowComponent->BuildDriveCommand(deltaTime,SetupInfo.NavigationConfigInfo);
				ApplyFrontObstacleSlowDown(moveCommandInfo, frontObjectInfo);
				return TEXT("repath");
			}
			else
			{
				ApplyStopCommand(moveCommandInfo);
				return TEXT("stop");
			}
		}

	case EDeliveryBotPolicyActionType::None:
	default:
		break;
	}

	return TEXT("path_follow");
}

void ADeliveryBot_ChaosActor::HandlePolicyFailed(const FDeliveryBotPolicyFailureInfo& failureInfo)
{
	const FDeliveryBotSimulationFailureInfo simulationFailureInfo = 	BuildPolicySimulationFailureInfo(failureInfo);
	FailSimulation(simulationFailureInfo);
}

void ADeliveryBot_ChaosActor::ApplyFailureStopCommand(float deltaTime)
{
	if (!IsValid(ChaosDriveComponent))
		return;

	UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (!IsValid(vehicleMovement))
		return;

	FDeliveryBotMoveCommandInfo moveCommandInfo;
	ApplyStopCommand(moveCommandInfo);

	LastMoveCommandInfo = moveCommandInfo;
	LastActionReason = TEXT("simulation_failed");
	bHasLastMoveCommand = true;
	ChaosDriveComponent->ApplyMoveCommand(vehicleMovement, moveCommandInfo, deltaTime);
}


void ADeliveryBot_ChaosActor::FailSimulation(const FDeliveryBotSimulationFailureInfo& failureInfo)
{
	if (bSimulationFailed)
		return;

	bSimulationFailed = true;
	LastSimulationFailureInfo = failureInfo;

	ClearLidarDynamicBlockedCells();

	if (IsValid(PathFollowComponent))
	{
		PathFollowComponent->ClearPath();
	}

	if (UWorld* world = GetWorld())
	{
		ApplyFailureStopCommand(world->GetDeltaSeconds());
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("DeliveryBot simulation failed | Type: %d, Message: %s, Location: %s, SpeedKmh: %.2f"),
		static_cast<int32>(failureInfo.FailureType),
		*failureInfo.Message,
		*failureInfo.LocationCm.ToString(),
		failureInfo.SpeedKmh);
	
	// 실패 로그 찍기
	if (LastSimulationFailureInfo.bHasPolicyFailureInfo)
	{
		const FDeliveryBotPolicyFailureInfo& policyFailureInfo =
			LastSimulationFailureInfo.PolicyFailureInfo;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("DeliveryBot Policy Failure Detail | PolicyFailureType: %d, RequestId: %s, Url: %s, ResponseCode: %d, ElapsedSecond: %.2f, Message: %s"),
			static_cast<int32>(policyFailureInfo.FailureType),
			*policyFailureInfo.RequestId,
			*policyFailureInfo.PolicyServerUrl,
			policyFailureInfo.ResponseCode,
			policyFailureInfo.RequestElapsedSecond,
			*policyFailureInfo.Message
		);
	}
	OnDeliveryBotSimulationFailed.Broadcast(this, LastSimulationFailureInfo);
}

FDeliveryBotSimulationFailureInfo ADeliveryBot_ChaosActor::BuildPolicySimulationFailureInfo(const FDeliveryBotPolicyFailureInfo& policyFailureInfo) const
{
	FDeliveryBotSimulationFailureInfo simulationFailureInfo;

	simulationFailureInfo.FailureType = EDeliveryBotSimulationFailureType::PolicyRequestFailed;
	simulationFailureInfo.Message = policyFailureInfo.Message;
	simulationFailureInfo.LocationCm = GetActorLocation();
	simulationFailureInfo.SpeedKmh = GetCurrentSpeedKmh();
	simulationFailureInfo.bHasPolicyFailureInfo = true;
	simulationFailureInfo.PolicyFailureInfo = policyFailureInfo;

	const UWorld* world = GetWorld();
	if (IsValid(world))
	{
		simulationFailureInfo.TimeSeconds = world->GetTimeSeconds();
	}

	return simulationFailureInfo;
}

float ADeliveryBot_ChaosActor::GetCurrentSpeedKmh() const
{
	const UChaosVehicleMovementComponent* vehicleMovement = GetVehicleMovementComponent();
	if (!IsValid(vehicleMovement))
		return 0.f;

	return FMath::Abs(vehicleMovement->GetForwardSpeed()) * 0.036f;
}

void ADeliveryBot_ChaosActor::CheckTipOverFailure()
{
	if (!bFailOnTipOver)
		return;

	const FRotator actorRotation = GetActorRotation();

	const float absRollDegree = FMath::Abs(FRotator::NormalizeAxis(actorRotation.Roll));
	const float absPitchDegree = FMath::Abs(FRotator::NormalizeAxis(actorRotation.Pitch));

	const bool bTipOver = absRollDegree >= TipOverFailureAngleDegree ||	absPitchDegree >= TipOverFailureAngleDegree;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	if (!bTipOver)
	{
		TipOverStartTimeSeconds = -1.f;
		return;
	}

	if (TipOverStartTimeSeconds < 0.f)
	{
		TipOverStartTimeSeconds = world->GetTimeSeconds();
		return;
	}

	const float elapsedSecond = world->GetTimeSeconds() - TipOverStartTimeSeconds;
	if (elapsedSecond < TipOverFailureHoldSecond)
		return;

	FailSimulation(BuildSimulationFailureInfo(EDeliveryBotSimulationFailureType::RobotTipOver,
		FString::Printf(TEXT("DeliveryBot tip over | Roll: %.1f, Pitch: %.1f"),	absRollDegree,	absPitchDegree)));
}

FDeliveryBotSimulationFailureInfo ADeliveryBot_ChaosActor::BuildSimulationFailureInfo(	EDeliveryBotSimulationFailureType failureType,	const FString& message,	AActor* targetActor) const
{
	FDeliveryBotSimulationFailureInfo failureInfo;

	failureInfo.FailureType = failureType;
	failureInfo.Message = message;
	failureInfo.LocationCm = GetActorLocation();
	failureInfo.SpeedKmh = GetCurrentSpeedKmh();
	failureInfo.TargetActor = targetActor;

	const UWorld* world = GetWorld();
	if (IsValid(world))
	{
		failureInfo.TimeSeconds = world->GetTimeSeconds();
	}

	return failureInfo;
}

bool ADeliveryBot_ChaosActor::ShouldFailByCollision(AActor* otherActor,	const FHitResult& hit) const
{
	if (!bFailOnCollision)
		return false;

	if (!IsValid(otherActor) || otherActor == this)
		return false;

	if (otherActor->ActorHasTag(TEXT("NoCollision")))
		return false;

	if (hit.ImpactNormal.Z >= GroundHitNormalZThreshold)
		return false;

	if (GetCurrentSpeedKmh() < CollisionFailureMinSpeedKmh)
		return false;

	return true;
}

FDeliveryBotSimulationFailureInfo ADeliveryBot_ChaosActor::BuildCollisionSimulationFailureInfo(	AActor* otherActor, const FHitResult& hit) const
{
	FDeliveryBotSimulationFailureInfo failureInfo = BuildSimulationFailureInfo(
		EDeliveryBotSimulationFailureType::Collision,
		IsValid(otherActor)	
		? FString::Printf(TEXT("DeliveryBot collided with %s"), *otherActor->GetName())
		: TEXT("DeliveryBot collided with unknown actor"), otherActor);

	failureInfo.LocationCm = hit.ImpactPoint;

	return failureInfo;
}

void ADeliveryBot_ChaosActor::CleanupSimulationRuntimeState()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (UWorld* world = GetWorld())
	{
		ApplyFailureStopCommand(world->GetDeltaSeconds());
	}

	if (IsValid(PolicyJudgmentComponent))
	{
		PolicyJudgmentComponent->OnPolicyFailed.RemoveDynamic(
			this,
			&ADeliveryBot_ChaosActor::HandlePolicyFailed);

		PolicyJudgmentComponent->CancelPendingRemoteRequest();
	}

	ClearLidarDynamicBlockedCells();

	if (IsValid(PathFollowComponent))
	{
		PathFollowComponent->ClearPath();
	}
}

void ADeliveryBot_ChaosActor::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
                                        FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(
		MyComp,
		Other,
		OtherComp,
		bSelfMoved,
		HitLocation,
		HitNormal,
		NormalImpulse,
		Hit);

	if (bSimulationFailed)
		return;

	if (!ShouldFailByCollision(Other, Hit))
		return;

	FailSimulation(BuildCollisionSimulationFailureInfo(Other, Hit));
}
