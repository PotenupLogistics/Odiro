#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioCorridorRuntime, Log, All);

namespace
{
	const double RuntimeMetersToCentimeters = 100.0;
	const double RuntimeSurfaceTopZCm = 1.0;
	const double RuntimeSurfaceHeightCm = 20.0;
	const double RuntimeBlockedHeightCm = 200.0;
	const double RuntimeSurfaceQueryToleranceMeters = 0.001;
	const FName WalkableRuntimeCollisionProfileName{ TEXT("Walkable") };
	const FName PenaltyRuntimeCollisionProfileName{ TEXT("Penalty") };
	const FName BlockedRuntimeCollisionProfileName{ TEXT("Blocked") };

	FName GetRuntimeCollisionProfileName(EScenarioGroundRegionType regionType)
	{
		switch (regionType)
		{
		case EScenarioGroundRegionType::Penalty:
			return PenaltyRuntimeCollisionProfileName;
		case EScenarioGroundRegionType::Blocked:
			return BlockedRuntimeCollisionProfileName;
		case EScenarioGroundRegionType::Walkable:
		default:
			return WalkableRuntimeCollisionProfileName;
		}
	}

	FVector2D RotateRuntimePoint(const FVector2D& point, double headingDegrees)
	{
		const double headingRadians = FMath::DegreesToRadians(headingDegrees);
		const double cosHeading = FMath::Cos(headingRadians);
		const double sinHeading = FMath::Sin(headingRadians);
		return FVector2D(
			(point.X * cosHeading) - (point.Y * sinHeading),
			(point.X * sinHeading) + (point.Y * cosHeading));
	}

	double GetAxisLengthMeters(const TArray<FVector2D>& pointsMeters)
	{
		double totalMeters = 0.0;
		for (int32 index = 0; index < pointsMeters.Num() - 1; ++index)
		{
			totalMeters += FVector2D::Distance(pointsMeters[index], pointsMeters[index + 1]);
		}
		return totalMeters;
	}

	void AddClippedAxisPoint(TArray<FVector2D>& points, const FVector2D& point)
	{
		if (!points.IsEmpty() && FVector2D::Distance(points.Last(), point) <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		points.Add(point);
	}

	TArray<FVector2D> BuildClippedAxisPointsMeters(
		const TArray<FVector2D>& axisPointsMeters,
		double startAlongMeters,
		double endAlongMeters)
	{
		TArray<FVector2D> clippedPoints;
		if (axisPointsMeters.Num() < 2)
		{
			return clippedPoints;
		}

		const double axisLengthMeters = GetAxisLengthMeters(axisPointsMeters);
		if (axisLengthMeters <= KINDA_SMALL_NUMBER)
		{
			return clippedPoints;
		}

		const double clippedStartMeters = FMath::Clamp(startAlongMeters, 0.0, axisLengthMeters);
		const double clippedEndMeters = FMath::Clamp(endAlongMeters, clippedStartMeters, axisLengthMeters);
		if (clippedEndMeters - clippedStartMeters <= KINDA_SMALL_NUMBER)
		{
			return clippedPoints;
		}

		double accumulatedMeters = 0.0;
		for (int32 index = 0; index < axisPointsMeters.Num() - 1; ++index)
		{
			const FVector2D segmentStart = axisPointsMeters[index];
			const FVector2D segmentEnd = axisPointsMeters[index + 1];
			const FVector2D segmentVector = segmentEnd - segmentStart;
			const double segmentLengthMeters = segmentVector.Size();
			if (segmentLengthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const double segmentStartAlongMeters = accumulatedMeters;
			const double segmentEndAlongMeters = accumulatedMeters + segmentLengthMeters;
			accumulatedMeters = segmentEndAlongMeters;

			if (segmentEndAlongMeters < clippedStartMeters)
			{
				continue;
			}
			if (segmentStartAlongMeters > clippedEndMeters)
			{
				break;
			}

			const double localStartMeters = FMath::Clamp(clippedStartMeters - segmentStartAlongMeters, 0.0, segmentLengthMeters);
			const double localEndMeters = FMath::Clamp(clippedEndMeters - segmentStartAlongMeters, 0.0, segmentLengthMeters);
			if (localEndMeters - localStartMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D segmentDirection = segmentVector / segmentLengthMeters;
			AddClippedAxisPoint(clippedPoints, segmentStart + (segmentDirection * localStartMeters));
			AddClippedAxisPoint(clippedPoints, segmentStart + (segmentDirection * localEndMeters));
		}

		return clippedPoints;
	}

	FVector TransformAxisPointMetersToWorldCm(const FScenarioRuntimeCorridorSpec& corridorSpec, const FVector2D& pointMeters)
	{
		const FVector2D worldPointMeters = corridorSpec.OriginXYMeters + RotateRuntimePoint(pointMeters, corridorSpec.HeadingDegrees);
		return FVector(
			worldPointMeters.X * RuntimeMetersToCentimeters,
			worldPointMeters.Y * RuntimeMetersToCentimeters,
			RuntimeSurfaceTopZCm);
	}

	FVector2D TransformWorldCmToAxisPointMeters(const FScenarioRuntimeCorridorSpec& corridorSpec, const FVector& worldLocation)
	{
		const FVector2D worldPointMeters(
			worldLocation.X / RuntimeMetersToCentimeters,
			worldLocation.Y / RuntimeMetersToCentimeters);
		return RotateRuntimePoint(worldPointMeters - corridorSpec.OriginXYMeters, -corridorSpec.HeadingDegrees);
	}

	bool TryProjectPointToAxisMeters(
		const TArray<FVector2D>& axisPointsMeters,
		const FVector2D& localPointMeters,
		double& outAlongMeters,
		double& outOffsetMeters)
	{
		outAlongMeters = 0.0;
		outOffsetMeters = 0.0;
		if (axisPointsMeters.Num() < 2)
		{
			return false;
		}

		bool bHasProjection = false;
		double accumulatedMeters = 0.0;
		double bestDistanceSquared = TNumericLimits<double>::Max();
		double bestAlongMeters = 0.0;
		double bestOffsetMeters = 0.0;
		for (int32 index = 0; index < axisPointsMeters.Num() - 1; ++index)
		{
			const FVector2D segmentStart = axisPointsMeters[index];
			const FVector2D segmentEnd = axisPointsMeters[index + 1];
			const FVector2D segmentVector = segmentEnd - segmentStart;
			const double segmentLengthMeters = segmentVector.Size();
			if (segmentLengthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D segmentDirection = segmentVector / segmentLengthMeters;
			const double projectedDistanceMeters = FMath::Clamp(
				FVector2D::DotProduct(localPointMeters - segmentStart, segmentDirection),
				0.0,
				segmentLengthMeters);
			const FVector2D projectedPointMeters = segmentStart + (segmentDirection * projectedDistanceMeters);
			const double distanceSquared = FVector2D::DistSquared(localPointMeters, projectedPointMeters);
			if (distanceSquared < bestDistanceSquared)
			{
				const FVector2D offsetDirection(-segmentDirection.Y, segmentDirection.X);
				bestDistanceSquared = distanceSquared;
				bestAlongMeters = accumulatedMeters + projectedDistanceMeters;
				bestOffsetMeters = FVector2D::DotProduct(localPointMeters - projectedPointMeters, offsetDirection);
				bHasProjection = true;
			}

			accumulatedMeters += segmentLengthMeters;
		}

		if (!bHasProjection)
		{
			return false;
		}

		outAlongMeters = bestAlongMeters;
		outOffsetMeters = bestOffsetMeters;
		return true;
	}

	bool ContainsRangeValue(double value, double minValue, double maxValue)
	{
		const double safeMin = FMath::Min(minValue, maxValue) - RuntimeSurfaceQueryToleranceMeters;
		const double safeMax = FMath::Max(minValue, maxValue) + RuntimeSurfaceQueryToleranceMeters;
		return value >= safeMin && value <= safeMax;
	}

	FString MakeSurfaceInstanceId(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const FScenarioRuntimeCorridorLayoutEntry& layoutEntry,
		const FScenarioRuntimeCorridorLaneSpec& laneSpec,
		int32 layoutIndex,
		int32 laneIndex)
	{
		const FString segmentId = layoutEntry.SegmentId.IsEmpty()
			? FString::Printf(TEXT("layout_%03d"), layoutIndex)
			: layoutEntry.SegmentId;
		const FString laneId = laneSpec.LaneId.IsEmpty()
			? FString::Printf(TEXT("lane_%03d"), laneIndex)
			: laneSpec.LaneId;
		const FString corridorId = corridorSpec.CorridorId.IsEmpty()
			? TEXT("corridor")
			: corridorSpec.CorridorId;
		return FString::Printf(TEXT("%s:%s:%s"), *corridorId, *segmentId, *laneId);
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
		TEXT("/Game/Materials/M_ScenarioGroundWalkable.M_ScenarioGroundWalkable"));
	if (walkableGroundMaterialAsset.Succeeded())
	{
		WalkableGroundMaterial = walkableGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> penaltyGroundMaterialAsset(
		TEXT("/Game/Materials/M_ScenarioGroundPenalty.M_ScenarioGroundPenalty"));
	if (penaltyGroundMaterialAsset.Succeeded())
	{
		PenaltyGroundMaterial = penaltyGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> blockedGroundMaterialAsset(
		TEXT("/Game/Materials/M_ScenarioGroundBlock.M_ScenarioGroundBlock"));
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

	for (const FScenarioRuntimeCorridorLayoutEntry& layoutEntry : CorridorSpec.Layout)
	{
		for (const FScenarioRuntimeCorridorLaneSpec& laneSpec : layoutEntry.Lanes)
		{
			AddLaneStrip(layoutEntry, laneSpec);
		}
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
	const FVector2D localPointMeters = TransformWorldCmToAxisPointMeters(CorridorSpec, worldLocation);
	double alongMeters = 0.0;
	double offsetMeters = 0.0;
	if (!TryProjectPointToAxisMeters(CorridorSpec.PointsMeters, localPointMeters, alongMeters, offsetMeters))
	{
		return false;
	}

	for (int32 layoutIndex = 0; layoutIndex < CorridorSpec.Layout.Num(); ++layoutIndex)
	{
		const FScenarioRuntimeCorridorLayoutEntry& layoutEntry = CorridorSpec.Layout[layoutIndex];
		if (!ContainsRangeValue(alongMeters, layoutEntry.AlongRangeMeters.StartMeters, layoutEntry.AlongRangeMeters.EndMeters))
		{
			continue;
		}

		for (int32 laneIndex = 0; laneIndex < layoutEntry.Lanes.Num(); ++laneIndex)
		{
			const FScenarioRuntimeCorridorLaneSpec& laneSpec = layoutEntry.Lanes[laneIndex];
			if (!ContainsRangeValue(offsetMeters, laneSpec.OffsetRangeMeters.MinMeters, laneSpec.OffsetRangeMeters.MaxMeters))
			{
				continue;
			}

			outSurface.SurfaceInstanceId = MakeSurfaceInstanceId(CorridorSpec, layoutEntry, laneSpec, layoutIndex, laneIndex);
			outSurface.CorridorId = CorridorSpec.CorridorId;
			outSurface.SegmentId = layoutEntry.SegmentId;
			outSurface.LaneId = laneSpec.LaneId;
			outSurface.SurfaceId = laneSpec.SurfaceId;
			outSurface.RegionType = laneSpec.RegionType;
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

	const TArray<FVector2D> clippedAxisPointsMeters = BuildClippedAxisPointsMeters(
		CorridorSpec.PointsMeters,
		layoutEntry.AlongRangeMeters.StartMeters,
		layoutEntry.AlongRangeMeters.EndMeters);
	if (clippedAxisPointsMeters.Num() < 2)
	{
		UE_LOG(
			LogScenarioCorridorRuntime,
			Warning,
			TEXT("Runtime corridor lane interval has no renderable axis span. CorridorId: %s | Segment: %s | Lane: %s"),
			*CorridorSpec.CorridorId,
			*layoutEntry.SegmentId,
			*laneSpec.LaneId);
		return;
	}

	FScenarioCorridorSurfaceEntry surfaceEntry;
	ResolveSurfaceEntry(laneSpec.SurfaceId, surfaceEntry);
	UMaterialInterface* material = ResolveSurfaceMaterial(surfaceEntry);

	const double centerOffsetCm = ((laneSpec.OffsetRangeMeters.MinMeters + laneSpec.OffsetRangeMeters.MaxMeters) * 0.5) * RuntimeMetersToCentimeters;
	const double laneWidthCm = laneWidthMeters * RuntimeMetersToCentimeters;
	const bool bBlockedSurface = laneSpec.RegionType == EScenarioGroundRegionType::Blocked;
	const double laneHeightCm = bBlockedSurface ? RuntimeBlockedHeightCm : RuntimeSurfaceHeightCm;
	const double laneCenterZCm = bBlockedSurface
		? RuntimeSurfaceTopZCm + (laneHeightCm * 0.5)
		: RuntimeSurfaceTopZCm - (laneHeightCm * 0.5);
	const double laneHeightScale = laneHeightCm / 100.0;
	const FName collisionProfileName = GetRuntimeCollisionProfileName(laneSpec.RegionType);

	for (int32 pointIndex = 0; pointIndex < clippedAxisPointsMeters.Num() - 1; ++pointIndex)
	{
		const FVector startLocation = TransformAxisPointMetersToWorldCm(CorridorSpec, clippedAxisPointsMeters[pointIndex]);
		const FVector endLocation = TransformAxisPointMetersToWorldCm(CorridorSpec, clippedAxisPointsMeters[pointIndex + 1]);
		const FVector segmentVector = endLocation - startLocation;
		const FVector segmentDirection = segmentVector.GetSafeNormal();
		if (segmentDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector offsetDirection = FVector::CrossProduct(FVector::UpVector, segmentDirection).GetSafeNormal();
		const FVector laneHeightOffset(0.0, 0.0, laneCenterZCm - RuntimeSurfaceTopZCm);
		const FVector startTangent = segmentVector;
		const FVector endTangent = segmentVector;
		const FName componentName = MakeUniqueObjectName(
			this,
			USplineMeshComponent::StaticClass(),
			FName(*FString::Printf(TEXT("RuntimeCorridor_%s_%02d"), *laneSpec.LaneId, pointIndex)));
		USplineMeshComponent* meshComponent = NewObject<USplineMeshComponent>(this, componentName);
		if (!meshComponent)
		{
			continue;
		}

		meshComponent->SetMobility(EComponentMobility::Movable);
		meshComponent->SetupAttachment(SceneRoot);
		meshComponent->SetStaticMesh(LaneStripMesh);
		meshComponent->SetForwardAxis(ESplineMeshAxis::X, false);
		meshComponent->SetStartAndEnd(
			startLocation + offsetDirection * centerOffsetCm + laneHeightOffset,
			startTangent,
			endLocation + offsetDirection * centerOffsetCm + laneHeightOffset,
			endTangent,
			false);
		meshComponent->SetStartScale(FVector2D(laneWidthCm / 100.0, laneHeightScale), false);
		meshComponent->SetEndScale(FVector2D(laneWidthCm / 100.0, laneHeightScale), false);
		meshComponent->SetCollisionProfileName(collisionProfileName);
		meshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		meshComponent->SetGenerateOverlapEvents(false);
		meshComponent->SetCastShadow(false);
		if (material)
		{
			meshComponent->SetMaterial(0, material);
		}
		if (!laneSpec.CollisionTag.IsEmpty())
		{
			const FName collisionTag(*laneSpec.CollisionTag);
			meshComponent->ComponentTags.AddUnique(collisionTag);
			Tags.AddUnique(collisionTag);
		}
		meshComponent->RegisterComponent();
		meshComponent->UpdateMesh();
		LaneMeshComponents.Add(meshComponent);
	}
}

bool AScenarioCorridorRuntimeActor::ResolveSurfaceEntry(
	const FString& surfaceId,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry) const
{
	outSurfaceEntry = FScenarioCorridorSurfaceEntry();
	const FName surfaceName(*surfaceId);
	if (const UScenarioCorridorSurfaceCatalog* loadedCatalog = SurfaceCatalog.LoadSynchronous())
	{
		if (loadedCatalog->FindSurfaceEntryById(surfaceName, outSurfaceEntry))
		{
			return true;
		}
	}
	else if (!SurfaceCatalog.IsNull())
	{
		UE_LOG(
			LogScenarioCorridorRuntime,
			Warning,
			TEXT("Corridor surface catalog could not be loaded. Path: %s"),
			*SurfaceCatalog.ToSoftObjectPath().ToString());
	}

	if (UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(surfaceName, outSurfaceEntry))
	{
		return true;
	}

	UE_LOG(
		LogScenarioCorridorRuntime,
		Warning,
		TEXT("Unknown runtime Corridor surface '%s'; using walkable fallback metadata."),
		surfaceId.IsEmpty() ? TEXT("<empty>") : *surfaceId);
	outSurfaceEntry.SurfaceId = surfaceName;
	outSurfaceEntry.DisplayName = FText::FromString(surfaceId.IsEmpty() ? TEXT("Unknown Surface") : surfaceId);
	outSurfaceEntry.LaneType = EScenarioSampleLaneType::Walkable;
	outSurfaceEntry.GroundRegionType = EScenarioGroundRegionType::Walkable;
	outSurfaceEntry.TraversabilityScore = 1.0;
	return false;
}

UMaterialInterface* AScenarioCorridorRuntimeActor::ResolveSurfaceMaterial(
	const FScenarioCorridorSurfaceEntry& surfaceEntry) const
{
	if (UMaterialInterface* catalogMaterial = surfaceEntry.PreviewMaterial.LoadSynchronous())
	{
		return catalogMaterial;
	}

	if (!surfaceEntry.PreviewMaterial.IsNull())
	{
		UE_LOG(
			LogScenarioCorridorRuntime,
			Warning,
			TEXT("Corridor surface material failed to load. Surface: %s | Path: %s"),
			*surfaceEntry.SurfaceId.ToString(),
			*surfaceEntry.PreviewMaterial.ToSoftObjectPath().ToString());
	}

	return ResolveFallbackSurfaceMaterial(surfaceEntry.GroundRegionType);
}

UMaterialInterface* AScenarioCorridorRuntimeActor::ResolveFallbackSurfaceMaterial(EScenarioGroundRegionType regionType) const
{
	if (regionType == EScenarioGroundRegionType::Blocked)
	{
		return BlockedGroundMaterial ? BlockedGroundMaterial.Get() : WalkableGroundMaterial.Get();
	}

	if (regionType == EScenarioGroundRegionType::Penalty)
	{
		return PenaltyGroundMaterial ? PenaltyGroundMaterial.Get() : WalkableGroundMaterial.Get();
	}

	return WalkableGroundMaterial.Get();
}
