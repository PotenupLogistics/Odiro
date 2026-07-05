#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ViewModel/StartupScreenViewModel.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"

namespace
{
	// StartupScreen ViewModel이 사용하는 recent-project config section.
	const TCHAR* StartupScreenVmConfigSection = TEXT("OdiroSim.Platform.ProjectOpen");

	// Migration fallback을 검증하기 위해 테스트 중 함께 격리하는 legacy section.
	const TCHAR* StartupScreenVmLegacyConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");

	// StartupScreen ViewModel이 기존 recent-project config와 공유하는 key.
	const TCHAR* StartupScreenVmRecentProjectPathsKey = TEXT("RecentProjectPaths");

	// 테스트 전용 project directory root를 만든다.
	FString MakeStartupScreenVmTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StartupScreenViewModel"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	// 최근 project 표시와 삭제 검증에 필요한 core project file을 쓴다.
	bool WriteStartupScreenVmProjectFiles(const FString& projectPath, const FString& projectId)
	{
		const FString settingJson = FString::Printf(
			TEXT("{\"schema\":\"project_setting\",\"version\":1,\"project_id\":\"%s\"}"),
			*projectId);
		const bool bSettingSaved = FFileHelper::SaveStringToFile(
			settingJson,
			*FPaths::Combine(projectPath, TEXT("setting.json")));
		const bool bProfileSaved = FFileHelper::SaveStringToFile(
			TEXT("{\"schema\":\"simulation_profile\",\"version\":1,\"profile_id\":\"startup_profile\",\"robot\":{}}"),
			*FPaths::Combine(projectPath, TEXT("profile.json")));
		const bool bScenarioSaved = FFileHelper::SaveStringToFile(
			TEXT("{\"schema\":\"scenario\",\"version\":1,\"scenario_id\":\"startup_scenario\",\"intent\":\"test\",\"corridor\":{},\"obstacles\":{},\"pedestrians\":{},\"robot\":{}}"),
			*FPaths::Combine(projectPath, TEXT("scenario.json")));
		return bSettingSaved && bProfileSaved && bScenarioSaved;
	}

	// 최근 project path 목록에 지정 경로가 남아 있는지 확인한다.
	bool StartupScreenVmRecentPathsContain(const TArray<FString>& projectPaths, const FString& projectPath)
	{
		FString normalizedProjectPath = FPaths::ConvertRelativePathToFull(projectPath);
		FPaths::NormalizeFilename(normalizedProjectPath);
		return projectPaths.ContainsByPredicate(
			[&normalizedProjectPath](const FString& storedPath)
			{
				FString normalizedStoredPath = FPaths::ConvertRelativePathToFull(storedPath);
				FPaths::NormalizeFilename(normalizedStoredPath);
				return normalizedStoredPath.Equals(normalizedProjectPath, ESearchCase::IgnoreCase);
			});
	}

	// 현재 project 위치에서 OS root/drive root로 해석되는 대표 경로를 만든다.
	FString ResolveStartupScreenVmRootLikePath(const FString& projectPath)
	{
		FString currentPath = FPaths::ConvertRelativePathToFull(projectPath);
		FPaths::NormalizeFilename(currentPath);
		while (!currentPath.IsEmpty())
		{
			FString parentPath = FPaths::GetPath(currentPath);
			FPaths::NormalizeFilename(parentPath);
			if (parentPath.IsEmpty() || parentPath.Equals(currentPath, ESearchCase::IgnoreCase))
			{
				break;
			}
			currentPath = parentPath;
		}

		if (currentPath.Len() == 2 && currentPath[1] == TEXT(":")[0])
		{
			currentPath += TEXT("/");
		}
		return currentPath;
	}

	// 테스트 중 변경한 recent-project config를 원래 사용자 값으로 복원한다.
	struct FScopedStartupScreenVmRecentProjectConfigRestore
	{
		FScopedStartupScreenVmRecentProjectConfigRestore()
		{
			if (!GConfig)
			{
				return;
			}

			GConfig->GetArray(
				StartupScreenVmConfigSection,
				StartupScreenVmRecentProjectPathsKey,
				OriginalRecentProjectPaths,
				GGameUserSettingsIni);
			GConfig->GetArray(
				StartupScreenVmLegacyConfigSection,
				StartupScreenVmRecentProjectPathsKey,
				OriginalLegacyRecentProjectPaths,
				GGameUserSettingsIni);
			GConfig->RemoveKey(
				StartupScreenVmConfigSection,
				StartupScreenVmRecentProjectPathsKey,
				GGameUserSettingsIni);
			GConfig->RemoveKey(
				StartupScreenVmLegacyConfigSection,
				StartupScreenVmRecentProjectPathsKey,
				GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		~FScopedStartupScreenVmRecentProjectConfigRestore()
		{
			if (!GConfig)
			{
				return;
			}

			if (OriginalRecentProjectPaths.IsEmpty())
			{
				GConfig->RemoveKey(
					StartupScreenVmConfigSection,
					StartupScreenVmRecentProjectPathsKey,
					GGameUserSettingsIni);
			}
			else
			{
				GConfig->SetArray(
					StartupScreenVmConfigSection,
					StartupScreenVmRecentProjectPathsKey,
					OriginalRecentProjectPaths,
					GGameUserSettingsIni);
			}
			if (OriginalLegacyRecentProjectPaths.IsEmpty())
			{
				GConfig->RemoveKey(
					StartupScreenVmLegacyConfigSection,
					StartupScreenVmRecentProjectPathsKey,
					GGameUserSettingsIni);
			}
			else
			{
				GConfig->SetArray(
					StartupScreenVmLegacyConfigSection,
					StartupScreenVmRecentProjectPathsKey,
					OriginalLegacyRecentProjectPaths,
					GGameUserSettingsIni);
			}
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		// 테스트 시작 전 사용자의 recent project path snapshot.
		TArray<FString> OriginalRecentProjectPaths;

		// 테스트 시작 전 사용자의 legacy recent project path snapshot.
		TArray<FString> OriginalLegacyRecentProjectPaths;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStartupScreenViewModelRecentProjectsTest,
	"OdiroSim.PlatformUi.ViewModel.StartupScreen.RecentProjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartupScreenViewModelRecentProjectsTest::RunTest(const FString& parameters)
{
	(void)parameters;

	const FScopedStartupScreenVmRecentProjectConfigRestore recentProjectConfigRestore;
	UStartupScreenViewModel* viewModel = NewObject<UStartupScreenViewModel>();
	TestNotNull(TEXT("startup screen viewmodel created"), viewModel);
	if (!viewModel)
	{
		return false;
	}
	FStartupScreenDiagnosticMessages diagnosticMessages;
	diagnosticMessages.ProjectRequired = TEXT("프로젝트를 선택하세요.");
	diagnosticMessages.ProjectFolderNotProject = TEXT("프로젝트 폴더가 아닙니다.");
	diagnosticMessages.ProjectConfigMissingFormat = TEXT("프로젝트 파일이 누락되었습니다. ({ConfigName})");
	diagnosticMessages.ProjectDeleteUnsafe = TEXT("삭제할 수 없는 프로젝트입니다.");
	diagnosticMessages.ProjectDeleteFailed = TEXT("프로젝트 삭제에 실패했습니다.");
	viewModel->SetDiagnosticMessages(diagnosticMessages);

	const FString testRoot = MakeStartupScreenVmTestRoot();
	const FString projectA = FPaths::Combine(testRoot, TEXT("ProjectA"));
	const FString projectB = FPaths::Combine(testRoot, TEXT("ProjectB"));
	const FString projectC = FPaths::Combine(testRoot, TEXT("ProjectC"));
	const FString projectD = FPaths::Combine(testRoot, TEXT("ProjectD"));
	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	IFileManager::Get().MakeDirectory(*projectA, true);
	IFileManager::Get().MakeDirectory(*projectB, true);
	IFileManager::Get().MakeDirectory(*projectC, true);
	IFileManager::Get().MakeDirectory(*projectD, true);
	TestTrue(TEXT("project A core files created"), WriteStartupScreenVmProjectFiles(projectA, TEXT("project-alpha")));
	TestTrue(TEXT("project B core files created"), WriteStartupScreenVmProjectFiles(projectB, TEXT("project-beta")));
	TestTrue(TEXT("project C core files created"), WriteStartupScreenVmProjectFiles(projectC, TEXT("project-gamma")));
	TestTrue(TEXT("project D core files created"), WriteStartupScreenVmProjectFiles(projectD, TEXT("project-delta")));

	TArray<FString> storedProjectPaths = { projectA, projectB, projectA };
	if (GConfig)
	{
		GConfig->SetArray(
			StartupScreenVmConfigSection,
			StartupScreenVmRecentProjectPathsKey,
			storedProjectPaths,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	viewModel->RefreshRecentProjects();
	const TArray<FString> recentProjectPaths = viewModel->GetRecentProjectPaths();
	TestEqual(TEXT("duplicate paths are normalized"), recentProjectPaths.Num(), 2);
	if (recentProjectPaths.Num() == 2)
	{
		TestTrue(TEXT("latest duplicate is kept at the end of config order"), recentProjectPaths[0].Equals(projectB, ESearchCase::IgnoreCase));
		TestTrue(TEXT("duplicated project appears once"), recentProjectPaths[1].Equals(projectA, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("first recent project selected by default"), viewModel->GetSelectedProjectPath().Equals(projectB, ESearchCase::IgnoreCase));

	viewModel->SelectProject(projectA);
	const TArray<FStartupScreenRecentProjectItem> recentProjects = viewModel->GetRecentProjects();
	TestEqual(TEXT("recent project items created"), recentProjects.Num(), 2);
	bool bProjectASelected = false;
	for (const FStartupScreenRecentProjectItem& item : recentProjects)
	{
		if (item.ProjectPath.Equals(projectA, ESearchCase::IgnoreCase))
		{
			bProjectASelected = item.bSelected;
			TestEqual(TEXT("project title uses project_id"), item.Title, FString(TEXT("project-alpha")));
			TestEqual(TEXT("project subtitle uses folder name"), item.Subtitle, FString(TEXT("ProjectA")));
			TestTrue(TEXT("project directory item is enabled"), item.bEnabled);
		}
	}
	TestTrue(TEXT("selected project item marked"), bProjectASelected);

	storedProjectPaths = { projectA, projectB, projectC, projectD };
	if (GConfig)
	{
		GConfig->SetArray(
			StartupScreenVmConfigSection,
			StartupScreenVmRecentProjectPathsKey,
			storedProjectPaths,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	viewModel->RefreshRecentProjects();
	TestEqual(TEXT("recent project paths remain stored"), viewModel->GetRecentProjectPaths().Num(), 4);
	const TArray<FStartupScreenRecentProjectItem> visibleRecentProjects = viewModel->GetRecentProjects();
	TestEqual(TEXT("startup screen shows up to three recent project cards"), visibleRecentProjects.Num(), 3);
	if (visibleRecentProjects.Num() == 3)
	{
		TestTrue(TEXT("first visible recent project is newest"), visibleRecentProjects[0].ProjectPath.Equals(projectA, ESearchCase::IgnoreCase));
		TestTrue(TEXT("third visible recent project is capped"), visibleRecentProjects[2].ProjectPath.Equals(projectC, ESearchCase::IgnoreCase));
	}

	TestTrue(TEXT("remove recent project only updates list"), viewModel->RemoveRecentProject(projectB));
	TestTrue(TEXT("remove recent project keeps directory"), FPaths::DirectoryExists(projectB));
	TestFalse(
		TEXT("removed recent project no longer stored"),
		StartupScreenVmRecentPathsContain(viewModel->GetRecentProjectPaths(), projectB));

	UPlatformUiSubsystem* platformUiSubsystem = NewObject<UPlatformUiSubsystem>();
	TestNotNull(TEXT("platform ui subsystem created"), platformUiSubsystem);
	TestEqual(
		TEXT("delete recent project removes folder"),
		platformUiSubsystem ? platformUiSubsystem->DeleteUserProjectDirectory(projectC) : EPlatformUserProjectDeleteResult::Failed,
		EPlatformUserProjectDeleteResult::Deleted);
	TestTrue(TEXT("delete recent project removes list entry"), viewModel->RemoveRecentProject(projectC));
	TestFalse(TEXT("deleted recent project directory removed"), FPaths::DirectoryExists(projectC));
	TestFalse(
		TEXT("deleted recent project no longer stored"),
		StartupScreenVmRecentPathsContain(viewModel->GetRecentProjectPaths(), projectC));

	TestTrue(TEXT("missing project directory removed before delete command"), IFileManager::Get().DeleteDirectory(*projectD, false, true));
	TestEqual(
		TEXT("delete missing recent project does not touch filesystem"),
		platformUiSubsystem ? platformUiSubsystem->DeleteUserProjectDirectory(projectD) : EPlatformUserProjectDeleteResult::Failed,
		EPlatformUserProjectDeleteResult::Missing);
	TestTrue(TEXT("delete missing recent project removes only list entry"), viewModel->RemoveRecentProject(projectD));
	TestFalse(
		TEXT("missing recent project no longer stored"),
		StartupScreenVmRecentPathsContain(viewModel->GetRecentProjectPaths(), projectD));

	const FString unsafeProjectRoot = FPaths::ProjectDir();
	storedProjectPaths = { unsafeProjectRoot };
	if (GConfig)
	{
		GConfig->SetArray(
			StartupScreenVmConfigSection,
			StartupScreenVmRecentProjectPathsKey,
			storedProjectPaths,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	viewModel->RefreshRecentProjects();
	TestEqual(
		TEXT("unsafe project root is not deleted"),
		platformUiSubsystem ? platformUiSubsystem->DeleteUserProjectDirectory(unsafeProjectRoot) : EPlatformUserProjectDeleteResult::Failed,
		EPlatformUserProjectDeleteResult::Unsafe);
	TestTrue(TEXT("unsafe project root directory remains"), FPaths::DirectoryExists(unsafeProjectRoot));
	TestTrue(
		TEXT("unsafe project root remains in recent list"),
		StartupScreenVmRecentPathsContain(viewModel->GetRecentProjectPaths(), unsafeProjectRoot));

	const FString rootLikePath = ResolveStartupScreenVmRootLikePath(unsafeProjectRoot);
	if (!rootLikePath.IsEmpty())
	{
		storedProjectPaths = { rootLikePath };
		if (GConfig)
		{
			GConfig->SetArray(
				StartupScreenVmConfigSection,
				StartupScreenVmRecentProjectPathsKey,
				storedProjectPaths,
				GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		viewModel->RefreshRecentProjects();
		TestEqual(
			TEXT("drive or root path is not deleted"),
			platformUiSubsystem ? platformUiSubsystem->DeleteUserProjectDirectory(rootLikePath) : EPlatformUserProjectDeleteResult::Failed,
			EPlatformUserProjectDeleteResult::Unsafe);
		TestTrue(
			TEXT("drive or root path remains in recent list"),
			StartupScreenVmRecentPathsContain(viewModel->GetRecentProjectPaths(), rootLikePath));
	}

	TArray<FString> diagnostics;
	TestFalse(TEXT("empty project path fails validation"), viewModel->ValidateProject(FString(), diagnostics));
	TestTrue(TEXT("empty path diagnostic returned"), diagnostics.Num() > 0);
	TestEqual(TEXT("empty path diagnostic text"), viewModel->GetDiagnosticsText(), FString(TEXT("프로젝트를 선택하세요.")));

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("simulator launch subsystem created"), simulatorLaunchSubsystem);
	if (!gameInstance || !simulatorLaunchSubsystem)
	{
		IFileManager::Get().DeleteDirectory(*testRoot, false, true);
		return false;
	}

	const FString invalidProject = FPaths::Combine(testRoot, TEXT("InvalidProject"));
	IFileManager::Get().MakeDirectory(*invalidProject, true);
	viewModel->SetSubsystemOverrides(simulatorLaunchSubsystem, nullptr, nullptr);
	diagnostics.Reset();
	TestFalse(TEXT("invalid project folder fails validation"), viewModel->ValidateProject(invalidProject, diagnostics));
	TestTrue(TEXT("invalid project returns multiple diagnostics"), diagnostics.Num() > 1);
	TestEqual(
		TEXT("non project folder diagnostic text"),
		viewModel->GetDiagnosticsText(),
		FString(TEXT("프로젝트 폴더가 아닙니다.")));
	TestFalse(TEXT("startup diagnostic text has no LF"), viewModel->GetDiagnosticsText().Contains(TEXT("\n")));
	TestFalse(TEXT("startup diagnostic text has no CR"), viewModel->GetDiagnosticsText().Contains(TEXT("\r")));

	const FString partialProject = FPaths::Combine(testRoot, TEXT("PartialProject"));
	FProjectPresetSelection presetSelection;
	presetSelection.ScenarioPresetId = TEXT("s-curve");
	presetSelection.ProfilePresetId = TEXT("full");
	presetSelection.PolicyPresetId = TEXT("demo");

	TArray<FString> createDiagnostics;
	TestTrue(
		TEXT("create project for partial missing config validation"),
		simulatorLaunchSubsystem->CreateProjectFromPresets(partialProject, presetSelection, createDiagnostics));
	if (!createDiagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("project create diagnostics: %s"), *FString::Join(createDiagnostics, TEXT("\n"))));
	}
	TestTrue(TEXT("remove one required config"), IFileManager::Get().Delete(*FPaths::Combine(partialProject, TEXT("setting.json"))));

	diagnostics.Reset();
	TestFalse(TEXT("partial project folder fails validation"), viewModel->ValidateProject(partialProject, diagnostics));
	TestEqual(
		TEXT("missing config diagnostic text"),
		viewModel->GetDiagnosticsText(),
		FString(TEXT("프로젝트 파일이 누락되었습니다. (setting.json)")));
	TestFalse(TEXT("missing config diagnostic text has no LF"), viewModel->GetDiagnosticsText().Contains(TEXT("\n")));
	TestFalse(TEXT("missing config diagnostic text has no CR"), viewModel->GetDiagnosticsText().Contains(TEXT("\r")));

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

#endif
