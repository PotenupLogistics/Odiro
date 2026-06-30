#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioPlacementPreviewActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class UStaticMesh;
class UScenarioStaticObstaclePropCatalog;
class AScenarioStaticObstacle;

UCLASS(BlueprintType)
class ODIROSIM_API AScenarioPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioPlacementPreviewActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor")
	TObjectPtr<USkeletalMeshComponent> PreviewSkeletalMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Visual")
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Visual")
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	bool ConfigureStaticObstacleProp(FName propId);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	bool ConfigureStaticObstaclePropEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	bool ConfigureActorPreviewClass(TSubclassOf<AActor> actorClass);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void SetPlacementValid(bool bCanPlace);

	// Clears any spawned or component-backed preview owned by this placement manager.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ClearPreview();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	FName GetPreviewPropId() const { return PreviewPropId; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	double GetPlacementRadius2D() const { return PlacementRadius2D; }

protected:
	// Destroys transient child preview actors during actor teardown or level transitions.
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

private:
	bool ConfigureActorPreviewFromActor(AActor* actor, FName previewId);
	bool ConfigureActorPreviewFromSpawnedActor(TSubclassOf<AActor> actorClass);
	bool ConfigureStaticObstaclePreviewActor(const FScenarioStaticObstaclePropEntry& propEntry);
	void ClearPreviewMeshes();
	void DestroyPreviewActor();
	void SetStaticMeshPreview(UStaticMesh* staticMesh);
	void SetSkeletalMeshPreview(USkeletalMesh* skeletalMesh);
	void ApplyPreviewMaterial(UMaterialInterface* material);
	void ApplyPreviewMaterialToActor(AActor* actor, UMaterialInterface* material);
	// Offsets static preview meshes so the actor origin remains the ground-contact point.
	void ApplyStaticMeshGroundAlignment();

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor")
	FName PreviewPropId;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor")
	double PlacementRadius2D = 0.0;

	// Transient actor preview spawned from a static-obstacle prop actor class.
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor;
};
