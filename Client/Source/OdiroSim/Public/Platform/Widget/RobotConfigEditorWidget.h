#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "Types/SlateEnums.h"
#include "RobotConfigEditorWidget.generated.h"

class UBorder;
class UButton;
class UBaseSliderWidget;
class UCheckBox;
class UComboBoxString;
class UEditableText;
class URobotProfileViewModel;
class UTextBlock;
class UWidget;

// Native adapter for WBP_RobotConfigEditor profile controls.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API URobotConfigEditorWidget : public UOdiroCommonUserWidget
{
	GENERATED_BODY()

public:
	// Binds profile editor controls and loads the active project profile.
	virtual void NativeConstruct() override;

	// Releases profile editor button bindings.
	virtual void NativeDestruct() override;

private:
	// Reset button command.
	UFUNCTION()
	void HandleResetProfileClicked();

	// Save button command.
	UFUNCTION()
	void HandleSaveProfileClicked();

	// Profile numeric slider command.
	UFUNCTION()
	void HandleProfileSliderChanged(UWidget* widget, float value);

	// Profile text input edit command.
	UFUNCTION()
	void HandleProfileInputTextChanged(const FText& text);

	// LiDAR mode selection edit command.
	UFUNCTION()
	void HandleLidarModeSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	// LiDAR debug checkbox edit command.
	UFUNCTION()
	void HandleLidarDrawDebugChanged(bool bIsChecked);

	URobotProfileViewModel* ResolveViewModel();
	bool LoadProfileFromViewModel();
	bool ReadFieldsIntoViewModel();
	bool TryReadFloatField(UEditableText* input, const FString& label, float& outValue);
	void ApplyViewModelToFields();
	void ShowAllProfileSections() const;
	void MarkProfileDirty();
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
	// Registers one custom profile slider with the shared profile slider handler.
	static void BindProfileSlider(UBaseSliderWidget* slider, URobotConfigEditorWidget* owner);
	// Releases one custom profile slider from the shared profile slider handler.
	static void UnbindProfileSlider(UBaseSliderWidget* slider, URobotConfigEditorWidget* owner);
	// Registers one profile text input with the shared dirty-state handler.
	static void BindProfileInput(UEditableText* input, URobotConfigEditorWidget* owner);
	// Releases one profile text input from the shared dirty-state handler.
	static void UnbindProfileInput(UEditableText* input, URobotConfigEditorWidget* owner);
	static void SetInputText(UEditableText* input, float value);
	// Applies one numeric value to a paired text field and optional custom slider.
	static void SetLinkedSliderFieldValue(UEditableText* input, UBaseSliderWidget* slider, float value);
	// Selects an existing combo-box option using case-insensitive matching.
	static void SetComboBoxSelection(UComboBoxString* comboBox, const FString& selectedOption);
	static FString FormatProfileFloat(float value);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<URobotProfileViewModel> RobotProfileViewModel;

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

	// Restores fields to the last loaded ViewModel values.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetProfileButton;

	// Saves edited fields to profile.json.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveProfileButton;

	// Body section field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BodyFieldsBox;

	// Drive section field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DriveFieldsBox;

	// LiDAR section field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LiDARFieldsBox;

	// robot.body.length_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> BodyLengthInput;

	// robot.body.length_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> BodyLengthSlider;

	// robot.body.width_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> BodyWidthInput;

	// robot.body.width_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> BodyWidthSlider;

	// robot.body.height_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> BodyHeightInput;

	// robot.body.height_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> BodyHeightSlider;

	// robot.body.wheel_base_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> BodyWheelBaseInput;

	// robot.body.wheel_base_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> BodyWheelBaseSlider;

	// robot.body.turning_radius_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> BodyTurningRadiusInput;

	// robot.body.turning_radius_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> BodyTurningRadiusSlider;

	// robot.drive.max_speed_kmh input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveMaxSpeedInput;

	// robot.drive.max_speed_kmh slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> DriveMaxSpeedSlider;

	// robot.drive.max_reverse_kmh input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveReverseSpeedInput;

	// robot.drive.max_reverse_kmh slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> DriveReverseSpeedSlider;

	// robot.drive.accel_kmh_per_s input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveAccelerationInput;

	// robot.drive.accel_kmh_per_s slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> DriveAccelerationSlider;

	// robot.drive.decel_kmh_per_s input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveDecelerationInput;

	// robot.drive.decel_kmh_per_s slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> DriveDecelerationSlider;

	// robot.drive.steering_rate_per_s input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveSteeringGainInput;

	// robot.drive.steering_rate_per_s slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> DriveSteeringGainSlider;

	// robot.drive.mass_kg input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveMassInput;

	// robot.drive.mass_kg slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> DriveMassSlider;

	// robot.lidar.lidar_mode combo box.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> LidarModeComboBox;

	// robot.lidar.draw_debug check box.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> LidarDrawDebugCheckBox;

	// robot.lidar.scan_range_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarRangeInput;

	// robot.lidar.scan_range_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarRangeSlider;

	// robot.lidar.sensor_height_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarSensorHeightInput;

	// robot.lidar.sensor_height_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarSensorHeightSlider;

	// robot.lidar.front_half_angle_degree input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarFrontAngleInput;

	// robot.lidar.front_half_angle_degree slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarFrontAngleSlider;

	// robot.lidar.stop_distance_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarStopDistanceInput;

	// robot.lidar.stop_distance_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarStopDistanceSlider;

	// robot.lidar.slow_down_distance_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarSlowDownDistanceInput;

	// robot.lidar.slow_down_distance_m slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarSlowDownDistanceSlider;

	// robot.lidar.angle_step_degree input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarAngleStepInput;

	// robot.lidar.angle_step_degree slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarAngleStepSlider;

	// robot.lidar.scan_rate_hz input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarScanRateInput;

	// robot.lidar.scan_rate_hz slider.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSliderWidget> LidarScanRateSlider;

	// True while code is applying ViewModel values into widgets.
	bool bApplyingProfileFields = false;
};
