#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePlacementPreviewActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodePlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AEpisodePlacementPreviewActor();

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

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	bool ConfigureStaticObstacleProp(FName propId);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	bool ConfigureStaticObstaclePropEntry(const FEpisodeStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	bool ConfigureActorPreviewClass(TSubclassOf<AActor> actorClass);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetPlacementValid(bool bCanPlace);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	FName GetPreviewPropId() const { return PreviewPropId; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	double GetPlacementRadius2D() const { return PlacementRadius2D; }

private:
	void ClearPreviewMeshes();
	void SetStaticMeshPreview(UStaticMesh* staticMesh);
	void SetSkeletalMeshPreview(USkeletalMesh* skeletalMesh);
	void ApplyPreviewMaterial(UMaterialInterface* material);

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	FName PreviewPropId;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	double PlacementRadius2D = 0.0;
};
