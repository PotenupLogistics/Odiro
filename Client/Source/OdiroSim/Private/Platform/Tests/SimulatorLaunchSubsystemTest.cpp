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

	bool WriteSimulatorLaunchProject(const FString& projectPath)
	{
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
				TEXT("\"corridor\":{},")
				TEXT("\"obstacles\":{},")
				TEXT("\"pedestrians\":{},")
				TEXT("\"robot\":{}")
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
	return true;
}

#endif
