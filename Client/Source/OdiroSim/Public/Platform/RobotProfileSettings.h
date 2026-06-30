#pragma once

#include "CoreMinimal.h"
#include "RobotProfileSettings.generated.h"

// Editable robot body fields backed by user-project profile.json robot.body.
USTRUCT(BlueprintType)
struct ODIROSIM_API FRobotProfileBodySettings
{
	GENERATED_BODY()

	// robot.body.length_m value in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float LengthM = 0.60f;

	// robot.body.width_m value in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float WidthM = 0.90f;

	// robot.body.height_m value in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float HeightM = 0.50f;

	// robot.body.wheel_base_m value in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float WheelBaseM = 0.42f;

	// robot.body.turning_radius_m value in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float TurningRadiusM = 3.00f;
};

// Editable robot drive fields backed by user-project profile.json robot.drive.
USTRUCT(BlueprintType)
struct ODIROSIM_API FRobotProfileDriveSettings
{
	GENERATED_BODY()

	// robot.drive.max_speed_kmh value in kilometers per hour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float MaxSpeedKmh = 7.00f;

	// robot.drive.max_reverse_kmh value in kilometers per hour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float MaxReverseSpeedKmh = 2.00f;

	// robot.drive.accel_kmh_per_s acceleration rate in kilometers per hour per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float AccelerationRateKmhPerSecond = 1.20f;

	// robot.drive.decel_kmh_per_s deceleration rate in kilometers per hour per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float DecelerationRateKmhPerSecond = 0.90f;

	// robot.drive.steering_rate_per_s steering input interpolation rate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float SteeringRatePerS = 3.20f;

	// robot.drive.mass_kg value applied to the Chaos vehicle chassis when present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float MassKg = 48.00f;
};

// Editable robot LiDAR fields backed by user-project profile.json robot.lidar.
USTRUCT(BlueprintType)
struct ODIROSIM_API FRobotProfileLidarSettings
{
	GENERATED_BODY()

	// robot.lidar.lidar_mode scan dimension mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	FString LidarMode = TEXT("3D");

	// robot.lidar.draw_debug debug visualization flag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	bool bDrawDebug = false;

	// robot.lidar.scan_range_m value in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float ScanRangeM = 15.00f;

	// robot.lidar.sensor_height_m sensor origin height in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float SensorHeightM = 0.07f;

	// robot.lidar.sensor_forward_offset_m sensor origin forward offset in meters from the robot root.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float SensorForwardOffsetM = 0.00f;

	// robot.lidar.sensor_right_offset_m sensor origin right offset in meters from the robot root.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float SensorRightOffsetM = 0.00f;

	// robot.lidar.front_half_angle_degree front-facing half angle in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float FrontHalfAngleDegree = 50.00f;

	// robot.lidar.stop_distance_m obstacle stop distance in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float StopDistanceM = 2.00f;

	// robot.lidar.slow_down_distance_m obstacle slowdown distance in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float SlowDownDistanceM = 8.00f;

	// robot.lidar.angle_step_degree horizontal ray spacing in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float AngleStepDegree = 3.00f;

	// robot.lidar.vertical_step_degree vertical ray spacing in degrees for 3D mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float VerticalStepDegree = 5.00f;

	// robot.lidar.scan_rate_hz sensor update frequency in hertz.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	float ScanRateHz = 5.00f;
};

// Editable robot profile subset currently exposed by WBP_RobotConfigEditor.
USTRUCT(BlueprintType)
struct ODIROSIM_API FRobotProfileSettings
{
	GENERATED_BODY()

	// robot.body settings edited by the Body tab.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	FRobotProfileBodySettings Body;

	// robot.drive settings edited by the Drive tab.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	FRobotProfileDriveSettings Drive;

	// robot.lidar settings edited by the LiDAR tab.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|RobotProfile")
	FRobotProfileLidarSettings Lidar;
};
