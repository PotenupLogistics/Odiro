#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"

#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultCorridorSurfaceCatalogPath(
		TEXT("/Game/Data/Scenario/DA_ScenarioCorridorSurfaceCatalog.DA_ScenarioCorridorSurfaceCatalog"));
	// Built-in fallback material paths used when the catalog asset is unavailable.
	const FSoftObjectPath SidewalkCorridorMaterialPath(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorSidewalk.M_ScenarioCorridorSidewalk"));
	const FSoftObjectPath GrassCorridorMaterialPath(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorGrass.M_ScenarioCorridorGrass"));
	const FSoftObjectPath RoadCorridorMaterialPath(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorRoad.M_ScenarioCorridorRoad"));
	const FSoftObjectPath WallCorridorMaterialPath(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorWall.M_ScenarioCorridorWall"));
	const FSoftObjectPath BuildingCorridorMaterialPath(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorBuilding.M_ScenarioCorridorBuilding"));

	FScenarioCorridorSurfaceEntry MakeCorridorSurfaceEntry(
		FName surfaceId,
		const FString& displayName,
		EScenarioSampleLaneType laneType,
		EScenarioGroundRegionType groundRegionType,
		double traversabilityScore,
		const FString& penaltyKind,
		double penaltyCost,
		const FString& collisionTag,
		const FSoftObjectPath& previewMaterialPath)
	{
		FScenarioCorridorSurfaceEntry entry;
		entry.SurfaceId = surfaceId;
		entry.DisplayName = FText::FromString(displayName);
		entry.LaneType = laneType;
		entry.GroundRegionType = groundRegionType;
		entry.TraversabilityScore = traversabilityScore;
		entry.PenaltyKind = penaltyKind;
		entry.PenaltyCost = penaltyCost;
		entry.CollisionTag = collisionTag;
		entry.PreviewMaterial = TSoftObjectPtr<UMaterialInterface>(previewMaterialPath);
		return entry;
	}
}

TSoftObjectPtr<UScenarioCorridorSurfaceCatalog> UScenarioCorridorSurfaceCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UScenarioCorridorSurfaceCatalog>(DefaultCorridorSurfaceCatalogPath);
}

TArray<FScenarioCorridorSurfaceEntry> UScenarioCorridorSurfaceCatalog::MakeDefaultEntries()
{
	return {
		MakeCorridorSurfaceEntry(
			TEXT("sidewalk"),
			TEXT("Sidewalk"),
			EScenarioSampleLaneType::Walkable,
			EScenarioGroundRegionType::Walkable,
			1.0,
			FString(),
			0.0,
			FString(),
			SidewalkCorridorMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("crosswalk_stripe"),
			TEXT("Crosswalk Stripe"),
			EScenarioSampleLaneType::Walkable,
			EScenarioGroundRegionType::Walkable,
			1.0,
			FString(),
			0.0,
			FString(),
			SidewalkCorridorMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("grass"),
			TEXT("Grass"),
			EScenarioSampleLaneType::Penalty,
			EScenarioGroundRegionType::Penalty,
			0.5,
			TEXT("grass"),
			1.0,
			FString(),
			GrassCorridorMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("road"),
			TEXT("Road"),
			EScenarioSampleLaneType::Penalty,
			EScenarioGroundRegionType::Penalty,
			0.35,
			TEXT("road"),
			1.0,
			FString(),
			RoadCorridorMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("driveway"),
			TEXT("Driveway"),
			EScenarioSampleLaneType::Penalty,
			EScenarioGroundRegionType::Penalty,
			0.45,
			TEXT("driveway"),
			1.0,
			FString(),
			RoadCorridorMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("wall"),
			TEXT("Wall"),
			EScenarioSampleLaneType::Blocked,
			EScenarioGroundRegionType::Blocked,
			0.0,
			FString(),
			0.0,
			TEXT("wall"),
			WallCorridorMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("building"),
			TEXT("Building"),
			EScenarioSampleLaneType::Blocked,
			EScenarioGroundRegionType::Blocked,
			0.0,
			FString(),
			0.0,
			TEXT("building"),
			BuildingCorridorMaterialPath)
	};
}

bool UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(
	FName surfaceId,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry)
{
	outSurfaceEntry = FScenarioCorridorSurfaceEntry();
	if (surfaceId.IsNone())
	{
		return false;
	}

	for (const FScenarioCorridorSurfaceEntry& entry : MakeDefaultEntries())
	{
		if (entry.SurfaceId == surfaceId)
		{
			outSurfaceEntry = entry;
			return true;
		}
	}

	return false;
}

bool UScenarioCorridorSurfaceCatalog::FindSurfaceEntryById(
	FName surfaceId,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry) const
{
	outSurfaceEntry = FScenarioCorridorSurfaceEntry();
	if (surfaceId.IsNone())
	{
		return false;
	}

	for (const FScenarioCorridorSurfaceEntry& entry : Entries)
	{
		if (entry.SurfaceId == surfaceId)
		{
			FScenarioCorridorSurfaceEntry defaultEntry;
			if (entry.bInheritBuiltInSemanticDefaults && FindDefaultSurfaceEntryById(surfaceId, defaultEntry))
			{
				outSurfaceEntry = defaultEntry;
				if (!entry.DisplayName.IsEmpty())
				{
					outSurfaceEntry.DisplayName = entry.DisplayName;
				}
				if (!entry.PreviewMaterial.IsNull())
				{
					outSurfaceEntry.PreviewMaterial = entry.PreviewMaterial;
				}
				return true;
			}

			outSurfaceEntry = entry;
			return true;
		}
	}

	return FindDefaultSurfaceEntryById(surfaceId, outSurfaceEntry);
}
