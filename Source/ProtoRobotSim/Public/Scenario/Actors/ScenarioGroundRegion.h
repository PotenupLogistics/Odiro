#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioGroundRegion.generated.h"

class UDecalComponent;
class UScenarioPlaceableComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AScenarioGroundRegion : public AActor
{
	GENERATED_BODY()

public:
	AScenarioGroundRegion();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UStaticMeshComponent> RegionBoundsComponent;

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

	UPROPERTY(EditAnywhere, Category = "Scenario|Visual")
	TObjectPtr<UMaterialInterface> WalkableGroundMaterial;

	UPROPERTY(EditAnywhere, Category = "Scenario|Visual")
	TObjectPtr<UMaterialInterface> PenaltyGroundMaterial;

	UPROPERTY(EditAnywhere, Category = "Scenario|Visual")
	TObjectPtr<UMaterialInterface> BlockedAreaMaterial;

	UPROPERTY(EditAnywhere, Category = "Scenario|Collision", meta = (ClampMin = "1.0"))
	double BlockedCollisionHeightCm = 200.0;

	UPROPERTY(EditAnywhere, Category = "Scenario|Collision", meta = (ClampMin = "1.0"))
	double GroundCollisionThicknessCm = 1.0;
};
