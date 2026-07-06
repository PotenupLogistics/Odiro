#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryBotLidarRayReviewActor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;
struct FDeliveryBotLidarSensorConfigInfo;
struct FEpisodeLidarRayFrame;
struct FEpisodeReplayRobotFrame;

// Renders replay-time LiDAR sensor range overlays using the same beam style as Robot Preview.
UCLASS(Blueprintable)
class ODIROSIM_API ADeliveryBotLidarRayReviewActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates the replay LiDAR range actor and its preview-style beam components.
	ADeliveryBotLidarRayReviewActor();

	// Rebuilds the sensor range overlay from the current replay robot frame and LiDAR settings.
	void ApplyLidarRayFrame(
		const FEpisodeReplayRobotFrame& RobotFrame,
		const FDeliveryBotLidarSensorConfigInfo& LidarConfig,
		const FEpisodeLidarRayFrame* RayFrame,
		const FVector& ReplayWorldOffset);

	// Clears any rendered LiDAR range beam instances.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Replay|LiDAR")
	void ClearLidarRays();

	// Shows or hides all replay LiDAR beams and clears stale instances when hidden.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Replay|LiDAR")
	void SetLidarRaysVisible(bool bVisible);

	// Returns whether any replay LiDAR beams are currently visible.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Replay|LiDAR")
	bool IsLidarRaysVisible() const { return bLidarRaysVisible; }

	// Shows or hides replay LiDAR sensor ray beams without affecting distance overlays.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Replay|LiDAR")
	void SetLidarSensorRaysVisible(bool bVisible);

	// Returns whether replay LiDAR sensor ray beams are currently visible.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Replay|LiDAR")
	bool AreLidarSensorRaysVisible() const { return bLidarSensorRaysVisible; }

	// Shows or hides replay LiDAR distance and range overlays without affecting sensor rays.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Replay|LiDAR")
	void SetLidarDistanceOverlayVisible(bool bVisible);

	// Returns whether replay LiDAR distance and range overlays are currently visible.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Replay|LiDAR")
	bool IsLidarDistanceOverlayVisible() const { return bLidarDistanceOverlayVisible; }

	// Returns the number of preview-style beam instances rendered for the current frame.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Replay|LiDAR")
	int32 GetRenderedRayCount() const { return RenderedRayCount; }

private:
	// Applies one operation to every preview-style beam component owned by this actor.
	void ForEachLidarBeamComponent(TFunctionRef<void(UInstancedStaticMeshComponent*)> Operation) const;

	// Applies one operation to every actual LiDAR sensor ray component.
	void ForEachSensorRayComponent(TFunctionRef<void(UInstancedStaticMeshComponent*)> Operation) const;

	// Applies one operation to every LiDAR distance and range overlay component.
	void ForEachDistanceOverlayComponent(TFunctionRef<void(UInstancedStaticMeshComponent*)> Operation) const;

	// Synchronizes actor and component visibility with the split replay LiDAR layer flags.
	void ApplyLidarLayerVisibility();

	// Draws one world-space beam and updates the rendered counter when it succeeds.
	bool AddWorldBeam(
		UInstancedStaticMeshComponent* Component,
		const FVector& StartLocationCm,
		const FVector& EndLocationCm,
		float ThicknessScale);

	// Draws one robot-local yaw/pitch ray transformed into replay world space.
	bool AddRobotLocalRay(
		UInstancedStaticMeshComponent* Component,
		const FTransform& RobotWorldTransform,
		const FVector& SensorLocationLocalCm,
		float YawDegree,
		float PitchDegree,
		float RangeCm,
		float ThicknessScale);

	// Draws one preview-style horizontal range ring in robot-local space.
	void AddRangeRing(
		UInstancedStaticMeshComponent* Component,
		const FTransform& RobotWorldTransform,
		const FVector& SensorLocationLocalCm,
		float RadiusCm,
		float ThicknessScale);

	// Draws center and front-boundary rays for one distance threshold.
	void AddRangeRaySet(
		UInstancedStaticMeshComponent* Component,
		const FTransform& RobotWorldTransform,
		const FVector& SensorLocationLocalCm,
		float RangeCm,
		float FrontHalfAngleDegree,
		float ThicknessScale);

	// Actor root scene component.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<USceneComponent> SceneRoot;

	// Beam instances for front-facing 1D or highlighted 2D rays.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarPrimaryRayInstances;

	// Beam instances for muted 2D peripheral rays.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarSecondaryRayInstances;

	// Beam instances for sampled 3D yaw and pitch rays.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarThreeDRayInstances;

	// Faint full-coverage beam instances for OS1 rotating 3D LiDAR replay.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarThreeDFullRayInstances;

	// Beam instances for the full scan range ring.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarRangeRingInstances;

	// Beam instances for the slowdown distance ring.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarSlowRangeRingInstances;

	// Beam instances for the stop distance ring.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarStopRangeRingInstances;

	// Beam instances for the obstacle-warning distance ring.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarObstacleWarningRangeRingInstances;

	// Beam instances that draw the slowdown threshold as front-facing rays.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarSlowRangeRayInstances;

	// Beam instances that draw the stop threshold as front-facing rays.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarStopRangeRayInstances;

	// Beam instances that draw the obstacle-warning threshold as front-facing rays.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarObstacleWarningRangeRayInstances;

	// Beam instances for front-half-angle boundary lines.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|Replay|LiDAR")
	TObjectPtr<UInstancedStaticMeshComponent> LidarFrontBoundaryInstances;

	// Maximum logical scan rays rendered per frame to keep replay capture responsive.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "1"))
	int32 MaxVisibleScanRays = 360;

	// Default X-axis length in centimeters of the beam static mesh asset.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.001"))
	double RayBeamLengthCm = 10.0;

	// Y/Z scale applied to normal scan ray beam instances.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.001"))
	double RayBeamThicknessScale = 0.64;

	// Y/Z scale applied to range rings.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.001"))
	double RangeBeamThicknessScale = 0.4;

	// Y/Z scale applied to stop/slow threshold rays.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|Replay|LiDAR", meta = (ClampMin = "0.001"))
	double RangeRayBeamThicknessScale = 0.54;

	// Whether any replay LiDAR beams are visible when a frame is applied.
	UPROPERTY(Transient)
	bool bLidarRaysVisible = false;

	// Whether actual replay LiDAR sensor ray beams are visible when a frame is applied.
	UPROPERTY(Transient)
	bool bLidarSensorRaysVisible = false;

	// Whether replay LiDAR distance and range overlay beams are visible when a frame is applied.
	UPROPERTY(Transient)
	bool bLidarDistanceOverlayVisible = false;

	// Number of preview-style beam instances currently stored in all components.
	int32 RenderedRayCount = 0;
};
