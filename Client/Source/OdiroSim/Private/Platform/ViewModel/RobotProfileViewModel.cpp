#include "Platform/ViewModel/RobotProfileViewModel.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ProjectSessionSubsystem.h"

namespace
{
	const TCHAR* RobotProfileVmProfileFileName = TEXT("profile.json");

	// Normalizes user-project paths before resolving profile.json.
	FString NormalizeRobotProfileVmPath(FString path)
	{
		path = path.TrimStartAndEnd();
		if (path.IsEmpty())
		{
			return FString();
		}

		path = FPaths::IsRelative(path)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), path))
			: FPaths::ConvertRelativePathToFull(path);
		FPaths::NormalizeFilename(path);
		return path;
	}

	// Converts profile.json LiDAR mode aliases into UI combo-box option text.
	bool TryNormalizeRobotProfileLidarModeForUi(const FString& value, FString& outMode)
	{
		const FString normalized = value.TrimStartAndEnd()
			.ToLower()
			.Replace(TEXT("_"), TEXT(""))
			.Replace(TEXT("-"), TEXT(""))
			.Replace(TEXT("+"), TEXT("and"))
			.Replace(TEXT(" "), TEXT(""));

		if (normalized == TEXT("1d") || normalized == TEXT("oned"))
		{
			outMode = TEXT("1D");
			return true;
		}
		if (normalized == TEXT("2d") || normalized == TEXT("twod") || normalized == TEXT("front2d"))
		{
			outMode = TEXT("2D");
			return true;
		}
		if (normalized == TEXT("3d") || normalized == TEXT("threed"))
		{
			outMode = TEXT("3D");
			return true;
		}
		if (normalized == TEXT("1dand2d") || normalized == TEXT("onedandtwod"))
		{
			outMode = TEXT("1D+2D");
			return true;
		}
		if (normalized == TEXT("2dand3d") || normalized == TEXT("twodandthreed"))
		{
			outMode = TEXT("2D+3D");
			return true;
		}
		if (normalized == TEXT("all"))
		{
			outMode = TEXT("All");
			return true;
		}

		outMode.Reset();
		return false;
	}
}

void URobotProfileViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	LoadFromActiveProject();
}

void URobotProfileViewModel::SetSubsystemOverride(UProjectSessionSubsystem* projectSessionSubsystem)
{
	ProjectSessionOverride = projectSessionSubsystem;
}

bool URobotProfileViewModel::LoadFromActiveProject()
{
	return LoadFromProject(FString());
}

bool URobotProfileViewModel::LoadFromProject(const FString& projectPath)
{
	const FString resolvedProjectPath = ResolveProjectPath(projectPath);
	if (resolvedProjectPath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Active project is not set."));
		return false;
	}

	FRobotProfileSettings settings;
	FString error;
	if (!UPlatformUiSubsystem::LoadRobotProfileForProject(resolvedProjectPath, settings, error))
	{
		SetDiagnosticsText(error);
		UE_MVVM_SET_PROPERTY_VALUE(ProfilePath, BuildProfilePath(resolvedProjectPath));
		return false;
	}

	ApplySettings(settings);
	UE_MVVM_SET_PROPERTY_VALUE(ProfilePath, BuildProfilePath(resolvedProjectPath));
	ClearDiagnostics();
	return true;
}

bool URobotProfileViewModel::SaveRobotProfile()
{
	return SaveToProject(FString());
}

bool URobotProfileViewModel::SaveToProject(const FString& projectPath)
{
	TArray<FString> diagnostics;
	if (!ValidateInputs(diagnostics))
	{
		SetDiagnosticsText(FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	const FString resolvedProjectPath = ResolveProjectPath(projectPath);
	if (resolvedProjectPath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Active project is not set."));
		return false;
	}

	FString statusText;
	const bool bSaved = UPlatformUiSubsystem::SaveRobotProfileForProject(
		resolvedProjectPath,
		MakeSettings(),
		statusText);
	SetDiagnosticsText(statusText);
	if (bSaved)
	{
		UE_MVVM_SET_PROPERTY_VALUE(ProfilePath, BuildProfilePath(resolvedProjectPath));
	}
	return bSaved;
}

void URobotProfileViewModel::SetBodyLengthM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(BodyLengthM, value);
}

void URobotProfileViewModel::SetBodyWidthM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(BodyWidthM, value);
}

void URobotProfileViewModel::SetBodyHeightM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(BodyHeightM, value);
}

void URobotProfileViewModel::SetBodyWheelBaseM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(BodyWheelBaseM, value);
}

void URobotProfileViewModel::SetBodyTurningRadiusM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(BodyTurningRadiusM, value);
}

void URobotProfileViewModel::SetDriveMaxSpeedKmh(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DriveMaxSpeedKmh, value);
}

void URobotProfileViewModel::SetDriveMaxReverseSpeedKmh(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DriveMaxReverseSpeedKmh, value);
}

void URobotProfileViewModel::SetDriveAccelerationRateKmhPerSecond(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DriveAccelerationRateKmhPerSecond, value);
}

void URobotProfileViewModel::SetDriveDecelerationRateKmhPerSecond(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DriveDecelerationRateKmhPerSecond, value);
}

void URobotProfileViewModel::SetDriveSteeringRatePerS(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DriveSteeringRatePerS, value);
}

void URobotProfileViewModel::SetDriveMassKg(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DriveMassKg, value);
}

void URobotProfileViewModel::SetLidarMode(const FString& value)
{
	FString normalizedMode;
	UE_MVVM_SET_PROPERTY_VALUE(
		LidarMode,
		TryNormalizeRobotProfileLidarModeForUi(value, normalizedMode) ? normalizedMode : value.TrimStartAndEnd());
}

void URobotProfileViewModel::SetLidarDrawDebug(const bool bValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bLidarDrawDebug, bValue);
}

void URobotProfileViewModel::SetLidarScanRangeM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarScanRangeM, value);
}

void URobotProfileViewModel::SetLidarSensorHeightM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarSensorHeightM, value);
}

void URobotProfileViewModel::SetLidarFrontHalfAngleDegree(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarFrontHalfAngleDegree, value);
}

void URobotProfileViewModel::SetLidarStopDistanceM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarStopDistanceM, value);
}

void URobotProfileViewModel::SetLidarSlowDownDistanceM(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarSlowDownDistanceM, value);
}

void URobotProfileViewModel::SetLidarAngleStepDegree(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarAngleStepDegree, value);
}

void URobotProfileViewModel::SetLidarScanRateHz(const float value)
{
	UE_MVVM_SET_PROPERTY_VALUE(LidarScanRateHz, value);
}

bool URobotProfileViewModel::CanSaveRobotProfile() const
{
	TArray<FString> diagnostics;
	return ValidateInputs(diagnostics);
}

UProjectSessionSubsystem* URobotProfileViewModel::ResolveProjectSessionSubsystem() const
{
	if (ProjectSessionOverride)
	{
		return ProjectSessionOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

bool URobotProfileViewModel::ValidateInputs(TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	if (BodyLengthM < 0.01f)
	{
		outDiagnostics.Add(TEXT("Body Length must be at least 0.01 m."));
	}
	if (BodyWidthM < 0.01f)
	{
		outDiagnostics.Add(TEXT("Body Width must be at least 0.01 m."));
	}
	if (BodyHeightM < 0.01f)
	{
		outDiagnostics.Add(TEXT("Body Height must be at least 0.01 m."));
	}
	if (BodyWheelBaseM < 0.0f)
	{
		outDiagnostics.Add(TEXT("Wheel Base must be 0 m or greater."));
	}
	if (BodyTurningRadiusM < 0.0f)
	{
		outDiagnostics.Add(TEXT("Turning Radius must be 0 m or greater."));
	}
	if (DriveMaxSpeedKmh < 0.0f)
	{
		outDiagnostics.Add(TEXT("Max Speed must be 0 km/h or greater."));
	}
	if (DriveMaxReverseSpeedKmh < 0.0f)
	{
		outDiagnostics.Add(TEXT("Max Reverse Speed must be 0 km/h or greater."));
	}
	if (DriveAccelerationRateKmhPerSecond < 0.0f)
	{
		outDiagnostics.Add(TEXT("Acceleration Rate must be 0 km/h/s or greater."));
	}
	if (DriveDecelerationRateKmhPerSecond < 0.0f)
	{
		outDiagnostics.Add(TEXT("Deceleration Rate must be 0 km/h/s or greater."));
	}
	if (DriveSteeringRatePerS < 0.0f)
	{
		outDiagnostics.Add(TEXT("Steering Rate must be 0 or greater."));
	}
	if (DriveMassKg < 0.01f)
	{
		outDiagnostics.Add(TEXT("Mass must be at least 0.01 kg."));
	}
	FString normalizedLidarMode;
	if (!TryNormalizeRobotProfileLidarModeForUi(LidarMode, normalizedLidarMode))
	{
		outDiagnostics.Add(TEXT("LiDAR Mode must be 1D, 2D, or 3D."));
	}
	if (LidarScanRangeM < 0.0f)
	{
		outDiagnostics.Add(TEXT("LiDAR Scan Range must be 0 m or greater."));
	}
	if (LidarSensorHeightM < 0.0f)
	{
		outDiagnostics.Add(TEXT("LiDAR Sensor Height must be 0 m or greater."));
	}
	if (LidarFrontHalfAngleDegree < 0.0f || LidarFrontHalfAngleDegree > 180.0f)
	{
		outDiagnostics.Add(TEXT("LiDAR Front Angle must be between 0 and 180 degrees."));
	}
	if (LidarStopDistanceM < 0.0f)
	{
		outDiagnostics.Add(TEXT("LiDAR Stop Distance must be 0 m or greater."));
	}
	if (LidarSlowDownDistanceM < 0.0f)
	{
		outDiagnostics.Add(TEXT("LiDAR Slowdown Distance must be 0 m or greater."));
	}
	if (LidarAngleStepDegree < 1.0f)
	{
		outDiagnostics.Add(TEXT("LiDAR Angle Step must be at least 1 degree."));
	}
	if (LidarScanRateHz < 0.1f)
	{
		outDiagnostics.Add(TEXT("LiDAR Scan Rate must be at least 0.1 Hz."));
	}
	return outDiagnostics.IsEmpty();
}

FString URobotProfileViewModel::ResolveProjectPath(const FString& projectPath) const
{
	const FString normalizedProjectPath = NormalizeRobotProfileVmPath(projectPath);
	if (!normalizedProjectPath.IsEmpty())
	{
		return normalizedProjectPath;
	}

	UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject()
		? NormalizeRobotProfileVmPath(projectSession->GetActiveProjectPath())
		: FString();
}

FString URobotProfileViewModel::BuildProfilePath(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeRobotProfileVmPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		return FString();
	}

	FString profilePath = FPaths::Combine(normalizedProjectPath, RobotProfileVmProfileFileName);
	FPaths::NormalizeFilename(profilePath);
	return profilePath;
}

FRobotProfileSettings URobotProfileViewModel::MakeSettings() const
{
	FRobotProfileSettings settings;
	settings.Body.LengthM = BodyLengthM;
	settings.Body.WidthM = BodyWidthM;
	settings.Body.HeightM = BodyHeightM;
	settings.Body.WheelBaseM = BodyWheelBaseM;
	settings.Body.TurningRadiusM = BodyTurningRadiusM;
	settings.Drive.MaxSpeedKmh = DriveMaxSpeedKmh;
	settings.Drive.MaxReverseSpeedKmh = DriveMaxReverseSpeedKmh;
	settings.Drive.AccelerationRateKmhPerSecond = DriveAccelerationRateKmhPerSecond;
	settings.Drive.DecelerationRateKmhPerSecond = DriveDecelerationRateKmhPerSecond;
	settings.Drive.SteeringRatePerS = DriveSteeringRatePerS;
	settings.Drive.MassKg = DriveMassKg;
	settings.Lidar.LidarMode = LidarMode;
	settings.Lidar.bDrawDebug = bLidarDrawDebug;
	settings.Lidar.ScanRangeM = LidarScanRangeM;
	settings.Lidar.SensorHeightM = LidarSensorHeightM;
	settings.Lidar.FrontHalfAngleDegree = LidarFrontHalfAngleDegree;
	settings.Lidar.StopDistanceM = LidarStopDistanceM;
	settings.Lidar.SlowDownDistanceM = LidarSlowDownDistanceM;
	settings.Lidar.AngleStepDegree = LidarAngleStepDegree;
	settings.Lidar.ScanRateHz = LidarScanRateHz;
	return settings;
}

void URobotProfileViewModel::ApplySettings(const FRobotProfileSettings& settings)
{
	SetBodyLengthM(settings.Body.LengthM);
	SetBodyWidthM(settings.Body.WidthM);
	SetBodyHeightM(settings.Body.HeightM);
	SetBodyWheelBaseM(settings.Body.WheelBaseM);
	SetBodyTurningRadiusM(settings.Body.TurningRadiusM);
	SetDriveMaxSpeedKmh(settings.Drive.MaxSpeedKmh);
	SetDriveMaxReverseSpeedKmh(settings.Drive.MaxReverseSpeedKmh);
	SetDriveAccelerationRateKmhPerSecond(settings.Drive.AccelerationRateKmhPerSecond);
	SetDriveDecelerationRateKmhPerSecond(settings.Drive.DecelerationRateKmhPerSecond);
	SetDriveSteeringRatePerS(settings.Drive.SteeringRatePerS);
	SetDriveMassKg(settings.Drive.MassKg);
	SetLidarMode(settings.Lidar.LidarMode);
	SetLidarDrawDebug(settings.Lidar.bDrawDebug);
	SetLidarScanRangeM(settings.Lidar.ScanRangeM);
	SetLidarSensorHeightM(settings.Lidar.SensorHeightM);
	SetLidarFrontHalfAngleDegree(settings.Lidar.FrontHalfAngleDegree);
	SetLidarStopDistanceM(settings.Lidar.StopDistanceM);
	SetLidarSlowDownDistanceM(settings.Lidar.SlowDownDistanceM);
	SetLidarAngleStepDegree(settings.Lidar.AngleStepDegree);
	SetLidarScanRateHz(settings.Lidar.ScanRateHz);
}
