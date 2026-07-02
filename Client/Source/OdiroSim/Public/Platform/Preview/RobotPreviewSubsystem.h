#pragma once

#include "CoreMinimal.h"
#include "Platform/Preview/RobotPreviewSceneActor.h"
#include "Platform/RobotProfileSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "RobotPreviewSubsystem.generated.h"

class UTextureRenderTarget2D;
class AActor;
class ACameraActor;
class APlayerController;
class ASceneCapture2D;
class USceneCaptureComponent2D;

// Selects how the robot preview scene is presented to the user.
enum class ERobotPreviewRenderMode : uint8
{
	PlayerViewport,
	SceneCaptureRenderTarget
};

// Owns transient robot preview resources for WBP_RobotConfigEditor.
UCLASS(BlueprintType)
class ODIROSIM_API URobotPreviewSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Releases preview resources when the owning world is torn down.
	virtual void Deinitialize() override;

	// Starts or reuses the preview scene for one widget owner.
	bool StartPreview(UObject* Owner, const FRobotProfileSettings& Settings);

	// Stops the preview when requested by the current widget owner.
	void StopPreview(const UObject* Owner);

	// Returns the render target that the robot config widget displays.
	UTextureRenderTarget2D* GetRenderTarget() const { return PreviewRenderTarget; }

	// Returns true when the legacy SceneCapture render target backend is active.
	bool IsUsingSceneCaptureRenderTarget() const;

	// Changes the preview presentation backend and resets active transient resources.
	void SetRenderMode(ERobotPreviewRenderMode NewRenderMode);

	// Applies editable profile values to the preview state.
	bool ApplyPreviewSettings(const FRobotProfileSettings& Settings);

	// Draws a LiDAR ray snapshot from the current preview settings.
	bool DrawLidarPreviewRays();

	// Applies preview-only LiDAR overlay layer and density options.
	void SetLidarDisplayOptions(const FRobotPreviewLidarDisplayOptions& Options);

	// Clears the currently drawn LiDAR ray snapshot.
	void ClearLidarPreviewRays();

	// Zooms the preview camera toward or away from the robot focus point.
	void AddCameraZoom(float WheelDelta);

	// Orbits the preview camera around the robot focus point.
	void AddCameraOrbit(const FVector2D& CursorDelta);

	// Rotates the preview robot yaw state by the requested degrees.
	void AddRobotYawDegrees(float DeltaDegrees);

	// Restores the preview robot yaw state to the front view.
	void ResetRobotYaw();

	// Returns user-facing preview status text for the widget overlay.
	const FString& GetStatusText() const { return StatusText; }

	// Returns the current preview robot yaw in degrees.
	float GetRobotYawDegrees() const { return RobotYawDegrees; }

private:
	// Creates the transient render target used by the robot preview image.
	UTextureRenderTarget2D* CreatePreviewRenderTarget();

	// Creates the transient scene actor when needed.
	bool EnsurePreviewScene();

	// Creates resources for the currently selected preview render backend.
	bool EnsurePreviewRenderBackend();

	// Creates the PlayerViewport camera backend and takes over the active view target.
	bool EnsurePlayerViewportBackend();

	// Creates the legacy SceneCapture render-target backend.
	bool EnsureSceneCaptureBackend();

	// Spawns the camera used by the PlayerViewport backend.
	ACameraActor* SpawnPreviewCameraActor();

	// Spawns the preview SceneCapture actor.
	ASceneCapture2D* SpawnPreviewCaptureActor();

	// Rebuilds capture visibility from preview-only actors.
	void RefreshCaptureShowOnlyActors();

	// Updates the fixed capture camera transform for the current preview bounds.
	void UpdatePreviewCaptureView();

	// Updates the PlayerViewport camera transform for the current preview bounds.
	void UpdatePlayerViewportView();

	// Calculates the shared preview camera transform from orbit state and preview bounds.
	FTransform CalculatePreviewCameraTransform();

	// Updates the active preview backend after scene, camera, or overlay state changes.
	void RefreshPreviewView();

	// Initializes camera distance once from the current preview bounds.
	void InitializeCameraViewFromPreviewBounds();

	// Captures the configured preview scene into the render target.
	void CapturePreviewScene();

	// Re-captures after render assets and material states have had time to initialize.
	void ScheduleDeferredCaptures();

	// Returns the SceneCapture component for the current capture actor.
	USceneCaptureComponent2D* GetPreviewCaptureComponent() const;

	// Releases transient render resources and owner state.
	void CleanupPreviewResources();

	// Restores the view target that was active before PlayerViewport preview began.
	void RestorePreviousViewTarget();

	// Returns whether the supplied owner still owns the active preview.
	bool IsCurrentOwner(const UObject* Owner) const;

	// Refreshes the status text from the current preview settings and yaw.
	void RefreshStatusText();

	// Widget instance that currently owns the preview lifecycle.
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> PreviewOwner;

	// Render target displayed by WBP_RobotConfigEditor.
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget;

	// Preview-only actor containing stage, robot, and sensor marker components.
	UPROPERTY(Transient)
	TObjectPtr<ARobotPreviewSceneActor> PreviewSceneActor;

	// SceneCapture2D actor rendering preview-only actors into PreviewRenderTarget.
	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> PreviewCaptureActor;

	// Camera actor used when the preview is rendered through the game viewport.
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> PreviewCameraActor;

	// PlayerController whose ViewTarget is temporarily owned by the preview.
	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PreviewPlayerController;

	// ViewTarget that must be restored when PlayerViewport preview stops.
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PreviousViewTarget;

	// Last editable profile values applied to the preview.
	UPROPERTY(Transient)
	FRobotProfileSettings CurrentSettings;

	// Preview-only LiDAR overlay options.
	FRobotPreviewLidarDisplayOptions LidarDisplayOptions;

	// Active preview presentation backend; SceneCapture remains available as fallback.
	ERobotPreviewRenderMode RenderMode = ERobotPreviewRenderMode::PlayerViewport;

	// Current visual yaw applied to the preview robot.
	float RobotYawDegrees = 0.0f;

	// True after the preview camera has been initialized for the current preview owner.
	bool bCameraViewInitialized = false;

	// Current orbit yaw around the preview robot focus point.
	float CameraOrbitYawDegrees = -150.0f;

	// Current orbit pitch around the preview robot focus point.
	float CameraOrbitPitchDegrees = 24.0f;

	// Current camera distance from the preview robot focus point.
	float CameraDistanceCm = 320.0f;

	// Far-away world offset that isolates preview actors from gameplay actors.
	FVector PreviewWorldOffset = FVector(650000.0, 0.0, 0.0);

	// User-facing status shown over the preview render target.
	FString StatusText = TEXT("Preview 준비 중");
};
