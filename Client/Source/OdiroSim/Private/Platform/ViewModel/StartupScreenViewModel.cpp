#include "Platform/ViewModel/StartupScreenViewModel.h"

#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"

namespace
{
	const TCHAR* StartupScreenConfigSection = TEXT("OdiroSim.Platform.ProjectOpen");
	const TCHAR* StartupScreenLegacyConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");
	const TCHAR* StartupScreenRecentProjectPathsKey = TEXT("RecentProjectPaths");
	const int32 StartupScreenMaxRecentProjectCount = 8;
	// StartupScreen에 동시에 표시할 최근 project card 수.
	const int32 StartupScreenMaxVisibleRecentProjectCount = 3;

	// 사용자 입력 project path를 absolute normalized path로 맞춘다.
	FString NormalizeStartupScreenPath(FString path)
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

	// 최근 project 카드의 보조 표시 문자열을 만든다.
	FString MakeStartupScreenProjectSubtitle(const FString& projectPath)
	{
		const FString parentFolderPath = FPaths::GetPath(NormalizeStartupScreenPath(projectPath));
		const FString parentFolderName = FPaths::GetCleanFilename(parentFolderPath);
		return parentFolderName.IsEmpty() ? parentFolderPath : parentFolderName;
	}

	// User project root 아래 preview.png가 있으면 절대 경로를 반환한다.
	FString ResolveStartupScreenPreviewPath(const FString& projectPath)
	{
		const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
		if (normalizedProjectPath.IsEmpty() || !FPaths::DirectoryExists(normalizedProjectPath))
		{
			return FString();
		}

		FString previewPath = FPaths::Combine(normalizedProjectPath, TEXT("preview.png"));
		FPaths::NormalizeFilename(previewPath);
		return FPaths::FileExists(previewPath) ? previewPath : FString();
	}
}

void UStartupScreenViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	RefreshRecentProjects();
}

void UStartupScreenViewModel::SetSubsystemOverrides(
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
	UProjectSessionSubsystem* projectSessionSubsystem,
	UScenarioEditorLaunchSubsystem* scenarioEditorLaunchSubsystem)
{
	SimulatorLaunchOverride = simulatorLaunchSubsystem;
	ProjectSessionOverride = projectSessionSubsystem;
	ScenarioEditorLaunchOverride = scenarioEditorLaunchSubsystem;
}

void UStartupScreenViewModel::RefreshRecentProjects()
{
	LoadRecentProjectPaths();
	PruneMissingRecentProjects();
	RebuildRecentProjectItems();
}

void UStartupScreenViewModel::SelectProject(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
	UE_MVVM_SET_PROPERTY_VALUE(SelectedProjectPath, normalizedProjectPath);
	RebuildRecentProjectItems();
}

bool UStartupScreenViewModel::ValidateProject(const FString& projectPath, TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outDiagnostics.Add(DiagnosticMessages.ProjectRequired);
		SetDiagnosticsText(outDiagnostics[0]);
		return false;
	}
	if (!FPaths::DirectoryExists(normalizedProjectPath))
	{
		outDiagnostics.Add(DiagnosticMessages.ProjectFolderMissing);
		SetDiagnosticsText(outDiagnostics[0]);
		return false;
	}

	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = ResolveSimulatorLaunchSubsystem();
	if (!simulatorLaunchSubsystem)
	{
		outDiagnostics.Add(DiagnosticMessages.SimulatorLaunchUnavailable);
		SetDiagnosticsText(outDiagnostics[0]);
		return false;
	}

	if (!simulatorLaunchSubsystem->ValidateUserProject(normalizedProjectPath, outDiagnostics))
	{
		const FString message = outDiagnostics.IsEmpty()
			? DiagnosticMessages.ProjectValidationFailed
			: FString::Join(outDiagnostics, TEXT("\n"));
		SetDiagnosticsText(message);
		return false;
	}

	ClearDiagnostics();
	return true;
}

bool UStartupScreenViewModel::AddRecentProjectIfValid(
	const FString& projectPath,
	TArray<FString>& outDiagnostics)
{
	if (!ValidateProject(projectPath, outDiagnostics))
	{
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
	RememberRecentProject(normalizedProjectPath);
	SelectProject(normalizedProjectPath);
	ClearDiagnostics();
	return true;
}

bool UStartupScreenViewModel::OpenProject(const FString& projectPath)
{
	TArray<FString> diagnostics;
	if (!ValidateProject(projectPath, diagnostics))
	{
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
	UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	UScenarioEditorLaunchSubsystem* scenarioEditorLaunch = ResolveScenarioEditorLaunchSubsystem();
	if (!projectSession || !scenarioEditorLaunch)
	{
		SetDiagnosticsText(DiagnosticMessages.ProjectOpenSubsystemUnavailable);
		return false;
	}

	projectSession->SetActiveProjectPath(normalizedProjectPath);
	RememberRecentProject(normalizedProjectPath);
	SelectProject(normalizedProjectPath);

	const FString scenarioPath = projectSession->GetActiveProjectScenarioPath();
	if (!scenarioEditorLaunch->OpenScenarioEditor(scenarioPath))
	{
		SetDiagnosticsText(DiagnosticMessages.ScenarioEditorOpenFailed);
		return false;
	}

	ClearDiagnostics();
	return true;
}

bool UStartupScreenViewModel::RemoveRecentProject(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
	const int32 removedCount = RecentProjectPaths.RemoveAll(
		[&normalizedProjectPath](const FString& storedPath)
		{
			return NormalizeStartupScreenPath(storedPath).Equals(normalizedProjectPath, ESearchCase::IgnoreCase);
		});
	if (removedCount <= 0)
	{
		return false;
	}

	if (SelectedProjectPath.Equals(normalizedProjectPath, ESearchCase::IgnoreCase))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedProjectPath, FString());
	}

	SaveRecentProjectPaths();
	RebuildRecentProjectItems();
	return true;
}

TArray<FString> UStartupScreenViewModel::GetRecentProjectPaths() const
{
	return RecentProjectPaths;
}

void UStartupScreenViewModel::SetDiagnosticsText(const FString& message)
{
	UE_MVVM_SET_PROPERTY_VALUE(DiagnosticsText, message);
}

void UStartupScreenViewModel::SetDiagnosticMessages(const FStartupScreenDiagnosticMessages& messages)
{
	DiagnosticMessages = messages;
}

void UStartupScreenViewModel::ClearDiagnostics()
{
	SetDiagnosticsText(FString());
}

void UStartupScreenViewModel::SetBusy(const bool bInBusy)
{
	UE_MVVM_SET_PROPERTY_VALUE(bBusy, bInBusy);
}

USimulatorLaunchSubsystem* UStartupScreenViewModel::ResolveSimulatorLaunchSubsystem() const
{
	if (SimulatorLaunchOverride)
	{
		return SimulatorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UStartupScreenViewModel::ResolveProjectSessionSubsystem() const
{
	if (ProjectSessionOverride)
	{
		return ProjectSessionOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

UScenarioEditorLaunchSubsystem* UStartupScreenViewModel::ResolveScenarioEditorLaunchSubsystem() const
{
	if (ScenarioEditorLaunchOverride)
	{
		return ScenarioEditorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
}

void UStartupScreenViewModel::LoadRecentProjectPaths()
{
	TArray<FString> storedPaths;
	if (GConfig)
	{
		const int32 loadedCount = GConfig->GetArray(
			StartupScreenConfigSection,
			StartupScreenRecentProjectPathsKey,
			storedPaths,
			GGameUserSettingsIni);
		if (loadedCount <= 0 || storedPaths.IsEmpty())
		{
			GConfig->GetArray(
				StartupScreenLegacyConfigSection,
				StartupScreenRecentProjectPathsKey,
				storedPaths,
				GGameUserSettingsIni);
		}
	}

	TArray<FString> normalizedPaths;
	for (const FString& storedPath : storedPaths)
	{
		const FString normalizedPath = NormalizeStartupScreenPath(storedPath);
		if (normalizedPath.IsEmpty())
		{
			continue;
		}

		normalizedPaths.RemoveAll(
			[&normalizedPath](const FString& existingPath)
			{
				return NormalizeStartupScreenPath(existingPath).Equals(normalizedPath, ESearchCase::IgnoreCase);
			});
		normalizedPaths.Add(normalizedPath);
	}

	RecentProjectPaths = normalizedPaths;
}

void UStartupScreenViewModel::SaveRecentProjectPaths() const
{
	if (GConfig)
	{
		GConfig->SetArray(
			StartupScreenConfigSection,
			StartupScreenRecentProjectPathsKey,
			RecentProjectPaths,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UStartupScreenViewModel::RememberRecentProject(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		return;
	}

	RecentProjectPaths.RemoveAll(
		[&normalizedProjectPath](const FString& storedPath)
		{
			return NormalizeStartupScreenPath(storedPath).Equals(normalizedProjectPath, ESearchCase::IgnoreCase);
		});
	RecentProjectPaths.Insert(normalizedProjectPath, 0);
	if (RecentProjectPaths.Num() > StartupScreenMaxRecentProjectCount)
	{
		RecentProjectPaths.SetNum(StartupScreenMaxRecentProjectCount);
	}

	SaveRecentProjectPaths();
	RebuildRecentProjectItems();
}

bool UStartupScreenViewModel::PruneMissingRecentProjects()
{
	const int32 oldCount = RecentProjectPaths.Num();
	RecentProjectPaths.RemoveAll(
		[](const FString& projectPath)
		{
			return !FPaths::DirectoryExists(NormalizeStartupScreenPath(projectPath));
		});

	if (RecentProjectPaths.Num() == oldCount)
	{
		return false;
	}

	SaveRecentProjectPaths();
	return true;
}

void UStartupScreenViewModel::RebuildRecentProjectItems()
{
	const bool bHasSelectedProject = RecentProjectPaths.ContainsByPredicate(
		[this](const FString& projectPath)
		{
			return NormalizeStartupScreenPath(projectPath).Equals(SelectedProjectPath, ESearchCase::IgnoreCase);
		});
	const FString defaultSelectedProjectPath = RecentProjectPaths.IsEmpty()
		? FString()
		: NormalizeStartupScreenPath(RecentProjectPaths[0]);
	if (!bHasSelectedProject && !SelectedProjectPath.Equals(defaultSelectedProjectPath, ESearchCase::IgnoreCase))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedProjectPath, defaultSelectedProjectPath);
	}

	TArray<FStartupScreenRecentProjectItem> items;
	const int32 visibleRecentProjectCount = FMath::Min(RecentProjectPaths.Num(), StartupScreenMaxVisibleRecentProjectCount);
	items.Reserve(visibleRecentProjectCount);
	for (int32 projectIndex = 0; projectIndex < visibleRecentProjectCount; ++projectIndex)
	{
		const FString& projectPath = RecentProjectPaths[projectIndex];
		const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
		FStartupScreenRecentProjectItem item;
		item.ProjectPath = normalizedProjectPath;
		item.Title = FPaths::GetCleanFilename(normalizedProjectPath);
		item.Subtitle = MakeStartupScreenProjectSubtitle(normalizedProjectPath);
		item.PreviewImagePath = ResolveStartupScreenPreviewPath(normalizedProjectPath);
		item.bSelected = normalizedProjectPath.Equals(SelectedProjectPath, ESearchCase::IgnoreCase);
		item.bEnabled = FPaths::DirectoryExists(normalizedProjectPath);
		items.Add(MoveTemp(item));
	}

	RecentProjects = MoveTemp(items);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecentProjects);
}
