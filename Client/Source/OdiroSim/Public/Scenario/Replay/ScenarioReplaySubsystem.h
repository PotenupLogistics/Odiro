#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeReplayDataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioReplaySubsystem.generated.h"

class ADeliveryBotReplayActor;
class ADeliveryBotPointCloudReviewActor;
class AActor;
class ASceneCapture2D;
class UScenarioReplayDeveloperSettings;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
struct FScenarioPlaceableInstanceSpec;
struct FScenarioStaticObstaclePropEntry;
struct FScenarioWorldSpec;

// Camera mode used by the embedded replay SceneCapture.
UENUM(BlueprintType)
enum class EScenarioReplayCameraMode : uint8
{
	// Orthographic top-down camera that follows the replay robot.
	TopDown,

	// User-controlled perspective camera moved by replay viewer input.
	Free,

	// Perspective camera mounted at the replay robot's forward body offset.
	VehicleFront
};

// Owns embedded replay loading, playback state, replay actors, and render target capture.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioReplaySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Loads replay artifacts from an episode directory and prepares the render target.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	bool LoadEpisodeReplay(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Unloads transient replay actors, scenario actors, and render resources.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void UnloadReplay();

	// Starts playback from the current replay time.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void Play();

	// Pauses playback without unloading replay artifacts.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void Pause();

	// Stops playback and seeks to the first frame when loaded.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void Stop();

	// Seeks to the nearest replay frame for the requested time.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	bool Seek(double TimeSeconds);

	// Returns the render target that UMG can display.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	UTextureRenderTarget2D* GetReplayRenderTarget() const { return ReplayRenderTarget; }

	// Returns the current playback state.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	EScenarioReplayPlaybackState GetPlaybackState() const { return PlaybackState; }

	// Returns the current replay time in seconds.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetCurrentReplayTimeSeconds() const { return CurrentReplayTimeSeconds; }

	// Returns the loaded replay duration in seconds.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetDurationSeconds() const { return Manifest.DurationSeconds; }

	// Returns the number of loaded replay frames.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	int32 GetFrameCount() const { return Frames.Num(); }

	// Returns the displayed frame index resolved from the current replay time.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	int32 GetCurrentFrameIndex() const { return CurrentFrameIndex; }

	// Returns the current robot speed in kilometers per hour.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetCurrentRobotSpeedKmh() const { return CurrentRobotSpeedKmh; }

	// Returns the current interpolated robot position in replay centimeters.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	FVector GetCurrentRobotPositionCm() const { return CurrentRobotPositionCm; }

	// Returns the playback speed multiplier.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetPlaybackSpeed() const { return PlaybackSpeed; }

	// Returns the current replay time normalized into the 0..1 timeline range.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	double GetPlaybackProgress() const;

	// Switches the replay SceneCapture between top-down, free, and vehicle-forward views.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void SetReplayCameraMode(EScenarioReplayCameraMode NewMode);

	// Returns the current replay SceneCapture camera mode.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	EScenarioReplayCameraMode GetReplayCameraMode() const { return CameraMode; }

	// Returns true when replay camera input may affect the active camera.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool IsReplayCameraInputAllowed() const;

	// Shows or hides replay scenario map actors in the replay capture.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void SetReplayMapVisible(bool bVisible);

	// Returns whether replay scenario map actors are visible in the replay capture.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool IsReplayMapVisible() const { return bReplayMapVisible; }

	// Shows or hides the replay point cloud actor in the replay capture.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void SetReplayPointCloudVisible(bool bVisible);

	// Returns whether the replay point cloud actor is visible in the replay capture.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool IsReplayPointCloudVisible() const { return bReplayPointCloudVisible; }

	// Returns whether the current replay has a loaded point cloud actor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool HasReplayPointCloud() const;

	// Moves the free replay camera in camera-local space.
	void AddFreeCameraMovement(
		const FVector& LocalInput,
		float DeltaSeconds);

	// Rotates the free replay camera from mouse-look input.
	void AddFreeCameraLook(const FVector2D& MouseDelta);

	// Adjusts the top-down orthographic zoom by changing the capture width.
	void AddTopDownZoom(float ZoomDirection);

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	// Destroys replay-only actors and releases transient render resources.
	void CleanupReplayWorld();

	// Applies project-level replay camera settings from Developer Settings.
	void ApplyCameraSettingsFromDefaults();

	// Applies one validated Developer Settings object to replay camera state.
	void ApplyCameraSettings(
		const UScenarioReplayDeveloperSettings& Settings);

	// Loads and materializes the episode scenario.json map when present.
	bool LoadEpisodeScenarioWorld(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Loads the optional episode point cloud layer when capture artifacts exist.
	bool LoadEpisodePointCloudWorld(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Spawns replay-only scenario actors from a compiled scenario world spec.
	bool SpawnReplayScenarioWorld(
		const FScenarioWorldSpec& WorldSpec,
		TArray<FString>& OutDiagnostics);

	// Spawns one replay-only static obstacle from the compiled scenario placeable.
	bool SpawnReplayStaticObstacle(
		const FScenarioPlaceableInstanceSpec& PlaceableSpec,
		TArray<FString>& OutDiagnostics);

	// Resolves one static obstacle prop entry from the default scenario catalog.
	bool TryFindStaticObstacleProp(
		FName PropId,
		FScenarioStaticObstaclePropEntry& OutPropEntry,
		TArray<FString>& OutDiagnostics) const;

	// Applies the replay world offset to a compiled scenario transform.
	FTransform MakeReplayWorldTransform(const FTransform& SourceTransform) const;

	// Adds one transient replay actor to capture visibility and cleanup ownership.
	void RegisterReplayScenarioActor(AActor* Actor);

	// Rebuilds one capture component's show-only list from current replay layer visibility.
	void PopulateReplayCaptureShowOnlyActors(
		USceneCaptureComponent2D& CaptureComponent) const;

	// Rebuilds the active replay capture show-only list from current replay layer visibility.
	void RefreshReplayCaptureShowOnlyActors();

	// Updates the point cloud renderer for the active replay camera mode.
	void RefreshReplayPointCloudRenderMode();

	// Applies the nearest loaded frame and captures the scene into the render target.
	bool ApplyFrameAtTime(double TimeSeconds);

	// Builds an interpolated replay frame for smooth visual playback.
	bool BuildInterpolatedFrameAtTime(
		double TimeSeconds,
		FEpisodeReplayRobotFrame& OutFrame,
		int32& OutFrameIndex) const;

	// Applies the selected camera mode to the replay SceneCapture actor.
	void UpdateReplayCaptureView(const FEpisodeReplayRobotFrame& Frame);

	// Applies robot-following orthographic capture settings.
	void UpdateTopDownReplayCamera(const FEpisodeReplayRobotFrame& Frame);

	// Applies user-controlled perspective capture settings.
	void UpdateFreeReplayCamera();

	// Applies a robot-mounted perspective camera transform.
	void UpdateVehicleFrontReplayCamera(const FEpisodeReplayRobotFrame& Frame);

	// Captures the configured replay scene into the render target.
	void CaptureReplayScene();

	// Creates the transient render target used by the replay panel.
	UTextureRenderTarget2D* CreateReplayRenderTarget();

	// Spawns and configures the top-down replay capture actor.
	ASceneCapture2D* SpawnReplayCaptureActor();

	// Returns the capture component for the current capture actor.
	USceneCaptureComponent2D* GetReplayCaptureComponent() const;

	// Episode directory currently loaded by the replay subsystem.
	UPROPERTY(Transient)
	FString LoadedEpisodeDirectory;

	// Parsed manifest for the loaded replay.
	UPROPERTY(Transient)
	FEpisodeReplayManifest Manifest;

	// Loaded V1 robot replay frames.
	UPROPERTY(Transient)
	TArray<FEpisodeReplayRobotFrame> Frames;

	// Visual-only robot actor driven by replay frames.
	UPROPERTY(Transient)
	TObjectPtr<ADeliveryBotReplayActor> ReplayRobotActor;

	// SceneCapture2D actor rendering replay-only actors.
	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> ReplayCaptureActor;

	// Render target shown by debug or formal replay UI.
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ReplayRenderTarget;

	// Replay-only static scenario actors generated from episode scenario.json.
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ReplayScenarioActors;

	// Replay-only point cloud actor generated from episode lidar_point_cloud artifacts.
	UPROPERTY(Transient)
	TObjectPtr<ADeliveryBotPointCloudReviewActor> ReplayPointCloudActor;

	// Current playback state.
	UPROPERTY(Transient)
	EScenarioReplayPlaybackState PlaybackState = EScenarioReplayPlaybackState::Stopped;

	// Camera mode used to place and configure the replay SceneCapture.
	UPROPERTY(Transient)
	EScenarioReplayCameraMode CameraMode = EScenarioReplayCameraMode::TopDown;

	// Whether replay scenario map actors are included in the replay capture.
	UPROPERTY(Transient)
	bool bReplayMapVisible = true;

	// Whether the replay point cloud actor is included in the replay capture.
	UPROPERTY(Transient)
	bool bReplayPointCloudVisible = true;

	// True when camera input is allowed while replay playback is paused.
	bool bAllowCameraInputWhilePaused = true;

	// Current replay time in seconds.
	double CurrentReplayTimeSeconds = 0.0;

	// Frame index currently represented in UI and diagnostics.
	int32 CurrentFrameIndex = INDEX_NONE;

	// Robot speed currently represented in UI and diagnostics.
	double CurrentRobotSpeedKmh = 0.0;

	// Robot position currently represented in UI and diagnostics.
	FVector CurrentRobotPositionCm = FVector::ZeroVector;

	// Playback speed multiplier.
	double PlaybackSpeed = 1.0;

	// Far-away offset that isolates replay actors from the active scenario world.
	FVector ReplayWorldOffset = FVector(500000.0, 0.0, 0.0);

	// Orthographic camera height above the replay robot.
	double CaptureHeightCm = 3000.0;

	// Orthographic width used by the V1 robot-only debug replay view.
	double CaptureOrthoWidthCm = 1800.0;

	// Minimum orthographic width allowed by replay zoom controls.
	double MinTopDownOrthoWidthCm = 400.0;

	// Maximum orthographic width allowed by replay zoom controls.
	double MaxTopDownOrthoWidthCm = 8000.0;

	// Orthographic width delta applied for one zoom input step.
	double TopDownZoomStepCm = 180.0;

	// User-controlled free camera location in replay world space.
	FVector FreeCameraLocation = FVector::ZeroVector;

	// User-controlled free camera rotation.
	FRotator FreeCameraRotation = FRotator(-35.0, 0.0, 0.0);

	// Movement speed for the replay free camera.
	double FreeCameraSpeedCmPerSecond = 1200.0;

	// Perspective field of view used by the replay free camera.
	double FreeCameraFovDegrees = 70.0;

	// Mouse-look sensitivity used by the replay free camera.
	double FreeCameraLookSensitivity = 0.12;

	// Minimum pitch allowed for replay free camera look.
	double MinFreeCameraPitchDegrees = -85.0;

	// Maximum pitch allowed for replay free camera look.
	double MaxFreeCameraPitchDegrees = 85.0;

	// Robot-local camera mount offset used by the vehicle-forward view.
	FVector VehicleFrontCameraLocalOffsetCm = FVector(140.0, 0.0, 90.0);

	// Robot-local camera mount rotation used by the vehicle-forward view.
	FRotator VehicleFrontCameraLocalRotation = FRotator(-5.0, 0.0, 0.0);

	// Perspective field of view used by the vehicle-forward camera.
	double VehicleFrontCameraFovDegrees = 85.0;
};
