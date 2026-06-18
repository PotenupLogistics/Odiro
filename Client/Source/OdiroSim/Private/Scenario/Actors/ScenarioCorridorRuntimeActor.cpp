#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
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

	// Mesh buffers for one runtime corridor lane prism.
	struct FRuntimeLanePrismMesh
	{
		// Component-local prism vertices in Unreal centimeters.
		TArray<FVector> Vertices;

		// Triangle index buffer for visible and collision geometry.
		TArray<int32> Triangles;

		// Per-vertex normals used by runtime ground materials.
		TArray<FVector> Normals;

		// Simple generated UVs for material sampling.
		TArray<FVector2D> UV0;

		// Per-vertex tangents aligned to the nearest corridor segment.
		TArray<FProcMeshTangent> Tangents;
	};

	double CrossRuntime2D(const FVector2D& lhs, const FVector2D& rhs)
	{
		return (lhs.X * rhs.Y) - (lhs.Y * rhs.X);
	}

	FVector2D GetRuntimeLeftNormal2D(const FVector2D& direction)
	{
		return FVector2D(-direction.Y, direction.X).GetSafeNormal();
	}

	bool TryIntersectRuntimeLines2D(
		const FVector2D& firstPoint,
		const FVector2D& firstDirection,
		const FVector2D& secondPoint,
		const FVector2D& secondDirection,
		FVector2D& outIntersection)
	{
		const double determinant = CrossRuntime2D(firstDirection, secondDirection);
		if (FMath::Abs(determinant) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const double firstDistance = CrossRuntime2D(secondPoint - firstPoint, secondDirection) / determinant;
		outIntersection = firstPoint + (firstDirection * firstDistance);
		return true;
	}

	TArray<FVector2D> BuildOffsetAxisPointsMeters(const TArray<FVector2D>& axisPointsMeters, double offsetMeters)
	{
		TArray<FVector2D> offsetPointsMeters;
		offsetPointsMeters.Reserve(axisPointsMeters.Num());
		if (axisPointsMeters.Num() < 2)
		{
			return offsetPointsMeters;
		}

		for (int32 pointIndex = 0; pointIndex < axisPointsMeters.Num(); ++pointIndex)
		{
			if (pointIndex == 0)
			{
				const FVector2D firstDirection = (axisPointsMeters[1] - axisPointsMeters[0]).GetSafeNormal();
				offsetPointsMeters.Add(axisPointsMeters[0] + (GetRuntimeLeftNormal2D(firstDirection) * offsetMeters));
				continue;
			}

			if (pointIndex == axisPointsMeters.Num() - 1)
			{
				const FVector2D lastDirection = (axisPointsMeters[pointIndex] - axisPointsMeters[pointIndex - 1]).GetSafeNormal();
				offsetPointsMeters.Add(axisPointsMeters[pointIndex] + (GetRuntimeLeftNormal2D(lastDirection) * offsetMeters));
				continue;
			}

			const FVector2D previousDirection = (axisPointsMeters[pointIndex] - axisPointsMeters[pointIndex - 1]).GetSafeNormal();
			const FVector2D nextDirection = (axisPointsMeters[pointIndex + 1] - axisPointsMeters[pointIndex]).GetSafeNormal();
			const FVector2D previousNormal = GetRuntimeLeftNormal2D(previousDirection);
			const FVector2D nextNormal = GetRuntimeLeftNormal2D(nextDirection);
			const FVector2D previousOffsetPoint = axisPointsMeters[pointIndex] + (previousNormal * offsetMeters);
			const FVector2D nextOffsetPoint = axisPointsMeters[pointIndex] + (nextNormal * offsetMeters);
			FVector2D intersectionPoint;
			if (TryIntersectRuntimeLines2D(previousOffsetPoint, previousDirection, nextOffsetPoint, nextDirection, intersectionPoint))
			{
				offsetPointsMeters.Add(intersectionPoint);
				continue;
			}

			const FVector2D averagedNormal = (previousNormal + nextNormal).GetSafeNormal();
			offsetPointsMeters.Add(axisPointsMeters[pointIndex] + ((averagedNormal.IsNearlyZero() ? previousNormal : averagedNormal) * offsetMeters));
		}

		return offsetPointsMeters;
	}

	FVector TransformAxisOffsetPointMetersToWorldCm(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const FVector2D& pointMeters,
		double zCm)
	{
		FVector worldPoint = TransformAxisPointMetersToWorldCm(corridorSpec, pointMeters);
		worldPoint.Z = zCm;
		return worldPoint;
	}

	void AppendRuntimePrismQuad(
		FRuntimeLanePrismMesh& mesh,
		const FVector& first,
		const FVector& second,
		const FVector& third,
		const FVector& fourth,
		const FVector& normal,
		const FVector& tangent)
	{
		const int32 baseIndex = mesh.Vertices.Num();
		const FVector safeNormal = normal.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : normal.GetSafeNormal();
		const FVector safeTangent = tangent.GetSafeNormal().IsNearlyZero() ? FVector::ForwardVector : tangent.GetSafeNormal();
		mesh.Vertices.Add(first);
		mesh.Vertices.Add(second);
		mesh.Vertices.Add(third);
		mesh.Vertices.Add(fourth);
		for (int32 vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
		{
			mesh.Normals.Add(safeNormal);
			mesh.Tangents.Add(FProcMeshTangent(safeTangent, false));
		}
		mesh.UV0.Add(FVector2D(0.0, 0.0));
		mesh.UV0.Add(FVector2D(1.0, 0.0));
		mesh.UV0.Add(FVector2D(1.0, 1.0));
		mesh.UV0.Add(FVector2D(0.0, 1.0));

		mesh.Triangles.Add(baseIndex);
		mesh.Triangles.Add(baseIndex + 1);
		mesh.Triangles.Add(baseIndex + 2);
		mesh.Triangles.Add(baseIndex);
		mesh.Triangles.Add(baseIndex + 2);
		mesh.Triangles.Add(baseIndex + 3);
		mesh.Triangles.Add(baseIndex + 2);
		mesh.Triangles.Add(baseIndex + 1);
		mesh.Triangles.Add(baseIndex);
		mesh.Triangles.Add(baseIndex + 3);
		mesh.Triangles.Add(baseIndex + 2);
		mesh.Triangles.Add(baseIndex);
	}

	bool BuildRuntimeLanePrismMeshCm(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const TArray<FVector2D>& axisPointsMeters,
		double minOffsetMeters,
		double maxOffsetMeters,
		double bottomZCm,
		double topZCm,
		FRuntimeLanePrismMesh& outMesh)
	{
		outMesh = FRuntimeLanePrismMesh();
		if (axisPointsMeters.Num() < 2)
		{
			return false;
		}

		const TArray<FVector2D> minEdgePointsMeters = BuildOffsetAxisPointsMeters(axisPointsMeters, minOffsetMeters);
		const TArray<FVector2D> maxEdgePointsMeters = BuildOffsetAxisPointsMeters(axisPointsMeters, maxOffsetMeters);
		if (minEdgePointsMeters.Num() != axisPointsMeters.Num() || maxEdgePointsMeters.Num() != axisPointsMeters.Num())
		{
			return false;
		}

		const double lowerZCm = FMath::Min(bottomZCm, topZCm);
		const double upperZCm = FMath::Max(bottomZCm, topZCm);
		TArray<FVector> topMinPoints;
		TArray<FVector> topMaxPoints;
		TArray<FVector> bottomMinPoints;
		TArray<FVector> bottomMaxPoints;
		topMinPoints.Reserve(axisPointsMeters.Num());
		topMaxPoints.Reserve(axisPointsMeters.Num());
		bottomMinPoints.Reserve(axisPointsMeters.Num());
		bottomMaxPoints.Reserve(axisPointsMeters.Num());
		for (int32 pointIndex = 0; pointIndex < axisPointsMeters.Num(); ++pointIndex)
		{
			topMinPoints.Add(TransformAxisOffsetPointMetersToWorldCm(corridorSpec, minEdgePointsMeters[pointIndex], upperZCm));
			topMaxPoints.Add(TransformAxisOffsetPointMetersToWorldCm(corridorSpec, maxEdgePointsMeters[pointIndex], upperZCm));
			bottomMinPoints.Add(TransformAxisOffsetPointMetersToWorldCm(corridorSpec, minEdgePointsMeters[pointIndex], lowerZCm));
			bottomMaxPoints.Add(TransformAxisOffsetPointMetersToWorldCm(corridorSpec, maxEdgePointsMeters[pointIndex], lowerZCm));
		}

		for (int32 pointIndex = 0; pointIndex < axisPointsMeters.Num() - 1; ++pointIndex)
		{
			const FVector segmentTangent = (topMinPoints[pointIndex + 1] - topMinPoints[pointIndex]).GetSafeNormal();
			if (segmentTangent.IsNearlyZero())
			{
				continue;
			}

			const FVector minSideNormal = FVector::CrossProduct(segmentTangent, FVector::UpVector).GetSafeNormal();
			const FVector maxSideNormal = FVector::CrossProduct(FVector::UpVector, segmentTangent).GetSafeNormal();
			AppendRuntimePrismQuad(
				outMesh,
				topMinPoints[pointIndex],
				topMinPoints[pointIndex + 1],
				topMaxPoints[pointIndex + 1],
				topMaxPoints[pointIndex],
				FVector::UpVector,
				segmentTangent);
			AppendRuntimePrismQuad(
				outMesh,
				bottomMinPoints[pointIndex],
				bottomMaxPoints[pointIndex],
				bottomMaxPoints[pointIndex + 1],
				bottomMinPoints[pointIndex + 1],
				-FVector::UpVector,
				segmentTangent);
			AppendRuntimePrismQuad(
				outMesh,
				topMinPoints[pointIndex],
				bottomMinPoints[pointIndex],
				bottomMinPoints[pointIndex + 1],
				topMinPoints[pointIndex + 1],
				minSideNormal,
				segmentTangent);
			AppendRuntimePrismQuad(
				outMesh,
				topMaxPoints[pointIndex],
				topMaxPoints[pointIndex + 1],
				bottomMaxPoints[pointIndex + 1],
				bottomMaxPoints[pointIndex],
				maxSideNormal,
				segmentTangent);
		}

		const FVector startTangent = (topMinPoints[1] - topMinPoints[0]).GetSafeNormal();
		const int32 lastPointIndex = topMinPoints.Num() - 1;
		const FVector endTangent = (topMinPoints[lastPointIndex] - topMinPoints[lastPointIndex - 1]).GetSafeNormal();
		AppendRuntimePrismQuad(
			outMesh,
			topMinPoints[0],
			topMaxPoints[0],
			bottomMaxPoints[0],
			bottomMinPoints[0],
			-startTangent,
			FVector::CrossProduct(FVector::UpVector, startTangent).GetSafeNormal());
		AppendRuntimePrismQuad(
			outMesh,
			topMinPoints[lastPointIndex],
			bottomMinPoints[lastPointIndex],
			bottomMaxPoints[lastPointIndex],
			topMaxPoints[lastPointIndex],
			endTangent,
			FVector::CrossProduct(FVector::UpVector, endTangent).GetSafeNormal());

		return !outMesh.Vertices.IsEmpty() && !outMesh.Triangles.IsEmpty();
	}
}

AScenarioCorridorRuntimeActor::AScenarioCorridorRuntimeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SurfaceCatalog = UScenarioCorridorSurfaceCatalog::MakeDefaultCatalogReference();

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

	if (CorridorSpec.PointsMeters.Num() < 2)
	{
		UE_LOG(
			LogScenarioCorridorRuntime,
			Warning,
			TEXT("Runtime corridor cannot render. CorridorId: %s | Points: %d"),
			*CorridorSpec.CorridorId,
			CorridorSpec.PointsMeters.Num());
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
	for (const TObjectPtr<UProceduralMeshComponent>& meshComponent : LaneMeshComponents)
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

	const bool bBlockedSurface = laneSpec.RegionType == EScenarioGroundRegionType::Blocked;
	const double laneHeightCm = bBlockedSurface ? RuntimeBlockedHeightCm : RuntimeSurfaceHeightCm;
	const double laneSurfaceZCm = RuntimeSurfaceTopZCm + laneSpec.SurfaceZOffsetCm;
	const double laneBottomZCm = bBlockedSurface ? laneSurfaceZCm : laneSurfaceZCm - laneHeightCm;
	const double laneTopZCm = bBlockedSurface ? laneSurfaceZCm + laneHeightCm : laneSurfaceZCm;
	const FName collisionProfileName = GetRuntimeCollisionProfileName(laneSpec.RegionType);

	FRuntimeLanePrismMesh prismMesh;
	if (!BuildRuntimeLanePrismMeshCm(
			CorridorSpec,
			clippedAxisPointsMeters,
			laneSpec.OffsetRangeMeters.MinMeters,
			laneSpec.OffsetRangeMeters.MaxMeters,
			laneBottomZCm,
			laneTopZCm,
			prismMesh))
	{
		UE_LOG(
			LogScenarioCorridorRuntime,
			Warning,
			TEXT("Runtime corridor lane prism mesh failed. CorridorId: %s | Segment: %s | Lane: %s"),
			*CorridorSpec.CorridorId,
			*layoutEntry.SegmentId,
			*laneSpec.LaneId);
		return;
	}

	const FName componentName = MakeUniqueObjectName(
		this,
		UProceduralMeshComponent::StaticClass(),
		FName(*FString::Printf(TEXT("RuntimeCorridor_%s"), *laneSpec.LaneId)));
	UProceduralMeshComponent* meshComponent = NewObject<UProceduralMeshComponent>(this, componentName);
	if (!meshComponent)
	{
		return;
	}

	meshComponent->SetMobility(EComponentMobility::Movable);
	meshComponent->SetupAttachment(SceneRoot);
	meshComponent->SetCollisionProfileName(collisionProfileName);
	meshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	meshComponent->SetGenerateOverlapEvents(false);
	meshComponent->SetCastShadow(false);
	meshComponent->bUseComplexAsSimpleCollision = true;
	meshComponent->bUseAsyncCooking = false;
	if (!laneSpec.CollisionTag.IsEmpty())
	{
		const FName collisionTag(*laneSpec.CollisionTag);
		meshComponent->ComponentTags.AddUnique(collisionTag);
		Tags.AddUnique(collisionTag);
	}
	meshComponent->RegisterComponent();
	meshComponent->CreateMeshSection_LinearColor(
		0,
		prismMesh.Vertices,
		prismMesh.Triangles,
		prismMesh.Normals,
		prismMesh.UV0,
		TArray<FLinearColor>(),
		prismMesh.Tangents,
		true);
	if (material)
	{
		meshComponent->SetMaterial(0, material);
	}
	LaneMeshComponents.Add(meshComponent);
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
		UE_LOG(
			LogScenarioCorridorRuntime,
			Log,
			TEXT("Using runtime Corridor surface material. Surface: %s | Material: %s"),
			*surfaceEntry.SurfaceId.ToString(),
			*catalogMaterial->GetPathName());
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

	UMaterialInterface* fallbackMaterial = ResolveFallbackSurfaceMaterial(surfaceEntry.GroundRegionType);
	UE_LOG(
		LogScenarioCorridorRuntime,
		Log,
		TEXT("Using runtime Corridor fallback material. Surface: %s | RegionType: %d | Material: %s"),
		*surfaceEntry.SurfaceId.ToString(),
		static_cast<int32>(surfaceEntry.GroundRegionType),
		fallbackMaterial ? *fallbackMaterial->GetPathName() : TEXT("<null>"));
	return fallbackMaterial;
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
