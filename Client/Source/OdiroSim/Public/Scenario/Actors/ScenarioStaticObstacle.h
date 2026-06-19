#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioStaticObstacle.generated.h"

class UScenarioObstacleCollisionComponent;
class UScenarioPlaceableComponent;
class UScenarioStaticObstaclePropCatalog;
class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

// Runtime and editor actor for static scenario obstacles.
UCLASS(BlueprintType)
class ODIROSIM_API AScenarioStaticObstacle : public AActor
{
	GENERATED_BODY()

public:
	// Creates the obstacle components used by authored and runtime placements.
	AScenarioStaticObstacle();

	// Spawns an obstacle actor and applies resolved catalog metadata before returning it to the caller.
	static AScenarioStaticObstacle* SpawnConfigured(
		UWorld* world,
		TSubclassOf<AScenarioStaticObstacle> obstacleClass,
		const FTransform& transform,
		const FScenarioStaticObstaclePropEntry& propEntry,
		FString& outFailureReason);

	// Stable actor transform root; its origin is the semantic ground-contact point.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<USceneComponent> SceneRoot;

	// Visual mesh aligned so its local bottom rests on SceneRoot Z.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UStaticMeshComponent> MeshRoot;

	// Fallback collision volume used when the prop mesh has no simple collision.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UBoxComponent> CollisionBoundsComponent;

	// Selection and authoring metadata for editor/runtime lookup.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioPlaceableComponent> PlaceableComponent;

	// Collision reporting metadata used by scenario evaluation.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioObstacleCollisionComponent> ObstacleCollisionComponent;

	// Reapplies configured mesh and collision state after editor construction.
	virtual void OnConstruction(const FTransform& transform) override;

	// Sets the soft mesh reference and applies it immediately when loadable.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Mesh")
	bool SetStaticMeshAsset(TSoftObjectPtr<UStaticMesh> inStaticMeshAsset);

	// Sets a loaded mesh directly and refreshes collision/ground alignment.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Mesh")
	void SetStaticMesh(UStaticMesh* inStaticMesh);

	// Loads and applies the configured static mesh asset.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Mesh")
	bool ApplyConfiguredStaticMesh();

	// Applies catalog metadata, mesh, collision settings, and semantic tags.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Catalog")
	bool ApplyPropEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	// Applies a prop entry from the configured catalog.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Catalog")
	bool ApplyDefaultPropById(FName inPropId);

	// Returns the current bounds used by 2D placement validation.
	UFUNCTION(BlueprintPure, Category = "Scenario|Placement")
	bool GetPlacementBounds(
		FVector& outOrigin,
		FVector& outBoxExtent,
		FVector2D& outHalfSize2D,
		double& outRadius2D) const;

	// Returns the radius used by 2D placement validation.
	UFUNCTION(BlueprintPure, Category = "Scenario|Placement")
	double GetPlacementRadius2D() const;

	// Stable prop id resolved from the static obstacle catalog.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Semantic")
	FName PropId;

	// Semantic object type used by evaluation and perception tags.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Semantic")
	FName SemanticTypeId;

	// User-facing display name from the prop catalog.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Semantic")
	FText PropDisplayName;

	// Catalog category used by the editor palette.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Semantic")
	EScenarioStaticObstaclePropCategory PropCategory = EScenarioStaticObstaclePropCategory::Unknown;

	// Catalog used to resolve PropId when applying default props.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	// Mesh asset configured by the selected catalog entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Mesh", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;

	// Local half extent used for fallback bounds and collision.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Placement", meta = (ClampMin = "0.0"))
	FVector FallbackBoxExtent = FVector(50.0, 50.0, 100.0);

	// Allows placement validation to use fallback bounds when no mesh is available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Placement")
	bool bUseFallbackBoundsWhenMeshMissing = true;

	// Enables the mesh's simple collision when the mesh provides one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Collision")
	bool bUseMeshSimpleCollision = true;

	// Enables the fallback collision box when mesh simple collision is unavailable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Collision")
	bool bUseFallbackBoxCollision = true;

private:
	// Resolves a prop entry from the configured catalog.
	bool TryFindConfiguredPropEntry(FName inPropId, FScenarioStaticObstaclePropEntry& outPropEntry) const;

	// Refreshes collision primitives from current prop settings.
	void ApplyCollisionSettings();

	// Maintains the actor tag used by semantic object queries.
	void ApplyObjectTypeActorTag();

	// Offsets the visual mesh so actor Z remains the ground-contact plane.
	void ApplyMeshGroundAlignment();
};
