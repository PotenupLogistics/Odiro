#include "Scenario/ScenarioSimulationProfileAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr const TCHAR* SimulationProfileSchemaName = TEXT("simulation_profile");
constexpr const TCHAR* SimulationProfileUnsetHash = TEXT("hash:unset");

FString ResolveSimulationProfilePath(const FString& JsonFilePath)
{
	if (FPaths::IsRelative(JsonFilePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), JsonFilePath));
	}

	return FPaths::ConvertRelativePathToFull(JsonFilePath);
}

void AddProfileDiagnostic(
	FScenarioSimulationProfileCompileResult& Result,
	EScenarioCompileDiagnosticSeverity Severity,
	const FString& Code,
	const FString& Message)
{
	FScenarioCompileDiagnostic Diagnostic;
	Diagnostic.Severity = Severity;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Result.Diagnostics.Add(Diagnostic);
}

bool HasProfileErrors(const FScenarioSimulationProfileCompileResult& Result)
{
	for (const FScenarioCompileDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
		{
			return true;
		}
	}

	return false;
}

bool TryReadProfileRootObject(
	const FString& JsonString,
	FScenarioSimulationProfileCompileResult& Result,
	TSharedPtr<FJsonObject>& OutRootObject)
{
	if (JsonString.TrimStartAndEnd().IsEmpty())
	{
		AddProfileDiagnostic(Result, EScenarioCompileDiagnosticSeverity::Error, TEXT("empty_json"), TEXT("simulation_profile JSON input is empty."));
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, OutRootObject) || !OutRootObject.IsValid())
	{
		AddProfileDiagnostic(Result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_json"), TEXT("simulation_profile JSON input could not be parsed."));
		return false;
	}

	return true;
}

bool TryReadSimulationProfileSchema(
	const FString& JsonFilePath,
	FString& OutSchema)
{
	const FString ResolvedPath = ResolveSimulationProfilePath(JsonFilePath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	FScenarioSimulationProfileCompileResult Result;
	if (!TryReadProfileRootObject(JsonText, Result, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	return RootObject->TryGetStringField(TEXT("schema"), OutSchema);
}

bool ReadOptionalProfileFloat(
	const FJsonObject& JsonObject,
	const FString& FieldName,
	const FString& Path,
	FScenarioSimulationProfileCompileResult& Result,
	float& TargetValue,
	float MinValue,
	float MaxValue = TNumericLimits<float>::Max())
{
	if (!JsonObject.HasField(FieldName))
	{
		return false;
	}

	double ParsedValue = TargetValue;
	if (!JsonObject.TryGetNumberField(FieldName, ParsedValue))
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_number"),
			FString::Printf(TEXT("%s.%s must be a number."), *Path, *FieldName));
		return false;
	}

	TargetValue = FMath::Clamp(static_cast<float>(ParsedValue), MinValue, MaxValue);
	return true;
}

bool ReadOptionalProfileBool(
	const FJsonObject& JsonObject,
	const FString& FieldName,
	const FString& Path,
	FScenarioSimulationProfileCompileResult& Result,
	bool& TargetValue)
{
	if (!JsonObject.HasField(FieldName))
	{
		return false;
	}

	if (!JsonObject.TryGetBoolField(FieldName, TargetValue))
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_bool"),
			FString::Printf(TEXT("%s.%s must be a boolean."), *Path, *FieldName));
		return false;
	}

	return true;
}

bool TryGetOptionalProfileObject(
	const FJsonObject& JsonObject,
	const FString& FieldName,
	const FString& Path,
	FScenarioSimulationProfileCompileResult& Result,
	TSharedPtr<FJsonObject>& OutObject)
{
	const TSharedPtr<FJsonValue> JsonValue = JsonObject.TryGetField(FieldName);
	if (!JsonValue.IsValid())
	{
		return false;
	}

	if (JsonValue->Type != EJson::Object)
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_object"),
			FString::Printf(TEXT("%s.%s must be an object."), *Path, *FieldName));
		return false;
	}

	OutObject = JsonValue->AsObject();
	return OutObject.IsValid();
}

bool ReadOptionalProfileLidarMode(
	const FJsonObject& JsonObject,
	const FString& FieldName,
	const FString& Path,
	FScenarioSimulationProfileCompileResult& Result,
	EDeliveryBotLidarModeType& TargetValue)
{
	const TSharedPtr<FJsonValue> JsonValue = JsonObject.TryGetField(FieldName);
	if (!JsonValue.IsValid())
	{
		return false;
	}

	if (JsonValue->Type != EJson::String)
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_lidar_mode"),
			FString::Printf(TEXT("%s.%s must be a string."), *Path, *FieldName));
		return false;
	}

	FString Normalized = JsonValue->AsString().ToLower();
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));

	if (Normalized == TEXT("oned"))
	{
		TargetValue = EDeliveryBotLidarModeType::OneD;
		return true;
	}
	if (Normalized == TEXT("twod"))
	{
		TargetValue = EDeliveryBotLidarModeType::TwoD;
		return true;
	}
	if (Normalized == TEXT("threed"))
	{
		TargetValue = EDeliveryBotLidarModeType::ThreeD;
		return true;
	}
	if (Normalized == TEXT("onedandtwod"))
	{
		TargetValue = EDeliveryBotLidarModeType::OneDAndTwoD;
		return true;
	}
	if (Normalized == TEXT("twodandthreed"))
	{
		TargetValue = EDeliveryBotLidarModeType::TwoDAndThreeD;
		return true;
	}
	if (Normalized == TEXT("all"))
	{
		TargetValue = EDeliveryBotLidarModeType::All;
		return true;
	}

	AddProfileDiagnostic(
		Result,
		EScenarioCompileDiagnosticSeverity::Error,
		TEXT("invalid_lidar_mode"),
		FString::Printf(TEXT("%s.%s '%s' is not supported."), *Path, *FieldName, *JsonValue->AsString()));
	return false;
}

void CompileProfileDriveSpec(
	const FJsonObject& RootObject,
	FScenarioSimulationProfileCompileResult& Result)
{
	TSharedPtr<FJsonObject> DriveObject;
	if (!TryGetOptionalProfileObject(RootObject, TEXT("drive_spec"), TEXT("$"), Result, DriveObject) || !DriveObject.IsValid())
	{
		return;
	}

	FDeliveryBotDriveConfigInfo& DriveConfig = Result.SetupInfo.ChaosDriveConfigInfo;
	const FString Path = TEXT("$.drive_spec");
	ReadOptionalProfileFloat(*DriveObject, TEXT("max_speed_kmh"), Path, Result, DriveConfig.MaxSpeedKmh, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("max_reverse_kmh"), Path, Result, DriveConfig.MaxReverseSpeedKmh, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("accel_kmh_per_s"), Path, Result, DriveConfig.AccelerationRateKmhPerSecond, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("decel_kmh_per_s"), Path, Result, DriveConfig.DecelerationRateKmhPerSecond, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("reverse_accel_kmh_per_s"), Path, Result, DriveConfig.ReverseAccelerationRateKmhPerSecond, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("steering_rate_per_s"), Path, Result, DriveConfig.SteeringInputRatePerSecond, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("throttle_rate_per_s"), Path, Result, DriveConfig.ThrottleInputRatePerSecond, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("brake_rate_per_s"), Path, Result, DriveConfig.BrakeInputRatePerSecond, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("stop_brake"), Path, Result, DriveConfig.StopBrakeInput, 0.0f, 1.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("gear_switch_stop_kmh"), Path, Result, DriveConfig.GearSwitchStopSpeedKmh, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("gear_switch_brake"), Path, Result, DriveConfig.GearSwitchBrakeInput, 0.0f, 1.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("slowdown_range_kmh"), Path, Result, DriveConfig.SlowdownSpeedRangeKmh, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("speed_tolerance_kmh"), Path, Result, DriveConfig.SpeedLimitToleranceKmh, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("speed_limit_brake"), Path, Result, DriveConfig.SpeedLimitBrake, 0.0f, 1.0f);
	ReadOptionalProfileBool(*DriveObject, TEXT("use_handbrake"), Path, Result, DriveConfig.bUseHandbrakeWhenBrake);
	ReadOptionalProfileFloat(*DriveObject, TEXT("max_torque"), Path, Result, DriveConfig.MaxTorque, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("max_rpm"), Path, Result, DriveConfig.MaxRPM, 1.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("idle_rpm"), Path, Result, DriveConfig.EngineIdleRPM, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("engine_brake"), Path, Result, DriveConfig.EngineBrakeEffect, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("rev_up_moi"), Path, Result, DriveConfig.EngineRevUpMOI, 0.0f);
	ReadOptionalProfileFloat(*DriveObject, TEXT("rev_down_rate"), Path, Result, DriveConfig.EngineRevDownRate, 0.0f);
}

void CompileProfileLidarSpec(
	const FJsonObject& RootObject,
	FScenarioSimulationProfileCompileResult& Result)
{
	TSharedPtr<FJsonObject> LidarObject;
	if (!TryGetOptionalProfileObject(RootObject, TEXT("lidar_spec"), TEXT("$"), Result, LidarObject) || !LidarObject.IsValid())
	{
		return;
	}

	FDeliveryBotLidarSensorConfigInfo& LidarConfig = Result.SetupInfo.LidarSensorConfigInfo;
	const FString Path = TEXT("$.lidar_spec");
	ReadOptionalProfileLidarMode(*LidarObject, TEXT("mode"), Path, Result, LidarConfig.LidarModeType);
	ReadOptionalProfileFloat(*LidarObject, TEXT("range_m"), Path, Result, LidarConfig.ScanRangeM, 0.0f);
	ReadOptionalProfileFloat(*LidarObject, TEXT("angle_step_degree"), Path, Result, LidarConfig.AngleStepDegree, 1.0f);
	ReadOptionalProfileFloat(*LidarObject, TEXT("height_m"), Path, Result, LidarConfig.SensorHeightM, 0.0f);
	ReadOptionalProfileBool(*LidarObject, TEXT("store_missed_rays"), Path, Result, LidarConfig.bStoreMissedRays);
}

void ValidateProfileHeader(
	const FJsonObject& RootObject,
	FScenarioSimulationProfileCompileResult& Result)
{
	FString Schema;
	if (!RootObject.TryGetStringField(TEXT("schema"), Schema))
	{
		AddProfileDiagnostic(Result, EScenarioCompileDiagnosticSeverity::Error, TEXT("missing_schema"), TEXT("simulation_profile.schema is required."));
	}
	else if (Schema != SimulationProfileSchemaName)
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("unsupported_schema"),
			FString::Printf(TEXT("Expected schema '%s' but found '%s'."), SimulationProfileSchemaName, *Schema));
	}

	double VersionValue = 0.0;
	if (!RootObject.TryGetNumberField(TEXT("version"), VersionValue))
	{
		AddProfileDiagnostic(Result, EScenarioCompileDiagnosticSeverity::Error, TEXT("missing_schema_version"), TEXT("simulation_profile.version is required."));
	}
	else if (!FMath::IsNearlyEqual(VersionValue, static_cast<double>(FScenarioSimulationProfileAdapter::SupportedVersion)))
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("unsupported_schema_version"),
			FString::Printf(
				TEXT("simulation_profile version %.0f is not supported by this adapter. Supported version: %d."),
				VersionValue,
				FScenarioSimulationProfileAdapter::SupportedVersion));
	}

	RootObject.TryGetStringField(TEXT("profile_id"), Result.ProfileId);
}
}

bool FScenarioSimulationProfileAdapter::IsSimulationProfileFile(const FString& JsonFilePath)
{
	FString Schema;
	return TryReadSimulationProfileSchema(JsonFilePath, Schema) && Schema == SimulationProfileSchemaName;
}

FString FScenarioSimulationProfileAdapter::MakeProfileFileHash(const FString& JsonFilePath)
{
	const FString ResolvedPath = ResolveSimulationProfilePath(JsonFilePath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
	{
		return SimulationProfileUnsetHash;
	}

	return FString::Printf(TEXT("hash:%u"), GetTypeHash(JsonText));
}

FScenarioSimulationProfileCompileResult FScenarioSimulationProfileAdapter::CompileProfileFromJsonFile(const FString& JsonFilePath)
{
	FScenarioSimulationProfileCompileResult Result;
	const FString ResolvedPath = ResolveSimulationProfilePath(JsonFilePath);

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ResolvedPath))
	{
		AddProfileDiagnostic(
			Result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("file_read_failed"),
			FString::Printf(TEXT("simulation_profile JSON file '%s' could not be read. ResolvedPath: '%s'"), *JsonFilePath, *ResolvedPath));
		Result.bSuccess = false;
		Result.ProfileHash = SimulationProfileUnsetHash;
		return Result;
	}

	Result = CompileProfileFromJsonString(JsonString);
	return Result;
}

FScenarioSimulationProfileCompileResult FScenarioSimulationProfileAdapter::CompileProfileFromJsonString(const FString& JsonString)
{
	FScenarioSimulationProfileCompileResult Result;
	Result.ProfileHash = FString::Printf(TEXT("hash:%u"), GetTypeHash(JsonString));

	TSharedPtr<FJsonObject> RootObject;
	if (!TryReadProfileRootObject(JsonString, Result, RootObject) || !RootObject.IsValid())
	{
		Result.bSuccess = false;
		return Result;
	}

	ValidateProfileHeader(*RootObject, Result);
	CompileProfileDriveSpec(*RootObject, Result);
	CompileProfileLidarSpec(*RootObject, Result);

	Result.bSuccess = !HasProfileErrors(Result);
	return Result;
}
