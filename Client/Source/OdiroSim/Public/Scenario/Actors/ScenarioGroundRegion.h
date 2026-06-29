#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioGroundRegion.generated.h"

class UDecalComponent;
class UProceduralMeshComponent;
class UScenarioPlaceableComponent;
class UScenarioCorridorSurfaceCatalog;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UWorld;

// Runtime and editor actor for configured ground-region surfaces.
UCLASS(BlueprintType)
class ODIROSIM_API AScenarioGroundRegion : public AActor
{
	GENERATED_BODY()

public:
	AScenarioGroundRegion();

	// Spawns a region actor and applies the supplied region spec before returning it to the caller.
	static AScenarioGroundRegion* SpawnConfigured(
		UWorld* world,
		TSubclassOf<AScenarioGroundRegion> regionClass,
		const FScenarioGroundRegionSpec& regionSpec,
		FString& outFailureReason);

	// Root transform shared by the visual surface and collision proxy.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<USceneComponent> SceneRoot;

	// Hidden box proxy that preserves GroundRegion collision and query semantics.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UStaticMeshComponent> RegionBoundsComponent;

	// Flat procedural surface used for editor/runtime GroundRegion visualization.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UProceduralMeshComponent> RegionVisualMeshComponent;

	// Optional decal hook retained for Blueprint/editor presentation extensions.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UDecalComponent> RegionDecalComponent;

	// 에디터에서 선택/gizmo(이동·yaw 회전) 대상으로 삼기 위한 식별 component임.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioPlaceableComponent> PlaceableComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	FScenarioGroundRegionSpec RegionSpec;

	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void ConfigureRegion(const FScenarioGroundRegionSpec& inRegionSpec);

	UFUNCTION(BlueprintPure, Category = "Scenario")
	bool ContainsWorldLocation2D(const FVector& worldLocation) const;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyCollisionSettings();
	void ApplyMaterialSettings();
	// Rebuilds the local flat procedural surface from the current rectangle spec.
	void RebuildVisualMesh();
	// Returns whether this GroundRegion owns its visual surface or delegates it to CityBuildings assets.
	bool ShouldRenderVisualMesh() const;
	UMaterialInterface* ResolveSurfaceCatalogMaterial() const;

	// Shared surface catalog for matching generated runtime ground-region visuals to editor Corridor preview.
	UPROPERTY(EditAnywhere, Category = "Scenario|Visual")
	TSoftObjectPtr<UScenarioCorridorSurfaceCatalog> SurfaceCatalog;

	UPROPERTY(EditAnywhere, Category = "Scenario|Collision", meta = (ClampMin = "1.0"))
	double BlockedCollisionHeightCm = 200.0;

	UPROPERTY(EditAnywhere, Category = "Scenario|Collision", meta = (ClampMin = "1.0"))
	double GroundCollisionThicknessCm = 1.0;
};
