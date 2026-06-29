#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioCityBlockCatalog.generated.h"

class AActor;

// Placement role used to select CityBuildings visual blocks around generated GroundRegions.
UENUM(BlueprintType)
enum class EScenarioCityBlockRole : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	WalkwayRoadStraight UMETA(DisplayName = "Walkway Road Straight"),
	WalkwayBuildingStraight UMETA(DisplayName = "Walkway Building Straight"),
	RoadStraight UMETA(DisplayName = "Road Straight"),
	Corner UMETA(DisplayName = "Corner"),
	Crossroad UMETA(DisplayName = "Crossroad"),
	Building UMETA(DisplayName = "Building")
};

// Lateral reference line used when aligning visual blocks against generated city GroundRegions.
UENUM(BlueprintType)
enum class EScenarioCityBlockLateralAnchor : uint8
{
	RegionCenter UMETA(DisplayName = "Generated Region Center"),
	// Continuous side seam closest to the authored Corridor, such as the walkway-curb seam.
	RegionInnerEdge UMETA(DisplayName = "Generated Region Inner Edge"),
	// Continuous side seam farthest from the authored Corridor, such as the road outside edge.
	RegionOuterEdge UMETA(DisplayName = "Generated Region Outer Edge")
};

// Author-supplied block bounds in meters for deterministic tile placement.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioCityBlockBoundsMeters
{
	GENERATED_BODY()

	// Forward-axis span used when splitting corridor-side strips into repeated blocks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block", meta = (ClampMin = "0.0"))
	double LengthMeters = 10.0;

	// Lateral span used to match the block against generated walkway, curb, road, or building bands.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block", meta = (ClampMin = "0.0"))
	double WidthMeters = 10.0;

	// Vertical span used for culling, preview bounds, and future placement validation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block", meta = (ClampMin = "0.0"))
	double HeightMeters = 1.0;

	// Offset from the actor origin to the authored bounds center; use this when the BP root is a seam anchor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FVector CenterOffsetMeters = FVector::ZeroVector;
};

// Placement hint that separates CityBuildings visual alignment from source GroundRegion semantics.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioCityBlockPlacementProfile
{
	GENERATED_BODY()

	// Selection tie-breaker used after role, surface, and GroundRegion type matching.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	int32 Priority = 0;

	// Generated region reference line that the block's authored bounds should align against.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	EScenarioCityBlockLateralAnchor LateralAnchor = EScenarioCityBlockLateralAnchor::RegionCenter;

	// Extra side-relative offset applied after anchor alignment; positive moves away from the Corridor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	double LateralOffsetMeters = 0.0;
};

// Descriptive semantic hint that keeps CityBuildings visuals aligned with source-of-truth GroundRegions.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioCityBlockSemanticProfile
{
	GENERATED_BODY()

	// Project-defined profile id such as walkway_road, walkway_building, road, or building.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FName ProfileId;

	// Corridor surface ids visually represented by this block, using the walkway, road, and building vocabulary.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	TArray<FName> SurfaceIds;

	// GroundRegion type that should back the primary visual surface for evaluation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	EScenarioGroundRegionType PrimaryRegionType = EScenarioGroundRegionType::Walkable;

	// Collision tag expected on backing Blocked GroundRegions when this block represents an obstacle band.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FString CollisionTag;

	// Penalty label expected on backing Penalty GroundRegions when this block represents a cost band.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FString PenaltyKind;
};

// Blueprint-editable CityBuildings block candidate used by scenario city materialization.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioCityBlockCatalogEntry
{
	GENERATED_BODY()

	// Stable block id used by materialization rules and debug output.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FName BlockId;

	// Actor Blueprint class spawned for this visual block.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	TSoftClassPtr<AActor> BPClass;

	// Placement role describing where this block can be used in the generated city layout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	EScenarioCityBlockRole Role = EScenarioCityBlockRole::Unknown;

	// Authored dimensions used instead of mesh collision or auto bounds as the placement contract.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FScenarioCityBlockBoundsMeters BoundsMeters;

	// Placement metadata that chooses variants and aligns composite blocks to generated city edges.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FScenarioCityBlockPlacementProfile PlacementProfile;

	// Semantic hint used to match visuals with generated GroundRegions without making the visual mesh authoritative.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	FScenarioCityBlockSemanticProfile SemanticProfile;
};

// DataAsset catalog for CityBuildings visual block candidates.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioCityBlockCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Default project asset location for CityBuildings block metadata.
	static TSoftObjectPtr<UScenarioCityBlockCatalog> MakeDefaultCatalogReference();

	// Project-owned CityBuildings block candidates keyed by BlockId.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|City Block")
	TArray<FScenarioCityBlockCatalogEntry> Entries;

	// Finds a project-owned block entry by id.
	UFUNCTION(BlueprintPure, Category = "Scenario|City Block")
	bool FindBlockEntryById(FName blockId, FScenarioCityBlockCatalogEntry& outBlockEntry) const;

	// Returns every project-owned block entry matching a placement role.
	UFUNCTION(BlueprintPure, Category = "Scenario|City Block")
	void GetEntriesForRole(EScenarioCityBlockRole role, TArray<FScenarioCityBlockCatalogEntry>& outEntries) const;

	// Returns the project-owned entries stored on this asset.
	const TArray<FScenarioCityBlockCatalogEntry>& GetEntries() const { return Entries; }
};
