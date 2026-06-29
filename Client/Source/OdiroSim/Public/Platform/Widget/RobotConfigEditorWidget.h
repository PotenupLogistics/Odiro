#pragma once

#include "CoreMinimal.h"
#include "Platform/Widget/OdiroCommonUserWidget.h"
#include "RobotConfigEditorWidget.generated.h"

class UButton;
class UBaseSliderWidget;
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
	// Editable profile sections shown by the right-side panel.
	enum class ERobotProfileSection
	{
		Body,
		Drive,
		Lidar
	};

	// Reload button command.
	UFUNCTION()
	void HandleReloadProfileClicked();

	// Reset button command.
	UFUNCTION()
	void HandleResetProfileClicked();

	// Save button command.
	UFUNCTION()
	void HandleSaveProfileClicked();

	// Body tab command for the first editable section.
	UFUNCTION()
	void HandleBodyTabClicked();

	// Drive tab command for the exposed drive section.
	UFUNCTION()
	void HandleDriveTabClicked();

	// LiDAR tab command for the exposed sensor section.
	UFUNCTION()
	void HandleLidarTabClicked();

	// Body section slider command.
	UFUNCTION()
	void HandleBodySliderChanged(UWidget* widget, float value);

	URobotProfileViewModel* ResolveViewModel();
	bool LoadProfileFromViewModel();
	bool ReadFieldsIntoViewModel();
	bool TryReadFloatField(UEditableText* input, const FString& label, float& outValue);
	void ApplyViewModelToFields();
	void ShowProfileSection(ERobotProfileSection section) const;
	void SetProfileStatus(const FString& statusText) const;
	void SetProfilePathText(const FString& profilePath) const;
	// Registers one custom body slider with the shared body slider handler.
	static void BindBodySlider(UBaseSliderWidget* slider, URobotConfigEditorWidget* owner);
	// Releases one custom body slider from the shared body slider handler.
	static void UnbindBodySlider(UBaseSliderWidget* slider, URobotConfigEditorWidget* owner);
	static void SetInputText(UEditableText* input, float value);
	// Applies one numeric value to a paired text field and optional custom slider.
	static void SetLinkedSliderFieldValue(UEditableText* input, UBaseSliderWidget* slider, float value);
	static FString FormatProfileFloat(float value);

	// ViewModel supplied by PlatformUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<URobotProfileViewModel> RobotProfileViewModel;

	// Current profile path display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfilePathText;

	// Validation and save/load status display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProfileStatusText;

	// Body section tab button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BodyTabButton;

	// Drive section tab button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> DriveTabButton;

	// LiDAR section tab button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LidarTabButton;

	// Reloads profile.json from disk.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReloadProfileButton;

	// Restores fields to the last loaded ViewModel values.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetProfileButton;

	// Saves edited fields to profile.json.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveProfileButton;

	// Body tab field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BodyFieldsBox;

	// Drive tab field container.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DriveFieldsBox;

	// LiDAR tab field container.
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

	// robot.drive.steering_rate_per_s input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveSteeringGainInput;

	// robot.drive.mass_kg input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> DriveMassInput;

	// robot.lidar.scan_range_m input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarRangeInput;

	// robot.lidar.front_half_angle_degree input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarFrontAngleInput;

	// robot.lidar.angle_step_degree input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarAngleStepInput;

	// robot.lidar.scan_rate_hz input.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> LidarScanRateInput;
};
