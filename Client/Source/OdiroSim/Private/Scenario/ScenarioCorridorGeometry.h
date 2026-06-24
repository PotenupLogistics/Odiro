#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSpecTypes.h"

class AActor;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;

// Complete input needed to create deterministic procedural Corridor lane mesh sections.
struct FScenarioCorridorLaneMeshBuildSpec
{
	// Actor that owns the generated components.
	AActor* Owner = nullptr;

	// Scene component used as the generated components' attachment parent.
	USceneComponent* AttachParent = nullptr;

	// Material assigned to every generated lane section.
	UMaterialInterface* Material = nullptr;

	// Stable component name used as the generated lane mesh base object name.
	FName ComponentNameBase;

	// Axis point locations in the target component space, expressed in centimeters.
	TArray<FVector> AxisLocationsCm;

	// Tangents paired by index with AxisLocationsCm, expressed in centimeters.
	TArray<FVector> AxisTangentsCm;

	// Lower lateral boundary from the axis centerline in centimeters.
	double MinOffsetCm = 0.0;

	// Upper lateral boundary from the axis centerline in centimeters.
	double MaxOffsetCm = 0.0;

	// Physical prism height for the generated lane mesh in centimeters.
	double LaneHeightCm = 0.0;

	// Lane mesh center Z in centimeters.
	double LaneCenterZCm = 0.0;

	// Collision profile applied when CollisionEnabled is not NoCollision.
	FName CollisionProfileName;

	// Collision mode assigned to generated lane mesh components.
	ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;

	// Optional component tag copied to each generated lane mesh component.
	FName ComponentTag;
};

// Private geometry helper shared by Corridor editor preview and runtime materialization.
class FScenarioCorridorGeometry
{
public:
	// Conversion ratio used by authored/runtime scenario meters and Unreal centimeters.
	static constexpr double MetersToCentimeters = 100.0;

	// Surface query tolerance in meters, matching scenario_sample precision.
	static constexpr double SurfaceQueryToleranceMeters = 0.001;

	// Shared vertical offset for curb-side Corridor lane surfaces.
	static constexpr double DefaultCurbSideSurfaceZOffsetCm = -15.0;

	// Rotates a 2D point around the origin by a scenario heading in degrees.
	static FVector2D RotatePointMeters(const FVector2D& pointMeters, double headingDegrees);

	// Converts a runtime corridor-local axis point to world centimeters.
	static FVector TransformRuntimeAxisPointMetersToWorldCm(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const FVector2D& pointMeters,
		double surfaceTopZCm);

	// Builds world-centimeter axis locations for the requested runtime layout interval.
	static bool BuildRuntimeAxisLocationsForAlongRangeCm(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const FScenarioAlongRangeMeters& alongRangeMeters,
		double surfaceTopZCm,
		TArray<FVector>& outAxisLocationsCm);

	// Converts a world-centimeter location into runtime corridor-local meters.
	static FVector2D TransformRuntimeWorldCmToAxisPointMeters(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const FVector& worldLocation);

	// Approximates axis tangents from axis vertices so generated visuals bend through the same points.
	static FVector ResolveCurveTangentCm(const TArray<FVector>& axisLocationsCm, int32 pointIndex);

	// Identifies sampler/editor lane ids that represent the curb-side surface band.
	static bool IsCurbSideLaneId(const FString& laneId);

	// Maps a semantic lane id to the shared Corridor surface height offset.
	static double ResolveLaneSurfaceZOffsetCm(const FString& laneId);

	// Resolves a Corridor offset position to the shared editor/runtime surface height offset.
	static double ResolveSurfaceZOffsetForOffsetMeters(
		double offsetMeters,
		double halfWalkwayWidthMeters,
		double curbSideWidthMeters);

	// Projects a local point onto the corridor axis and returns along/offset distances in meters.
	static bool TryProjectPointToAxisMeters(
		const TArray<FVector2D>& axisPointsMeters,
		const FVector2D& localPointMeters,
		double& outAlongMeters,
		double& outOffsetMeters);

	// Checks a scalar against an inclusive range with deterministic tolerance.
	static bool ContainsRangeValue(
		double value,
		double minValue,
		double maxValue,
		double toleranceMeters = SurfaceQueryToleranceMeters);

	// Maps runtime region semantics to the collision profile used by generated lane components.
	static FName ResolveRuntimeCollisionProfileName(EScenarioGroundRegionType regionType);

	// Builds a stable runtime surface instance id from corridor, layout, and lane ownership.
	static FString MakeSurfaceInstanceId(
		const FScenarioRuntimeCorridorSpec& corridorSpec,
		const FScenarioRuntimeCorridorLayoutEntry& layoutEntry,
		const FScenarioRuntimeCorridorLaneSpec& laneSpec,
		int32 layoutIndex,
		int32 laneIndex);

	// Creates procedural lane mesh components and appends them to the caller-owned component list.
	static int32 AddLaneStripMeshes(
		const FScenarioCorridorLaneMeshBuildSpec& meshSpec,
		TArray<TObjectPtr<UProceduralMeshComponent>>& outLaneMeshComponents);
};
