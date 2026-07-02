#include "Platform/ViewModel/StartupScreenViewModel.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* StartupScreenConfigSection = TEXT("OdiroSim.Platform.ProjectOpen");
	const TCHAR* StartupScreenLegacyConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");
	const TCHAR* StartupScreenRecentProjectPathsKey = TEXT("RecentProjectPaths");
	const TCHAR* StartupScreenProjectSettingFileName = TEXT("setting.json");
	const TCHAR* StartupScreenProjectIdFieldName = TEXT("project_id");
	const int32 StartupScreenMaxRecentProjectCount = 8;
	// StartupScreen에 동시에 표시할 최근 project card 수.
	const int32 StartupScreenMaxVisibleRecentProjectCount = 3;

	// SimulatorLaunchSubsystem validation diagnostics에서 missing file 이름을 추출하기 위한 표시 규칙.
	struct FStartupScreenProjectFileDiagnosticRule
	{
		const TCHAR* ConfigName;
		const TCHAR* MissingPrefix;
		bool bCoreConfig;
	};

	const FStartupScreenProjectFileDiagnosticRule StartupScreenProjectFileDiagnosticRules[] = {
		{ TEXT("setting.json"), TEXT("setting.json missing:"), true },
		{ TEXT("profile.json"), TEXT("profile.json missing:"), true },
		{ TEXT("scenario.json"), TEXT("scenario.json missing:"), true },
		{ TEXT("policy"), TEXT("policy directory missing:"), false },
		{ TEXT("policy/__init__.py"), TEXT("policy entrypoint missing:"), false },
		{ TEXT("runs"), TEXT("runs directory missing:"), false },
	};

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

	// 최근 project 카드의 fallback/보조 표시용 project folder 이름을 만든다.
	FString ResolveStartupScreenProjectFolderName(const FString& projectPath)
	{
		const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
		const FString folderName = FPaths::GetCleanFilename(normalizedProjectPath);
		return folderName.IsEmpty() ? normalizedProjectPath : folderName;
	}

	// 최근 project 카드의 주 표시 이름으로 쓸 setting.json project_id를 읽는다.
	FString ResolveStartupScreenProjectId(const FString& projectPath)
	{
		const FString normalizedProjectPath = NormalizeStartupScreenPath(projectPath);
		const FString fallbackProjectId = ResolveStartupScreenProjectFolderName(normalizedProjectPath);
		if (normalizedProjectPath.IsEmpty())
		{
			return FString();
		}

		const FString settingPath = NormalizeStartupScreenPath(FPaths::Combine(
			normalizedProjectPath,
			StartupScreenProjectSettingFileName));
		FString settingJson;
		if (!FFileHelper::LoadFileToString(settingJson, *settingPath))
		{
			return fallbackProjectId;
		}

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(settingJson);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			return fallbackProjectId;
		}

		FString projectId;
		rootObject->TryGetStringField(StartupScreenProjectIdFieldName, projectId);
		projectId.TrimStartAndEndInline();
		return projectId.IsEmpty() ? fallbackProjectId : projectId;
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

	// Footer diagnostics에 저장할 첫 번째 유효 단일 행 메시지를 고른다.
	FString ExtractFirstStartupScreenDiagnosticLine(const FString& message)
	{
		FString normalizedMessage = message;
		normalizedMessage.ReplaceInline(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		normalizedMessage.ReplaceInline(TEXT("\r"), TEXT("\n"), ESearchCase::CaseSensitive);

		TArray<FString> lines;
		normalizedMessage.ParseIntoArray(lines, TEXT("\n"), true);
		for (FString& line : lines)
		{
			line.TrimStartAndEndInline();
			if (!line.IsEmpty())
			{
				return line;
			}
		}

		return FString();
	}

	FString ResolveStartupScreenDiagnosticMessage(
		const TArray<FString>& diagnostics,
		const FString& fallbackMessage);

	// 진단 문구 template에 누락 project file placeholder를 반영한다.
	FString ExpandStartupScreenConfigMissingMessage(
		const FString& messageTemplate,
		const FString& configName)
	{
		FString message = messageTemplate.TrimStartAndEnd();
		message.ReplaceInline(TEXT("{ConfigName}"), *configName, ESearchCase::CaseSensitive);
		message.ReplaceInline(TEXT("{0}"), *configName, ESearchCase::CaseSensitive);
		return ExtractFirstStartupScreenDiagnosticLine(message);
	}

	// SimulatorLaunchSubsystem missing diagnostic의 대상 project file 이름을 찾는다.
	const FStartupScreenProjectFileDiagnosticRule* FindStartupScreenMissingProjectFileRule(
		const FString& diagnostic)
	{
		const FString diagnosticLine = ExtractFirstStartupScreenDiagnosticLine(diagnostic);
		for (const FStartupScreenProjectFileDiagnosticRule& rule : StartupScreenProjectFileDiagnosticRules)
		{
			if (diagnosticLine.StartsWith(rule.MissingPrefix, ESearchCase::CaseSensitive))
			{
				return &rule;
			}
		}

		return nullptr;
	}

	// Project validation diagnostics를 StartupScreen용 단일 행 사용자 문구로 바꾼다.
	FString ResolveStartupScreenProjectValidationMessage(
		const TArray<FString>& diagnostics,
		const FStartupScreenDiagnosticMessages& diagnosticMessages)
	{
		TArray<FString> missingCoreConfigNames;
		TArray<FString> missingProjectFileNames;
		for (const FString& diagnostic : diagnostics)
		{
			const FStartupScreenProjectFileDiagnosticRule* rule = FindStartupScreenMissingProjectFileRule(diagnostic);
			if (!rule)
			{
				continue;
			}

			const FString configName(rule->ConfigName);
			missingProjectFileNames.AddUnique(configName);
			if (rule->bCoreConfig)
			{
				missingCoreConfigNames.AddUnique(configName);
			}
		}

		int32 requiredCoreConfigCount = 0;
		for (const FStartupScreenProjectFileDiagnosticRule& rule : StartupScreenProjectFileDiagnosticRules)
		{
			if (rule.bCoreConfig)
			{
				++requiredCoreConfigCount;
			}
		}

		if (requiredCoreConfigCount > 0 && missingCoreConfigNames.Num() == requiredCoreConfigCount)
		{
			const FString notProjectMessage =
				ExtractFirstStartupScreenDiagnosticLine(diagnosticMessages.ProjectFolderNotProject);
			if (!notProjectMessage.IsEmpty())
			{
				return notProjectMessage;
			}
		}

		if (!missingProjectFileNames.IsEmpty())
		{
			const FString missingConfigMessage = ExpandStartupScreenConfigMissingMessage(
				diagnosticMessages.ProjectConfigMissingFormat,
				missingProjectFileNames[0]);
			if (!missingConfigMessage.IsEmpty())
			{
				return missingConfigMessage;
			}
		}

		return ResolveStartupScreenDiagnosticMessage(diagnostics, diagnosticMessages.ProjectValidationFailed);
	}

	// Subsystem diagnostics 중 첫 번째 단일 행을 우선하고 없으면 WBP fallback 문구를 사용한다.
	FString ResolveStartupScreenDiagnosticMessage(
		const TArray<FString>& diagnostics,
		const FString& fallbackMessage)
	{
		for (const FString& diagnostic : diagnostics)
		{
			const FString diagnosticLine = ExtractFirstStartupScreenDiagnosticLine(diagnostic);
			if (!diagnosticLine.IsEmpty())
			{
				return diagnosticLine;
			}
		}

		return ExtractFirstStartupScreenDiagnosticLine(fallbackMessage);
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
		SetDiagnosticsText(ResolveStartupScreenProjectValidationMessage(
			outDiagnostics,
			DiagnosticMessages));
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
	UE_MVVM_SET_PROPERTY_VALUE(DiagnosticsText, ExtractFirstStartupScreenDiagnosticLine(message));
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
		item.Title = ResolveStartupScreenProjectId(normalizedProjectPath);
		item.Subtitle = ResolveStartupScreenProjectFolderName(normalizedProjectPath);
		item.PreviewImagePath = ResolveStartupScreenPreviewPath(normalizedProjectPath);
		item.bSelected = normalizedProjectPath.Equals(SelectedProjectPath, ESearchCase::IgnoreCase);
		item.bEnabled = FPaths::DirectoryExists(normalizedProjectPath);
		items.Add(MoveTemp(item));
	}

	RecentProjects = MoveTemp(items);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RecentProjects);
}
