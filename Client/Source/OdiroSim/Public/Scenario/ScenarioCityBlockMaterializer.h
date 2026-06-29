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

	// Candidate GroundRegions skipped because the configured catalog had no matching entry.
	int32 SkippedNoEntryCount = 0;

	// Candidate GroundRegions skipped because their matching catalog entry could not spawn a valid actor.
	int32 SkippedSpawnFailureCount = 0;

	// Candidate GroundRegions skipped because a composite visual block already covers the same generated side strip.
	int32 SkippedCoveredByCompositeCount = 0;
};

// Caller-supplied labels used to keep shared materializer diagnostics readable.
struct ODIROSIM_API FScenarioCityBlockMaterializationOptions
{
	// Human-readable owner name included in logs, such as ScenarioSimulation or ScenarioEditor.
	FString LogContext = TEXT("ScenarioCity");

	// Catalog path or source label shown when the catalog is missing.
	FString CatalogDebugName;
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
