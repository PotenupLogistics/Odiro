#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Scenario/ScenarioCorridorGeometry.h"
#include "Scenario/Data/ScenarioCorridorSurfaceResolver.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioCorridorRuntime, Log, All);

namespace
{
	// Thin surface tops stay slightly above the ground to avoid z-fighting.
	const double RuntimeSurfaceTopZCm = 1.0;
	// Non-blocking surfaces are thick enough to overlap 15cm side offsets without vertical holes.
	const double RuntimeSurfaceHeightCm = 20.0;
	// Blocked corridor surfaces match the blocked ground-region collision height.
	const double RuntimeBlockedHeightCm = 200.0;

	struct FRuntimeCorridorVisualLaneSpec
	{
		FScenarioRuntimeCorridorLayoutEntry LayoutEntry;
		FScenarioRuntimeCorridorLaneSpec LaneSpec;
	};

	bool AreOffsetRangesEquivalent(
		const FScenarioOffsetRangeMeters& left,
		const FScenarioOffsetRangeMeters& right)
	{
		return FMath::IsNearlyEqual(
				left.MinMeters,
				right.MinMeters,
				FScenarioCorridorGeometry::SurfaceQueryToleranceMeters)
			&& FMath::IsNearlyEqual(
				left.MaxMeters,
				right.MaxMeters,
				FScenarioCorridorGeometry::SurfaceQueryToleranceMeters);
	}

	bool AreRuntimeVisualLanesEquivalent(
		const FScenarioRuntimeCorridorLaneSpec& left,
		const FScenarioRuntimeCorridorLaneSpec& right)
	{
		return left.LaneId == right.LaneId
			&& left.SurfaceId == right.SurfaceId
			&& left.RegionType == right.RegionType
			&& left.CollisionTag == right.CollisionTag
			&& left.PenaltyKind == right.PenaltyKind
			&& FMath::IsNearlyEqual(
				left.PenaltyCost,
				right.PenaltyCost,
				FScenarioCorridorGeometry::SurfaceQueryToleranceMeters)
			&& FMath::IsNearlyEqual(
				left.TraversabilityScore,
				right.TraversabilityScore,
				FScenarioCorridorGeometry::SurfaceQueryToleranceMeters)
			&& FMath::IsNearlyEqual(
				left.SurfaceZOffsetCm,
				right.SurfaceZOffsetCm,
				KINDA_SMALL_NUMBER)
			&& AreOffsetRangesEquivalent(left.OffsetRangeMeters, right.OffsetRangeMeters);
	}

	void AddOrMergeRuntimeVisualLane(
		TArray<FRuntimeCorridorVisualLaneSpec>& visualLaneSpecs,
		const FScenarioRuntimeCorridorLayoutEntry& layoutEntry,
		const FScenarioRuntimeCorridorLaneSpec& laneSpec)
	{
		for (FRuntimeCorridorVisualLaneSpec& visualLaneSpec : visualLaneSpecs)
		{
			if (AreRuntimeVisualLanesEquivalent(visualLaneSpec.LaneSpec, laneSpec)
				&& FMath::IsNearlyEqual(
					visualLaneSpec.LayoutEntry.AlongRangeMeters.EndMeters,
					layoutEntry.AlongRangeMeters.StartMeters,
					FScenarioCorridorGeometry::SurfaceQueryToleranceMeters))
			{
				visualLaneSpec.LayoutEntry.AlongRangeMeters.EndMeters = layoutEntry.AlongRangeMeters.EndMeters;
				return;
			}
		}

		FRuntimeCorridorVisualLaneSpec visualLaneSpec;
		visualLaneSpec.LayoutEntry = layoutEntry;
		visualLaneSpec.LaneSpec = laneSpec;
		visualLaneSpecs.Add(visualLaneSpec);
	}
}

AScenarioCorridorRuntimeActor::AScenarioCorridorRuntimeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SurfaceCatalog = UScenarioCorridorSurfaceCatalog::MakeDefaultCatalogReference();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> laneStripMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (laneStripMeshAsset.Succeeded())
	{
		LaneStripMesh = laneStripMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> walkableGroundMaterialAsset(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorSidewalk.M_ScenarioCorridorSidewalk"));
	if (walkableGroundMaterialAsset.Succeeded())
	{
		WalkableGroundMaterial = walkableGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> penaltyGroundMaterialAsset(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorRoad.M_ScenarioCorridorRoad"));
	if (penaltyGroundMaterialAsset.Succeeded())
	{
		PenaltyGroundMaterial = penaltyGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> blockedGroundMaterialAsset(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorBuilding.M_ScenarioCorridorBuilding"));
	if (blockedGroundMaterialAsset.Succeeded())
	{
		BlockedGroundMaterial = blockedGroundMaterialAsset.Object;
	}
}

void AScenarioCorridorRuntimeActor::ConfigureCorridor(const FScenarioRuntimeCorridorSpec& inCorridorSpec)
{
	CorridorSpec = inCorridorSpec;
	ClearLaneMeshes();

	if (!LaneStripMesh || CorridorSpec.PointsMeters.Num() < 2)
	{
		UE_LOG(
			LogScenarioCorridorRuntime,
			Warning,
			TEXT("Runtime corridor cannot render. CorridorId: %s | Points: %d | HasMesh: %s"),
			*CorridorSpec.CorridorId,
			CorridorSpec.PointsMeters.Num(),
			LaneStripMesh ? TEXT("true") : TEXT("false"));
		return;
	}

	TArray<FRuntimeCorridorVisualLaneSpec> visualLaneSpecs;
	for (const FScenarioRuntimeCorridorLayoutEntry& layoutEntry : CorridorSpec.Layout)
	{
		for (const FScenarioRuntimeCorridorLaneSpec& laneSpec : layoutEntry.Lanes)
		{
			AddOrMergeRuntimeVisualLane(visualLaneSpecs, layoutEntry, laneSpec);
		}
	}

	for (const FRuntimeCorridorVisualLaneSpec& visualLaneSpec : visualLaneSpecs)
	{
		AddLaneStrip(visualLaneSpec.LayoutEntry, visualLaneSpec.LaneSpec);
	}
}

void AScenarioCorridorRuntimeActor::ClearLaneMeshes()
{
	for (const TObjectPtr<USplineMeshComponent>& meshComponent : LaneMeshComponents)
	{
		if (IsValid(meshComponent))
		{
			meshComponent->DestroyComponent();
		}
	}

	LaneMeshComponents.Reset();
	Tags.Reset();
}

bool AScenarioCorridorRuntimeActor::TryFindSurfaceAtWorldLocation2D(
	const FVector& worldLocation,
	FScenarioRuntimeCorridorSurfaceQueryResult& outSurface) const
{
	outSurface = FScenarioRuntimeCorridorSurfaceQueryResult();
	const FVector2D localPointMeters =
		FScenarioCorridorGeometry::TransformRuntimeWorldCmToAxisPointMeters(CorridorSpec, worldLocation);
	double alongMeters = 0.0;
	double offsetMeters = 0.0;
	if (!FScenarioCorridorGeometry::TryProjectPointToAxisMeters(
		CorridorSpec.PointsMeters,
		localPointMeters,
		alongMeters,
		offsetMeters))
	{
		return false;
	}

	for (int32 layoutIndex = 0; layoutIndex < CorridorSpec.Layout.Num(); ++layoutIndex)
	{
		const FScenarioRuntimeCorridorLayoutEntry& layoutEntry = CorridorSpec.Layout[layoutIndex];
		if (!FScenarioCorridorGeometry::ContainsRangeValue(
			alongMeters,
			layoutEntry.AlongRangeMeters.StartMeters,
			layoutEntry.AlongRangeMeters.EndMeters))
		{
			continue;
		}

		for (int32 laneIndex = 0; laneIndex < layoutEntry.Lanes.Num(); ++laneIndex)
		{
			const FScenarioRuntimeCorridorLaneSpec& laneSpec = layoutEntry.Lanes[laneIndex];
			if (!FScenarioCorridorGeometry::ContainsRangeValue(
				offsetMeters,
				laneSpec.OffsetRangeMeters.MinMeters,
				laneSpec.OffsetRangeMeters.MaxMeters))
			{
				continue;
			}

			outSurface.SurfaceInstanceId = FScenarioCorridorGeometry::MakeSurfaceInstanceId(
				CorridorSpec,
				layoutEntry,
				laneSpec,
				layoutIndex,
				laneIndex);
			outSurface.CorridorId = CorridorSpec.CorridorId;
			outSurface.SegmentId = layoutEntry.SegmentId;
			outSurface.LaneId = laneSpec.LaneId;
			outSurface.SurfaceId = laneSpec.SurfaceId;
			outSurface.RegionType = laneSpec.RegionType;
			outSurface.SurfaceZOffsetCm = laneSpec.SurfaceZOffsetCm;
			outSurface.AlongMeters = alongMeters;
			outSurface.OffsetMeters = offsetMeters;
			return true;
		}
	}

	return false;
}

void AScenarioCorridorRuntimeActor::AddLaneStrip(
	const FScenarioRuntimeCorridorLayoutEntry& layoutEntry,
	const FScenarioRuntimeCorridorLaneSpec& laneSpec)
{
	const double laneWidthMeters = laneSpec.OffsetRangeMeters.MaxMeters - laneSpec.OffsetRangeMeters.MinMeters;
	if (laneWidthMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FScenarioCorridorSurfaceEntry surfaceEntry;
	FScenarioCorridorSurfaceResolver::ResolveSurfaceEntry(laneSpec.SurfaceId, SurfaceCatalog, surfaceEntry);
	UMaterialInterface* material = FScenarioCorridorSurfaceResolver::ResolveSurfaceMaterial(
		surfaceEntry,
		surfaceEntry.GroundRegionType,
		WalkableGroundMaterial.Get(),
		PenaltyGroundMaterial.Get(),
		BlockedGroundMaterial.Get());

	const double centerOffsetCm =
		((laneSpec.OffsetRangeMeters.MinMeters + laneSpec.OffsetRangeMeters.MaxMeters) * 0.5)
		* FScenarioCorridorGeometry::MetersToCentimeters;
	const double laneWidthCm = laneWidthMeters * FScenarioCorridorGeometry::MetersToCentimeters;
	const bool bBlockedSurface = laneSpec.RegionType == EScenarioGroundRegionType::Blocked;
	const double laneHeightCm = bBlockedSurface ? RuntimeBlockedHeightCm : RuntimeSurfaceHeightCm;
	const double laneSurfaceZCm = RuntimeSurfaceTopZCm + laneSpec.SurfaceZOffsetCm;
	const double laneCenterZCm = bBlockedSurface
		? laneSurfaceZCm + (laneHeightCm * 0.5)
		: laneSurfaceZCm - (laneHeightCm * 0.5);
	const double laneHeightScale = laneHeightCm / 100.0;
	const FName collisionProfileName = FScenarioCorridorGeometry::ResolveRuntimeCollisionProfileName(laneSpec.RegionType);

	TArray<FVector> axisLocationsCm;
	if (!FScenarioCorridorGeometry::BuildRuntimeAxisLocationsForAlongRangeCm(
		CorridorSpec,
		layoutEntry.AlongRangeMeters,
		RuntimeSurfaceTopZCm,
		axisLocationsCm))
	{
		return;
	}

	TArray<FVector> axisTangentsCm;
	axisTangentsCm.Reserve(axisLocationsCm.Num());
	for (int32 pointIndex = 0; pointIndex < axisLocationsCm.Num(); ++pointIndex)
	{
		axisTangentsCm.Add(FScenarioCorridorGeometry::ResolveCurveTangentCm(axisLocationsCm, pointIndex));
	}

	const FName collisionTag = laneSpec.CollisionTag.IsEmpty() ? NAME_None : FName(*laneSpec.CollisionTag);
	FScenarioCorridorLaneMeshBuildSpec meshSpec;
	meshSpec.Owner = this;
	meshSpec.AttachParent = SceneRoot;
	meshSpec.LaneStripMesh = LaneStripMesh.Get();
	meshSpec.Material = material;
	meshSpec.ComponentNameBase = FName(*FString::Printf(
		TEXT("RuntimeCorridor_%s_%s"),
		*layoutEntry.SegmentId,
		*laneSpec.LaneId));
	meshSpec.AxisLocationsCm = MoveTemp(axisLocationsCm);
	meshSpec.AxisTangentsCm = MoveTemp(axisTangentsCm);
	meshSpec.CenterOffsetCm = centerOffsetCm;
	meshSpec.LaneWidthCm = laneWidthCm;
	meshSpec.LaneHeightScale = laneHeightScale;
	meshSpec.LaneCenterZCm = laneCenterZCm;
	meshSpec.SurfaceTopZCm = RuntimeSurfaceTopZCm;
	meshSpec.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	meshSpec.CollisionProfileName = collisionProfileName;
	meshSpec.ComponentTag = collisionTag;
	const int32 createdMeshCount = FScenarioCorridorGeometry::AddLaneStripMeshes(meshSpec, LaneMeshComponents);
	if (createdMeshCount > 0 && !collisionTag.IsNone())
	{
		Tags.AddUnique(collisionTag);
	}
}
