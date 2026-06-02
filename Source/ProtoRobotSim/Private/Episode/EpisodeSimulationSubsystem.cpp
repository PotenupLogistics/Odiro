

#include "Episode/EpisodeSimulationSubsystem.h"
#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/EpisodeCompiler.h"
#include "DeliveryBot/Actor/DeliveryBot_ChaosActor.h"
#include "Shared/Struct/DeliveryBotSetupInfo.h"
#include "Kismet/GameplayStatics.h"


DEFINE_LOG_CATEGORY_STATIC(LogEpisodeSimulation, Log, All);

namespace
{
	const TCHAR* ToSimulationCompileSeverityString(EEpisodeCompileDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EEpisodeCompileDiagnosticSeverity::Info:
			return TEXT("정보");
		case EEpisodeCompileDiagnosticSeverity::Warning:
			return TEXT("경고");
		case EEpisodeCompileDiagnosticSeverity::Error:
			return TEXT("오류");
		default:
			return TEXT("알 수 없음");
		}
	}

	void LogCompileDiagnostics(const FEpisodeCompileResult& CompileResult)
	{
		for (const FEpisodeCompileDiagnostic& Diagnostic : CompileResult.Diagnostics)
		{
			UE_LOG(LogEpisodeSimulation, Log, TEXT("에피소드 컴파일 진단 %s [%s]: %s"), ToSimulationCompileSeverityString(Diagnostic.Severity), *Diagnostic.Code, *Diagnostic.Message);
		}
	}
}

UEpisodeSimulationSubsystem::UEpisodeSimulationSubsystem()
{
	StaticObstacleClass = AEpisodeStaticObstacle::StaticClass();

	// DeliveryBot branch uses the Chaos-based robot actor as the episode robot runtime.
	static ConstructorHelpers::FClassFinder<ADeliveryBot_ChaosActor> RobotBlueprintClass(
		TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot_ChaosMesh"));

	if (RobotBlueprintClass.Succeeded())
	{
		RobotActorClass = RobotBlueprintClass.Class;
	}
	else
	{
		RobotActorClass = ADeliveryBot_ChaosActor::StaticClass();
	}

	static ConstructorHelpers::FClassFinder<AActor> GoalPointBlueprintClass(TEXT("/Game/Episode/Blueprints/BP_GoalPoint"));
	if (GoalPointBlueprintClass.Succeeded())
	{
		GoalPointClass = GoalPointBlueprintClass.Class;
	}
	static ConstructorHelpers::FClassFinder<AActor> StartPointBlueprintClass(TEXT("/Game/Episode/Blueprints/BP_StartPoint"));
	if (StartPointBlueprintClass.Succeeded())
	{
		StartPointClass = StartPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AEpisodePedestrian> PedestrianBlueprintClass(TEXT("/Game/Episode/Blueprints/BP_EpisodePedestrian"));
	if (PedestrianBlueprintClass.Succeeded())
	{
		PedestrianClass = PedestrianBlueprintClass.Class;
	}
	else
	{
		PedestrianClass = AEpisodePedestrian::StaticClass();
	}
}

void UEpisodeSimulationSubsystem::ClearEpisode()
{
	const int32 ActorCount = RuntimeActors.Num();
	const int32 ActorIdCount = RuntimeActorsById.Num();
	const int32 GroundRegionCount = RuntimeGroundRegions.Num();
	const int32 PathCount = RuntimePaths.Num();

	for (int32 Index = RuntimeActors.Num() - 1; Index >= 0; --Index)
	{
		if (AActor* Actor = RuntimeActors[Index].Get())
		{
			Actor->Destroy();
		}
	}

	RuntimeActors.Reset();
	RuntimeGroundRegions.Reset();
	RuntimePaths.Reset();
	RuntimeActorsById.Reset();

	if (ActorCount > 0 || ActorIdCount > 0 || GroundRegionCount > 0 || PathCount > 0)
	{
		UE_LOG(
			LogEpisodeSimulation,
			Log,
			TEXT("Episode runtime cleared | Actors: %d, ActorIds: %d, GroundRegions: %d, Paths: %d"),
			ActorCount,
			ActorIdCount,
			GroundRegionCount,
			PathCount);
	}
}

bool UEpisodeSimulationSubsystem::SpawnEpisodeWorld(const FEpisodeWorldSpec& WorldSpec)
{
	ClearEpisode();

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("Episode world spawn started | Episode: %s, GroundRegions: %d, Paths: %d, Placeables: %d, DynamicActors: %d"),
		*WorldSpec.RunConfig.TemplateId,
		WorldSpec.GroundRegions.Num(),
		WorldSpec.Paths.Num(),
		WorldSpec.Placeables.Num(),
		WorldSpec.DynamicActors.Num());

	bool bAllSpawned{ true };

	for (const FEpisodeGroundRegionSpec& RegionSpec : WorldSpec.GroundRegions)
	{
		if (!SpawnGroundRegion(RegionSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("지면 영역 '%s' 스폰 실패."), *RegionSpec.RegionId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodePathSpec& PathSpec : WorldSpec.Paths)
	{
		if (PathSpec.PathType != EEpisodePathType::Spline)
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("경로 '%s'가 spline 타입이 아님."), *PathSpec.PathId);
		}

		if (!SpawnSplinePath(PathSpec.PathId, PathSpec.Points, PathSpec.bClosedLoop))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("경로 '%s' 스폰 실패."), *PathSpec.PathId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodePlaceableInstanceSpec& PlaceableSpec : WorldSpec.Placeables)
	{
		if (!SpawnPlaceable(PlaceableSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("배치 액터 '%s' 스폰 실패."), *PlaceableSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodeDynamicActorSpec& DynamicActorSpec : WorldSpec.DynamicActors)
	{
		if (!SpawnDynamicActor(DynamicActorSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("동적 액터 '%s' 스폰 실패."), *DynamicActorSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("Episode world spawn completed | Episode: %s, Success: %s, RuntimeActors: %d, ActorIds: %d, GroundRegions: %d, Paths: %d"),
		*WorldSpec.RunConfig.TemplateId,
		bAllSpawned ? TEXT("true") : TEXT("false"),
		RuntimeActors.Num(),
		RuntimeActorsById.Num(),
		RuntimeGroundRegions.Num(),
		RuntimePaths.Num());

	return bAllSpawned;
}

bool UEpisodeSimulationSubsystem::SpawnEpisodeWorldFromJsonFile(const FString& JsonFilePath)
{
	if (JsonFilePath.IsEmpty())
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("에피소드 JSON 파일 경로가 비어 있음."));
		return false;
	}

	UEpisodeCompiler* Compiler = NewObject<UEpisodeCompiler>(this);
	if (!Compiler)
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("EpisodeCompiler 생성 실패."));
		return false;
	}

	const FEpisodeCompileResult CompileResult = Compiler->CompileEpisodeWorldSpecFromJsonFile(JsonFilePath);
	LogCompileDiagnostics(CompileResult);
	if (!CompileResult.bSuccess)
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("에피소드 JSON 컴파일 실패: %s"), *JsonFilePath);
		return false;
	}

	return SpawnEpisodeWorld(CompileResult.WorldSpec);
}

AActor* UEpisodeSimulationSubsystem::FindRuntimeActor(const FString& InstanceId) const
{
	if (const TObjectPtr<AActor>* FoundActor = RuntimeActorsById.Find(InstanceId))
	{
		return FoundActor->Get();
	}

	return nullptr;
}

FEpisodeRuntimeContext UEpisodeSimulationSubsystem::BuildRuntimeContext(const FEpisodeWorldSpec& WorldSpec) const
{
	FEpisodeRuntimeContext RuntimeContext;
	RuntimeContext.EpisodeId = WorldSpec.RunConfig.TemplateId;
	RuntimeContext.SpecHash = WorldSpec.SpecHash;

	for (AActor* Actor : RuntimeActors)
	{
		if (IsValid(Actor))
		{
			RuntimeContext.RuntimeActors.Add(Actor);
		}
	}

	for (const TPair<FString, TObjectPtr<AEpisodeGroundRegion>>& Pair : RuntimeGroundRegions)
	{
		if (AActor* GroundRegionActor = Pair.Value.Get())
		{
			RuntimeContext.GroundRegionActors.Add(GroundRegionActor);
		}
	}

	for (const FEpisodePlaceableInstanceSpec& PlaceableSpec : WorldSpec.Placeables)
	{
		AActor* RuntimeActor = FindRuntimeActor(PlaceableSpec.InstanceId);
		if (!RuntimeActor)
		{
			continue;
		}

		if ((PlaceableSpec.Category == EEpisodeActorCategory::DeliveryBot ||
			PlaceableSpec.Category == EEpisodeActorCategory::RoadVehicle) && !RuntimeContext.RobotActor)
		{
			RuntimeContext.RobotInstanceId = PlaceableSpec.InstanceId;
			RuntimeContext.RobotActor = RuntimeActor;
			RuntimeContext.bHasGoalLocation = GetVectorProperty(PlaceableSpec.Properties, TEXT("goal_cm"), RuntimeContext.GoalLocation);
			continue;
		}

		if (PlaceableSpec.Category == EEpisodeActorCategory::StaticObstacle)
		{
			RuntimeContext.StaticObstacleActors.Add(RuntimeActor);
		}
	}

	for (const FEpisodeDynamicActorSpec& DynamicActorSpec : WorldSpec.DynamicActors)
	{
		AActor* RuntimeActor = FindRuntimeActor(DynamicActorSpec.InstanceId);
		if (!RuntimeActor)
		{
			continue;
		}

		if (DynamicActorSpec.Category == EEpisodeActorCategory::Pedestrian)
		{
			RuntimeContext.PedestrianActors.Add(RuntimeActor);
			RuntimeContext.PedestrianInstanceIds.Add(DynamicActorSpec.InstanceId);
		}
	}

	UE_LOG(
		LogEpisodeSimulation,
		Log,
		TEXT("Runtime context built | Episode: %s, SpecHash: %s, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
		*RuntimeContext.EpisodeId,
		*RuntimeContext.SpecHash,
		*RuntimeContext.RobotInstanceId,
		RuntimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		RuntimeContext.RuntimeActors.Num(),
		RuntimeContext.GroundRegionActors.Num(),
		RuntimeContext.StaticObstacleActors.Num(),
		RuntimeContext.PedestrianActors.Num());

	if (!IsValid(RuntimeContext.RobotActor))
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("Runtime context has no valid robot actor | Episode: %s"), *RuntimeContext.EpisodeId);
	}

	return RuntimeContext;
}

AEpisodeSplinePath* UEpisodeSimulationSubsystem::SpawnSplinePath(const FString& PathId, const TArray<FVector>& Points, bool bClosedLoop)
{
	UWorld* World = GetWorld();
	if (!World || PathId.IsEmpty() || Points.Num() < 2)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeSplinePath* PathActor = World->SpawnActor<AEpisodeSplinePath>(
		AEpisodeSplinePath::StaticClass(),
		FTransform::Identity,
		SpawnParams);
	if (!PathActor)
	{
		return nullptr;
	}

	PathActor->ConfigurePath(PathId, Points, bClosedLoop);
	RuntimeActors.Add(PathActor);
	RuntimePaths.Add(PathId, PathActor);
	return PathActor;
}

AEpisodeSplinePath* UEpisodeSimulationSubsystem::FindSplinePath(const FString& PathId) const
{
	if (const TObjectPtr<AEpisodeSplinePath>* FoundPath = RuntimePaths.Find(PathId))
	{
		return FoundPath->Get();
	}

	return nullptr;
}

AEpisodeGroundRegion* UEpisodeSimulationSubsystem::SpawnGroundRegion(const FEpisodeGroundRegionSpec& RegionSpec)
{
	UWorld* World = GetWorld();
	if (!World || RegionSpec.RegionId.IsEmpty() || RegionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeGroundRegion* GroundRegion = World->SpawnActor<AEpisodeGroundRegion>(
		AEpisodeGroundRegion::StaticClass(),
		FTransform::Identity,
		SpawnParams);
	if (!GroundRegion)
	{
		return nullptr;
	}

	GroundRegion->ConfigureRegion(RegionSpec);
	RuntimeActors.Add(GroundRegion);
	RuntimeGroundRegions.Add(RegionSpec.RegionId, GroundRegion);
	return GroundRegion;
}

void UEpisodeSimulationSubsystem::SpawnGroundRegions(const TArray<FEpisodeGroundRegionSpec>& RegionSpecs)
{
	for (const FEpisodeGroundRegionSpec& RegionSpec : RegionSpecs)
	{
		SpawnGroundRegion(RegionSpec);
	}
}

AEpisodeGroundRegion* UEpisodeSimulationSubsystem::FindGroundRegion(const FString& RegionId) const
{
	if (const TObjectPtr<AEpisodeGroundRegion>* FoundRegion = RuntimeGroundRegions.Find(RegionId))
	{
		return FoundRegion->Get();
	}

	return nullptr;
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPedestrianOnPath(
	TSubclassOf<AEpisodePedestrian> InPedestrianClass,
	const FTransform& SpawnTransform,
	AEpisodeSplinePath* SplinePath,
	double SpeedCmPerSecond,
	double InitialDistanceCm,
	bool bStartFollowing)
{
	UWorld* World = GetWorld();
	if (!World || !InPedestrianClass || !SplinePath)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodePedestrian* Pedestrian = World->SpawnActorDeferred<AEpisodePedestrian>(
		InPedestrianClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pedestrian)
	{
		return nullptr;
	}

	if (UEpisodePathFollowerComponent* PathFollower = Pedestrian->PathFollowerComponent)
	{
		PathFollower->bAutoStart = false;
		PathFollower->SpeedCmPerSecond = SpeedCmPerSecond;
		PathFollower->InitialDistanceCm = InitialDistanceCm;
		PathFollower->CurrentDistanceCm = InitialDistanceCm;
		PathFollower->SetSplinePath(SplinePath);
	}

	UGameplayStatics::FinishSpawningActor(Pedestrian, SpawnTransform);
	SetActorReceivesDecals(Pedestrian, false);
	RuntimeActors.Add(Pedestrian);

	if (bStartFollowing && Pedestrian->PathFollowerComponent)
	{
		Pedestrian->PathFollowerComponent->StartFollowing();
	}

	return Pedestrian;
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPedestrianOnPathId(
	TSubclassOf<AEpisodePedestrian> InPedestrianClass,
	const FTransform& SpawnTransform,
	const FString& PathId,
	double SpeedCmPerSecond,
	double InitialDistanceCm,
	bool bStartFollowing)
{
	return SpawnPedestrianOnPath(
		InPedestrianClass,
		SpawnTransform,
		FindSplinePath(PathId),
		SpeedCmPerSecond,
		InitialDistanceCm,
		bStartFollowing);
}

AActor* UEpisodeSimulationSubsystem::SpawnPlaceable(const FEpisodePlaceableInstanceSpec& PlaceableSpec)
{
	switch (PlaceableSpec.Category)
	{
	case EEpisodeActorCategory::StaticObstacle:
		return SpawnStaticObstacle(PlaceableSpec);
	case EEpisodeActorCategory::DeliveryBot:
	case EEpisodeActorCategory::RoadVehicle:
		return SpawnRobotActor(PlaceableSpec);
	default:
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("배치 액터 '%s'의 카테고리를 지원하지 않음."),
			*PlaceableSpec.InstanceId);
		return nullptr;
	}
}

AEpisodeStaticObstacle* UEpisodeSimulationSubsystem::SpawnStaticObstacle(const FEpisodePlaceableInstanceSpec& PlaceableSpec)
{
	UWorld* World = GetWorld();
	TSubclassOf<AEpisodeStaticObstacle> SpawnClass = StaticObstacleClass;
	if (!SpawnClass)
	{
		SpawnClass = AEpisodeStaticObstacle::StaticClass();
	}
	if (!World || PlaceableSpec.InstanceId.IsEmpty() || PlaceableSpec.AssetId.IsEmpty() || !SpawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeStaticObstacle* StaticObstacle = World->SpawnActor<AEpisodeStaticObstacle>(
		SpawnClass,
		PlaceableSpec.Transform,
		SpawnParams);
	if (!StaticObstacle)
	{
		return nullptr;
	}

	if (!StaticObstacle->ApplyDefaultPropById(FName(*PlaceableSpec.AssetId)))
	{
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("정적 장애물 '%s'에 prop '%s' 적용 실패."),
			*PlaceableSpec.InstanceId,
			*PlaceableSpec.AssetId);
		StaticObstacle->Destroy();
		return nullptr;
	}

	RegisterRuntimeActor(
		PlaceableSpec.InstanceId,
		PlaceableSpec.AssetId,
		PlaceableSpec.Category,
		StaticObstacle);
	return StaticObstacle;
}

AActor* UEpisodeSimulationSubsystem::SpawnRobotActor(const FEpisodePlaceableInstanceSpec& PlaceableSpec)
{
	UWorld* World{ GetWorld() };

	if (!World || PlaceableSpec.InstanceId.IsEmpty() || !RobotActorClass)
	{
		return nullptr;
	}

	const bool bSpawnOnly = GetBoolProperty(PlaceableSpec.Properties, TEXT("spawn_only"), true);
	const bool bRouteAutoStart = GetBoolProperty(PlaceableSpec.Properties, TEXT("route_auto_start"), true);
	const FVector StartLocation = PlaceableSpec.Transform.GetLocation();
	FVector GoalLocation = StartLocation;
	const bool bHasGoal = GetVectorProperty(PlaceableSpec.Properties, TEXT("goal_cm"), GoalLocation);

	FDeliveryBotSetupInfo SetupInfo;
	SetupInfo.LocationSetupInfo.StartLocationCm = StartLocation;
	SetupInfo.LocationSetupInfo.GoalLocationCm = GoalLocation;
	SetupInfo.LocationSetupInfo.bAutoStartRoute = !bSpawnOnly && bRouteAutoStart && bHasGoal;

	ADeliveryBot_ChaosActor* RobotActor{
		World->SpawnActorDeferred<ADeliveryBot_ChaosActor>(
			RobotActorClass,
			PlaceableSpec.Transform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		)
	};

	if (!RobotActor)
	{
		return nullptr;
	}

	RobotActor->InitializeSetupInfo(SetupInfo);

	UGameplayStatics::FinishSpawningActor(
		RobotActor,
		PlaceableSpec.Transform
	);

	RegisterRuntimeActor(
		PlaceableSpec.InstanceId,
		PlaceableSpec.AssetId,
		PlaceableSpec.Category,
		RobotActor);

	if (!bSpawnOnly)
	{
		if (!bHasGoal)
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("로봇 '%s'의 이동 목표(goal_cm)가 없어 경로 주입을 건너뜀."), *PlaceableSpec.InstanceId);
			return RobotActor;
		}

		if (GoalPointClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AActor* GoalPointActor = World->SpawnActor<AActor>(GoalPointClass, FTransform(FRotator::ZeroRotator, GoalLocation), SpawnParams))
			{
				RuntimeActors.Add(GoalPointActor);
			}
		}

		if (StartPointClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AActor* StartPointActor = World->SpawnActor<AActor>(StartPointClass, FTransform(PlaceableSpec.Transform), SpawnParams))
			{
				RuntimeActors.Add(StartPointActor);
			}
		}

	}

	return RobotActor;
}

AActor* UEpisodeSimulationSubsystem::SpawnDynamicActor(const FEpisodeDynamicActorSpec& DynamicActorSpec)
{
	switch (DynamicActorSpec.Category)
	{
	case EEpisodeActorCategory::Pedestrian:
		return SpawnPedestrian(DynamicActorSpec);
	default:
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("동적 액터 '%s'의 카테고리를 지원하지 않음."),
			*DynamicActorSpec.InstanceId);
		return nullptr;
	}
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnPedestrian(const FEpisodeDynamicActorSpec& DynamicActorSpec)
{
	const double SpeedCmPerSecond = GetFloatProperty(DynamicActorSpec.Properties, TEXT("speed_cm_per_second"), 120.0);
	const double InitialDistanceCm = GetFloatProperty(DynamicActorSpec.Properties, TEXT("initial_distance_cm"), 0.0);
	const bool bAutoStart = GetBoolProperty(DynamicActorSpec.Properties, TEXT("auto_start"), true);

	AEpisodePedestrian* Pedestrian = SpawnPedestrianOnPathId(
		PedestrianClass ? PedestrianClass.Get() : AEpisodePedestrian::StaticClass(),
		DynamicActorSpec.InitialTransform,
		DynamicActorSpec.PathId,
		SpeedCmPerSecond,
		InitialDistanceCm,
		bAutoStart);
	if (!Pedestrian)
	{
		return nullptr;
	}

	ConfigurePlaceableComponent(
		Pedestrian->PlaceableComponent,
		DynamicActorSpec.InstanceId,
		DynamicActorSpec.AssetId,
		DynamicActorSpec.Category);
	RuntimeActorsById.Add(DynamicActorSpec.InstanceId, Pedestrian);
	return Pedestrian;
}

void UEpisodeSimulationSubsystem::RegisterRuntimeActor(
	const FString& InstanceId,
	const FString& AssetId,
	EEpisodeActorCategory Category,
	AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	RuntimeActors.Add(Actor);
	RuntimeActorsById.Add(InstanceId, Actor);
	SetActorReceivesDecals(Actor, false);
	ConfigurePlaceableComponent(
		Actor->FindComponentByClass<UEpisodePlaceableComponent>(),
		InstanceId,
		AssetId,
		Category);
}

void UEpisodeSimulationSubsystem::ConfigurePlaceableComponent(
	UEpisodePlaceableComponent* PlaceableComponent,
	const FString& InstanceId,
	const FString& AssetId,
	EEpisodeActorCategory Category) const
{
	if (!PlaceableComponent) return;

	PlaceableComponent->InstanceId = InstanceId;
	PlaceableComponent->AssetId = AssetId;
	PlaceableComponent->Category = Category;
}

double UEpisodeSimulationSubsystem::GetFloatProperty(
	const TMap<FString, FEpisodeParamValue>& Properties,
	const FString& Key,
	double DefaultValue)
{
	const FEpisodeParamValue* ParamValue = Properties.Find(Key);
	if (!ParamValue)
	{
		return DefaultValue;
	}

	if (ParamValue->Type == EEpisodeParamValueType::Float)
	{
		return ParamValue->FloatValue;
	}

	if (ParamValue->Type == EEpisodeParamValueType::Integer)
	{
		return ParamValue->IntegerValue;
	}

	return DefaultValue;
}

bool UEpisodeSimulationSubsystem::GetBoolProperty(
	const TMap<FString, FEpisodeParamValue>& Properties,
	const FString& Key,
	bool DefaultValue)
{
	const FEpisodeParamValue* ParamValue = Properties.Find(Key);
	if (!ParamValue || ParamValue->Type != EEpisodeParamValueType::Bool)
	{
		return DefaultValue;
	}

	return ParamValue->BoolValue;
}

bool UEpisodeSimulationSubsystem::GetVectorProperty(
	const TMap<FString, FEpisodeParamValue>& Properties,
	const FString& Key,
	FVector& OutValue)
{
	const FEpisodeParamValue* ParamValue = Properties.Find(Key);
	if (!ParamValue || ParamValue->Type != EEpisodeParamValueType::Vector)
	{
		return false;
	}

	OutValue = ParamValue->VectorValue;
	return true;
}

void UEpisodeSimulationSubsystem::SetActorReceivesDecals(AActor* Actor, bool bReceivesDecals)
{
	if (!Actor) return;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent)
		{
			PrimitiveComponent->SetReceivesDecals(bReceivesDecals);
		}
	}
}
