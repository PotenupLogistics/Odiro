#include "Episode/EpisodeSimulationSubsystem.h"
#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePedestrianRuntimeComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/EpisodeEvaluationSubsystem.h"
#include "Episode/EpisodePedestrianPlanSubsystem.h"
#include "Shared/EpisodePedestrianPlanTypes.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "DeliveryBot/Actor/DeliveryBot_ChaosActor.h"
#include "Kismet/GameplayStatics.h"


DEFINE_LOG_CATEGORY_STATIC(LogEpisodeSimulation, Log, All);

UEpisodeSimulationSubsystem::UEpisodeSimulationSubsystem()
{
	StaticObstacleClass = AEpisodeStaticObstacle::StaticClass();

	static ConstructorHelpers::FClassFinder<ADeliveryBot_ChaosActor> robotBlueprintClass(
		TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot_ChaosMesh"));

	if (robotBlueprintClass.Succeeded())
	{
		RobotActorClass = robotBlueprintClass.Class;
	}
	else
	{
		RobotActorClass = ADeliveryBot_ChaosActor::StaticClass();
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

	if (!staticObstacle->ApplyDefaultPropById(FName(*placeableSpec.AssetId)))
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

	RegisterRuntimeActor(
		placeableSpec.InstanceId,
		placeableSpec.AssetId,
		placeableSpec.Category,
		EEpisodeMobilityMode::Static,
		staticObstacle);
	return staticObstacle;
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

	setupInfo.LocationSetupInfo.bAutoStartRoute = !bSpawnOnly && bRouteAutoStart && bHasGoal;

	ADeliveryBot_ChaosActor* robotActor{
		world->SpawnActorDeferred<ADeliveryBot_ChaosActor>(
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

	if (UEpisodeEvaluationSubsystem* evaluationSubsystem = world->GetSubsystem<UEpisodeEvaluationSubsystem>())
	{
		robotActor->OnDeliveryBotSimulationFailed.RemoveDynamic(
			evaluationSubsystem,
			&UEpisodeEvaluationSubsystem::HandleDeliveryBotSimulationFailed);
		robotActor->OnDeliveryBotSimulationFailed.AddDynamic(
			evaluationSubsystem,
			&UEpisodeEvaluationSubsystem::HandleDeliveryBotSimulationFailed);
	}

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
