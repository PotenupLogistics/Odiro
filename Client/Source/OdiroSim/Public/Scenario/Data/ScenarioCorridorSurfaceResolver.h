#pragma once

#include "CoreMinimal.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"

class UMaterialInterface;

// Shared Corridor surface metadata and material resolver for editor preview and runtime actors.
class ODIROSIM_API FScenarioCorridorSurfaceResolver
{
public:
	// Resolves a Corridor surface id from a configured catalog, built-in defaults, or walkable fallback metadata.
	static bool ResolveSurfaceEntry(
		const FString& surfaceId,
		const TSoftObjectPtr<UScenarioCorridorSurfaceCatalog>& surfaceCatalog,
		FScenarioCorridorSurfaceEntry& outSurfaceEntry);

	// Selects a catalog material, then falls back to caller-owned materials by semantic region type.
	static UMaterialInterface* ResolveSurfaceMaterial(
		const FScenarioCorridorSurfaceEntry& surfaceEntry,
		EScenarioGroundRegionType fallbackRegionType,
		UMaterialInterface* walkableFallbackMaterial,
		UMaterialInterface* penaltyFallbackMaterial,
		UMaterialInterface* blockedFallbackMaterial);

	// Selects a caller-owned fallback material for a semantic region type.
	static UMaterialInterface* ResolveFallbackSurfaceMaterial(
		EScenarioGroundRegionType regionType,
		UMaterialInterface* walkableFallbackMaterial,
		UMaterialInterface* penaltyFallbackMaterial,
		UMaterialInterface* blockedFallbackMaterial);

	// Converts authoring lane semantics to the material fallback region used by legacy preview rendering.
	static EScenarioGroundRegionType ResolveFallbackRegionType(EScenarioSampleLaneType laneType);

	// Returns true when either surface semantic class makes the lane a blocking volume.
	static bool IsBlockedSurface(const FScenarioCorridorSurfaceEntry& surfaceEntry);
};
