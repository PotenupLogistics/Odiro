#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/Widget/MainMenuWidget.h"
#include "Platform/Widget/StartupMenuWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Platform/SimulatorLaunchSubsystem.h"

namespace
{
	// StartupMenu config section that owns persisted project picker options.
	const TCHAR* StartupMenuProjectOpenConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");

	// Persisted recent project path array key used by StartupMenu.
	const TCHAR* StartupMenuRecentProjectPathsConfigKey = TEXT("RecentProjectPaths");

	FString MakeMainMenuProjectModeTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/MainMenuProjectMode"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	// Restores user recent-project config after tests that exercise real persistence.
	struct FScopedStartupMenuRecentProjectConfigRestore
	{
		FScopedStartupMenuRecentProjectConfigRestore()
		{
			if (GConfig)
			{
				GConfig->GetArray(
					StartupMenuProjectOpenConfigSection,
					StartupMenuRecentProjectPathsConfigKey,
					OriginalRecentProjectPaths,
					GGameUserSettingsIni);
			}
		}

		~FScopedStartupMenuRecentProjectConfigRestore()
		{
			if (!GConfig)
			{
				return;
			}

			if (OriginalRecentProjectPaths.IsEmpty())
			{
				GConfig->RemoveKey(
					StartupMenuProjectOpenConfigSection,
					StartupMenuRecentProjectPathsConfigKey,
					GGameUserSettingsIni);
			}
			else
			{
				GConfig->SetArray(
					StartupMenuProjectOpenConfigSection,
					StartupMenuRecentProjectPathsConfigKey,
					OriginalRecentProjectPaths,
					GGameUserSettingsIni);
			}
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		// Snapshot of the user's recent project config before the test mutates it.
		TArray<FString> OriginalRecentProjectPaths;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStartupMenuProjectModeSmokeTest,
	"OdiroSim.StartupMenu.ProjectMode.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartupMenuProjectModeSmokeTest::RunTest(const FString& parameters)
{
	UStartupMenuWidget* widget = NewObject<UStartupMenuWidget>();
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("widget created"), widget);
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!widget || !gameInstance || !subsystem)
	{
		return false;
	}

	const FString projectPath = MakeMainMenuProjectModeTestRoot();
	IFileManager::Get().DeleteDirectory(*projectPath, false, true);

	widget->SetProjectPathForPrototype(projectPath);
	widget->SelectProjectPresets(TEXT("demo"), TEXT("full"), TEXT("demo"));
	TestEqual(TEXT("project path selected"), widget->GetProjectPathForPrototype(), projectPath);

	TArray<FString> diagnostics;
	TestTrue(
		TEXT("create project through StartupMenu project mode"),
		widget->CreateSelectedProject(diagnostics, subsystem));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("create diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}

	TestTrue(
		TEXT("validate project through StartupMenu project mode"),
		widget->ValidateSelectedProject(diagnostics, subsystem));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("validate diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}

	FString runId;
	TestTrue(
		TEXT("create run through project subsystem"),
		subsystem->CreateProjectRun(projectPath, runId, diagnostics));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("run diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}
	TestEqual(TEXT("first run selected"), runId, FString(TEXT("000001")));

	const FString runPath = FPaths::Combine(projectPath, TEXT("runs"), runId);
	TestTrue(TEXT("run snapshot setting exists"), FPaths::FileExists(FPaths::Combine(runPath, TEXT("snapshot/setting.json"))));
	TestTrue(TEXT("run snapshot policy exists"), FPaths::FileExists(FPaths::Combine(runPath, TEXT("snapshot/policy/__init__.py"))));
	TestTrue(TEXT("demo policy snapshot exists"), FPaths::FileExists(FPaths::Combine(runPath, TEXT("snapshot/policy/action.py"))));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStartupMenuRecentProjectsManualAddTest,
	"OdiroSim.StartupMenu.RecentProjects.ManualAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartupMenuRecentProjectsManualAddTest::RunTest(const FString& parameters)
{
	const FScopedStartupMenuRecentProjectConfigRestore recentProjectConfigRestore;

	UStartupMenuWidget* widget = NewObject<UStartupMenuWidget>();
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("widget created"), widget);
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!widget || !gameInstance || !subsystem)
	{
		return false;
	}

	const FString testRoot = MakeMainMenuProjectModeTestRoot();
	const FString projectAPath = FPaths::Combine(testRoot, TEXT("ProjectA"));
	const FString projectBPath = FPaths::Combine(testRoot, TEXT("ProjectB"));
	const FString invalidProjectPath = FPaths::Combine(testRoot, TEXT("InvalidProject"));
	IFileManager::Get().DeleteDirectory(*testRoot, false, true);

	widget->SelectProjectPresets(TEXT("demo"), TEXT("full"), TEXT("demo"));
	TArray<FString> diagnostics;

	widget->SetProjectPathForPrototype(projectAPath);
	TestTrue(TEXT("create project A"), widget->CreateSelectedProject(diagnostics, subsystem));
	widget->SetProjectPathForPrototype(projectBPath);
	TestTrue(TEXT("create project B"), widget->CreateSelectedProject(diagnostics, subsystem));

	TestTrue(TEXT("manual add project A"), widget->AddRecentProjectForPrototype(projectAPath, diagnostics, subsystem));
	TestTrue(TEXT("manual add project B"), widget->AddRecentProjectForPrototype(projectBPath, diagnostics, subsystem));
	TestTrue(TEXT("manual add duplicate project A"), widget->AddRecentProjectForPrototype(projectAPath, diagnostics, subsystem));

	const TArray<FString> recentProjectPaths = widget->GetRecentProjectPathsForPrototype();
	TestEqual(TEXT("duplicate add keeps two recent projects"), recentProjectPaths.Num(), 2);
	if (recentProjectPaths.Num() >= 2)
	{
		FString expectedProjectAPath = projectAPath;
		FString expectedProjectBPath = projectBPath;
		FPaths::NormalizeFilename(expectedProjectAPath);
		FPaths::NormalizeFilename(expectedProjectBPath);
		TestEqual(TEXT("duplicate project moves to newest slot"), recentProjectPaths[0], expectedProjectAPath);
		TestEqual(TEXT("previous newest shifts to second slot"), recentProjectPaths[1], expectedProjectBPath);
	}

	diagnostics.Reset();
	TestFalse(
		TEXT("invalid project folder is not added"),
		widget->AddRecentProjectForPrototype(invalidProjectPath, diagnostics, subsystem));
	TestTrue(TEXT("invalid project reports diagnostics"), diagnostics.Num() > 0);
	TestEqual(TEXT("invalid project does not change recent list"), widget->GetRecentProjectPathsForPrototype().Num(), 2);

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMainMenuProjectSessionNoLegacyPathFallbackTest,
	"OdiroSim.MainMenu.ProjectSession.NoLegacyPathFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMainMenuProjectSessionNoLegacyPathFallbackTest::RunTest(const FString& parameters)
{
	UMainMenuWidget* widget = NewObject<UMainMenuWidget>();
	TestNotNull(TEXT("widget created"), widget);
	if (!widget)
	{
		return false;
	}

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!gameInstance || !subsystem)
	{
		return false;
	}

	FString runId;
	TArray<FString> diagnostics;
	TestFalse(
		TEXT("MainMenu cannot create a project run without an active ProjectSession"),
		widget->CreateProjectRunForPrototype(runId, diagnostics, subsystem));
	TestTrue(TEXT("run id remains empty"), runId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectSessionSubsystemPathsTest,
	"OdiroSim.ProjectSession.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectSessionSubsystemPathsTest::RunTest(const FString& parameters)
{
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	UProjectSessionSubsystem* subsystem = NewObject<UProjectSessionSubsystem>(gameInstance);
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("project session created"), subsystem);
	if (!gameInstance || !subsystem)
	{
		return false;
	}

	const FString projectPath = MakeMainMenuProjectModeTestRoot();
	FString expectedProjectPath = projectPath;
	FPaths::NormalizeFilename(expectedProjectPath);
	subsystem->SetActiveProjectPath(projectPath);
	TestTrue(TEXT("active project set"), subsystem->HasActiveProject());
	TestEqual(TEXT("active project path"), subsystem->GetActiveProjectPath(), expectedProjectPath);
	FString expectedScenarioPath = FPaths::Combine(expectedProjectPath, TEXT("scenario.json"));
	FString expectedSettingPath = FPaths::Combine(expectedProjectPath, TEXT("setting.json"));
	FPaths::NormalizeFilename(expectedScenarioPath);
	FPaths::NormalizeFilename(expectedSettingPath);
	TestEqual(
		TEXT("active scenario path"),
		subsystem->GetActiveProjectScenarioPath(),
		expectedScenarioPath);
	TestEqual(
		TEXT("active setting path"),
		subsystem->GetActiveProjectSettingPath(),
		expectedSettingPath);

	subsystem->ClearActiveProject();
	TestFalse(TEXT("active project cleared"), subsystem->HasActiveProject());
	return true;
}

#endif
