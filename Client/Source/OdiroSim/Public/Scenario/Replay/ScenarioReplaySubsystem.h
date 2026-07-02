#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeLidarRayReplayDataTypes.h"
#include "Shared/EpisodeReplayDataTypes.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioReplaySubsystem.generated.h"

class ADeliveryBotReplayActor;
class ADeliveryBotPointCloudReviewActor;
class ADeliveryBotLidarRayReviewActor;
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
	TopDown = 0,

	// User-controlled perspective camera moved by replay viewer input.
	Free = 1,

	// Perspective camera mounted at the replay robot's forward body offset.
	VehicleFront = 2,

	// Robot-centered perspective camera that orbits around the replay robot.
	Orbit = 3
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

	// Returns true when replay frames and capture actors are ready for camera operations.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool HasLoadedReplayFrames() const;

	// Places the free camera behind and above the current replay robot.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	bool FocusFreeCameraOnReplayRobot();

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

	// Shows or hides the replay LiDAR ray actor in the replay capture.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void SetReplayLidarRaysVisible(bool bVisible);

	// Returns whether replay LiDAR rays are visible in the replay capture.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool IsReplayLidarRaysVisible() const { return bReplayLidarRaysVisible; }

	// Returns whether the current replay has a loaded point cloud actor.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool HasReplayPointCloud() const;

	// Returns whether the current replay has loaded LiDAR ray frames.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	bool HasReplayLidarRays() const;

	// Returns the number of loaded LiDAR ray sensor frames.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	int32 GetLidarRayFrameCount() const;

	// Returns the LiDAR ray frame index resolved from the current replay time.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	int32 GetCurrentLidarRayFrameIndex() const;

	// Returns the number of rays in the current LiDAR ray frame.
	UFUNCTION(BlueprintPure, Category = "Scenario|Replay")
	int32 GetCurrentLidarRayCount() const;

	// Returns the current LiDAR ray frame for C++ replay render layers.
	const FEpisodeLidarRayFrame* GetCurrentLidarRayFrame() const;

	// Moves the free replay camera in camera-local space.
	void AddFreeCameraMovement(
		const FVector& LocalInput,
		float DeltaSeconds);

	// Rotates the free replay camera from mouse-look input.
	void AddFreeCameraLook(const FVector2D& MouseDelta);

	// Rotates the robot-centered replay orbit camera from mouse-look input.
	void AddOrbitCameraLook(const FVector2D& MouseDelta);

	// Adjusts the robot-centered replay orbit camera distance.
	void AddOrbitCameraZoom(float ZoomDirection);

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

	// Loads optional LiDAR ray frames for replay-only ray review.
	bool LoadEpisodeLidarRayReplay(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Loads the episode robot LiDAR config used for replay sensor range overlays.
	bool LoadEpisodeLidarSensorConfig(
		const FString& EpisodeDirectory,
		TArray<FString>& OutDiagnostics);

	// Spawns the replay-only LiDAR ray actor when ray frames are loaded.
	bool SpawnReplayLidarRayActor(TArray<FString>& OutDiagnostics);

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

	// Updates the current LiDAR ray frame index for the requested replay time.
	void ApplyLidarRayFrameAtTime(double TimeSeconds);

	// Pushes the currently selected LiDAR ray frame into the render actor.
	void RefreshReplayLidarRayActor();

	// Applies the selected camera mode to the replay SceneCapture actor.
	void UpdateReplayCaptureView(const FEpisodeReplayRobotFrame& Frame);

	// Applies robot-following orthographic capture settings.
	void UpdateTopDownReplayCamera(const FEpisodeReplayRobotFrame& Frame);

	// Applies user-controlled perspective capture settings.
	void UpdateFreeReplayCamera();

	// Applies robot-centered orbit capture settings.
	void UpdateOrbitReplayCamera(const FEpisodeReplayRobotFrame& Frame);

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

	// Replay-only actor that renders currently selected LiDAR ray frame lines.
	UPROPERTY(Transient)
	TObjectPtr<ADeliveryBotLidarRayReviewActor> ReplayLidarRayActor;

	// Parsed manifest for optional episode/replay/lidar_rays artifacts.
	UPROPERTY(Transient)
	FEpisodeLidarRayReplayManifest LidarRayManifest;

	// Loaded optional LiDAR ray frames keyed by replay time.
	UPROPERTY(Transient)
	TArray<FEpisodeLidarRayFrame> LidarRayFrames;

	// LiDAR config loaded from the episode run snapshot profile.
	UPROPERTY(Transient)
	FDeliveryBotLidarSensorConfigInfo ReplayLidarSensorConfig;

	// True when ReplayLidarSensorConfig was loaded from a profile artifact.
	UPROPERTY(Transient)
	bool bHasReplayLidarSensorConfig = false;

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

	// Whether the replay LiDAR ray actor is included in the replay capture.
	UPROPERTY(Transient)
	bool bReplayLidarRaysVisible = false;

	// True when camera input is allowed while replay playback is paused.
	bool bAllowCameraInputWhilePaused = true;

	// Current replay time in seconds.
	double CurrentReplayTimeSeconds = 0.0;

	// Frame index currently represented in UI and diagnostics.
	int32 CurrentFrameIndex = INDEX_NONE;

	// LiDAR ray frame index currently represented in UI and future ray rendering.
	int32 CurrentLidarRayFrameIndex = INDEX_NONE;

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

	// Robot-local backward distance used when refocusing the replay free camera.
	double FreeCameraFocusBackDistanceCm = 1200.0;

	// Robot-local side offset used when refocusing the replay free camera.
	double FreeCameraFocusSideOffsetCm = 350.0;

	// Vertical lift used when refocusing the replay free camera.
	double FreeCameraFocusHeightCm = 900.0;

	// Height above the replay robot that the free camera looks at when refocusing.
	double FreeCameraFocusTargetHeightCm = 120.0;

	// Perspective field of view used by the replay orbit camera.
	double OrbitCameraFovDegrees = 70.0;

	// Current orbit yaw around the replay robot.
	double OrbitCameraYawDegrees = 0.0;

	// Current orbit pitch around the replay robot.
	double OrbitCameraPitchDegrees = -35.0;

	// Current orbit distance from the replay robot target.
	double OrbitCameraDistanceCm = 1400.0;

	// Minimum orbit distance allowed by replay zoom controls.
	double MinOrbitCameraDistanceCm = 300.0;

	// Maximum orbit distance allowed by replay zoom controls.
	double MaxOrbitCameraDistanceCm = 6000.0;

	// Orbit distance delta applied for one zoom input step.
	double OrbitCameraZoomStepCm = 180.0;

	// Minimum pitch allowed for replay orbit camera look.
	double MinOrbitCameraPitchDegrees = -85.0;

	// Maximum pitch allowed for replay orbit camera look.
	double MaxOrbitCameraPitchDegrees = -5.0;

	// Mouse-look sensitivity used by the replay orbit camera.
	double OrbitCameraLookSensitivity = 0.12;

	// Height above the replay robot that the orbit camera looks at.
	double OrbitCameraTargetHeightCm = 120.0;

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
