#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"

#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultCorridorSurfaceCatalogPath(
		TEXT("/Game/Data/Scenario/DA_ScenarioCorridorSurfaceCatalog.DA_ScenarioCorridorSurfaceCatalog"));
	const FSoftObjectPath WalkableGroundMaterialPath(
		TEXT("/Game/Materials/M_ScenarioGroundWalkable.M_ScenarioGroundWalkable"));
	const FSoftObjectPath PenaltyGroundMaterialPath(
		TEXT("/Game/Materials/M_ScenarioGroundPenalty.M_ScenarioGroundPenalty"));
	const FSoftObjectPath BlockedGroundMaterialPath(
		TEXT("/Game/Materials/M_ScenarioGroundBlock.M_ScenarioGroundBlock"));

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
			WalkableGroundMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("crosswalk_stripe"),
			TEXT("Crosswalk Stripe"),
			EScenarioSampleLaneType::Walkable,
			EScenarioGroundRegionType::Walkable,
			1.0,
			FString(),
			0.0,
			FString(),
			WalkableGroundMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("grass"),
			TEXT("Grass"),
			EScenarioSampleLaneType::Penalty,
			EScenarioGroundRegionType::Penalty,
			0.5,
			TEXT("grass"),
			1.0,
			FString(),
			PenaltyGroundMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("road"),
			TEXT("Road"),
			EScenarioSampleLaneType::Penalty,
			EScenarioGroundRegionType::Penalty,
			0.35,
			TEXT("road"),
			1.0,
			FString(),
			PenaltyGroundMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("driveway"),
			TEXT("Driveway"),
			EScenarioSampleLaneType::Penalty,
			EScenarioGroundRegionType::Penalty,
			0.45,
			TEXT("driveway"),
			1.0,
			FString(),
			PenaltyGroundMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("wall"),
			TEXT("Wall"),
			EScenarioSampleLaneType::Blocked,
			EScenarioGroundRegionType::Blocked,
			0.0,
			FString(),
			0.0,
			TEXT("wall"),
			BlockedGroundMaterialPath),
		MakeCorridorSurfaceEntry(
			TEXT("building"),
			TEXT("Building"),
			EScenarioSampleLaneType::Blocked,
			EScenarioGroundRegionType::Blocked,
			0.0,
			FString(),
			0.0,
			TEXT("building"),
			BlockedGroundMaterialPath)
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
