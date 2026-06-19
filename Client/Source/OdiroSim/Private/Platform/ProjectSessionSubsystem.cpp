#include "Platform/ProjectSessionSubsystem.h"

#include "Misc/Paths.h"

namespace
{
	const TCHAR* ProjectSessionScenarioFileName = TEXT("scenario.json");
	const TCHAR* ProjectSessionSettingFileName = TEXT("setting.json");
}

void UProjectSessionSubsystem::SetActiveProjectPath(const FString& projectPath)
{
	ActiveProjectPath = NormalizeProjectPath(projectPath);
}

void UProjectSessionSubsystem::ClearActiveProject()
{
	ActiveProjectPath.Reset();
}

FString UProjectSessionSubsystem::GetActiveProjectScenarioPath() const
{
	return BuildProjectFilePath(ActiveProjectPath, ProjectSessionScenarioFileName);
}

FString UProjectSessionSubsystem::GetActiveProjectSettingPath() const
{
	return BuildProjectFilePath(ActiveProjectPath, ProjectSessionSettingFileName);
}

FString UProjectSessionSubsystem::NormalizeProjectPath(const FString& projectPath)
{
	FString normalizedPath = projectPath.TrimStartAndEnd();
	if (normalizedPath.IsEmpty())
	{
		return FString();
	}

	normalizedPath = FPaths::IsRelative(normalizedPath)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), normalizedPath))
		: FPaths::ConvertRelativePathToFull(normalizedPath);
	FPaths::NormalizeFilename(normalizedPath);
	return normalizedPath;
}

FString UProjectSessionSubsystem::BuildProjectFilePath(const FString& projectPath, const TCHAR* fileName)
{
	const FString normalizedProjectPath = NormalizeProjectPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		return FString();
	}

	FString filePath = FPaths::Combine(normalizedProjectPath, fileName);
	FPaths::NormalizeFilename(filePath);
	return filePath;
}
