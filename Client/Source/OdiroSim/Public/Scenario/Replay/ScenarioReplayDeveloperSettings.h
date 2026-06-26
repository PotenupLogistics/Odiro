#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ScenarioReplayDeveloperSettings.generated.h"

// Project-level tuning values for embedded episode replay cameras.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Scenario Replay"))
class ODIROSIM_API UScenarioReplayDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Returns the Project Settings category for OdiroSim settings.
	virtual FName GetCategoryName() const override;

	// True when replay camera controls are allowed while playback is paused.
	UPROPERTY(EditAnywhere, Config, Category = "Input")
	bool bAllowCameraInputWhilePaused = true;

	// Orthographic camera height above the replay robot.
	UPROPERTY(EditAnywhere, Config, Category = "Top Down", meta = (ClampMin = "1.0", UIMin = "100.0"))
	double TopDownCaptureHeightCm = 3000.0;

	// Initial orthographic width used by the top-down replay view.
	UPROPERTY(EditAnywhere, Config, Category = "Top Down", meta = (ClampMin = "1.0", UIMin = "100.0"))
	double TopDownOrthoWidthCm = 1800.0;

	// Minimum orthographic width allowed by replay zoom controls.
	UPROPERTY(EditAnywhere, Config, Category = "Top Down", meta = (ClampMin = "1.0", UIMin = "100.0"))
	double MinTopDownOrthoWidthCm = 400.0;

	// Maximum orthographic width allowed by replay zoom controls.
	UPROPERTY(EditAnywhere, Config, Category = "Top Down", meta = (ClampMin = "1.0", UIMin = "100.0"))
	double MaxTopDownOrthoWidthCm = 8000.0;

	// Orthographic width delta applied for one zoom input step.
	UPROPERTY(EditAnywhere, Config, Category = "Top Down", meta = (ClampMin = "1.0", UIMin = "1.0"))
	double TopDownZoomStepCm = 180.0;

	// Movement speed for the replay free camera.
	UPROPERTY(EditAnywhere, Config, Category = "Free Camera", meta = (ClampMin = "1.0", UIMin = "100.0"))
	double FreeCameraSpeedCmPerSecond = 1200.0;

	// Perspective field of view used by the replay free camera.
	UPROPERTY(EditAnywhere, Config, Category = "Free Camera", meta = (ClampMin = "5.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "120.0"))
	double FreeCameraFovDegrees = 70.0;

	// Mouse-look sensitivity used by the replay free camera.
	UPROPERTY(EditAnywhere, Config, Category = "Free Camera", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "1.0"))
	double FreeCameraLookSensitivity = 0.12;

	// Minimum pitch allowed for replay free camera look.
	UPROPERTY(EditAnywhere, Config, Category = "Free Camera", meta = (ClampMin = "-89.0", ClampMax = "89.0", UIMin = "-89.0", UIMax = "0.0"))
	double MinFreeCameraPitchDegrees = -85.0;

	// Maximum pitch allowed for replay free camera look.
	UPROPERTY(EditAnywhere, Config, Category = "Free Camera", meta = (ClampMin = "-89.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
	double MaxFreeCameraPitchDegrees = 85.0;

	// Robot-local camera mount offset used by the vehicle-forward view.
	UPROPERTY(EditAnywhere, Config, Category = "Vehicle Front")
	FVector VehicleFrontCameraLocalOffsetCm = FVector(140.0, 0.0, 90.0);

	// Robot-local camera mount rotation used by the vehicle-forward view.
	UPROPERTY(EditAnywhere, Config, Category = "Vehicle Front")
	FRotator VehicleFrontCameraLocalRotation = FRotator(-5.0, 0.0, 0.0);

	// Perspective field of view used by the vehicle-forward camera.
	UPROPERTY(EditAnywhere, Config, Category = "Vehicle Front", meta = (ClampMin = "5.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "120.0"))
	double VehicleFrontCameraFovDegrees = 85.0;
};
