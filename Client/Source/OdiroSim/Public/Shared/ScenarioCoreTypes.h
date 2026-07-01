#pragma once

#include "CoreMinimal.h"
#include "ScenarioCoreTypes.generated.h"

class UStaticMesh;
class UTexture2D;
class AScenarioStaticObstacle;

// 에피소드 전반에서 공유하는 가장 작은 공통 타입들을 모아둔 파일임.

UENUM(BlueprintType)
enum class EScenarioParamValueType : uint8
{
	None,
	Bool,
	Integer,
	Float,
	String,
	Vector
};

UENUM(BlueprintType)
enum class EScenarioActorCategory : uint8
{
	DeliveryBot,
	StaticObstacle,
	Pedestrian,
	GroundRegion
};

UENUM(BlueprintType)
enum class EScenarioStaticObstaclePropCategory : uint8
{
	Unknown,
	StreetFurniture,
	TrafficControl,
	DeliveryItem,
	Utility,
	SurfaceObject
};

UENUM(BlueprintType)
enum class EScenarioStaticObstacleCollisionSourceMode : uint8
{
	// Preserves the legacy decision order: authored bounds, then MeshRoot simple collision, then fallback box.
	Auto,

	// Uses the catalog-authored bounds box, falling back to FallbackBoxExtent when full bounds are missing.
	AuthoredBoundsBox,

	// Uses every static mesh component that has simple convex collision as the obstacle collision source.
	MeshSimpleCollision
};

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioStaticObstaclePropEntry
{
	GENERATED_BODY()

	// Stable prop id used by scenario JSON and authoring/runtime lookup.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FName PropId;

	// Semantic object type exposed to evaluation, sensors, and actor tags.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FName SemanticTypeId;

	// User-facing name shown in palette and sidebar surfaces.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FText DisplayName;

	// Editor palette category used for grouping and filtering.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	EScenarioStaticObstaclePropCategory Category = EScenarioStaticObstaclePropCategory::Unknown;

	// Actor class spawned for this prop; legacy mesh entries use the default ScenarioStaticObstacle class.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Actor")
	TSoftClassPtr<AScenarioStaticObstacle> ObstacleActorClass;

	// Legacy visual mesh used before prop entries moved to obstacle actor classes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	// Palette thumbnail shown by the authoring UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Palette")
	TSoftObjectPtr<UTexture2D> ThumbnailTexture;

	// Full authored bounds size in meters; non-zero values are the authoritative prop footprint.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Placement", meta = (ClampMin = "0.0"))
	FVector BoundsSizeMeters = FVector::ZeroVector;

	// Bounds center offset from the actor root in meters; zero defaults to half-height above the root.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Placement")
	FVector BoundsCenterOffsetMeters = FVector::ZeroVector;

	// Legacy local half extent in centimeters used until authored bounds are migrated.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Placement", meta = (ClampMin = "0.0"))
	FVector FallbackBoxExtent = FVector(50.0, 50.0, 100.0);

	// Enables physical blocking collision for the obstacle's canonical bounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUsePhysicalCollision = true;

	// Enables query-only safety checks when physical collision is disabled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUseSafetyQuery = true;

	// Selects which actor primitives represent this obstacle for physics and sensor queries.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	EScenarioStaticObstacleCollisionSourceMode CollisionSourceMode =
		EScenarioStaticObstacleCollisionSourceMode::Auto;

	// Enables legacy mesh simple collision when no authored bounds size is set.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUseMeshSimpleCollision = true;

	// Enables the canonical bounds collision box when mesh collision is not authoritative.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision")
	bool bUseFallbackBoxCollision = true;

	// Optional radial safety distance in centimeters used by preview and overlap heuristics.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Collision", meta = (ClampMin = "0.0"))
	double SafetyRadius = 100.0;

	// Returns true when the prop has migrated to catalog-authored full bounds.
	bool HasAuthoredBoundsSize() const
	{
		return BoundsSizeMeters.X > KINDA_SMALL_NUMBER
			&& BoundsSizeMeters.Y > KINDA_SMALL_NUMBER
			&& BoundsSizeMeters.Z > KINDA_SMALL_NUMBER;
	}

	// Resolves the half extent used by collision, placement, and planner footprints.
	FVector ResolveBoundsExtentCm() const
	{
		if (HasAuthoredBoundsSize())
		{
			return FVector(
				FMath::Max(BoundsSizeMeters.X * 50.0, 0.0),
				FMath::Max(BoundsSizeMeters.Y * 50.0, 0.0),
				FMath::Max(BoundsSizeMeters.Z * 50.0, 0.0));
		}

		return FVector(
			FMath::Max(FallbackBoxExtent.X, 0.0),
			FMath::Max(FallbackBoxExtent.Y, 0.0),
			FMath::Max(FallbackBoxExtent.Z, 0.0));
	}

	// Resolves the local center offset used by the canonical bounds box.
	FVector ResolveBoundsCenterOffsetCm() const
	{
		const FVector boundsExtentCm = ResolveBoundsExtentCm();
		FVector centerOffsetCm = BoundsCenterOffsetMeters * 100.0;
		if (centerOffsetCm.IsNearlyZero())
		{
			centerOffsetCm.Z = boundsExtentCm.Z;
		}
		return centerOffsetCm;
	}
};

// JSON으로 옮길 수 있는 파라미터 값을 담기 위한 작은 variant 타입임.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioParamValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	EScenarioParamValueType Type = EScenarioParamValueType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 IntegerValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	double FloatValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector VectorValue = FVector::ZeroVector;
};
