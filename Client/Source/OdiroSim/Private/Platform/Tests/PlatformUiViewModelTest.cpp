#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ViewModel/ExperimentConfigViewModel.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/ViewModel/StartupMenuViewModel.h"
#include "Platform/PlatformUiDeveloperSettings.h"
#include "Platform/Widget/MainMenuWidget.h"
#include "Platform/Widget/OdiroActivatableScreenWidget.h"
#include "Platform/Widget/OdiroCommonButtonWidget.h"
#include "Platform/Widget/ExperimentResultIterationSelectorWidget.h"
#include "Platform/Widget/ProjectExperimentRunRowWidget.h"
#include "Platform/Widget/ProjectTemplateCardWidget.h"
#include "Platform/Widget/StartupMenuWidget.h"

#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"

namespace
{
	// Startup ViewModel이 기존 StartupMenu와 공유하는 recent-project config section.
	const TCHAR* PlatformUiVmProjectOpenConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");

	// Startup ViewModel이 기존 StartupMenu와 공유하는 recent-project config key.
	const TCHAR* PlatformUiVmRecentProjectPathsConfigKey = TEXT("RecentProjectPaths");

	FString MakePlatformUiVmTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/PlatformUiViewModel"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	// 테스트 중 변경한 recent-project config를 원래 사용자 값으로 복원한다.
	struct FScopedPlatformUiVmRecentProjectConfigRestore
	{
		FScopedPlatformUiVmRecentProjectConfigRestore()
		{
			if (GConfig)
			{
				GConfig->GetArray(
					PlatformUiVmProjectOpenConfigSection,
					PlatformUiVmRecentProjectPathsConfigKey,
					OriginalRecentProjectPaths,
					GGameUserSettingsIni);
				GConfig->RemoveKey(
					PlatformUiVmProjectOpenConfigSection,
					PlatformUiVmRecentProjectPathsConfigKey,
					GGameUserSettingsIni);
				GConfig->Flush(false, GGameUserSettingsIni);
			}
		}

		~FScopedPlatformUiVmRecentProjectConfigRestore()
		{
			if (!GConfig)
			{
				return;
			}

			if (OriginalRecentProjectPaths.IsEmpty())
			{
				GConfig->RemoveKey(
					PlatformUiVmProjectOpenConfigSection,
					PlatformUiVmRecentProjectPathsConfigKey,
					GGameUserSettingsIni);
			}
			else
			{
				GConfig->SetArray(
					PlatformUiVmProjectOpenConfigSection,
					PlatformUiVmRecentProjectPathsConfigKey,
					OriginalRecentProjectPaths,
					GGameUserSettingsIni);
			}
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		// 테스트 시작 전 사용자의 recent project path snapshot.
		TArray<FString> OriginalRecentProjectPaths;
	};

	FProjectPresetSelection MakePlatformUiVmDemoPresetSelection()
	{
		FProjectPresetSelection selection;
		selection.ScenarioPresetId = TEXT("blank");
		selection.ProfilePresetId = TEXT("basic");
		selection.PolicyPresetId = TEXT("blank");
		return selection;
	}

	bool CreatePlatformUiVmTestProject(
		USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
		const FString& projectPath,
		TArray<FString>& outDiagnostics)
	{
		return simulatorLaunchSubsystem
			&& simulatorLaunchSubsystem->CreateProjectFromPresets(
				projectPath,
				MakePlatformUiVmDemoPresetSelection(),
				outDiagnostics)
			&& simulatorLaunchSubsystem->ValidateUserProject(projectPath, outDiagnostics);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformUiViewModelStartupTest,
	"OdiroSim.PlatformUi.ViewModel.Startup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiViewModelStartupTest::RunTest(const FString& parameters)
{
	(void)parameters;

	const FScopedPlatformUiVmRecentProjectConfigRestore recentProjectConfigRestore;
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	UStartupMenuViewModel* viewModel = NewObject<UStartupMenuViewModel>();
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("simulator launch subsystem created"), simulatorLaunchSubsystem);
	TestNotNull(TEXT("startup viewmodel created"), viewModel);
	if (!gameInstance || !simulatorLaunchSubsystem || !viewModel)
	{
		return false;
	}

	viewModel->SetSubsystemOverrides(simulatorLaunchSubsystem, nullptr, nullptr);
	viewModel->InitializeForGameInstance(gameInstance);
	TestTrue(TEXT("scenario preset items available"), viewModel->GetScenarioPresetItems().Num() > 0);

	const FString testRoot = MakePlatformUiVmTestRoot();
	const FString projectName = TEXT("StartupVmProject");
	const FString projectPath = FPaths::Combine(testRoot, projectName);
	IFileManager::Get().DeleteDirectory(*testRoot, false, true);

	TestTrue(
		TEXT("create project through startup viewmodel"),
		viewModel->CreateProject(testRoot, projectName, MakePlatformUiVmDemoPresetSelection()));
	TestEqual(TEXT("project path set"), viewModel->GetProjectPathForPrototype(), projectPath);
	TestTrue(TEXT("created project exists"), FPaths::DirectoryExists(projectPath));
	TestEqual(TEXT("recent project stored"), viewModel->GetRecentProjectPaths().Num(), 1);
	TestEqual(TEXT("recent item stored"), viewModel->GetRecentProjectItems().Num(), 1);

	TestTrue(TEXT("remove recent project"), viewModel->RemoveRecentProject(projectPath));
	TestEqual(TEXT("recent project removed"), viewModel->GetRecentProjectPaths().Num(), 0);

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformUiViewModelProjectWorkspaceTest,
	"OdiroSim.PlatformUi.ViewModel.ProjectWorkspace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiViewModelProjectWorkspaceTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	UProjectSessionSubsystem* projectSessionSubsystem = NewObject<UProjectSessionSubsystem>(gameInstance);
	UProjectWorkspaceViewModel* viewModel = NewObject<UProjectWorkspaceViewModel>();
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("simulator launch subsystem created"), simulatorLaunchSubsystem);
	TestNotNull(TEXT("project session subsystem created"), projectSessionSubsystem);
	TestNotNull(TEXT("workspace viewmodel created"), viewModel);
	if (!gameInstance || !simulatorLaunchSubsystem || !projectSessionSubsystem || !viewModel)
	{
		return false;
	}

	const FString testRoot = MakePlatformUiVmTestRoot();
	const FString projectPath = FPaths::Combine(testRoot, TEXT("WorkspaceVmProject"));
	IFileManager::Get().DeleteDirectory(*testRoot, false, true);

	TArray<FString> diagnostics;
	TestTrue(
		TEXT("create project fixture"),
		CreatePlatformUiVmTestProject(simulatorLaunchSubsystem, projectPath, diagnostics));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("fixture diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}

	projectSessionSubsystem->SetActiveProjectPath(projectPath);
	viewModel->SetSubsystemOverrides(simulatorLaunchSubsystem, projectSessionSubsystem, nullptr, nullptr);
	viewModel->InitializeForGameInstance(gameInstance);
	TestEqual(TEXT("active project from session"), viewModel->GetActiveProjectPath(), projectSessionSubsystem->GetActiveProjectPath());

	FString runId;
	TestTrue(TEXT("create run through workspace viewmodel"), viewModel->CreateRun(runId));
	TestEqual(TEXT("first run id selected"), runId, FString(TEXT("000001")));
	TestEqual(TEXT("selected run id"), viewModel->GetSelectedRunId(), runId);
	TestEqual(TEXT("run item count"), viewModel->GetRunItems().Num(), 1);

	FString nextRunId;
	TestTrue(TEXT("create next run while previous run is selected"), viewModel->CreateRun(nextRunId));
	TestEqual(TEXT("second run id selected"), nextRunId, FString(TEXT("000002")));
	TestEqual(TEXT("selected run id after second create"), viewModel->GetSelectedRunId(), nextRunId);
	TestEqual(TEXT("run item count after second create"), viewModel->GetRunItems().Num(), 2);

	FString thirdRunId;
	TestTrue(TEXT("create third run while second run is selected"), viewModel->CreateRun(thirdRunId));
	TestEqual(TEXT("third run id selected"), thirdRunId, FString(TEXT("000003")));
	TestEqual(TEXT("selected run id after third create"), viewModel->GetSelectedRunId(), thirdRunId);
	TestEqual(TEXT("run item count after third create"), viewModel->GetRunItems().Num(), 3);

	viewModel->SelectWorkspaceTab(TEXT("ExperimentStatus"));
	TestEqual(TEXT("workspace tab selected"), viewModel->GetSelectedWorkspaceTabId(), FName(TEXT("ExperimentStatus")));

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformUiViewModelExperimentConfigTest,
	"OdiroSim.PlatformUi.ViewModel.ExperimentConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiViewModelExperimentConfigTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	UProjectSessionSubsystem* projectSessionSubsystem = NewObject<UProjectSessionSubsystem>(gameInstance);
	UExperimentConfigViewModel* viewModel = NewObject<UExperimentConfigViewModel>();
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("simulator launch subsystem created"), simulatorLaunchSubsystem);
	TestNotNull(TEXT("project session subsystem created"), projectSessionSubsystem);
	TestNotNull(TEXT("experiment config viewmodel created"), viewModel);
	if (!gameInstance || !simulatorLaunchSubsystem || !projectSessionSubsystem || !viewModel)
	{
		return false;
	}

	const FString testRoot = MakePlatformUiVmTestRoot();
	const FString projectPath = FPaths::Combine(testRoot, TEXT("ExperimentConfigVmProject"));
	IFileManager::Get().DeleteDirectory(*testRoot, false, true);

	TArray<FString> diagnostics;
	TestTrue(
		TEXT("create project fixture"),
		CreatePlatformUiVmTestProject(simulatorLaunchSubsystem, projectPath, diagnostics));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("fixture diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}

	projectSessionSubsystem->SetActiveProjectPath(projectPath);
	viewModel->SetSubsystemOverride(projectSessionSubsystem);
	viewModel->InitializeForGameInstance(gameInstance);
	viewModel->SetMapId(TEXT("ScenarioSimulationMap"));
	viewModel->SetFixedFps(30);
	viewModel->SetEpisodeCount(4);
	viewModel->SetBaseSeed(12345);
	TestTrue(TEXT("save experiment settings through viewmodel"), viewModel->SaveExperimentSettings());

	viewModel->SetMapId(TEXT("Changed"));
	viewModel->SetFixedFps(1);
	viewModel->SetEpisodeCount(1);
	viewModel->SetBaseSeed(0);
	TestTrue(TEXT("reload experiment settings through viewmodel"), viewModel->LoadFromActiveProject());
	TestEqual(TEXT("map id round trip"), viewModel->GetMapId(), FString(TEXT("ScenarioSimulationMap")));
	TestEqual(TEXT("fixed fps round trip"), viewModel->GetFixedFps(), 30);
	TestEqual(TEXT("episode count round trip"), viewModel->GetEpisodeCount(), 4);
	TestEqual(TEXT("base seed round trip"), viewModel->GetBaseSeed(), static_cast<int64>(12345));

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformUiViewModelExperimentResultTest,
	"OdiroSim.PlatformUi.ViewModel.ExperimentResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiViewModelExperimentResultTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UExperimentResultViewModel* viewModel = NewObject<UExperimentResultViewModel>();
	TestNotNull(TEXT("experiment result viewmodel created"), viewModel);
	if (!viewModel)
	{
		return false;
	}

	const FString testRoot = MakePlatformUiVmTestRoot();
	const FString runDirectory = FPaths::Combine(testRoot, TEXT("runs/000123"));
	IFileManager::Get().MakeDirectory(*runDirectory, true);
	const FString summaryPath = FPaths::Combine(runDirectory, TEXT("summary.json"));
	const FString summaryJson = TEXT(R"({
		"schema": "run_summary",
		"version": 1,
		"run": { "run_id": "000123" },
		"rows": [
			{
				"episode_id": "000001",
				"outcome": "Success",
				"terminal_reason": "GoalReached",
				"duration_s": 2.5,
				"metrics": {
					"goal_reached": 1,
					"blocked_region_collision_count": 0,
					"pedestrian_collision_count": 0,
					"static_obstacle_collision_count": 0
				}
			}
		]
	})");
	TestTrue(
		TEXT("write summary fixture"),
		FFileHelper::SaveStringToFile(summaryJson, *summaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	TestTrue(TEXT("load run directory"), viewModel->LoadRunDirectory(runDirectory));
	TestEqual(TEXT("run id loaded"), viewModel->GetRunId(), FString(TEXT("000123")));
	TestEqual(TEXT("episode count loaded"), viewModel->GetDashboardData().EpisodeCount, 1);
	TestEqual(TEXT("total duration label"), viewModel->GetTotalDurationLabel(), FString(TEXT("2.5 s")));
	TestEqual(TEXT("success rate label"), viewModel->GetSuccessRateLabel(), FString(TEXT("100%")));
	TestEqual(TEXT("collision count label"), viewModel->GetCollisionCountLabel(), FString(TEXT("0")));

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformUiCommonUiActivationSmokeTest,
	"OdiroSim.PlatformUi.CommonUI.Activatable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiCommonUiActivationSmokeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UOdiroActivatableScreenWidget* screenWidget = NewObject<UOdiroActivatableScreenWidget>();
	UOdiroCommonButtonWidget* buttonWidget = NewObject<UOdiroCommonButtonWidget>();
	TestNotNull(TEXT("activatable screen widget created"), screenWidget);
	TestNotNull(TEXT("common button widget created"), buttonWidget);
	if (!screenWidget || !buttonWidget)
	{
		return false;
	}

	TestTrue(TEXT("screen derives from CommonActivatableWidget"), screenWidget->IsA<UCommonActivatableWidget>());
	TestTrue(TEXT("screen handles back action by default"), screenWidget->CanHandleBackAction());
	TestTrue(TEXT("button derives from CommonButtonBase"), buttonWidget->IsA<UCommonButtonBase>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformUiStartupToScenarioEditorMapSmokeTest,
	"OdiroSim.PlatformUi.Map.StartupToScenarioEditorSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiStartupToScenarioEditorMapSmokeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	constexpr TCHAR StartupMapPackage[] = TEXT("/Game/Maps/StartupMap");
	constexpr TCHAR ScenarioEditorMapPackage[] = TEXT("/Game/Maps/ScenarioEditorMap");
	TestTrue(TEXT("StartupMap package exists"), FPackageName::DoesPackageExist(StartupMapPackage));
	TestTrue(TEXT("ScenarioEditorMap package exists"), FPackageName::DoesPackageExist(ScenarioEditorMapPackage));

	const UPlatformUiDeveloperSettings* settings = GetDefault<UPlatformUiDeveloperSettings>();
	TestNotNull(TEXT("Platform UI settings available"), settings);
	if (!settings)
	{
		return false;
	}

	UClass* startupMenuClass = settings->StartupMenuWidgetClass.LoadSynchronous();
	UClass* mainMenuClass = settings->MainMenuWidgetClass.LoadSynchronous();
	UClass* projectTemplateCardClass = settings->ProjectTemplateCardWidgetClass.LoadSynchronous();
	UClass* projectExperimentRunRowClass = settings->ProjectExperimentRunRowWidgetClass.LoadSynchronous();
	UClass* projectEpisodeReplayCardClass = settings->ProjectEpisodeReplayCardWidgetClass.LoadSynchronous();
	UClass* projectAiSuggestionRowClass = settings->ProjectAiSuggestionRowWidgetClass.LoadSynchronous();
	UClass* experimentResultIterationSelectorClass = settings->ExperimentResultIterationSelectorWidgetClass.LoadSynchronous();
	TestNotNull(TEXT("StartupMenu widget class configured"), startupMenuClass);
	TestNotNull(TEXT("MainMenu widget class configured"), mainMenuClass);
	TestNotNull(TEXT("Project template card widget class configured"), projectTemplateCardClass);
	TestNotNull(TEXT("Project experiment run row widget class configured"), projectExperimentRunRowClass);
	TestNotNull(TEXT("Project episode replay card widget class configured"), projectEpisodeReplayCardClass);
	TestNotNull(TEXT("Project AI suggestion row widget class configured"), projectAiSuggestionRowClass);
	TestNotNull(TEXT("Experiment result iteration selector widget class configured"), experimentResultIterationSelectorClass);
	if (!startupMenuClass
		|| !mainMenuClass
		|| !projectTemplateCardClass
		|| !projectExperimentRunRowClass
		|| !projectEpisodeReplayCardClass
		|| !projectAiSuggestionRowClass
		|| !experimentResultIterationSelectorClass)
	{
		return false;
	}

	TestTrue(
		TEXT("StartupMap class derives from UStartupMenuWidget"),
		startupMenuClass->IsChildOf(UStartupMenuWidget::StaticClass()));
	TestTrue(
		TEXT("ScenarioEditorMap workspace class derives from UMainMenuWidget"),
		mainMenuClass->IsChildOf(UMainMenuWidget::StaticClass()));
	TestTrue(
		TEXT("Project template card class derives from UProjectTemplateCardWidget"),
		projectTemplateCardClass->IsChildOf(UProjectTemplateCardWidget::StaticClass()));
	TestTrue(
		TEXT("Project experiment run row class derives from UProjectExperimentRunRowWidget"),
		projectExperimentRunRowClass->IsChildOf(UProjectExperimentRunRowWidget::StaticClass()));
	TestTrue(
		TEXT("Project episode replay card class derives from UUserWidget"),
		projectEpisodeReplayCardClass->IsChildOf(UUserWidget::StaticClass()));
	TestTrue(
		TEXT("Project AI suggestion row class derives from UUserWidget"),
		projectAiSuggestionRowClass->IsChildOf(UUserWidget::StaticClass()));
	TestTrue(
		TEXT("Experiment result iteration selector class derives from UExperimentResultIterationSelectorWidget"),
		experimentResultIterationSelectorClass->IsChildOf(UExperimentResultIterationSelectorWidget::StaticClass()));
	return true;
}

#endif
