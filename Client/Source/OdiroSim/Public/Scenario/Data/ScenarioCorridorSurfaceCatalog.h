#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shared/ScenarioSampleTypes.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioCorridorSurfaceCatalog.generated.h"

class UMaterialInterface;

// Corridor surface metadata used to interpret scenario surface ids in editor, sampling, and runtime paths.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioCorridorSurfaceEntry
{
	GENERATED_BODY()

	// Stable surface id used by corridor.building_side, corridor.curb_side, and corridor.segments.replaced_by.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	FName SurfaceId;

	// Editor display label for this surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	FText DisplayName;

	// Semantic traversability class emitted into scenario_sample layout lanes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	EScenarioSampleLaneType LaneType = EScenarioSampleLaneType::Walkable;

	// Runtime ground-region class used when a lane becomes world collision/penalty geometry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	EScenarioGroundRegionType GroundRegionType = EScenarioGroundRegionType::Walkable;

	// Score reported on generated runtime surfaces for this surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	double TraversabilityScore = 1.0;

	// Penalty label used when GroundRegionType is Penalty.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	FString PenaltyKind;

	// Penalty cost used when GroundRegionType is Penalty.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	double PenaltyCost = 0.0;

	// Collision tag used when GroundRegionType is Blocked.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	FString CollisionTag;

	// Material used by Corridor spline preview and generated runtime ground-region visuals.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	TSoftObjectPtr<UMaterialInterface> PreviewMaterial;

	// Keeps built-in semantic defaults for known ids while allowing visual overrides from this entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	bool bInheritBuiltInSemanticDefaults = true;
};

// DataAsset catalog for Corridor surface ids from the authoring vocabulary.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioCorridorSurfaceCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Default project asset location for Corridor surface metadata.
	static TSoftObjectPtr<UScenarioCorridorSurfaceCatalog> MakeDefaultCatalogReference();

	// Built-in fallback entries matching Client/Json/environment-catalog.md.
	static TArray<FScenarioCorridorSurfaceEntry> MakeDefaultEntries();

	// Finds a built-in fallback surface entry by id.
	static bool FindDefaultSurfaceEntryById(FName surfaceId, FScenarioCorridorSurfaceEntry& outSurfaceEntry);

	// Project-owned Corridor surface entries keyed by SurfaceId.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Corridor Surface")
	TArray<FScenarioCorridorSurfaceEntry> Entries;

	// Finds an asset entry first, then falls back to built-in defaults.
	UFUNCTION(BlueprintPure, Category = "Scenario|Corridor Surface")
	bool FindSurfaceEntryById(FName surfaceId, FScenarioCorridorSurfaceEntry& outSurfaceEntry) const;

	// Returns the project-owned entries stored on this asset.
	const TArray<FScenarioCorridorSurfaceEntry>& GetEntries() const { return Entries; }
};
