#include "Scenario/ScenarioCityBlockMaterializer.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Scenario/ScenarioCorridorGeometry.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioCityBlockMaterializer, Log, All);

namespace
{
	// Generated-city GroundRegion ids are the only regions eligible for CityBuildings visual blocks.
	const FString GeneratedCityRegionIdPrefix(TEXT("generated_city_"));
	// Surface ids match the reduced authoring/runtime vocabulary.
	const FName BuildingSurfaceId(TEXT("building"));
	const FName RoadSurfaceId(TEXT("road"));
	const FName WalkwaySurfaceId(TEXT("walkway"));
	// Generated curb GroundRegions use this collision tag while remaining road-surface metadata.
	const FString CurbCollisionTag(TEXT("curb"));
	// Generated road GroundRegions use this penalty kind for the two-lane road band.
	const FString RoadPenaltyKind(TEXT("road"));

	// Generated city band markers encode the side needed for corridor-relative edge anchoring.
	const TCHAR* GeneratedLowerSideMarkers[] =
	{
		TEXT("_lower_walkway_extension_"),
		TEXT("_lower_building_"),
		TEXT("_lower_curb_"),
		TEXT("_lower_road_2lane_")
	};

	// Generated city band markers encode the side needed for corridor-relative edge anchoring.
	const TCHAR* GeneratedUpperSideMarkers[] =
	{
		TEXT("_upper_walkway_extension_"),
		TEXT("_upper_building_"),
		TEXT("_upper_curb_"),
		TEXT("_upper_road_2lane_")
	};

	// Filters the materializer to generated straight city padding without touching authored GroundRegions.
	bool IsGeneratedCityVisualRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		return regionSpec.ShapeType == EScenarioGroundShapeType::Rectangle
			&& regionSpec.RegionId.StartsWith(GeneratedCityRegionIdPrefix);
	}

	// Detects the generated curb band used as the road-side composite placement trigger.
	bool IsGeneratedRoadCurbRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == RoadSurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Blocked
			&& (regionSpec.CollisionTag.Equals(CurbCollisionTag, ESearchCase::IgnoreCase)
				|| regionSpec.RegionId.Contains(TEXT("_curb_")));
	}

	// Detects the generated road band that should be covered by a curb-triggered composite block.
	bool IsGeneratedRoadPenaltyRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == RoadSurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Penalty
			&& (regionSpec.PenaltyKind.Equals(RoadPenaltyKind, ESearchCase::IgnoreCase)
				|| regionSpec.RegionId.Contains(TEXT("_road_2lane_")));
	}

	// Maps source-of-truth GroundRegion semantics to the straight block roles supported by the catalog.
	void ResolveCityBlockRolesForRegion(
		const FScenarioGroundRegionSpec& regionSpec,
		TArray<EScenarioCityBlockRole>& outRoles)
	{
		outRoles.Reset();
		const FName surfaceId(*regionSpec.SurfaceId);
		if (IsGeneratedRoadCurbRegion(regionSpec))
		{
			outRoles.Add(EScenarioCityBlockRole::WalkwayRoadStraight);
			return;
		}

		if (surfaceId == WalkwaySurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Walkable)
		{
			outRoles.Add(EScenarioCityBlockRole::WalkwayRoadStraight);
			outRoles.Add(EScenarioCityBlockRole::WalkwayBuildingStraight);
			return;
		}

		if (surfaceId == RoadSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Penalty)
		{
			outRoles.Add(EScenarioCityBlockRole::RoadStraight);
			return;
		}

		if (surfaceId == BuildingSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Blocked)
		{
			outRoles.Add(EScenarioCityBlockRole::Building);
		}
	}

	// Checks whether a catalog entry explicitly names the GroundRegion surface.
	bool DoesCityBlockEntryNameSurface(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		FName surfaceId)
	{
		return !surfaceId.IsNone() && blockEntry.SemanticProfile.SurfaceIds.Contains(surfaceId);
	}

	// Checks whether a catalog entry is allowed to represent the source GroundRegion surface.
	bool IsCityBlockEntrySurfaceCompatible(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		FName surfaceId)
	{
		return blockEntry.SemanticProfile.SurfaceIds.IsEmpty()
			|| DoesCityBlockEntryNameSurface(blockEntry, surfaceId);
	}

	// Checks optional semantic detail fields only when they are relevant to the source GroundRegion type.
	bool IsCityBlockEntryDetailCompatible(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec)
	{
		if (regionSpec.RegionType == EScenarioGroundRegionType::Blocked
			&& !blockEntry.SemanticProfile.CollisionTag.IsEmpty())
		{
			return blockEntry.SemanticProfile.CollisionTag.Equals(
				regionSpec.CollisionTag,
				ESearchCase::IgnoreCase);
		}

		if (regionSpec.RegionType == EScenarioGroundRegionType::Penalty
			&& !blockEntry.SemanticProfile.PenaltyKind.IsEmpty())
		{
			return blockEntry.SemanticProfile.PenaltyKind.Equals(
				regionSpec.PenaltyKind,
				ESearchCase::IgnoreCase);
		}

		return true;
	}

	// Checks whether optional semantic detail fields explicitly match the source GroundRegion.
	bool DoesCityBlockEntryDetailMatch(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec)
	{
		if (regionSpec.RegionType == EScenarioGroundRegionType::Blocked)
		{
			return !blockEntry.SemanticProfile.CollisionTag.IsEmpty()
				&& blockEntry.SemanticProfile.CollisionTag.Equals(
					regionSpec.CollisionTag,
					ESearchCase::IgnoreCase);
		}

		if (regionSpec.RegionType == EScenarioGroundRegionType::Penalty)
		{
			return !blockEntry.SemanticProfile.PenaltyKind.IsEmpty()
				&& blockEntry.SemanticProfile.PenaltyKind.Equals(
					regionSpec.PenaltyKind,
					ESearchCase::IgnoreCase);
		}

		return false;
	}

	// Builds a side-strip key shared by the curb and road bands in one generated layout chunk.
	bool TryMakeGeneratedRoadSideCompositeKey(
		const FString& regionId,
		FString& outCompositeKey)
	{
		struct FGeneratedSideMarker
		{
			// Stable id fragment embedded between generated segment id and layout/chunk suffix.
			const TCHAR* Marker = TEXT("");

			// Normalized lower/upper side label used in coverage keys.
			const TCHAR* Side = TEXT("");
		};

		const FGeneratedSideMarker sideMarkers[] =
		{
			{ TEXT("_lower_curb_"), TEXT("lower") },
			{ TEXT("_lower_road_2lane_"), TEXT("lower") },
			{ TEXT("_upper_curb_"), TEXT("upper") },
			{ TEXT("_upper_road_2lane_"), TEXT("upper") }
		};

		for (const FGeneratedSideMarker& sideMarker : sideMarkers)
		{
			const FString marker(sideMarker.Marker);
			const int32 markerIndex = regionId.Find(marker, ESearchCase::CaseSensitive);
			if (markerIndex == INDEX_NONE)
			{
				continue;
			}

			const FString segmentPrefix = regionId.Left(markerIndex);
			const FString layoutChunkSuffix = regionId.Mid(markerIndex + marker.Len());
			if (segmentPrefix.IsEmpty() || layoutChunkSuffix.IsEmpty())
			{
				continue;
			}

			outCompositeKey = FString::Printf(
				TEXT("%s|%s|%s"),
				*segmentPrefix,
				sideMarker.Side,
				*layoutChunkSuffix);
			return true;
		}

		outCompositeKey.Reset();
		return false;
	}

	// Resolves generated lower/upper side into a sign along the region's local right vector.
	bool TryResolveGeneratedCitySideSign(const FString& regionId, double& outSideSign)
	{
		for (const TCHAR* marker : GeneratedLowerSideMarkers)
		{
			if (regionId.Contains(marker))
			{
				outSideSign = -1.0;
				return true;
			}
		}

		for (const TCHAR* marker : GeneratedUpperSideMarkers)
		{
			if (regionId.Contains(marker))
			{
				outSideSign = 1.0;
				return true;
			}
		}

		outSideSign = 0.0;
		return false;
	}

	// Scores one catalog entry while preserving the role/surface/type fallback order.
	int32 ScoreCityBlockEntryForRegion(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec,
		FName surfaceId)
	{
		int32 matchTier = 1;
		if (DoesCityBlockEntryNameSurface(blockEntry, surfaceId))
		{
			matchTier = 2;
			if (blockEntry.SemanticProfile.PrimaryRegionType == regionSpec.RegionType)
			{
				matchTier = 3;
				if (DoesCityBlockEntryDetailMatch(blockEntry, regionSpec))
				{
					matchTier = 4;
				}
			}
		}

		return (matchTier * 10000) + blockEntry.PlacementProfile.Priority;
	}

	// Selects the most specific catalog entry for one generated GroundRegion.
	bool FindCityBlockEntryForRegion(
		const UScenarioCityBlockCatalog& catalog,
		const FScenarioGroundRegionSpec& regionSpec,
		FScenarioCityBlockCatalogEntry& outBlockEntry)
	{
		outBlockEntry = FScenarioCityBlockCatalogEntry();
		TArray<EScenarioCityBlockRole> roles;
		ResolveCityBlockRolesForRegion(regionSpec, roles);
		if (roles.IsEmpty())
		{
			return false;
		}

		const FName surfaceId(*regionSpec.SurfaceId);
		const FScenarioCityBlockCatalogEntry* bestEntry = nullptr;
		int32 bestScore = MIN_int32;
		for (const FScenarioCityBlockCatalogEntry& blockEntry : catalog.GetEntries())
		{
			if (!roles.Contains(blockEntry.Role))
			{
				continue;
			}

			if (!IsCityBlockEntrySurfaceCompatible(blockEntry, surfaceId)
				|| !IsCityBlockEntryDetailCompatible(blockEntry, regionSpec))
			{
				continue;
			}

			if (IsGeneratedRoadCurbRegion(regionSpec)
				&& blockEntry.SemanticProfile.PrimaryRegionType != regionSpec.RegionType)
			{
				continue;
			}

			const int32 score = ScoreCityBlockEntryForRegion(blockEntry, regionSpec, surfaceId);
			if (!bestEntry || score > bestScore)
			{
				bestEntry = &blockEntry;
				bestScore = score;
			}
		}

		if (bestEntry)
		{
			outBlockEntry = *bestEntry;
			return true;
		}

		return false;
	}

	// Finds road-side chunks whose curb band will spawn a composite visual block.
	void BuildRoadSideCompositeCoverageKeys(
		const UScenarioCityBlockCatalog& catalog,
		const TArray<FScenarioGroundRegionSpec>& groundRegions,
		TSet<FString>& outCompositeKeys)
	{
		outCompositeKeys.Reset();
		for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
		{
			if (!IsGeneratedRoadCurbRegion(regionSpec))
			{
				continue;
			}

			FScenarioCityBlockCatalogEntry blockEntry;
			if (!FindCityBlockEntryForRegion(catalog, regionSpec, blockEntry)
				|| blockEntry.Role != EScenarioCityBlockRole::WalkwayRoadStraight)
			{
				continue;
			}

			FString compositeKey;
			if (TryMakeGeneratedRoadSideCompositeKey(regionSpec.RegionId, compositeKey))
			{
				outCompositeKeys.Add(compositeKey);
			}
		}
	}

	// Checks whether a road band should be skipped because its curb band owns the composite visual.
	bool IsRoadBandCoveredByComposite(
		const FScenarioGroundRegionSpec& regionSpec,
		const TSet<FString>& compositeKeys)
	{
		if (!IsGeneratedRoadPenaltyRegion(regionSpec))
		{
			return false;
		}

		FString compositeKey;
		return TryMakeGeneratedRoadSideCompositeKey(regionSpec.RegionId, compositeKey)
			&& compositeKeys.Contains(compositeKey);
	}

	// Resolves the authored surface height that should receive the visual block origin.
	double ResolveCityBlockSurfaceTopZCm(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		if (surfaceId == RoadSurfaceId)
		{
			return 0.0;
		}

		if (surfaceId == WalkwaySurfaceId || surfaceId == BuildingSurfaceId)
		{
			return FScenarioCorridorGeometry::DefaultSurfaceTopZCm;
		}

		return regionSpec.Center.Z;
	}

	// Computes the desired authored bounds center for center- and edge-anchored visual blocks.
	FVector ResolveDesiredBlockBoundsCenter(
		const FScenarioGroundRegionSpec& regionSpec,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FRotator& blockRotation,
		const FVector& forward,
		double alongOffsetCm)
	{
		FVector baseRegionCenter = regionSpec.Center + (forward * alongOffsetCm);
		baseRegionCenter.Z = ResolveCityBlockSurfaceTopZCm(regionSpec);
		const EScenarioCityBlockLateralAnchor lateralAnchor =
			blockEntry.PlacementProfile.LateralAnchor;
		if (lateralAnchor == EScenarioCityBlockLateralAnchor::RegionCenter)
		{
			double sideSign = 0.0;
			if (TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
			{
				const FVector right = blockRotation.RotateVector(FVector::RightVector);
				return baseRegionCenter
					+ (right * sideSign * blockEntry.PlacementProfile.LateralOffsetMeters * 100.0);
			}
			return baseRegionCenter;
		}

		double sideSign = 0.0;
		if (!TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
		{
			return baseRegionCenter;
		}

		const FVector right = blockRotation.RotateVector(FVector::RightVector);
		const double regionHalfWidthCm = regionSpec.Size.Y * 0.5;
		const double blockHalfWidthCm = blockEntry.BoundsMeters.WidthMeters * 50.0;
		const double postAnchorOffsetCm = blockEntry.PlacementProfile.LateralOffsetMeters * 100.0;

		if (lateralAnchor == EScenarioCityBlockLateralAnchor::RegionInnerEdge)
		{
			return baseRegionCenter
				+ (right * sideSign * (blockHalfWidthCm - regionHalfWidthCm + postAnchorOffsetCm));
		}

		return baseRegionCenter
			+ (right * sideSign * (regionHalfWidthCm - blockHalfWidthCm + postAnchorOffsetCm));
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
			const FVector desiredBoundsCenter = ResolveDesiredBlockBoundsCenter(
				regionSpec,
				blockEntry,
				blockRotation,
				forward,
				alongOffsetCm);
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

	TSet<FString> roadSideCompositeKeys;
	BuildRoadSideCompositeCoverageKeys(*catalog, groundRegions, roadSideCompositeKeys);

	for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
	{
		TArray<EScenarioCityBlockRole> roles;
		ResolveCityBlockRolesForRegion(regionSpec, roles);
		if (!IsGeneratedCityVisualRegion(regionSpec) || roles.IsEmpty())
		{
			continue;
		}

		++result.CandidateRegionCount;
		if (IsRoadBandCoveredByComposite(regionSpec, roadSideCompositeKeys))
		{
			++result.SkippedCoveredByCompositeCount;
			continue;
		}

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
		|| result.SkippedSpawnFailureCount > 0
		|| result.SkippedCoveredByCompositeCount > 0)
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Log,
			TEXT(
				"%s generated city block visuals complete | "
				"CandidateRegions: %d, SpawnedActors: %d, SkippedNoEntry: %d, "
				"SkippedSpawnFailure: %d, SkippedCoveredByComposite: %d"),
			*options.LogContext,
			result.CandidateRegionCount,
			result.SpawnedActorCount,
			result.SkippedNoEntryCount,
			result.SkippedSpawnFailureCount,
			result.SkippedCoveredByCompositeCount);
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
