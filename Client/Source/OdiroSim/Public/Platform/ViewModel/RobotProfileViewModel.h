#pragma once

#include "CoreMinimal.h"
#include "Platform/RobotProfileSettings.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "RobotProfileViewModel.generated.h"

class UGameInstance;
class UProjectSessionSubsystem;

// ViewModel for editing the active user-project profile.json robot fields.
UCLASS(BlueprintType)
class ODIROSIM_API URobotProfileViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// Connects this ViewModel to the GameInstance-owned project session.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void InitializeForGameInstance(UGameInstance* gameInstance);

	// Injects a project session for tests or explicit hosts.
	void SetSubsystemOverride(UProjectSessionSubsystem* projectSessionSubsystem);

	// Loads <ActiveProject>/profile.json into the editable state.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	bool LoadFromActiveProject();

	// Loads <projectPath>/profile.json into the editable state.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	bool LoadFromProject(const FString& projectPath);

	// Saves the editable state to <ActiveProject>/profile.json.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	bool SaveRobotProfile();

	// Saves the editable state to <projectPath>/profile.json.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	bool SaveToProject(const FString& projectPath);

	// Updates the body length input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetBodyLengthM(float value);

	// Returns the body length input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetBodyLengthM() const { return BodyLengthM; }

	// Updates the body width input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetBodyWidthM(float value);

	// Returns the body width input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetBodyWidthM() const { return BodyWidthM; }

	// Updates the body height input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetBodyHeightM(float value);

	// Returns the body height input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetBodyHeightM() const { return BodyHeightM; }

	// Updates the wheel base input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetBodyWheelBaseM(float value);

	// Returns the wheel base input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetBodyWheelBaseM() const { return BodyWheelBaseM; }

	// Updates the turning radius input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetBodyTurningRadiusM(float value);

	// Returns the turning radius input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetBodyTurningRadiusM() const { return BodyTurningRadiusM; }

	// Updates the max speed input in kilometers per hour.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetDriveMaxSpeedKmh(float value);

	// Returns the max speed input in kilometers per hour.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetDriveMaxSpeedKmh() const { return DriveMaxSpeedKmh; }

	// Updates the max reverse speed input in kilometers per hour.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetDriveMaxReverseSpeedKmh(float value);

	// Returns the max reverse speed input in kilometers per hour.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetDriveMaxReverseSpeedKmh() const { return DriveMaxReverseSpeedKmh; }

	// Updates the acceleration rate input in kilometers per hour per second.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetDriveAccelerationRateKmhPerSecond(float value);

	// Returns the acceleration rate input in kilometers per hour per second.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetDriveAccelerationRateKmhPerSecond() const { return DriveAccelerationRateKmhPerSecond; }

	// Updates the deceleration rate input in kilometers per hour per second.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetDriveDecelerationRateKmhPerSecond(float value);

	// Returns the deceleration rate input in kilometers per hour per second.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetDriveDecelerationRateKmhPerSecond() const { return DriveDecelerationRateKmhPerSecond; }

	// Updates the steering input interpolation rate.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetDriveSteeringRatePerS(float value);

	// Returns the steering input interpolation rate.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetDriveSteeringRatePerS() const { return DriveSteeringRatePerS; }

	// Updates the Chaos vehicle chassis mass input in kilograms.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetDriveMassKg(float value);

	// Returns the Chaos vehicle chassis mass input in kilograms.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetDriveMassKg() const { return DriveMassKg; }

	// Updates the LiDAR mode input.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarMode(const FString& value);

	// Returns the LiDAR mode input.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	FString GetLidarMode() const { return LidarMode; }

	// Updates the LiDAR debug visualization input.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarDrawDebug(bool bValue);

	// Returns the LiDAR debug visualization input.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	bool GetLidarDrawDebug() const { return bLidarDrawDebug; }

	// Updates the LiDAR scan range input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarScanRangeM(float value);

	// Returns the LiDAR scan range input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarScanRangeM() const { return LidarScanRangeM; }

	// Updates the LiDAR sensor height input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarSensorHeightM(float value);

	// Returns the LiDAR sensor height input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarSensorHeightM() const { return LidarSensorHeightM; }

	// Updates the LiDAR front half angle input in degrees.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarFrontHalfAngleDegree(float value);

	// Returns the LiDAR front half angle input in degrees.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarFrontHalfAngleDegree() const { return LidarFrontHalfAngleDegree; }

	// Updates the LiDAR stop distance input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarStopDistanceM(float value);

	// Returns the LiDAR stop distance input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarStopDistanceM() const { return LidarStopDistanceM; }

	// Updates the LiDAR slowdown distance input in meters.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarSlowDownDistanceM(float value);

	// Returns the LiDAR slowdown distance input in meters.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarSlowDownDistanceM() const { return LidarSlowDownDistanceM; }

	// Updates the LiDAR horizontal angle step input in degrees.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarAngleStepDegree(float value);

	// Returns the LiDAR horizontal angle step input in degrees.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarAngleStepDegree() const { return LidarAngleStepDegree; }

	// Updates the LiDAR scan rate input in hertz.
	UFUNCTION(BlueprintCallable, Category = "Platform|RobotProfile")
	void SetLidarScanRateHz(float value);

	// Returns the LiDAR scan rate input in hertz.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	float GetLidarScanRateHz() const { return LidarScanRateHz; }

	// Returns the profile.json path currently represented by the ViewModel.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	FString GetProfilePath() const { return ProfilePath; }

	// Returns whether the current editable robot profile values pass save validation.
	UFUNCTION(BlueprintPure, Category = "Platform|RobotProfile")
	bool CanSaveRobotProfile() const;

private:
	UProjectSessionSubsystem* ResolveProjectSessionSubsystem() const;
	bool ValidateInputs(TArray<FString>& outDiagnostics) const;
	FString ResolveProjectPath(const FString& projectPath) const;
	static FString BuildProfilePath(const FString& projectPath);
	FRobotProfileSettings MakeSettings() const;
	void ApplySettings(const FRobotProfileSettings& settings);

	// Subsystem lookup source.
	UPROPERTY(Transient)
	TObjectPtr<UGameInstance> GameInstance;

	// Optional project session supplied by tests or an explicit host.
	UPROPERTY(Transient)
	TObjectPtr<UProjectSessionSubsystem> ProjectSessionOverride;

	// Absolute profile.json path last loaded or saved by this ViewModel.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	FString ProfilePath;

	// robot.body.length_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float BodyLengthM = 0.60f;

	// robot.body.width_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float BodyWidthM = 0.90f;

	// robot.body.height_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float BodyHeightM = 0.50f;

	// robot.body.wheel_base_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float BodyWheelBaseM = 0.42f;

	// robot.body.turning_radius_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float BodyTurningRadiusM = 3.00f;

	// robot.drive.max_speed_kmh input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float DriveMaxSpeedKmh = 7.00f;

	// robot.drive.max_reverse_kmh input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float DriveMaxReverseSpeedKmh = 2.00f;

	// robot.drive.accel_kmh_per_s input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float DriveAccelerationRateKmhPerSecond = 1.20f;

	// robot.drive.decel_kmh_per_s input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float DriveDecelerationRateKmhPerSecond = 0.90f;

	// robot.drive.steering_rate_per_s input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float DriveSteeringRatePerS = 3.20f;

	// robot.drive.mass_kg input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float DriveMassKg = 48.00f;

	// robot.lidar.lidar_mode input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	FString LidarMode = TEXT("3D");

	// robot.lidar.draw_debug input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	bool bLidarDrawDebug = false;

	// robot.lidar.scan_range_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarScanRangeM = 15.00f;

	// robot.lidar.sensor_height_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarSensorHeightM = 0.07f;

	// robot.lidar.front_half_angle_degree input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarFrontHalfAngleDegree = 50.00f;

	// robot.lidar.stop_distance_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarStopDistanceM = 2.00f;

	// robot.lidar.slow_down_distance_m input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarSlowDownDistanceM = 8.00f;

	// robot.lidar.angle_step_degree input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarAngleStepDegree = 3.00f;

	// robot.lidar.scan_rate_hz input state.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Platform|RobotProfile", meta = (AllowPrivateAccess = "true"))
	float LidarScanRateHz = 5.00f;
};
