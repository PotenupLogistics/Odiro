#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/SimulatorProcessSubsystem.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorProcessMapIdTest,
	"OdiroSim.SimulatorProcess.MapId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorProcessMapIdTest::RunTest(const FString& parameters)
{
	TestEqual(
		TEXT("short map id stays short"),
		USimulatorProcessSubsystem::NormalizeMapIdForOpenLevel(TEXT("ScenarioSimulationMap")),
		FString(TEXT("ScenarioSimulationMap")));
	TestEqual(
		TEXT("object path drops asset object suffix"),
		USimulatorProcessSubsystem::NormalizeMapIdForOpenLevel(TEXT("/Game/Maps/ScenarioSimulationMap.ScenarioSimulationMap")),
		FString(TEXT("/Game/Maps/ScenarioSimulationMap")));
	TestEqual(
		TEXT("package path short name"),
		USimulatorProcessSubsystem::GetMapShortNameFromId(TEXT("/Game/Maps/ScenarioSimulationMap")),
		FString(TEXT("ScenarioSimulationMap")));
	TestEqual(
		TEXT("empty map id falls back"),
		USimulatorProcessSubsystem::NormalizeMapIdForOpenLevel(FString()),
		FString(TEXT("ScenarioSimulationMap")));
	TestFalse(
		TEXT("null world never matches"),
		USimulatorProcessSubsystem::DoesWorldMatchMapId(nullptr, TEXT("ScenarioSimulationMap")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorProcessFixedStepTest,
	"OdiroSim.SimulatorProcess.FixedStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorProcessFixedStepTest::RunTest(const FString& parameters)
{
	TestTrue(
		TEXT("60 fps fixed delta"),
		FMath::IsNearlyEqual(USimulatorProcessSubsystem::CalculateFixedDeltaSeconds(60), 1.0 / 60.0));
	TestTrue(
		TEXT("invalid fps clamps to one"),
		FMath::IsNearlyEqual(USimulatorProcessSubsystem::CalculateFixedDeltaSeconds(0), 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimulatorProcessRunnerStateTest,
	"OdiroSim.SimulatorProcess.RunnerState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimulatorProcessRunnerStateTest::RunTest(const FString& parameters)
{
	TestEqual(
		TEXT("preparing maps to running"),
		USimulatorProcessSubsystem::ConvertRunnerStateToRunState(EScenarioRunnerState::Preparing),
		ESimulationRunState::Running);
	TestEqual(
		TEXT("completed maps to completed"),
		USimulatorProcessSubsystem::ConvertRunnerStateToRunState(EScenarioRunnerState::Completed),
		ESimulationRunState::Completed);
	TestEqual(
		TEXT("cancelled maps to canceled"),
		USimulatorProcessSubsystem::ConvertRunnerStateToRunState(EScenarioRunnerState::Cancelled),
		ESimulationRunState::Canceled);

	return true;
}

#endif
