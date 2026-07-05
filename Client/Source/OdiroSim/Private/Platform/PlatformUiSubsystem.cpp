#include "Platform/PlatformUiSubsystem.h"

#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/ViewModel/ExperimentConfigViewModel.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* PlatformUiExperimentSettingFileName = TEXT("setting.json");
	const TCHAR* PlatformUiRobotProfileFileName = TEXT("profile.json");
	const TCHAR* PlatformUiScenarioFileName = TEXT("scenario.json");
	const TCHAR* PlatformUiRunStatusFileName = TEXT("status.json");
	const TCHAR* PlatformUiRunSnapshotDirectoryName = TEXT("snapshot");
	const TCHAR* PlatformUiRunStatusSchema = TEXT("run_status");
	const TCHAR* PlatformUiRunStatusPending = TEXT("pending");
	const TCHAR* PlatformUiRunStatusStarting = TEXT("starting");
	const TCHAR* PlatformUiRunStatusRunning = TEXT("running");
	const TCHAR* PlatformUiRunStatusStopping = TEXT("stopping");
	const TCHAR* PlatformUiRunStatusCanceled = TEXT("canceled");
	const TCHAR* PlatformUiRunStatusCancelled = TEXT("cancelled");
	const TCHAR* PlatformUiRunStatusExited = TEXT("exited");
	const TCHAR* PlatformUiRunStatusCompleted = TEXT("completed");
	const TCHAR* PlatformUiRunStatusFailed = TEXT("failed");
	const TCHAR* PlatformUiExperimentDefaultMapId = TEXT("ScenarioSimulationMap");

	// Bridge status.json에서 읽은 process lifecycle 상태.
	struct FPlatformUiRunStatusSnapshot
	{
		bool bStatusFileExists = false;
		bool bValidStatus = false;
		bool bStatusError = false;
		FString RawState;
		ESimulationRunState RunState = ESimulationRunState::Pending;
		EPlatformProjectRunProgressKind ProgressKind = EPlatformProjectRunProgressKind::Pending;
	};

	// user project path 입력을 absolute normalized path로 맞춘다.
	FString NormalizePlatformUiProjectPath(FString path)
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

	// 경로 비교 전 trailing slash 차이를 제거한다.
	FString NormalizePlatformUiPathForCompare(const FString& path)
	{
		FString normalizedPath = NormalizePlatformUiProjectPath(path);
		while (normalizedPath.Len() > 1 && normalizedPath.EndsWith(TEXT("/")))
		{
			normalizedPath.LeftChopInline(1);
		}
		return normalizedPath;
	}

	// candidate가 target과 같거나 target의 parent 경로인지 확인한다.
	bool IsPlatformUiSameOrParentPath(const FString& candidate, const FString& target)
	{
		FString normalizedCandidate = NormalizePlatformUiPathForCompare(candidate);
		FString normalizedTarget = NormalizePlatformUiPathForCompare(target);
		if (normalizedCandidate.IsEmpty() || normalizedTarget.IsEmpty())
		{
			return false;
		}
		if (normalizedCandidate.Equals(normalizedTarget, ESearchCase::IgnoreCase))
		{
			return true;
		}
		normalizedCandidate += TEXT("/");
		return normalizedTarget.StartsWith(normalizedCandidate, ESearchCase::IgnoreCase);
	}

	// Drive, Unix root, UNC share root처럼 폴더 삭제 대상으로 너무 넓은 경로를 막는다.
	bool IsPlatformUiRootLikePath(const FString& normalizedProjectPath)
	{
		if (normalizedProjectPath.IsEmpty() || normalizedProjectPath.Equals(TEXT("/")))
		{
			return true;
		}
		if (FPaths::IsDrive(normalizedProjectPath))
		{
			return true;
		}

		const TCHAR driveSeparator = TEXT(":")[0];
		const TCHAR pathSeparator = TEXT("/")[0];
		if (normalizedProjectPath.Len() >= 2
			&& FChar::IsAlpha(normalizedProjectPath[0])
			&& normalizedProjectPath[1] == driveSeparator)
		{
			return normalizedProjectPath.Len() == 2
				|| (normalizedProjectPath.Len() == 3 && normalizedProjectPath[2] == pathSeparator);
		}

		if (normalizedProjectPath.StartsWith(TEXT("//")))
		{
			TArray<FString> pathParts;
			normalizedProjectPath.RightChop(2).ParseIntoArray(pathParts, TEXT("/"), true);
			return pathParts.Num() <= 2;
		}

		return false;
	}

	// OS root, workspace root, project root, engine root 같은 위험한 경로를 막는다.
	bool IsUnsafePlatformUiDeleteRoot(const FString& projectPath)
	{
		const FString normalizedProjectPath = NormalizePlatformUiPathForCompare(projectPath);
		if (IsPlatformUiRootLikePath(normalizedProjectPath))
		{
			return true;
		}

		TArray<FString> protectedRoots;
		protectedRoots.Add(FPaths::ProjectDir());
		protectedRoots.Add(FPaths::RootDir());
		protectedRoots.Add(FPaths::EngineDir());
		protectedRoots.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."))));

		for (const FString& protectedRoot : protectedRoots)
		{
			if (IsPlatformUiSameOrParentPath(normalizedProjectPath, protectedRoot))
			{
				return true;
			}
		}

		return false;
	}

	// Core user-project files가 모두 있는 폴더만 실제 삭제 대상으로 인정한다.
	bool HasPlatformUiUserProjectCoreFiles(const FString& projectPath)
	{
		const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
		return FPaths::FileExists(FPaths::Combine(normalizedProjectPath, PlatformUiExperimentSettingFileName))
			&& FPaths::FileExists(FPaths::Combine(normalizedProjectPath, PlatformUiRobotProfileFileName))
			&& FPaths::FileExists(FPaths::Combine(normalizedProjectPath, PlatformUiScenarioFileName));
	}

	// Platform UI가 읽는 JSON 파일 root object를 best-effort로 읽는다.
	bool TryReadPlatformUiJsonObject(const FString& jsonPath, TSharedPtr<FJsonObject>& outRootObject)
	{
		outRootObject.Reset();

		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *jsonPath))
		{
			return false;
		}

		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
		return FJsonSerializer::Deserialize(reader, outRootObject) && outRootObject.IsValid();
	}

	// Bridge raw status 값을 Platform run/progress 상태로 변환한다.
	bool TryMapPlatformUiRunStatusState(
		const FString& rawState,
		ESimulationRunState& outRunState,
		EPlatformProjectRunProgressKind& outProgressKind)
	{
		if (rawState == PlatformUiRunStatusPending)
		{
			outRunState = ESimulationRunState::Pending;
			outProgressKind = EPlatformProjectRunProgressKind::Pending;
			return true;
		}
		if (rawState == PlatformUiRunStatusStarting)
		{
			outRunState = ESimulationRunState::Running;
			outProgressKind = EPlatformProjectRunProgressKind::Starting;
			return true;
		}
		if (rawState == PlatformUiRunStatusRunning)
		{
			outRunState = ESimulationRunState::Running;
			outProgressKind = EPlatformProjectRunProgressKind::Running;
			return true;
		}
		if (rawState == PlatformUiRunStatusStopping
			|| rawState == PlatformUiRunStatusCanceled
			|| rawState == PlatformUiRunStatusCancelled)
		{
			outRunState = ESimulationRunState::Canceled;
			outProgressKind = EPlatformProjectRunProgressKind::Canceled;
			return true;
		}
		if (rawState == PlatformUiRunStatusExited || rawState == PlatformUiRunStatusCompleted)
		{
			outRunState = ESimulationRunState::Completed;
			outProgressKind = EPlatformProjectRunProgressKind::Completed;
			return true;
		}
		if (rawState == PlatformUiRunStatusFailed)
		{
			outRunState = ESimulationRunState::Failed;
			outProgressKind = EPlatformProjectRunProgressKind::Failed;
			return true;
		}
		return false;
	}

	// Bridge status.json을 raw/process state snapshot으로 읽는다.
	FPlatformUiRunStatusSnapshot ReadPlatformUiRunStatusSnapshot(const FString& statusPath)
	{
		FPlatformUiRunStatusSnapshot snapshot;
		if (!FPaths::FileExists(statusPath))
		{
			return snapshot;
		}

		snapshot.bStatusFileExists = true;
		TSharedPtr<FJsonObject> rootObject;
		if (!TryReadPlatformUiJsonObject(statusPath, rootObject))
		{
			snapshot.bStatusError = true;
			snapshot.RunState = ESimulationRunState::Failed;
			snapshot.ProgressKind = EPlatformProjectRunProgressKind::Error;
			return snapshot;
		}

		FString schema;
		FString stateText;
		if (!rootObject->TryGetStringField(TEXT("schema"), schema)
			|| !schema.Equals(PlatformUiRunStatusSchema, ESearchCase::CaseSensitive)
			|| !rootObject->TryGetStringField(TEXT("state"), stateText))
		{
			snapshot.bStatusError = true;
			snapshot.RunState = ESimulationRunState::Failed;
			snapshot.ProgressKind = EPlatformProjectRunProgressKind::Error;
			return snapshot;
		}

		snapshot.RawState = stateText.TrimStartAndEnd().ToLower();
		if (!TryMapPlatformUiRunStatusState(snapshot.RawState, snapshot.RunState, snapshot.ProgressKind))
		{
			snapshot.bStatusError = true;
			snapshot.RunState = ESimulationRunState::Failed;
			snapshot.ProgressKind = EPlatformProjectRunProgressKind::Error;
			return snapshot;
		}

		snapshot.bValidStatus = true;
		return snapshot;
	}

	// Run snapshot setting.json에서 episode_count를 읽는다.
	int32 ReadPlatformUiRunSnapshotEpisodeCount(const FString& runDirectory)
	{
		const FString settingPath = NormalizePlatformUiProjectPath(FPaths::Combine(
			NormalizePlatformUiProjectPath(runDirectory),
			PlatformUiRunSnapshotDirectoryName,
			PlatformUiExperimentSettingFileName));
		if (!FPaths::FileExists(settingPath))
		{
			return 0;
		}

		TSharedPtr<FJsonObject> rootObject;
		if (!TryReadPlatformUiJsonObject(settingPath, rootObject))
		{
			return 0;
		}

		const TSharedPtr<FJsonObject>* samplingObject = nullptr;
		double episodeCount = 0.0;
		if (!rootObject->TryGetObjectField(TEXT("sampling"), samplingObject)
			|| !samplingObject
			|| !samplingObject->IsValid()
			|| !(*samplingObject)->TryGetNumberField(TEXT("episode_count"), episodeCount))
		{
			return 0;
		}

		return episodeCount > 0.0 ? FMath::Max(1, FMath::RoundToInt(episodeCount)) : 0;
	}

	// Completed/total count를 0-100 percent로 변환한다.
	float CalculatePlatformUiRunProgressPercent(const int32 completedCount, const int32 totalCount)
	{
		if (totalCount <= 0)
		{
			return 0.0f;
		}

		return FMath::Clamp(
			100.0f * static_cast<float>(FMath::Max(0, completedCount)) / static_cast<float>(totalCount),
			0.0f,
			100.0f);
	}

	// Platform UI가 편집하는 JSON 파일 root object를 읽는다.
	bool LoadPlatformUiJsonRoot(
		const FString& path,
		TSharedPtr<FJsonObject>& outRootObject,
		FString& outError)
	{
		outRootObject.Reset();
		outError.Reset();

		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *path))
		{
			outError = FString::Printf(TEXT("setting.json 읽기 실패: %s"), *path);
			return false;
		}

		const TSharedRef<TJsonReader<TCHAR>> reader = TJsonReaderFactory<TCHAR>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, outRootObject) || !outRootObject.IsValid())
		{
			outError = FString::Printf(TEXT("setting.json 파싱 실패: %s"), *path);
			return false;
		}

		return true;
	}

	// JSON object field를 찾아 반환하거나 저장을 위해 새 object field를 만든다.
	TSharedRef<FJsonObject> FindOrCreatePlatformUiObjectField(
		const TSharedRef<FJsonObject>& rootObject,
		const TCHAR* fieldName)
	{
		const TSharedPtr<FJsonObject>* existingObject = nullptr;
		if (rootObject->TryGetObjectField(fieldName, existingObject) && existingObject && existingObject->IsValid())
		{
			return existingObject->ToSharedRef();
		}

		TSharedRef<FJsonObject> newObject = MakeShared<FJsonObject>();
		rootObject->SetObjectField(fieldName, newObject);
		return newObject;
	}

	// user project root에서 experiment setting.json 경로를 만든다.
	FString BuildPlatformUiExperimentSettingPath(const FString& projectPath)
	{
		return NormalizePlatformUiProjectPath(FPaths::Combine(projectPath, PlatformUiExperimentSettingFileName));
	}

	// Builds the user-project profile.json path.
	FString BuildPlatformUiRobotProfilePath(const FString& projectPath)
	{
		return NormalizePlatformUiProjectPath(FPaths::Combine(projectPath, PlatformUiRobotProfileFileName));
	}

	// Validates robot.body values before writing profile.json.
	bool ValidatePlatformUiRobotProfileBodySettings(
		const FRobotProfileBodySettings& settings,
		FString& outErrorText)
	{
		TArray<FString> diagnostics;
		if (settings.LengthM < 0.01f)
		{
			diagnostics.Add(TEXT("Body Length must be at least 0.01 m."));
		}
		if (settings.WidthM < 0.01f)
		{
			diagnostics.Add(TEXT("Body Width must be at least 0.01 m."));
		}
		if (settings.HeightM < 0.01f)
		{
			diagnostics.Add(TEXT("Body Height must be at least 0.01 m."));
		}
		if (settings.WheelBaseM < 0.0f)
		{
			diagnostics.Add(TEXT("Wheel Base must be 0 m or greater."));
		}
		if (settings.TurningRadiusM < 0.0f)
		{
			diagnostics.Add(TEXT("Turning Radius must be 0 m or greater."));
		}

		outErrorText = FString::Join(diagnostics, TEXT("\n"));
		return diagnostics.IsEmpty();
	}

	// Validates robot.drive values before writing profile.json.
	bool ValidatePlatformUiRobotProfileDriveSettings(
		const FRobotProfileDriveSettings& settings,
		FString& outErrorText)
	{
		TArray<FString> diagnostics;
		if (settings.MaxSpeedKmh < 0.0f)
		{
			diagnostics.Add(TEXT("Max Speed must be 0 km/h or greater."));
		}
		if (settings.MaxReverseSpeedKmh < 0.0f)
		{
			diagnostics.Add(TEXT("Max Reverse Speed must be 0 km/h or greater."));
		}
		if (settings.AccelerationRateKmhPerSecond < 0.0f)
		{
			diagnostics.Add(TEXT("Acceleration Rate must be 0 km/h/s or greater."));
		}
		if (settings.DecelerationRateKmhPerSecond < 0.0f)
		{
			diagnostics.Add(TEXT("Deceleration Rate must be 0 km/h/s or greater."));
		}
		if (settings.SteeringRatePerS < 0.0f)
		{
			diagnostics.Add(TEXT("Steering Rate must be 0 or greater."));
		}
		if (settings.MassKg < 0.01f)
		{
			diagnostics.Add(TEXT("Mass must be at least 0.01 kg."));
		}

		outErrorText = FString::Join(diagnostics, TEXT("\n"));
		return diagnostics.IsEmpty();
	}

	// Validates robot.lidar values before writing profile.json.
	bool ValidatePlatformUiRobotProfileLidarSettings(
		const FRobotProfileLidarSettings& settings,
		FString& outErrorText)
	{
		TArray<FString> diagnostics;
		const FString normalizedMode = settings.LidarMode.TrimStartAndEnd()
			.ToLower()
			.Replace(TEXT("_"), TEXT(""))
			.Replace(TEXT("-"), TEXT(""))
			.Replace(TEXT("+"), TEXT("and"))
			.Replace(TEXT(" "), TEXT(""));
		if (normalizedMode != TEXT("1d")
			&& normalizedMode != TEXT("oned")
			&& normalizedMode != TEXT("2d")
			&& normalizedMode != TEXT("twod")
			&& normalizedMode != TEXT("front2d")
			&& normalizedMode != TEXT("3d")
			&& normalizedMode != TEXT("threed")
			&& normalizedMode != TEXT("ousteros1")
			&& normalizedMode != TEXT("os1")
			&& normalizedMode != TEXT("1dand2d")
			&& normalizedMode != TEXT("onedandtwod")
			&& normalizedMode != TEXT("2dand3d")
			&& normalizedMode != TEXT("twodandthreed")
			&& normalizedMode != TEXT("all"))
		{
			diagnostics.Add(TEXT("LiDAR Mode must be 1D, 2D, 3D, or Ouster OS1."));
		}
		if (settings.ScanRangeM < 0.0f)
		{
			diagnostics.Add(TEXT("LiDAR Scan Range must be 0 m or greater."));
		}
		if (settings.SensorHeightM < 0.0f)
		{
			diagnostics.Add(TEXT("LiDAR Sensor Height must be 0 m or greater."));
		}
		if (FMath::Abs(settings.SensorForwardOffsetM) > 10.0f)
		{
			diagnostics.Add(TEXT("LiDAR Sensor Forward Offset must be between -10 m and 10 m."));
		}
		if (FMath::Abs(settings.SensorRightOffsetM) > 10.0f)
		{
			diagnostics.Add(TEXT("LiDAR Sensor Right Offset must be between -10 m and 10 m."));
		}
		if (settings.FrontHalfAngleDegree < 0.0f || settings.FrontHalfAngleDegree > 180.0f)
		{
			diagnostics.Add(TEXT("LiDAR Front Angle must be between 0 and 180 degrees."));
		}
		if (settings.StopDistanceM < 0.0f)
		{
			diagnostics.Add(TEXT("LiDAR Stop Distance must be 0 m or greater."));
		}
		if (settings.ObstacleWarningDistanceM < 0.0f)
		{
			diagnostics.Add(TEXT("LiDAR Obstacle Warning Distance must be 0 m or greater."));
		}
		if (settings.SlowDownDistanceM < 0.0f)
		{
			diagnostics.Add(TEXT("LiDAR Slowdown Distance must be 0 m or greater."));
		}
		if (settings.AngleStepDegree < 1.0f)
		{
			diagnostics.Add(TEXT("LiDAR Angle Step must be at least 1 degree."));
		}
		if (settings.VerticalMinDegree < -89.0f || settings.VerticalMinDegree > 89.0f)
		{
			diagnostics.Add(TEXT("LiDAR Vertical Min must be between -89 and 89 degrees."));
		}
		if (settings.VerticalMaxDegree < -89.0f || settings.VerticalMaxDegree > 89.0f)
		{
			diagnostics.Add(TEXT("LiDAR Vertical Max must be between -89 and 89 degrees."));
		}
		if (settings.VerticalMinDegree > settings.VerticalMaxDegree)
		{
			diagnostics.Add(TEXT("LiDAR Vertical Min must be less than or equal to Vertical Max."));
		}
		if (settings.VerticalStepDegree < 1.0f)
		{
			diagnostics.Add(TEXT("LiDAR Vertical Step must be at least 1 degree."));
		}
		if (settings.ScanRateHz < 0.1f)
		{
			diagnostics.Add(TEXT("LiDAR Scan Rate must be at least 0.1 Hz."));
		}

		outErrorText = FString::Join(diagnostics, TEXT("\n"));
		return diagnostics.IsEmpty();
	}

	// Validates exposed robot profile values before writing profile.json.
	bool ValidatePlatformUiRobotProfileSettings(
		const FRobotProfileSettings& settings,
		FString& outErrorText)
	{
		TArray<FString> diagnostics;
		FString bodyError;
		if (!ValidatePlatformUiRobotProfileBodySettings(settings.Body, bodyError) && !bodyError.IsEmpty())
		{
			diagnostics.Add(bodyError);
		}

		FString driveError;
		if (!ValidatePlatformUiRobotProfileDriveSettings(settings.Drive, driveError) && !driveError.IsEmpty())
		{
			diagnostics.Add(driveError);
		}

		FString lidarError;
		if (!ValidatePlatformUiRobotProfileLidarSettings(settings.Lidar, lidarError) && !lidarError.IsEmpty())
		{
			diagnostics.Add(lidarError);
		}

		outErrorText = FString::Join(diagnostics, TEXT("\n"));
		return diagnostics.IsEmpty();
	}

	// Applies a JSON number field to the target value when present.
	void ReadOptionalRobotProfileNumberField(
		const FJsonObject& object,
		const TCHAR* fieldName,
		float& targetValue)
	{
		double jsonValue = 0.0;
		if (object.TryGetNumberField(fieldName, jsonValue))
		{
			targetValue = static_cast<float>(jsonValue);
		}
	}

	// Applies a JSON string field to the target value when present.
	void ReadOptionalRobotProfileStringField(
		const FJsonObject& object,
		const TCHAR* fieldName,
		FString& targetValue)
	{
		FString jsonValue;
		if (object.TryGetStringField(fieldName, jsonValue))
		{
			targetValue = jsonValue;
		}
	}

	// Applies a JSON bool field to the target value when present.
	void ReadOptionalRobotProfileBoolField(
		const FJsonObject& object,
		const TCHAR* fieldName,
		bool& targetValue)
	{
		bool bJsonValue = false;
		if (object.TryGetBoolField(fieldName, bJsonValue))
		{
			targetValue = bJsonValue;
		}
	}

	// Writes LiDAR mode using the profile.json spelling consumed by runtime setup compilation.
	FString NormalizePlatformUiRobotProfileLidarModeForSave(const FString& value)
	{
		const FString normalized = value.TrimStartAndEnd()
			.ToLower()
			.Replace(TEXT("_"), TEXT(""))
			.Replace(TEXT("-"), TEXT(""))
			.Replace(TEXT("+"), TEXT("and"))
			.Replace(TEXT(" "), TEXT(""));

		if (normalized == TEXT("1d") || normalized == TEXT("oned"))
		{
			return TEXT("1d");
		}
		if (normalized == TEXT("2d") || normalized == TEXT("twod") || normalized == TEXT("front2d"))
		{
			return TEXT("2d");
		}
		if (normalized == TEXT("3d") || normalized == TEXT("threed"))
		{
			return TEXT("3d");
		}
		if (normalized == TEXT("ousteros1") || normalized == TEXT("os1"))
		{
			return TEXT("ouster_os1");
		}
		if (normalized == TEXT("1dand2d") || normalized == TEXT("onedandtwod"))
		{
			return TEXT("1d_and_2d");
		}
		if (normalized == TEXT("2dand3d") || normalized == TEXT("twodandthreed"))
		{
			return TEXT("2d_and_3d");
		}
		if (normalized == TEXT("all"))
		{
			return TEXT("all");
		}

		return value.TrimStartAndEnd();
	}

	// Converts DeliveryBot compiler diagnostics into a profile editor status message.
	FString FormatDeliveryBotSetupDiagnostics(const FDeliveryBotSetupCompileResult& compileResult)
	{
		TArray<FString> lines;
		for (const FScenarioCompileDiagnostic& diagnostic : compileResult.Diagnostics)
		{
			if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
			{
				lines.Add(diagnostic.Message.IsEmpty() ? diagnostic.Code : diagnostic.Message);
			}
		}

		return lines.IsEmpty()
			? TEXT("profile.json validation failed.")
			: FString::Join(lines, TEXT("\n"));
	}

	// Validates experiment setting values before writing setting.json.
	bool ValidatePlatformUiExperimentSettings(
		const FExperimentConfigSettings& settings,
		FString& outErrorText)
	{
		TArray<FString> diagnostics;
		if (settings.MapId.TrimStartAndEnd().IsEmpty())
		{
			diagnostics.Add(TEXT("Map ID를 입력하세요."));
		}
		if (settings.FixedFps <= 0)
		{
			diagnostics.Add(TEXT("Fixed FPS는 1 이상이어야 합니다."));
		}
		if (settings.TimeScale <= 0.0f)
		{
			diagnostics.Add(TEXT("Time Scale은 0보다 커야 합니다."));
		}
		if (settings.MaxDurationSeconds < 0.0f)
		{
			diagnostics.Add(TEXT("Max Duration은 0 이상이어야 합니다."));
		}
		if (settings.EpisodeCount <= 0)
		{
			diagnostics.Add(TEXT("Episode Count는 1 이상이어야 합니다."));
		}
		if (settings.TipOverAngleDegrees < 10.0f || settings.TipOverAngleDegrees > 120.0f)
		{
			diagnostics.Add(TEXT("Tip Over Angle은 10~120 범위여야 합니다."));
		}
		if (settings.NearMissDistanceMeters < 0.0f)
		{
			diagnostics.Add(TEXT("Near Miss Distance는 0 이상이어야 합니다."));
		}
		if (settings.GoalAcceptanceRadiusMeters <= 0.0f)
		{
			diagnostics.Add(TEXT("Goal Acceptance Radius는 0보다 커야 합니다."));
		}

		outErrorText = FString::Join(diagnostics, TEXT("\n"));
		return diagnostics.IsEmpty();
	}
}

void UPlatformUiSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	ProjectWorkspaceViewModel = NewObject<UProjectWorkspaceViewModel>(this);
	ExperimentConfigViewModel = NewObject<UExperimentConfigViewModel>(this);
	RobotProfileViewModel = NewObject<URobotProfileViewModel>(this);
	ExperimentResultViewModel = NewObject<UExperimentResultViewModel>(this);

	if (ProjectWorkspaceViewModel)
	{
		ProjectWorkspaceViewModel->InitializeForGameInstance(GetGameInstance());
	}
	if (ExperimentConfigViewModel)
	{
		ExperimentConfigViewModel->InitializeForGameInstance(GetGameInstance());
	}
	if (RobotProfileViewModel)
	{
		RobotProfileViewModel->InitializeForGameInstance(GetGameInstance());
	}
	if (ExperimentResultViewModel)
	{
		ExperimentResultViewModel->InitializeForGameInstance(GetGameInstance());
		ExperimentResultViewModel->SetSubsystemOverride(this);
	}

	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
		subsystem->OnRunInfoChanged.AddUObject(this, &UPlatformUiSubsystem::HandleRunInfoChanged);
	}
	if (UPlatformAnalysisAiSubsystem* analysisSubsystem = ResolvePlatformAnalysisAiSubsystem())
	{
		analysisSubsystem->OnAnalysisCompleted.RemoveAll(this);
		analysisSubsystem->OnAnalysisCompleted.AddUObject(this, &UPlatformUiSubsystem::HandleAnalysisCompleted);
	}
}

void UPlatformUiSubsystem::Deinitialize()
{
	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
	}
	if (UPlatformAnalysisAiSubsystem* analysisSubsystem = ResolvePlatformAnalysisAiSubsystem())
	{
		analysisSubsystem->OnAnalysisCompleted.RemoveAll(this);
	}

	ProjectWorkspaceViewModel = nullptr;
	ExperimentConfigViewModel = nullptr;
	RobotProfileViewModel = nullptr;
	ExperimentResultViewModel = nullptr;

	Super::Deinitialize();
}

UPlatformUiSubsystem* UPlatformUiSubsystem::ResolveForWorldContext(const UObject* worldContextObject)
{
	UWorld* world = worldContextObject ? worldContextObject->GetWorld() : nullptr;
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<UPlatformUiSubsystem>() : nullptr;
}

void UPlatformUiSubsystem::RefreshFromProjectSession()
{
	if (ProjectWorkspaceViewModel)
	{
		ProjectWorkspaceViewModel->RefreshFromProjectSession();
	}
	if (ExperimentConfigViewModel)
	{
		ExperimentConfigViewModel->LoadFromActiveProject();
	}
	if (RobotProfileViewModel)
	{
		RobotProfileViewModel->LoadFromActiveProject();
	}
}

FProjectPresetCatalog UPlatformUiSubsystem::ListProjectPresets() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListProjectPresets() : FProjectPresetCatalog();
}

bool UPlatformUiSubsystem::ValidateUserProject(const FString& projectPath, TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();

	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	return subsystem->ValidateUserProject(projectPath, outDiagnostics);
}

bool UPlatformUiSubsystem::HasActiveProject() const
{
	const UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject();
}

FString UPlatformUiSubsystem::GetActiveProjectPath() const
{
	const UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject()
		? projectSession->GetActiveProjectPath()
		: FString();
}

FString UPlatformUiSubsystem::GetActiveProjectScenarioPath() const
{
	const UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject()
		? projectSession->GetActiveProjectScenarioPath()
		: FString();
}

bool UPlatformUiSubsystem::ReturnToStartupMap(FString& outErrorText) const
{
	outErrorText.Reset();

	if (UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem())
	{
		projectSession->ClearActiveProject();
	}

	UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!world)
	{
		outErrorText = TEXT("Startup screen을 표시할 World가 없습니다.");
		return false;
	}

	const FString mapId = StartupMapId.TrimStartAndEnd();
	if (mapId.IsEmpty())
	{
		outErrorText = TEXT("Startup screen map id가 없습니다.");
		return false;
	}

	UGameplayStatics::OpenLevel(world, FName(*mapId));
	return true;
}

EPlatformUserProjectDeleteResult UPlatformUiSubsystem::DeleteUserProjectDirectory(const FString& projectPath) const
{
	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (IsUnsafePlatformUiDeleteRoot(normalizedProjectPath))
	{
		return EPlatformUserProjectDeleteResult::Unsafe;
	}
	const FString activeProjectPath = NormalizePlatformUiProjectPath(GetActiveProjectPath());
	if (!activeProjectPath.IsEmpty() && normalizedProjectPath.Equals(activeProjectPath, ESearchCase::IgnoreCase))
	{
		return EPlatformUserProjectDeleteResult::Unsafe;
	}
	if (!FPaths::DirectoryExists(normalizedProjectPath))
	{
		return EPlatformUserProjectDeleteResult::Missing;
	}
	if (!HasPlatformUiUserProjectCoreFiles(normalizedProjectPath))
	{
		return EPlatformUserProjectDeleteResult::Unsafe;
	}

	return IFileManager::Get().DeleteDirectory(*normalizedProjectPath, false, true)
		? EPlatformUserProjectDeleteResult::Deleted
		: EPlatformUserProjectDeleteResult::Failed;
}

bool UPlatformUiSubsystem::StartLegacySimulationRun(
	const FString& setupPath,
	const FString& requestedRunId,
	FString& outErrorText) const
{
	outErrorText.Reset();

	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outErrorText = TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다.");
		return false;
	}

	if (!subsystem->StartSimulationRun(setupPath, requestedRunId))
	{
		outErrorText = subsystem->GetLastError();
		return false;
	}

	return true;
}

bool UPlatformUiSubsystem::OpenScenarioEditorPath(const FString& scenarioPath, FString& outErrorText) const
{
	outErrorText.Reset();

	const UGameInstance* gameInstance = GetGameInstance();
	UScenarioEditorLaunchSubsystem* subsystem = gameInstance ? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
	if (!subsystem)
	{
		outErrorText = TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다.");
		return false;
	}

	if (!subsystem->OpenScenarioEditor(scenarioPath))
	{
		outErrorText = TEXT("ScenarioEditorMap 열기 실패.");
		return false;
	}

	return true;
}

TArray<FString> UPlatformUiSubsystem::ListProjectEpisodeResultFiles(const FString& runDirectory) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListProjectEpisodeResultFiles(runDirectory) : TArray<FString>();
}

FPlatformProjectRunProgressSnapshot UPlatformUiSubsystem::BuildProjectRunProgressSnapshot(
	const FString& runDirectory,
	const int32 configuredEpisodeCount) const
{
	const FString normalizedRunDirectory = NormalizePlatformUiProjectPath(runDirectory);
	FPlatformProjectRunProgressSnapshot snapshot;
	if (normalizedRunDirectory.IsEmpty())
	{
		return snapshot;
	}

	const FString statusPath = NormalizePlatformUiProjectPath(FPaths::Combine(
		normalizedRunDirectory,
		PlatformUiRunStatusFileName));
	const FPlatformUiRunStatusSnapshot statusSnapshot = ReadPlatformUiRunStatusSnapshot(statusPath);

	FProjectRunResultDashboardData dashboardData;
	const bool bDashboardLoaded = LoadProjectRunDashboard(normalizedRunDirectory, dashboardData);
	snapshot.bSummaryLoaded = bDashboardLoaded && dashboardData.bSummaryLoaded;
	snapshot.RunState = statusSnapshot.bStatusError
		? ESimulationRunState::Failed
		: (statusSnapshot.bValidStatus
			? statusSnapshot.RunState
			: (snapshot.bSummaryLoaded
				? ESimulationRunState::Completed
				: ESimulationRunState::Pending));
	snapshot.ProgressKind = statusSnapshot.bStatusError
		? EPlatformProjectRunProgressKind::Error
		: (statusSnapshot.bValidStatus
			? statusSnapshot.ProgressKind
			: (snapshot.RunState == ESimulationRunState::Completed
				? EPlatformProjectRunProgressKind::Completed
				: EPlatformProjectRunProgressKind::Pending));

	const int32 episodeResultCount = ListProjectEpisodeResultFiles(normalizedRunDirectory).Num();
	snapshot.CompletedCount = snapshot.bSummaryLoaded
		? dashboardData.EpisodeCount
		: episodeResultCount;
	snapshot.TotalCount = ReadPlatformUiRunSnapshotEpisodeCount(normalizedRunDirectory);
	if (snapshot.TotalCount <= 0 && snapshot.bSummaryLoaded)
	{
		snapshot.TotalCount = dashboardData.EpisodeCount;
	}
	if (snapshot.TotalCount <= 0)
	{
		snapshot.TotalCount = configuredEpisodeCount;
	}
	if (snapshot.TotalCount <= 0)
	{
		snapshot.TotalCount = episodeResultCount;
	}

	snapshot.CompletedCount = FMath::Max(0, snapshot.CompletedCount);
	snapshot.TotalCount = FMath::Max(FMath::Max(0, snapshot.TotalCount), snapshot.CompletedCount);
	snapshot.Percent = CalculatePlatformUiRunProgressPercent(snapshot.CompletedCount, snapshot.TotalCount);
	if (snapshot.ProgressKind == EPlatformProjectRunProgressKind::Starting
		|| snapshot.ProgressKind == EPlatformProjectRunProgressKind::Pending)
	{
		snapshot.Percent = 0.0f;
	}
	if (snapshot.ProgressKind == EPlatformProjectRunProgressKind::Completed)
	{
		snapshot.Percent = 100.0f;
	}
	return snapshot;
}

bool UPlatformUiSubsystem::LoadProjectRunDashboard(
	const FString& runDirectory,
	FProjectRunResultDashboardData& outDashboardData)
{
	outDashboardData = FProjectRunResultDashboardData{};

	const FString normalizedRunDirectory = NormalizePlatformUiProjectPath(runDirectory);
	if (normalizedRunDirectory.IsEmpty())
	{
		outDashboardData.Diagnostics.Add(TEXT("Run directory가 없습니다."));
		return false;
	}

	const bool bLoaded = FProjectRunResultDashboardJson::BuildFromRunDirectory(
		normalizedRunDirectory,
		outDashboardData);
	if (outDashboardData.RunId.IsEmpty())
	{
		outDashboardData.RunId = FPaths::GetCleanFilename(normalizedRunDirectory);
	}
	return bLoaded;
}

ESimulationRunState UPlatformUiSubsystem::ResolveProjectRunState(const FString& runDirectory)
{
	const FString normalizedRunDirectory = NormalizePlatformUiProjectPath(runDirectory);
	if (normalizedRunDirectory.IsEmpty())
	{
		return ESimulationRunState::Pending;
	}

	const FString statusPath = NormalizePlatformUiProjectPath(FPaths::Combine(
		normalizedRunDirectory,
		PlatformUiRunStatusFileName));
	const FPlatformUiRunStatusSnapshot statusSnapshot = ReadPlatformUiRunStatusSnapshot(statusPath);
	if (statusSnapshot.bStatusError)
	{
		return ESimulationRunState::Failed;
	}
	if (statusSnapshot.bValidStatus)
	{
		return statusSnapshot.RunState;
	}

	FProjectRunResultDashboardData dashboardData;
	return LoadProjectRunDashboard(normalizedRunDirectory, dashboardData) && dashboardData.bSummaryLoaded
		? ESimulationRunState::Completed
		: ESimulationRunState::Pending;
}

bool UPlatformUiSubsystem::RequestProjectRunAnalysis(
	const FString& projectPath,
	const FString& runId,
	FString& outErrorText) const
{
	outErrorText.Reset();

	UPlatformAnalysisAiSubsystem* subsystem = ResolvePlatformAnalysisAiSubsystem();
	if (!subsystem)
	{
		outErrorText = TEXT("PlatformAnalysisAiSubsystem을 사용할 수 없습니다.");
		return false;
	}

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	const FString normalizedRunId = runId.TrimStartAndEnd();
	if (normalizedProjectPath.IsEmpty() || normalizedRunId.IsEmpty())
	{
		outErrorText = TEXT("AI 분석을 요청할 project/run 선택이 필요합니다.");
		return false;
	}

	if (!subsystem->RequestAnalysisForProjectRun(normalizedProjectPath, normalizedRunId))
	{
		outErrorText = TEXT("AI 분석 요청 실패.");
		return false;
	}

	return true;
}

TArray<FString> UPlatformUiSubsystem::ListLegacySimulationSetupFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListSimulationSetupFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyScenarioSetupFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListScenarioSetupFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyDeliveryBotSetupFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListDeliveryBotSetupFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyPolicySpecFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListPolicySpecFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacySimulationRunResultDirectories() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListSimulationRunResultDirectories() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacySimulationRunStatusFiles() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListSimulationRunStatusFiles() : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyEvaluationReportFilesInDirectory(const FString& runDirectory) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListEvaluationReportFilesInDirectory(runDirectory) : TArray<FString>();
}

TArray<FString> UPlatformUiSubsystem::ListLegacyMeasurementLogFilesInDirectory(const FString& runDirectory) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->ListMeasurementLogFilesInDirectory(runDirectory) : TArray<FString>();
}

TArray<UOdiroListItemViewModel*> UPlatformUiSubsystem::CreatePathItemViewModels(
	const TArray<FString>& itemPaths,
	const bool bUseBaseFilenameAsTitle)
{
	TArray<UOdiroListItemViewModel*> items;
	items.Reserve(itemPaths.Num());
	for (const FString& itemPath : itemPaths)
	{
		UOdiroListItemViewModel* item = NewObject<UOdiroListItemViewModel>(this);
		if (!item)
		{
			continue;
		}

		const FString normalizedPath = itemPath.TrimStartAndEnd();
		const FString title = bUseBaseFilenameAsTitle
			? FPaths::GetBaseFilename(normalizedPath)
			: normalizedPath;
		const FString subtitle = bUseBaseFilenameAsTitle ? normalizedPath : FPaths::GetPath(normalizedPath);
		item->InitializeItem(normalizedPath, title, subtitle, normalizedPath);
		items.Add(item);
	}
	return items;
}

FSimulationSetupParseResult UPlatformUiSubsystem::LoadLegacySimulationSetupFile(const FString& setupPath) const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->LoadSimulationSetupFile(setupPath) : FSimulationSetupParseResult();
}

bool UPlatformUiSubsystem::SaveLegacyFixedStepFpsToSetupFile(
	const FString& setupPath,
	const int32 fps,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->SaveFixedStepFpsToSetupFile(setupPath, fps, outDiagnostics);
}

bool UPlatformUiSubsystem::SaveLegacySimulationSetupFile(
	const FString& setupPath,
	const FSimulationSetup& setup,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->SaveSimulationSetupFile(setupPath, setup, outDiagnostics);
}

bool UPlatformUiSubsystem::LoadLegacyScenarioRunQueueFile(
	const FString& runQueuePath,
	TArray<FScenarioRunInput>& outRunInputs,
	TArray<FString>& outDiagnostics) const
{
	outRunInputs.Reset();
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->LoadScenarioRunQueueFile(runQueuePath, outRunInputs, outDiagnostics);
}

bool UPlatformUiSubsystem::SaveLegacyScenarioRunQueueFile(
	const FString& runQueuePath,
	const TArray<FScenarioRunInput>& runInputs,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->SaveScenarioRunQueueFile(runQueuePath, runInputs, outDiagnostics);
}

bool UPlatformUiSubsystem::LoadExperimentSettingsForProject(
	const FString& projectPath,
	FExperimentConfigSettings& outSettings,
	FString& outErrorText)
{
	outSettings = FExperimentConfigSettings{};
	outErrorText.Reset();

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outErrorText = TEXT("Active project가 없습니다.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const FString settingPath = BuildPlatformUiExperimentSettingPath(normalizedProjectPath);
	if (!LoadPlatformUiJsonRoot(settingPath, rootObject, outErrorText))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* runtimeObject = nullptr;
	const TSharedPtr<FJsonObject>* samplingObject = nullptr;
	const TSharedPtr<FJsonObject>* evaluationObject = nullptr;
	if (!rootObject->TryGetObjectField(TEXT("runtime"), runtimeObject) || !runtimeObject || !runtimeObject->IsValid())
	{
		outErrorText = TEXT("setting.json에 runtime object가 없습니다.");
		return false;
	}
	if (!rootObject->TryGetObjectField(TEXT("sampling"), samplingObject) || !samplingObject || !samplingObject->IsValid())
	{
		outErrorText = TEXT("setting.json에 sampling object가 없습니다.");
		return false;
	}
	if (!rootObject->TryGetObjectField(TEXT("evaluation"), evaluationObject) || !evaluationObject || !evaluationObject->IsValid())
	{
		outErrorText = TEXT("setting.json에 evaluation object가 없습니다.");
		return false;
	}

	FString mapId;
	if (!(*runtimeObject)->TryGetStringField(TEXT("map_id"), mapId) || mapId.TrimStartAndEnd().IsEmpty())
	{
		mapId = PlatformUiExperimentDefaultMapId;
	}

	double fixedFps = 60.0;
	(*runtimeObject)->TryGetNumberField(TEXT("fixed_fps"), fixedFps);
	double timeScale = 1.0;
	(*runtimeObject)->TryGetNumberField(TEXT("time_scale"), timeScale);
	double maxDurationSeconds = 60.0;
	(*runtimeObject)->TryGetNumberField(TEXT("max_duration_s"), maxDurationSeconds);
	double episodeCount = 1.0;
	(*samplingObject)->TryGetNumberField(TEXT("episode_count"), episodeCount);
	double baseSeed = 0.0;
	(*samplingObject)->TryGetNumberField(TEXT("base_seed"), baseSeed);
	double tipOverAngleDegrees = 60.0;
	(*evaluationObject)->TryGetNumberField(TEXT("tip_over_angle_deg"), tipOverAngleDegrees);
	double nearMissDistanceMeters = 0.5;
	(*evaluationObject)->TryGetNumberField(TEXT("near_miss_distance_m"), nearMissDistanceMeters);
	double goalAcceptanceRadiusMeters = 1.0;
	(*evaluationObject)->TryGetNumberField(TEXT("goal_acceptance_radius_m"), goalAcceptanceRadiusMeters);

	outSettings.MapId = mapId.TrimStartAndEnd();
	outSettings.FixedFps = FMath::Max(1, FMath::RoundToInt(fixedFps));
	outSettings.TimeScale = FMath::Max(UE_SMALL_NUMBER, static_cast<float>(timeScale));
	outSettings.MaxDurationSeconds = FMath::Max(0.0f, static_cast<float>(maxDurationSeconds));
	outSettings.EpisodeCount = FMath::Max(1, FMath::RoundToInt(episodeCount));
	outSettings.BaseSeed = static_cast<int64>(baseSeed);
	outSettings.TipOverAngleDegrees = FMath::Clamp(static_cast<float>(tipOverAngleDegrees), 10.0f, 120.0f);
	outSettings.NearMissDistanceMeters = FMath::Max(0.0f, static_cast<float>(nearMissDistanceMeters));
	outSettings.GoalAcceptanceRadiusMeters = FMath::Max(UE_SMALL_NUMBER, static_cast<float>(goalAcceptanceRadiusMeters));
	return true;
}

bool UPlatformUiSubsystem::SaveExperimentSettingsForProject(
	const FString& projectPath,
	const FExperimentConfigSettings& settings,
	FString& outStatusText)
{
	outStatusText.Reset();
	if (!ValidatePlatformUiExperimentSettings(settings, outStatusText))
	{
		return false;
	}

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outStatusText = TEXT("Active project가 없습니다.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const FString settingPath = BuildPlatformUiExperimentSettingPath(normalizedProjectPath);
	if (!LoadPlatformUiJsonRoot(settingPath, rootObject, outStatusText))
	{
		return false;
	}

	const TSharedRef<FJsonObject> runtimeObject =
		FindOrCreatePlatformUiObjectField(rootObject.ToSharedRef(), TEXT("runtime"));
	const TSharedRef<FJsonObject> samplingObject =
		FindOrCreatePlatformUiObjectField(rootObject.ToSharedRef(), TEXT("sampling"));
	const TSharedRef<FJsonObject> evaluationObject =
		FindOrCreatePlatformUiObjectField(rootObject.ToSharedRef(), TEXT("evaluation"));
	runtimeObject->SetStringField(TEXT("map_id"), settings.MapId.TrimStartAndEnd());
	runtimeObject->SetNumberField(TEXT("fixed_fps"), settings.FixedFps);
	runtimeObject->SetNumberField(TEXT("time_scale"), settings.TimeScale);
	runtimeObject->SetNumberField(TEXT("max_duration_s"), settings.MaxDurationSeconds);
	samplingObject->SetNumberField(TEXT("episode_count"), settings.EpisodeCount);
	samplingObject->SetNumberField(TEXT("base_seed"), static_cast<double>(settings.BaseSeed));
	evaluationObject->SetNumberField(TEXT("tip_over_angle_deg"), settings.TipOverAngleDegrees);
	evaluationObject->SetNumberField(TEXT("near_miss_distance_m"), settings.NearMissDistanceMeters);
	evaluationObject->SetNumberField(TEXT("goal_acceptance_radius_m"), settings.GoalAcceptanceRadiusMeters);

	FString updatedJson;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&updatedJson);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		outStatusText = TEXT("setting.json 직렬화 실패.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		updatedJson,
		*settingPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outStatusText = FString::Printf(TEXT("setting.json 저장 실패: %s"), *settingPath);
		return false;
	}

	outStatusText = FString::Printf(TEXT("Experiment settings saved: %s"), *settingPath);
	return true;
}

bool UPlatformUiSubsystem::LoadRobotProfileBodyForProject(
	const FString& projectPath,
	FRobotProfileBodySettings& outSettings,
	FString& outErrorText)
{
	FRobotProfileSettings profileSettings;
	if (!LoadRobotProfileForProject(projectPath, profileSettings, outErrorText))
	{
		outSettings = FRobotProfileBodySettings{};
		return false;
	}

	outSettings = profileSettings.Body;
	return true;
}

bool UPlatformUiSubsystem::SaveRobotProfileBodyForProject(
	const FString& projectPath,
	const FRobotProfileBodySettings& settings,
	FString& outStatusText)
{
	FRobotProfileSettings profileSettings;
	if (!LoadRobotProfileForProject(projectPath, profileSettings, outStatusText))
	{
		return false;
	}

	profileSettings.Body = settings;
	return SaveRobotProfileForProject(projectPath, profileSettings, outStatusText);
}

bool UPlatformUiSubsystem::LoadRobotProfileForProject(
	const FString& projectPath,
	FRobotProfileSettings& outSettings,
	FString& outErrorText)
{
	outSettings = FRobotProfileSettings{};
	outErrorText.Reset();

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outErrorText = TEXT("Active project is not set.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const FString profilePath = BuildPlatformUiRobotProfilePath(normalizedProjectPath);
	if (!LoadPlatformUiJsonRoot(profilePath, rootObject, outErrorText))
	{
		return false;
	}

	const UDeliveryBotSetupCompiler* compiler = NewObject<UDeliveryBotSetupCompiler>();
	if (compiler)
	{
		FString profileJson;
		if (FFileHelper::LoadFileToString(profileJson, *profilePath))
		{
			const FDeliveryBotSetupCompileResult compileResult =
				compiler->CompileDeliveryBotSetupFromJsonString(profileJson);
			if (!compileResult.bSuccess)
			{
				outErrorText = FormatDeliveryBotSetupDiagnostics(compileResult);
				return false;
			}
		}
	}

	const TSharedPtr<FJsonObject>* robotObject = nullptr;
	if (!rootObject->TryGetObjectField(TEXT("robot"), robotObject) || !robotObject || !robotObject->IsValid())
	{
		outErrorText = TEXT("profile.json must contain a robot object.");
		return false;
	}

	const TSharedPtr<FJsonValue> bodyValue = (*robotObject)->TryGetField(TEXT("body"));
	if (bodyValue.IsValid())
	{
		if (bodyValue->Type != EJson::Object || !bodyValue->AsObject().IsValid())
		{
			outErrorText = TEXT("profile.json robot.body must be an object.");
			return false;
		}

		const TSharedPtr<FJsonObject> bodyObject = bodyValue->AsObject();
		ReadOptionalRobotProfileNumberField(*bodyObject, TEXT("length_m"), outSettings.Body.LengthM);
		ReadOptionalRobotProfileNumberField(*bodyObject, TEXT("width_m"), outSettings.Body.WidthM);
		ReadOptionalRobotProfileNumberField(*bodyObject, TEXT("height_m"), outSettings.Body.HeightM);
		ReadOptionalRobotProfileNumberField(*bodyObject, TEXT("wheel_base_m"), outSettings.Body.WheelBaseM);
		ReadOptionalRobotProfileNumberField(*bodyObject, TEXT("turning_radius_m"), outSettings.Body.TurningRadiusM);
	}

	const TSharedPtr<FJsonValue> driveValue = (*robotObject)->TryGetField(TEXT("drive"));
	if (driveValue.IsValid())
	{
		if (driveValue->Type != EJson::Object || !driveValue->AsObject().IsValid())
		{
			outErrorText = TEXT("profile.json robot.drive must be an object.");
			return false;
		}

		const TSharedPtr<FJsonObject> driveObject = driveValue->AsObject();
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("max_speed_kmh"), outSettings.Drive.MaxSpeedKmh);
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("max_reverse_speed_kmh"), outSettings.Drive.MaxReverseSpeedKmh);
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("max_reverse_kmh"), outSettings.Drive.MaxReverseSpeedKmh);
		ReadOptionalRobotProfileNumberField(
			*driveObject,
			TEXT("acceleration_rate_kmh_per_second"),
			outSettings.Drive.AccelerationRateKmhPerSecond);
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("accel_kmh_per_s"), outSettings.Drive.AccelerationRateKmhPerSecond);
		ReadOptionalRobotProfileNumberField(
			*driveObject,
			TEXT("deceleration_rate_kmh_per_second"),
			outSettings.Drive.DecelerationRateKmhPerSecond);
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("decel_kmh_per_s"), outSettings.Drive.DecelerationRateKmhPerSecond);
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("steering_rate_per_s"), outSettings.Drive.SteeringRatePerS);
		ReadOptionalRobotProfileNumberField(*driveObject, TEXT("mass_kg"), outSettings.Drive.MassKg);
	}

	const TSharedPtr<FJsonValue> lidarValue = (*robotObject)->TryGetField(TEXT("lidar"));
	if (lidarValue.IsValid())
	{
		if (lidarValue->Type != EJson::Object || !lidarValue->AsObject().IsValid())
		{
			outErrorText = TEXT("profile.json robot.lidar must be an object.");
			return false;
		}

		const TSharedPtr<FJsonObject> lidarObject = lidarValue->AsObject();
		ReadOptionalRobotProfileStringField(*lidarObject, TEXT("lidar_mode"), outSettings.Lidar.LidarMode);
		ReadOptionalRobotProfileStringField(*lidarObject, TEXT("mode"), outSettings.Lidar.LidarMode);
		ReadOptionalRobotProfileBoolField(*lidarObject, TEXT("draw_debug"), outSettings.Lidar.bDrawDebug);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("scan_range_m"), outSettings.Lidar.ScanRangeM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("range_m"), outSettings.Lidar.ScanRangeM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("sensor_height_m"), outSettings.Lidar.SensorHeightM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("height_m"), outSettings.Lidar.SensorHeightM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("sensor_forward_offset_m"), outSettings.Lidar.SensorForwardOffsetM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("sensor_right_offset_m"), outSettings.Lidar.SensorRightOffsetM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("front_half_angle_degree"), outSettings.Lidar.FrontHalfAngleDegree);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("stop_distance_m"), outSettings.Lidar.StopDistanceM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("near_miss_distance_m"), outSettings.Lidar.ObstacleWarningDistanceM);
		ReadOptionalRobotProfileNumberField(
			*lidarObject,
			TEXT("near_obstacle_warning_distance_m"),
			outSettings.Lidar.ObstacleWarningDistanceM);
		ReadOptionalRobotProfileNumberField(
			*lidarObject,
			TEXT("obstacle_warning_distance_m"),
			outSettings.Lidar.ObstacleWarningDistanceM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("slow_down_distance_m"), outSettings.Lidar.SlowDownDistanceM);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("angle_step_degree"), outSettings.Lidar.AngleStepDegree);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("vertical_min_degree"), outSettings.Lidar.VerticalMinDegree);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("vertical_max_degree"), outSettings.Lidar.VerticalMaxDegree);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("vertical_step_degree"), outSettings.Lidar.VerticalStepDegree);
		ReadOptionalRobotProfileNumberField(*lidarObject, TEXT("scan_rate_hz"), outSettings.Lidar.ScanRateHz);
	}
	return true;
}

bool UPlatformUiSubsystem::SaveRobotProfileForProject(
	const FString& projectPath,
	const FRobotProfileSettings& settings,
	FString& outStatusText)
{
	outStatusText.Reset();
	if (!ValidatePlatformUiRobotProfileSettings(settings, outStatusText))
	{
		return false;
	}

	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outStatusText = TEXT("Active project is not set.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const FString profilePath = BuildPlatformUiRobotProfilePath(normalizedProjectPath);
	if (!LoadPlatformUiJsonRoot(profilePath, rootObject, outStatusText))
	{
		return false;
	}

	const TSharedRef<FJsonObject> robotObject =
		FindOrCreatePlatformUiObjectField(rootObject.ToSharedRef(), TEXT("robot"));
	const TSharedRef<FJsonObject> bodyObject =
		FindOrCreatePlatformUiObjectField(robotObject, TEXT("body"));
	bodyObject->SetNumberField(TEXT("length_m"), settings.Body.LengthM);
	bodyObject->SetNumberField(TEXT("width_m"), settings.Body.WidthM);
	bodyObject->SetNumberField(TEXT("height_m"), settings.Body.HeightM);
	bodyObject->SetNumberField(TEXT("wheel_base_m"), settings.Body.WheelBaseM);
	bodyObject->SetNumberField(TEXT("turning_radius_m"), settings.Body.TurningRadiusM);

	const TSharedRef<FJsonObject> driveObject =
		FindOrCreatePlatformUiObjectField(robotObject, TEXT("drive"));
	driveObject->SetNumberField(TEXT("max_speed_kmh"), settings.Drive.MaxSpeedKmh);
	driveObject->SetNumberField(TEXT("max_reverse_speed_kmh"), settings.Drive.MaxReverseSpeedKmh);
	if (driveObject->HasField(TEXT("max_reverse_kmh")))
	{
		driveObject->SetNumberField(TEXT("max_reverse_kmh"), settings.Drive.MaxReverseSpeedKmh);
	}
	driveObject->SetNumberField(
		TEXT("acceleration_rate_kmh_per_second"),
		settings.Drive.AccelerationRateKmhPerSecond);
	if (driveObject->HasField(TEXT("accel_kmh_per_s")))
	{
		driveObject->SetNumberField(TEXT("accel_kmh_per_s"), settings.Drive.AccelerationRateKmhPerSecond);
	}
	driveObject->SetNumberField(
		TEXT("deceleration_rate_kmh_per_second"),
		settings.Drive.DecelerationRateKmhPerSecond);
	if (driveObject->HasField(TEXT("decel_kmh_per_s")))
	{
		driveObject->SetNumberField(TEXT("decel_kmh_per_s"), settings.Drive.DecelerationRateKmhPerSecond);
	}
	driveObject->SetNumberField(TEXT("steering_rate_per_s"), settings.Drive.SteeringRatePerS);
	driveObject->SetNumberField(TEXT("mass_kg"), settings.Drive.MassKg);

	const TSharedRef<FJsonObject> lidarObject =
		FindOrCreatePlatformUiObjectField(robotObject, TEXT("lidar"));
	const FString lidarMode = NormalizePlatformUiRobotProfileLidarModeForSave(settings.Lidar.LidarMode);
	lidarObject->SetStringField(TEXT("lidar_mode"), lidarMode);
	if (lidarObject->HasField(TEXT("mode")))
	{
		lidarObject->SetStringField(TEXT("mode"), lidarMode);
	}
	lidarObject->SetBoolField(TEXT("draw_debug"), settings.Lidar.bDrawDebug);
	lidarObject->SetNumberField(TEXT("scan_range_m"), settings.Lidar.ScanRangeM);
	if (lidarObject->HasField(TEXT("range_m")))
	{
		lidarObject->SetNumberField(TEXT("range_m"), settings.Lidar.ScanRangeM);
	}
	lidarObject->SetNumberField(TEXT("sensor_height_m"), settings.Lidar.SensorHeightM);
	if (lidarObject->HasField(TEXT("height_m")))
	{
		lidarObject->SetNumberField(TEXT("height_m"), settings.Lidar.SensorHeightM);
	}
	lidarObject->SetNumberField(TEXT("sensor_forward_offset_m"), settings.Lidar.SensorForwardOffsetM);
	lidarObject->SetNumberField(TEXT("sensor_right_offset_m"), settings.Lidar.SensorRightOffsetM);
	lidarObject->SetNumberField(TEXT("front_half_angle_degree"), settings.Lidar.FrontHalfAngleDegree);
	lidarObject->SetNumberField(TEXT("stop_distance_m"), settings.Lidar.StopDistanceM);
	lidarObject->SetNumberField(TEXT("obstacle_warning_distance_m"), settings.Lidar.ObstacleWarningDistanceM);
	lidarObject->SetNumberField(TEXT("slow_down_distance_m"), settings.Lidar.SlowDownDistanceM);
	lidarObject->SetNumberField(TEXT("angle_step_degree"), settings.Lidar.AngleStepDegree);
	lidarObject->SetNumberField(TEXT("vertical_min_degree"), settings.Lidar.VerticalMinDegree);
	lidarObject->SetNumberField(TEXT("vertical_max_degree"), settings.Lidar.VerticalMaxDegree);
	lidarObject->SetNumberField(TEXT("vertical_step_degree"), settings.Lidar.VerticalStepDegree);
	lidarObject->SetNumberField(TEXT("scan_rate_hz"), settings.Lidar.ScanRateHz);

	FString updatedJson;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&updatedJson);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		outStatusText = TEXT("profile.json serialization failed.");
		return false;
	}

	const UDeliveryBotSetupCompiler* compiler = NewObject<UDeliveryBotSetupCompiler>();
	if (compiler)
	{
		const FDeliveryBotSetupCompileResult compileResult =
			compiler->CompileDeliveryBotSetupFromJsonString(updatedJson);
		if (!compileResult.bSuccess)
		{
			outStatusText = FormatDeliveryBotSetupDiagnostics(compileResult);
			return false;
		}
	}

	if (!FFileHelper::SaveStringToFile(
		updatedJson,
		*profilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outStatusText = FString::Printf(TEXT("profile.json save failed: %s"), *profilePath);
		return false;
	}

	outStatusText = FString::Printf(TEXT("Robot profile saved: %s"), *profilePath);
	return true;
}

bool UPlatformUiSubsystem::ReplaceLegacyScenarioSetupReferencesInRunQueues(
	const FString& oldScenarioSetupPath,
	const FString& newScenarioSetupPath,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->ReplaceScenarioSetupReferencesInRunQueues(
		oldScenarioSetupPath,
		newScenarioSetupPath,
		outDiagnostics);
}

bool UPlatformUiSubsystem::ReplaceLegacyDeliveryBotSetupReferencesInRunQueues(
	const FString& oldDeliveryBotSetupPath,
	const FString& newDeliveryBotSetupPath,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	return subsystem->ReplaceDeliveryBotSetupReferencesInRunQueues(
		oldDeliveryBotSetupPath,
		newDeliveryBotSetupPath,
		outDiagnostics);
}

bool UPlatformUiSubsystem::RefreshActiveRunStatus() const
{
	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->RefreshActiveRunStatus() : false;
}

void UPlatformUiSubsystem::NotifyProjectPreviewUpdated(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizePlatformUiProjectPath(projectPath);
	if (!normalizedProjectPath.IsEmpty())
	{
		OnProjectPreviewUpdated.Broadcast(normalizedProjectPath);
	}
}

FSimulatorRunInfo UPlatformUiSubsystem::GetActiveRunInfo() const
{
	const USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	return subsystem ? subsystem->GetActiveRunInfo() : FSimulatorRunInfo();
}

bool UPlatformUiSubsystem::IsAnalysisRequestPending() const
{
	const UPlatformAnalysisAiSubsystem* subsystem = ResolvePlatformAnalysisAiSubsystem();
	return subsystem && subsystem->IsAnalysisRequestPending();
}

bool UPlatformUiSubsystem::TryReadBridgeRunStatusState(
	const FString& statusPath,
	ESimulationRunState& outState)
{
	outState = ESimulationRunState::Pending;
	const FPlatformUiRunStatusSnapshot snapshot = ReadPlatformUiRunStatusSnapshot(statusPath);
	if (!snapshot.bValidStatus)
	{
		return false;
	}

	outState = snapshot.RunState;
	return true;
}

FString UPlatformUiSubsystem::BuildLogPreview(const FString& logPath, const int32 edgeLineCount)
{
	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *FSimulationSetupJson::ResolveProjectPath(logPath)))
	{
		return FString::Printf(TEXT("Log read failed: %s"), *logPath);
	}

	const int32 clampedEdgeLineCount = FMath::Max(0, edgeLineCount);
	TArray<FString> previewLines;
	for (int32 lineIndex = 0; lineIndex < FMath::Min(clampedEdgeLineCount, lines.Num()); ++lineIndex)
	{
		previewLines.Add(lines[lineIndex]);
	}

	if (lines.Num() > clampedEdgeLineCount * 2)
	{
		previewLines.Add(TEXT("..."));
	}

	const int32 tailStartIndex = FMath::Max(clampedEdgeLineCount, lines.Num() - clampedEdgeLineCount);
	for (int32 lineIndex = tailStartIndex; lineIndex < lines.Num(); ++lineIndex)
	{
		previewLines.Add(lines[lineIndex]);
	}

	return FString::Join(previewLines, TEXT("\n"));
}

bool UPlatformUiSubsystem::TryReadExperimentResultReportItem(
	const FString& reportPath,
	FExperimentResultReportItem& outItem)
{
	outItem = FExperimentResultReportItem{};
	outItem.ReportPath = reportPath;

	FString reportJson;
	if (!ReadResolvedTextFile(reportPath, reportJson))
	{
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(reportJson);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		return false;
	}

	FString schema;
	if (!rootObject->TryGetStringField(TEXT("schema"), schema)
		|| !schema.Equals(TEXT("episode_evaluation_report"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	const TSharedPtr<FJsonValue> runValue = rootObject->TryGetField(TEXT("run"));
	if (!runValue.IsValid() || runValue->Type != EJson::Object)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> runObject = runValue->AsObject();
	if (!runObject.IsValid())
	{
		return false;
	}

	double runIndex = 0.0;
	if (runObject->TryGetNumberField(TEXT("run_index"), runIndex))
	{
		outItem.RunIndex = FMath::RoundToInt(runIndex);
	}

	return true;
}

TArray<FExperimentResultReportItem> UPlatformUiSubsystem::BuildExperimentResultReportItems(
	const TArray<FString>& reportPaths)
{
	TArray<FExperimentResultReportItem> items;
	for (const FString& reportPath : reportPaths)
	{
		FExperimentResultReportItem item;
		if (TryReadExperimentResultReportItem(reportPath, item))
		{
			items.Add(item);
		}
	}

	items.Sort([](const FExperimentResultReportItem& left, const FExperimentResultReportItem& right)
	{
		if (left.RunIndex != right.RunIndex)
		{
			if (left.RunIndex == INDEX_NONE)
			{
				return false;
			}
			if (right.RunIndex == INDEX_NONE)
			{
				return true;
			}
			return left.RunIndex < right.RunIndex;
		}
		return left.ReportPath < right.ReportPath;
	});
	return items;
}

bool UPlatformUiSubsystem::CreateTextFileFromTemplate(
	const FString& templatePath,
	const FString& outputPath,
	FString& outErrorText)
{
	outErrorText.Reset();

	const FString resolvedTemplatePath = FSimulationSetupJson::ResolveProjectPath(templatePath);
	FString templateText;
	if (!FFileHelper::LoadFileToString(templateText, *resolvedTemplatePath))
	{
		outErrorText = FString::Printf(TEXT("Template read failed: %s"), *templatePath);
		return false;
	}

	const FString resolvedOutputPath = FSimulationSetupJson::ResolveProjectPath(outputPath);
	const FString outputDirectory = FPaths::GetPath(resolvedOutputPath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true)
		|| !FFileHelper::SaveStringToFile(
			templateText,
			*resolvedOutputPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outErrorText = FString::Printf(TEXT("File create failed: %s"), *resolvedOutputPath);
		return false;
	}

	return true;
}

bool UPlatformUiSubsystem::MoveProjectRelativeFile(
	const FString& sourcePath,
	const FString& targetPath,
	const FString& itemLabel,
	FString& outErrorText)
{
	outErrorText.Reset();
	if (sourcePath.Equals(targetPath, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString resolvedSourcePath = FSimulationSetupJson::ResolveProjectPath(sourcePath);
	const FString resolvedTargetPath = FSimulationSetupJson::ResolveProjectPath(targetPath);
	if (!FPaths::FileExists(resolvedSourcePath))
	{
		outErrorText = FString::Printf(TEXT("%s 파일을 찾을 수 없습니다: %s"), *itemLabel, *sourcePath);
		return false;
	}
	if (FPaths::FileExists(resolvedTargetPath))
	{
		outErrorText = FString::Printf(TEXT("%s 파일이 이미 존재합니다: %s"), *itemLabel, *targetPath);
		return false;
	}

	const FString targetDirectory = FPaths::GetPath(resolvedTargetPath);
	if (!IFileManager::Get().MakeDirectory(*targetDirectory, true)
		|| !IFileManager::Get().Move(*resolvedTargetPath, *resolvedSourcePath, false, false))
	{
		outErrorText = FString::Printf(TEXT("%s 이름 변경 실패: %s -> %s"), *itemLabel, *sourcePath, *targetPath);
		return false;
	}

	return true;
}

FString UPlatformUiSubsystem::MakeUniqueInputJsonPath(const FString& baseFileName)
{
	for (int32 index = 0; index < 1000; ++index)
	{
		const FString fileName = index == 0
			? FString::Printf(TEXT("%s.json"), *baseFileName)
			: FString::Printf(TEXT("%s_%d.json"), *baseFileName, index);
		FString relativePath = FPaths::Combine(TEXT("Json/Input"), fileName);
		relativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!DoesResolvedFileExist(relativePath))
		{
			return relativePath;
		}
	}

	FString fallbackPath = FPaths::Combine(
		TEXT("Json/Input"),
		FString::Printf(TEXT("%s_%s.json"), *baseFileName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
	fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return fallbackPath;
}

bool UPlatformUiSubsystem::DoesResolvedFileExist(const FString& path)
{
	return FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(path));
}

bool UPlatformUiSubsystem::DoesResolvedDirectoryExist(const FString& path)
{
	return IFileManager::Get().DirectoryExists(*FSimulationSetupJson::ResolveProjectPath(path));
}

bool UPlatformUiSubsystem::ReadResolvedTextFile(const FString& path, FString& outText)
{
	outText.Reset();
	return FFileHelper::LoadFileToString(outText, *FSimulationSetupJson::ResolveProjectPath(path));
}

USimulatorLaunchSubsystem* UPlatformUiSubsystem::ResolveSimulatorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UPlatformUiSubsystem::ResolveProjectSessionSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

UPlatformAnalysisAiSubsystem* UPlatformUiSubsystem::ResolvePlatformAnalysisAiSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UPlatformAnalysisAiSubsystem>() : nullptr;
}

void UPlatformUiSubsystem::HandleRunInfoChanged(const FSimulatorRunInfo& runInfo)
{
	OnRunInfoChanged.Broadcast(runInfo);
}

void UPlatformUiSubsystem::HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response)
{
	OnAnalysisCompleted.Broadcast(response);
}
