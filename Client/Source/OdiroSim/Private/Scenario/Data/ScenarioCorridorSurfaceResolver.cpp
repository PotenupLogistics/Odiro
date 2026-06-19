#include "Scenario/Data/ScenarioCorridorSurfaceResolver.h"

#include "Materials/MaterialInterface.h"

// Log category for shared Corridor surface metadata and material resolution.
DEFINE_LOG_CATEGORY_STATIC(LogScenarioCorridorSurfaceResolver, Log, All);

bool FScenarioCorridorSurfaceResolver::ResolveSurfaceEntry(
	const FString& surfaceId,
	const TSoftObjectPtr<UScenarioCorridorSurfaceCatalog>& surfaceCatalog,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry)
{
	outSurfaceEntry = FScenarioCorridorSurfaceEntry();
	const FName surfaceName(*surfaceId);
	if (const UScenarioCorridorSurfaceCatalog* loadedCatalog = surfaceCatalog.LoadSynchronous())
	{
		if (loadedCatalog->FindSurfaceEntryById(surfaceName, outSurfaceEntry))
		{
			UE_LOG(
				LogScenarioCorridorSurfaceResolver,
				Log,
				TEXT("Resolved Corridor surface '%s' from catalog/defaults. Catalog: %s"),
				*surfaceName.ToString(),
				*loadedCatalog->GetPathName());
			return true;
		}
	}
	else if (!surfaceCatalog.IsNull())
	{
		UE_LOG(
			LogScenarioCorridorSurfaceResolver,
			Warning,
			TEXT("Corridor surface catalog could not be loaded. Path: %s"),
			*surfaceCatalog.ToSoftObjectPath().ToString());
	}

	if (UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(surfaceName, outSurfaceEntry))
	{
		UE_LOG(
			LogScenarioCorridorSurfaceResolver,
			Log,
			TEXT("Resolved Corridor surface '%s' from built-in defaults."),
			*surfaceName.ToString());
		return true;
	}

	UE_LOG(
		LogScenarioCorridorSurfaceResolver,
		Warning,
		TEXT("Unknown Corridor surface '%s'; using walkable fallback metadata."),
		surfaceId.IsEmpty() ? TEXT("<empty>") : *surfaceId);
	outSurfaceEntry.SurfaceId = surfaceName;
	outSurfaceEntry.DisplayName = FText::FromString(surfaceId.IsEmpty() ? TEXT("Unknown Surface") : surfaceId);
	outSurfaceEntry.LaneType = EScenarioSampleLaneType::Walkable;
	outSurfaceEntry.GroundRegionType = EScenarioGroundRegionType::Walkable;
	outSurfaceEntry.TraversabilityScore = 1.0;
	return false;
}

UMaterialInterface* FScenarioCorridorSurfaceResolver::ResolveSurfaceMaterial(
	const FScenarioCorridorSurfaceEntry& surfaceEntry,
	EScenarioGroundRegionType fallbackRegionType,
	UMaterialInterface* walkableFallbackMaterial,
	UMaterialInterface* penaltyFallbackMaterial,
	UMaterialInterface* blockedFallbackMaterial)
{
	if (UMaterialInterface* catalogMaterial = surfaceEntry.PreviewMaterial.LoadSynchronous())
	{
		UE_LOG(
			LogScenarioCorridorSurfaceResolver,
			Log,
			TEXT("Using Corridor surface material. Surface: %s | Material: %s"),
			*surfaceEntry.SurfaceId.ToString(),
			*catalogMaterial->GetPathName());
		return catalogMaterial;
	}

	if (!surfaceEntry.PreviewMaterial.IsNull())
	{
		UE_LOG(
			LogScenarioCorridorSurfaceResolver,
			Warning,
			TEXT("Corridor surface material failed to load. Surface: %s | Path: %s"),
			*surfaceEntry.SurfaceId.ToString(),
			*surfaceEntry.PreviewMaterial.ToSoftObjectPath().ToString());
	}

	UMaterialInterface* fallbackMaterial = ResolveFallbackSurfaceMaterial(
		fallbackRegionType,
		walkableFallbackMaterial,
		penaltyFallbackMaterial,
		blockedFallbackMaterial);
	UE_LOG(
		LogScenarioCorridorSurfaceResolver,
		Log,
		TEXT("Using Corridor surface fallback material. Surface: %s | RegionType: %d | Material: %s"),
		*surfaceEntry.SurfaceId.ToString(),
		static_cast<int32>(fallbackRegionType),
		fallbackMaterial ? *fallbackMaterial->GetPathName() : TEXT("<null>"));
	return fallbackMaterial;
}

UMaterialInterface* FScenarioCorridorSurfaceResolver::ResolveFallbackSurfaceMaterial(
	EScenarioGroundRegionType regionType,
	UMaterialInterface* walkableFallbackMaterial,
	UMaterialInterface* penaltyFallbackMaterial,
	UMaterialInterface* blockedFallbackMaterial)
{
	if (regionType == EScenarioGroundRegionType::Blocked)
	{
		return blockedFallbackMaterial ? blockedFallbackMaterial : walkableFallbackMaterial;
	}

	if (regionType == EScenarioGroundRegionType::Penalty)
	{
		return penaltyFallbackMaterial ? penaltyFallbackMaterial : walkableFallbackMaterial;
	}

	return walkableFallbackMaterial;
}

EScenarioGroundRegionType FScenarioCorridorSurfaceResolver::ResolveFallbackRegionType(EScenarioSampleLaneType laneType)
{
	if (laneType == EScenarioSampleLaneType::Blocked)
	{
		return EScenarioGroundRegionType::Blocked;
	}

	if (laneType == EScenarioSampleLaneType::Penalty)
	{
		return EScenarioGroundRegionType::Penalty;
	}

	return EScenarioGroundRegionType::Walkable;
}

bool FScenarioCorridorSurfaceResolver::IsBlockedSurface(const FScenarioCorridorSurfaceEntry& surfaceEntry)
{
	return surfaceEntry.GroundRegionType == EScenarioGroundRegionType::Blocked
		|| surfaceEntry.LaneType == EScenarioSampleLaneType::Blocked;
}
