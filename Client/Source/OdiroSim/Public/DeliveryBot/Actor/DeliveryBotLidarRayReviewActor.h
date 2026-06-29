#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EpisodeLidarRayReplayDataTypes.h"
#include "DeliveryBotLidarRayReviewActor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;

// Renders one replay LiDAR ray frame as transient replay-only instanced beam geometry.
UCLASS(Blueprintable)
class ODIROSIM_API ADeliveryBotLidarRayReviewActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates the replay LiDAR ray actor and its instanced beam components.
	ADeliveryBotLidarRayReviewActor();

	// Rebuilds the beam instances from the current replay LiDAR ray frame.
	void ApplyLidarRayFrame(
		const FEpisodeLidarRayFrame* RayFrame,
		const FEpisodeLidarRayReplayManifest& Manifest,
		const FVector& ReplayWorldOffset);

	// Clears any rendered LiDAR ray beam instances.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Replay|LiDAR")
	void ClearLidarRays();

	// Shows or hides replay LiDAR rays and clears stale beam instances when hidden.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Replay|LiDAR")
	void SetLidarRaysVisible(bool bVisible);

	// Returns whether replay LiDAR rays are currently visible.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Replay|LiDAR")
	bool IsLidarRaysVisible() const { return bLidarRaysVisible; }

	// Returns the number of ray lines rendered for the current frame.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Replay|LiDAR")
	int32 GetRenderedRayCount() const { return RenderedRayCount; }

private:
	// Resolves the world-space end location used by the replay line.
	FVector ResolveRayEndLocation(const FEpisodeLidarRaySample& Ray) const;

	// Builds the world-space instance transform for one replay ray beam.
	bool TryBuildRayInstanceTransform(
		const FEpisodeLidarRaySample& Ray,
		const FVector& ReplayWorldOffset,
		FTransform& OutTransform) const;

	// Returns true when the ray can produce a meaningful line segment.
	bool ShouldDrawRay(const FEpisodeLidarRaySample& Ray) const;

	// Actor root scene component.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<USceneComponent> SceneRoot;

	// Instanced beam component that owns replay LiDAR rays with a hit result. 
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> HitRayInstances;

	// Instanced beam component that owns replay LiDAR rays without a hit result.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> MissRayInstances;

	// Maximum number of ray instances rendered per frame to keep replay capture responsive.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "1"))
	int32 MaxVisibleRays = 2000;

	// Default X-axis length in centimeters of the beam static mesh asset.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.001"))
	double RayBeamLengthCm = 10.0;

	// Y/Z scale applied to every beam instance to control visible ray thickness.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.001"))
	double RayBeamThicknessScale = 0.3;

	// Position quantization grid used to merge duplicate ray segments; zero disables merging.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.0"))
	double DuplicateRayMergeGridCm = 1.0;

	// Whether miss rays should be rendered using the manifest miss color.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	bool bDrawMissRays = false;

	// Whether replay LiDAR rays are visible when a frame is applied.
	UPROPERTY(Transient)
	bool bLidarRaysVisible = false;

	// Number of ray beam instances currently stored in the hit and miss components.
	int32 RenderedRayCount = 0;
};
