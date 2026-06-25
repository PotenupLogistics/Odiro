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
	// Maximum curve chord length used when approximating the old spline-mesh bend.
	constexpr double MaxProceduralLaneCurveStepCm = 25.0;
	// Hard cap that prevents unusually long segments from generating excessive mesh sections.
	constexpr int32 MaxProceduralLaneCurveSubdivisions = 256;

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

	// Converts a world-centimeter vector into a 2D point for corridor plane operations.
	FVector2D ToScenarioCorridorPoint2D(const FVector& locationCm)
	{
		return FVector2D(locationCm.X, locationCm.Y);
	}

	// Converts a 2D corridor point back to a world-centimeter vector at the supplied height.
	FVector ToScenarioCorridorPoint3D(const FVector2D& locationCm, double zCm)
	{
		return FVector(locationCm.X, locationCm.Y, zCm);
	}

	// Returns the signed 2D cross product used by line intersection and turn tests.
	double CrossScenarioCorridor2D(const FVector2D& first, const FVector2D& second)
	{
		return (first.X * second.Y) - (first.Y * second.X);
	}

	// Resolves one horizontal segment direction from sampled corridor axis points.
	bool ResolveScenarioCorridorSegmentDirectionCm(
		const TArray<FVector>& axisLocationsCm,
		int32 segmentIndex,
		FVector2D& outDirectionCm)
	{
		outDirectionCm = FVector2D::ZeroVector;
		if (!axisLocationsCm.IsValidIndex(segmentIndex) || !axisLocationsCm.IsValidIndex(segmentIndex + 1))
		{
			return false;
		}

		outDirectionCm = ToScenarioCorridorPoint2D(axisLocationsCm[segmentIndex + 1])
			- ToScenarioCorridorPoint2D(axisLocationsCm[segmentIndex]);
		if (outDirectionCm.IsNearlyZero())
		{
			return false;
		}

		outDirectionCm.Normalize();
		return true;
	}

	// Resolves the right side of a normalized 2D corridor direction.
	FVector2D ResolveScenarioCorridorRightCm(const FVector2D& directionCm)
	{
		return FVector2D(-directionCm.Y, directionCm.X).GetSafeNormal();
	}

	// Builds top and bottom prism boundary samples from semantic lane offset bounds.
	bool BuildScenarioCorridorProceduralLaneSamples(
		const FScenarioCorridorLaneMeshBuildSpec& meshSpec,
		TArray<FScenarioCorridorProceduralLaneSample>& outSamples)
	{
		outSamples.Reset();
		if (meshSpec.AxisLocationsCm.Num() < 2)
		{
			return false;
		}

		const double minOffsetCm = FMath::Min(meshSpec.MinOffsetCm, meshSpec.MaxOffsetCm);
		const double maxOffsetCm = FMath::Max(meshSpec.MinOffsetCm, meshSpec.MaxOffsetCm);
		const double topZCm = meshSpec.LaneCenterZCm + (meshSpec.LaneHeightCm * 0.5);
		const double bottomZCm = meshSpec.LaneCenterZCm - (meshSpec.LaneHeightCm * 0.5);
		const double maxOffsetMagnitudeCm = FMath::Max(FMath::Abs(minOffsetCm), FMath::Abs(maxOffsetCm));
		const double cornerFilletRadiusCm = FMath::Max(meshSpec.CornerFilletRadiusCm, 0.0);
		outSamples.Reserve(meshSpec.AxisLocationsCm.Num() * 4);

		auto addLaneSample = [&outSamples, minOffsetCm, maxOffsetCm, topZCm, bottomZCm](
			const FVector& centerLocationCm,
			const FVector2D& rightCm,
			double alongCm) -> bool
		{
			const FVector2D safeRightCm = rightCm.GetSafeNormal();
			if (safeRightCm.IsNearlyZero())
			{
				return false;
			}

			const FVector2D centerPointCm = ToScenarioCorridorPoint2D(centerLocationCm);
			const FVector2D minLocationCm = centerPointCm + (safeRightCm * minOffsetCm);
			const FVector2D maxLocationCm = centerPointCm + (safeRightCm * maxOffsetCm);
			if ((maxLocationCm - minLocationCm).IsNearlyZero())
			{
				return false;
			}

			FScenarioCorridorProceduralLaneSample sample;
			sample.MinTopLocationCm = ToScenarioCorridorPoint3D(minLocationCm, topZCm);
			sample.MaxTopLocationCm = ToScenarioCorridorPoint3D(maxLocationCm, topZCm);
			sample.MinBottomLocationCm = ToScenarioCorridorPoint3D(minLocationCm, bottomZCm);
			sample.MaxBottomLocationCm = ToScenarioCorridorPoint3D(maxLocationCm, bottomZCm);
			sample.AlongCm = alongCm;
			outSamples.Add(sample);
			return true;
		};

		auto evaluateQuadraticCenterCm = [](
			const FVector2D& startCm,
			const FVector2D& controlCm,
			const FVector2D& endCm,
			double alpha)
		{
			const double inverseAlpha = 1.0 - alpha;
			return (startCm * FMath::Square(inverseAlpha))
				+ (controlCm * (2.0 * inverseAlpha * alpha))
				+ (endCm * FMath::Square(alpha));
		};

		auto evaluateQuadraticTangentCm = [](
			const FVector2D& startCm,
			const FVector2D& controlCm,
			const FVector2D& endCm,
			double alpha)
		{
			return ((controlCm - startCm) * (2.0 * (1.0 - alpha)))
				+ ((endCm - controlCm) * (2.0 * alpha));
		};

		FVector2D firstDirectionCm;
		if (!ResolveScenarioCorridorSegmentDirectionCm(meshSpec.AxisLocationsCm, 0, firstDirectionCm)
			|| !addLaneSample(
				meshSpec.AxisLocationsCm[0],
				ResolveScenarioCorridorRightCm(firstDirectionCm),
				0.0))
		{
			return false;
		}

		double visualAlongCm = 0.0;
		FVector2D lastCenterCm = ToScenarioCorridorPoint2D(meshSpec.AxisLocationsCm[0]);
		for (int32 pointIndex = 1; pointIndex < meshSpec.AxisLocationsCm.Num() - 1; ++pointIndex)
		{
			FVector2D previousDirectionCm;
			FVector2D nextDirectionCm;
			if (!ResolveScenarioCorridorSegmentDirectionCm(meshSpec.AxisLocationsCm, pointIndex - 1, previousDirectionCm)
				|| !ResolveScenarioCorridorSegmentDirectionCm(meshSpec.AxisLocationsCm, pointIndex, nextDirectionCm))
			{
				return false;
			}

			const double turnAngleRadians = FMath::Atan2(
				CrossScenarioCorridor2D(previousDirectionCm, nextDirectionCm),
				FVector2D::DotProduct(previousDirectionCm, nextDirectionCm));
			if (FMath::IsNearlyZero(turnAngleRadians, KINDA_SMALL_NUMBER)
				|| cornerFilletRadiusCm <= KINDA_SMALL_NUMBER)
			{
				const FVector2D cornerCenterCm = ToScenarioCorridorPoint2D(meshSpec.AxisLocationsCm[pointIndex]);
				visualAlongCm += FVector2D::Distance(lastCenterCm, cornerCenterCm);
				if (!addLaneSample(
						ToScenarioCorridorPoint3D(cornerCenterCm, meshSpec.AxisLocationsCm[pointIndex].Z),
						ResolveScenarioCorridorRightCm(nextDirectionCm),
						visualAlongCm))
				{
					return false;
				}
				lastCenterCm = cornerCenterCm;
				continue;
			}

			const double absTurnAngleRadians = FMath::Abs(turnAngleRadians);
			const FVector2D cornerCenterCm = ToScenarioCorridorPoint2D(meshSpec.AxisLocationsCm[pointIndex]);
			const double previousSegmentLengthCm = FVector2D::Distance(
				ToScenarioCorridorPoint2D(meshSpec.AxisLocationsCm[pointIndex - 1]),
				cornerCenterCm);
			const double nextSegmentLengthCm = FVector2D::Distance(
				cornerCenterCm,
				ToScenarioCorridorPoint2D(meshSpec.AxisLocationsCm[pointIndex + 1]));
			const double maxSetbackCm = FMath::Min(previousSegmentLengthCm, nextSegmentLengthCm) * 0.45;
			const double idealSetbackCm = cornerFilletRadiusCm * FMath::Tan(absTurnAngleRadians * 0.5);
			const double setbackCm = FMath::Min(idealSetbackCm, maxSetbackCm);
			if (setbackCm <= KINDA_SMALL_NUMBER)
			{
				visualAlongCm += FVector2D::Distance(lastCenterCm, cornerCenterCm);
				if (!addLaneSample(
						ToScenarioCorridorPoint3D(cornerCenterCm, meshSpec.AxisLocationsCm[pointIndex].Z),
						ResolveScenarioCorridorRightCm(nextDirectionCm),
						visualAlongCm))
				{
					return false;
				}
				lastCenterCm = cornerCenterCm;
				continue;
			}

			const FVector2D filletStartCm = cornerCenterCm - (previousDirectionCm * setbackCm);
			const FVector2D filletEndCm = cornerCenterCm + (nextDirectionCm * setbackCm);
			visualAlongCm += FVector2D::Distance(lastCenterCm, filletStartCm);
			if (!addLaneSample(
					ToScenarioCorridorPoint3D(filletStartCm, meshSpec.AxisLocationsCm[pointIndex].Z),
					ResolveScenarioCorridorRightCm(previousDirectionCm),
					visualAlongCm))
			{
				return false;
			}

			const int32 cornerSubdivisionCount = FMath::Clamp(
				FMath::CeilToInt((absTurnAngleRadians * FMath::Max(cornerFilletRadiusCm, maxOffsetMagnitudeCm))
					/ MaxProceduralLaneCurveStepCm),
				1,
				MaxProceduralLaneCurveSubdivisions);
			FVector2D previousCurveCenterCm = filletStartCm;
			for (int32 stepIndex = 1; stepIndex <= cornerSubdivisionCount; ++stepIndex)
			{
				const double alpha = static_cast<double>(stepIndex) / static_cast<double>(cornerSubdivisionCount);
				const FVector2D curveCenterCm = evaluateQuadraticCenterCm(
					filletStartCm,
					cornerCenterCm,
					filletEndCm,
					alpha);
				const FVector2D curveTangentCm = evaluateQuadraticTangentCm(
					filletStartCm,
					cornerCenterCm,
					filletEndCm,
					alpha);
				visualAlongCm += FVector2D::Distance(previousCurveCenterCm, curveCenterCm);
				if (!addLaneSample(
						ToScenarioCorridorPoint3D(curveCenterCm, meshSpec.AxisLocationsCm[pointIndex].Z),
						ResolveScenarioCorridorRightCm(curveTangentCm),
						visualAlongCm))
				{
					return false;
				}
				previousCurveCenterCm = curveCenterCm;
			}

			lastCenterCm = filletEndCm;
		}

		const int32 lastPointIndex = meshSpec.AxisLocationsCm.Num() - 1;
		FVector2D lastDirectionCm;
		if (!ResolveScenarioCorridorSegmentDirectionCm(meshSpec.AxisLocationsCm, lastPointIndex - 1, lastDirectionCm))
		{
			return false;
		}

		const FVector2D lastCenterPointCm = ToScenarioCorridorPoint2D(meshSpec.AxisLocationsCm[lastPointIndex]);
		visualAlongCm += FVector2D::Distance(lastCenterCm, lastCenterPointCm);
		if (!addLaneSample(
				meshSpec.AxisLocationsCm[lastPointIndex],
				ResolveScenarioCorridorRightCm(lastDirectionCm),
				visualAlongCm))
		{
			return false;
		}

		return outSamples.Num() >= 2;
	}

	FProcMeshTangent ResolveScenarioCorridorProceduralTangent(
		const FVector& firstLocationCm,
		const FVector& secondLocationCm,
		const FVector& fourthLocationCm,
		const FVector2D& firstUv,
		const FVector2D& secondUv,
		const FVector2D& fourthUv,
		const FVector& normal);

	// Appends one wound quad with shared per-face normal and tangent data.
	void AddScenarioCorridorProceduralQuadFace(
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
		TArray<FVector2D>& uv0,
		TArray<FProcMeshTangent>& tangents)
	{
		const int32 firstVertexIndex = vertices.Num();
		const FVector normal = FVector::CrossProduct(
			secondLocationCm - firstLocationCm,
			fourthLocationCm - firstLocationCm).GetSafeNormal();
		const FProcMeshTangent tangent = ResolveScenarioCorridorProceduralTangent(
			firstLocationCm,
			secondLocationCm,
			fourthLocationCm,
			firstUv,
			secondUv,
			fourthUv,
			normal);

		vertices.Add(firstLocationCm);
		vertices.Add(secondLocationCm);
		vertices.Add(thirdLocationCm);
		vertices.Add(fourthLocationCm);

		// ProceduralMesh front-face winding is opposite this outward-normal cross-product order.
		triangles.Add(firstVertexIndex + 3);
		triangles.Add(firstVertexIndex + 1);
		triangles.Add(firstVertexIndex);
		triangles.Add(firstVertexIndex + 3);
		triangles.Add(firstVertexIndex + 2);
		triangles.Add(firstVertexIndex + 1);

		normals.Add(normal);
		normals.Add(normal);
		normals.Add(normal);
		normals.Add(normal);

		uv0.Add(firstUv);
		uv0.Add(secondUv);
		uv0.Add(thirdUv);
		uv0.Add(fourthUv);

		tangents.Add(tangent);
		tangents.Add(tangent);
		tangents.Add(tangent);
		tangents.Add(tangent);
	}

	// Appends a single-sided quad using the supplied vertex order.
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
		TArray<FVector2D>& uv0,
		TArray<FProcMeshTangent>& tangents)
	{
		AddScenarioCorridorProceduralQuadFace(
			firstLocationCm,
			secondLocationCm,
			thirdLocationCm,
			fourthLocationCm,
			firstUv,
			secondUv,
			thirdUv,
			fourthUv,
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
	}

	// Appends front and back geometry for vertical side faces that must be visible from both sides.
	void AddScenarioCorridorProceduralTwoSidedQuad(
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
		TArray<FVector2D>& uv0,
		TArray<FProcMeshTangent>& tangents)
	{
		AddScenarioCorridorProceduralQuadFace(
			firstLocationCm,
			secondLocationCm,
			thirdLocationCm,
			fourthLocationCm,
			firstUv,
			secondUv,
			thirdUv,
			fourthUv,
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
		AddScenarioCorridorProceduralQuadFace(
			firstLocationCm,
			fourthLocationCm,
			thirdLocationCm,
			secondLocationCm,
			firstUv,
			fourthUv,
			thirdUv,
			secondUv,
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
	}

	// Appends one convex prism matching a rendered lane segment for simple collision.
	void AddScenarioCorridorSegmentCollisionConvex(
		const FScenarioCorridorProceduralLaneSample& startSample,
		const FScenarioCorridorProceduralLaneSample& endSample,
		TArray<TArray<FVector>>& outCollisionConvexMeshes)
	{
		TArray<FVector> convexVertices;
		convexVertices.Reserve(8);
		convexVertices.Add(startSample.MinTopLocationCm);
		convexVertices.Add(startSample.MaxTopLocationCm);
		convexVertices.Add(endSample.MaxTopLocationCm);
		convexVertices.Add(endSample.MinTopLocationCm);
		convexVertices.Add(startSample.MinBottomLocationCm);
		convexVertices.Add(startSample.MaxBottomLocationCm);
		convexVertices.Add(endSample.MaxBottomLocationCm);
		convexVertices.Add(endSample.MinBottomLocationCm);
		outCollisionConvexMeshes.Add(MoveTemp(convexVertices));
	}

	// Resolves a face tangent without letting ProceduralMesh recalculate the authored face normal.
	FProcMeshTangent ResolveScenarioCorridorProceduralTangent(
		const FVector& firstLocationCm,
		const FVector& secondLocationCm,
		const FVector& fourthLocationCm,
		const FVector2D& firstUv,
		const FVector2D& secondUv,
		const FVector2D& fourthUv,
		const FVector& normal)
	{
		FVector tangentCm = FVector::ZeroVector;
		const FVector firstEdgeCm = secondLocationCm - firstLocationCm;
		const FVector secondEdgeCm = fourthLocationCm - firstLocationCm;
		const FVector2D firstUvDelta = secondUv - firstUv;
		const FVector2D secondUvDelta = fourthUv - firstUv;
		const double uvDeterminant = (firstUvDelta.X * secondUvDelta.Y) - (secondUvDelta.X * firstUvDelta.Y);
		if (!FMath::IsNearlyZero(uvDeterminant, KINDA_SMALL_NUMBER))
		{
			tangentCm = ((firstEdgeCm * secondUvDelta.Y) - (secondEdgeCm * firstUvDelta.Y)) / uvDeterminant;
		}
		if (tangentCm.IsNearlyZero())
		{
			tangentCm = firstEdgeCm.IsNearlyZero() ? secondEdgeCm : firstEdgeCm;
		}

		tangentCm = (tangentCm - (normal * FVector::DotProduct(tangentCm, normal))).GetSafeNormal();
		if (tangentCm.IsNearlyZero())
		{
			tangentCm = FVector::CrossProduct(FVector::UpVector, normal).GetSafeNormal();
		}
		if (tangentCm.IsNearlyZero())
		{
			tangentCm = FVector::ForwardVector;
		}

		return FProcMeshTangent(tangentCm, false);
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
	TArray<FProcMeshTangent> tangents;
	TArray<TArray<FVector>> collisionConvexMeshes;
	const int32 segmentCount = samples.Num() - 1;
	const int32 estimatedVertexCount = (segmentCount * 24) + 8;
	const bool bCreateCollision = meshSpec.CollisionEnabled != ECollisionEnabled::NoCollision;
	const double laneWidthMeters =
		FMath::Abs(meshSpec.MaxOffsetCm - meshSpec.MinOffsetCm) / MetersToCentimeters;
	const double laneHeightMeters = meshSpec.LaneHeightCm / MetersToCentimeters;
	vertices.Reserve(estimatedVertexCount);
	triangles.Reserve((segmentCount * 36) + 12);
	normals.Reserve(estimatedVertexCount);
	uv0.Reserve(estimatedVertexCount);
	tangents.Reserve(estimatedVertexCount);
	if (bCreateCollision)
	{
		collisionConvexMeshes.Reserve(segmentCount);
	}

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
			FVector2D(endU, laneWidthMeters),
			FVector2D(startU, laneWidthMeters),
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
		AddScenarioCorridorProceduralQuad(
			startSample.MinBottomLocationCm,
			startSample.MaxBottomLocationCm,
			endSample.MaxBottomLocationCm,
			endSample.MinBottomLocationCm,
			FVector2D(startU, 0.0),
			FVector2D(startU, laneWidthMeters),
			FVector2D(endU, laneWidthMeters),
			FVector2D(endU, 0.0),
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
		AddScenarioCorridorProceduralTwoSidedQuad(
			startSample.MinBottomLocationCm,
			endSample.MinBottomLocationCm,
			endSample.MinTopLocationCm,
			startSample.MinTopLocationCm,
			FVector2D(startU, laneHeightMeters),
			FVector2D(endU, laneHeightMeters),
			FVector2D(endU, 0.0),
			FVector2D(startU, 0.0),
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
		AddScenarioCorridorProceduralTwoSidedQuad(
			startSample.MaxBottomLocationCm,
			startSample.MaxTopLocationCm,
			endSample.MaxTopLocationCm,
			endSample.MaxBottomLocationCm,
			FVector2D(startU, laneHeightMeters),
			FVector2D(startU, 0.0),
			FVector2D(endU, 0.0),
			FVector2D(endU, laneHeightMeters),
			vertices,
			triangles,
			normals,
			uv0,
			tangents);
		if (bCreateCollision)
		{
			AddScenarioCorridorSegmentCollisionConvex(startSample, endSample, collisionConvexMeshes);
		}
	}

	const FScenarioCorridorProceduralLaneSample& firstSample = samples[0];
	const FScenarioCorridorProceduralLaneSample& lastSample = samples.Last();
	AddScenarioCorridorProceduralQuad(
		firstSample.MinBottomLocationCm,
		firstSample.MinTopLocationCm,
		firstSample.MaxTopLocationCm,
		firstSample.MaxBottomLocationCm,
		FVector2D(0.0, laneHeightMeters),
		FVector2D(0.0, 0.0),
		FVector2D(laneWidthMeters, 0.0),
		FVector2D(laneWidthMeters, laneHeightMeters),
		vertices,
		triangles,
		normals,
		uv0,
		tangents);
	AddScenarioCorridorProceduralQuad(
		lastSample.MinBottomLocationCm,
		lastSample.MaxBottomLocationCm,
		lastSample.MaxTopLocationCm,
		lastSample.MinTopLocationCm,
		FVector2D(0.0, laneHeightMeters),
		FVector2D(laneWidthMeters, laneHeightMeters),
		FVector2D(laneWidthMeters, 0.0),
		FVector2D(0.0, 0.0),
		vertices,
		triangles,
		normals,
		uv0,
		tangents);

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
	meshComponent->bUseComplexAsSimpleCollision = false;
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
	meshComponent->CreateMeshSection_LinearColor(
		0,
		vertices,
		triangles,
		normals,
		uv0,
		vertexColors,
		tangents,
		bCreateCollision);
	if (bCreateCollision)
	{
		for (TArray<FVector>& collisionConvexMesh : collisionConvexMeshes)
		{
			meshComponent->AddCollisionConvexMesh(MoveTemp(collisionConvexMesh));
		}
	}
	if (meshSpec.Material)
	{
		meshComponent->SetMaterial(0, meshSpec.Material);
	}

	outLaneMeshComponents.Add(meshComponent);
	return 1;
}
