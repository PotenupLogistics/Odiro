#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioCorridorRuntimeActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;

// Deterministic lookup result for one sampled runtime Corridor lane surface.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioRuntimeCorridorSurfaceQueryResult
{
	GENERATED_BODY()

	// Stable runtime surface id built from corridor, layout, and lane ids.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	FString SurfaceInstanceId;

	// Runtime corridor id that owns the matched lane surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	FString CorridorId;

	// Layout segment id that owns the matched lane surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	FString SegmentId;

	// Lane id that matched the queried world location.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	FString LaneId;

	// Surface catalog id assigned to the matched lane.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	FString SurfaceId;

	// Runtime traversability class of the matched lane.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	EScenarioGroundRegionType RegionType = EScenarioGroundRegionType::Walkable;

	// Along distance on the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	double AlongMeters = 0.0;

	// Lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	double OffsetMeters = 0.0;
};

// Runtime corridor surface actor generated from scenario_sample semantic layout.
UCLASS(BlueprintType)
class ODIROSIM_API AScenarioCorridorRuntimeActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioCorridorRuntimeActor();

	// Root remains at world origin so sampled meters can map directly to world centimeters.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor")
	TObjectPtr<USceneComponent> SceneRoot;

	// Catalog used to resolve sampled surface ids into runtime materials.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Corridor")
	TSoftObjectPtr<UScenarioCorridorSurfaceCatalog> SurfaceCatalog;

	// Rebuilds runtime collision and visual lane strips from the sampled corridor layout.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Corridor")
	void ConfigureCorridor(const FScenarioRuntimeCorridorSpec& inCorridorSpec);

	// Returns the sampled corridor spec currently materialized by this actor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Corridor")
	FScenarioRuntimeCorridorSpec GetCorridorSpec() const { return CorridorSpec; }

	// Resolves the runtime lane surface containing the supplied world location in XY.
	UFUNCTION(BlueprintPure, Category = "Scenario|Corridor")
	bool TryFindSurfaceAtWorldLocation2D(
		const FVector& worldLocation,
		FScenarioRuntimeCorridorSurfaceQueryResult& outSurface) const;

private:
	// Runtime corridor data used to rebuild components and identify this actor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor", meta = (AllowPrivateAccess = "true"))
	FScenarioRuntimeCorridorSpec CorridorSpec;

	// Generated lane surface mesh components owned by this actor.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> LaneMeshComponents;

	// Material used for walkable lane strips when the catalog has no material.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WalkableGroundMaterial;

	// Material used for penalty lane strips when the catalog has no material.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PenaltyGroundMaterial;

	// Material used for blocked lane strips when the catalog has no material.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BlockedGroundMaterial;

	// Removes generated lane mesh components before rebuilding the corridor.
	void ClearLaneMeshes();

	// Creates a deterministic prism mesh over one layout interval.
	void AddLaneStrip(const FScenarioRuntimeCorridorLayoutEntry& layoutEntry, const FScenarioRuntimeCorridorLaneSpec& laneSpec);

	// Resolves a Corridor surface id from the configured catalog or built-in defaults.
	bool ResolveSurfaceEntry(const FString& surfaceId, FScenarioCorridorSurfaceEntry& outSurfaceEntry) const;

	// Selects a runtime material from resolved Corridor surface metadata.
	UMaterialInterface* ResolveSurfaceMaterial(const FScenarioCorridorSurfaceEntry& surfaceEntry) const;

	// Selects a fallback material when a catalog entry has no preview material assigned.
	UMaterialInterface* ResolveFallbackSurfaceMaterial(EScenarioGroundRegionType regionType) const;
};
