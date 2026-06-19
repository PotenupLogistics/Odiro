#include "Scenario/ScenarioCorridorGeometry.h"

#include "Components/SplineMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// Collision profile used by runtime walkable lane meshes.
	const FName WalkableRuntimeCollisionProfileName{ TEXT("Walkable") };
	// Collision profile used by runtime penalty lane meshes.
	const FName PenaltyRuntimeCollisionProfileName{ TEXT("Penalty") };
	// Collision profile used by runtime blocked lane meshes.
	const FName BlockedRuntimeCollisionProfileName{ TEXT("Blocked") };
}

FVector2D FScenarioCorridorGeometry::RotatePointMeters(const FVector2D& pointMeters, double headingDegrees)
{
	const double headingRadians = FMath::DegreesToRadians(headingDegrees);
	const double cosHeading = FMath::Cos(headingRadians);
	const double sinHeading = FMath::Sin(headingRadians);
	return FVector2D(
		(pointMeters.X * cosHeading) - (pointMeters.Y * sinHeading),
		(pointMeters.X * sinHeading) + (pointMeters.Y * cosHeading));
}

FVector FScenarioCorridorGeometry::TransformRuntimeAxisPointMetersToWorldCm(
	const FScenarioRuntimeCorridorSpec& corridorSpec,
	const FVector2D& pointMeters,
	double surfaceTopZCm)
{
	const FVector2D worldPointMeters = corridorSpec.OriginXYMeters + RotatePointMeters(pointMeters, corridorSpec.HeadingDegrees);
	return FVector(
		worldPointMeters.X * MetersToCentimeters,
		worldPointMeters.Y * MetersToCentimeters,
		surfaceTopZCm);
}

FVector2D FScenarioCorridorGeometry::TransformRuntimeWorldCmToAxisPointMeters(
	const FScenarioRuntimeCorridorSpec& corridorSpec,
	const FVector& worldLocation)
{
	const FVector2D worldPointMeters(
		worldLocation.X / MetersToCentimeters,
		worldLocation.Y / MetersToCentimeters);
	return RotatePointMeters(worldPointMeters - corridorSpec.OriginXYMeters, -corridorSpec.HeadingDegrees);
}

FVector FScenarioCorridorGeometry::ResolveCurveTangentCm(const TArray<FVector>& axisLocationsCm, int32 pointIndex)
{
	if (axisLocationsCm.Num() < 2 || !axisLocationsCm.IsValidIndex(pointIndex))
	{
		return FVector::ZeroVector;
	}

	if (pointIndex == 0)
	{
		return axisLocationsCm[1] - axisLocationsCm[0];
	}

	const int32 lastPointIndex = axisLocationsCm.Num() - 1;
	if (pointIndex == lastPointIndex)
	{
		return axisLocationsCm[lastPointIndex] - axisLocationsCm[lastPointIndex - 1];
	}

	const FVector previousSegment = axisLocationsCm[pointIndex] - axisLocationsCm[pointIndex - 1];
	const FVector nextSegment = axisLocationsCm[pointIndex + 1] - axisLocationsCm[pointIndex];
	const FVector blendedDirection = (previousSegment.GetSafeNormal() + nextSegment.GetSafeNormal()).GetSafeNormal();
	if (blendedDirection.IsNearlyZero())
	{
		return nextSegment;
	}

	const double tangentLengthCm = (previousSegment.Size() + nextSegment.Size()) * 0.5;
	return blendedDirection * tangentLengthCm;
}

bool FScenarioCorridorGeometry::TryProjectPointToAxisMeters(
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

bool FScenarioCorridorGeometry::ContainsRangeValue(double value, double minValue, double maxValue, double toleranceMeters)
{
	const double safeMin = FMath::Min(minValue, maxValue) - toleranceMeters;
	const double safeMax = FMath::Max(minValue, maxValue) + toleranceMeters;
	return value >= safeMin && value <= safeMax;
}

FName FScenarioCorridorGeometry::ResolveRuntimeCollisionProfileName(EScenarioGroundRegionType regionType)
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

FString FScenarioCorridorGeometry::MakeSurfaceInstanceId(
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

FString FScenarioCorridorGeometry::MakeVisualLaneKey(const FScenarioRuntimeCorridorLaneSpec& laneSpec)
{
	if (!laneSpec.LaneId.IsEmpty())
	{
		return laneSpec.LaneId;
	}

	return FString::Printf(
		TEXT("lane:%0.3f:%0.3f:%d"),
		laneSpec.OffsetRangeMeters.MinMeters,
		laneSpec.OffsetRangeMeters.MaxMeters,
		static_cast<int32>(laneSpec.RegionType));
}

int32 FScenarioCorridorGeometry::AddLaneStripMeshes(
	const FScenarioCorridorLaneMeshBuildSpec& meshSpec,
	TArray<TObjectPtr<USplineMeshComponent>>& outLaneMeshComponents)
{
	if (!meshSpec.Owner
		|| !meshSpec.AttachParent
		|| !meshSpec.LaneStripMesh
		|| meshSpec.AxisLocationsCm.Num() < 2
		|| meshSpec.AxisLocationsCm.Num() != meshSpec.AxisTangentsCm.Num()
		|| meshSpec.LaneWidthCm <= KINDA_SMALL_NUMBER
		|| meshSpec.LaneHeightScale <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	const FString componentNameBase = meshSpec.ComponentNameBase.IsNone()
		? TEXT("CorridorLane")
		: meshSpec.ComponentNameBase.ToString();
	int32 createdCount = 0;
	for (int32 pointIndex = 0; pointIndex < meshSpec.AxisLocationsCm.Num() - 1; ++pointIndex)
	{
		const FVector startLocation = meshSpec.AxisLocationsCm[pointIndex];
		const FVector endLocation = meshSpec.AxisLocationsCm[pointIndex + 1];
		const FVector startTangent = meshSpec.AxisTangentsCm[pointIndex];
		const FVector endTangent = meshSpec.AxisTangentsCm[pointIndex + 1];
		const FVector startDirection = startTangent.GetSafeNormal();
		const FVector endDirection = endTangent.GetSafeNormal();
		if (startDirection.IsNearlyZero() || endDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector startRight = FVector::CrossProduct(FVector::UpVector, startDirection).GetSafeNormal();
		const FVector endRight = FVector::CrossProduct(FVector::UpVector, endDirection).GetSafeNormal();
		const FVector laneHeightOffset(0.0, 0.0, meshSpec.LaneCenterZCm - meshSpec.SurfaceTopZCm);
		const FName componentName = MakeUniqueObjectName(
			meshSpec.Owner,
			USplineMeshComponent::StaticClass(),
			FName(*FString::Printf(TEXT("%s_%02d"), *componentNameBase, pointIndex)));
		USplineMeshComponent* meshComponent = NewObject<USplineMeshComponent>(meshSpec.Owner, componentName);
		if (!meshComponent)
		{
			continue;
		}

		meshComponent->SetMobility(EComponentMobility::Movable);
		meshComponent->SetupAttachment(meshSpec.AttachParent);
		meshComponent->SetStaticMesh(meshSpec.LaneStripMesh);
		meshComponent->SetForwardAxis(ESplineMeshAxis::X, false);
		meshComponent->SetStartAndEnd(
			startLocation + startRight * meshSpec.CenterOffsetCm + laneHeightOffset,
			startTangent,
			endLocation + endRight * meshSpec.CenterOffsetCm + laneHeightOffset,
			endTangent,
			false);
		meshComponent->SetStartScale(FVector2D(meshSpec.LaneWidthCm / 100.0, meshSpec.LaneHeightScale), false);
		meshComponent->SetEndScale(FVector2D(meshSpec.LaneWidthCm / 100.0, meshSpec.LaneHeightScale), false);
		if (meshSpec.CollisionEnabled != ECollisionEnabled::NoCollision)
		{
			meshComponent->SetCollisionProfileName(meshSpec.CollisionProfileName);
		}
		meshComponent->SetCollisionEnabled(meshSpec.CollisionEnabled);
		meshComponent->SetGenerateOverlapEvents(false);
		meshComponent->SetCastShadow(false);
		if (!meshSpec.ComponentTag.IsNone())
		{
			meshComponent->ComponentTags.AddUnique(meshSpec.ComponentTag);
		}
		if (meshSpec.Material)
		{
			meshComponent->SetMaterial(0, meshSpec.Material);
		}
		meshComponent->RegisterComponent();
		meshComponent->UpdateMesh();
		outLaneMeshComponents.Add(meshComponent);
		++createdCount;
	}

	return createdCount;
}
