#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/SimulatorLaunchSubsystem.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchCommandLineBuildTest,
	"ProtoRobotSim.SimulatorLaunch.CommandLine",
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
		TEXT("RunPreview.bat"),
		TEXT("Json/Input/SimulationSetupSample.json"),
		TEXT("run-001"));
	TestTrue(TEXT("preview uses cmd run wrapper"), previewArguments.StartsWith(TEXT("/d /s /c \"\"")));
	TestTrue(TEXT("preview passes simulate setup"), previewArguments.Contains(TEXT("\"-Simulate=Json/Input/SimulationSetupSample.json\"")));
	TestTrue(TEXT("preview passes run id"), previewArguments.Contains(TEXT("\"-RunId=run-001\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorLaunchRunStateTerminalTest,
	"ProtoRobotSim.SimulatorLaunch.TerminalStates",
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

#endif
