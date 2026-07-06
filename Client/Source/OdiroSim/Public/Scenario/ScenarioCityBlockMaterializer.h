#pragma once

#include "CoreMinimal.h"
#include "Scenario/Data/ScenarioCityBlockCatalog.h"
#include "Shared/ScenarioSpecTypes.h"

class AActor;
class UWorld;

// Summary of one generated-city visual block materialization pass.
struct ODIROSIM_API FScenarioCityBlockMaterializationResult
{
	// Generated GroundRegions that matched a supported CityBuildings role.
	int32 CandidateRegionCount = 0;

	// Visual block actors spawned during this materialization pass.
	int32 SpawnedActorCount = 0;

	// Generated road-side seam corners inferred from adjacent right-angle curb bands.
	int32 CornerCandidateCount = 0;

	// Generated road chunks that have a matching RoadStraight road-side composite catalog entry.
	int32 RoadSideCompositeCandidateCount = 0;

	// Candidate GroundRegions skipped because the configured catalog had no matching entry.
	int32 SkippedNoEntryCount = 0;

	// Generated road chunks skipped because no RoadStraight road-side composite entry was configured.
	int32 SkippedRoadSideCompositeNoEntryCount = 0;

	// Inferred road-side seam corners skipped because the configured catalog had no corner entry.
	int32 SkippedCornerNoEntryCount = 0;

	// Candidate GroundRegions skipped because their matching catalog entry could not spawn a valid actor.
	int32 SkippedSpawnFailureCount = 0;

	// Inferred road-side seam corners skipped because their catalog entry could not spawn a valid actor.
	int32 SkippedCornerSpawnFailureCount = 0;

	// RoadStraight visual actors spawned for generated road-side curb+road chunks.
	int32 SpawnedRoadSideCompositeCount = 0;

	// Candidate building frontage blocks skipped because their authored bounds overlap an accepted frontage footprint.
	int32 SkippedBuildingOverlapCount = 0;

	// Building navigation collision proxies available for runtime grid classification.
	int32 SpawnedBuildingCollisionProxyCount = 0;
};

// Caller-supplied labels used to keep shared materializer diagnostics readable.
struct ODIROSIM_API FScenarioCityBlockMaterializationOptions
{
	// Human-readable owner name included in logs, such as ScenarioSimulation or ScenarioEditor.
	FString LogContext = TEXT("ScenarioCity");

	// Catalog path or source label shown when the catalog is missing.
	FString CatalogDebugName;

	// Preserves BP-authored building blockers, keeps building meshes LiDAR-queryable, or creates fallback navigation proxies.
	bool bCreateBuildingCollisionProxies = false;
};

// Shared CityBuildings visual materializer for Scenario Editor and Simulation Map parity.
class ODIROSIM_API FScenarioCityBlockMaterializer
{
public:
	// Spawns visual-only CityBuildings blocks for generated city GroundRegions.
	static FScenarioCityBlockMaterializationResult SpawnGeneratedCityBlocks(
		UWorld* world,
		const UScenarioCityBlockCatalog* catalog,
		const TArray<FScenarioGroundRegionSpec>& groundRegions,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		const FScenarioCityBlockMaterializationOptions& options);

	// Destroys visual blocks previously returned through the materializer-owned actor array.
	static void DestroySpawnedActors(TArray<TObjectPtr<AActor>>& spawnedActors);
};
