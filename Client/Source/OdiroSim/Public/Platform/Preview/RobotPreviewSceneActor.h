#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Platform/RobotProfileSettings.h"
#include "RobotPreviewSceneActor.generated.h"

class UPointLightComponent;
class UInstancedStaticMeshComponent;
class UMeshComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

// Preview-only LiDAR sample density used to cap visual clutter without changing real ray counts.
enum class ERobotPreviewLidarDisplayDensity : uint8
{
	Sparse,
	Standard,
	Dense
};

// Preview-only toggles for LiDAR overlay layers shown in the render target.
struct FRobotPreviewLidarDisplayOptions
{
	// True when sampled LiDAR beam geometry should be shown.
	bool bShowRays = true;

	// True when max/slow/stop range rings and front boundary lines should be shown.
	bool bShowRange = true;

	// True when sampled LiDAR end points should be shown.
	bool bShowPoints = false;

	// Controls how many logical rays are sampled into the preview overlay.
	ERobotPreviewLidarDisplayDensity Density = ERobotPreviewLidarDisplayDensity::Standard;
};

// Visual-only robot configuration stage captured by URobotPreviewSubsystem.
UCLASS()
class ODIROSIM_API ARobotPreviewSceneActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates preview-only stage, robot body, wheel, and sensor marker components.
	ARobotPreviewSceneActor();

	// Applies the editable robot profile values to preview-only components.
	void ApplySettings(const FRobotProfileSettings& Settings);

	// Applies preview-only LiDAR overlay toggles without modifying robot profile values.
	void SetLidarDisplayOptions(const FRobotPreviewLidarDisplayOptions& Options);

	// Rotates the preview robot while keeping the capture camera fixed.
	void SetRobotYawDegrees(float YawDegrees);

	// Draws preview-only LiDAR rays from the current sensor origin and profile settings.
	void DrawLidarPreviewRays();

	// Clears preview-only LiDAR ray and range geometry.
	void ClearLidarPreviewRays();

	// Returns whether preview-only LiDAR rays are currently displayed.
	bool AreLidarPreviewRaysVisible() const { return bLidarPreviewRaysVisible; }

	// Returns the number of LiDAR ray beam instances rendered in the last preview draw.
	int32 GetRenderedLidarPreviewRayCount() const { return RenderedLidarPreviewRayCount; }

	// Returns the number of LiDAR point instances rendered in the last preview draw.
	int32 GetRenderedLidarPreviewPointCount() const { return RenderedLidarPreviewPointCount; }

	// Returns the number of LiDAR rays represented by the current profile values.
	int32 GetActualLidarPreviewRayCount() const { return ActualLidarPreviewRayCount; }

	// Returns the world point the preview camera should look at.
	FVector GetPreviewFocusLocation() const;

	// Returns the approximate robot/stage radius used for camera distance.
	float GetPreviewRadiusCm() const;

	// Appends actors that must be visible to the preview SceneCapture show-only list.
	void AddShowOnlyActors(TArray<AActor*>& OutActors);

private:
	// Assigns a mesh to one preview component and disables gameplay collision.
	static void ConfigurePreviewMeshComponent(UStaticMeshComponent* Component, UStaticMesh* Mesh);

	// Configures the preview skeletal mesh component with no gameplay collision.
	static void ConfigurePreviewSkeletalMeshComponent(USkeletalMeshComponent* Component);

	// Applies component colors through a per-component dynamic material when available.
	static void ApplyPreviewColor(UMeshComponent* Component, const FLinearColor& Color);

	// Updates wheel preview component transforms from body width and wheel base.
	void RefreshWheelTransforms(const FRobotProfileSettings& Settings);

	// Updates the body visual scale from profile dimensions while preserving the configured mesh.
	void RefreshBodyTransform(const FRobotProfileSettings& Settings);

	// Applies profile dimensions to one mesh component using its imported bounds.
	void ApplyMeshComponentBoundsTransform(UMeshComponent* Component, const FRobotProfileSettings& Settings);

	// Configures one preview-only instanced mesh component for LiDAR overlay beams.
	void ConfigurePreviewInstancedMeshComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh);

	// Rebuilds LiDAR ray and range geometry when the active preview values change.
	void RefreshLidarPreviewRays();

	// Clears all LiDAR overlay instances without changing the visibility state flag.
	void ClearLidarPreviewGeometry();

	// Adds one local-space beam instance to the supplied LiDAR overlay component.
	bool AddLidarPreviewBeam(
		UInstancedStaticMeshComponent* Component,
		const FVector& StartLocationCm,
		const FVector& EndLocationCm,
		float ThicknessScale);

	// Adds one horizontal range ring using local-space beam segments.
	void AddLidarPreviewRangeRing(
		UInstancedStaticMeshComponent* Component,
		const FVector& CenterLocationCm,
		float RadiusCm,
		float ThicknessScale);

	// Adds one LiDAR ray using a local-space yaw and pitch direction.
	void AddLidarPreviewRay(
		UInstancedStaticMeshComponent* Component,
		const FVector& SensorLocationCm,
		float YawDegree,
		float PitchDegree,
		float RangeCm,
		float ThicknessScale);

	// Adds one LiDAR end point marker instance in local preview space.
	void AddLidarPreviewPoint(
		UInstancedStaticMeshComponent* Component,
		const FVector& LocationCm,
		float DiameterCm);

	// Preview actor root.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<USceneComponent> SceneRoot;

	// Root rotated by preview yaw controls.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<USceneComponent> RobotRoot;

	// Flat floor mesh that gives the render target spatial context.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UStaticMeshComponent> StageFloor;

	// Simple body mesh scaled from robot.body dimensions.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UStaticMeshComponent> BodyVisual;

	// Actual DeliveryBot skeletal visual used by the preview when available.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<USkeletalMeshComponent> SkeletalBodyVisual;

	// Sphere marker placed at the runtime LiDAR sensor origin height.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UStaticMeshComponent> LidarMarker;

	// Root that owns all preview-only LiDAR ray and range overlay components.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<USceneComponent> LidarRayRoot;

	// Beam instances for front-facing 1D or highlighted 2D rays.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarPrimaryRayInstances;

	// Beam instances for muted 2D peripheral rays.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarSecondaryRayInstances;

	// Beam instances for sampled 3D yaw and pitch rays.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarThreeDRayInstances;

	// Beam instances for the full scan range ring.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarRangeRingInstances;

	// Beam instances for the slowdown distance ring.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarSlowRangeRingInstances;

	// Beam instances for the stop distance ring.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarStopRangeRingInstances;

	// Beam instances for front-half-angle boundary lines.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarFrontBoundaryInstances;

	// Point instances for sampled max-range LiDAR end locations.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UInstancedStaticMeshComponent> LidarEndPointInstances;

	// Four visual-only wheel meshes positioned from robot.body wheel base.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;

	// Spot key light for preview-only meshes in the SceneCapture.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<USpotLightComponent> KeyLight;

	// Soft fill light for the preview stage.
	UPROPERTY(VisibleAnywhere, Category = "Platform|RobotPreview")
	TObjectPtr<UPointLightComponent> FillLight;

	// Last settings applied to the preview actor.
	FRobotProfileSettings CurrentSettings;

	// Preview-only LiDAR display layer and density settings.
	FRobotPreviewLidarDisplayOptions LidarDisplayOptions;

	// True when the actual skeletal DeliveryBot mesh is active.
	bool bUsingSkeletalBodyMesh = false;

	// True when no actual DeliveryBot mesh could be loaded and the cube fallback is active.
	bool bUsingFallbackBodyMesh = true;

	// Last yaw applied to the preview robot root.
	float CurrentYawDegrees = 0.0f;

	// Default X-axis length in centimeters of the beam mesh used for LiDAR overlays.
	float LidarBeamMeshLengthCm = 10.0f;

	// True when preview-only LiDAR rays should be rebuilt after setting changes.
	bool bLidarPreviewRaysVisible = false;

	// Number of LiDAR ray beam instances rendered by the last draw request.
	int32 RenderedLidarPreviewRayCount = 0;

	// Number of LiDAR point instances rendered by the last draw request.
	int32 RenderedLidarPreviewPointCount = 0;

	// Number of logical LiDAR rays represented by the active settings.
	int32 ActualLidarPreviewRayCount = 0;
};
