
#include "Episode/EpisodeSimulationSubsystem.h"

#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

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
	TSubclassOf<AEpisodePedestrian> PedestrianClass,
	const FTransform& SpawnTransform,
	AEpisodeSplinePath* SplinePath,
	double SpeedCmPerSecond,
	double InitialDistanceCm,
	bool bStartFollowing)
{
	UWorld* World = GetWorld();
	if (!World || !PedestrianClass || !SplinePath)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodePedestrian* Pedestrian = World->SpawnActorDeferred<AEpisodePedestrian>(
		PedestrianClass,
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
	TSubclassOf<AEpisodePedestrian> PedestrianClass,
	const FTransform& SpawnTransform,
	const FString& PathId,
	double SpeedCmPerSecond,
	double InitialDistanceCm,
	bool bStartFollowing)
{
	return SpawnPedestrianOnPath(
		PedestrianClass,
		SpawnTransform,
		FindSplinePath(PathId),
		SpeedCmPerSecond,
		InitialDistanceCm,
		bStartFollowing);
}

AEpisodePedestrian* UEpisodeSimulationSubsystem::SpawnSimplePedestrianPathTest(
	TSubclassOf<AEpisodePedestrian> PedestrianClass,
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
		PedestrianClass,
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
