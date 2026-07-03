#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "ProjectEpisodeReplayViewerWidget.generated.h"

class UButton;
class UBorder;
class UCanvasPanel;
class UImage;
class UProjectEpisodeReplayInterestRegionStripWidget;
class USlider;
class UTextBlock;
class UTexture2D;
class UProjectEpisodeReplayViewerWidget;
class UScenarioReplaySubsystem;
enum class EScenarioReplayCameraMode : uint8;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FProjectEpisodeReplayFullscreenChangedNative,
	UProjectEpisodeReplayViewerWidget*,
	bool);

// Embedded project-run episode replay viewer used by WBP_Replay.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectEpisodeReplayViewerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Enables keyboard focus for replay camera input before the underlying Slate widget is built.
	UProjectEpisodeReplayViewerWidget(const FObjectInitializer& ObjectInitializer);

	// Loads one episode replay into the already-created embedded viewer and starts playback.
	UFUNCTION(BlueprintCallable, Category = "Project|Replay")
	bool OpenEpisodeReplay(const FString& EpisodeDirectory);

	// Stops playback, unloads transient replay actors, and clears the viewer state.
	UFUNCTION(BlueprintCallable, Category = "Project|Replay")
	void ResetReplay();

	// Rebinds owned WBP control delegates after this viewer is moved between widget hosts.
	void RefreshReplayControlBindings();

	// Returns the latest viewer diagnostic text for parent UI status mirroring.
	const FString& GetLastDiagnosticsText() const { return LastDiagnosticsText; }

	// Notifies the owning UI when this replay viewer should move between normal and fullscreen hosts.
	FProjectEpisodeReplayFullscreenChangedNative OnReplayFullscreenChanged;

protected:
	// Binds optional WBP controls owned by WBP_Replay.
	virtual void NativeConstruct() override;

	// Unbinds optional WBP controls owned by WBP_Replay.
	virtual void NativeDestruct() override;

	// Drives replay camera movement from held keys while the viewer is active.
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

	// Gives the replay viewer keyboard focus when the panel is clicked.
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Ends free-camera mouse look when the right mouse button is released.
	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Rotates the free camera while right mouse look is active.
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Records replay camera movement keys and top-down zoom shortcuts.
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

	// Clears replay camera movement keys when released.
	virtual FReply NativeOnKeyUp(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

	// Applies top-down zoom from the mouse wheel when replay input is active.
	virtual FReply NativeOnMouseWheel(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Clears movement key state when focus leaves the replay viewer.
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

private:
	// Starts playback from the current replay frame.
	UFUNCTION()
	void HandlePlayClicked();

	// Pauses playback at the current replay frame.
	UFUNCTION()
	void HandlePauseClicked();

	// Toggles the active replay subsystem between playing and paused.
	UFUNCTION()
	void HandlePlayPauseClicked();

	// Pauses playback at the current frame so Play can resume from there.
	UFUNCTION()
	void HandleStopClicked();

	// Seeks the active replay back to the first frame while keeping it loaded.
	UFUNCTION()
	void HandleResetClicked();

	// Advances the replay camera to the next available mode.
	UFUNCTION()
	void HandleCameraModeClicked();

	// Shows the replay fullscreen overlay.
	UFUNCTION()
	void HandleFullscreenClicked();

	// Hides the replay fullscreen overlay.
	UFUNCTION()
	void HandleExitFullscreenClicked();

	// Seeks the loaded replay from the normalized timeline slider value.
	UFUNCTION()
	void HandleReplayTimelineValueChanged(float Value);

	// Stops replay on a snapped event marker after timeline mouse dragging ends.
	UFUNCTION()
	void HandleReplayTimelineMouseCaptureEnd();

	// Switches the replay camera to robot-following top-down mode.
	UFUNCTION()
	void HandleTopDownCameraClicked();

	// Switches the replay camera to robot-centered orbit mode.
	UFUNCTION()
	void HandleOrbitCameraClicked();

	// Switches the replay camera to user-controlled free mode.
	UFUNCTION()
	void HandleFreeCameraClicked();

	// Switches the replay camera to the robot-mounted forward mode.
	UFUNCTION()
	void HandleVehicleFrontCameraClicked();

	// Toggles replay scenario map visibility in fullscreen mode.
	UFUNCTION()
	void HandleFullscreenMapToggleClicked();

	// Toggles replay point cloud visibility in fullscreen mode.
	UFUNCTION()
	void HandleFullscreenPointCloudToggleClicked();

	// Toggles replay LiDAR ray visibility in fullscreen mode.
	UFUNCTION()
	void HandleFullscreenRayToggleClicked();

	// Seeks replay playback to the selected interest event.
	void HandleReplayInterestEventSelected(
		UProjectEpisodeReplayInterestRegionStripWidget* InterestStrip,
		double TimeSeconds);

	// Returns the world replay subsystem for this viewer.
	UScenarioReplaySubsystem* GetReplaySubsystem() const;

	// Applies the replay render target to normal and fullscreen images.
	void ApplyReplayRenderTarget();

	// Applies fullscreen overlay visibility and input state.
	void SetReplayFullscreen(bool bNewFullscreen);

	// Updates the fullscreen overlay visibility from bReplayFullscreen.
	void UpdateReplayFullscreenVisibility();

	// Forces fullscreen-only child widgets to fill their authored parent slots.
	void ApplyReplayFullscreenLayout();

	// Finds fullscreen layer toggle buttons by name when they are not exposed as WBP variables.
	void ResolveFullscreenLayerToggleWidgets();

	// Finds compact replay controls by name when they are not exposed as WBP variables.
	void ResolveCompactReplayWidgets();

	// Finds the optional replay interest-region strip by supported WBP names.
	void ResolveReplayInterestRegionWidgets();

	// Binds the optional interest-region strip selection event.
	void BindReplayInterestRegionStrip();

	// Unbinds the optional interest-region strip selection event.
	void UnbindReplayInterestRegionStrip();

	// Binds one optional replay timeline slider to seek and snap handling.
	void BindReplayTimelineSlider(USlider* TimelineSlider);

	// Unbinds one optional replay timeline slider from seek and snap handling.
	void UnbindReplayTimelineSlider(USlider* TimelineSlider);

	// Applies one replay camera mode and refreshes camera UI text.
	void ApplyReplayCameraMode(EScenarioReplayCameraMode NewMode);

	// Returns the next replay camera mode in the UI cycle.
	EScenarioReplayCameraMode GetNextReplayCameraMode(
		EScenarioReplayCameraMode CurrentMode) const;

	// Returns the display label for a replay camera mode.
	FText GetReplayCameraModeLabel(
		EScenarioReplayCameraMode CameraMode) const;

	// Returns the compact icon texture for one replay camera mode.
	UTexture2D* GetReplayCameraModeIcon(
		EScenarioReplayCameraMode CameraMode) const;

	// Updates the compact camera mode icon from the active replay subsystem.
	void UpdateReplayCameraModeIcon(
		EScenarioReplayCameraMode CameraMode);

	// Updates the optional camera mode label and icon from the active replay subsystem.
	void UpdateCameraModeText();

	// Updates optional timeline, frame, and speed display widgets.
	void UpdateReplayTimelineUi();

	// Updates every authored timeline slider from the normalized replay progress.
	void SetReplayTimelineSliderValues(float NormalizedValue);

	// Updates the compact and fullscreen play/pause button icons from the actual replay state.
	void UpdateReplayPlaybackIcon(
		bool bReplayPlaying,
		bool bHasReplayFrames);

	// Rebuilds optional event dots over the replay timeline.
	void RebuildReplayEventMarkers();

	// Adds replay event dots to one authored marker canvas.
	void AddReplayEventMarkersToCanvas(
		UCanvasPanel* MarkerCanvas,
		const UScenarioReplaySubsystem& ReplaySubsystem);

	// Clears optional event dots and timeline snap state.
	void ClearReplayEventMarkers();

	// Rebuilds optional replay interest cards from loaded event markers.
	void RebuildReplayInterestRegions();

	// Clears optional replay interest cards.
	void ClearReplayInterestRegions();

	// Updates optional replay interest card selection from current replay time.
	void UpdateReplayInterestRegionSelection(
		bool bScrollSelectedIntoView = false);

	// Scrolls optional replay interest cards to one event index.
	void FocusReplayInterestEvent(int32 EventIndex);

	// Returns true when the requested time should snap to a nearby event marker.
	bool TryFindTimelineSnapEvent(
		double TimeSeconds,
		double& OutEventTimeSeconds,
		int32& OutEventIndex) const;

	// Formats a replay time in seconds for compact UI display.
	FText FormatReplayTime(double TimeSeconds) const;

	// Requests keyboard focus so replay camera keys route to this widget.
	void RequestReplayInputFocus();

	// Returns true when replay camera shortcuts should affect the active replay.
	bool CanUseReplayCameraInput() const;

	// Returns true when mouse look should affect the active replay camera.
	bool CanUseReplayCameraLook() const;

	// Builds a camera-local movement vector from held replay movement keys.
	FVector BuildFreeCameraInput() const;

	// Clears all held movement keys tracked by the replay viewer.
	void ClearReplayMovementInput();

	// Clears right-mouse look state tracked by the replay viewer.
	void ClearReplayLookInput();

	// Stores the latest replay diagnostics message for parent/UI consumers.
	void SetDiagnosticsText(const FString& Message);

	// Image that displays the replay render target.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ReplayImage;

	// Button that toggles replay playback between playing and paused.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayPauseButton;

	// Icon image inside PlayPauseButton that mirrors playing or paused state.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PlayPauseImage;

	// Embedded replay control bar shown only while the viewer is not fullscreen.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ReplayControlBar;

	// Button that pauses playback at the current frame.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StopButton;

	// Button that resets playback to the first frame.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	// Button that cycles through top-down, free, and vehicle-forward cameras.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CameraModeButton;

	// Compact camera mode icon shown inside CameraModeButton.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CameraModeImage;

	// Button that shows the fullscreen replay overlay.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenButton;

	// Timeline slider that seeks by normalized replay time.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USlider> ReplayTimelineSlider;

	// Optional canvas layered above ReplayTimelineSlider for colored event dots.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> ReplayTimelineMarkerCanvas;

	// Compact timeline slider shown in the embedded replay control bar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USlider> ReplayCompactTimelineSlider;

	// Compact marker canvas layered above ReplayCompactTimelineSlider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> ReplayCompactTimelineMarkerCanvas;

	// Optional WBP strip that displays replay event interest cards.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProjectEpisodeReplayInterestRegionStripWidget> ReplayInterestRegionStrip;

	// Fullscreen overlay root that covers the normal replay layout.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ReplayFullscreenLayer;

	// Image that displays the replay render target in fullscreen mode.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ReplayFullscreenImage;

	// Fullscreen button that toggles replay playback between playing and paused.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenPlayPauseButton;

	// Icon image inside FullscreenPlayPauseButton that mirrors playing or paused state.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> FullscreenPlayPauseImage;

	// Fullscreen button that pauses playback at the current frame.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenStopButton;

	// Fullscreen button that resets playback to the first frame.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenResetButton;

	// Fullscreen button that selects the robot-following top-down replay camera.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenTopDownCameraButton;

	// Fullscreen button that selects the robot-centered orbit replay camera.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenOrbitCameraButton;

	// Fullscreen button that selects the user-controlled free replay camera.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenFreeCameraButton;

	// Fullscreen button that selects the robot-mounted forward replay camera.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenVehicleFrontCameraButton;

	// Fullscreen button that toggles replay map visibility.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenMapToggleButton;

	// Fullscreen button that toggles replay point cloud visibility.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenPointCloudToggleButton;

	// Fullscreen button that toggles replay LiDAR ray visibility.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FullscreenRayToggleButton;

	// Button that hides the fullscreen replay overlay.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExitFullscreenButton;

	// Optional label shown inside or near the camera cycle button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CameraModeText;

	// Optional fullscreen camera mode label.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FullscreenCameraModeText;

	// Optional current time and duration display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayTimeText;

	// Optional current frame and frame count display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayFrameText;

	// Optional robot speed and playback-rate display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplaySpeedText;

	// Optional current robot position display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayPositionText;

	// Button that explicitly starts replay playback in the redesigned control bar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayButton;

	// Button that explicitly pauses replay playback in the redesigned control bar.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PauseButton;

	// Optional numeric robot speed value for card-style replay telemetry.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplaySpeedValueText;

	// Optional playback rate value for card-style replay telemetry.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayPlaybackRateText;

	// Optional robot X position value for card-style replay telemetry.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayPositionXText;

	// Optional robot Y position value for card-style replay telemetry.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayPositionYText;

	// Optional robot Z position value for card-style replay telemetry.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayPositionZText;

	// Episode directory currently loaded by this viewer.
	UPROPERTY(Transient)
	FString LoadedEpisodeDirectory;

	// Latest diagnostic text emitted by this viewer.
	UPROPERTY(Transient)
	FString LastDiagnosticsText;

	// True while the replay free-camera forward key is held.
	bool bMoveForwardHeld = false;

	// True while the replay free-camera backward key is held.
	bool bMoveBackwardHeld = false;

	// True while the replay free-camera left key is held.
	bool bMoveLeftHeld = false;

	// True while the replay free-camera right key is held.
	bool bMoveRightHeld = false;

	// True while the replay free-camera up key is held.
	bool bMoveUpHeld = false;

	// True while the replay free-camera down key is held.
	bool bMoveDownHeld = false;

	// True while right mouse is held for free-camera look.
	bool bFreeCameraLookHeld = false;

	// True while native code is refreshing the slider value.
	bool bUpdatingReplayTimelineSlider = false;

	// True when the current timeline drag is snapped to an event marker.
	bool bTimelineSnappedToEvent = false;

	// Event time used when the current timeline drag is snapped.
	double SnappedEventTimeSeconds = 0.0;

	// Event index used when the current timeline drag is snapped.
	int32 SnappedEventIndex = INDEX_NONE;

	// Maximum timeline distance in seconds that snaps dragging to an event marker.
	double TimelineEventSnapThresholdSeconds = 0.3;

	// True while the fullscreen replay overlay should be visible.
	bool bReplayFullscreen = false;

	// Cached play icon used by the fullscreen play/pause button.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplayPlayIconTexture;

	// Cached pause icon used by the fullscreen play/pause button.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplayPauseIconTexture;

	// Cached top-down camera icon used by the compact camera cycle button.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplayTopDownCameraIconTexture;

	// Cached third-person camera icon used by orbit and free camera modes.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplayThirdPersonCameraIconTexture;

	// Cached free camera icon used by the compact camera cycle button.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplayFreeCameraIconTexture;

	// Cached first-person camera icon used by the vehicle-front camera mode.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplayFirstPersonCameraIconTexture;
};
