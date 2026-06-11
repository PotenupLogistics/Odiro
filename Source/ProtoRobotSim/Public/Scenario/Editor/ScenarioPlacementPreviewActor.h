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

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AScenarioPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioPlacementPreviewActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor")
	TObjectPtr<USkeletalMeshComponent> PreviewSkeletalMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Visual")
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Visual")
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	bool ConfigureStaticObstacleProp(FName propId);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	bool ConfigureStaticObstaclePropEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	bool ConfigureActorPreviewClass(TSubclassOf<AActor> actorClass);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetPlacementValid(bool bCanPlace);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	FName GetPreviewPropId() const { return PreviewPropId; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	double GetPlacementRadius2D() const { return PlacementRadius2D; }

private:
	bool ConfigureActorPreviewFromActor(AActor* actor, FName previewId);
	bool ConfigureActorPreviewFromSpawnedActor(TSubclassOf<AActor> actorClass);
	void ClearPreviewMeshes();
	void SetStaticMeshPreview(UStaticMesh* staticMesh);
	void SetSkeletalMeshPreview(USkeletalMesh* skeletalMesh);
	void ApplyPreviewMaterial(UMaterialInterface* material);

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	FName PreviewPropId;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	double PlacementRadius2D = 0.0;
};
