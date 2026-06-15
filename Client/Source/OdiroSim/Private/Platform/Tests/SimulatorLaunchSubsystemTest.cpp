#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/SimulatorLaunchSubsystem.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchCommandLineBuildTest,
	"OdiroSim.SimulatorLaunch.CommandLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchCommandLineBuildTest::RunTest(const FString& parameters)
{
	// Launcher는 simulator public 계약만 넘기고 fixed-step은 SimulationSetup JSON에만 둔다.
	const FString simulatorArguments = USimulatorLaunchSubsystem::BuildSimulatorArgumentString(
		TEXT("Json/Input/SimulationSetupSample.json"),
		TEXT("run-001"));
	TestTrue(TEXT("simulator passes simulate setup"), simulatorArguments.Contains(TEXT("\"-Simulate=Json/Input/SimulationSetupSample.json\"")));
	TestTrue(TEXT("simulator passes run id"), simulatorArguments.Contains(TEXT("\"-RunId=run-001\"")));
	TestFalse(TEXT("simulator omits fixed step args"), simulatorArguments.Contains(TEXT("UseFixedTimeStep")));

	// 개발 fallback도 packaged exe와 같은 public args를 유지해야 한다.
	const FString previewArguments = USimulatorLaunchSubsystem::BuildPreviewLauncherArgumentString(
		TEXT("Task-RunPreview.bat"),
		TEXT("Json/Input/SimulationSetupSample.json"),
		TEXT("run-001"));
	TestTrue(TEXT("preview uses cmd run wrapper"), previewArguments.StartsWith(TEXT("/d /s /c \"\"")));
	TestTrue(TEXT("preview passes simulate setup"), previewArguments.Contains(TEXT("\"-Simulate=Json/Input/SimulationSetupSample.json\"")));
	TestTrue(TEXT("preview passes run id"), previewArguments.Contains(TEXT("\"-RunId=run-001\"")));

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
	FSimulatorLaunchRunQueueJsonRoundTripTest,
	"OdiroSim.SimulatorLaunch.RunQueueJson.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorLaunchRunQueueJsonRoundTripTest::RunTest(const FString& parameters)
{
	TArray<FScenarioRunInput> runInputs;
	FScenarioRunInput runInput;
	runInput.PairId = TEXT("sample_0");
	runInput.ScenarioSourceJsonPath = TEXT("Json/Input/ScenarioTemplates/FeatureProbeNoPedestrians.template.json");
	runInput.SimulationProfileJsonPath = TEXT("Json/Input/ScenarioTemplates/TemplateProfileForTest.json");
	runInput.PolicySpecJsonPath = TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json");
	runInputs.Add(runInput);

	FString json;
	TArray<FString> diagnostics;
	TestTrue(TEXT("run queue writes"), USimulatorLaunchSubsystem::TryWriteScenarioRunQueueJson(runInputs, json, diagnostics));
	TestEqual(TEXT("write diagnostics"), diagnostics.Num(), 0);
	TestTrue(TEXT("schema field"), json.Contains(TEXT("\"schema\"")));
	TestTrue(TEXT("scenario template field"), json.Contains(TEXT("\"scenario_template\"")));
	TestTrue(TEXT("scenario template path"), json.Contains(TEXT("FeatureProbeNoPedestrians.template.json")));
	TestTrue(TEXT("simulation profile field"), json.Contains(TEXT("\"simulation_profile\"")));
	TestTrue(TEXT("policy spec field"), json.Contains(TEXT("\"policy_spec\"")));

	TArray<FScenarioRunInput> parsedRunInputs;
	TestTrue(TEXT("run queue reads"), USimulatorLaunchSubsystem::TryReadScenarioRunQueueJson(json, parsedRunInputs, diagnostics));
	TestEqual(TEXT("read diagnostics"), diagnostics.Num(), 0);
	TestEqual(TEXT("run input count"), parsedRunInputs.Num(), 1);
	TestEqual(TEXT("pair id"), parsedRunInputs[0].PairId, FString(TEXT("sample_0")));
	TestEqual(
		TEXT("scenario setup"),
		parsedRunInputs[0].ScenarioSourceJsonPath,
		FString(TEXT("Json/Input/ScenarioTemplates/FeatureProbeNoPedestrians.template.json")));
	TestEqual(
		TEXT("delivery setup"),
		parsedRunInputs[0].SimulationProfileJsonPath,
		FString(TEXT("Json/Input/ScenarioTemplates/TemplateProfileForTest.json")));
	TestEqual(
		TEXT("policy spec"),
		parsedRunInputs[0].PolicySpecJsonPath,
		FString(TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json")));

	return true;
}

#endif
