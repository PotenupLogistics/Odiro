#include "Platform/ViewModel/ExperimentConfigViewModel.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Platform/ExperimentConfigSettings.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ProjectSessionSubsystem.h"

namespace
{
	// 입력 project path를 absolute normalized path로 맞춘다.
	FString NormalizeExperimentConfigVmPath(FString path)
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
}

void UExperimentConfigViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	LoadFromActiveProject();
}

void UExperimentConfigViewModel::SetSubsystemOverride(UProjectSessionSubsystem* projectSessionSubsystem)
{
	ProjectSessionOverride = projectSessionSubsystem;
}

bool UExperimentConfigViewModel::LoadFromActiveProject()
{
	return LoadFromProject(FString());
}

bool UExperimentConfigViewModel::LoadFromProject(const FString& projectPath)
{
	const FString resolvedProjectPath = ResolveProjectPath(projectPath);
	if (resolvedProjectPath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Active project가 없습니다."));
		return false;
	}

	FString error;
	FExperimentConfigSettings settings;
	if (!UPlatformUiSubsystem::LoadExperimentSettingsForProject(resolvedProjectPath, settings, error))
	{
		SetDiagnosticsText(error);
		return false;
	}

	SetMapId(settings.MapId);
	SetFixedFps(settings.FixedFps);
	SetTimeScale(settings.TimeScale);
	SetMaxDurationSeconds(settings.MaxDurationSeconds);
	SetEpisodeCount(settings.EpisodeCount);
	SetBaseSeed(settings.BaseSeed);
	SetTipOverAngleDegrees(settings.TipOverAngleDegrees);
	SetNearMissDistanceMeters(settings.NearMissDistanceMeters);
	SetGoalAcceptanceRadiusMeters(settings.GoalAcceptanceRadiusMeters);
	ClearDiagnostics();
	return true;
}

bool UExperimentConfigViewModel::SaveExperimentSettings()
{
	return SaveToProject(FString());
}

bool UExperimentConfigViewModel::SaveToProject(const FString& projectPath)
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
		SetDiagnosticsText(TEXT("Active project가 없습니다."));
		return false;
	}

	FExperimentConfigSettings settings;
	settings.MapId = MapId.TrimStartAndEnd();
	settings.FixedFps = FixedFps;
	settings.TimeScale = TimeScale;
	settings.MaxDurationSeconds = MaxDurationSeconds;
	settings.EpisodeCount = EpisodeCount;
	settings.BaseSeed = BaseSeed;
	settings.TipOverAngleDegrees = TipOverAngleDegrees;
	settings.NearMissDistanceMeters = NearMissDistanceMeters;
	settings.GoalAcceptanceRadiusMeters = GoalAcceptanceRadiusMeters;

	FString statusText;
	const bool bSaved = UPlatformUiSubsystem::SaveExperimentSettingsForProject(
		resolvedProjectPath,
		settings,
		statusText);
	SetDiagnosticsText(statusText);
	return bSaved;
}

void UExperimentConfigViewModel::SetMapId(const FString& mapId)
{
	UE_MVVM_SET_PROPERTY_VALUE(MapId, mapId.TrimStartAndEnd());
}

void UExperimentConfigViewModel::SetFixedFps(const int32 fixedFps)
{
	UE_MVVM_SET_PROPERTY_VALUE(FixedFps, fixedFps);
}

void UExperimentConfigViewModel::SetTimeScale(const float timeScale)
{
	UE_MVVM_SET_PROPERTY_VALUE(TimeScale, timeScale);
}

void UExperimentConfigViewModel::SetMaxDurationSeconds(const float maxDurationSeconds)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxDurationSeconds, maxDurationSeconds);
}

void UExperimentConfigViewModel::SetEpisodeCount(const int32 episodeCount)
{
	UE_MVVM_SET_PROPERTY_VALUE(EpisodeCount, episodeCount);
}

void UExperimentConfigViewModel::SetBaseSeed(const int64 baseSeed)
{
	UE_MVVM_SET_PROPERTY_VALUE(BaseSeed, baseSeed);
}

void UExperimentConfigViewModel::SetTipOverAngleDegrees(const float tipOverAngleDegrees)
{
	UE_MVVM_SET_PROPERTY_VALUE(TipOverAngleDegrees, tipOverAngleDegrees);
}

void UExperimentConfigViewModel::SetNearMissDistanceMeters(const float nearMissDistanceMeters)
{
	UE_MVVM_SET_PROPERTY_VALUE(NearMissDistanceMeters, nearMissDistanceMeters);
}

void UExperimentConfigViewModel::SetGoalAcceptanceRadiusMeters(const float goalAcceptanceRadiusMeters)
{
	UE_MVVM_SET_PROPERTY_VALUE(GoalAcceptanceRadiusMeters, goalAcceptanceRadiusMeters);
}

bool UExperimentConfigViewModel::CanSaveExperimentSettings() const
{
	TArray<FString> diagnostics;
	return ValidateInputs(diagnostics);
}

UProjectSessionSubsystem* UExperimentConfigViewModel::ResolveProjectSessionSubsystem() const
{
	if (ProjectSessionOverride)
	{
		return ProjectSessionOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

bool UExperimentConfigViewModel::ValidateInputs(TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	if (MapId.TrimStartAndEnd().IsEmpty())
	{
		outDiagnostics.Add(TEXT("Map ID를 입력하세요."));
	}
	if (FixedFps <= 0)
	{
		outDiagnostics.Add(TEXT("Fixed FPS는 1 이상이어야 합니다."));
	}
	if (TimeScale <= 0.0f)
	{
		outDiagnostics.Add(TEXT("Time Scale은 0보다 커야 합니다."));
	}
	if (MaxDurationSeconds < 0.0f)
	{
		outDiagnostics.Add(TEXT("Max Duration은 0 이상이어야 합니다."));
	}
	if (EpisodeCount <= 0)
	{
		outDiagnostics.Add(TEXT("Episode Count는 1 이상이어야 합니다."));
	}
	if (TipOverAngleDegrees < 10.0f || TipOverAngleDegrees > 120.0f)
	{
		outDiagnostics.Add(TEXT("Tip Over Angle은 10~120 범위여야 합니다."));
	}
	if (NearMissDistanceMeters < 0.0f)
	{
		outDiagnostics.Add(TEXT("Near Miss Distance는 0 이상이어야 합니다."));
	}
	if (GoalAcceptanceRadiusMeters <= 0.0f)
	{
		outDiagnostics.Add(TEXT("Goal Acceptance Radius는 0보다 커야 합니다."));
	}
	return outDiagnostics.IsEmpty();
}

FString UExperimentConfigViewModel::ResolveProjectPath(const FString& projectPath) const
{
	const FString normalizedProjectPath = NormalizeExperimentConfigVmPath(projectPath);
	if (!normalizedProjectPath.IsEmpty())
	{
		return normalizedProjectPath;
	}

	UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	return projectSession && projectSession->HasActiveProject()
		? NormalizeExperimentConfigVmPath(projectSession->GetActiveProjectPath())
		: FString();
}
