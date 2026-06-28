#include "Scenario/ScenarioCityBlockMaterializer.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioCityBlockMaterializer, Log, All);

namespace
{
	// Generated-city GroundRegion ids are the only regions eligible for CityBuildings visual blocks.
	const FString GeneratedCityRegionIdPrefix(TEXT("generated_city_"));
	// Surface ids match the reduced authoring/runtime vocabulary.
	const FName BuildingSurfaceId(TEXT("building"));
	const FName RoadSurfaceId(TEXT("road"));
	const FName WalkwaySurfaceId(TEXT("walkway"));

	// Filters the materializer to generated straight city padding without touching authored GroundRegions.
	bool IsGeneratedCityVisualRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		return regionSpec.ShapeType == EScenarioGroundShapeType::Rectangle
			&& regionSpec.RegionId.StartsWith(GeneratedCityRegionIdPrefix);
	}

	// Maps source-of-truth GroundRegion semantics to the first straight block roles supported by the catalog.
	EScenarioCityBlockRole ResolveCityBlockRoleForRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		if (surfaceId == WalkwaySurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Walkable)
		{
			return EScenarioCityBlockRole::WalkwayRoadStraight;
		}

		if (surfaceId == RoadSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Penalty)
		{
			return EScenarioCityBlockRole::RoadStraight;
		}

		if (surfaceId == BuildingSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Blocked)
		{
			return EScenarioCityBlockRole::Building;
		}

		return EScenarioCityBlockRole::Unknown;
	}

	// Checks whether a catalog entry explicitly names the GroundRegion surface.
	bool DoesCityBlockEntryNameSurface(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		FName surfaceId)
	{
		return !surfaceId.IsNone() && blockEntry.SemanticProfile.SurfaceIds.Contains(surfaceId);
	}

	// Selects the most specific catalog entry for one generated GroundRegion.
	bool FindCityBlockEntryForRegion(
		const UScenarioCityBlockCatalog& catalog,
		const FScenarioGroundRegionSpec& regionSpec,
		FScenarioCityBlockCatalogEntry& outBlockEntry)
	{
		outBlockEntry = FScenarioCityBlockCatalogEntry();
		const EScenarioCityBlockRole role = ResolveCityBlockRoleForRegion(regionSpec);
		if (role == EScenarioCityBlockRole::Unknown)
		{
			return false;
		}

		const FName surfaceId(*regionSpec.SurfaceId);
		const FScenarioCityBlockCatalogEntry* surfaceFallback = nullptr;
		const FScenarioCityBlockCatalogEntry* roleFallback = nullptr;
		for (const FScenarioCityBlockCatalogEntry& blockEntry : catalog.GetEntries())
		{
			if (blockEntry.Role != role)
			{
				continue;
			}

			if (!roleFallback)
			{
				roleFallback = &blockEntry;
			}

			if (!DoesCityBlockEntryNameSurface(blockEntry, surfaceId))
			{
				continue;
			}

			if (!surfaceFallback)
			{
				surfaceFallback = &blockEntry;
			}

			if (blockEntry.SemanticProfile.PrimaryRegionType == regionSpec.RegionType)
			{
				outBlockEntry = blockEntry;
				return true;
			}
		}

		if (surfaceFallback)
		{
			outBlockEntry = *surfaceFallback;
			return true;
		}

		if (roleFallback)
		{
			outBlockEntry = *roleFallback;
			return true;
		}

		return false;
	}

	// Disables every collision path on visual-only CityBuildings actors.
	void DisableCityBlockActorCollision(AActor& blockActor)
	{
		blockActor.SetActorEnableCollision(false);

		TArray<UPrimitiveComponent*> primitiveComponents;
		blockActor.GetComponents<UPrimitiveComponent>(primitiveComponents);
		for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
		{
			if (!primitiveComponent)
			{
				continue;
			}

			primitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			primitiveComponent->SetGenerateOverlapEvents(false);
		}
	}

	// Prevents generated visual blocks from receiving projected runtime/editor decals.
	void SetActorReceivesDecals(AActor& blockActor, bool bReceivesDecals)
	{
		TArray<UPrimitiveComponent*> primitiveComponents;
		blockActor.GetComponents<UPrimitiveComponent>(primitiveComponents);
		for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
		{
			if (primitiveComponent)
			{
				primitiveComponent->SetReceivesDecals(bReceivesDecals);
			}
		}
	}

	// Spawns repeated visual-only blocks along one generated rectangular GroundRegion.
	int32 SpawnCityBlockVisualsForRegion(
		UWorld& world,
		const FScenarioGroundRegionSpec& regionSpec,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		UClass* blockClass = blockEntry.BPClass.LoadSynchronous();
		if (!IsValid(blockClass) || !blockClass->IsChildOf(AActor::StaticClass()))
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated city block '%s' skipped for region '%s' because BPClass is empty or not an Actor class."),
				*options.LogContext,
				*blockEntry.BlockId.ToString(),
				*regionSpec.RegionId);
			return 0;
		}

		const double blockLengthCm = blockEntry.BoundsMeters.LengthMeters * 100.0;
		if (blockLengthCm <= KINDA_SMALL_NUMBER || regionSpec.Size.X <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated city block '%s' skipped for region '%s' because block or region length is invalid."),
				*options.LogContext,
				*blockEntry.BlockId.ToString(),
				*regionSpec.RegionId);
			return 0;
		}

		const int32 blockCount = FMath::Max(1, FMath::CeilToInt(regionSpec.Size.X / blockLengthCm));
		const FRotator blockRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = blockRotation.RotateVector(FVector::ForwardVector);
		const FVector boundsCenterOffsetCm =
			blockRotation.RotateVector(blockEntry.BoundsMeters.CenterOffsetMeters * 100.0);
		const double chainLengthCm = static_cast<double>(blockCount - 1) * blockLengthCm;

		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		int32 spawnedActorCount = 0;
		for (int32 blockIndex = 0; blockIndex < blockCount; ++blockIndex)
		{
			const double alongOffsetCm =
				(static_cast<double>(blockIndex) * blockLengthCm) - (chainLengthCm * 0.5);
			const FVector desiredBoundsCenter = regionSpec.Center + (forward * alongOffsetCm);
			const FVector spawnLocation = desiredBoundsCenter - boundsCenterOffsetCm;
			AActor* blockActor = world.SpawnActor<AActor>(
				blockClass,
				FTransform(blockRotation, spawnLocation),
				spawnParams);
			if (!blockActor)
			{
				UE_LOG(
					LogScenarioCityBlockMaterializer,
					Warning,
					TEXT("%s generated city block '%s' failed to spawn for region '%s'."),
					*options.LogContext,
					*blockEntry.BlockId.ToString(),
					*regionSpec.RegionId);
				continue;
			}

			DisableCityBlockActorCollision(*blockActor);
			SetActorReceivesDecals(*blockActor, false);
			outSpawnedActors.Add(blockActor);
			++spawnedActorCount;
		}

		return spawnedActorCount;
	}
}

FScenarioCityBlockMaterializationResult FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
	UWorld* world,
	const UScenarioCityBlockCatalog* catalog,
	const TArray<FScenarioGroundRegionSpec>& groundRegions,
	TArray<TObjectPtr<AActor>>& outSpawnedActors,
	const FScenarioCityBlockMaterializationOptions& options)
{
	FScenarioCityBlockMaterializationResult result;
	if (!world)
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Warning,
			TEXT("%s generated city block visuals skipped because world is unavailable."),
			*options.LogContext);
		return result;
	}

	if (!IsValid(catalog))
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Warning,
			TEXT("%s scenario city block catalog is not configured or failed to load: %s"),
			*options.LogContext,
			*options.CatalogDebugName);
		return result;
	}

	for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
	{
		if (!IsGeneratedCityVisualRegion(regionSpec)
			|| ResolveCityBlockRoleForRegion(regionSpec) == EScenarioCityBlockRole::Unknown)
		{
			continue;
		}

		++result.CandidateRegionCount;
		FScenarioCityBlockCatalogEntry blockEntry;
		if (!FindCityBlockEntryForRegion(*catalog, regionSpec, blockEntry))
		{
			++result.SkippedNoEntryCount;
			continue;
		}

		const int32 beforeSpawnCount = outSpawnedActors.Num();
		result.SpawnedActorCount += SpawnCityBlockVisualsForRegion(
			*world,
			regionSpec,
			blockEntry,
			outSpawnedActors,
			options);
		if (outSpawnedActors.Num() == beforeSpawnCount)
		{
			++result.SkippedSpawnFailureCount;
		}
	}

	if (result.CandidateRegionCount > 0
		|| result.SpawnedActorCount > 0
		|| result.SkippedNoEntryCount > 0
		|| result.SkippedSpawnFailureCount > 0)
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Log,
			TEXT(
				"%s generated city block visuals complete | "
				"CandidateRegions: %d, SpawnedActors: %d, SkippedNoEntry: %d, SkippedSpawnFailure: %d"),
			*options.LogContext,
			result.CandidateRegionCount,
			result.SpawnedActorCount,
			result.SkippedNoEntryCount,
			result.SkippedSpawnFailureCount);
	}

	return result;
}

void FScenarioCityBlockMaterializer::DestroySpawnedActors(TArray<TObjectPtr<AActor>>& spawnedActors)
{
	for (int32 index = spawnedActors.Num() - 1; index >= 0; --index)
	{
		if (AActor* actor = spawnedActors[index].Get())
		{
			actor->Destroy();
		}
	}
	spawnedActors.Reset();
}
