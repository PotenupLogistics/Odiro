#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/SimulatorLaunchSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	bool SaveSimulatorLaunchTestFile(const FString& filePath, const FString& contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(filePath), true);
		return FFileHelper::SaveStringToFile(contents, *filePath);
	}

	bool HasSimulatorLaunchDiagnosticContaining(const TArray<FString>& diagnostics, const FString& needle)
	{
		return diagnostics.ContainsByPredicate(
			[&needle](const FString& diagnostic)
			{
				return diagnostic.Contains(needle);
			});
	}

	bool WriteSimulatorLaunchProject(const FString& projectPath)
	{
		IFileManager::Get().MakeDirectory(*FPaths::Combine(projectPath, TEXT("runs")), true);
		return SaveSimulatorLaunchTestFile(
				FPaths::Combine(projectPath, TEXT("setting.json")),
				TEXT("{")
				TEXT("\"schema\":\"project_setting\",")
				TEXT("\"version\":1,")
				TEXT("\"project_id\":\"launcher_project\",")
				TEXT("\"sampling\":{\"base_seed\":1000,\"episode_count\":1,\"generator_version\":\"0.1.0\"},")
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":60,\"time_scale\":1.0,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.0,\"tip_over_angle_deg\":60,\"near_miss_distance_m\":0.5}")
				TEXT("}"))
			&& SaveSimulatorLaunchTestFile(
				FPaths::Combine(projectPath, TEXT("profile.json")),
				TEXT("{")
				TEXT("\"schema\":\"simulation_profile\",")
				TEXT("\"version\":1,")
				TEXT("\"profile_id\":\"launcher_profile\",")
				TEXT("\"display_name\":\"Launcher\",")
				TEXT("\"description\":\"Launcher profile\",")
				TEXT("\"robot\":{}")
				TEXT("}"))
			&& SaveSimulatorLaunchTestFile(
				FPaths::Combine(projectPath, TEXT("scenario.json")),
				TEXT("{")
				TEXT("\"schema\":\"scenario\",")
				TEXT("\"version\":1,")
				TEXT("\"scenario_id\":\"launcher_scenario\",")
				TEXT("\"intent\":\"Launcher\",")
				TEXT("\"corridor\":{")
				TEXT("\"axis\":{\"type\":\"polyline\",\"points_m\":[[0.0,0.0],[10.0,0.0]]},")
				TEXT("\"walkway_width_m\":3.0,")
				TEXT("\"segments\":[{\"id\":\"main\",\"type\":\"straight\",\"along_range_m\":[0.0,10.0]}]")
				TEXT("},")
				TEXT("\"obstacles\":{},")
				TEXT("\"pedestrians\":{},")
				TEXT("\"robot\":{\"start\":{\"type\":\"entry\"},\"goal\":{\"type\":\"exit\"}}")
				TEXT("}"))
			&& SaveSimulatorLaunchTestFile(
				FPaths::Combine(projectPath, TEXT("policy"), TEXT("__init__.py")),
				TEXT("def create_policy():\n    return None\n"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchCommandLineBuildTest,
	"OdiroSim.SimulatorLaunch.CommandLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchCommandLineBuildTest::RunTest(const FString& parameters)
{
	// Launcher는 project/run만 넘기고 실행 설정은 run snapshot에서 읽는다.
	const FString simulatorArguments = USimulatorLaunchSubsystem::BuildProjectRunSimulatorArgumentString(
		TEXT("X:/Projects/DeliveryBotA"),
		TEXT("000001"));
	TestTrue(TEXT("simulator passes project path"), simulatorArguments.Contains(TEXT("\"-OdiroProject=X:/Projects/DeliveryBotA\"")));
	TestTrue(TEXT("simulator passes run id"), simulatorArguments.Contains(TEXT("\"-RunId=000001\"")));
	TestFalse(TEXT("simulator omits legacy simulate setup"), simulatorArguments.Contains(TEXT("-Simulate")));
	TestFalse(TEXT("simulator omits fixed step args"), simulatorArguments.Contains(TEXT("UseFixedTimeStep")));

	// 개발 fallback도 packaged exe와 같은 public args를 유지해야 한다.
	const FString previewArguments = USimulatorLaunchSubsystem::BuildProjectRunPreviewLauncherArgumentString(
		TEXT("Task-RunPreview.bat"),
		TEXT("X:/Projects/DeliveryBotA"),
		TEXT("000001"));
	TestTrue(TEXT("preview uses cmd run wrapper"), previewArguments.StartsWith(TEXT("/d /s /c \"\"")));
	TestTrue(TEXT("preview passes project path"), previewArguments.Contains(TEXT("\"-OdiroProject=X:/Projects/DeliveryBotA\"")));
	TestTrue(TEXT("preview passes run id"), previewArguments.Contains(TEXT("\"-RunId=000001\"")));
	TestFalse(TEXT("preview omits legacy simulate setup"), previewArguments.Contains(TEXT("-Simulate")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchProjectRunRejectsInvalidScenarioTest,
	"OdiroSim.SimulatorLaunch.ProjectRun.RejectsInvalidScenario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchProjectRunRejectsInvalidScenarioTest::RunTest(const FString& parameters)
{
	const FString projectPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/SimulatorLaunchProjectRunInvalidReject"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TestTrue(TEXT("write project inputs"), WriteSimulatorLaunchProject(projectPath));
	TestTrue(
		TEXT("overwrite invalid scenario"),
		SaveSimulatorLaunchTestFile(
			FPaths::Combine(projectPath, TEXT("scenario.json")),
			TEXT("{")
			TEXT("\"schema\":\"scenario\",")
			TEXT("\"version\":1,")
			TEXT("\"scenario_id\":\"invalid_launcher_scenario\",")
			TEXT("\"intent\":\"Invalid launcher\",")
			TEXT("\"corridor\":{\"segments\":[{\"id\":\"main\",\"along_range_m\":[0.0,10.0]}]},")
			TEXT("\"obstacles\":{},")
			TEXT("\"pedestrians\":{},")
			TEXT("\"robot\":{\"start\":{\"segment\":\"main\"},\"goal\":{\"segment\":\"main\"}}")
			TEXT("}")));

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!subsystem)
	{
		IFileManager::Get().DeleteDirectory(*projectPath, false, true);
		return false;
	}

	FString runId;
	TArray<FString> diagnostics;
	TestFalse(TEXT("prepare rejects invalid scenario"), subsystem->PrepareProjectRunSnapshot(projectPath, FString(), runId, diagnostics));
	TestTrue(TEXT("diagnostics include missing axis"), HasSimulatorLaunchDiagnosticContaining(diagnostics, TEXT("missing_axis")));
	TestTrue(TEXT("diagnostics include missing robot anchor type"), HasSimulatorLaunchDiagnosticContaining(diagnostics, TEXT("missing_type")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchRunStateTerminalTest,
	"OdiroSim.SimulatorLaunch.TerminalStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchRunStateTerminalTest::RunTest(const FString& parameters)
{
	TestFalse(TEXT("pending is not terminal"), USimulatorLaunchSubsystem::IsTerminalRunState(ESimulationRunState::Pending));
	TestFalse(TEXT("running is not terminal"), USimulatorLaunchSubsystem::IsTerminalRunState(ESimulationRunState::Running));
	TestTrue(TEXT("completed is terminal"), USimulatorLaunchSubsystem::IsTerminalRunState(ESimulationRunState::Completed));
	TestTrue(TEXT("failed is terminal"), USimulatorLaunchSubsystem::IsTerminalRunState(ESimulationRunState::Failed));
	TestTrue(TEXT("canceled is terminal"), USimulatorLaunchSubsystem::IsTerminalRunState(ESimulationRunState::Canceled));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchProjectRunValidationTest,
	"OdiroSim.SimulatorLaunch.ProjectRun.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchProjectRunValidationTest::RunTest(const FString& parameters)
{
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!subsystem)
	{
		return false;
	}

	const bool bStarted = subsystem->StartProjectRun(TEXT("X:/Odiro/MissingProject"), TEXT("000001"));
	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();

	TestFalse(TEXT("missing project does not start"), bStarted);
	TestTrue(TEXT("run info records project mode"), runInfo.bProjectRun);
	TestEqual(TEXT("run id recorded"), runInfo.RunId, FString(TEXT("000001")));
	TestEqual(TEXT("failed state"), runInfo.Status.State, ESimulationRunState::Failed);
	TestFalse(TEXT("error recorded"), runInfo.LastError.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchProjectRunSnapshotPrepareTest,
	"OdiroSim.SimulatorLaunch.ProjectRun.PrepareSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchProjectRunSnapshotPrepareTest::RunTest(const FString& parameters)
{
	const FString projectPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/SimulatorLaunchProjectRunPrepare"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TestTrue(TEXT("write project inputs"), WriteSimulatorLaunchProject(projectPath));

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!subsystem)
	{
		return false;
	}

	FString runId;
	TArray<FString> diagnostics;
	const bool bPrepared = subsystem->PrepareProjectRunSnapshot(projectPath, FString(), runId, diagnostics);
	for (const FString& diagnostic : diagnostics)
	{
		AddInfo(FString::Printf(TEXT("PrepareProjectRunSnapshot diagnostic: %s"), *diagnostic));
	}
	TestTrue(TEXT("prepare snapshot"), bPrepared);
	if (!bPrepared)
	{
		return false;
	}
	TestEqual(TEXT("first run id"), runId, FString(TEXT("000001")));
	TestEqual(TEXT("diagnostics"), diagnostics.Num(), 0);

	const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(projectPath, runId);
	TestTrue(TEXT("snapshot setting exists"), FPaths::FileExists(paths.SettingPath));
	TestTrue(TEXT("snapshot profile exists"), FPaths::FileExists(paths.ProfilePath));
	TestTrue(TEXT("snapshot scenario exists"), FPaths::FileExists(paths.ScenarioPath));
	TestTrue(TEXT("snapshot policy entrypoint exists"), FPaths::FileExists(paths.PolicyEntrypointPath));

	const FUserProjectRunSnapshotParseResult parseResult = FUserProjectRunSnapshot::Parse(projectPath, runId);
	TestTrue(TEXT("snapshot parses"), parseResult.bSuccess);

	FString secondRunId;
	TestTrue(TEXT("prepare second snapshot"), subsystem->PrepareProjectRunSnapshot(projectPath, FString(), secondRunId, diagnostics));
	TestEqual(TEXT("second run id"), secondRunId, FString(TEXT("000002")));

	FString thirdRunId;
	TestTrue(TEXT("prepare third snapshot"), subsystem->PrepareProjectRunSnapshot(projectPath, FString(), thirdRunId, diagnostics));
	TestEqual(TEXT("third run id"), thirdRunId, FString(TEXT("000003")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchProjectRunRejectsExistingOutputTest,
	"OdiroSim.SimulatorLaunch.ProjectRun.RejectsExistingOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchProjectRunRejectsExistingOutputTest::RunTest(const FString& parameters)
{
	(void)parameters;

	const FString projectPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/SimulatorLaunchProjectRunRejectExisting"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TestTrue(TEXT("write project inputs"), WriteSimulatorLaunchProject(projectPath));

	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!subsystem)
	{
		return false;
	}

	FString runId;
	TArray<FString> diagnostics;
	TestTrue(TEXT("prepare snapshot"), subsystem->PrepareProjectRunSnapshot(projectPath, FString(), runId, diagnostics));
	TestEqual(TEXT("prepared run id"), runId, FString(TEXT("000001")));

	const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(projectPath, runId);
	TestTrue(
		TEXT("write existing status artifact"),
		SaveSimulatorLaunchTestFile(paths.StatusPath, TEXT("{\"schema\":\"simulation_run_status\",\"run_id\":\"000001\"}")));

	const bool bStarted = subsystem->StartProjectRun(projectPath, runId);
	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
	TestFalse(TEXT("existing output run does not start"), bStarted);
	TestTrue(TEXT("error explains existing output"), runInfo.LastError.Contains(TEXT("already has output artifacts")));
	TestEqual(TEXT("blocked run id recorded"), runInfo.RunId, runId);
	TestEqual(TEXT("blocked state"), runInfo.Status.State, ESimulationRunState::Failed);

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchProjectPresetsTest,
	"OdiroSim.SimulatorLaunch.ProjectPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchProjectPresetsTest::RunTest(const FString& parameters)
{
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* subsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	TestNotNull(TEXT("subsystem created"), subsystem);
	if (!subsystem)
	{
		return false;
	}

	const FProjectPresetCatalog catalog = subsystem->ListProjectPresets();
	TestTrue(TEXT("scenario presets include blank"), catalog.ScenarioPresetIds.Contains(TEXT("blank")));
	TestTrue(TEXT("scenario presets include curved road"), catalog.ScenarioPresetIds.Contains(TEXT("curved-road")));
	TestTrue(TEXT("scenario presets include demo"), catalog.ScenarioPresetIds.Contains(TEXT("demo")));
	TestTrue(TEXT("profile presets include basic"), catalog.ProfilePresetIds.Contains(TEXT("basic")));
	TestTrue(TEXT("profile presets include full"), catalog.ProfilePresetIds.Contains(TEXT("full")));
	TestTrue(TEXT("policy presets include blank"), catalog.PolicyPresetIds.Contains(TEXT("blank")));
	TestTrue(TEXT("policy presets include demo"), catalog.PolicyPresetIds.Contains(TEXT("demo")));

	const FString projectPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/SimulatorLaunchProjectPresets"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));

	FProjectPresetSelection selection;
	selection.ScenarioPresetId = TEXT("curved-road");
	selection.ProfilePresetId = TEXT("full");
	selection.PolicyPresetId = TEXT("demo");

	TArray<FString> diagnostics;
	TestTrue(TEXT("create project from selected presets"), subsystem->CreateProjectFromPresets(projectPath, selection, diagnostics));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("preset create diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}
	TestTrue(TEXT("created scenario exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("scenario.json"))));
	TestTrue(TEXT("created profile exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("profile.json"))));
	TestTrue(TEXT("created default setting exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("setting.json"))));
	TestTrue(TEXT("created demo policy exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("policy/action.py"))));
	TestTrue(TEXT("created runs directory exists"), IFileManager::Get().DirectoryExists(*FPaths::Combine(projectPath, TEXT("runs"))));

	FProjectPresetSelection invalidSelection;
	invalidSelection.ScenarioPresetId = TEXT("../bad");
	diagnostics.Reset();
	TestFalse(
		TEXT("invalid scenario preset id rejected"),
		subsystem->CreateProjectFromPresets(
			FPaths::Combine(projectPath, TEXT("invalid-child")),
			invalidSelection,
			diagnostics));
	TestTrue(TEXT("invalid preset diagnostic"), HasSimulatorLaunchDiagnosticContaining(diagnostics, TEXT("scenario preset id")));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

#endif
