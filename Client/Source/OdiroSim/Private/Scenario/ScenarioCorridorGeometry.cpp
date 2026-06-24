#include "Scenario/ScenarioCorridorGeometry.h"

#include "ProceduralMeshComponent.h"
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

	struct FScenarioCorridorProceduralLaneSample
	{
		// Top vertex on the lower lateral boundary.
		FVector MinTopLocationCm = FVector::ZeroVector;

		// Top vertex on the upper lateral boundary.
		FVector MaxTopLocationCm = FVector::ZeroVector;

		// Bottom vertex on the lower lateral boundary.
		FVector MinBottomLocationCm = FVector::ZeroVector;

		// Bottom vertex on the upper lateral boundary.
		FVector MaxBottomLocationCm = FVector::ZeroVector;

		// Distance from the first sample along the generated mesh.
		double AlongCm = 0.0;
	};

	// Resolves a stable horizontal tangent for one lane sample, falling back to adjacent axis points.
	FVector ResolveScenarioCorridorProceduralDirectionCm(
		const TArray<FVector>& axisLocationsCm,
		const TArray<FVector>& axisTangentsCm,
		int32 pointIndex)
	{
		FVector directionCm = axisTangentsCm.IsValidIndex(pointIndex) ? axisTangentsCm[pointIndex] : FVector::ZeroVector;
		directionCm.Z = 0.0;
		if (!directionCm.IsNearlyZero())
		{
			return directionCm.GetSafeNormal();
		}

		if (axisLocationsCm.IsValidIndex(pointIndex + 1))
		{
			directionCm = axisLocationsCm[pointIndex + 1] - axisLocationsCm[pointIndex];
		}
		else if (axisLocationsCm.IsValidIndex(pointIndex - 1))
		{
			directionCm = axisLocationsCm[pointIndex] - axisLocationsCm[pointIndex - 1];
		}

		directionCm.Z = 0.0;
		return directionCm.GetSafeNormal();
	}

	// Builds top and bottom prism boundary samples from semantic lane offset bounds.
	bool BuildScenarioCorridorProceduralLaneSamples(
		const FScenarioCorridorLaneMeshBuildSpec& meshSpec,
		TArray<FScenarioCorridorProceduralLaneSample>& outSamples)
	{
		outSamples.Reset();
		const double minOffsetCm = FMath::Min(meshSpec.MinOffsetCm, meshSpec.MaxOffsetCm);
		const double maxOffsetCm = FMath::Max(meshSpec.MinOffsetCm, meshSpec.MaxOffsetCm);
		const double topZCm = meshSpec.LaneCenterZCm + (meshSpec.LaneHeightCm * 0.5);
		const double bottomZCm = meshSpec.LaneCenterZCm - (meshSpec.LaneHeightCm * 0.5);
		double accumulatedAlongCm = 0.0;
		outSamples.Reserve(meshSpec.AxisLocationsCm.Num());

		for (int32 pointIndex = 0; pointIndex < meshSpec.AxisLocationsCm.Num(); ++pointIndex)
		{
			if (pointIndex > 0)
			{
				accumulatedAlongCm += FVector::Dist2D(
					meshSpec.AxisLocationsCm[pointIndex - 1],
					meshSpec.AxisLocationsCm[pointIndex]);
			}

			const FVector directionCm = ResolveScenarioCorridorProceduralDirectionCm(
				meshSpec.AxisLocationsCm,
				meshSpec.AxisTangentsCm,
				pointIndex);
			if (directionCm.IsNearlyZero())
			{
				return false;
			}

			const FVector rightCm = FVector::CrossProduct(FVector::UpVector, directionCm).GetSafeNormal();
			if (rightCm.IsNearlyZero())
			{
				return false;
			}

			const FVector minLocationCm = meshSpec.AxisLocationsCm[pointIndex] + (rightCm * minOffsetCm);
			const FVector maxLocationCm = meshSpec.AxisLocationsCm[pointIndex] + (rightCm * maxOffsetCm);

			FScenarioCorridorProceduralLaneSample sample;
			sample.MinTopLocationCm = FVector(minLocationCm.X, minLocationCm.Y, topZCm);
			sample.MaxTopLocationCm = FVector(maxLocationCm.X, maxLocationCm.Y, topZCm);
			sample.MinBottomLocationCm = FVector(minLocationCm.X, minLocationCm.Y, bottomZCm);
			sample.MaxBottomLocationCm = FVector(maxLocationCm.X, maxLocationCm.Y, bottomZCm);
			sample.AlongCm = accumulatedAlongCm;
			outSamples.Add(sample);
		}

		return outSamples.Num() >= 2;
	}

	// Appends one outward-wound quad with shared per-face normal data.
	void AddScenarioCorridorProceduralQuad(
		const FVector& firstLocationCm,
		const FVector& secondLocationCm,
		const FVector& thirdLocationCm,
		const FVector& fourthLocationCm,
		const FVector2D& firstUv,
		const FVector2D& secondUv,
		const FVector2D& thirdUv,
		const FVector2D& fourthUv,
		TArray<FVector>& vertices,
		TArray<int32>& triangles,
		TArray<FVector>& normals,
		TArray<FVector2D>& uv0)
	{
		const int32 firstVertexIndex = vertices.Num();
		const FVector normal = FVector::CrossProduct(
			secondLocationCm - firstLocationCm,
			thirdLocationCm - firstLocationCm).GetSafeNormal();

		vertices.Add(firstLocationCm);
		vertices.Add(secondLocationCm);
		vertices.Add(thirdLocationCm);
		vertices.Add(fourthLocationCm);

		triangles.Add(firstVertexIndex);
		triangles.Add(firstVertexIndex + 1);
		triangles.Add(firstVertexIndex + 2);
		triangles.Add(firstVertexIndex);
		triangles.Add(firstVertexIndex + 2);
		triangles.Add(firstVertexIndex + 3);

		normals.Add(normal);
		normals.Add(normal);
		normals.Add(normal);
		normals.Add(normal);

		uv0.Add(firstUv);
		uv0.Add(secondUv);
		uv0.Add(thirdUv);
		uv0.Add(fourthUv);
	}
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

bool FScenarioCorridorGeometry::BuildRuntimeAxisLocationsForAlongRangeCm(
	const FScenarioRuntimeCorridorSpec& corridorSpec,
	const FScenarioAlongRangeMeters& alongRangeMeters,
	double surfaceTopZCm,
	TArray<FVector>& outAxisLocationsCm)
{
	outAxisLocationsCm.Reset();
	if (corridorSpec.PointsMeters.Num() < 2)
	{
		return false;
	}

	const double requestedStartMeters = FMath::Min(alongRangeMeters.StartMeters, alongRangeMeters.EndMeters);
	const double requestedEndMeters = FMath::Max(alongRangeMeters.StartMeters, alongRangeMeters.EndMeters);
	if (requestedEndMeters - requestedStartMeters <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	auto addAxisPoint = [&outAxisLocationsCm, &corridorSpec, surfaceTopZCm](const FVector2D& pointMeters)
	{
		const FVector worldPointCm = TransformRuntimeAxisPointMetersToWorldCm(corridorSpec, pointMeters, surfaceTopZCm);
		if (outAxisLocationsCm.IsEmpty() || !outAxisLocationsCm.Last().Equals(worldPointCm, KINDA_SMALL_NUMBER))
		{
			outAxisLocationsCm.Add(worldPointCm);
		}
	};

	double accumulatedMeters = 0.0;
	for (int32 index = 0; index < corridorSpec.PointsMeters.Num() - 1; ++index)
	{
		const FVector2D segmentStart = corridorSpec.PointsMeters[index];
		const FVector2D segmentEnd = corridorSpec.PointsMeters[index + 1];
		const FVector2D segmentVector = segmentEnd - segmentStart;
		const double segmentLengthMeters = segmentVector.Size();
		if (segmentLengthMeters <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const double segmentStartAlongMeters = accumulatedMeters;
		const double segmentEndAlongMeters = accumulatedMeters + segmentLengthMeters;
		accumulatedMeters = segmentEndAlongMeters;
		if (requestedEndMeters < segmentStartAlongMeters - SurfaceQueryToleranceMeters
			|| requestedStartMeters > segmentEndAlongMeters + SurfaceQueryToleranceMeters)
		{
			continue;
		}

		const double overlapStartMeters =
			FMath::Clamp(requestedStartMeters, segmentStartAlongMeters, segmentEndAlongMeters);
		const double overlapEndMeters =
			FMath::Clamp(requestedEndMeters, segmentStartAlongMeters, segmentEndAlongMeters);
		if (overlapEndMeters - overlapStartMeters <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector2D segmentDirection = segmentVector / segmentLengthMeters;
		const FVector2D overlapStartPoint =
			segmentStart + segmentDirection * (overlapStartMeters - segmentStartAlongMeters);
		const FVector2D overlapEndPoint =
			segmentStart + segmentDirection * (overlapEndMeters - segmentStartAlongMeters);
		addAxisPoint(overlapStartPoint);
		addAxisPoint(overlapEndPoint);
	}

	return outAxisLocationsCm.Num() >= 2;
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

bool FScenarioCorridorGeometry::IsCurbSideLaneId(const FString& laneId)
{
	const FString normalizedLaneId = laneId.ToLower();
	return normalizedLaneId == TEXT("curb_edge")
		|| normalizedLaneId.StartsWith(TEXT("curb_"))
		|| normalizedLaneId.StartsWith(TEXT("curbside"));
}

double FScenarioCorridorGeometry::ResolveLaneSurfaceZOffsetCm(const FString& laneId)
{
	return IsCurbSideLaneId(laneId) ? DefaultCurbSideSurfaceZOffsetCm : 0.0;
}

double FScenarioCorridorGeometry::ResolveSurfaceZOffsetForOffsetMeters(
	double offsetMeters,
	double halfWalkwayWidthMeters,
	double curbSideWidthMeters)
{
	if (offsetMeters <= halfWalkwayWidthMeters + KINDA_SMALL_NUMBER)
	{
		return 0.0;
	}

	return curbSideWidthMeters > KINDA_SMALL_NUMBER ? DefaultCurbSideSurfaceZOffsetCm : 0.0;
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

int32 FScenarioCorridorGeometry::AddLaneStripMeshes(
	const FScenarioCorridorLaneMeshBuildSpec& meshSpec,
	TArray<TObjectPtr<UProceduralMeshComponent>>& outLaneMeshComponents)
{
	if (!meshSpec.Owner
		|| !meshSpec.AttachParent
		|| meshSpec.AxisLocationsCm.Num() < 2
		|| meshSpec.AxisLocationsCm.Num() != meshSpec.AxisTangentsCm.Num()
		|| FMath::Abs(meshSpec.MaxOffsetCm - meshSpec.MinOffsetCm) <= KINDA_SMALL_NUMBER
		|| meshSpec.LaneHeightCm <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	const FString componentNameBase = meshSpec.ComponentNameBase.IsNone()
		? TEXT("CorridorLane")
		: meshSpec.ComponentNameBase.ToString();

	TArray<FScenarioCorridorProceduralLaneSample> samples;
	if (!BuildScenarioCorridorProceduralLaneSamples(meshSpec, samples))
	{
		return 0;
	}

	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector> normals;
	TArray<FVector2D> uv0;
	const int32 segmentCount = samples.Num() - 1;
	const int32 estimatedVertexCount = (segmentCount * 16) + 8;
	vertices.Reserve(estimatedVertexCount);
	triangles.Reserve((segmentCount * 24) + 12);
	normals.Reserve(estimatedVertexCount);
	uv0.Reserve(estimatedVertexCount);

	for (int32 segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
	{
		const FScenarioCorridorProceduralLaneSample& startSample = samples[segmentIndex];
		const FScenarioCorridorProceduralLaneSample& endSample = samples[segmentIndex + 1];
		const double startU = startSample.AlongCm / MetersToCentimeters;
		const double endU = endSample.AlongCm / MetersToCentimeters;

		AddScenarioCorridorProceduralQuad(
			startSample.MinTopLocationCm,
			endSample.MinTopLocationCm,
			endSample.MaxTopLocationCm,
			startSample.MaxTopLocationCm,
			FVector2D(startU, 0.0),
			FVector2D(endU, 0.0),
			FVector2D(endU, 1.0),
			FVector2D(startU, 1.0),
			vertices,
			triangles,
			normals,
			uv0);
		AddScenarioCorridorProceduralQuad(
			startSample.MinBottomLocationCm,
			startSample.MaxBottomLocationCm,
			endSample.MaxBottomLocationCm,
			endSample.MinBottomLocationCm,
			FVector2D(startU, 0.0),
			FVector2D(startU, 1.0),
			FVector2D(endU, 1.0),
			FVector2D(endU, 0.0),
			vertices,
			triangles,
			normals,
			uv0);
		AddScenarioCorridorProceduralQuad(
			startSample.MinBottomLocationCm,
			endSample.MinBottomLocationCm,
			endSample.MinTopLocationCm,
			startSample.MinTopLocationCm,
			FVector2D(startU, 1.0),
			FVector2D(endU, 1.0),
			FVector2D(endU, 0.0),
			FVector2D(startU, 0.0),
			vertices,
			triangles,
			normals,
			uv0);
		AddScenarioCorridorProceduralQuad(
			startSample.MaxBottomLocationCm,
			startSample.MaxTopLocationCm,
			endSample.MaxTopLocationCm,
			endSample.MaxBottomLocationCm,
			FVector2D(startU, 1.0),
			FVector2D(startU, 0.0),
			FVector2D(endU, 0.0),
			FVector2D(endU, 1.0),
			vertices,
			triangles,
			normals,
			uv0);
	}

	const FScenarioCorridorProceduralLaneSample& firstSample = samples[0];
	const FScenarioCorridorProceduralLaneSample& lastSample = samples.Last();
	const double lastU = lastSample.AlongCm / MetersToCentimeters;
	AddScenarioCorridorProceduralQuad(
		firstSample.MinBottomLocationCm,
		firstSample.MinTopLocationCm,
		firstSample.MaxTopLocationCm,
		firstSample.MaxBottomLocationCm,
		FVector2D(0.0, 1.0),
		FVector2D(0.0, 0.0),
		FVector2D(1.0, 0.0),
		FVector2D(1.0, 1.0),
		vertices,
		triangles,
		normals,
		uv0);
	AddScenarioCorridorProceduralQuad(
		lastSample.MinBottomLocationCm,
		lastSample.MaxBottomLocationCm,
		lastSample.MaxTopLocationCm,
		lastSample.MinTopLocationCm,
		FVector2D(lastU, 1.0),
		FVector2D(lastU + 1.0, 1.0),
		FVector2D(lastU + 1.0, 0.0),
		FVector2D(lastU, 0.0),
		vertices,
		triangles,
		normals,
		uv0);

	const FName componentName = MakeUniqueObjectName(
		meshSpec.Owner,
		UProceduralMeshComponent::StaticClass(),
		FName(*componentNameBase));
	UProceduralMeshComponent* meshComponent = NewObject<UProceduralMeshComponent>(meshSpec.Owner, componentName);
	if (!meshComponent)
	{
		return 0;
	}

	meshComponent->SetMobility(EComponentMobility::Movable);
	meshComponent->SetupAttachment(meshSpec.AttachParent);
	meshComponent->bUseAsyncCooking = true;
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
	meshComponent->RegisterComponent();

	TArray<FLinearColor> vertexColors;
	TArray<FProcMeshTangent> tangents;
	const bool bCreateCollision = meshSpec.CollisionEnabled != ECollisionEnabled::NoCollision;
	meshComponent->CreateMeshSection_LinearColor(
		0,
		vertices,
		triangles,
		normals,
		uv0,
		vertexColors,
		tangents,
		bCreateCollision);
	if (meshSpec.Material)
	{
		meshComponent->SetMaterial(0, meshSpec.Material);
	}

	outLaneMeshComponents.Add(meshComponent);
	return 1;
}
