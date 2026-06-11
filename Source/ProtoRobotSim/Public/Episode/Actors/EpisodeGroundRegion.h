#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeSpecTypes.h"
#include "EpisodeGroundRegion.generated.h"

class UDecalComponent;
class UEpisodePlaceableComponent;
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

	// 에디터에서 선택/gizmo(이동·yaw 회전) 대상으로 삼기 위한 식별 component임.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

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
	void ApplyMaterialSettings();

	UPROPERTY(EditAnywhere, Category = "Episode|Visual")
	TObjectPtr<UMaterialInterface> WalkableGroundMaterial;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual")
	TObjectPtr<UMaterialInterface> PenaltyGroundMaterial;

	UPROPERTY(EditAnywhere, Category = "Episode|Visual")
	TObjectPtr<UMaterialInterface> BlockedAreaMaterial;

	UPROPERTY(EditAnywhere, Category = "Episode|Collision", meta = (ClampMin = "1.0"))
	double BlockedCollisionHeightCm = 200.0;

	UPROPERTY(EditAnywhere, Category = "Episode|Collision", meta = (ClampMin = "1.0"))
	double GroundCollisionThicknessCm = 1.0;
};
