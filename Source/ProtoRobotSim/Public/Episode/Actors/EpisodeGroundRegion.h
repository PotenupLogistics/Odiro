#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeSpecTypes.h"
#include "EpisodeGroundRegion.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeGroundRegion : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeGroundRegion();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UStaticMeshComponent> RegionBoundsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UDecalComponent> RegionDecalComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	FEpisodeGroundRegionSpec RegionSpec;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void ConfigureRegion(const FEpisodeGroundRegionSpec& inRegionSpec);

	UFUNCTION(BlueprintPure, Category = "Episode")
	bool ContainsWorldLocation2D(const FVector& worldLocation) const;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyCollisionSettings();
	void UpdateDecalVisualization();
	void CreateOrUpdateDecalMaterialInstance();
	FLinearColor GetRegionColor() const;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual")
	bool bUseDecalVisualization = true;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual")
	TObjectPtr<UMaterialInterface> GroundDecalMaterial;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual")
	TObjectPtr<UMaterialInterface> BlockedAreaMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundDecalMaterialInstance;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual", meta = (ClampMin = "0.0"))
	double DecalZOffsetCm = 4.0;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual", meta = (ClampMin = "1.0"))
	double DecalProjectionDepthCm = 120.0;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DecalOpacity = 0.22;

	UPROPERTY(EditAnywhere, Category = "Episode|Collision", meta = (ClampMin = "1.0"))
	double BlockedCollisionHeightCm = 200.0;
};
