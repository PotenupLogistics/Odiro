#include "Platform/ViewModel/StartupMenuViewModel.h"

#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"

namespace
{
	const TCHAR* StartupVmConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");
	const TCHAR* StartupVmParentFolderKey = TEXT("ParentFolder");
	const TCHAR* StartupVmProjectNameKey = TEXT("ProjectName");
	const TCHAR* StartupVmScenarioPresetIdKey = TEXT("ScenarioPresetId");
	const TCHAR* StartupVmProfilePresetIdKey = TEXT("ProfilePresetId");
	const TCHAR* StartupVmPolicyPresetIdKey = TEXT("PolicyPresetId");
	const TCHAR* StartupVmRecentProjectPathsKey = TEXT("RecentProjectPaths");
	const int32 StartupVmMaxRecentProjectCount = 8;
	const TCHAR* StartupVmDefaultProjectName = TEXT("OdiroProject");

	FString NormalizeStartupVmPath(FString path)
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

	FString NormalizeStartupVmProjectName(const FString& projectName)
	{
		return FPaths::GetCleanFilename(projectName.TrimStartAndEnd());
	}

	FString BuildStartupVmProjectPath(const FString& parentFolder, const FString& projectName)
	{
		const FString normalizedParentFolder = NormalizeStartupVmPath(parentFolder);
		const FString normalizedProjectName = NormalizeStartupVmProjectName(projectName);
		if (normalizedParentFolder.IsEmpty() || normalizedProjectName.IsEmpty())
		{
			return FString();
		}

		return NormalizeStartupVmPath(FPaths::Combine(normalizedParentFolder, normalizedProjectName));
	}

	FString MakeStartupVmPresetDisplayName(const FString& presetId)
	{
		FString displayName = presetId.TrimStartAndEnd();
		displayName.ReplaceInline(TEXT("-"), TEXT(" "));
		displayName.ReplaceInline(TEXT("_"), TEXT(" "));
		return FName::NameToDisplayString(displayName, false);
	}

	FString MakeStartupVmRecentProjectSubtitle(const FString& projectPath)
	{
		const FString parentFolderPath = FPaths::GetPath(NormalizeStartupVmPath(projectPath));
		const FString parentFolderName = FPaths::GetCleanFilename(parentFolderPath);
		return parentFolderName.IsEmpty() ? parentFolderPath : parentFolderName;
	}

	TArray<UOdiroListItemViewModel*> CopyStartupVmItems(const TArray<TObjectPtr<UOdiroListItemViewModel>>& sourceItems)
	{
		TArray<UOdiroListItemViewModel*> result;
		result.Reserve(sourceItems.Num());
		for (UOdiroListItemViewModel* item : sourceItems)
		{
			result.Add(item);
		}
		return result;
	}
}

void UStartupMenuViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	if (ProjectParentFolder.IsEmpty())
	{
		SetProjectParentFolder(NormalizeStartupVmPath(FPlatformProcess::UserDir()));
	}
	RefreshProjectPresets();
	RefreshRecentProjects();
}

void UStartupMenuViewModel::SetSubsystemOverrides(
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
	UProjectSessionSubsystem* projectSessionSubsystem,
	UScenarioEditorLaunchSubsystem* scenarioEditorLaunchSubsystem)
{
	SimulatorLaunchOverride = simulatorLaunchSubsystem;
	ProjectSessionOverride = projectSessionSubsystem;
	ScenarioEditorLaunchOverride = scenarioEditorLaunchSubsystem;
}

void UStartupMenuViewModel::SetProjectPathForPrototype(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupVmPath(projectPath);
	UE_MVVM_SET_PROPERTY_VALUE(ProjectPath, normalizedProjectPath);
	UE_MVVM_SET_PROPERTY_VALUE(ProjectParentFolder, NormalizeStartupVmPath(FPaths::GetPath(normalizedProjectPath)));
	UE_MVVM_SET_PROPERTY_VALUE(ProjectName, FPaths::GetCleanFilename(normalizedProjectPath));
	RefreshActionState();
}

FString UStartupMenuViewModel::GetProjectPathForPrototype() const
{
	return ProjectPath;
}

void UStartupMenuViewModel::SetProjectParentFolder(const FString& parentFolder)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectParentFolder, NormalizeStartupVmPath(parentFolder));
	UE_MVVM_SET_PROPERTY_VALUE(ProjectPath, BuildStartupVmProjectPath(ProjectParentFolder, ProjectName));
	RefreshActionState();
}

void UStartupMenuViewModel::SetProjectName(const FString& projectName)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectName, NormalizeStartupVmProjectName(projectName));
	UE_MVVM_SET_PROPERTY_VALUE(ProjectPath, BuildStartupVmProjectPath(ProjectParentFolder, ProjectName));
	RefreshActionState();
}

void UStartupMenuViewModel::SelectProjectPresets(
	const FString& scenarioPresetId,
	const FString& profilePresetId,
	const FString& policyPresetId)
{
	UE_MVVM_SET_PROPERTY_VALUE(
		SelectedScenarioPresetId,
		scenarioPresetId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("blank")) : scenarioPresetId.TrimStartAndEnd());
	UE_MVVM_SET_PROPERTY_VALUE(
		SelectedProfilePresetId,
		profilePresetId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("basic")) : profilePresetId.TrimStartAndEnd());
	UE_MVVM_SET_PROPERTY_VALUE(
		SelectedPolicyPresetId,
		policyPresetId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("blank")) : policyPresetId.TrimStartAndEnd());
	RebuildPresetItems();
}

void UStartupMenuViewModel::LoadProjectOpenOptions()
{
	FString parentFolder;
	FString projectName;
	FString scenarioPresetId;
	FString profilePresetId;
	FString policyPresetId;
	if (GConfig)
	{
		GConfig->GetString(StartupVmConfigSection, StartupVmParentFolderKey, parentFolder, GGameUserSettingsIni);
		GConfig->GetString(StartupVmConfigSection, StartupVmProjectNameKey, projectName, GGameUserSettingsIni);
		GConfig->GetString(StartupVmConfigSection, StartupVmScenarioPresetIdKey, scenarioPresetId, GGameUserSettingsIni);
		GConfig->GetString(StartupVmConfigSection, StartupVmProfilePresetIdKey, profilePresetId, GGameUserSettingsIni);
		GConfig->GetString(StartupVmConfigSection, StartupVmPolicyPresetIdKey, policyPresetId, GGameUserSettingsIni);
	}

	SetProjectParentFolder(parentFolder.TrimStartAndEnd().IsEmpty()
		? NormalizeStartupVmPath(FPlatformProcess::UserDir())
		: NormalizeStartupVmPath(parentFolder));
	SetProjectName(projectName.TrimStartAndEnd().IsEmpty()
		? FString(StartupVmDefaultProjectName)
		: NormalizeStartupVmProjectName(projectName));
	SelectProjectPresets(
		scenarioPresetId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("blank")) : scenarioPresetId.TrimStartAndEnd(),
		profilePresetId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("basic")) : profilePresetId.TrimStartAndEnd(),
		policyPresetId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("blank")) : policyPresetId.TrimStartAndEnd());
}

void UStartupMenuViewModel::SaveProjectOpenOptions() const
{
	if (!GConfig)
	{
		return;
	}

	const FProjectPresetSelection selection = GetSelectedPresetSelection();
	GConfig->SetString(StartupVmConfigSection, StartupVmParentFolderKey, *ProjectParentFolder, GGameUserSettingsIni);
	GConfig->SetString(StartupVmConfigSection, StartupVmProjectNameKey, *ProjectName, GGameUserSettingsIni);
	GConfig->SetString(StartupVmConfigSection, StartupVmScenarioPresetIdKey, *selection.ScenarioPresetId, GGameUserSettingsIni);
	GConfig->SetString(StartupVmConfigSection, StartupVmProfilePresetIdKey, *selection.ProfilePresetId, GGameUserSettingsIni);
	GConfig->SetString(StartupVmConfigSection, StartupVmPolicyPresetIdKey, *selection.PolicyPresetId, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

bool UStartupMenuViewModel::CreateProject(
	const FString& parentFolder,
	const FString& projectName,
	const FProjectPresetSelection& presets)
{
	SetProjectParentFolder(parentFolder);
	SetProjectName(projectName);
	SelectProjectPresets(presets.ScenarioPresetId, presets.ProfilePresetId, presets.PolicyPresetId);

	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetProjectOpenWarningText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString projectPath = GetProjectPathForPrototype();
	if (projectPath.IsEmpty())
	{
		SetProjectOpenWarningText(TEXT("Project parent/name 입력이 필요합니다."));
		return false;
	}
	if (UPlatformUiSubsystem::DoesResolvedDirectoryExist(projectPath))
	{
		SetProjectOpenWarningText(TEXT("이미 존재하는 프로젝트입니다."));
		return false;
	}
	if (UPlatformUiSubsystem::DoesResolvedFileExist(projectPath))
	{
		SetProjectOpenWarningText(TEXT("같은 이름의 파일이 있습니다."));
		return false;
	}

	TArray<FString> diagnostics;
	if (!subsystem->CreateProjectFromPresets(projectPath, GetSelectedPresetSelection(), diagnostics)
		|| !subsystem->ValidateUserProject(projectPath, diagnostics))
	{
		const FString message = diagnostics.IsEmpty() ? TEXT("Project 생성 실패.") : FString::Join(diagnostics, TEXT("\n"));
		SetProjectOpenWarningText(message);
		SetDiagnosticsText(message);
		return false;
	}

	RememberRecentProject(projectPath);
	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString::Printf(TEXT("Project created: %s"), *projectPath));
	return true;
}

bool UStartupMenuViewModel::OpenProject(const FString& projectPath)
{
	TArray<FString> diagnostics;
	if (!ValidateProject(projectPath, diagnostics))
	{
		return false;
	}

	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = ResolveSimulatorLaunchSubsystem();
	if (!simulatorLaunchSubsystem)
	{
		SetProjectOpenWarningText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupVmPath(projectPath);
	UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	UScenarioEditorLaunchSubsystem* scenarioEditorLaunch = ResolveScenarioEditorLaunchSubsystem();
	if (!projectSession || !scenarioEditorLaunch)
	{
		SetProjectOpenWarningText(TEXT("ProjectSessionSubsystem 또는 ScenarioEditorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	projectSession->SetActiveProjectPath(normalizedProjectPath);
	RememberRecentProject(normalizedProjectPath);
	SetProjectPathForPrototype(normalizedProjectPath);

	const FString scenarioPath = projectSession->GetActiveProjectScenarioPath();
	if (!scenarioEditorLaunch->OpenScenarioEditor(scenarioPath))
	{
		SetProjectOpenWarningText(TEXT("ScenarioEditorMap 열기 실패."));
		return false;
	}

	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString::Printf(TEXT("Project opened: %s"), *normalizedProjectPath));
	return true;
}

bool UStartupMenuViewModel::ValidateProject(const FString& projectPath, TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = ResolveSimulatorLaunchSubsystem();
	if (!simulatorLaunchSubsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		SetProjectOpenWarningText(outDiagnostics[0]);
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupVmPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("프로젝트를 선택하세요."));
		SetProjectOpenWarningText(outDiagnostics[0]);
		return false;
	}
	if (!UPlatformUiSubsystem::DoesResolvedDirectoryExist(normalizedProjectPath))
	{
		outDiagnostics.Add(TEXT("프로젝트 폴더가 없습니다."));
		SetProjectOpenWarningText(outDiagnostics[0]);
		return false;
	}
	if (!simulatorLaunchSubsystem->ValidateUserProject(normalizedProjectPath, outDiagnostics))
	{
		const FString message = outDiagnostics.IsEmpty() ? TEXT("프로젝트 검증 실패.") : FString::Join(outDiagnostics, TEXT("\n"));
		SetProjectOpenWarningText(message);
		SetDiagnosticsText(message);
		return false;
	}

	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString());
	return true;
}

bool UStartupMenuViewModel::AddRecentProjectIfValid(const FString& projectPath, TArray<FString>& outDiagnostics)
{
	if (!ValidateProject(projectPath, outDiagnostics))
	{
		return false;
	}

	RememberRecentProject(NormalizeStartupVmPath(projectPath));
	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString());
	return true;
}

bool UStartupMenuViewModel::RemoveRecentProject(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupVmPath(projectPath);
	const int32 removedCount = RecentProjectPaths.RemoveAll(
		[&normalizedProjectPath](const FString& storedPath)
		{
			return NormalizeStartupVmPath(storedPath).Equals(normalizedProjectPath, ESearchCase::IgnoreCase);
		});
	if (removedCount <= 0)
	{
		return false;
	}

	SaveRecentProjectPaths();
	RebuildRecentProjectItems();
	return true;
}

void UStartupMenuViewModel::RefreshRecentProjects()
{
	LoadRecentProjectPaths();
	PruneMissingRecentProjects();
	RebuildRecentProjectItems();
	RefreshActionState();
}

void UStartupMenuViewModel::RefreshProjectPresets()
{
	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		PresetCatalog = subsystem->ListProjectPresets();
	}
	else
	{
		PresetCatalog = FProjectPresetCatalog();
		PresetCatalog.ScenarioPresetIds = { TEXT("blank") };
		PresetCatalog.ProfilePresetIds = { TEXT("basic") };
		PresetCatalog.PolicyPresetIds = { TEXT("blank") };
	}

	RebuildPresetItems();
}

TArray<UOdiroListItemViewModel*> UStartupMenuViewModel::GetRecentProjectItems() const
{
	return CopyStartupVmItems(RecentProjectItems);
}

TArray<UOdiroListItemViewModel*> UStartupMenuViewModel::GetScenarioPresetItems() const
{
	return CopyStartupVmItems(ScenarioPresetItems);
}

TArray<UOdiroListItemViewModel*> UStartupMenuViewModel::GetProfilePresetItems() const
{
	return CopyStartupVmItems(ProfilePresetItems);
}

TArray<UOdiroListItemViewModel*> UStartupMenuViewModel::GetPolicyPresetItems() const
{
	return CopyStartupVmItems(PolicyPresetItems);
}

FProjectPresetSelection UStartupMenuViewModel::GetSelectedProjectPresetSelection() const
{
	return GetSelectedPresetSelection();
}

USimulatorLaunchSubsystem* UStartupMenuViewModel::ResolveSimulatorLaunchSubsystem() const
{
	if (SimulatorLaunchOverride)
	{
		return SimulatorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UStartupMenuViewModel::ResolveProjectSessionSubsystem() const
{
	if (ProjectSessionOverride)
	{
		return ProjectSessionOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

UScenarioEditorLaunchSubsystem* UStartupMenuViewModel::ResolveScenarioEditorLaunchSubsystem() const
{
	if (ScenarioEditorLaunchOverride)
	{
		return ScenarioEditorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
}

void UStartupMenuViewModel::LoadRecentProjectPaths()
{
	TArray<FString> storedPaths;
	if (GConfig)
	{
		GConfig->GetArray(StartupVmConfigSection, StartupVmRecentProjectPathsKey, storedPaths, GGameUserSettingsIni);
	}

	TArray<FString> normalizedPaths;
	for (const FString& storedPath : storedPaths)
	{
		const FString normalizedPath = NormalizeStartupVmPath(storedPath);
		if (normalizedPath.IsEmpty())
		{
			continue;
		}
		normalizedPaths.RemoveAll(
			[&normalizedPath](const FString& existingPath)
			{
				return NormalizeStartupVmPath(existingPath).Equals(normalizedPath, ESearchCase::IgnoreCase);
			});
		normalizedPaths.Add(normalizedPath);
	}

	RecentProjectPaths = normalizedPaths;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecentProjectPaths);
}

void UStartupMenuViewModel::SaveRecentProjectPaths() const
{
	if (GConfig)
	{
		GConfig->SetArray(StartupVmConfigSection, StartupVmRecentProjectPathsKey, RecentProjectPaths, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UStartupMenuViewModel::RememberRecentProject(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupVmPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		return;
	}

	RecentProjectPaths.RemoveAll(
		[&normalizedProjectPath](const FString& storedPath)
		{
			return NormalizeStartupVmPath(storedPath).Equals(normalizedProjectPath, ESearchCase::IgnoreCase);
		});
	RecentProjectPaths.Insert(normalizedProjectPath, 0);
	if (RecentProjectPaths.Num() > StartupVmMaxRecentProjectCount)
	{
		RecentProjectPaths.SetNum(StartupVmMaxRecentProjectCount);
	}

	SaveRecentProjectPaths();
	RebuildRecentProjectItems();
}

bool UStartupMenuViewModel::PruneMissingRecentProjects()
{
	const int32 oldCount = RecentProjectPaths.Num();
	RecentProjectPaths.RemoveAll(
		[](const FString& projectPath)
		{
			return !UPlatformUiSubsystem::DoesResolvedDirectoryExist(NormalizeStartupVmPath(projectPath));
		});

	if (RecentProjectPaths.Num() == oldCount)
	{
		return false;
	}

	SaveRecentProjectPaths();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecentProjectPaths);
	return true;
}

void UStartupMenuViewModel::RebuildRecentProjectItems()
{
	RecentProjectItems.Reset();
	RecentProjectItems.Reserve(RecentProjectPaths.Num());
	for (const FString& projectPath : RecentProjectPaths)
	{
		UOdiroListItemViewModel* item = NewObject<UOdiroListItemViewModel>(this);
		item->InitializeItem(
			projectPath,
			FPaths::GetCleanFilename(projectPath),
			MakeStartupVmRecentProjectSubtitle(projectPath),
			projectPath);
		item->SetEnabled(UPlatformUiSubsystem::DoesResolvedDirectoryExist(projectPath));
		RecentProjectItems.Add(item);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecentProjectItems);
}

void UStartupMenuViewModel::RebuildPresetItems()
{
	const auto buildItems =
		[this](const TArray<FString>& presetIds, const FString& selectedId, TArray<TObjectPtr<UOdiroListItemViewModel>>& outItems)
		{
			outItems.Reset();
			outItems.Reserve(presetIds.Num());
			for (const FString& presetId : presetIds)
			{
				UOdiroListItemViewModel* item = NewObject<UOdiroListItemViewModel>(this);
				item->InitializeItem(presetId, MakeStartupVmPresetDisplayName(presetId), presetId, presetId);
				item->SetSelected(presetId.Equals(selectedId, ESearchCase::IgnoreCase));
				outItems.Add(item);
			}
		};

	buildItems(PresetCatalog.ScenarioPresetIds, SelectedScenarioPresetId, ScenarioPresetItems);
	buildItems(PresetCatalog.ProfilePresetIds, SelectedProfilePresetId, ProfilePresetItems);
	buildItems(PresetCatalog.PolicyPresetIds, SelectedPolicyPresetId, PolicyPresetItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ScenarioPresetItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ProfilePresetItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PolicyPresetItems);
}

void UStartupMenuViewModel::RefreshActionState()
{
	const bool bHasCreatePath = !ProjectParentFolder.IsEmpty() && !ProjectName.IsEmpty() && !ProjectPath.IsEmpty();
	const bool bCreatePathAvailable = bHasCreatePath
		&& !UPlatformUiSubsystem::DoesResolvedDirectoryExist(ProjectPath)
		&& !UPlatformUiSubsystem::DoesResolvedFileExist(ProjectPath);
	UE_MVVM_SET_PROPERTY_VALUE(bCanCreateProject, bCreatePathAvailable);
	UE_MVVM_SET_PROPERTY_VALUE(bCanOpenProject, !ProjectPath.IsEmpty());
}

void UStartupMenuViewModel::SetProjectOpenWarningText(const FString& message)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectOpenWarningText, message);
}

FProjectPresetSelection UStartupMenuViewModel::GetSelectedPresetSelection() const
{
	FProjectPresetSelection selection;
	selection.ScenarioPresetId = SelectedScenarioPresetId;
	selection.ProfilePresetId = SelectedProfilePresetId;
	selection.PolicyPresetId = SelectedPolicyPresetId;
	return selection;
}
