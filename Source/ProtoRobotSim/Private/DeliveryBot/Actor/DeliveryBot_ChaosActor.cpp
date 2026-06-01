#include "DeliveryBot/Actor/DeliveryBot_ChaosActor.h"

#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"
#include "DeliveryBot/Component/DeliveryBot_ChaosDriveComponent.h"
#include "DeliveryBot/Component/DeliveryBot_GlobalPathComponent.h"
#include "DeliveryBot/Component/DeliveryBot_PathFollowComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"

ADeliveryBot_ChaosActor::ADeliveryBot_ChaosActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	ChaosDriveComponent = CreateDefaultSubobject<UDeliveryBot_ChaosDriveComponent>(TEXT("ChaosDriveComponent"));
	GlobalPathComponent = CreateDefaultSubobject<UDeliveryBot_GlobalPathComponent>(TEXT("GlobalPathComponent"));
	PathFollowComponent = CreateDefaultSubobject<UDeliveryBot_PathFollowComponent>(TEXT("PathFollowComponent"));
	LidarSensorComponent = CreateDefaultSubobject<UDeliveryBot_LidarSensorComponent>(TEXT("LidarSensorComponent"));
	
	UChaosWheeledVehicleMovementComponent* wheeledMovement{
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent())
	};

	if (IsValid(ChaosDriveComponent))
	{
		ChaosDriveComponent->SetupVehicleMovement(wheeledMovement);
	}
}

void ADeliveryBot_ChaosActor::BeginPlay()
{
	Super::BeginPlay();

	ApplySetupInfo();

	// UE_LOG(
	// LogTemp,
	// Warning,
	// TEXT("DeliveryBot Setup | Start: %s, Goal: %s, AutoStart: %s, MaxSpeedKmh: %.2f, TargetSpeedKmh: %.2f, LidarRangeM: %.2f"),
	// *SetupInfo.LocationSetupInfo.StartLocationCm.ToString(),
	// *SetupInfo.LocationSetupInfo.GoalLocationCm.ToString(),
	// SetupInfo.LocationSetupInfo.bAutoStartRoute ? TEXT("true") : TEXT("false"),
	// SetupInfo.ChaosDriveConfigInfo.MaxSpeedKmh,
	// SetupInfo.PathFollowConfigInfo.TargetSpeedKmh,
	// SetupInfo.LidarSensorConfigInfo.ScanRangeM);
	
	UChaosVehicleMovementComponent* vehicleMovement{ GetVehicleMovementComponent() };

	if (!IsValid(vehicleMovement))
		return;

	vehicleMovement->SetRequiresControllerForInputs(false);
	vehicleMovement->SetUseAutomaticGears(true);
	vehicleMovement->SetTargetGear(1, true);
	
	SetActorLocation(
		SetupInfo.LocationSetupInfo.StartLocationCm,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (SetupInfo.LocationSetupInfo.bAutoStartRoute)
	{
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ADeliveryBot_ChaosActor::BuildGlobalPathAndStartFollow
		);
	}
}

void ADeliveryBot_ChaosActor::InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo)
{
	SetupInfo = setupInfo;
	ApplySetupInfo();
}

void ADeliveryBot_ChaosActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLidarScan();
	DebugFrontLidarObject();

	ApplyPathFollowMoveCommand(DeltaSeconds);
}

void ADeliveryBot_ChaosActor::ApplySetupInfo()
{
	UChaosWheeledVehicleMovementComponent* wheeledMovement{	Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent())};

	if (IsValid(ChaosDriveComponent))
	{
		ChaosDriveComponent->InitializeChaosDrive(
			wheeledMovement,
			SetupInfo.ChaosDriveConfigInfo
		);
	}

	if (IsValid(PathFollowComponent))
		PathFollowComponent->InitializePathFollow(SetupInfo.PathFollowConfigInfo);

	if (IsValid(LidarSensorComponent))
		LidarSensorComponent->InitializeLidar(SetupInfo.LidarSensorConfigInfo);
}

void ADeliveryBot_ChaosActor::BuildGlobalPathAndStartFollow()
{
	if (!IsValid(GlobalPathComponent) || !IsValid(PathFollowComponent))
		return;

	const bool bSuccess{
		GlobalPathComponent->BuildPathByAStar(
			SetupInfo.LocationSetupInfo.StartLocationCm,
			SetupInfo.LocationSetupInfo.GoalLocationCm
		)
	};

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeliveryBot Chaos A* path build failed."));
		return;
	}
	AlignRotationToPathStart();
	PathFollowComponent->SetPath(GlobalPathComponent->GetGlobalPath());
}

void ADeliveryBot_ChaosActor::ApplyPathFollowMoveCommand(float deltaTime)
{
	if (!IsValid(PathFollowComponent) || !IsValid(ChaosDriveComponent))
		return;

	UChaosVehicleMovementComponent* vehicleMovement{ GetVehicleMovementComponent() };
	if (!IsValid(vehicleMovement))
		return;

	FDeliveryBotMoveCommandInfo moveCommandInfo{
		PathFollowComponent->BuildMoveCommand(deltaTime)
	};

	FDeliveryBotLidarDetectedObjectInfo frontObjectInfo;
	const bool bShouldStopByFrontObject{
		IsValid(LidarSensorComponent) &&
		LidarSensorComponent->ShouldStopByFrontObject(LastLidarScanInfo, frontObjectInfo)
	};

	if (bShouldStopByFrontObject)
	{
		if (IsInRepathMoveGraceTime())
		{
			moveCommandInfo = PathFollowComponent->BuildMoveCommand(deltaTime);
			moveCommandInfo.TargetSpeedKmh = FMath::Min(
				moveCommandInfo.TargetSpeedKmh,
				SetupInfo.PathFollowConfigInfo.MinTurnSpeedKmh
			);
		}
		else
		{
			const bool bRepathSuccess{
				bUseFrontObstacleRepath && TryRequestRepathByFrontObject(frontObjectInfo)
			};

			if (bRepathSuccess)
			{
				moveCommandInfo = PathFollowComponent->BuildMoveCommand(deltaTime);
				moveCommandInfo.TargetSpeedKmh = FMath::Min(
					moveCommandInfo.TargetSpeedKmh,
					SetupInfo.PathFollowConfigInfo.MinTurnSpeedKmh
				);
			}
			else
			{
				ApplyStopCommand(moveCommandInfo);
			}
		}
	}

	if (!bShouldStopByFrontObject && !IsInRepathMoveGraceTime())
	{
		ClearLidarDynamicBlockedCells();
	}

	ChaosDriveComponent->ApplyMoveCommand(vehicleMovement, moveCommandInfo, deltaTime);
}

void ADeliveryBot_ChaosActor::ApplyStopCommand(FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	moveCommandInfo.TargetSpeedKmh = 0.f;
	moveCommandInfo.Steering = 0.f;
	moveCommandInfo.Brake = 1.f;
	moveCommandInfo.bBrake = true;
}

bool ADeliveryBot_ChaosActor::TryRequestRepathByFrontObject(const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo)
{
	UWorld* world{ GetWorld() };
	if (!IsValid(world) || !IsValid(GlobalPathComponent) || !IsValid(PathFollowComponent))
		return false;

	if (world->GetTimeSeconds() - LastRepathRequestTimeSeconds < RepathCooldownSeconds)
		return false;

	AActor* detectedActor{ frontObjectInfo.DetectedActor.Get() };
	if (!IsValid(detectedActor))
		return false;

	UDeliveryBot_GridSubsystem* gridSubsystem{
		world->GetSubsystem<UDeliveryBot_GridSubsystem>()
	};

	if (!IsValid(gridSubsystem))
		return false;

	LastRepathRequestTimeSeconds = world->GetTimeSeconds();

	gridSubsystem->ClearDynamicBlockedCells();
	bHasLidarDynamicBlockedCells = false;
	const int32 lidarBlockedActorCount{SetLidarDetectedActorsAsDynamicBlocked(gridSubsystem, detectedActor)};
	bHasLidarDynamicBlockedCells = lidarBlockedActorCount > 0;

	const bool bSuccess{
		GlobalPathComponent->BuildPathByAStar(
			GetActorLocation(),
			SetupInfo.LocationSetupInfo.GoalLocationCm
		)
	};

	if (!bSuccess)
	{
		UE_LOG(	LogTemp,Warning,TEXT("DeliveryBot Repath failed | FrontObject: %s, LidarBlockedActors: %d"),*detectedActor->GetName(),	lidarBlockedActorCount);
		return false;
	}

	PathFollowComponent->SetPath(GlobalPathComponent->GetGlobalPath());
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

bool ADeliveryBot_ChaosActor::IsInRepathMoveGraceTime() const
{
	const UWorld* world{ GetWorld() };
	if (!IsValid(world))
		return false;

	return world->GetTimeSeconds() - LastSuccessfulRepathTimeSeconds <= RepathMoveGraceSeconds;
}

void ADeliveryBot_ChaosActor::ClearLidarDynamicBlockedCells()
{
	if (!bHasLidarDynamicBlockedCells)
		return;

	UWorld* world{ GetWorld() };
	if (!IsValid(world))
		return;

	UDeliveryBot_GridSubsystem* gridSubsystem{
		world->GetSubsystem<UDeliveryBot_GridSubsystem>()
	};

	if (!IsValid(gridSubsystem))
		return;

	gridSubsystem->ClearDynamicBlockedCells();
	bHasLidarDynamicBlockedCells = false;
}

void ADeliveryBot_ChaosActor::AlignRotationToPathStart()
{
	if (!IsValid(GlobalPathComponent))
		return;

	const TArray<FVector>& pathPoints{ GlobalPathComponent->GetGlobalPath() };

	if (pathPoints.Num() < 2)
		return;

	FVector direction{ pathPoints[1] - pathPoints[0] };
	direction.Z = 0.f;

	if (!direction.Normalize())
		return;

	SetActorRotation(direction.Rotation(), ETeleportType::TeleportPhysics);
}

void ADeliveryBot_ChaosActor::UpdateLidarScan()
{
	if (!IsValid(LidarSensorComponent))
		return;

	LastLidarScanInfo = LidarSensorComponent->ScanLidar();
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

int32 ADeliveryBot_ChaosActor::SetLidarDetectedActorsAsDynamicBlocked(
	UDeliveryBot_GridSubsystem* gridSubsystem,
	AActor* requiredFrontActor) const
{
	if (!IsValid(gridSubsystem) || !IsValid(LidarSensorComponent))
		return 0;

	TSet<AActor*> blockedActors;

	if (IsValid(requiredFrontActor) && requiredFrontActor != this)
	{
		blockedActors.Add(requiredFrontActor);
	}

	const TArray<FDeliveryBotLidarDetectedObjectInfo> detectedObjectInfos{
		LidarSensorComponent->BuildDetectedObjects(LastLidarScanInfo)
	};

	for (const FDeliveryBotLidarDetectedObjectInfo& detectedObjectInfo : detectedObjectInfos)
	{
		AActor* detectedActor{ detectedObjectInfo.DetectedActor.Get() };

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
