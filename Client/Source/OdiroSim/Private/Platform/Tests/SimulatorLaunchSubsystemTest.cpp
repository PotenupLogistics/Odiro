#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/SimulatorLaunchSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

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
	FSimulatorLaunchRunQueueJsonRoundTripTest,
	"OdiroSim.SimulatorLaunch.RunQueueJson.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchRunQueueJsonRoundTripTest::RunTest(const FString& parameters)
{
	TArray<FScenarioRunInput> runInputs;
	FScenarioRunInput runInput;
	runInput.PairId = TEXT("sample_0");
	runInput.ScenarioSetupJsonPath = TEXT("Json/Input/ScenarioSetupSample_0.json");
	runInput.DeliveryBotSetupJsonPath = TEXT("Json/Input/DeliveryBotSetupSample_0.json");
	runInput.PolicySpecJsonPath = TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json");
	runInputs.Add(runInput);

	FString json;
	TArray<FString> diagnostics;
	TestTrue(TEXT("run queue writes"), USimulatorLaunchSubsystem::TryWriteScenarioRunQueueJson(runInputs, json, diagnostics));
	TestEqual(TEXT("write diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("schema field"), json.Contains(TEXT("\"schema\"")));
	TestTrue(TEXT("scenario setup field"), json.Contains(TEXT("\"scenario_setup\"")));
	TestTrue(TEXT("scenario setup path"), json.Contains(TEXT("ScenarioSetupSample_0.json")));
	TestTrue(TEXT("policy spec field"), json.Contains(TEXT("\"policy_spec\"")));

	TArray<FScenarioRunInput> parsedRunInputs;
	TestTrue(TEXT("run queue reads"), USimulatorLaunchSubsystem::TryReadScenarioRunQueueJson(json, parsedRunInputs, diagnostics));
	TestEqual(TEXT("read diagnostics"), diagnostics.Num(), 0);
	TestEqual(TEXT("run input count"), parsedRunInputs.Num(), 1);
	TestEqual(TEXT("pair id"), parsedRunInputs[0].PairId, FString(TEXT("sample_0")));
	TestEqual(
		TEXT("scenario setup"),
		parsedRunInputs[0].ScenarioSetupJsonPath,
		FString(TEXT("Json/Input/ScenarioSetupSample_0.json")));
	TestEqual(
		TEXT("delivery setup"),
		parsedRunInputs[0].DeliveryBotSetupJsonPath,
		FString(TEXT("Json/Input/DeliveryBotSetupSample_0.json")));
	TestEqual(
		TEXT("policy spec"),
		parsedRunInputs[0].PolicySpecJsonPath,
		FString(TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json")));

	return true;
}

#endif
