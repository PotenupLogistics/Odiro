#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"

// Describes one robot-local LiDAR ray emitted by a scan pattern.
struct ODIROSIM_API FDeliveryBotLidarRaySample
{
	// Stable index within the ray dimension that produced this sample.
	int32 RayIndex = INDEX_NONE;

	// Robot-local raw yaw angle in degrees before signed reporting normalization.
	float YawDegree = 0.0f;

	// Robot-local pitch angle in degrees.
	float PitchDegree = 0.0f;

	// LiDAR dimension that produced this sample.
	EDeliveryBotLidarRayDimensionType DimensionType = EDeliveryBotLidarRayDimensionType::TwoD;

	// Robot-local unit direction derived from yaw and pitch.
	FVector LocalDirection = FVector::ForwardVector;

	// Vertical scan channel for model-specific 3D LiDAR samples.
	int32 ChannelIndex = INDEX_NONE;

	// Horizontal scan column for rotating 3D LiDAR samples.
	int32 ColumnIndex = INDEX_NONE;

	// Per-ray offset from the start of the simulated sensor frame.
	float RelativeTimeSeconds = 0.0f;

	// Sensor model that produced this sample when a concrete preset is active.
	FName SensorModel = NAME_None;
};

// Owns shared LiDAR yaw/pitch pattern math used by runtime tracing and robot preview rendering.
class ODIROSIM_API FDeliveryBotLidarRayPattern
{
public:
	// Rebuilds all ray samples required by Config.LidarModeType.
	static void BuildRaySamples(
		const FDeliveryBotLidarSensorConfigInfo& Config,
		TArray<FDeliveryBotLidarRaySample>& OutSamples);

	// Appends one dimension's ray samples without clearing OutSamples.
	static void AppendRaySamplesForDimension(
		const FDeliveryBotLidarSensorConfigInfo& Config,
		EDeliveryBotLidarRayDimensionType DimensionType,
		TArray<FDeliveryBotLidarRaySample>& OutSamples);

	// Returns whether a LiDAR mode includes a dimension.
	static bool DoesModeIncludeDimension(
		EDeliveryBotLidarModeType Mode,
		EDeliveryBotLidarRayDimensionType DimensionType);

	// Returns whether Mode uses the Ouster OS1 rotating 3D preset.
	static bool IsOusterOS1Mode(EDeliveryBotLidarModeType Mode);

	// Returns the model name used in sensor payloads and per-ray metadata.
	static FName GetSensorModelName(EDeliveryBotLidarModeType Mode);

	// Returns the horizontal ray count for one yaw sweep.
	static int32 CountYawSamples(const FDeliveryBotLidarSensorConfigInfo& Config);

	// Returns the vertical layer count for one 3D pitch sweep.
	static int32 CountPitchSamples(const FDeliveryBotLidarSensorConfigInfo& Config);

	// Returns the default Ouster OS1 channel count used by the simulation preset.
	static int32 GetOusterOS1ChannelCount();

	// Returns the default Ouster OS1 vertical field of view used by the simulation preset.
	static float GetOusterOS1VerticalFovDegree();

	// Normalizes a robot-local yaw angle to the signed [-180, 180] range.
	static float NormalizeSignedYawDegree(float YawDegree);

	// Returns whether YawDegree is inside the configured front half-angle.
	static bool IsFrontYaw(float YawDegree, float FrontHalfAngleDegree);
};
