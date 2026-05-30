#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeSpecTypes.h"
#include "EpisodeGroundRegion.generated.h"

class UBoxComponent;
class USceneComponent;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeGroundRegion : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeGroundRegion();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UBoxComponent> RegionBoundsComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	FEpisodeGroundRegionSpec RegionSpec;

	UFUNCTION(BlueprintCallable, Category = "Episode")
	void ConfigureRegion(const FEpisodeGroundRegionSpec& InRegionSpec);

	UFUNCTION(BlueprintPure, Category = "Episode")
	bool ContainsWorldLocation2D(const FVector& WorldLocation) const;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyCollisionSettings();
	void DrawDebugRegion(double LifeTimeSeconds) const;
	FColor GetDebugColor() const;
	FString GetRegionTypeLabel() const;

	UPROPERTY(EditAnywhere, Category = "Episode|Debug", meta = (ClampMin = "0.0"))
	double DebugDrawZOffsetCm = 4.0;

	UPROPERTY(EditAnywhere, Category = "Episode|Debug", meta = (ClampMin = "0.0"))
	double DebugDrawHalfHeightCm = 2.0;

	UPROPERTY(EditAnywhere, Category = "Episode|Debug", meta = (ClampMin = "0.0"))
	double DebugDrawLifeTimeSeconds = 0.35;

	UPROPERTY(EditAnywhere, Category = "Episode|Debug", meta = (ClampMin = "0.0"))
	double DebugLineThickness = 4.0;

	UPROPERTY(EditAnywhere, Category = "Episode|Collision", meta = (ClampMin = "1.0"))
	double BlockedCollisionHeightCm = 140.0;
};
