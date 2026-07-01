#include "Platform/ViewModel/ProjectCreateScreenViewModel.h"

#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"

namespace
{
	const TCHAR* ProjectCreateConfigSection = TEXT("OdiroSim.Platform.ProjectOpen");
	const TCHAR* ProjectCreateLegacyConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");
	const TCHAR* ProjectCreateParentFolderKey = TEXT("ParentFolder");
	const TCHAR* ProjectCreateNameKey = TEXT("ProjectName");
	const TCHAR* ProjectCreateScenarioPresetIdKey = TEXT("ScenarioPresetId");
	const TCHAR* ProjectCreateProfilePresetIdKey = TEXT("ProfilePresetId");
	const TCHAR* ProjectCreatePolicyPresetIdKey = TEXT("PolicyPresetId");
	const TCHAR* ProjectCreateRecentProjectPathsKey = TEXT("RecentProjectPaths");
	const int32 ProjectCreateMaxRecentProjectCount = 8;

	// 사용자 입력 project path를 absolute normalized path로 맞춘다.
	FString NormalizeProjectCreatePath(FString path)
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

	// Project create dialog의 기본 parent folder를 반환한다.
	FString GetDefaultProjectCreateParentFolder()
	{
		return NormalizeProjectCreatePath(FPlatformProcess::UserDir());
	}

	// 폴더명으로 쓸 수 있도록 project name에서 path segment만 남긴다.
	FString NormalizeProjectDirectoryName(const FString& projectName)
	{
		return FPaths::GetCleanFilename(projectName.TrimStartAndEnd());
	}

	// Parent folder와 project name에서 생성 대상 root를 만든다.
	FString BuildProjectCreatePath(const FString& parentFolder, const FString& projectName)
	{
		const FString normalizedParentFolder = NormalizeProjectCreatePath(parentFolder);
		const FString normalizedProjectName = NormalizeProjectDirectoryName(projectName);
		if (normalizedParentFolder.IsEmpty() || normalizedProjectName.IsEmpty())
		{
			return FString();
		}

		return NormalizeProjectCreatePath(FPaths::Combine(normalizedParentFolder, normalizedProjectName));
	}

	// Catalog에 preset id가 존재하는지 확인한다.
	bool ContainsPresetId(const TArray<FProjectPresetInfo>& presetInfos, const FString& presetId)
	{
		return presetInfos.ContainsByPredicate(
			[&presetId](const FProjectPresetInfo& presetInfo)
			{
				return presetInfo.Id.Equals(presetId, ESearchCase::IgnoreCase);
			});
	}

	// Simulator preset kind를 Project Create 화면 category로 변환한다.
	EProjectCreatePresetCategory ToProjectCreateCategory(const EProjectPresetKind kind)
	{
		switch (kind)
		{
		case EProjectPresetKind::Scenario:
			return EProjectCreatePresetCategory::Scenario;
		case EProjectPresetKind::Profile:
			return EProjectCreatePresetCategory::Profile;
		case EProjectPresetKind::Policy:
			return EProjectCreatePresetCategory::Policy;
		default:
			return EProjectCreatePresetCategory::Scenario;
		}
	}

	// Catalog descriptor를 Project Create card item으로 변환한다.
	FProjectCreatePresetItem MakeProjectCreatePresetItem(
		const FProjectPresetInfo& presetInfo,
		const FString& selectedPresetId)
	{
		FProjectCreatePresetItem item;
		item.Category = ToProjectCreateCategory(presetInfo.Kind);
		item.PresetId = presetInfo.Id;
		item.Title = presetInfo.Title.TrimStartAndEnd().IsEmpty() ? presetInfo.Id : presetInfo.Title;
		item.Subtitle = presetInfo.Subtitle;
		item.ThumbnailPath = presetInfo.ThumbnailPath;
		item.bSelected = presetInfo.Id.Equals(selectedPresetId, ESearchCase::IgnoreCase);
		return item;
	}

	// Project Create surface shows the reference-sized starter set while preserving a stored custom selection.
	TArray<FProjectCreatePresetItem> BuildVisibleProjectCreatePresetItems(
		const TArray<FProjectPresetInfo>& presetInfos,
		const FString& selectedPresetId,
		const TArray<FString>& preferredPresetIds)
	{
		TArray<FProjectCreatePresetItem> items;
		TSet<FString> addedPresetIds;
		auto addPresetById = [&items, &addedPresetIds, &presetInfos, &selectedPresetId](const FString& presetId)
		{
			if (presetId.TrimStartAndEnd().IsEmpty() || addedPresetIds.Contains(presetId))
			{
				return;
			}

			const FProjectPresetInfo* presetInfo = presetInfos.FindByPredicate(
				[&presetId](const FProjectPresetInfo& candidate)
				{
					return candidate.Id.Equals(presetId, ESearchCase::IgnoreCase);
				});
			if (!presetInfo)
			{
				return;
			}

			items.Add(MakeProjectCreatePresetItem(*presetInfo, selectedPresetId));
			addedPresetIds.Add(presetInfo->Id);
		};

		for (const FString& presetId : preferredPresetIds)
		{
			addPresetById(presetId);
		}
		addPresetById(selectedPresetId);
		return items;
	}

	// WBP가 제공한 진단 문구가 있을 때만 outDiagnostics와 UI text에 반영한다.
	FString ResolveProjectCreateDiagnosticMessage(
		const TArray<FString>& diagnostics,
		const FString& fallbackMessage)
	{
		return diagnostics.IsEmpty()
			? fallbackMessage.TrimStartAndEnd()
			: FString::Join(diagnostics, TEXT("\n"));
	}

	// 성공 문구 template에 project path placeholder를 반영한다.
	FString ExpandProjectCreateMessage(const FString& messageTemplate, const FString& projectPath)
	{
		FString message = messageTemplate.TrimStartAndEnd();
		message.ReplaceInline(TEXT("{ProjectPath}"), *projectPath, ESearchCase::CaseSensitive);
		message.ReplaceInline(TEXT("{0}"), *projectPath, ESearchCase::CaseSensitive);
		return message;
	}

	bool GetProjectCreateConfigString(const TCHAR* key, FString& outValue)
	{
		outValue.Reset();
		if (!GConfig)
		{
			return false;
		}

		if (GConfig->GetString(ProjectCreateConfigSection, key, outValue, GGameUserSettingsIni))
		{
			return true;
		}

		return GConfig->GetString(ProjectCreateLegacyConfigSection, key, outValue, GGameUserSettingsIni);
	}

	void GetProjectCreateRecentProjectPaths(TArray<FString>& outRecentProjectPaths)
	{
		outRecentProjectPaths.Reset();
		if (!GConfig)
		{
			return;
		}

		const int32 loadedCount = GConfig->GetArray(
			ProjectCreateConfigSection,
			ProjectCreateRecentProjectPathsKey,
			outRecentProjectPaths,
			GGameUserSettingsIni);
		if (loadedCount <= 0 || outRecentProjectPaths.IsEmpty())
		{
			GConfig->GetArray(
				ProjectCreateLegacyConfigSection,
				ProjectCreateRecentProjectPathsKey,
				outRecentProjectPaths,
				GGameUserSettingsIni);
		}
	}
}

void UProjectCreateScreenViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	Refresh();
}

void UProjectCreateScreenViewModel::SetSubsystemOverrides(
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
	UProjectSessionSubsystem* projectSessionSubsystem)
{
	SimulatorLaunchOverride = simulatorLaunchSubsystem;
	ProjectSessionOverride = projectSessionSubsystem;
}

void UProjectCreateScreenViewModel::Refresh()
{
	LoadProjectCreateOptions();
	RefreshProjectPresets();
	RefreshProjectPath();
	RefreshActionState();
}

void UProjectCreateScreenViewModel::SetProjectName(const FString& projectName)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectName, NormalizeProjectDirectoryName(projectName));
	RefreshProjectPath();
	RefreshActionState();
}

void UProjectCreateScreenViewModel::SetProjectParentFolder(const FString& projectParentFolder)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectParentFolder, NormalizeProjectCreatePath(projectParentFolder));
	RefreshProjectPath();
	RefreshActionState();
}

void UProjectCreateScreenViewModel::SelectPreset(
	const EProjectCreatePresetCategory category,
	const FString& presetId)
{
	const FString normalizedPresetId = presetId.TrimStartAndEnd();
	if (normalizedPresetId.IsEmpty())
	{
		return;
	}

	switch (category)
	{
	case EProjectCreatePresetCategory::Scenario:
		if (ContainsPresetId(PresetCatalog.ScenarioPresets, normalizedPresetId))
		{
			UE_MVVM_SET_PROPERTY_VALUE(SelectedScenarioPresetId, normalizedPresetId);
		}
		break;
	case EProjectCreatePresetCategory::Profile:
		if (ContainsPresetId(PresetCatalog.ProfilePresets, normalizedPresetId))
		{
			UE_MVVM_SET_PROPERTY_VALUE(SelectedProfilePresetId, normalizedPresetId);
		}
		break;
	case EProjectCreatePresetCategory::Policy:
		if (ContainsPresetId(PresetCatalog.PolicyPresets, normalizedPresetId))
		{
			UE_MVVM_SET_PROPERTY_VALUE(SelectedPolicyPresetId, normalizedPresetId);
		}
		break;
	default:
		break;
	}

	RebuildPresetItems();
	SaveProjectCreateOptions();
	ClearDiagnostics();
}

bool UProjectCreateScreenViewModel::CreateProject(TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	RefreshProjectPath();
	RefreshActionState();

	if (!CanCreateProject())
	{
		const FString message = DiagnosticMessages.ProjectPathInvalid.TrimStartAndEnd();
		if (!message.IsEmpty())
		{
			outDiagnostics.Add(message);
		}
		SetDiagnosticsText(message);
		return false;
	}

	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = ResolveSimulatorLaunchSubsystem();
	UProjectSessionSubsystem* projectSessionSubsystem = ResolveProjectSessionSubsystem();
	if (!simulatorLaunchSubsystem || !projectSessionSubsystem)
	{
		const FString message = DiagnosticMessages.SubsystemUnavailable.TrimStartAndEnd();
		if (!message.IsEmpty())
		{
			outDiagnostics.Add(message);
		}
		SetDiagnosticsText(message);
		return false;
	}

	SetBusy(true);
	const FProjectPresetSelection presetSelection = GetSelectedPresetSelection();
	const bool bCreated = simulatorLaunchSubsystem->CreateProjectFromPresets(
		ProjectPath,
		presetSelection,
		outDiagnostics);
	if (!bCreated)
	{
		SetBusy(false);
		SetDiagnosticsText(ResolveProjectCreateDiagnosticMessage(outDiagnostics, DiagnosticMessages.ProjectCreateFailed));
		RefreshActionState();
		return false;
	}

	if (!simulatorLaunchSubsystem->ValidateUserProject(ProjectPath, outDiagnostics))
	{
		SetBusy(false);
		SetDiagnosticsText(ResolveProjectCreateDiagnosticMessage(outDiagnostics, DiagnosticMessages.CreatedProjectValidationFailed));
		RefreshActionState();
		return false;
	}

	projectSessionSubsystem->SetActiveProjectPath(ProjectPath);
	RememberRecentProject(ProjectPath);
	SaveProjectCreateOptions();
	SetDiagnosticsText(ExpandProjectCreateMessage(DiagnosticMessages.ProjectCreatedFormat, ProjectPath));
	SetBusy(false);
	RefreshActionState();
	return true;
}

FProjectPresetSelection UProjectCreateScreenViewModel::GetSelectedPresetSelection() const
{
	FProjectPresetSelection selection;
	const FProjectPresetSelection defaultSelection;
	selection.ScenarioPresetId = SelectedScenarioPresetId.TrimStartAndEnd().IsEmpty()
		? defaultSelection.ScenarioPresetId
		: SelectedScenarioPresetId.TrimStartAndEnd();
	selection.ProfilePresetId = SelectedProfilePresetId.TrimStartAndEnd().IsEmpty()
		? defaultSelection.ProfilePresetId
		: SelectedProfilePresetId.TrimStartAndEnd();
	selection.PolicyPresetId = SelectedPolicyPresetId.TrimStartAndEnd().IsEmpty()
		? defaultSelection.PolicyPresetId
		: SelectedPolicyPresetId.TrimStartAndEnd();
	return selection;
}

FString UProjectCreateScreenViewModel::GetSelectedScenarioSummary() const
{
	return GetSelectedPresetSelection().ScenarioPresetId + TEXT(".json");
}

FString UProjectCreateScreenViewModel::GetSelectedProfileSummary() const
{
	return GetSelectedPresetSelection().ProfilePresetId + TEXT(".json");
}

FString UProjectCreateScreenViewModel::GetSelectedPolicySummary() const
{
	return FString::Printf(TEXT("policy/%s"), *GetSelectedPresetSelection().PolicyPresetId);
}

void UProjectCreateScreenViewModel::SetDiagnosticsText(const FString& message)
{
	UE_MVVM_SET_PROPERTY_VALUE(DiagnosticsText, message);
}

void UProjectCreateScreenViewModel::SetDiagnosticMessages(const FProjectCreateScreenDiagnosticMessages& messages)
{
	DiagnosticMessages = messages;
}

void UProjectCreateScreenViewModel::SetDefaultValues(const FProjectCreateScreenDefaultValues& values)
{
	DefaultValues = values;
}

void UProjectCreateScreenViewModel::ClearDiagnostics()
{
	SetDiagnosticsText(FString());
}

USimulatorLaunchSubsystem* UProjectCreateScreenViewModel::ResolveSimulatorLaunchSubsystem() const
{
	if (SimulatorLaunchOverride)
	{
		return SimulatorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UProjectCreateScreenViewModel::ResolveProjectSessionSubsystem() const
{
	if (ProjectSessionOverride)
	{
		return ProjectSessionOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

void UProjectCreateScreenViewModel::LoadProjectCreateOptions()
{
	FString storedParentFolder;
	FString storedProjectName;
	FString storedScenarioPresetId;
	FString storedProfilePresetId;
	FString storedPolicyPresetId;
	GetProjectCreateConfigString(ProjectCreateParentFolderKey, storedParentFolder);
	GetProjectCreateConfigString(ProjectCreateNameKey, storedProjectName);
	GetProjectCreateConfigString(ProjectCreateScenarioPresetIdKey, storedScenarioPresetId);
	GetProjectCreateConfigString(ProjectCreateProfilePresetIdKey, storedProfilePresetId);
	GetProjectCreateConfigString(ProjectCreatePolicyPresetIdKey, storedPolicyPresetId);

	const FProjectPresetSelection defaultSelection;
	const FString defaultParentFolder = NormalizeProjectCreatePath(DefaultValues.ProjectParentFolder);
	const FString defaultProjectName = NormalizeProjectDirectoryName(DefaultValues.ProjectName);
	UE_MVVM_SET_PROPERTY_VALUE(ProjectParentFolder, storedParentFolder.TrimStartAndEnd().IsEmpty()
		? (defaultParentFolder.IsEmpty() ? GetDefaultProjectCreateParentFolder() : defaultParentFolder)
		: NormalizeProjectCreatePath(storedParentFolder));
	UE_MVVM_SET_PROPERTY_VALUE(ProjectName, storedProjectName.TrimStartAndEnd().IsEmpty()
		? defaultProjectName
		: NormalizeProjectDirectoryName(storedProjectName));
	UE_MVVM_SET_PROPERTY_VALUE(SelectedScenarioPresetId, storedScenarioPresetId.TrimStartAndEnd().IsEmpty()
		? defaultSelection.ScenarioPresetId
		: storedScenarioPresetId.TrimStartAndEnd());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedProfilePresetId, storedProfilePresetId.TrimStartAndEnd().IsEmpty()
		? defaultSelection.ProfilePresetId
		: storedProfilePresetId.TrimStartAndEnd());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedPolicyPresetId, storedPolicyPresetId.TrimStartAndEnd().IsEmpty()
		? defaultSelection.PolicyPresetId
		: storedPolicyPresetId.TrimStartAndEnd());
}

void UProjectCreateScreenViewModel::SaveProjectCreateOptions() const
{
	if (!GConfig)
	{
		return;
	}

	GConfig->SetString(ProjectCreateConfigSection, ProjectCreateParentFolderKey, *ProjectParentFolder, GGameUserSettingsIni);
	GConfig->SetString(ProjectCreateConfigSection, ProjectCreateNameKey, *ProjectName, GGameUserSettingsIni);
	GConfig->SetString(ProjectCreateConfigSection, ProjectCreateScenarioPresetIdKey, *SelectedScenarioPresetId, GGameUserSettingsIni);
	GConfig->SetString(ProjectCreateConfigSection, ProjectCreateProfilePresetIdKey, *SelectedProfilePresetId, GGameUserSettingsIni);
	GConfig->SetString(ProjectCreateConfigSection, ProjectCreatePolicyPresetIdKey, *SelectedPolicyPresetId, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UProjectCreateScreenViewModel::RememberRecentProject(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeProjectCreatePath(projectPath);
	if (normalizedProjectPath.IsEmpty() || !GConfig)
	{
		return;
	}

	TArray<FString> recentProjectPaths;
	GetProjectCreateRecentProjectPaths(recentProjectPaths);

	TArray<FString> normalizedRecentProjectPaths;
	for (const FString& recentProjectPath : recentProjectPaths)
	{
		const FString normalizedRecentProjectPath = NormalizeProjectCreatePath(recentProjectPath);
		if (!normalizedRecentProjectPath.IsEmpty()
			&& !normalizedRecentProjectPath.Equals(normalizedProjectPath, ESearchCase::IgnoreCase))
		{
			normalizedRecentProjectPaths.Add(normalizedRecentProjectPath);
		}
	}

	normalizedRecentProjectPaths.Insert(normalizedProjectPath, 0);
	if (normalizedRecentProjectPaths.Num() > ProjectCreateMaxRecentProjectCount)
	{
		normalizedRecentProjectPaths.SetNum(ProjectCreateMaxRecentProjectCount);
	}

	GConfig->SetArray(
		ProjectCreateConfigSection,
		ProjectCreateRecentProjectPathsKey,
		normalizedRecentProjectPaths,
		GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UProjectCreateScreenViewModel::RefreshProjectPresets()
{
	if (USimulatorLaunchSubsystem* simulatorLaunchSubsystem = ResolveSimulatorLaunchSubsystem())
	{
		PresetCatalog = simulatorLaunchSubsystem->ListProjectPresets();
	}
	else
	{
		PresetCatalog = USimulatorLaunchSubsystem::LoadProjectPresetCatalog();
	}

	PresetCatalog.ScenarioPresetIds.Empty();
	for (const FProjectPresetInfo& presetInfo : PresetCatalog.ScenarioPresets)
	{
		PresetCatalog.ScenarioPresetIds.Add(presetInfo.Id);
	}
	PresetCatalog.ProfilePresetIds.Empty();
	for (const FProjectPresetInfo& presetInfo : PresetCatalog.ProfilePresets)
	{
		PresetCatalog.ProfilePresetIds.Add(presetInfo.Id);
	}
	PresetCatalog.PolicyPresetIds.Empty();
	for (const FProjectPresetInfo& presetInfo : PresetCatalog.PolicyPresets)
	{
		PresetCatalog.PolicyPresetIds.Add(presetInfo.Id);
	}

	const FProjectPresetSelection defaultSelection;
	if (!ContainsPresetId(PresetCatalog.ScenarioPresets, SelectedScenarioPresetId))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedScenarioPresetId, PresetCatalog.ScenarioPresets.IsEmpty()
			? defaultSelection.ScenarioPresetId
			: PresetCatalog.ScenarioPresets[0].Id);
	}
	if (!ContainsPresetId(PresetCatalog.ProfilePresets, SelectedProfilePresetId))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedProfilePresetId, PresetCatalog.ProfilePresets.IsEmpty()
			? defaultSelection.ProfilePresetId
			: PresetCatalog.ProfilePresets[0].Id);
	}
	if (!ContainsPresetId(PresetCatalog.PolicyPresets, SelectedPolicyPresetId))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedPolicyPresetId, PresetCatalog.PolicyPresets.IsEmpty()
			? defaultSelection.PolicyPresetId
			: PresetCatalog.PolicyPresets[0].Id);
	}

	RebuildPresetItems();
}

void UProjectCreateScreenViewModel::RebuildPresetItems()
{
	TArray<FProjectCreatePresetItem> scenarioPresetItems = BuildVisibleProjectCreatePresetItems(
		PresetCatalog.ScenarioPresets,
		SelectedScenarioPresetId,
		{ TEXT("blank"), TEXT("demo"), TEXT("s-curve") });

	TArray<FProjectCreatePresetItem> profilePresetItems = BuildVisibleProjectCreatePresetItems(
		PresetCatalog.ProfilePresets,
		SelectedProfilePresetId,
		{ TEXT("basic"), TEXT("full") });

	TArray<FProjectCreatePresetItem> policyPresetItems = BuildVisibleProjectCreatePresetItems(
		PresetCatalog.PolicyPresets,
		SelectedPolicyPresetId,
		{ TEXT("blank"), TEXT("demo") });

	ScenarioPresetItems = MoveTemp(scenarioPresetItems);
	ProfilePresetItems = MoveTemp(profilePresetItems);
	PolicyPresetItems = MoveTemp(policyPresetItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ScenarioPresetItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ProfilePresetItems);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PolicyPresetItems);
}

void UProjectCreateScreenViewModel::RefreshProjectPath()
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectPath, BuildProjectCreatePath(ProjectParentFolder, ProjectName));
}

void UProjectCreateScreenViewModel::RefreshActionState()
{
	const bool bHasCreatePath = !ProjectParentFolder.IsEmpty() && !ProjectName.IsEmpty() && !ProjectPath.IsEmpty();
	const bool bCreatePathAvailable = bHasCreatePath
		&& !FPaths::DirectoryExists(ProjectPath)
		&& !FPaths::FileExists(ProjectPath);
	UE_MVVM_SET_PROPERTY_VALUE(bCanCreateProject, bCreatePathAvailable && !bBusy);
}

void UProjectCreateScreenViewModel::SetBusy(const bool bInBusy)
{
	UE_MVVM_SET_PROPERTY_VALUE(bBusy, bInBusy);
	RefreshActionState();
}
