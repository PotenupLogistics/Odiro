#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/Widget/MainMenuWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Platform/SimulatorLaunchSubsystem.h"

namespace
{
	FString MakeMainMenuProjectModeTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/MainMenuProjectMode"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMainMenuProjectModeSmokeTest,
	"OdiroSim.MainMenu.ProjectMode.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMainMenuProjectModeSmokeTest::RunTest(const FString& parameters)
{
	UMainMenuWidget* widget = NewObject<UMainMenuWidget>();
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
	TestEqual(TEXT("project path selected"), widget->GetProjectPathForPrototype(), projectPath);

	TArray<FString> diagnostics;
	TestTrue(
		TEXT("create project through MainMenu project mode"),
		widget->CreateSelectedProjectForPrototype(diagnostics, subsystem));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("create diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}

	TestTrue(
		TEXT("validate project through MainMenu project mode"),
		widget->ValidateSelectedProjectForPrototype(diagnostics, subsystem));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("validate diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}

	FString runId;
	TestTrue(
		TEXT("create run through MainMenu project mode"),
		widget->CreateProjectRunForPrototype(runId, diagnostics, subsystem));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("run diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}
	TestEqual(TEXT("first run selected"), widget->GetProjectRunIdForPrototype(), FString(TEXT("000001")));

	const FString runPath = FPaths::Combine(projectPath, TEXT("runs"), runId);
	TestTrue(TEXT("run snapshot setting exists"), FPaths::FileExists(FPaths::Combine(runPath, TEXT("snapshot/setting.json"))));
	TestTrue(TEXT("run snapshot policy exists"), FPaths::FileExists(FPaths::Combine(runPath, TEXT("snapshot/policy/__init__.py"))));

	IFileManager::Get().DeleteDirectory(*projectPath, false, true);
	return true;
}

#endif
