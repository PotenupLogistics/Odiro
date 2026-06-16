#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioCorridorPreviewActor.generated.h"

class UMaterialInterface;
class UScenarioCorridorSurfaceCatalog;
class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;

// Editor-only spline visualization for the scenario_template corridor surface.
UCLASS()
class ODIROSIM_API AScenarioCorridorPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioCorridorPreviewActor();

	// Root kept at world origin so template-local meters map directly to world centimeters.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<USceneComponent> SceneRoot;

	// Spline rebuilt from corridor.axis.points_m for preview-only interpolation.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<USplineComponent> AxisSplineComponent;

	// Catalog used to resolve Corridor surface ids into preview material and traversability metadata.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Corridor")
	TSoftObjectPtr<UScenarioCorridorSurfaceCatalog> SurfaceCatalog;

	// Rebuilds axis and lane strip preview meshes from the current corridor template.
	void ConfigureFromCorridor(const FScenarioTemplateCorridor& corridor);

	// True when the preview has enough axis data to render lane strips.
	bool HasRenderableCorridor() const;

private:
	// Generated lane surface mesh components owned by this actor.
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> LaneMeshComponents;

	// Mesh deformed along spline sections for each lane strip.
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> LaneStripMesh;

	// Material used for walkable lane strips such as sidewalk and walkway.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WalkableGroundMaterial;

	// Material used for penalty lane strips such as road, grass, or curb.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PenaltyGroundMaterial;

	// Material used for blocked lane strips such as building or wall.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BlockedGroundMaterial;

	// Removes generated lane strip components before rebuilding the preview.
	void ClearLaneMeshes();

	// Rebuilds the preview spline points from template-local meters.
	void RebuildAxisSpline(const TArray<FVector2D>& pointsMeters);

	// 현재 axis 전체에 하나의 lane strip을 추가하고 필요하면 중심 Z offset을 적용.
	void AddLaneStrip(
		const FString& laneId,
		const FString& surfaceId,
		double minOffsetMeters,
		double maxOffsetMeters,
		double surfaceZOffsetCm);

	// Resolves fixed/range template numbers to a deterministic editor-preview value.
	static double ResolvePreviewNumber(const FScenarioTemplateNumberValue& value, double defaultValue);

	// Resolves a Corridor surface id from the configured catalog or built-in defaults.
	bool ResolveSurfaceEntry(const FString& surfaceId, FScenarioCorridorSurfaceEntry& outSurfaceEntry) const;

	// Selects a preview material from the resolved Corridor surface metadata.
	UMaterialInterface* ResolveSurfaceMaterial(const FScenarioCorridorSurfaceEntry& surfaceEntry) const;

	// Selects a fallback material when a catalog entry has no preview material assigned.
	UMaterialInterface* ResolveFallbackSurfaceMaterial(EScenarioSampleLaneType laneType) const;
};
