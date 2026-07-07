#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "CommonUserWidget.h"
#include "UI/BaseFormElementTypes.h"
#include "RobotConfigEditorWidget.generated.h"

class UBorder;
class UBaseButtonWidget;
class UBaseCheckBoxWidget;
class UBaseDropdownWidget;
class UBaseSliderComboWidget;
class UImage;
class URobotPreviewSubsystem;
class URobotProfileViewModel;
class UTextBlock;
class UWidget;
struct FRobotProfileSettings;

// Native adapter for WBP_RobotConfigEditor profile controls.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URobotConfigEditorWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Binds profile editor controls and loads the active project profile.
	virtual void NativeConstruct() override;

	// Releases profile editor button bindings.
	virtual void NativeDestruct() override;

	// Starts the transient robot preview for the active Robot tab.
	void ActivateRobotPreview();

	// Stops the transient robot preview when leaving the Robot tab.
	void DeactivateRobotPreview();

	// Returns whether this editor currently owns an active preview.
	bool IsRobotPreviewActive() const { return bRobotPreviewActive; }

protected:
	// Starts preview camera orbit when right mouse is pressed over the preview image.
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Stops preview camera orbit when right mouse is released.
	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Keeps the viewport-backed preview centered inside the WBP preview frame.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Applies preview camera orbit while right mouse is held.
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	// Applies preview camera zoom from mouse wheel over the preview image.
	virtual FReply NativeOnMouseWheel(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

private:
	// Reset button command.
	UFUNCTION()
	void HandleResetProfileClicked(UBaseButtonWidget* button);

	// Save button command.
	UFUNCTION()
	void HandleSaveProfileClicked(UBaseButtonWidget* button);

	// Profile numeric slider-combo command.
	UFUNCTION()
	void HandleProfileSliderComboChanged(UWidget* widget, float value);

	// LiDAR mode selection edit command.
	UFUNCTION()
	void HandleLidarModeSelectionChanged(UWidget* widget, FName selectedId);

	// LiDAR debug checkbox edit command.
	UFUNCTION()
	void HandleLidarDrawDebugChanged(UWidget* widget, ECheckBoxState checkState);

	// LiDAR preview layer checkbox edit command.
	UFUNCTION()
	void HandleLidarPreviewOptionChanged(UWidget* widget, ECheckBoxState checkState);

	// LiDAR preview density selection edit command.
	UFUNCTION()
	void HandleLidarPreviewDensitySelectionChanged(UWidget* widget, FName selectedId);

	// Preview left-rotation command.
	UFUNCTION()
	void HandleRotatePreviewLeftClicked(UBaseButtonWidget* button);

	// Preview front-view reset command.
	UFUNCTION()
	void HandleResetPreviewRotationClicked(UBaseButtonWidget* button);

	// Preview right-rotation command.
	UFUNCTION()
	void HandleRotatePreviewRightClicked(UBaseButtonWidget* button);

	// Draws LiDAR rays in the preview from the current editable values.
	UFUNCTION()
	void HandleDrawLidarPreviewRaysClicked(UBaseButtonWidget* button);

	// Clears LiDAR rays from the preview.
	UFUNCTION()
	void HandleClearLidarPreviewRaysClicked(UBaseButtonWidget* button);

	URobotProfileViewModel* ResolveViewModel();
	// Resolves the world-scoped robot preview subsystem for this widget.
	URobotPreviewSubsystem* ResolveRobotPreviewSubsystem() const;
	bool LoadProfileFromViewModel();
	bool ReadFieldsIntoViewModel();
	bool TryReadFloatField(UBaseSliderComboWidget* sliderCombo, const FString& label, float& outValue);
	// Reads an optional numeric field when the matching WBP widget exists.
	bool TryReadOptionalFloatField(UBaseSliderComboWidget* sliderCombo, const FString& label, float& outValue);
	// Reads the current UI values into a preview-only settings snapshot.
	bool TryReadFieldsIntoPreviewSettings(FRobotProfileSettings& outSettings) const;
	// Reads one numeric field for preview updates without changing save validation state.
	static bool TryReadPreviewFloatField(UBaseSliderComboWidget* sliderCombo, float& outValue);
	// Reads an optional numeric field for preview updates when the matching WBP widget exists.
	static bool TryReadOptionalPreviewFloatField(UBaseSliderComboWidget* sliderCombo, float& outValue);
	void ApplyViewModelToFields();
	void ShowAllProfileSections() const;
	void MarkProfileDirty();
	// Starts the transient robot preview owned by this widget.
	void StartRobotPreview();
	// Stops the transient robot preview owned by this widget.
	void StopRobotPreview();
	// Pushes current UI input values into the active robot preview.
	void RefreshRobotPreviewFromFields();
	// Pushes current LiDAR preview display options into the active robot preview.
	void ApplyRobotPreviewDisplayOptions();
	// Shows or clears the preview LiDAR ray snapshot from the current editable values.
	void SetLidarPreviewRaysVisible(bool bShouldShow);
	// Syncs Preview-only option controls to the current ray visibility state.
	void SyncLidarPreviewControlState();
	// Applies the subsystem render target to the preview image brush.
	void ApplyRobotPreviewRenderTarget();
	// Pushes the WBP-authored preview input frame to the viewport-backed preview camera.
	void SyncRobotPreviewViewportFrame(bool bForce = false);
	// Updates the preview overlay status text.
	void SetRobotPreviewStatus(const FString& statusText) const;
	// Returns true when a screen-space pointer location is inside the preview input area.
	bool IsPointerOverRobotPreviewInputArea(const FVector2D& ScreenSpacePosition) const;
	// Clears right-mouse preview orbit state.
	void ClearRobotPreviewOrbitInput();
	void SetProfileStateSaved(const FString& detailText) const;
	void SetProfileStateDirty() const;
	void SetProfileStateError(const FString& detailText) const;
	void SetProfileStateTexts(
		const FString& stateText,
		const FString& detailText,
		const FString& actionTimeText,
		const FLinearColor& badgeColor,
		const FLinearColor& textColor,
		const FLinearColor& actionDotColor) const;
	void SetProfileStatus(const FString& statusText) const;
	void SetProfilePathText(const FString& profilePath) const;
	// Registers one profile slider combo with the shared dirty-state handler.
	static void BindProfileSliderCombo(UBaseSliderComboWidget* sliderCombo, URobotConfigEditorWidget* owner);
	// Releases one profile slider combo from the shared dirty-state handler.
	static void UnbindProfileSliderCombo(UBaseSliderComboWidget* sliderCombo, URobotConfigEditorWidget* owner);
	// Applies one numeric value to a slider combo field.
	static void SetSliderComboFieldValue(UBaseSliderComboWidget* sliderCombo, float value);
	// Applies authored dropdown options to base dropdowns.
	void InitializeDropdowns() const;
	// Selects an existing dropdown item using case-insensitive id matching.
	static void SetDropdownSelection(UBaseDropdownWidget* dropdown, const FString& selectedOption);
	// Returns the selected dropdown id as a profile string.
	static FString GetDropdownSelection(const UBaseDropdownWidget* dropdown);
	// Returns whether a base checkbox is checked.
	static bool IsBaseCheckBoxChecked(const UBaseCheckBoxWidget* checkBox);
	// Applies a boolean value to a base checkbox.
	static void SetBaseCheckBoxChecked(UBaseCheckBoxWidget* checkBox, bool bChecked);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<URobotProfileViewModel> RobotProfileViewModel;

	// True while the Robot tab owns the PlayerViewport-backed preview lifecycle.
	bool bRobotPreviewActive = false;

	// Last preview frame center sent to the viewport-backed preview camera.
	FVector2D LastRobotPreviewFrameCenterPixel = FVector2D::ZeroVector;

	// Last full viewport size sent to the viewport-backed preview camera.
	FVector2D LastRobotPreviewViewportSizePixel = FVector2D::ZeroVector;

	// Current profile path display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfilePathText;

	// Profile saved/dirty/error badge display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfileStateText;

	// Profile saved/dirty/error detail display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfileSavedAtText;

	// Profile saved/dirty/error badge background.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ProfileStateBadgeBorder;

	// Profile action bar status dot.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ProfileActionStatusDot;

	// Profile action bar secondary time/status display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfileActionTimeText;

	// Validation and save/load status display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfileStatusText;

	// Render target image for the transient robot preview scene.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RobotPreviewImage;

	// Transparent input hit area covering the PlayerViewport-backed preview.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RobotPreviewViewportInputArea;

	// Overlay status text for robot preview lifecycle and input state.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RobotPreviewStatusText;

	// Rotates the preview robot left without changing the camera.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> RotateLeftButton;

	// Restores the preview robot to the front-facing yaw.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> ResetPreviewRotationButton;

	// Rotates the preview robot right without changing the camera.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> RotateRightButton;

	// Optional push button fallback that toggles LiDAR rays in alternate WBP layouts.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> DrawLidarRaysButton;

	// Base button that owns the persistent selected/unselected ray visual state.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> ToggleLidarRaysButton;

	// Optional clear button fallback kept for alternate WBP layouts.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> ClearLidarRaysButton;

	// Preview overlay panel containing ray/range/points/density display options.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LidarPreviewOptionsPanel;

	// Preview overlay row containing ray/range/points/density display options.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LidarPreviewOptionsRow;

	// Toggles sampled LiDAR ray beams in the preview.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseCheckBoxWidget> ShowLidarRaysCheckBox;

	// Toggles LiDAR range rings and front boundary lines in the preview.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseCheckBoxWidget> ShowLidarRangeCheckBox;

	// Toggles sampled LiDAR end point markers in the preview.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseCheckBoxWidget> ShowLidarPointsCheckBox;

	// Selects how many logical LiDAR rays are sampled into the preview.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseDropdownWidget> LidarPreviewDensityComboBox;

	// Restores fields to the last loaded ViewModel values.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> ResetProfileButton;

	// Saves edited fields to profile.json.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButtonWidget> SaveProfileButton;

	// Body section field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BodyFieldsBox;

	// Drive section field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DriveFieldsBox;

	// LiDAR section field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LiDARFieldsBox;

	// robot.body.length_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> BodyLengthSliderCombo;

	// robot.body.width_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> BodyWidthSliderCombo;

	// robot.body.height_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> BodyHeightSliderCombo;

	// robot.body.wheel_base_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> BodyWheelBaseSliderCombo;

	// robot.body.turning_radius_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> BodyTurningRadiusSliderCombo;

	// robot.drive.max_speed_kmh slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> DriveMaxSpeedSliderCombo;

	// robot.drive.max_reverse_kmh slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> DriveReverseSpeedSliderCombo;

	// robot.drive.accel_kmh_per_s slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> DriveAccelerationSliderCombo;

	// robot.drive.decel_kmh_per_s slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> DriveDecelerationSliderCombo;

	// robot.drive.steering_rate_per_s slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> DriveSteeringGainSliderCombo;

	// robot.drive.mass_kg slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> DriveMassSliderCombo;

	// robot.lidar.lidar_mode dropdown.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseDropdownWidget> LidarModeComboBox;

	// robot.lidar.draw_debug check box.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseCheckBoxWidget> LidarDrawDebugCheckBox;

	// robot.lidar.scan_range_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarRangeSliderCombo;

	// robot.lidar.sensor_height_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarSensorHeightSliderCombo;

	// robot.lidar.sensor_forward_offset_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarSensorForwardOffsetSliderCombo;

	// robot.lidar.sensor_right_offset_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarSensorRightOffsetSliderCombo;

	// robot.lidar.front_half_angle_degree slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarFrontAngleSliderCombo;

	// robot.lidar.stop_distance_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarStopDistanceSliderCombo;

	// robot.lidar.obstacle_warning_distance_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarObstacleWarningDistanceSliderCombo;

	// robot.lidar.slow_down_distance_m slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarSlowDownDistanceSliderCombo;

	// robot.lidar.angle_step_degree slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarAngleStepSliderCombo;

	// robot.lidar.vertical_min_degree slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarVerticalMinSliderCombo;

	// robot.lidar.vertical_max_degree slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarVerticalMaxSliderCombo;

	// robot.lidar.vertical_step_degree slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarVerticalStepSliderCombo;

	// robot.lidar.scan_rate_hz slider combo.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderComboWidget> LidarScanRateSliderCombo;

	// True while code is applying ViewModel values into widgets.
	bool bApplyingProfileFields = false;

	// True while right mouse is held over the robot preview input area for orbit control.
	bool bRobotPreviewOrbitHeld = false;
};
