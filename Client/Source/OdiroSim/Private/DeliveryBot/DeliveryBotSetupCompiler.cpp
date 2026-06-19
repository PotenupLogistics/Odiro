#include "DeliveryBot/DeliveryBotSetupCompiler.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ResolveDeliveryBotSetupJsonFilePath(const FString& jsonFilePath)
	{
		if (jsonFilePath.IsEmpty()) return jsonFilePath;

		if (FPaths::IsRelative(jsonFilePath))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), jsonFilePath));
		}

		return jsonFilePath;
	}
}

void UDeliveryBotSetupCompiler::AddDiagnostic(FDeliveryBotSetupCompileResult& result, EScenarioCompileDiagnosticSeverity severity, const FString& code, const FString& message)
{
	FScenarioCompileDiagnostic diagnostic;
	diagnostic.Severity = severity;
	diagnostic.Code = code;
	diagnostic.Message = message;
	result.Diagnostics.Add(diagnostic);
}

bool UDeliveryBotSetupCompiler::HasErrors(const FDeliveryBotSetupCompileResult& result)
{
	for (const FScenarioCompileDiagnostic& diagnostic : result.Diagnostics)
	{
		if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error) return true;
	}

	return false;
}

bool UDeliveryBotSetupCompiler::ReadOptionalFloatField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, float& targetValue, float minValue, float maxValue)
{
	if (!jsonObject.HasField(fieldName)) return false;

	double parsedValue = targetValue;
	if (!jsonObject.TryGetNumberField(fieldName, parsedValue))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_number"),
			FString::Printf(TEXT("%s.%s 필드는 숫자여야 함."), *path, *fieldName));
		return false;
	}

	targetValue = FMath::Clamp(static_cast<float>(parsedValue), minValue, maxValue);
	return true;
}

bool UDeliveryBotSetupCompiler::ReadOptionalIntField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, int32& targetValue, int32 minValue, int32 maxValue)
{
	if (!jsonObject.HasField(fieldName)) return false;

	double parsedValue = static_cast<double>(targetValue);
	if (!jsonObject.TryGetNumberField(fieldName, parsedValue))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_number"),
			FString::Printf(TEXT("%s.%s must be a number."), *path, *fieldName));
		return false;
	}

	targetValue = FMath::Clamp(FMath::RoundToInt(parsedValue), minValue, maxValue);
	return true;
}

bool UDeliveryBotSetupCompiler::ReadOptionalBoolField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, bool& targetValue)
{
	if (!jsonObject.HasField(fieldName)) return false;

	if (!jsonObject.TryGetBoolField(fieldName, targetValue))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_bool"),
			FString::Printf(TEXT("%s.%s 필드는 bool 값이어야 함."), *path, *fieldName));
		return false;
	}

	return true;
}

bool UDeliveryBotSetupCompiler::ReadOptionalNameArrayField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, TArray<FName>& targetValue)
{
	const TSharedPtr<FJsonValue> arrayValue = jsonObject.TryGetField(fieldName);
	if (!arrayValue.IsValid()) return false;

	if (arrayValue->Type != EJson::Array)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_string_array"),
			FString::Printf(TEXT("%s.%s 필드는 string 배열이어야 함."), *path, *fieldName));
		return false;
	}

	TArray<FName> parsedNames;
	const TArray<TSharedPtr<FJsonValue>> jsonArray = arrayValue->AsArray();
	for (int32 index = 0; index < jsonArray.Num(); ++index)
	{
		const TSharedPtr<FJsonValue>& jsonValue = jsonArray[index];
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::String)
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("invalid_string_array"),
				FString::Printf(TEXT("%s.%s[%d] 값은 string이어야 함."), *path, *fieldName, index));
			return false;
		}

		parsedNames.Add(FName(*jsonValue->AsString()));
	}

	targetValue = parsedNames;
	return true;
}

bool UDeliveryBotSetupCompiler::ReadOptionalCollisionChannelField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, TEnumAsByte<ECollisionChannel>& targetValue)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid()) return false;

	if (jsonValue->Type == EJson::Number)
	{
		const int32 channelValue = FMath::RoundToInt(jsonValue->AsNumber());
		if (channelValue < 0 || channelValue >= ECC_MAX)
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("invalid_collision_channel"),
				FString::Printf(TEXT("%s.%s 값이 collision channel 범위를 벗어남."), *path, *fieldName));
			return false;
		}

		targetValue = static_cast<ECollisionChannel>(channelValue);
		return true;
	}

	if (jsonValue->Type != EJson::String)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_collision_channel"),
			FString::Printf(TEXT("%s.%s 필드는 string 또는 number여야 함."), *path, *fieldName));
		return false;
	}

	const FString normalized = jsonValue->AsString().ToLower().Replace(TEXT("_"), TEXT(""));
	if (normalized == TEXT("worldstatic"))
	{
		targetValue = ECC_WorldStatic;
		return true;
	}
	if (normalized == TEXT("worlddynamic"))
	{
		targetValue = ECC_WorldDynamic;
		return true;
	}
	if (normalized == TEXT("pawn"))
	{
		targetValue = ECC_Pawn;
		return true;
	}
	if (normalized == TEXT("visibility"))
	{
		targetValue = ECC_Visibility;
		return true;
	}
	if (normalized == TEXT("camera"))
	{
		targetValue = ECC_Camera;
		return true;
	}
	if (normalized == TEXT("physicsbody"))
	{
		targetValue = ECC_PhysicsBody;
		return true;
	}
	if (normalized == TEXT("vehicle"))
	{
		targetValue = ECC_Vehicle;
		return true;
	}
	if (normalized == TEXT("destructible"))
	{
		targetValue = ECC_Destructible;
		return true;
	}

	AddDiagnostic(
		result,
		EScenarioCompileDiagnosticSeverity::Error,
		TEXT("invalid_collision_channel"),
		FString::Printf(TEXT("%s.%s '%s' 값은 지원하지 않음."), *path, *fieldName, *jsonValue->AsString()));
	return false;
}

bool UDeliveryBotSetupCompiler::ReadOptionalLidarModeField(const FJsonObject& jsonObject, const FString& fieldName,
	const FString& path, FDeliveryBotSetupCompileResult& result, EDeliveryBotLidarModeType& targetValue)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid())
		return false;

	if (jsonValue->Type != EJson::String)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_lidar_mode"),
			FString::Printf(TEXT("%s.%s 필드는 string이어야 함."), *path, *fieldName));
		return false;
	}

	const FString normalized = jsonValue->AsString().ToLower().Replace(TEXT("_"), TEXT(""));

	if (normalized == TEXT("1d") || normalized == TEXT("oned"))
	{
		targetValue = EDeliveryBotLidarModeType::OneD;
		return true;
	}

	if (normalized == TEXT("2d") || normalized == TEXT("twod"))
	{
		targetValue = EDeliveryBotLidarModeType::TwoD;
		return true;
	}

	if (normalized == TEXT("front2d"))
	{
		targetValue = EDeliveryBotLidarModeType::TwoD;
		return true;
	}

	if (normalized == TEXT("3d") || normalized == TEXT("threed"))
	{
		targetValue = EDeliveryBotLidarModeType::ThreeD;
		return true;
	}

	if (normalized == TEXT("1dand2d") || normalized == TEXT("onedandtwod"))
	{
		targetValue = EDeliveryBotLidarModeType::OneDAndTwoD;
		return true;
	}

	if (normalized == TEXT("2dand3d") || normalized == TEXT("twodandthreed"))
	{
		targetValue = EDeliveryBotLidarModeType::TwoDAndThreeD;
		return true;
	}

	if (normalized == TEXT("all"))
	{
		targetValue = EDeliveryBotLidarModeType::All;
		return true;
	}

	AddDiagnostic(
		result,
		EScenarioCompileDiagnosticSeverity::Error,
		TEXT("invalid_lidar_mode"),
		FString::Printf(TEXT("%s.%s '%s' 값은 지원하지 않음."), *path, *fieldName, *jsonValue->AsString()));
	return false;
}

void UDeliveryBotSetupCompiler::CompileBody(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotBodyConfigInfo& bodyConfigInfo)
{
	const TSharedPtr<FJsonValue> bodyValue = robotObject.TryGetField(TEXT("body"));
	if (!bodyValue.IsValid()) return;

	if (bodyValue->Type != EJson::Object)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_object"),
			TEXT("robot.body 필드는 object여야 함."));
		return;
	}

	const TSharedPtr<FJsonObject> bodyObject = bodyValue->AsObject();
	if (!bodyObject.IsValid()) return;

	const FString path = TEXT("robot.body");
	bodyConfigInfo.bHasSetupBodyConfig = true;
	ReadOptionalFloatField(*bodyObject, TEXT("length_m"), path, result, bodyConfigInfo.LengthM, 0.01f);
	ReadOptionalFloatField(*bodyObject, TEXT("width_m"), path, result, bodyConfigInfo.WidthM, 0.01f);
	ReadOptionalFloatField(*bodyObject, TEXT("height_m"), path, result, bodyConfigInfo.HeightM, 0.01f);
	ReadOptionalFloatField(*bodyObject, TEXT("wheel_base_m"), path, result, bodyConfigInfo.WheelBaseM, 0.0f);
	ReadOptionalFloatField(*bodyObject, TEXT("turning_radius_m"), path, result, bodyConfigInfo.TurningRadiusM, 0.0f);
}

void UDeliveryBotSetupCompiler::CompileDrive( const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotDriveConfigInfo& driveConfigInfo)
{
	const TSharedPtr<FJsonValue> driveValue = robotObject.TryGetField(TEXT("drive"));
	if (!driveValue.IsValid()) return;

	if (driveValue->Type != EJson::Object)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_object"),
			TEXT("robot.drive 필드는 object여야 함."));
		return;
	}

	const TSharedPtr<FJsonObject> driveObject = driveValue->AsObject();
	if (!driveObject.IsValid()) return;

	const FString path = TEXT("robot.drive");
	ReadOptionalFloatField(*driveObject, TEXT("max_speed_kmh"), path, result, driveConfigInfo.MaxSpeedKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("max_reverse_speed_kmh"), path, result, driveConfigInfo.MaxReverseSpeedKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("max_reverse_kmh"), path, result, driveConfigInfo.MaxReverseSpeedKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("reverse_acceleration_rate_kmh_per_second"), path, result, driveConfigInfo.ReverseAccelerationRateKmhPerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("reverse_accel_kmh_per_s"), path, result, driveConfigInfo.ReverseAccelerationRateKmhPerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("gear_switch_stop_speed_kmh"), path, result, driveConfigInfo.GearSwitchStopSpeedKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("gear_switch_stop_kmh"), path, result, driveConfigInfo.GearSwitchStopSpeedKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("gear_switch_brake_input"), path, result, driveConfigInfo.GearSwitchBrakeInput, 0.0f, 1.0f);
	ReadOptionalFloatField(*driveObject, TEXT("gear_switch_brake"), path, result, driveConfigInfo.GearSwitchBrakeInput, 0.0f, 1.0f);
	ReadOptionalFloatField(*driveObject, TEXT("slowdown_speed_range_kmh"), path, result, driveConfigInfo.SlowdownSpeedRangeKmh, 0.1f);
	ReadOptionalFloatField(*driveObject, TEXT("slowdown_range_kmh"), path, result, driveConfigInfo.SlowdownSpeedRangeKmh, 0.1f);
	ReadOptionalFloatField(*driveObject, TEXT("speed_limit_tolerance_kmh"), path, result, driveConfigInfo.SpeedLimitToleranceKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("speed_tolerance_kmh"), path, result, driveConfigInfo.SpeedLimitToleranceKmh, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("speed_limit_brake"), path, result, driveConfigInfo.SpeedLimitBrake, 0.0f, 1.0f);
	ReadOptionalFloatField(*driveObject, TEXT("stop_brake_input"), path, result, driveConfigInfo.StopBrakeInput, 0.0f, 1.0f);
	ReadOptionalFloatField(*driveObject, TEXT("stop_brake"), path, result, driveConfigInfo.StopBrakeInput, 0.0f, 1.0f);
	ReadOptionalFloatField(*driveObject, TEXT("throttle_input_rate_per_second"), path, result, driveConfigInfo.ThrottleInputRatePerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("throttle_rate_per_s"), path, result, driveConfigInfo.ThrottleInputRatePerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("brake_input_rate_per_second"), path, result, driveConfigInfo.BrakeInputRatePerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("brake_rate_per_s"), path, result, driveConfigInfo.BrakeInputRatePerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("steering_input_rate_per_second"), path, result, driveConfigInfo.SteeringInputRatePerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("steering_rate_per_s"), path, result, driveConfigInfo.SteeringInputRatePerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("acceleration_rate_kmh_per_second"), path, result, driveConfigInfo.AccelerationRateKmhPerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("accel_kmh_per_s"), path, result, driveConfigInfo.AccelerationRateKmhPerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("deceleration_rate_kmh_per_second"), path, result, driveConfigInfo.DecelerationRateKmhPerSecond, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("decel_kmh_per_s"), path, result, driveConfigInfo.DecelerationRateKmhPerSecond, 0.0f);
	ReadOptionalBoolField(*driveObject, TEXT("use_handbrake_when_brake"), path, result, driveConfigInfo.bUseHandbrakeWhenBrake);
	ReadOptionalBoolField(*driveObject, TEXT("use_handbrake"), path, result, driveConfigInfo.bUseHandbrakeWhenBrake);
	ReadOptionalFloatField(*driveObject, TEXT("max_torque"), path, result, driveConfigInfo.MaxTorque, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("max_rpm"), path, result, driveConfigInfo.MaxRPM, 1.0f);
	ReadOptionalFloatField(*driveObject, TEXT("engine_idle_rpm"), path, result, driveConfigInfo.EngineIdleRPM, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("idle_rpm"), path, result, driveConfigInfo.EngineIdleRPM, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("engine_brake_effect"), path, result, driveConfigInfo.EngineBrakeEffect, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("engine_brake"), path, result, driveConfigInfo.EngineBrakeEffect, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("engine_rev_up_moi"), path, result, driveConfigInfo.EngineRevUpMOI, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("rev_up_moi"), path, result, driveConfigInfo.EngineRevUpMOI, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("engine_rev_down_rate"), path, result, driveConfigInfo.EngineRevDownRate, 0.0f);
	ReadOptionalFloatField(*driveObject, TEXT("rev_down_rate"), path, result, driveConfigInfo.EngineRevDownRate, 0.0f);
}

void UDeliveryBotSetupCompiler::WarnDeprecatedPathFollow(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result)
{
	const TSharedPtr<FJsonValue> pathFollowValue = robotObject.TryGetField(TEXT("path_follow"));
	if (!pathFollowValue.IsValid()) return;

	if (pathFollowValue->Type != EJson::Object)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Warning,
			TEXT("deprecated_path_follow"),
			TEXT("robot.path_follow is ignored. Goal arrival is evaluated by ScenarioEvaluation, and Python policy tuning lives in Tools/PythonAgent."));
		return;
	}

	const TSharedPtr<FJsonObject> pathFollowObject = pathFollowValue->AsObject();
	if (!pathFollowObject.IsValid()) return;

	const FString path = TEXT("robot.path_follow");

	static const TCHAR* DeprecatedPathFollowFields[] = {
		TEXT("goal_acceptance_distance_m"),
		TEXT("draw_debug"),
		TEXT("target_speed_kmh"),
		TEXT("look_ahead_distance_m"),
		TEXT("min_look_ahead_distance_m"),
		TEXT("max_look_ahead_distance_m"),
		TEXT("look_ahead_speed_gain_m_per_kmh"),
		TEXT("look_ahead_steering_reduction_ratio"),
		TEXT("look_ahead_smoothing_ratio"),
		TEXT("path_point_acceptance_distance_m"),
		TEXT("goal_slow_down_distance_m"),
		TEXT("goal_approach_speed_kmh"),
		TEXT("goal_approach_look_ahead_distance_m"),
		TEXT("steering_sensitivity"),
		TEXT("steering_full_scale_degree"),
		TEXT("max_steering"),
		TEXT("max_steering_delta"),
		TEXT("min_turn_speed_kmh"),
		TEXT("obstacle_slow_speed_kmh"),
		TEXT("obstacle_soft_cost_radius_m"),
		TEXT("obstacle_soft_cost_max_penalty"),
		TEXT("obstacle_soft_cost_power"),
		TEXT("path_turn_cost_penalty"),
		TEXT("path_smoothing_distance_m"),
		TEXT("allow_diagonal_pathfinding"),
		TEXT("smooth_path_with_line_of_sight"),
		TEXT("use_exact_goal_as_final_point"),
	};

	for (const TCHAR* deprecatedField : DeprecatedPathFollowFields)
	{
		if (!pathFollowObject->HasField(deprecatedField))
		{
			continue;
		}

		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Warning,
			TEXT("deprecated_path_follow_field"),
			FString::Printf(
				TEXT("%s.%s is ignored. Goal arrival is evaluated by ScenarioEvaluation, and Python policy tuning lives in Tools/PythonAgent."),
				*path,
				deprecatedField));
	}
}

void UDeliveryBotSetupCompiler::CompileLidar(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotLidarSensorConfigInfo& lidarSensorConfigInfo, FDeliveryBotPointCloudCaptureConfigInfo& pointCloudCaptureConfigInfo)
{
	const TSharedPtr<FJsonValue> lidarValue = robotObject.TryGetField(TEXT("lidar"));
	if (!lidarValue.IsValid()) return;

	if (lidarValue->Type != EJson::Object)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_object"),
			TEXT("robot.lidar 필드는 object여야 함."));
		return;
	}

	const TSharedPtr<FJsonObject> lidarObject = lidarValue->AsObject();
	if (!lidarObject.IsValid()) return;

	const FString path = TEXT("robot.lidar");
	ReadOptionalBoolField(*lidarObject, TEXT("draw_debug"), path, result, lidarSensorConfigInfo.bDrawDebug);
	ReadOptionalBoolField(*lidarObject, TEXT("draw_near_miss_debug"), path, result, lidarSensorConfigInfo.bDrawObstacleWarningDebug);
	ReadOptionalBoolField(*lidarObject, TEXT("draw_near_obstacle_warning_debug"), path, result, lidarSensorConfigInfo.bDrawObstacleWarningDebug);
	ReadOptionalBoolField(*lidarObject, TEXT("draw_obstacle_warning_debug"), path, result, lidarSensorConfigInfo.bDrawObstacleWarningDebug);
	ReadOptionalFloatField(*lidarObject, TEXT("scan_range_m"), path, result, lidarSensorConfigInfo.ScanRangeM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("range_m"), path, result, lidarSensorConfigInfo.ScanRangeM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("angle_step_degree"), path, result, lidarSensorConfigInfo.AngleStepDegree, 1.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("sensor_height_m"), path, result, lidarSensorConfigInfo.SensorHeightM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("height_m"), path, result, lidarSensorConfigInfo.SensorHeightM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("front_half_angle_degree"), path, result, lidarSensorConfigInfo.FrontHalfAngleDegree, 0.0f, 180.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("stop_distance_m"), path, result, lidarSensorConfigInfo.StopDistanceM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("near_miss_distance_m"), path, result, lidarSensorConfigInfo.ObstacleWarningDistanceM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("near_obstacle_warning_distance_m"), path, result, lidarSensorConfigInfo.ObstacleWarningDistanceM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("obstacle_warning_distance_m"), path, result, lidarSensorConfigInfo.ObstacleWarningDistanceM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("slow_down_distance_m"), path, result, lidarSensorConfigInfo.SlowDownDistanceM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("collision_stop_half_angle_degree"), path, result, lidarSensorConfigInfo.CollisionStopHalfAngleDegree, 0.0f, 180.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("collision_stop_distance_m"), path, result, lidarSensorConfigInfo.CollisionStopDistanceM, 0.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("scan_rate_hz"), path, result, lidarSensorConfigInfo.ScanRateHz, 0.1f);
	ReadOptionalLidarModeField(*lidarObject, TEXT("lidar_mode"), path, result, lidarSensorConfigInfo.LidarModeType);
	ReadOptionalLidarModeField(*lidarObject, TEXT("mode"), path, result, lidarSensorConfigInfo.LidarModeType);
	ReadOptionalBoolField(*lidarObject, TEXT("store_missed_rays"), path, result, lidarSensorConfigInfo.bStoreMissedRays);
	ReadOptionalFloatField(*lidarObject, TEXT("vertical_min_degree"), path, result, lidarSensorConfigInfo.VerticalMinDegree, -89.0f, 89.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("vertical_max_degree"), path, result, lidarSensorConfigInfo.VerticalMaxDegree, -89.0f, 89.0f);
	ReadOptionalFloatField(*lidarObject, TEXT("vertical_step_degree"), path, result, lidarSensorConfigInfo.VerticalStepDegree, 1.0f);
	CompilePointCloudCapture(*lidarObject, result, pointCloudCaptureConfigInfo);
	
	lidarSensorConfigInfo.ObstacleWarningDistanceM = FMath::Max(
		lidarSensorConfigInfo.ObstacleWarningDistanceM,
		lidarSensorConfigInfo.StopDistanceM + 0.1f);
	lidarSensorConfigInfo.SlowDownDistanceM = FMath::Max(
		lidarSensorConfigInfo.SlowDownDistanceM,
		lidarSensorConfigInfo.ObstacleWarningDistanceM + 0.1f);
	lidarSensorConfigInfo.CollisionStopHalfAngleDegree = FMath::Clamp(
		lidarSensorConfigInfo.CollisionStopHalfAngleDegree,
		0.0f,
		lidarSensorConfigInfo.FrontHalfAngleDegree);
	lidarSensorConfigInfo.CollisionStopDistanceM = FMath::Clamp(
		lidarSensorConfigInfo.CollisionStopDistanceM,
		0.0f,
		lidarSensorConfigInfo.SlowDownDistanceM);
}

void UDeliveryBotSetupCompiler::CompilePointCloudCapture(const FJsonObject& lidarObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotPointCloudCaptureConfigInfo& pointCloudCaptureConfigInfo)
{
	const FString lidarPath = TEXT("robot.lidar");
	if (lidarObject.HasField(TEXT("observation_profile")))
	{
		pointCloudCaptureConfigInfo.bHasSetupPointCloudConfig = true;
		ReadOptionalStringField(lidarObject, TEXT("observation_profile"), lidarPath, result, pointCloudCaptureConfigInfo.ObservationProfile);
	}

	const TSharedPtr<FJsonValue> pointCloudValue = lidarObject.TryGetField(TEXT("point_cloud"));
	if (!pointCloudValue.IsValid())
	{
		return;
	}

	pointCloudCaptureConfigInfo.bHasSetupPointCloudConfig = true;
	if (pointCloudValue->Type != EJson::Object)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_object"),
			TEXT("robot.lidar.point_cloud must be an object."));
		return;
	}

	const TSharedPtr<FJsonObject> pointCloudObject = pointCloudValue->AsObject();
	if (!pointCloudObject.IsValid())
	{
		return;
	}

	const FString pointCloudPath = TEXT("robot.lidar.point_cloud");
	ReadOptionalBoolField(*pointCloudObject, TEXT("capture_enabled"), pointCloudPath, result, pointCloudCaptureConfigInfo.bCaptureEnabled);
	ReadOptionalIntField(*pointCloudObject, TEXT("capture_every_n_sensor_frames"), pointCloudPath, result, pointCloudCaptureConfigInfo.CaptureEveryNSensorFrames, 1);
	ReadOptionalFloatField(*pointCloudObject, TEXT("range_limit_m"), pointCloudPath, result, pointCloudCaptureConfigInfo.RangeLimitM, 0.0f);
	ReadOptionalBoolField(*pointCloudObject, TEXT("include_ground_points"), pointCloudPath, result, pointCloudCaptureConfigInfo.bIncludeGroundPoints);
	ReadOptionalIntField(*pointCloudObject, TEXT("max_points"), pointCloudPath, result, pointCloudCaptureConfigInfo.MaxPoints, 1);
}

bool UDeliveryBotSetupCompiler::ReadOptionalStringField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, FString& targetValue)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid()) return false;

	if (jsonValue->Type != EJson::String)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_string"),
			FString::Printf(TEXT("%s.%s must be a string."), *path, *fieldName));
		return false;
	}

	const FString trimmedValue = jsonValue->AsString().TrimStartAndEnd();
	if (trimmedValue.IsEmpty())
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("empty_string"),
			FString::Printf(TEXT("%s.%s must not be empty."), *path, *fieldName));
		return false;
	}

	targetValue = trimmedValue;
	return true;
}

void UDeliveryBotSetupCompiler::CompileRobotObject(const FJsonObject& rootObject, FDeliveryBotSetupCompileResult& result)
{
	if (rootObject.HasField(TEXT("run")) || rootObject.HasField(TEXT("actors")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("episode_fields_in_delivery_bot_setup"),
			TEXT("DeliveryBotSetup JSON에는 run/actors를 넣지 않음. 시나리오 배치 정보는 ScenarioSetup JSON이 담당함."));
	}

	const TSharedPtr<FJsonValue> robotValue = rootObject.TryGetField(TEXT("robot"));
	if (!robotValue.IsValid())
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Warning,
			TEXT("missing_robot"),
			TEXT("robot 필드가 없어 FDeliveryBotSetupInfo 기본값을 사용함."));
		return;
	}

	if (robotValue->Type != EJson::Object)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_object"),
			TEXT("robot 필드는 object여야 함."));
		return;
	}

	const TSharedPtr<FJsonObject> robotObject = robotValue->AsObject();
	if (!robotObject.IsValid()) return;

	if (robotObject->HasField(TEXT("location"))
		|| robotObject->HasField(TEXT("route"))
		|| robotObject->HasField(TEXT("instance_id"))
		|| robotObject->HasField(TEXT("asset_id"))
		|| robotObject->HasField(TEXT("spawn_only")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("episode_robot_fields_in_delivery_bot_setup"),
			TEXT("DeliveryBotSetup.robot에는 location/route/instance_id/asset_id/spawn_only를 넣지 않음. 로봇 배치와 목적지는 ScenarioSetup JSON이 담당함."));
	}

	CompileBody(*robotObject, result, result.SetupInfo.BodyConfigInfo);
	CompileDrive(*robotObject, result, result.SetupInfo.ChaosDriveConfigInfo);
	WarnDeprecatedPathFollow(*robotObject, result);
	CompileLidar(*robotObject, result, result.SetupInfo.LidarSensorConfigInfo, result.SetupInfo.PointCloudCaptureConfigInfo);
	CompilePolicy(*robotObject, result, result.SetupInfo.StartupPolicySpecFileName);
}

FDeliveryBotSetupCompileResult UDeliveryBotSetupCompiler::CompileDeliveryBotSetupFromJsonFile(const FString& jsonFilePath) const
{
	FDeliveryBotSetupCompileResult result;
	const FString resolvedJsonFilePath = ResolveDeliveryBotSetupJsonFilePath(jsonFilePath);

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedJsonFilePath))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("file_read_failed"),
			FString::Printf(TEXT("DeliveryBotSetup JSON 파일 '%s' 읽기 실패. ResolvedPath: '%s'"), *jsonFilePath, *resolvedJsonFilePath));
		result.bSuccess = false;
		return result;
	}

	return CompileDeliveryBotSetupFromJsonString(jsonString);
}

FDeliveryBotSetupCompileResult UDeliveryBotSetupCompiler::CompileDeliveryBotSetupFromJsonString(const FString& jsonString) const
{
	FDeliveryBotSetupCompileResult result;

	if (jsonString.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("empty_json"),
			TEXT("DeliveryBotSetup JSON 입력이 비어 있음."));
		result.bSuccess = false;
		return result;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_json"),
			TEXT("DeliveryBotSetup JSON 입력을 파싱할 수 없음."));
		result.bSuccess = false;
		return result;
	}

	CompileRobotObject(*rootObject, result);
	result.SpecHash = FString::Printf(TEXT("%u"), GetTypeHash(jsonString));
	result.bSuccess = !HasErrors(result);
	return result;
}

void UDeliveryBotSetupCompiler::CompilePolicy(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FString& startupPolicySpecFileName)
{
	const TSharedPtr<FJsonValue> policyValue = robotObject.TryGetField(TEXT("policy"));
	if (!policyValue.IsValid()) return;

	if (policyValue->Type != EJson::Object)
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_object"), TEXT("robot.policy must be an object."));
		return;
	}

	const TSharedPtr<FJsonObject> policyObject = policyValue->AsObject();
	if (!policyObject.IsValid()) return;

	ReadOptionalStringField(*policyObject,TEXT("startup_policy_spec_file_name"), TEXT("robot.policy"), result, startupPolicySpecFileName);
}
