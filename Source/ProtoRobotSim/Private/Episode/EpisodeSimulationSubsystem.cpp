#include "Episode/EpisodeSimulationSubsystem.h"
#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePedestrianRuntimeComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/EpisodePedestrianPlanSubsystem.h"
#include "Shared/EpisodePedestrianPlanTypes.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"


DEFINE_LOG_CATEGORY_STATIC(LogEpisodeSimulation, Log, All);

UEpisodeSimulationSubsystem::UEpisodeSimulationSubsystem()
{
	StaticObstacleClass = AEpisodeStaticObstacle::StaticClass();
	StaticObstaclePropCatalog = UEpisodeStaticObstaclePropCatalog::MakeDefaultCatalogReference();

	static ConstructorHelpers::FClassFinder<ADeliveryBot> robotBlueprintClass(
		TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot"));

	if (robotBlueprintClass.Succeeded())
	{
		RobotActorClass = robotBlueprintClass.Class;
	}
	else
	{
		RobotActorClass = ADeliveryBot::StaticClass();
	}
	static ConstructorHelpers::FClassFinder<AActor> goalPointBlueprintClass(TEXT("/Game/Blueprints/Episode/BP_GoalPoint"));
	if (goalPointBlueprintClass.Succeeded())
	{
		GoalPointClass = goalPointBlueprintClass.Class;
	}
	static ConstructorHelpers::FClassFinder<AActor> startPointBlueprintClass(TEXT("/Game/Blueprints/Episode/BP_StartPoint"));
	if (startPointBlueprintClass.Succeeded())
	{
		StartPointClass = startPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<ADeliveryBot_GridBoundsActor> gridBoundsActorBlueprintClass(
		TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot_GridBoundsActor"));
	if (gridBoundsActorBlueprintClass.Succeeded())
	{
		GridBoundsActorClass = gridBoundsActorBlueprintClass.Class;
	}
	else
	{
		GridBoundsActorClass = ADeliveryBot_GridBoundsActor::StaticClass();
	}

	static ConstructorHelpers::FClassFinder<AEpisodePedestrian> pedestrianBlueprintClass(TEXT("/Game/Blueprints/Episode/BP_EpisodePedestrian"));
	if (pedestrianBlueprintClass.Succeeded())
	{
		PedestrianClass = pedestrianBlueprintClass.Class;
	}
	else
	{
		PedestrianClass = AEpisodePedestrian::StaticClass();
	}
}

void UEpisodeSimulationSubsystem::ClearEpisode()
{
	const int32 actorCount = RuntimeActors.Num();
	const int32 actorIdCount = RuntimeActorsById.Num();
	const int32 groundRegionCount = RuntimeGroundRegions.Num();
	const int32 pathCount = RuntimePaths.Num();

	if (IsValid(RuntimeGridBoundsActor))
	{
		RuntimeGridBoundsActor->Destroy();
	}
	RuntimeGridBoundsActor = nullptr;

	for (int32 index = RuntimeActors.Num() - 1; index >= 0; --index)
	{
		if (AActor* actor = RuntimeActors[index].Get())
		{
			actor->Destroy();
		}
	}

	RuntimeActors.Reset();
	RuntimeGroundRegions.Reset();
	RuntimePaths.Reset();
	RuntimeActorsById.Reset();
	FlushPersistentDebugLines(GetWorld());

	if (UWorld* world = GetWorld())
	{
		if (UEpisodePedestrianPlanSubsystem* pedestrianPlanSubsystem = world->GetSubsystem<UEpisodePedestrianPlanSubsystem>())
		{
			pedestrianPlanSubsystem->ClearPlans();
		}
	}

	if (actorCount > 0 || actorIdCount > 0 || groundRegionCount > 0 || pathCount > 0)
	{
		UE_LOG(
			LogEpisodeSimulation,
			 Warning,
			TEXT("Episode 런타임 정리 완료 | Actors: %d, ActorIds: %d, GroundRegions: %d, Paths: %d"),
			actorCount,
			actorIdCount,
			groundRegionCount,
			pathCount);
	}
}

bool UEpisodeSimulationSubsystem::RebuildDeliveryBotGridFromEpisodeGroundRegions(const FEpisodeSimulationSetupSpec& setupSpec)
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("DeliveryBot grid rebuild failed. GridSubsystem is invalid."));
		return false;
	}

	FBox2D xyBounds(ForceInit);
	double centerZ = 0.0;
	if (!TryBuildGroundRegionXYBounds(setupSpec.GroundRegions, xyBounds, centerZ))
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("DeliveryBot grid bounds build failed. No valid ground regions."));
		return false;
	}

	if (IsValid(RuntimeGridBoundsActor))
	{
		RuntimeGridBoundsActor->Destroy();
		RuntimeGridBoundsActor = nullptr;
	}

	ADeliveryBot_GridBoundsActor* gridBoundsActor = SpawnDeliveryBotGridBoundsActor(xyBounds, centerZ);
	if (!IsValid(gridBoundsActor))
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("DeliveryBot grid rebuild failed. Runtime GridBoundsActor spawn failed."));
		return false;
	}

	gridSubsystem->BuildGridFromBounds(gridBoundsActor);

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("DeliveryBot grid rebuilt from episode ground regions. BoundsActor: %s, HasGrid: %s, Cells: %d"),
		*gridBoundsActor->GetName(),
		gridSubsystem->HasBuiltGrid() ? TEXT("true") : TEXT("false"),
		gridSubsystem->GetGridCellCount()
	);

	return gridSubsystem->HasBuiltGrid();
}

bool UEpisodeSimulationSubsystem::TryBuildGroundRegionXYBounds(
	const TArray<FEpisodeGroundRegionSpec>& groundRegionSpecs,
	FBox2D& outXYBounds,
	double& outCenterZ) const
{
	outXYBounds = FBox2D(ForceInit);

	double zSum = 0.0;
	int32 validRegionCount = 0;

	for (const FEpisodeGroundRegionSpec& regionSpec : groundRegionSpecs)
	{
		if (regionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle)
			continue;

		ExpandXYBoundsWithGroundRegion(regionSpec, outXYBounds);
		zSum += regionSpec.Center.Z;
		++validRegionCount;
	}

	if (!outXYBounds.bIsValid || validRegionCount <= 0)
		return false;

	outCenterZ = zSum / static_cast<double>(validRegionCount);
	return true;
}

ADeliveryBot_GridBoundsActor* UEpisodeSimulationSubsystem::SpawnDeliveryBotGridBoundsActor(
	const FBox2D& xyBounds,
	double centerZ)
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return nullptr;

	TSubclassOf<ADeliveryBot_GridBoundsActor> spawnClass = GridBoundsActorClass;
	if (!spawnClass)
	{
		spawnClass = ADeliveryBot_GridBoundsActor::StaticClass();
	}

	ADeliveryBot_GridBoundsActor* gridBoundsActor = world->SpawnActorDeferred<ADeliveryBot_GridBoundsActor>(
		spawnClass,
		FTransform::Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(gridBoundsActor))
		return nullptr;

	ApplyXYBoundsToGridBoundsActor(gridBoundsActor, xyBounds, centerZ);

	UGameplayStatics::FinishSpawningActor(gridBoundsActor, gridBoundsActor->GetActorTransform());

	RuntimeGridBoundsActor = gridBoundsActor;
	return gridBoundsActor;
}

void UEpisodeSimulationSubsystem::ApplyXYBoundsToGridBoundsActor(
	ADeliveryBot_GridBoundsActor* gridBoundsActor,
	const FBox2D& xyBounds,
	double centerZ) const
{
	if (!IsValid(gridBoundsActor))
		return;

	UBoxComponent* boundsBox = gridBoundsActor->GetBoundsBox();
	if (!IsValid(boundsBox))
		return;

	const double safePadding = FMath::Max(static_cast<double>(DeliveryBotGridBoundsPaddingCm), 0.0);
	const FVector2D paddedMin = xyBounds.Min - FVector2D(safePadding, safePadding);
	const FVector2D paddedMax = xyBounds.Max + FVector2D(safePadding, safePadding);
	const FVector2D boundsCenter = (paddedMin + paddedMax) * 0.5;
	const FVector2D boundsSize = paddedMax - paddedMin;

	gridBoundsActor->SetActorLocation(FVector(boundsCenter.X, boundsCenter.Y, centerZ));
	gridBoundsActor->SetActorRotation(FRotator::ZeroRotator);
	gridBoundsActor->SetActorScale3D(FVector::OneVector);

	boundsBox->SetRelativeLocation(FVector::ZeroVector);
	boundsBox->SetRelativeRotation(FRotator::ZeroRotator);
	boundsBox->SetRelativeScale3D(FVector::OneVector);
	boundsBox->SetBoxExtent(FVector(boundsSize.X * 0.5, boundsSize.Y * 0.5, 100.0), true);
}

void UEpisodeSimulationSubsystem::ExpandXYBoundsWithGroundRegion(
	const FEpisodeGroundRegionSpec& regionSpec,
	FBox2D& inOutXYBounds)
{
	const FVector2D halfSize = regionSpec.Size * 0.5;
	const FTransform regionTransform(
		FRotator(0.0, regionSpec.YawDegrees, 0.0),
		regionSpec.Center
	);

	const FVector localCorners[4] =
	{
		FVector(-halfSize.X, -halfSize.Y, 0.0),
		FVector(halfSize.X, -halfSize.Y, 0.0),
		FVector(halfSize.X, halfSize.Y, 0.0),
		FVector(-halfSize.X, halfSize.Y, 0.0)
	};

	for (const FVector& localCorner : localCorners)
	{
		const FVector worldCorner = regionTransform.TransformPosition(localCorner);
		inOutXYBounds += FVector2D(worldCorner.X, worldCorner.Y);
	}
}

bool UEpisodeSimulationSubsystem::ValidateDeliveryBotGridLocation(
	const FString& robotInstanceId,
	const FString& locationLabel,
	const FVector& worldLocation) const
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem) || !gridSubsystem->HasBuiltGrid())
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("DeliveryBot grid validation failed. Grid is not built. Robot: %s"), *robotInstanceId);
		return false;
	}

	const FIntPoint gridIndex = gridSubsystem->GetGridIndexByWorldLocation(worldLocation);
	const FDeliveryBotGridCellInfo* cellInfo = gridSubsystem->FindCellInfoByGridIndex(gridIndex);

	if (!cellInfo || !gridSubsystem->IsWalkableGridIndex(gridIndex))
	{
		const FString worldLocationString = worldLocation.ToString();
		const FString gridIndexString = gridIndex.ToString();
		const FString sourceProfileName = cellInfo
			? cellInfo->SourceCollisionProfileName.ToString()
			: FString(TEXT("InvalidCell"));

		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("DeliveryBot %s location is not walkable. Robot: %s, World: %s, Grid: %s, Source: %s"),
			*locationLabel,
			*robotInstanceId,
			*worldLocationString,
			*gridIndexString,
			*sourceProfileName
		);

		return false;
	}

	return true;
}

bool UEpisodeSimulationSubsystem::ValidateDeliveryBotRouteOnGrid(
	const FEpisodePlaceableInstanceSpec& placeableSpec,
	const FDeliveryBotSetupInfo& setupInfo,
	bool bHasGoal,
	const FVector& goalLocation) const
{
	const bool bStartValid = ValidateDeliveryBotGridLocation(
		placeableSpec.InstanceId,
		TEXT("start"),
		setupInfo.LocationSetupInfo.StartLocationCm
	);

	const bool bGoalValid = !bHasGoal || ValidateDeliveryBotGridLocation(
		placeableSpec.InstanceId,
		TEXT("goal"),
		goalLocation
	);

	return bStartValid && bGoalValid;
}

bool UEpisodeSimulationSubsystem::SetupEpisodeWorld(const FEpisodeSimulationSetupSpec& setupSpec)
{
	ClearEpisode();

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("Episode 월드 설정 시작 | Episode: %s, GroundRegions: %d, Paths: %d, Placeables: %d, DynamicActors: %d, Events: %d"),
		*setupSpec.EpisodeId,
		setupSpec.GroundRegions.Num(),
		setupSpec.Paths.Num(),
		setupSpec.Placeables.Num(),
		setupSpec.DynamicActors.Num(),
		setupSpec.Events.Num());

	bool bAllSpawned{ true };

	for (const FEpisodeGroundRegionSpec& regionSpec : setupSpec.GroundRegions)
	{
		if (!SpawnGroundRegion(regionSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("지면 영역 '%s' 스폰 실패."), *regionSpec.RegionId);
			bAllSpawned = false;
		}
	}

	const bool bHasDeliveryBotPlaceable = setupSpec.Placeables.ContainsByPredicate(
		[](const FEpisodePlaceableInstanceSpec& placeableSpec)
		{
			return placeableSpec.Category == EEpisodeActorCategory::DeliveryBot
				|| placeableSpec.Category == EEpisodeActorCategory::RoadVehicle;
		});

	if (bHasDeliveryBotPlaceable && !RebuildDeliveryBotGridFromEpisodeGroundRegions(setupSpec))
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("DeliveryBot grid rebuild failed after ground regions were spawned."));
		bAllSpawned = false;
	}

	for (const FEpisodePathSpec& pathSpec : setupSpec.Paths)
	{
		if (pathSpec.PathType != EEpisodePathType::Spline)
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("경로 '%s'가 spline 타입이 아님."), *pathSpec.PathId);
		}

		if (!SpawnSplinePath(pathSpec.PathId, pathSpec.Points, pathSpec.bClosedLoop))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("경로 '%s' 스폰 실패."), *pathSpec.PathId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodePlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		if (!SpawnPlaceable(placeableSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("배치 액터 '%s' 스폰 실패."), *placeableSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	if (UWorld* world = GetWorld())
	{
		if (UEpisodePedestrianPlanSubsystem* pedestrianPlanSubsystem = world->GetSubsystem<UEpisodePedestrianPlanSubsystem>())
		{
			FEpisodePedestrianPlanBuildContext planBuildContext;
			BuildPedestrianPlanContext(setupSpec, planBuildContext);

			FEpisodePedestrianPlanBuildResult planBuildResult;
			if (!pedestrianPlanSubsystem->BuildPlans(setupSpec, planBuildContext, planBuildResult))
			{
				bAllSpawned = false;
				UE_LOG(
					LogEpisodeSimulation,
					Warning,
					TEXT("보행자 planned trajectory 생성 실패 | Episode: %s, Diagnostics: %d"),
					*setupSpec.EpisodeId,
					planBuildResult.Diagnostics.Num());
			}
		}
	}

	for (const FEpisodeDynamicActorSpec& dynamicActorSpec : setupSpec.DynamicActors)
	{
		if (!SpawnDynamicActor(dynamicActorSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("동적 액터 '%s' 스폰 실패."), *dynamicActorSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("Episode 월드 설정 완료 | Episode: %s, Success: %s, RuntimeActors: %d, ActorIds: %d, GroundRegions: %d, Paths: %d"),
		*setupSpec.EpisodeId,
		bAllSpawned ? TEXT("true") : TEXT("false"),
		RuntimeActors.Num(),
		RuntimeActorsById.Num(),
		RuntimeGroundRegions.Num(),
		RuntimePaths.Num());

	return bAllSpawned;
}

AActor* UEpisodeSimulationSubsystem::FindRuntimeActor(const FString& instanceId) const
{
	if (const TObjectPtr<AActor>* foundActor = RuntimeActorsById.Find(instanceId)) return foundActor->Get();

	return nullptr;
}

FEpisodeRuntimeContext UEpisodeSimulationSubsystem::BuildRuntimeContext(const FEpisodeSimulationSetupSpec& setupSpec) const
{
	FEpisodeRuntimeContext runtimeContext;
	runtimeContext.EpisodeId = setupSpec.EpisodeId;
	runtimeContext.SpecHash = setupSpec.SpecHash;

	for (AActor* actor : RuntimeActors)
	{
		if (IsValid(actor))
		{
			runtimeContext.RuntimeActors.Add(actor);
		}
	}

	for (const TPair<FString, TObjectPtr<AEpisodeGroundRegion>>& pair : RuntimeGroundRegions)
	{
		if (AActor* groundRegionActor = pair.Value.Get())
		{
			runtimeContext.GroundRegionActors.Add(groundRegionActor);
		}
	}

	for (const FEpisodePlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		AActor* runtimeActor = FindRuntimeActor(placeableSpec.InstanceId);
		if (!runtimeActor) continue;

		if (placeableSpec.Category == EEpisodeActorCategory::DeliveryBot && !runtimeContext.RobotActor)
		{
			runtimeContext.RobotInstanceId = placeableSpec.InstanceId;
			runtimeContext.RobotActor = runtimeActor;
			if (placeableSpec.DeliveryBot.bHasGoalLocation)
			{
				runtimeContext.GoalLocation = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm;
				runtimeContext.bHasGoalLocation = true;
			}
			else if (!placeableSpec.DeliveryBot.bHasStartLocation)
			{
				runtimeContext.bHasGoalLocation = GetVectorProperty(
					placeableSpec.Properties,
					TEXT("goal_cm"),
					runtimeContext.GoalLocation);
			}
			continue;
		}

		if (placeableSpec.Category == EEpisodeActorCategory::StaticObstacle)
		{
			runtimeContext.StaticObstacleActors.Add(runtimeActor);
		}
	}

	for (const FEpisodeDynamicActorSpec& dynamicActorSpec : setupSpec.DynamicActors)
	{
		AActor* runtimeActor = FindRuntimeActor(dynamicActorSpec.InstanceId);
		if (!runtimeActor) continue;

		if (dynamicActorSpec.Category == EEpisodeActorCategory::Pedestrian)
		{
			runtimeContext.PedestrianActors.Add(runtimeActor);
			runtimeContext.PedestrianInstanceIds.Add(dynamicActorSpec.InstanceId);
		}
	}

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("런타임 컨텍스트 생성 완료 | Episode: %s, SpecHash: %s, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
		*runtimeContext.EpisodeId,
		*runtimeContext.SpecHash,
		*runtimeContext.RobotInstanceId,
		runtimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		runtimeContext.RuntimeActors.Num(),
		runtimeContext.GroundRegionActors.Num(),
		runtimeContext.StaticObstacleActors.Num(),
		runtimeContext.PedestrianActors.Num());

	if (!IsValid(runtimeContext.RobotActor))
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("런타임 컨텍스트에 유효한 로봇 액터가 없음 | Episode: %s"), *runtimeContext.EpisodeId);
	}

	return runtimeContext;
}

AEpisodeSplinePath* UEpisodeSimulationSubsystem::SpawnSplinePath(const FString& pathId, const TArray<FVector>& points, bool bClosedLoop)
{
	UWorld* world = GetWorld();
	if (!world || pathId.IsEmpty() || points.Num() < 2) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeSplinePath* pathActor = world->SpawnActor<AEpisodeSplinePath>(
		AEpisodeSplinePath::StaticClass(),
		FTransform::Identity,
		spawnParams);
	if (!pathActor) return nullptr;

	pathActor->ConfigurePath(pathId, points, bClosedLoop);
	RuntimeActors.Add(pathActor);
	RuntimePaths.Add(pathId, pathActor);
	return pathActor;
}

AEpisodeSplinePath* UEpisodeSimulationSubsystem::FindSplinePath(const FString& pathId) const
{
	if (const TObjectPtr<AEpisodeSplinePath>* foundPath = RuntimePaths.Find(pathId)) return foundPath->Get();

	return nullptr;
}

AEpisodeGroundRegion* UEpisodeSimulationSubsystem::SpawnGroundRegion(const FEpisodeGroundRegionSpec& regionSpec)
{
	UWorld* world = GetWorld();
	if (!world || regionSpec.RegionId.IsEmpty() || regionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeGroundRegion* groundRegion = world->SpawnActor<AEpisodeGroundRegion>(
		AEpisodeGroundRegion::StaticClass(),
		FTransform::Identity,
		spawnParams);
	if (!groundRegion) return nullptr;

	groundRegion->ConfigureRegion(regionSpec);
	RuntimeActors.Add(groundRegion);
	RuntimeGroundRegions.Add(regionSpec.RegionId, groundRegion);
	return groundRegion;
}

void UEpisodeSimulationSubsystem::SpawnGroundRegions(const TArray<FEpisodeGroundRegionSpec>& regionSpecs)
{
	for (const FEpisodeGroundRegionSpec& regionSpec : regionSpecs)
	{
		SpawnGroundRegion(regionSpec);
	}
}

AEpisodeGroundRegion* UEpisodeSimulationSubsystem::FindGroundRegion(const FString& regionId) const
{
	if (const TObjectPtr<AEpisodeGroundRegion>* foundRegion = RuntimeGroundRegions.Find(regionId)) return foundRegion->Get();

	return nullptr;
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPedestrianOnPath(
	TSubclassOf<AEpisodePedestrian> inPedestrianClass,
	const FTransform& spawnTransform,
	AEpisodeSplinePath* splinePath,
	double speedCmPerSecond,
	double initialDistanceCm,
	bool bStartFollowing)
{
	UWorld* world = GetWorld();
	if (!world || !inPedestrianClass || !splinePath) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodePedestrian* pedestrian = world->SpawnActorDeferred<AEpisodePedestrian>(
		inPedestrianClass,
		spawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!pedestrian) return nullptr;

	if (UEpisodePathFollowerComponent* pathFollower = pedestrian->PathFollowerComponent)
	{
		pathFollower->bAutoStart = false;
		pathFollower->SpeedCmPerSecond = speedCmPerSecond;
		pathFollower->InitialDistanceCm = initialDistanceCm;
		pathFollower->CurrentDistanceCm = initialDistanceCm;
		pathFollower->SetSplinePath(splinePath);
	}

	UGameplayStatics::FinishSpawningActor(pedestrian, spawnTransform);
	SetActorReceivesDecals(pedestrian, false);
	RuntimeActors.Add(pedestrian);

	if (bStartFollowing && pedestrian->PathFollowerComponent)
	{
		pedestrian->PathFollowerComponent->StartFollowing();
	}

	return pedestrian;
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPedestrianOnPathId(
	TSubclassOf<AEpisodePedestrian> inPedestrianClass,
	const FTransform& spawnTransform,
	const FString& pathId,
	double speedCmPerSecond,
	double initialDistanceCm,
	bool bStartFollowing)
{
	return SpawnPedestrianOnPath(
		inPedestrianClass,
		spawnTransform,
		FindSplinePath(pathId),
		speedCmPerSecond,
		initialDistanceCm,
		bStartFollowing);
}

AActor* UEpisodeSimulationSubsystem::SpawnPlaceable(const FEpisodePlaceableInstanceSpec& placeableSpec)
{
	switch (placeableSpec.Category)
	{
	case EEpisodeActorCategory::StaticObstacle:
		return SpawnStaticObstacle(placeableSpec);
	case EEpisodeActorCategory::DeliveryBot:
	case EEpisodeActorCategory::RoadVehicle:
		return SpawnRobotActor(placeableSpec);
	default:
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("배치 액터 '%s'의 카테고리를 지원하지 않음."),
			*placeableSpec.InstanceId);
		return nullptr;
	}
}

AEpisodeStaticObstacle* UEpisodeSimulationSubsystem::SpawnStaticObstacle(const FEpisodePlaceableInstanceSpec& placeableSpec)
{
	UWorld* world = GetWorld();
	TSubclassOf<AEpisodeStaticObstacle> spawnClass = StaticObstacleClass;
	if (!spawnClass)
	{
		spawnClass = AEpisodeStaticObstacle::StaticClass();
	}
	if (!world || placeableSpec.InstanceId.IsEmpty() || placeableSpec.AssetId.IsEmpty() || !spawnClass) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeStaticObstacle* staticObstacle = world->SpawnActor<AEpisodeStaticObstacle>(
		spawnClass,
		placeableSpec.Transform,
		spawnParams);
	if (!staticObstacle) return nullptr;

	FEpisodeStaticObstaclePropEntry propEntry;
	if (!TryFindStaticObstacleProp(FName(*placeableSpec.AssetId), propEntry))
	{
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("정적 장애물 '%s'에 prop '%s' 적용 실패."),
			*placeableSpec.InstanceId,
			*placeableSpec.AssetId);
		staticObstacle->Destroy();
		return nullptr;
	}

	if (!staticObstacle->ApplyPropEntry(propEntry))
	{
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("?뺤쟻 ?μ븷臾?'%s'??prop '%s' entry ?곸슜 ?ㅽ뙣."),
			*placeableSpec.InstanceId,
			*placeableSpec.AssetId);
		staticObstacle->Destroy();
		return nullptr;
	}

	RegisterRuntimeActor(
		placeableSpec.InstanceId,
		placeableSpec.AssetId,
		placeableSpec.Category,
		EEpisodeMobilityMode::Static,
		staticObstacle);
	return staticObstacle;
}

bool UEpisodeSimulationSubsystem::TryFindStaticObstacleProp(
	FName propId,
	FEpisodeStaticObstaclePropEntry& outPropEntry) const
{
	if (propId.IsNone()) return false;

	const UEpisodeStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog))
	{
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("Episode static obstacle prop catalog is not configured or failed to load: %s"),
			*StaticObstaclePropCatalog.ToSoftObjectPath().ToString());
		return false;
	}

	return propCatalog->FindPropEntryById(propId, outPropEntry);
}

AActor* UEpisodeSimulationSubsystem::SpawnRobotActor(const FEpisodePlaceableInstanceSpec& placeableSpec)
{
	UWorld* world{ GetWorld() };

	if (!world || placeableSpec.InstanceId.IsEmpty() || !RobotActorClass) return nullptr;

	const FEpisodeDeliveryBotSpawnSpec& deliveryBotSpec = placeableSpec.DeliveryBot;
	const bool bUseLegacyPropertyFallback = !deliveryBotSpec.bHasStartLocation;
	const bool bSpawnOnly = bUseLegacyPropertyFallback
		? GetBoolProperty(placeableSpec.Properties, TEXT("spawn_only"), deliveryBotSpec.bSpawnOnly)
		: deliveryBotSpec.bSpawnOnly;

	FDeliveryBotSetupInfo setupInfo = deliveryBotSpec.SetupInfo;
	if (!deliveryBotSpec.bHasStartLocation)
	{
		setupInfo.LocationSetupInfo.StartLocationCm = placeableSpec.Transform.GetLocation();
		setupInfo.LocationSetupInfo.GoalLocationCm = setupInfo.LocationSetupInfo.StartLocationCm;
	}

	bool bRouteAutoStart = setupInfo.LocationSetupInfo.bAutoStartRoute;
	if (bUseLegacyPropertyFallback)
	{
		bRouteAutoStart = GetBoolProperty(placeableSpec.Properties, TEXT("route_auto_start"), bRouteAutoStart);
	}

	FVector goalLocation = setupInfo.LocationSetupInfo.GoalLocationCm;
	bool bHasGoal = deliveryBotSpec.bHasGoalLocation;
	if (!bHasGoal && bUseLegacyPropertyFallback)
	{
		bHasGoal = GetVectorProperty(placeableSpec.Properties, TEXT("goal_cm"), goalLocation);
		if (bHasGoal)
		{
			setupInfo.LocationSetupInfo.GoalLocationCm = goalLocation;
		}
	}

	if (!bHasGoal)
	{
		goalLocation = setupInfo.LocationSetupInfo.StartLocationCm;
	}

	const bool bRouteGridValid = ValidateDeliveryBotRouteOnGrid(
		placeableSpec,
		setupInfo,
		bHasGoal,
		goalLocation
	);

	if (!bRouteGridValid)
	{
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("DeliveryBot auto start disabled because route grid validation failed. Robot: %s"),
			*placeableSpec.InstanceId
		);
	}

	setupInfo.LocationSetupInfo.bAutoStartRoute = !bSpawnOnly && bRouteAutoStart && bHasGoal && bRouteGridValid;

	ADeliveryBot* robotActor{
		world->SpawnActorDeferred<ADeliveryBot>(
			RobotActorClass,
			placeableSpec.Transform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		)
	};

	if (!robotActor) return nullptr;

	robotActor->InitializeSetupInfo(setupInfo);

	UGameplayStatics::FinishSpawningActor(
		robotActor,
		placeableSpec.Transform
	);

	RegisterRuntimeActor(
		placeableSpec.InstanceId,
		placeableSpec.AssetId,
		placeableSpec.Category,
		EEpisodeMobilityMode::Moving,
		robotActor);

	if (!bSpawnOnly)
	{
		if (!bHasGoal)
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("로봇 '%s'에 이동 목표가 없어 경로 주입을 건너뜀."), *placeableSpec.InstanceId);
			return robotActor;
		}

		if (GoalPointClass)
		{
			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AActor* goalPointActor = world->SpawnActor<AActor>(GoalPointClass, FTransform(FRotator::ZeroRotator, goalLocation), spawnParams))
			{
				RuntimeActors.Add(goalPointActor);
			}
		}

		if (StartPointClass)
		{
			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AActor* startPointActor = world->SpawnActor<AActor>(StartPointClass, FTransform(placeableSpec.Transform), spawnParams))
			{
				RuntimeActors.Add(startPointActor);
			}
		}

	}

	return robotActor;
}

AActor* UEpisodeSimulationSubsystem::SpawnDynamicActor(const FEpisodeDynamicActorSpec& dynamicActorSpec)
{
	switch (dynamicActorSpec.Category)
	{
	case EEpisodeActorCategory::Pedestrian:
		return SpawnPedestrian(dynamicActorSpec);
	default:
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("동적 액터 '%s'의 카테고리를 지원하지 않음."),
			*dynamicActorSpec.InstanceId);
		return nullptr;
	}
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPedestrian(const FEpisodeDynamicActorSpec& dynamicActorSpec)
{
	const FString movementModel = GetStringProperty(dynamicActorSpec.Properties, TEXT("movement_model"), TEXT("spline_Relative"));
	if (movementModel.Equals(TEXT("planned_trajectory"), ESearchCase::IgnoreCase))
	{
		return SpawnPlannedPedestrian(dynamicActorSpec);
	}
	if (movementModel.Equals(TEXT("static_placement"), ESearchCase::IgnoreCase))
	{
		UWorld* world = GetWorld();
		if (!world || dynamicActorSpec.InstanceId.IsEmpty()) return nullptr;

		AEpisodePedestrian* pedestrian = world->SpawnActor<AEpisodePedestrian>(
			PedestrianClass ? PedestrianClass.Get() : AEpisodePedestrian::StaticClass(),
			dynamicActorSpec.InitialTransform);
		if (!pedestrian) return nullptr;

		if (pedestrian->PathFollowerComponent)
		{
			pedestrian->PathFollowerComponent->bAutoStart = false;
			pedestrian->PathFollowerComponent->StopFollowing();
		}
		if (pedestrian->PedestrianRuntimeComponent)
		{
			pedestrian->PedestrianRuntimeComponent->bAutoStart = false;
			pedestrian->PedestrianRuntimeComponent->StopFollowing();
		}

		RuntimeActors.Add(pedestrian);
		ConfigurePlaceableComponent(
			pedestrian->PlaceableComponent,
			dynamicActorSpec.InstanceId,
			dynamicActorSpec.AssetId,
			dynamicActorSpec.Category,
			EEpisodeMobilityMode::Static);
		RuntimeActorsById.Add(dynamicActorSpec.InstanceId, pedestrian);
		return pedestrian;
	}

	const double speedCmPerSecond = GetFloatProperty(dynamicActorSpec.Properties, TEXT("speed_cm_per_second"), 120.0);
	const double initialDistanceCm = GetFloatProperty(dynamicActorSpec.Properties, TEXT("initial_distance_cm"), 0.0);
	const bool bAutoStart = GetBoolProperty(dynamicActorSpec.Properties, TEXT("auto_start"), true);

	AEpisodePedestrian* pedestrian = SpawnPedestrianOnPathId(
		PedestrianClass ? PedestrianClass.Get() : AEpisodePedestrian::StaticClass(),
		dynamicActorSpec.InitialTransform,
		dynamicActorSpec.PathId,
		speedCmPerSecond,
		initialDistanceCm,
		bAutoStart);
	if (!pedestrian) return nullptr;

	ConfigurePlaceableComponent(
		pedestrian->PlaceableComponent,
		dynamicActorSpec.InstanceId,
		dynamicActorSpec.AssetId,
		dynamicActorSpec.Category,
		EEpisodeMobilityMode::Moving);
	RuntimeActorsById.Add(dynamicActorSpec.InstanceId, pedestrian);
	return pedestrian;
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPlannedPedestrian(const FEpisodeDynamicActorSpec& dynamicActorSpec)
{
	UWorld* world = GetWorld();
	if (!world || dynamicActorSpec.InstanceId.IsEmpty()) return nullptr;

	UEpisodePedestrianPlanSubsystem* pedestrianPlanSubsystem = world->GetSubsystem<UEpisodePedestrianPlanSubsystem>();
	if (!pedestrianPlanSubsystem)
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("보행자 plan subsystem이 없어 planned pedestrian '%s' 스폰 실패."), *dynamicActorSpec.InstanceId);
		return nullptr;
	}

	const FEpisodePedestrianPlan* plan = pedestrianPlanSubsystem->FindPlan(dynamicActorSpec.InstanceId);
	if (!plan)
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("planned pedestrian '%s'에 대응하는 plan이 없음."), *dynamicActorSpec.InstanceId);
		return nullptr;
	}

	const double speedCmPerSecond = GetFloatProperty(dynamicActorSpec.Properties, TEXT("speed_cm_per_second"), 120.0);
	const double initialDistanceCm = GetFloatProperty(dynamicActorSpec.Properties, TEXT("initial_distance_cm"), 0.0);
	const bool bAutoStart = GetBoolProperty(dynamicActorSpec.Properties, TEXT("auto_start"), true);

	FTransform spawnTransform = dynamicActorSpec.InitialTransform;
	if (plan->Points.Num() > 0)
	{
		spawnTransform.SetLocation(plan->Points[0].Location);
		if (!plan->Points[0].Direction.IsNearlyZero())
		{
			spawnTransform.SetRotation(plan->Points[0].Direction.Rotation().Quaternion());
		}
	}

	AActor* robotActor = FindRuntimeActorByCategory(EEpisodeActorCategory::DeliveryBot);
	if (!robotActor)
	{
		UE_LOG(
			LogEpisodeSimulation,
			Error,
			TEXT("planned pedestrian '%s' requires a DeliveryBot runtime actor."),
			*dynamicActorSpec.InstanceId);
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodePedestrian* pedestrian = world->SpawnActorDeferred<AEpisodePedestrian>(
		PedestrianClass ? PedestrianClass.Get() : AEpisodePedestrian::StaticClass(),
		spawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!pedestrian) return nullptr;

	if (pedestrian->PathFollowerComponent)
	{
		pedestrian->PathFollowerComponent->bAutoStart = false;
	}

	if (pedestrian->PedestrianRuntimeComponent)
	{
		pedestrian->PedestrianRuntimeComponent->ConfigurePlan(
			dynamicActorSpec.InstanceId,
			*plan,
			speedCmPerSecond,
			initialDistanceCm,
			bAutoStart);
		pedestrian->PedestrianRuntimeComponent->SetRobotActor(robotActor);
	}

	UGameplayStatics::FinishSpawningActor(pedestrian, spawnTransform);
	SetActorReceivesDecals(pedestrian, false);
	RuntimeActors.Add(pedestrian);

	ConfigurePlaceableComponent(
		pedestrian->PlaceableComponent,
		dynamicActorSpec.InstanceId,
		dynamicActorSpec.AssetId,
		dynamicActorSpec.Category,
		EEpisodeMobilityMode::Moving);
	RuntimeActorsById.Add(dynamicActorSpec.InstanceId, pedestrian);
	return pedestrian;
}

void UEpisodeSimulationSubsystem::RegisterRuntimeActor(
	const FString& instanceId,
	const FString& assetId,
	EEpisodeActorCategory category,
	EEpisodeMobilityMode mobilityMode,
	AActor* actor)
{
	if (!actor) return;

	RuntimeActors.Add(actor);
	RuntimeActorsById.Add(instanceId, actor);
	SetActorReceivesDecals(actor, false);
	ConfigurePlaceableComponent(
		actor->FindComponentByClass<UEpisodePlaceableComponent>(),
		instanceId,
		assetId,
		category,
		mobilityMode);
}

void UEpisodeSimulationSubsystem::ConfigurePlaceableComponent(
	UEpisodePlaceableComponent* placeableComponent,
	const FString& instanceId,
	const FString& assetId,
	EEpisodeActorCategory category,
	EEpisodeMobilityMode mobilityMode) const
{
	if (!placeableComponent) return;

	placeableComponent->InstanceId = instanceId;
	placeableComponent->AssetId = assetId;
	placeableComponent->Category = category;
	placeableComponent->MobilityMode = mobilityMode;
}

double UEpisodeSimulationSubsystem::GetFloatProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	double defaultValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue) return defaultValue;

	if (paramValue->Type == EEpisodeParamValueType::Float) return paramValue->FloatValue;

	if (paramValue->Type == EEpisodeParamValueType::Integer) return paramValue->IntegerValue;

	return defaultValue;
}

bool UEpisodeSimulationSubsystem::GetBoolProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	bool defaultValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::Bool) return defaultValue;

	return paramValue->BoolValue;
}

FString UEpisodeSimulationSubsystem::GetStringProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	const FString& defaultValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::String) return defaultValue;

	return paramValue->StringValue;
}

bool UEpisodeSimulationSubsystem::GetVectorProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	FVector& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::Vector) return false;

	outValue = paramValue->VectorValue;
	return true;
}

AActor* UEpisodeSimulationSubsystem::FindRuntimeActorByCategory(EEpisodeActorCategory category) const
{
	for (const TPair<FString, TObjectPtr<AActor>>& pair : RuntimeActorsById)
	{
		AActor* actor = pair.Value.Get();
		if (!IsValid(actor))
		{
			continue;
		}

		const UEpisodePlaceableComponent* placeableComponent = actor->FindComponentByClass<UEpisodePlaceableComponent>();
		if (!placeableComponent || placeableComponent->Category != category)
		{
			continue;
		}

		return actor;
	}

	return nullptr;
}

void UEpisodeSimulationSubsystem::BuildPedestrianPlanContext(
	const FEpisodeSimulationSetupSpec& setupSpec,
	FEpisodePedestrianPlanBuildContext& outBuildContext) const
{
	outBuildContext = FEpisodePedestrianPlanBuildContext{};
	outBuildContext.SourceSpecHash = setupSpec.SpecHash;
	outBuildContext.SemanticNavigationHash = TEXT("default");

	for (const FEpisodePlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		if (placeableSpec.Category != EEpisodeActorCategory::StaticObstacle)
		{
			continue;
		}

		AActor* obstacleActor = FindRuntimeActor(placeableSpec.InstanceId);
		if (!IsValid(obstacleActor))
		{
			continue;
		}

		FVector boundsOrigin = FVector::ZeroVector;
		FVector boundsExtent = FVector::ZeroVector;
		obstacleActor->GetActorBounds(true, boundsOrigin, boundsExtent);

		if (boundsExtent.IsNearlyZero())
		{
			boundsOrigin = obstacleActor->GetActorLocation();
			boundsExtent = FVector(50.0, 50.0, 100.0);
		}

		FEpisodePedestrianObstacleFootprint footprint;
		footprint.InstanceId = placeableSpec.InstanceId;
		footprint.AssetId = placeableSpec.AssetId;
		footprint.Center = boundsOrigin;
		footprint.Extent = boundsExtent;
		outBuildContext.StaticObstacleFootprints.Add(footprint);
	}
}

void UEpisodeSimulationSubsystem::SetActorReceivesDecals(AActor* actor, bool bReceivesDecals)
{
	if (!actor) return;

	TArray<UPrimitiveComponent*> primitiveComponents;
	actor->GetComponents<UPrimitiveComponent>(primitiveComponents);
	for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
	{
		if (primitiveComponent)
		{
			primitiveComponent->SetReceivesDecals(bReceivesDecals);
		}
	}
}
