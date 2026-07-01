#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ViewModel/ExperimentConfigViewModel.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/PlatformUiDeveloperSettings.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/Widget/PlatformRootWidget.h"

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
	FString MakePlatformUiVmTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/PlatformUiViewModel"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

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
	FPlatformUiViewModelRobotProfileTest,
	"OdiroSim.PlatformUi.ViewModel.RobotProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformUiViewModelRobotProfileTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	UProjectSessionSubsystem* projectSessionSubsystem = NewObject<UProjectSessionSubsystem>(gameInstance);
	URobotProfileViewModel* viewModel = NewObject<URobotProfileViewModel>();
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("simulator launch subsystem created"), simulatorLaunchSubsystem);
	TestNotNull(TEXT("project session subsystem created"), projectSessionSubsystem);
	TestNotNull(TEXT("robot profile viewmodel created"), viewModel);
	if (!gameInstance || !simulatorLaunchSubsystem || !projectSessionSubsystem || !viewModel)
	{
		return false;
	}

	const FString testRoot = MakePlatformUiVmTestRoot();
	const FString projectPath = FPaths::Combine(testRoot, TEXT("RobotProfileVmProject"));
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
	viewModel->SetBodyLengthM(0.75f);
	viewModel->SetBodyWidthM(0.95f);
	viewModel->SetBodyHeightM(0.55f);
	viewModel->SetBodyWheelBaseM(0.50f);
	viewModel->SetBodyTurningRadiusM(2.40f);
	viewModel->SetDriveMaxSpeedKmh(8.50f);
	viewModel->SetDriveMaxReverseSpeedKmh(2.25f);
	viewModel->SetDriveAccelerationRateKmhPerSecond(1.45f);
	viewModel->SetDriveDecelerationRateKmhPerSecond(1.10f);
	viewModel->SetDriveSteeringRatePerS(4.10f);
	viewModel->SetDriveMassKg(52.00f);
	viewModel->SetLidarMode(TEXT("3D"));
	viewModel->SetLidarDrawDebug(true);
	viewModel->SetLidarScanRangeM(18.00f);
	viewModel->SetLidarSensorHeightM(0.09f);
	viewModel->SetLidarFrontHalfAngleDegree(65.00f);
	viewModel->SetLidarStopDistanceM(2.50f);
	viewModel->SetLidarSlowDownDistanceM(9.00f);
	viewModel->SetLidarAngleStepDegree(4.00f);
	viewModel->SetLidarScanRateHz(7.50f);
	TestTrue(TEXT("save robot profile through viewmodel"), viewModel->SaveRobotProfile());

	viewModel->SetBodyLengthM(1.0f);
	viewModel->SetBodyWidthM(1.0f);
	viewModel->SetBodyHeightM(1.0f);
	viewModel->SetBodyWheelBaseM(1.0f);
	viewModel->SetBodyTurningRadiusM(1.0f);
	viewModel->SetDriveMaxSpeedKmh(1.0f);
	viewModel->SetDriveMaxReverseSpeedKmh(1.0f);
	viewModel->SetDriveAccelerationRateKmhPerSecond(1.0f);
	viewModel->SetDriveDecelerationRateKmhPerSecond(1.0f);
	viewModel->SetDriveSteeringRatePerS(1.0f);
	viewModel->SetDriveMassKg(1.0f);
	viewModel->SetLidarMode(TEXT("1D"));
	viewModel->SetLidarDrawDebug(false);
	viewModel->SetLidarScanRangeM(1.0f);
	viewModel->SetLidarSensorHeightM(1.0f);
	viewModel->SetLidarFrontHalfAngleDegree(1.0f);
	viewModel->SetLidarStopDistanceM(1.0f);
	viewModel->SetLidarSlowDownDistanceM(1.0f);
	viewModel->SetLidarAngleStepDegree(1.0f);
	viewModel->SetLidarScanRateHz(1.0f);
	TestTrue(TEXT("reload robot profile through viewmodel"), viewModel->LoadFromActiveProject());
	TestTrue(TEXT("body length round trip"), FMath::IsNearlyEqual(viewModel->GetBodyLengthM(), 0.75f));
	TestTrue(TEXT("body width round trip"), FMath::IsNearlyEqual(viewModel->GetBodyWidthM(), 0.95f));
	TestTrue(TEXT("body height round trip"), FMath::IsNearlyEqual(viewModel->GetBodyHeightM(), 0.55f));
	TestTrue(TEXT("wheel base round trip"), FMath::IsNearlyEqual(viewModel->GetBodyWheelBaseM(), 0.50f));
	TestTrue(TEXT("turning radius round trip"), FMath::IsNearlyEqual(viewModel->GetBodyTurningRadiusM(), 2.40f));
	TestTrue(TEXT("drive max speed round trip"), FMath::IsNearlyEqual(viewModel->GetDriveMaxSpeedKmh(), 8.50f));
	TestTrue(TEXT("drive max reverse speed round trip"), FMath::IsNearlyEqual(viewModel->GetDriveMaxReverseSpeedKmh(), 2.25f));
	TestTrue(TEXT("drive acceleration round trip"), FMath::IsNearlyEqual(viewModel->GetDriveAccelerationRateKmhPerSecond(), 1.45f));
	TestTrue(TEXT("drive deceleration round trip"), FMath::IsNearlyEqual(viewModel->GetDriveDecelerationRateKmhPerSecond(), 1.10f));
	TestTrue(TEXT("drive steering rate round trip"), FMath::IsNearlyEqual(viewModel->GetDriveSteeringRatePerS(), 4.10f));
	TestTrue(TEXT("drive mass round trip"), FMath::IsNearlyEqual(viewModel->GetDriveMassKg(), 52.00f));
	TestEqual(TEXT("lidar mode round trip"), viewModel->GetLidarMode(), FString(TEXT("3D")));
	TestTrue(TEXT("lidar draw debug round trip"), viewModel->GetLidarDrawDebug());
	TestTrue(TEXT("lidar scan range round trip"), FMath::IsNearlyEqual(viewModel->GetLidarScanRangeM(), 18.00f));
	TestTrue(TEXT("lidar sensor height round trip"), FMath::IsNearlyEqual(viewModel->GetLidarSensorHeightM(), 0.09f));
	TestTrue(TEXT("lidar front angle round trip"), FMath::IsNearlyEqual(viewModel->GetLidarFrontHalfAngleDegree(), 65.00f));
	TestTrue(TEXT("lidar stop distance round trip"), FMath::IsNearlyEqual(viewModel->GetLidarStopDistanceM(), 2.50f));
	TestTrue(TEXT("lidar slowdown distance round trip"), FMath::IsNearlyEqual(viewModel->GetLidarSlowDownDistanceM(), 9.00f));
	TestTrue(TEXT("lidar angle step round trip"), FMath::IsNearlyEqual(viewModel->GetLidarAngleStepDegree(), 4.00f));
	TestTrue(TEXT("lidar scan rate round trip"), FMath::IsNearlyEqual(viewModel->GetLidarScanRateHz(), 7.50f));

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
	UPlatformUiSubsystem* platformUiSubsystem = NewObject<UPlatformUiSubsystem>();
	viewModel->SetSubsystemOverride(platformUiSubsystem);

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

	const FString reviewDirectory = FPaths::Combine(runDirectory, TEXT("review"));
	TestTrue(TEXT("create review fixture directory"), IFileManager::Get().MakeDirectory(*reviewDirectory, true));
	const FString analysisResponseJson = TEXT(R"({
		"schema": "analysis_run_response_v2",
		"version": 2,
		"status": "success",
		"run_id": "000123",
		"summary": {
			"overall_judgement": "change_recommended",
			"message": "AI 분석 완료 후 결과를 다시 읽었습니다."
		},
		"insights": [
			{
				"severity": "high",
				"title": "정체 후 제한 시간 초과",
				"description": "제한 시간 내 목표에 도달하지 못했습니다."
			}
		],
		"recommendations": [
			{
				"target": "policy",
				"priority": "high",
				"title": "정책 파라미터 검토",
				"reason": "실패 episode가 감지되었습니다.",
				"recommendation": "감속 조건을 보수적으로 조정하세요."
			}
		],
		"warnings": [
			"skipped large file: actions.jsonl"
		]
	})");
	TestTrue(
		TEXT("write analysis response fixture"),
		FFileHelper::SaveStringToFile(
			analysisResponseJson,
			*FPaths::Combine(reviewDirectory, TEXT("analysis_run_response_v2.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FPlatformAnalysisAiResponse analysisResponse;
	analysisResponse.bSuccess = true;
	analysisResponse.RunId = TEXT("000123");
	analysisResponse.RunDirectory = runDirectory;
	platformUiSubsystem->OnAnalysisCompleted.Broadcast(analysisResponse);

	TestTrue(TEXT("completion reloads saved AI response"), viewModel->GetDashboardData().bAiLoaded);
	TestTrue(TEXT("completion updates AI summary"), viewModel->GetAiSummaryText().Contains(TEXT("다시 읽었습니다")));
	TestEqual(TEXT("completion updates insights"), viewModel->GetInsightItems().Num(), 1);
	TestEqual(TEXT("completion updates suggestions"), viewModel->GetSuggestionItems().Num(), 1);
	TestEqual(TEXT("completion updates warnings"), viewModel->GetWarningItems().Num(), 1);

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
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

	FString gameDefaultMap;
	FString editorStartupMap;
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GameMapsSettings"),
		TEXT("GameDefaultMap"),
		gameDefaultMap,
		GEngineIni);
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GameMapsSettings"),
		TEXT("EditorStartupMap"),
		editorStartupMap,
		GEngineIni);
	TestTrue(
		TEXT("GameDefaultMap uses ScenarioEditorMap root shell"),
		gameDefaultMap.Contains(ScenarioEditorMapPackage));
	TestTrue(
		TEXT("EditorStartupMap uses ScenarioEditorMap root shell"),
		editorStartupMap.Contains(ScenarioEditorMapPackage));

	const UPlatformUiDeveloperSettings* settings = GetDefault<UPlatformUiDeveloperSettings>();
	TestNotNull(TEXT("Platform UI settings available"), settings);
	if (!settings)
	{
		return false;
	}

	UClass* platformRootClass = settings->PlatformRootWidgetClass.LoadSynchronous();
	TestNotNull(TEXT("Platform root widget class configured"), platformRootClass);
	if (!platformRootClass)
	{
		return false;
	}

	TestTrue(
		TEXT("Platform root class derives from UPlatformRootWidget"),
		platformRootClass->IsChildOf(UPlatformRootWidget::StaticClass()));

	return true;
}

#endif
