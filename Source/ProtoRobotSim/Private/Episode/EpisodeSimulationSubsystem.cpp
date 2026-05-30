
#include "Episode/EpisodeSimulationSubsystem.h"

#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Actors/EpisodeVehicle.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/EpisodeCompiler.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeSimulation, Log, All);

namespace
{
	const TCHAR* ToCompileSeverityString(EEpisodeCompileDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EEpisodeCompileDiagnosticSeverity::Info:
			return TEXT("Info");
		case EEpisodeCompileDiagnosticSeverity::Warning:
			return TEXT("Warning");
		case EEpisodeCompileDiagnosticSeverity::Error:
			return TEXT("Error");
		default:
			return TEXT("Unknown");
		}
	}

	void LogCompileDiagnostics(const FEpisodeCompileResult& CompileResult)
	{
		for (const FEpisodeCompileDiagnostic& Diagnostic : CompileResult.Diagnostics)
		{
			UE_LOG(
				LogEpisodeSimulation,
				Log,
				TEXT("Episode compile %s [%s]: %s"),
				ToCompileSeverityString(Diagnostic.Severity),
				*Diagnostic.Code,
				*Diagnostic.Message);
		}
	}
}

UEpisodeSimulationSubsystem::UEpisodeSimulationSubsystem()
{
	StaticObstacleClass = AEpisodeStaticObstacle::StaticClass();
	RobotActorClass = AEpisodeVehicle::StaticClass();

	static ConstructorHelpers::FClassFinder<AEpisodePedestrian> PedestrianBlueprintClass(
		TEXT("/Game/Episode/Blueprints/BP_EpisodePedestrian"));
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
}

bool UEpisodeSimulationSubsystem::SpawnEpisodeWorld(const FEpisodeWorldSpec& WorldSpec)
{
	ClearEpisode();

	bool bAllSpawned = true;

	for (const FEpisodeGroundRegionSpec& RegionSpec : WorldSpec.GroundRegions)
	{
		if (!SpawnGroundRegion(RegionSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("Failed to spawn ground region '%s'."), *RegionSpec.RegionId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodePathSpec& PathSpec : WorldSpec.Paths)
	{
		if (PathSpec.PathType != EEpisodePathType::Spline)
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("Path '%s' is not spline type. Spawning it as spline for MVP."), *PathSpec.PathId);
		}

		if (!SpawnSplinePath(PathSpec.PathId, PathSpec.Points, PathSpec.bClosedLoop))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("Failed to spawn path '%s'."), *PathSpec.PathId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodePlaceableInstanceSpec& PlaceableSpec : WorldSpec.Placeables)
	{
		if (!SpawnPlaceable(PlaceableSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("Failed to spawn placeable '%s'."), *PlaceableSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	for (const FEpisodeDynamicActorSpec& DynamicActorSpec : WorldSpec.DynamicActors)
	{
		if (!SpawnDynamicActor(DynamicActorSpec))
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("Failed to spawn dynamic actor '%s'."), *DynamicActorSpec.InstanceId);
			bAllSpawned = false;
		}
	}

	return bAllSpawned;
}

bool UEpisodeSimulationSubsystem::SpawnEpisodeWorldFromJsonFile(const FString& JsonFilePath)
{
	if (JsonFilePath.IsEmpty())
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("Episode JSON file path is empty."));
		return false;
	}

	UEpisodeCompiler* Compiler = NewObject<UEpisodeCompiler>(this);
	if (!Compiler)
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("Failed to create EpisodeCompiler."));
		return false;
	}

	const FEpisodeCompileResult CompileResult = Compiler->CompileEpisodeWorldSpecFromJsonFile(JsonFilePath);
	LogCompileDiagnostics(CompileResult);
	if (!CompileResult.bSuccess)
	{
		UE_LOG(LogEpisodeSimulation, Warning, TEXT("Episode JSON compile failed: %s"), *JsonFilePath);
		return false;
	}

	return SpawnEpisodeWorld(CompileResult.WorldSpec);
}

bool UEpisodeSimulationSubsystem::SpawnSampleEpisodeWorldFromJson()
{
	const FString SampleJsonPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Json"), TEXT("EpisodeActorSpawnMVP.json")));
	return SpawnEpisodeWorldFromJsonFile(SampleJsonPath);
}

AActor* UEpisodeSimulationSubsystem::FindRuntimeActor(const FString& InstanceId) const
{
	if (const TObjectPtr<AActor>* FoundActor = RuntimeActorsById.Find(InstanceId))
	{
		return FoundActor->Get();
	}

	return nullptr;
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

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnSimplePedestrianPathTest(
	TSubclassOf<AEpisodePedestrian> InPedestrianClass,
	const FVector& StartLocation,
	const FVector& EndLocation,
	double SpeedCmPerSecond)
{
	ClearEpisode();

	const FVector PathDelta = EndLocation - StartLocation;
	if (PathDelta.IsNearlyZero())
	{
		return nullptr;
	}

	TArray<FVector> PathPoints;
	PathPoints.Add(StartLocation);
	PathPoints.Add(EndLocation);

	const FString DebugPathId = TEXT("debug_pedestrian_path");
	AEpisodeSplinePath* PathActor = SpawnSplinePath(DebugPathId, PathPoints, false);
	if (!PathActor)
	{
		return nullptr;
	}

	FVector SpawnForward = PathDelta;
	SpawnForward.Z = 0.0;
	const FRotator SpawnRotation = SpawnForward.IsNearlyZero()
		? FRotator::ZeroRotator
		: SpawnForward.Rotation();

	return SpawnPedestrianOnPath(
		InPedestrianClass,
		FTransform(SpawnRotation, StartLocation, FVector::OneVector),
		PathActor,
		SpeedCmPerSecond,
		0.0,
		true);
}

void UEpisodeSimulationSubsystem::SpawnDebugGroundRegionTest()
{
	ClearEpisode();

	TArray<FEpisodeGroundRegionSpec> GroundRegionSpecs;

	FEpisodeGroundRegionSpec WalkableRegion;
	WalkableRegion.RegionId = TEXT("debug_walkable_sidewalk");
	WalkableRegion.RegionType = EEpisodeGroundRegionType::Walkable;
	WalkableRegion.ShapeType = EEpisodeGroundShapeType::Rectangle;
	WalkableRegion.Center = FVector(0.0, 0.0, 0.0);
	WalkableRegion.Size = FVector2D(1200.0, 240.0);
	WalkableRegion.TraversabilityScore = 1.0;
	GroundRegionSpecs.Add(WalkableRegion);

	FEpisodeGroundRegionSpec PenaltyRegion;
	PenaltyRegion.RegionId = TEXT("debug_penalty_road");
	PenaltyRegion.RegionType = EEpisodeGroundRegionType::Penalty;
	PenaltyRegion.ShapeType = EEpisodeGroundShapeType::Rectangle;
	PenaltyRegion.Center = FVector(0.0, -230.0, 0.0);
	PenaltyRegion.Size = FVector2D(1200.0, 300.0);
	PenaltyRegion.TraversabilityScore = 0.6;
	PenaltyRegion.PenaltyKind = TEXT("sidewalk_departure");
	PenaltyRegion.PenaltyCost = 5.0;
	PenaltyRegion.ViolationAfterSeconds = 0.2;
	GroundRegionSpecs.Add(PenaltyRegion);

	FEpisodeGroundRegionSpec BlockedRegion;
	BlockedRegion.RegionId = TEXT("debug_blocked_wall");
	BlockedRegion.RegionType = EEpisodeGroundRegionType::Blocked;
	BlockedRegion.ShapeType = EEpisodeGroundShapeType::Rectangle;
	BlockedRegion.Center = FVector(0.0, 160.0, 0.0);
	BlockedRegion.Size = FVector2D(1200.0, 30.0);
	BlockedRegion.CollisionTag = TEXT("wall");
	GroundRegionSpecs.Add(BlockedRegion);

	SpawnGroundRegions(GroundRegionSpecs);
}

AActor* UEpisodeSimulationSubsystem::SpawnPlaceable(const FEpisodePlaceableInstanceSpec& PlaceableSpec)
{
	switch (PlaceableSpec.Category)
	{
	case EEpisodeActorCategory::StaticObstacle:
		return SpawnStaticObstacle(PlaceableSpec);
	case EEpisodeActorCategory::RoadVehicle:
		return SpawnRobotActor(PlaceableSpec);
	default:
		UE_LOG(
			LogEpisodeSimulation,
			Warning,
			TEXT("Unsupported placeable category for '%s'."),
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
			TEXT("Failed to apply static obstacle prop '%s' to '%s'."),
			*PlaceableSpec.AssetId,
			*PlaceableSpec.InstanceId);
		StaticObstacle->Destroy();
		return nullptr;
	}

	RegisterRuntimeActor(
		PlaceableSpec.InstanceId,
		PlaceableSpec.AssetId,
		PlaceableSpec.Category,
		PlaceableSpec.MobilityMode,
		StaticObstacle);
	return StaticObstacle;
}

AActor* UEpisodeSimulationSubsystem::SpawnRobotActor(const FEpisodePlaceableInstanceSpec& PlaceableSpec)
{
	UWorld* World = GetWorld();
	if (!World || PlaceableSpec.InstanceId.IsEmpty() || !RobotActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* RobotActor = World->SpawnActor<AActor>(
		RobotActorClass,
		PlaceableSpec.Transform,
		SpawnParams);
	if (!RobotActor)
	{
		return nullptr;
	}

	RegisterRuntimeActor(
		PlaceableSpec.InstanceId,
		PlaceableSpec.AssetId,
		PlaceableSpec.Category,
		PlaceableSpec.MobilityMode,
		RobotActor);
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
			TEXT("Unsupported dynamic actor category for '%s'."),
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
		DynamicActorSpec.Category,
		DynamicActorSpec.MobilityMode);
	RuntimeActorsById.Add(DynamicActorSpec.InstanceId, Pedestrian);
	return Pedestrian;
}

void UEpisodeSimulationSubsystem::RegisterRuntimeActor(
	const FString& InstanceId,
	const FString& AssetId,
	EEpisodeActorCategory Category,
	EEpisodeMobilityMode MobilityMode,
	AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	RuntimeActors.Add(Actor);
	RuntimeActorsById.Add(InstanceId, Actor);
	ConfigurePlaceableComponent(
		Actor->FindComponentByClass<UEpisodePlaceableComponent>(),
		InstanceId,
		AssetId,
		Category,
		MobilityMode);
}

void UEpisodeSimulationSubsystem::ConfigurePlaceableComponent(
	UEpisodePlaceableComponent* PlaceableComponent,
	const FString& InstanceId,
	const FString& AssetId,
	EEpisodeActorCategory Category,
	EEpisodeMobilityMode MobilityMode) const
{
	if (!PlaceableComponent)
	{
		return;
	}

	PlaceableComponent->InstanceId = InstanceId;
	PlaceableComponent->AssetId = AssetId;
	PlaceableComponent->Category = Category;
	PlaceableComponent->MobilityMode = MobilityMode;
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
		return static_cast<double>(ParamValue->IntegerValue);
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
