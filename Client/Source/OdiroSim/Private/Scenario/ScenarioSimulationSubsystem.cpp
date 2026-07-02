#include "Scenario/ScenarioSimulationSubsystem.h"
#include "Shared/Actors/ScenarioMapBounds.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Actors/ScenarioSplinePath.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Components/ScenarioPathFollowerComponent.h"
#include "Scenario/Components/ScenarioPedestrianRuntimeComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/ScenarioCityBlockMaterializer.h"
#include "Scenario/ScenarioPedestrianPlanSubsystem.h"
#include "Scenario/Widget/ScenarioEditorRouteMarkerOverlayWidget.h"
#include "Shared/ScenarioPedestrianPlanTypes.h"
#include "Shared/ScenarioViewportPresentation.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioSimulation, Log, All);

UScenarioSimulationSubsystem::UScenarioSimulationSubsystem()
{
	StaticObstacleClass = AScenarioStaticObstacle::StaticClass();
	StaticObstaclePropCatalog = UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference();
	CityBlockCatalog = UScenarioCityBlockCatalog::MakeDefaultCatalogReference();

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
	static ConstructorHelpers::FClassFinder<AScenarioPedestrian> pedestrianBlueprintClass(TEXT("/Game/Blueprints/Scenario/BP_ScenarioPedestrian"));
	if (pedestrianBlueprintClass.Succeeded())
	{
		PedestrianClass = pedestrianBlueprintClass.Class;
	}
	else
	{
		PedestrianClass = AScenarioPedestrian::StaticClass();
	}
}

// 현재 Scenario가 소유한 runtime actor, Grid와 보행자 계획을 정리한다.
void UScenarioSimulationSubsystem::ClearScenario()
{
	// 정리 결과 로그에 사용할 현재 runtime object 수를 보관한다.
	const int32 actorCount = RuntimeActors.Num();
	const int32 cityBlockActorCount = RuntimeCityBlockActors.Num();
	const int32 actorIdCount = RuntimeActorsById.Num();
	const int32 groundRegionCount = RuntimeGroundRegions.Num();
	const int32 corridorCount = RuntimeCorridors.Num();
	const int32 pathCount = RuntimePaths.Num();

	RemoveRuntimeRouteMarkerOverlayWidget();
	RuntimeRouteMarkerOverlayItems.Reset();

	// 이전 Scenario가 생성한 Runtime GridBoundsActor를 제거한다.
	if (IsValid(RuntimeGridBoundsActor))
	{
		RuntimeGridBoundsActor->Destroy();
	}
	RuntimeGridBoundsActor = nullptr;

	if (IsValid(RuntimeGreyBackgroundPostProcessVolume))
	{
		RuntimeGreyBackgroundPostProcessVolume->Destroy();
	}
	RuntimeGreyBackgroundPostProcessVolume = nullptr;

	// CityBuildings visual actors are cleaned separately because they are intentionally excluded from RuntimeActors.
	FScenarioCityBlockMaterializer::DestroySpawnedActors(RuntimeCityBlockActors);

	// SimulationSubsystem이 소유한 모든 runtime actor를 제거한다.
	for (int32 index = RuntimeActors.Num() - 1; index >= 0; --index)
	{
		if (AActor* actor = RuntimeActors[index].Get())
		{
			actor->Destroy();
		}
	}

	// Runtime actor와 lookup collection을 초기화한다.
	RuntimeActors.Reset();
	RuntimeGroundRegions.Reset();
	RuntimeCorridors.Reset();
	RuntimePaths.Reset();
	RuntimeActorsById.Reset();

	// 이전 Scenario가 남긴 persistent debug drawing을 제거한다.
	FlushPersistentDebugLines(GetWorld());

	// 이전 Scenario가 생성한 보행자 계획을 제거한다.
	if (UWorld* world = GetWorld())
	{
		if (UScenarioPedestrianPlanSubsystem* pedestrianPlanSubsystem =
			world->GetSubsystem<UScenarioPedestrianPlanSubsystem>())
		{
			pedestrianPlanSubsystem->ClearPlans();
		}
	}

	// 실제로 정리한 runtime object가 있을 때만 cleanup 결과를 기록한다.
	if (actorCount > 0
		|| cityBlockActorCount > 0
		|| actorIdCount > 0
		|| groundRegionCount > 0
		|| corridorCount > 0
		|| pathCount > 0)
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT(
				"Scenario runtime cleanup complete | "
				"Actors: %d, CityBlockActors: %d, ActorIds: %d, GroundRegions: %d, "
				"Corridors: %d, Paths: %d"),
			actorCount,
			cityBlockActorCount,
			actorIdCount,
			groundRegionCount,
			corridorCount,
			pathCount);
	}
}

// 시나리오 Surface와 DeliveryBot route에서 최종 Bounds를 계산해 runtime Grid를 다시 만든다.
bool UScenarioSimulationSubsystem::RebuildDeliveryBotGridFromScenarioSurfaces(
	const FScenarioSimulationSetupSpec& setupSpec)
{
	// Grid를 생성할 World와 GridSubsystem을 검증한다.
	UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		return false;
	}

	UDeliveryBot_GridSubsystem* gridSubsystem =
		world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("DeliveryBot grid rebuild failed. GridSubsystem is invalid."));
		return false;
	}

	// Spawn된 Surface actor를 우선 사용하고 실패하면 GroundRegion spec으로 fallback한다.
	FBox2D surfaceXYBounds(ForceInit);
	double surfaceCenterZ = 0.0;

	if (!TryBuildRuntimeSurfaceXYBounds(
			surfaceXYBounds,
			surfaceCenterZ)
		&& !TryBuildGroundRegionXYBounds(
			setupSpec.GroundRegions,
			surfaceXYBounds,
			surfaceCenterZ))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("DeliveryBot grid bounds build failed. No valid scenario surfaces."));
		return false;
	}

	// Surface Bounds에 DeliveryBot Start, Goal과 Grid Padding을 적용한다.
	FScenarioMapBounds mapBounds;
	if (!FScenarioMapBoundsResolver::TryResolveFromSurfaceBounds(
			surfaceXYBounds,
			surfaceCenterZ,
			setupSpec.Placeables,
			DeliveryBotGridBoundsPaddingCm,
			mapBounds))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("DeliveryBot grid bounds resolve failed."));
		return false;
	}

	// Grid 생성에 사용할 Bounds를 기록한다.
	UE_LOG(
		LogScenarioSimulation,
		Log,
		TEXT(
			"DeliveryBot grid bounds resolved from scenario surfaces, "
			"robot route anchors, and padding. Min: %s, Max: %s, CenterZ: %.2f"),
		*mapBounds.XYBounds.Min.ToString(),
		*mapBounds.XYBounds.Max.ToString(),
		mapBounds.CenterZ);

	// 이전 Scenario가 생성한 GridBoundsActor를 제거한다.
	if (IsValid(RuntimeGridBoundsActor))
	{
		RuntimeGridBoundsActor->Destroy();
		RuntimeGridBoundsActor = nullptr;
	}

	// 최종 Map Bounds를 사용하는 새 GridBoundsActor를 생성한다.
	ADeliveryBot_GridBoundsActor* gridBoundsActor =
		SpawnDeliveryBotGridBoundsActor(mapBounds);
	if (!IsValid(gridBoundsActor))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT(
				"DeliveryBot grid rebuild failed. "
				"Runtime GridBoundsActor spawn failed."));
		return false;
	}

	// 생성된 GridBoundsActor를 기준으로 실제 Navigation Grid를 만든다.
	gridSubsystem->BuildGridFromBounds(gridBoundsActor);
	const bool bGridBuilt = gridSubsystem->HasBuiltGrid();

	// 최종 Grid 생성 결과와 cell 수를 기록한다.
	UE_LOG(
		LogScenarioSimulation,
		Log,
		TEXT(
			"DeliveryBot grid rebuilt from episode scenario surfaces. "
			"BoundsActor: %s, HasGrid: %s, Cells: %d"),
		*gridBoundsActor->GetName(),
		bGridBuilt ? TEXT("true") : TEXT("false"),
		gridSubsystem->GetGridCellCount());

	return bGridBuilt;
}

bool UScenarioSimulationSubsystem::TryBuildGroundRegionXYBounds(
	const TArray<FScenarioGroundRegionSpec>& groundRegionSpecs,
	FBox2D& outXYBounds,
	double& outCenterZ) const
{
	outXYBounds = FBox2D(ForceInit);

	double zSum = 0.0;
	int32 validRegionCount = 0;

	for (const FScenarioGroundRegionSpec& regionSpec : groundRegionSpecs)
	{
		if (regionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon
			&& regionSpec.PolygonVertices.Num() < 3)
		{
			continue;
		}

		ExpandXYBoundsWithGroundRegion(regionSpec, outXYBounds);
		zSum += regionSpec.Center.Z;
		++validRegionCount;
	}

	if (!outXYBounds.bIsValid || validRegionCount <= 0)
		return false;

	outCenterZ = zSum / static_cast<double>(validRegionCount);
	return true;
}

// Spawn된 runtime surface actor에서 Grid/Preview 공용 XY 영역과 평균 중심 Z를 계산한다.
bool UScenarioSimulationSubsystem::TryBuildRuntimeSurfaceXYBounds(
	FBox2D& outXYBounds,
	double& outCenterZ) const
{
	outXYBounds = FBox2D(ForceInit);
	outCenterZ = 0.0;

	TArray<AActor*> surfaceActors;
	surfaceActors.Reserve(
		RuntimeCorridors.Num()
		+ RuntimeGroundRegions.Num());

	for (const TPair<FString, TObjectPtr<AScenarioCorridorRuntimeActor>>& pair : RuntimeCorridors)
	{
		surfaceActors.Add(pair.Value.Get());
	}

	for (const TPair<FString, TObjectPtr<AScenarioGroundRegion>>& pair : RuntimeGroundRegions)
	{
		surfaceActors.Add(pair.Value.Get());
	}

	const TArray<FScenarioPlaceableInstanceSpec> emptyPlaceables;

	FScenarioMapBounds resolvedBounds;
	if (!FScenarioMapBoundsResolver::TryResolve(
			surfaceActors,
			emptyPlaceables,
			0.0,
			resolvedBounds))
	{
		return false;
	}

	outXYBounds = resolvedBounds.XYBounds;
	outCenterZ = resolvedBounds.CenterZ;
	return true;
}

// 최종 Map Bounds를 사용하는 runtime DeliveryBot GridBoundsActor를 생성한다.
ADeliveryBot_GridBoundsActor*
UScenarioSimulationSubsystem::SpawnDeliveryBotGridBoundsActor(
	const FScenarioMapBounds& mapBounds)
{
	// GridActor 생성 전에 최종 Map Bounds를 검증한다.
	if (!mapBounds.IsValid())
	{
		return nullptr;
	}

	// Actor를 생성할 World를 검증한다.
	UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		return nullptr;
	}

	// 설정된 GridBoundsActor class를 사용하고 없으면 기본 C++ class를 사용한다.
	TSubclassOf<ADeliveryBot_GridBoundsActor> spawnClass =
		GridBoundsActorClass;
	if (!spawnClass)
	{
		spawnClass = ADeliveryBot_GridBoundsActor::StaticClass();
	}

	// 최종 Map Bounds 중앙에 Actor를 배치할 spawn transform을 계산한다.
	const FVector2D boundsCenter =
		mapBounds.XYBounds.GetCenter();

	const FTransform spawnTransform(
		FRotator::ZeroRotator,
		FVector(
			boundsCenter.X,
			boundsCenter.Y,
			mapBounds.CenterZ),
		FVector::OneVector);

	// BeginPlay 전 설정을 적용할 수 있도록 GridBoundsActor를 지연 생성한다.
	ADeliveryBot_GridBoundsActor* gridBoundsActor =
		world->SpawnActorDeferred<ADeliveryBot_GridBoundsActor>(
			spawnClass,
			spawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(gridBoundsActor))
	{
		return nullptr;
	}

	// ScenarioSimulationSubsystem이 Grid 생성을 소유하도록 BeginPlay 자동 생성을 막는다.
	gridBoundsActor->SetBuildGridOnBeginPlay(false);

	// Actor 생성을 완료하고 최종 Bounds 크기를 적용한다.
	UGameplayStatics::FinishSpawningActor(
		gridBoundsActor,
		spawnTransform);

	ApplyMapBoundsToGridBoundsActor(
		gridBoundsActor,
		mapBounds);

	// 현재 Scenario가 소유한 Runtime GridBoundsActor를 보관한다.
	RuntimeGridBoundsActor = gridBoundsActor;
	return gridBoundsActor;
}

// 최종 Map Bounds의 중심과 크기를 GridBoundsActor에 적용한다.
void UScenarioSimulationSubsystem::ApplyMapBoundsToGridBoundsActor(
	ADeliveryBot_GridBoundsActor* gridBoundsActor,
	const FScenarioMapBounds& mapBounds) const
{
	// 적용 대상 Actor와 Map Bounds를 검증한다.
	if (!IsValid(gridBoundsActor) || !mapBounds.IsValid())
	{
		return;
	}

	// Grid 영역을 표현할 BoxComponent를 검증한다.
	UBoxComponent* boundsBox =
		gridBoundsActor->GetBoundsBox();
	if (!IsValid(boundsBox))
	{
		return;
	}

	// 최종 Bounds에서 Actor 중심과 Box 전체 크기를 계산한다.
	const FVector2D boundsCenter =
		mapBounds.XYBounds.GetCenter();
	const FVector2D boundsSize =
		mapBounds.XYBounds.GetSize();

	// GridBoundsActor를 최종 Map Bounds 중앙에 배치한다.
	gridBoundsActor->SetActorLocation(
		FVector(
			boundsCenter.X,
			boundsCenter.Y,
			mapBounds.CenterZ));
	gridBoundsActor->SetActorRotation(
		FRotator::ZeroRotator);
	gridBoundsActor->SetActorScale3D(
		FVector::OneVector);

	// BoxComponent가 별도 child인 경우 상대 transform을 초기화한다.
	if (boundsBox != gridBoundsActor->GetRootComponent())
	{
		boundsBox->SetRelativeLocation(
			FVector::ZeroVector);
		boundsBox->SetRelativeRotation(
			FRotator::ZeroRotator);
		boundsBox->SetRelativeScale3D(
			FVector::OneVector);
	}

	// 최종 Map Bounds 크기를 Grid Box의 half extent로 적용한다.
	boundsBox->SetBoxExtent(
		FVector(
			boundsSize.X * 0.5,
			boundsSize.Y * 0.5,
			100.0),
		true);
	boundsBox->UpdateBounds();
}

void UScenarioSimulationSubsystem::ExpandXYBoundsWithGroundRegion(
	const FScenarioGroundRegionSpec& regionSpec,
	FBox2D& inOutXYBounds)
{
	const FTransform regionTransform(
		FRotator(0.0, regionSpec.YawDegrees, 0.0),
		regionSpec.Center
	);

	if (regionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon)
	{
		for (const FVector2D& localVertex : regionSpec.PolygonVertices)
		{
			const FVector worldCorner = regionTransform.TransformPosition(FVector(localVertex.X, localVertex.Y, 0.0));
			inOutXYBounds += FVector2D(worldCorner.X, worldCorner.Y);
		}
		return;
	}

	const FVector2D halfSize = regionSpec.Size * 0.5;

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

bool UScenarioSimulationSubsystem::ResolveDeliveryBotGridLocation(
	const FString& robotInstanceId,
	const FString& locationLabel,
	FVector& inOutWorldLocation) const
{
	UWorld* world = GetWorld();
	if (!IsValid(world)) return false;

	const UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem) || !gridSubsystem->HasBuiltGrid()) return false;

	const FIntPoint gridIndex = gridSubsystem->GetGridIndexByWorldLocation(inOutWorldLocation);
	const FDeliveryBotGridCellInfo* cellInfo = gridSubsystem->FindCellInfoByGridIndex(gridIndex);

	if (cellInfo && gridSubsystem->IsWalkableGridIndex(gridIndex))
	{
		inOutWorldLocation = gridSubsystem->GetWorldLocationByGridIndex(gridIndex);
		return true;
	}

	FVector snappedLocation = FVector::ZeroVector;
	if (gridSubsystem->GetNearestWalkableWorldLocation(inOutWorldLocation, 96, snappedLocation))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("DeliveryBot %s location snapped to nearest walkable grid cell. Robot: %s, From: %s, To: %s, OriginalGrid: %s"),
			*locationLabel,
			*robotInstanceId,
			*inOutWorldLocation.ToString(),
			*snappedLocation.ToString(),
			*gridIndex.ToString());
		inOutWorldLocation = snappedLocation;
		return true;
	}

	{
		const FString worldLocationString = inOutWorldLocation.ToString();
		const FString gridIndexString = gridIndex.ToString();
		const FString sourceProfileName = cellInfo
			? cellInfo->SourceCollisionProfileName.ToString()
			: FString(TEXT("InvalidCell"));

		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("DeliveryBot %s location is not walkable. Robot: %s, World: %s, Grid: %s, Source: %s"),
			*locationLabel,
			*robotInstanceId,
			*worldLocationString,
			*gridIndexString,
			*sourceProfileName
		);

	}

	return false;
}

bool UScenarioSimulationSubsystem::ValidateDeliveryBotRouteOnGrid(
	const FScenarioPlaceableInstanceSpec& placeableSpec,
	FDeliveryBotSetupInfo& setupInfo,
	bool bHasGoal,
	FVector& inOutGoalLocation) const
{
	FVector startLocation = setupInfo.LocationSetupInfo.StartLocationCm;
	const bool bStartValid = ResolveDeliveryBotGridLocation(
		placeableSpec.InstanceId,
		TEXT("start"),
		startLocation
	);
	if (bStartValid)
	{
		setupInfo.LocationSetupInfo.StartLocationCm = startLocation;
	}

	const bool bGoalValid = !bHasGoal || ResolveDeliveryBotGridLocation(
		placeableSpec.InstanceId,
		TEXT("goal"),
		inOutGoalLocation
	);
	if (bGoalValid && bHasGoal)
	{
		setupInfo.LocationSetupInfo.GoalLocationCm = inOutGoalLocation;
	}

	return bStartValid && bGoalValid;
}

bool UScenarioSimulationSubsystem::SetupScenarioWorld(const FScenarioSimulationSetupSpec& setupSpec)
{
	ClearScenario();
	ApplyRuntimeViewportPresentation();

	UE_LOG(
		LogScenarioSimulation,
		Log,
		TEXT("Scenario world setup started | Scenario: %s, Corridors: %d, GroundRegions: %d, Paths: %d, Placeables: %d, DynamicActors: %d, Events: %d"),
		*setupSpec.EpisodeId,
		setupSpec.Corridors.Num(),
		setupSpec.GroundRegions.Num(),
		setupSpec.Paths.Num(),
		setupSpec.Placeables.Num(),
		setupSpec.DynamicActors.Num(),
		setupSpec.Events.Num());

	bool bAllSpawned{ true };

	for (const FScenarioRuntimeCorridorSpec& corridorSpec : setupSpec.Corridors)
	{
		if (!SpawnCorridor(corridorSpec))
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("Runtime corridor spawn failed. CorridorId: %s"), *corridorSpec.CorridorId);
			bAllSpawned = false;
		}
	}

	for (const FScenarioGroundRegionSpec& regionSpec : setupSpec.GroundRegions)
	{
		if (!SpawnGroundRegion(regionSpec))
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("지면 영역 '%s' 스폰 실패."), *regionSpec.RegionId);
			bAllSpawned = false;
		}
	}

	for (const FScenarioPlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		if (placeableSpec.Category != EScenarioActorCategory::StaticObstacle)
		{
			continue;
		}

		if (!SpawnStaticObstacle(placeableSpec))
		{
			UE_LOG(
				LogScenarioSimulation,
				Warning,
				TEXT("Static obstacle placeable spawn failed before DeliveryBot grid rebuild. InstanceId: %s"),
				*placeableSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	const UScenarioCityBlockCatalog* cityBlockCatalog = CityBlockCatalog.LoadSynchronous();
	FScenarioCityBlockMaterializationOptions cityBlockOptions;
	cityBlockOptions.LogContext = TEXT("ScenarioSimulation");
	cityBlockOptions.CatalogDebugName = CityBlockCatalog.ToSoftObjectPath().ToString();
	cityBlockOptions.bCreateBuildingCollisionProxies = true;
	const FScenarioCityBlockMaterializationResult cityBlockResult =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			GetWorld(),
			cityBlockCatalog,
			setupSpec.GroundRegions,
			RuntimeCityBlockActors,
			cityBlockOptions);
	const int32 cityBlockActorCount = cityBlockResult.SpawnedActorCount;

	const bool bHasDeliveryBotPlaceable = setupSpec.Placeables.ContainsByPredicate(
		[](const FScenarioPlaceableInstanceSpec& placeableSpec)
		{
			return placeableSpec.Category == EScenarioActorCategory::DeliveryBot;
		});

	if (bHasDeliveryBotPlaceable && !RebuildDeliveryBotGridFromScenarioSurfaces(setupSpec))
	{
		UE_LOG(LogScenarioSimulation, Warning, TEXT("DeliveryBot grid rebuild failed after scenario surfaces were spawned."));
		bAllSpawned = false;
	}

	for (const FScenarioPathSpec& pathSpec : setupSpec.Paths)
	{
		if (pathSpec.PathType != EScenarioPathType::Spline)
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("경로 '%s'가 spline 타입이 아님."), *pathSpec.PathId);
		}

		if (!SpawnSplinePath(pathSpec.PathId, pathSpec.Points, pathSpec.bClosedLoop))
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("경로 '%s' 스폰 실패."), *pathSpec.PathId);
			bAllSpawned = false;
		}
	}

	for (const FScenarioPlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		if (placeableSpec.Category == EScenarioActorCategory::StaticObstacle)
		{
			continue;
		}

		if (!SpawnPlaceable(placeableSpec))
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("배치 액터 '%s' 스폰 실패."), *placeableSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	if (UWorld* world = GetWorld())
	{
		if (UScenarioPedestrianPlanSubsystem* pedestrianPlanSubsystem = world->GetSubsystem<UScenarioPedestrianPlanSubsystem>())
		{
			FScenarioPedestrianPlanBuildContext planBuildContext;
			BuildPedestrianPlanContext(setupSpec, planBuildContext);

			FScenarioPedestrianPlanBuildResult planBuildResult;
			if (!pedestrianPlanSubsystem->BuildPlans(setupSpec, planBuildContext, planBuildResult))
			{
				bAllSpawned = false;
				UE_LOG(
					LogScenarioSimulation,
					Warning,
					TEXT("보행자 planned trajectory 생성 실패 | Scenario: %s, Diagnostics: %d"),
					*setupSpec.EpisodeId,
					planBuildResult.Diagnostics.Num());
			}
		}
	}

	for (const FScenarioDynamicActorSpec& dynamicActorSpec : setupSpec.DynamicActors)
	{
		if (!SpawnDynamicActor(dynamicActorSpec))
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("동적 액터 '%s' 스폰 실패."), *dynamicActorSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	UE_LOG(
		LogScenarioSimulation,
		Log,
		TEXT("Scenario world setup complete | Scenario: %s, Success: %s, RuntimeActors: %d, CityBlockActors: %d, ActorIds: %d, GroundRegions: %d, Corridors: %d, Paths: %d"),
		*setupSpec.EpisodeId,
		bAllSpawned ? TEXT("true") : TEXT("false"),
		RuntimeActors.Num(),
		cityBlockActorCount,
		RuntimeActorsById.Num(),
		RuntimeGroundRegions.Num(),
		RuntimeCorridors.Num(),
		RuntimePaths.Num());

	ShowRuntimeRouteMarkerOverlayWidget();

	return bAllSpawned;
}

// 전달받은 Surface Actor와 Placeable을 Grid와 동일한 Padding으로 계산한다.
bool UScenarioSimulationSubsystem::TryResolveScenarioMapBounds(
	const TArray<AActor*>& surfaceActors,
	const TArray<FScenarioPlaceableInstanceSpec>& placeables,
	FScenarioMapBounds& outBounds) const
{
	// 실패 시 이전 결과가 사용되지 않도록 초기화한다.
	outBounds = FScenarioMapBounds{};

	// Grid와 동일한 Resolver와 Padding 설정으로 최종 영역을 계산한다.
	return FScenarioMapBoundsResolver::TryResolve(
		surfaceActors,
		placeables,
		DeliveryBotGridBoundsPaddingCm,
		outBounds);
}

void UScenarioSimulationSubsystem::GetRobotRouteMarkerOverlayItems(
	TArray<FScenarioEditorRouteMarkerOverlayItem>& outItems) const
{
	outItems = RuntimeRouteMarkerOverlayItems;
}

AActor* UScenarioSimulationSubsystem::FindRuntimeActor(const FString& instanceId) const
{
	if (const TObjectPtr<AActor>* foundActor = RuntimeActorsById.Find(instanceId)) return foundActor->Get();

	return nullptr;
}

FScenarioRuntimeContext UScenarioSimulationSubsystem::BuildRuntimeContext(const FScenarioSimulationSetupSpec& setupSpec) const
{
	FScenarioRuntimeContext runtimeContext;
	runtimeContext.EpisodeId = setupSpec.EpisodeId;
	runtimeContext.SpecHash = setupSpec.SpecHash;

	for (AActor* actor : RuntimeActors)
	{
		if (IsValid(actor))
		{
			runtimeContext.RuntimeActors.Add(actor);
		}
	}

	for (const TPair<FString, TObjectPtr<AScenarioGroundRegion>>& pair : RuntimeGroundRegions)
	{
		if (AActor* groundRegionActor = pair.Value.Get())
		{
			runtimeContext.GroundRegionActors.Add(groundRegionActor);
		}
	}

	for (const TPair<FString, TObjectPtr<AScenarioCorridorRuntimeActor>>& pair : RuntimeCorridors)
	{
		if (AActor* corridorActor = pair.Value.Get())
		{
			runtimeContext.CorridorActors.Add(corridorActor);
		}
	}

	for (const FScenarioPlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		AActor* runtimeActor = FindRuntimeActor(placeableSpec.InstanceId);
		if (!runtimeActor) continue;

		if (placeableSpec.Category == EScenarioActorCategory::DeliveryBot && !runtimeContext.RobotActor)
		{
			runtimeContext.RobotInstanceId = placeableSpec.InstanceId;
			runtimeContext.RobotActor = runtimeActor;
			if (placeableSpec.DeliveryBot.bHasGoalLocation)
			{
				runtimeContext.GoalLocation = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm;
				runtimeContext.bHasGoalLocation = true;
			}
			continue;
		}

		if (placeableSpec.Category == EScenarioActorCategory::StaticObstacle)
		{
			runtimeContext.StaticObstacleActors.Add(runtimeActor);
		}
	}

	for (const FScenarioDynamicActorSpec& dynamicActorSpec : setupSpec.DynamicActors)
	{
		AActor* runtimeActor = FindRuntimeActor(dynamicActorSpec.InstanceId);
		if (!runtimeActor) continue;

		if (dynamicActorSpec.Category == EScenarioActorCategory::Pedestrian)
		{
			runtimeContext.PedestrianActors.Add(runtimeActor);
			runtimeContext.PedestrianInstanceIds.Add(dynamicActorSpec.InstanceId);
		}
	}

	UE_LOG(
		LogScenarioSimulation,
		Log,
		TEXT("런타임 컨텍스트 생성 완료 | Scenario: %s, SpecHash: %s, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
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
		UE_LOG(LogScenarioSimulation, Warning, TEXT("런타임 컨텍스트에 유효한 로봇 액터가 없음 | Scenario: %s"), *runtimeContext.EpisodeId);
	}

	return runtimeContext;
}

AScenarioSplinePath* UScenarioSimulationSubsystem::SpawnSplinePath(const FString& pathId, const TArray<FVector>& points, bool bClosedLoop)
{
	UWorld* world = GetWorld();
	if (!world || pathId.IsEmpty() || points.Num() < 2) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioSplinePath* pathActor = world->SpawnActor<AScenarioSplinePath>(
		AScenarioSplinePath::StaticClass(),
		FTransform::Identity,
		spawnParams);
	if (!pathActor) return nullptr;

	pathActor->ConfigurePath(pathId, points, bClosedLoop);
	RuntimeActors.Add(pathActor);
	RuntimePaths.Add(pathId, pathActor);
	return pathActor;
}

AScenarioSplinePath* UScenarioSimulationSubsystem::FindSplinePath(const FString& pathId) const
{
	if (const TObjectPtr<AScenarioSplinePath>* foundPath = RuntimePaths.Find(pathId)) return foundPath->Get();

	return nullptr;
}

AScenarioCorridorRuntimeActor* UScenarioSimulationSubsystem::SpawnCorridor(const FScenarioRuntimeCorridorSpec& corridorSpec)
{
	UWorld* world = GetWorld();
	if (!world || corridorSpec.CorridorId.IsEmpty() || corridorSpec.PointsMeters.Num() < 2)
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioCorridorRuntimeActor* corridorActor = world->SpawnActor<AScenarioCorridorRuntimeActor>(
		AScenarioCorridorRuntimeActor::StaticClass(),
		FTransform::Identity,
		spawnParams);
	if (!corridorActor)
	{
		return nullptr;
	}

	corridorActor->ConfigureCorridor(corridorSpec);
	RuntimeActors.Add(corridorActor);
	RuntimeCorridors.Add(corridorSpec.CorridorId, corridorActor);
	return corridorActor;
}

AScenarioGroundRegion* UScenarioSimulationSubsystem::SpawnGroundRegion(const FScenarioGroundRegionSpec& regionSpec)
{
	FString failureReason;
	AScenarioGroundRegion* groundRegion = AScenarioGroundRegion::SpawnConfigured(
		GetWorld(),
		AScenarioGroundRegion::StaticClass(),
		regionSpec,
		failureReason);
	if (!groundRegion)
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("Ground region '%s' spawn failed: %s"),
			*regionSpec.RegionId,
			*failureReason);
		return nullptr;
	}

	RuntimeActors.Add(groundRegion);
	RuntimeGroundRegions.Add(regionSpec.RegionId, groundRegion);
	return groundRegion;
}

AScenarioGroundRegion* UScenarioSimulationSubsystem::FindGroundRegion(const FString& regionId) const
{
	if (const TObjectPtr<AScenarioGroundRegion>* foundRegion = RuntimeGroundRegions.Find(regionId)) return foundRegion->Get();

	return nullptr;
}

AScenarioPedestrian* UScenarioSimulationSubsystem::SpawnPedestrianOnPath(
	TSubclassOf<AScenarioPedestrian> inPedestrianClass,
	const FTransform& spawnTransform,
	AScenarioSplinePath* splinePath,
	double speedCmPerSecond,
	double initialDistanceCm,
	bool bStartFollowing)
{
	UWorld* world = GetWorld();
	if (!world || !inPedestrianClass || !splinePath) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioPedestrian* pedestrian = world->SpawnActorDeferred<AScenarioPedestrian>(
		inPedestrianClass,
		spawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!pedestrian) return nullptr;

	if (UScenarioPathFollowerComponent* pathFollower = pedestrian->PathFollowerComponent)
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

AScenarioPedestrian* UScenarioSimulationSubsystem::SpawnPedestrianOnPathId(
	TSubclassOf<AScenarioPedestrian> inPedestrianClass,
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

AActor* UScenarioSimulationSubsystem::SpawnPlaceable(const FScenarioPlaceableInstanceSpec& placeableSpec)
{
	switch (placeableSpec.Category)
	{
	case EScenarioActorCategory::StaticObstacle:
		return SpawnStaticObstacle(placeableSpec);
	case EScenarioActorCategory::DeliveryBot:
		return SpawnRobotActor(placeableSpec);
	default:
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("배치 액터 '%s'의 카테고리를 지원하지 않음."),
			*placeableSpec.InstanceId);
		return nullptr;
	}
}

AScenarioStaticObstacle* UScenarioSimulationSubsystem::SpawnStaticObstacle(const FScenarioPlaceableInstanceSpec& placeableSpec)
{
	if (placeableSpec.InstanceId.IsEmpty() || placeableSpec.AssetId.IsEmpty()) return nullptr;

	FScenarioStaticObstaclePropEntry propEntry;
	if (!TryFindStaticObstacleProp(FName(*placeableSpec.AssetId), propEntry))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("Static obstacle '%s' references unknown prop '%s'."),
			*placeableSpec.InstanceId,
			*placeableSpec.AssetId);
		return nullptr;
	}

	FString failureReason;
	AScenarioStaticObstacle* staticObstacle = AScenarioStaticObstacle::SpawnConfigured(
		GetWorld(),
		StaticObstacleClass,
		placeableSpec.Transform,
		propEntry,
		failureReason);
	if (!staticObstacle)
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("Static obstacle '%s' spawn failed for prop '%s': %s"),
			*placeableSpec.InstanceId,
			*placeableSpec.AssetId,
			*failureReason);
		return nullptr;
	}

	RegisterRuntimeActor(
		placeableSpec.InstanceId,
		placeableSpec.AssetId,
		placeableSpec.Category,
		staticObstacle);
	return staticObstacle;
}

bool UScenarioSimulationSubsystem::TryFindStaticObstacleProp(
	FName propId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	if (propId.IsNone()) return false;

	const UScenarioStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog))
	{
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("Scenario static obstacle prop catalog is not configured or failed to load: %s"),
			*StaticObstaclePropCatalog.ToSoftObjectPath().ToString());
		return false;
	}

	return propCatalog->FindPropEntryById(propId, outPropEntry);
}

void UScenarioSimulationSubsystem::ApplyRuntimeViewportPresentation()
{
	if (IsValid(RuntimeGreyBackgroundPostProcessVolume))
	{
		return;
	}

	RuntimeGreyBackgroundPostProcessVolume =
		FScenarioViewportPresentation::SpawnGreyBackgroundPostProcessVolume(GetWorld(), 1.0f);
}

void UScenarioSimulationSubsystem::AddRuntimeRouteMarkerOverlayItem(
	const FString& instanceId,
	const bool bStartMarker,
	const FVector& worldLocation)
{
	FScenarioEditorRouteMarkerOverlayItem overlayItem;
	overlayItem.InstanceId = instanceId;
	overlayItem.Kind = bStartMarker
		? EScenarioEditorRouteMarkerKind::Start
		: EScenarioEditorRouteMarkerKind::Goal;
	overlayItem.WorldLocation = worldLocation;
	RuntimeRouteMarkerOverlayItems.Add(overlayItem);
}

void UScenarioSimulationSubsystem::ShowRuntimeRouteMarkerOverlayWidget()
{
	if (RuntimeRouteMarkerOverlayItems.IsEmpty())
	{
		RemoveRuntimeRouteMarkerOverlayWidget();
		return;
	}

	UWorld* world = GetWorld();
	APlayerController* playerController = world ? world->GetFirstPlayerController() : nullptr;
	if (!IsValid(playerController))
	{
		return;
	}

	if (!IsValid(RuntimeRouteMarkerOverlayWidget))
	{
		RuntimeRouteMarkerOverlayWidget = CreateWidget<UScenarioEditorRouteMarkerOverlayWidget>(
			playerController,
			UScenarioEditorRouteMarkerOverlayWidget::StaticClass());
		if (!IsValid(RuntimeRouteMarkerOverlayWidget))
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("Runtime route marker overlay widget creation failed."));
			return;
		}
	}

	RuntimeRouteMarkerOverlayWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!RuntimeRouteMarkerOverlayWidget->IsInViewport())
	{
		RuntimeRouteMarkerOverlayWidget->AddToViewport(0);
	}
}

void UScenarioSimulationSubsystem::RemoveRuntimeRouteMarkerOverlayWidget()
{
	if (IsValid(RuntimeRouteMarkerOverlayWidget))
	{
		RuntimeRouteMarkerOverlayWidget->RemoveFromParent();
	}
	RuntimeRouteMarkerOverlayWidget = nullptr;
}

// Robot spawn 직후 local player에 고정 초기 시야를 적용한다.
void UScenarioSimulationSubsystem::ApplyRuntimeInitialView(AActor* robotActor)
{
	UWorld* world = GetWorld();
	APlayerController* playerController = world ? world->GetFirstPlayerController() : nullptr;
	APawn* playerPawn = IsValid(playerController) ? playerController->GetPawn() : nullptr;
	if (!IsValid(world) || !IsValid(playerController) || !IsValid(playerPawn) || !IsValid(robotActor))
	{
		return;
	}

	const FVector robotLocation = robotActor->GetActorLocation();
	const FVector rawRobotForward = robotActor->GetActorForwardVector();
	const FVector robotForward = rawRobotForward.IsNearlyZero()
		? FVector::ForwardVector
		: rawRobotForward.GetSafeNormal();
	const FVector targetLocation =
		robotLocation + FVector::UpVector * RuntimeInitialCameraTargetHeightCm;
	const FVector cameraLocation =
		robotLocation
		- robotForward * RuntimeInitialCameraBackDistanceCm
		+ FVector::UpVector * RuntimeInitialCameraHeightCm;
	const FRotator cameraRotation =
		(targetLocation - cameraLocation).Rotation();

	playerPawn->SetActorLocationAndRotation(cameraLocation, cameraRotation);
	playerController->SetControlRotation(cameraRotation);
	FViewTargetTransitionParams transitionParams;
	playerController->SetViewTarget(playerPawn, transitionParams);
}

AActor* UScenarioSimulationSubsystem::SpawnRobotActor(const FScenarioPlaceableInstanceSpec& placeableSpec)
{
	UWorld* world{ GetWorld() };

	if (!world || placeableSpec.InstanceId.IsEmpty() || !RobotActorClass) return nullptr;

	const FScenarioDeliveryBotSpawnSpec& deliveryBotSpec = placeableSpec.DeliveryBot;
	const bool bSpawnOnly = deliveryBotSpec.bSpawnOnly;

	FDeliveryBotSetupInfo setupInfo = deliveryBotSpec.SetupInfo;
	if (!deliveryBotSpec.bHasStartLocation)
	{
		setupInfo.LocationSetupInfo.StartLocationCm = placeableSpec.Transform.GetLocation();
		setupInfo.LocationSetupInfo.GoalLocationCm = setupInfo.LocationSetupInfo.StartLocationCm;
	}

	const bool bRouteAutoStart = setupInfo.LocationSetupInfo.bAutoStartRoute;
	FVector goalLocation = setupInfo.LocationSetupInfo.GoalLocationCm;
	const bool bHasGoal = deliveryBotSpec.bHasGoalLocation;

	setupInfo.LocationSetupInfo.bHasGoal = bHasGoal;
	if (!bHasGoal)
	{
		goalLocation = setupInfo.LocationSetupInfo.StartLocationCm;
		setupInfo.LocationSetupInfo.GoalLocationCm = goalLocation;
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
			LogScenarioSimulation,
			Error,
			TEXT("Scenario setup 실패: DeliveryBot의 start/goal이 그리드 상 도달할 수 없는 지점. Robot: %s"),
			*placeableSpec.InstanceId
		);
		return nullptr;
	}

	setupInfo.LocationSetupInfo.bAutoStartRoute = !bSpawnOnly && bRouteAutoStart && bHasGoal;
	FTransform robotSpawnTransform = placeableSpec.Transform;
	robotSpawnTransform.SetLocation(setupInfo.LocationSetupInfo.StartLocationCm);

	ADeliveryBot* robotActor{
		world->SpawnActorDeferred<ADeliveryBot>(
			RobotActorClass,
			robotSpawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		)
	};

	if (!robotActor) return nullptr;

	robotActor->InitializeSetupInfo(setupInfo);

	UGameplayStatics::FinishSpawningActor(
		robotActor,
		robotSpawnTransform
	);

	RegisterRuntimeActor(
		placeableSpec.InstanceId,
		placeableSpec.AssetId,
		placeableSpec.Category,
		robotActor);
	ApplyRuntimeInitialView(robotActor);

	if (!bSpawnOnly)
	{
		if (!bHasGoal)
		{
			UE_LOG(LogScenarioSimulation, Warning, TEXT("로봇 '%s'에 이동 목표가 없어 경로 주입을 건너뜀."), *placeableSpec.InstanceId);
			return robotActor;
		}

		AddRuntimeRouteMarkerOverlayItem(
			FString::Printf(TEXT("%s:start"), *placeableSpec.InstanceId),
			true,
			robotSpawnTransform.GetLocation());
		AddRuntimeRouteMarkerOverlayItem(
			FString::Printf(TEXT("%s:goal"), *placeableSpec.InstanceId),
			false,
			goalLocation);

	}

	return robotActor;
}

AActor* UScenarioSimulationSubsystem::SpawnDynamicActor(const FScenarioDynamicActorSpec& dynamicActorSpec)
{
	switch (dynamicActorSpec.Category)
	{
	case EScenarioActorCategory::Pedestrian:
		return SpawnPedestrian(dynamicActorSpec);
	default:
		UE_LOG(
			LogScenarioSimulation,
			Warning,
			TEXT("동적 액터 '%s'의 카테고리를 지원하지 않음."),
			*dynamicActorSpec.InstanceId);
		return nullptr;
	}
}

AScenarioPedestrian* UScenarioSimulationSubsystem::SpawnPedestrian(const FScenarioDynamicActorSpec& dynamicActorSpec)
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

		AScenarioPedestrian* pedestrian = world->SpawnActor<AScenarioPedestrian>(
			PedestrianClass ? PedestrianClass.Get() : AScenarioPedestrian::StaticClass(),
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
			dynamicActorSpec.Category);
		RuntimeActorsById.Add(dynamicActorSpec.InstanceId, pedestrian);
		return pedestrian;
	}

	const double speedCmPerSecond = GetFloatProperty(dynamicActorSpec.Properties, TEXT("speed_cm_per_second"), 120.0);
	const double initialDistanceCm = GetFloatProperty(dynamicActorSpec.Properties, TEXT("initial_distance_cm"), 0.0);
	const bool bAutoStart = GetBoolProperty(dynamicActorSpec.Properties, TEXT("auto_start"), true);

	AScenarioPedestrian* pedestrian = SpawnPedestrianOnPathId(
		PedestrianClass ? PedestrianClass.Get() : AScenarioPedestrian::StaticClass(),
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
		dynamicActorSpec.Category);
	RuntimeActorsById.Add(dynamicActorSpec.InstanceId, pedestrian);
	return pedestrian;
}

AScenarioPedestrian* UScenarioSimulationSubsystem::SpawnPlannedPedestrian(const FScenarioDynamicActorSpec& dynamicActorSpec)
{
	UWorld* world = GetWorld();
	if (!world || dynamicActorSpec.InstanceId.IsEmpty()) return nullptr;

	UScenarioPedestrianPlanSubsystem* pedestrianPlanSubsystem = world->GetSubsystem<UScenarioPedestrianPlanSubsystem>();
	if (!pedestrianPlanSubsystem)
	{
		UE_LOG(LogScenarioSimulation, Warning, TEXT("보행자 plan subsystem이 없어 planned pedestrian '%s' 스폰 실패."), *dynamicActorSpec.InstanceId);
		return nullptr;
	}

	const FScenarioPedestrianPlan* plan = pedestrianPlanSubsystem->FindPlan(dynamicActorSpec.InstanceId);
	if (!plan)
	{
		UE_LOG(LogScenarioSimulation, Warning, TEXT("planned pedestrian '%s'에 대응하는 plan이 없음."), *dynamicActorSpec.InstanceId);
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

	AActor* robotActor = FindRuntimeActorByCategory(EScenarioActorCategory::DeliveryBot);
	if (!robotActor)
	{
		UE_LOG(
			LogScenarioSimulation,
			Error,
			TEXT("planned pedestrian '%s' requires a DeliveryBot runtime actor."),
			*dynamicActorSpec.InstanceId);
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioPedestrian* pedestrian = world->SpawnActorDeferred<AScenarioPedestrian>(
		PedestrianClass ? PedestrianClass.Get() : AScenarioPedestrian::StaticClass(),
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
		dynamicActorSpec.Category);
	RuntimeActorsById.Add(dynamicActorSpec.InstanceId, pedestrian);
	return pedestrian;
}

void UScenarioSimulationSubsystem::RegisterRuntimeActor(
	const FString& instanceId,
	const FString& assetId,
	EScenarioActorCategory category,
	AActor* actor)
{
	if (!actor) return;

	RuntimeActors.Add(actor);
	RuntimeActorsById.Add(instanceId, actor);
	SetActorReceivesDecals(actor, false);
	ConfigurePlaceableComponent(
		actor->FindComponentByClass<UScenarioPlaceableComponent>(),
		instanceId,
		assetId,
		category);
}

void UScenarioSimulationSubsystem::ConfigurePlaceableComponent(
	UScenarioPlaceableComponent* placeableComponent,
	const FString& instanceId,
	const FString& assetId,
	EScenarioActorCategory category) const
{
	if (!placeableComponent) return;

	placeableComponent->InstanceId = instanceId;
	placeableComponent->AssetId = assetId;
	placeableComponent->Category = category;
}

double UScenarioSimulationSubsystem::GetFloatProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	double defaultValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue) return defaultValue;

	if (paramValue->Type == EScenarioParamValueType::Float) return paramValue->FloatValue;

	if (paramValue->Type == EScenarioParamValueType::Integer) return paramValue->IntegerValue;

	return defaultValue;
}

bool UScenarioSimulationSubsystem::GetBoolProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	bool defaultValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::Bool) return defaultValue;

	return paramValue->BoolValue;
}

FString UScenarioSimulationSubsystem::GetStringProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	const FString& defaultValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::String) return defaultValue;

	return paramValue->StringValue;
}

bool UScenarioSimulationSubsystem::GetVectorProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	FVector& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::Vector) return false;

	outValue = paramValue->VectorValue;
	return true;
}

AActor* UScenarioSimulationSubsystem::FindRuntimeActorByCategory(EScenarioActorCategory category) const
{
	for (const TPair<FString, TObjectPtr<AActor>>& pair : RuntimeActorsById)
	{
		AActor* actor = pair.Value.Get();
		if (!IsValid(actor))
		{
			continue;
		}

		const UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>();
		if (!placeableComponent || placeableComponent->Category != category)
		{
			continue;
		}

		return actor;
	}

	return nullptr;
}

void UScenarioSimulationSubsystem::BuildPedestrianPlanContext(
	const FScenarioSimulationSetupSpec& setupSpec,
	FScenarioPedestrianPlanBuildContext& outBuildContext) const
{
	outBuildContext = FScenarioPedestrianPlanBuildContext{};
	outBuildContext.SourceSpecHash = setupSpec.SpecHash;
	outBuildContext.SemanticNavigationHash = TEXT("default");

	for (const FScenarioPlaceableInstanceSpec& placeableSpec : setupSpec.Placeables)
	{
		if (placeableSpec.Category != EScenarioActorCategory::StaticObstacle)
		{
			continue;
		}

		FVector boundsOrigin = FVector::ZeroVector;
		FVector boundsExtent = FVector::ZeroVector;
		FScenarioStaticObstaclePropEntry propEntry;
		if (TryFindStaticObstacleProp(FName(*placeableSpec.AssetId), propEntry))
		{
			boundsOrigin = placeableSpec.Transform.TransformPosition(propEntry.ResolveBoundsCenterOffsetCm());
			boundsExtent = propEntry.ResolveBoundsExtentCm();
		}
		else
		{
			AActor* obstacleActor = FindRuntimeActor(placeableSpec.InstanceId);
			if (!IsValid(obstacleActor))
			{
				continue;
			}

			obstacleActor->GetActorBounds(true, boundsOrigin, boundsExtent);
		}

		if (boundsExtent.IsNearlyZero())
		{
			boundsOrigin = placeableSpec.Transform.GetLocation();
			boundsExtent = FVector(50.0, 50.0, 100.0);
		}

		FScenarioPedestrianObstacleFootprint footprint;
		footprint.InstanceId = placeableSpec.InstanceId;
		footprint.AssetId = placeableSpec.AssetId;
		footprint.Center = boundsOrigin;
		footprint.Extent = boundsExtent;
		outBuildContext.StaticObstacleFootprints.Add(footprint);
	}
}

void UScenarioSimulationSubsystem::SetActorReceivesDecals(AActor* actor, bool bReceivesDecals)
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
