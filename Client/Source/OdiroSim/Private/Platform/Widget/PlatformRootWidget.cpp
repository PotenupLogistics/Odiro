#include "Platform/Widget/PlatformRootWidget.h"

#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/StartupScreenViewModel.h"
#include "Platform/Widget/ProjectCreateScreenWidget.h"
#include "Platform/Widget/ProjectOverviewScreenWidget.h"
#include "Platform/Widget/RobotConfigScreenWidget.h"
#include "Platform/Widget/RunDetailScreenWidget.h"
#include "Platform/Widget/RunListScreenWidget.h"
#include "Platform/Widget/ScenarioEditorScreenWidget.h"
#include "Platform/Widget/StartupScreenWidget.h"
#include "Platform/Widget/WindowStatusBarWidget.h"
#include "Platform/Widget/WindowTabBarWidget.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlatformRootWidget, Log, All);

namespace
{
	const FName AnalyzeActionId(TEXT("Analyze"));
	const FName BackActionId(TEXT("Back"));
	const FString ResultTabPrefix(TEXT("RunResult_"));

	FWindowActionButtonConfig MakeRootActionConfig(FWindowActionButtonConfig config, const FName actionId)
	{
		config.ActionId = actionId;
		return config;
	}

	void LogMissingPlatformScreen(const EPlatformRootScreen screen)
	{
		const TCHAR* screenName = TEXT("Unknown");
		switch (screen)
		{
		case EPlatformRootScreen::ProjectOverview:
			screenName = TEXT("ProjectOverviewScreen");
			break;
		case EPlatformRootScreen::ScenarioEditor:
			screenName = TEXT("ScenarioEditorScreen");
			break;
		case EPlatformRootScreen::RobotConfig:
			screenName = TEXT("RobotConfigScreen");
			break;
		case EPlatformRootScreen::RunList:
			screenName = TEXT("RunListScreen");
			break;
		case EPlatformRootScreen::RunDetail:
			screenName = TEXT("RunDetailScreen");
			break;
		case EPlatformRootScreen::ProjectCreate:
			screenName = TEXT("ProjectCreateScreen");
			break;
		case EPlatformRootScreen::Startup:
			screenName = TEXT("StartupScreen");
			break;
		default:
			break;
		}
		UE_LOG(
			LogPlatformRootWidget,
			Error,
			TEXT("WBP_Root is missing required screen child: %s."),
			screenName);
	}

	void LogScenarioEditorActionBlocked(const TCHAR* actionName)
	{
		UE_LOG(
			LogPlatformRootWidget,
			Error,
			TEXT("Scenario editor %s action was blocked because WBP_Root could not resolve WBP_ScenarioEditorScreen -> ScenarioEditorRootWidget."),
			actionName);
	}
}

void UPlatformRootWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ConfigureStatusBarForActiveScreen();
}

void UPlatformRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	const bool bHasActiveProject = HasActiveProject();
	BindControls();
	if (!bHasActiveProject)
	{
		ActiveScreen = EPlatformRootScreen::Startup;
	}
	else if (ActiveScreen == EPlatformRootScreen::Startup && ShouldUseProjectWorkspaceDefault())
	{
		ActiveScreen = EPlatformRootScreen::ProjectOverview;
	}
	SetActiveScreen(ActiveScreen);
	ApplyRootInputMode();
}

void UPlatformRootWidget::NativeDestruct()
{
	UpdateRobotPreviewActivation(ActiveScreen, EPlatformRootScreen::Startup);
	UnbindControls();
	OnActiveScreenChangedNative.Clear();
	Super::NativeDestruct();
}

void UPlatformRootWidget::ShowStartupScreen()
{
	SetActiveScreen(EPlatformRootScreen::Startup);
}

void UPlatformRootWidget::ShowProjectCreateScreen()
{
	SetActiveScreen(EPlatformRootScreen::ProjectCreate);
}

void UPlatformRootWidget::ShowProjectOverviewScreen()
{
	SetActiveScreen(EPlatformRootScreen::ProjectOverview);
}

void UPlatformRootWidget::ShowScenarioEditorScreen()
{
	SetActiveScreen(EPlatformRootScreen::ScenarioEditor);
}

void UPlatformRootWidget::ShowRobotConfigScreen()
{
	SetActiveScreen(EPlatformRootScreen::RobotConfig);
}

void UPlatformRootWidget::ShowRunListScreen()
{
	SetActiveScreen(EPlatformRootScreen::RunList);
}

void UPlatformRootWidget::ShowRunDetailScreen(const FString& runId)
{
	ActiveRunDetailId = runId.TrimStartAndEnd();
	SetActiveScreen(EPlatformRootScreen::RunDetail);
}

void UPlatformRootWidget::SetActiveScreen(const EPlatformRootScreen screen)
{
	const EPlatformRootScreen PreviousScreen = ActiveScreen;
	ActiveScreen = (!HasActiveProject()
		&& screen != EPlatformRootScreen::Startup
		&& screen != EPlatformRootScreen::ProjectCreate)
		? EPlatformRootScreen::Startup
		: screen;

	if (PreviousScreen == EPlatformRootScreen::RobotConfig && ActiveScreen != EPlatformRootScreen::RobotConfig)
	{
		UpdateRobotPreviewActivation(PreviousScreen, ActiveScreen);
	}

	if (ScreenContentSwitcher)
	{
		switch (ActiveScreen)
		{
		case EPlatformRootScreen::ProjectCreate:
			if (ProjectCreateScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(ProjectCreateScreen.Get());
				ProjectCreateScreen->RefreshFromViewModel();
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		case EPlatformRootScreen::ProjectOverview:
			if (ProjectOverviewScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(ProjectOverviewScreen.Get());
				ProjectOverviewScreen->RefreshFromViewModel();
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		case EPlatformRootScreen::ScenarioEditor:
			if (ScenarioEditorScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(ScenarioEditorScreen.Get());
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		case EPlatformRootScreen::RobotConfig:
			if (RobotConfigScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(RobotConfigScreen.Get());
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		case EPlatformRootScreen::RunList:
			if (RunListScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(RunListScreen.Get());
				RunListScreen->RefreshFromViewModels();
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		case EPlatformRootScreen::RunDetail:
			if (RunDetailScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(RunDetailScreen.Get());
				RunDetailScreen->ShowRun(ActiveRunDetailId);
				ActiveRunDetailId = RunDetailScreen->GetDisplayedRunId();
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		case EPlatformRootScreen::Startup:
		default:
			if (StartupScreen)
			{
				ScreenContentSwitcher->SetActiveWidget(StartupScreen.Get());
				StartupScreen->RefreshFromViewModel();
			}
			else
			{
				LogMissingPlatformScreen(ActiveScreen);
			}
			break;
		}
	}

	if (ActiveScreen == EPlatformRootScreen::RobotConfig)
	{
		UpdateRobotPreviewActivation(PreviousScreen, ActiveScreen);
	}

	ConfigureStatusBarForActiveScreen();
	OnActiveScreenChangedNative.Broadcast(ActiveScreen);
}

void UPlatformRootWidget::UpdateRobotPreviewActivation(
	const EPlatformRootScreen previousScreen,
	const EPlatformRootScreen nextScreen)
{
	URobotConfigScreenWidget* robotConfigScreen = Cast<URobotConfigScreenWidget>(RobotConfigScreen.Get());
	if (!robotConfigScreen)
	{
		return;
	}

	if (previousScreen == EPlatformRootScreen::RobotConfig && nextScreen != EPlatformRootScreen::RobotConfig)
	{
		robotConfigScreen->SetRobotPreviewActive(false);
	}

	if (nextScreen == EPlatformRootScreen::RobotConfig)
	{
		robotConfigScreen->SetRobotPreviewActive(true);
	}
}

UScenarioEditorRootWidget* UPlatformRootWidget::GetScenarioEditorRootWidget() const
{
	UWidget* scenarioEditorScreenWidget = ScenarioEditorScreen.Get();
	if (!IsValid(scenarioEditorScreenWidget))
	{
		UE_LOG(
			LogPlatformRootWidget,
			Error,
			TEXT("WBP_Root is missing required ScenarioEditorScreen child; expected WBP_Root -> WBP_ScenarioEditorScreen -> ScenarioEditorRootWidget."));
		return nullptr;
	}

	const UScenarioEditorScreenWidget* scenarioEditorScreen = Cast<UScenarioEditorScreenWidget>(scenarioEditorScreenWidget);
	if (scenarioEditorScreen)
	{
		UScenarioEditorRootWidget* scenarioEditorRootWidget = scenarioEditorScreen->GetScenarioEditorRootWidget();
		if (!IsValid(scenarioEditorRootWidget))
		{
			UE_LOG(
				LogPlatformRootWidget,
				Error,
				TEXT("WBP_ScenarioEditorScreen is missing required ScenarioEditorRootWidget child; expected WBP_Root -> WBP_ScenarioEditorScreen -> ScenarioEditorRootWidget."));
			return nullptr;
		}

		return scenarioEditorRootWidget;
	}

	if (UScenarioEditorRootWidget* scenarioEditorRootWidget = Cast<UScenarioEditorRootWidget>(scenarioEditorScreenWidget))
	{
		return scenarioEditorRootWidget;
	}

	UE_LOG(
		LogPlatformRootWidget,
		Error,
		TEXT("WBP_Root ScenarioEditorScreen has invalid class '%s' on widget '%s'; expected UScenarioEditorScreenWidget or UScenarioEditorRootWidget."),
		*GetNameSafe(scenarioEditorScreenWidget->GetClass()),
		*GetNameSafe(scenarioEditorScreenWidget));
	return nullptr;
}

void UPlatformRootWidget::BindControls()
{
	if (StartupScreen)
	{
		StartupScreen->OnCreateProjectRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleStartupCreateProjectRequested);
		StartupScreen->OnCreateProjectRequested.AddDynamic(this, &UPlatformRootWidget::HandleStartupCreateProjectRequested);
		StartupScreen->OnProjectOpened.RemoveDynamic(this, &UPlatformRootWidget::HandleStartupProjectOpened);
		StartupScreen->OnProjectOpened.AddDynamic(this, &UPlatformRootWidget::HandleStartupProjectOpened);
	}

	if (ProjectCreateScreen)
	{
		ProjectCreateScreen->OnCancelRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleProjectCreateCancelRequested);
		ProjectCreateScreen->OnCancelRequested.AddDynamic(this, &UPlatformRootWidget::HandleProjectCreateCancelRequested);
		ProjectCreateScreen->OnProjectCreated.RemoveDynamic(this, &UPlatformRootWidget::HandleProjectCreated);
		ProjectCreateScreen->OnProjectCreated.AddDynamic(this, &UPlatformRootWidget::HandleProjectCreated);
	}

	if (ProjectOverviewScreen)
	{
		ProjectOverviewScreen->OnScenarioRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleOverviewScenarioRequested);
		ProjectOverviewScreen->OnScenarioRequested.AddDynamic(this, &UPlatformRootWidget::HandleOverviewScenarioRequested);
		ProjectOverviewScreen->OnRobotRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleOverviewRobotRequested);
		ProjectOverviewScreen->OnRobotRequested.AddDynamic(this, &UPlatformRootWidget::HandleOverviewRobotRequested);
		ProjectOverviewScreen->OnExperimentRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleOverviewExperimentRequested);
		ProjectOverviewScreen->OnExperimentRequested.AddDynamic(this, &UPlatformRootWidget::HandleOverviewExperimentRequested);
	}

	if (RunListScreen)
	{
		RunListScreen->OnRunDetailRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleRunDetailRequested);
		RunListScreen->OnRunDetailRequested.AddDynamic(this, &UPlatformRootWidget::HandleRunDetailRequested);
	}

	if (WindowStatusBar)
	{
		WindowStatusBar->OnTabSelectedNative.RemoveAll(this);
		WindowStatusBar->OnTabSelectedNative.AddUObject(this, &UPlatformRootWidget::HandleStatusBarTabSelected);
		WindowStatusBar->OnActionRequestedNative.RemoveAll(this);
		WindowStatusBar->OnActionRequestedNative.AddUObject(this, &UPlatformRootWidget::HandleStatusBarActionRequested);
		WindowStatusBar->OnResultTabCloseRequestedNative.RemoveAll(this);
		WindowStatusBar->OnResultTabCloseRequestedNative.AddUObject(
			this,
			&UPlatformRootWidget::HandleStatusBarResultTabCloseRequested);
	}
}

void UPlatformRootWidget::UnbindControls()
{
	if (StartupScreen)
	{
		StartupScreen->OnCreateProjectRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleStartupCreateProjectRequested);
		StartupScreen->OnProjectOpened.RemoveDynamic(this, &UPlatformRootWidget::HandleStartupProjectOpened);
	}

	if (ProjectCreateScreen)
	{
		ProjectCreateScreen->OnCancelRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleProjectCreateCancelRequested);
		ProjectCreateScreen->OnProjectCreated.RemoveDynamic(this, &UPlatformRootWidget::HandleProjectCreated);
	}

	if (ProjectOverviewScreen)
	{
		ProjectOverviewScreen->OnScenarioRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleOverviewScenarioRequested);
		ProjectOverviewScreen->OnRobotRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleOverviewRobotRequested);
		ProjectOverviewScreen->OnExperimentRequested.RemoveDynamic(
			this,
			&UPlatformRootWidget::HandleOverviewExperimentRequested);
	}

	if (RunListScreen)
	{
		RunListScreen->OnRunDetailRequested.RemoveDynamic(this, &UPlatformRootWidget::HandleRunDetailRequested);
	}

	if (WindowStatusBar)
	{
		WindowStatusBar->OnTabSelectedNative.RemoveAll(this);
		WindowStatusBar->OnActionRequestedNative.RemoveAll(this);
		WindowStatusBar->OnResultTabCloseRequestedNative.RemoveAll(this);
	}
}

void UPlatformRootWidget::ConfigureStatusBarForActiveScreen()
{
	if (!WindowStatusBar)
	{
		return;
	}

	const bool bWorkspaceTabsVisible = HasActiveProject();

	WindowStatusBar->SetTabBarVisible(true);
	WindowStatusBar->SetTabVisible(UWindowTabBarWidget::GetStartupTabId(), true);
	WindowStatusBar->SetTabVisible(UWindowTabBarWidget::GetOverviewTabId(), bWorkspaceTabsVisible);
	WindowStatusBar->SetTabVisible(UWindowTabBarWidget::GetScenarioTabId(), bWorkspaceTabsVisible);
	WindowStatusBar->SetTabVisible(UWindowTabBarWidget::GetRobotTabId(), bWorkspaceTabsVisible);
	WindowStatusBar->SetTabVisible(UWindowTabBarWidget::GetExperimentTabId(), bWorkspaceTabsVisible);

	TArray<FWindowTabConfig> resultTabs;
	if (ActiveScreen == EPlatformRootScreen::RunDetail && !ActiveRunDetailId.IsEmpty())
	{
		FWindowTabConfig resultTab;
		resultTab.TabId = BuildRunDetailTabId();
		resultTab.Label = FText::Format(
			NSLOCTEXT("OdiroPlatform", "RunDetailResultTabLabel", "실험 결과 {0}"),
			FText::FromString(ActiveRunDetailId));
		resultTab.bVisible = true;
		resultTab.bClosable = true;
		resultTabs.Add(resultTab);
	}
	WindowStatusBar->SetResultTabs(resultTabs);

	TArray<FWindowActionButtonConfig> actionConfigs;
	switch (ActiveScreen)
	{
	case EPlatformRootScreen::ProjectCreate:
		WindowStatusBar->SetActiveTab(UWindowTabBarWidget::GetStartupTabId());
		actionConfigs.Add(MakeRootActionConfig(
			ProjectCreateConfirmActionConfig,
			UWindowStatusBarWidget::GetConfirmActionId()));
		break;
	case EPlatformRootScreen::ProjectOverview:
		WindowStatusBar->SetActiveTab(UWindowTabBarWidget::GetOverviewTabId());
		break;
	case EPlatformRootScreen::ScenarioEditor:
		WindowStatusBar->SetActiveTab(UWindowTabBarWidget::GetScenarioTabId());
		actionConfigs.Add(MakeRootActionConfig(
			SaveActionConfig,
			UWindowStatusBarWidget::GetConfirmActionId()));
		actionConfigs.Add(MakeRootActionConfig(
			RunActionConfig,
			UWindowStatusBarWidget::GetRunActionId()));
		break;
	case EPlatformRootScreen::RobotConfig:
		WindowStatusBar->SetActiveTab(UWindowTabBarWidget::GetRobotTabId());
		break;
	case EPlatformRootScreen::RunList:
		WindowStatusBar->SetActiveTab(UWindowTabBarWidget::GetExperimentTabId());
		actionConfigs.Add(MakeRootActionConfig(
			AnalyzeActionConfig,
			AnalyzeActionId));
		actionConfigs.Add(MakeRootActionConfig(
			RunActionConfig,
			UWindowStatusBarWidget::GetRunActionId()));
		break;
	case EPlatformRootScreen::RunDetail:
		WindowStatusBar->SetActiveTab(BuildRunDetailTabId());
		actionConfigs.Add(MakeRootActionConfig(
			BackActionConfig,
			BackActionId));
		break;
	case EPlatformRootScreen::Startup:
	default:
		WindowStatusBar->SetActiveTab(UWindowTabBarWidget::GetStartupTabId());
		break;
	}

	WindowStatusBar->SetActionButtons(actionConfigs);
}

bool UPlatformRootWidget::ShouldUseProjectWorkspaceDefault() const
{
	return HasActiveProject();
}

bool UPlatformRootWidget::HasActiveProject() const
{
	const UWorld* world = GetWorld();
	const UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	const UProjectSessionSubsystem* projectSession = gameInstance
		? gameInstance->GetSubsystem<UProjectSessionSubsystem>()
		: nullptr;
	return projectSession && projectSession->HasActiveProject();
}

FName UPlatformRootWidget::BuildRunDetailTabId() const
{
	return ActiveRunDetailId.IsEmpty()
		? NAME_None
		: FName(*(ResultTabPrefix + ActiveRunDetailId));
}

FString UPlatformRootWidget::ExtractRunIdFromResultTabId(const FName tabId)
{
	const FString tabString = tabId.ToString();
	return tabString.StartsWith(ResultTabPrefix)
		? tabString.RightChop(ResultTabPrefix.Len())
		: FString();
}

void UPlatformRootWidget::ApplyRootInputMode()
{
	APlayerController* playerController = GetOwningPlayer();
	if (!playerController)
	{
		playerController = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!playerController)
	{
		return;
	}

	FInputModeGameAndUI inputMode;
	inputMode.SetWidgetToFocus(TakeWidget());
	inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	inputMode.SetHideCursorDuringCapture(false);
	playerController->SetInputMode(inputMode);
	playerController->bShowMouseCursor = true;
	playerController->bEnableClickEvents = true;
	playerController->bEnableMouseOverEvents = true;
}

void UPlatformRootWidget::HandleStartupCreateProjectRequested(UStartupScreenWidget* startupScreen)
{
	if (IsValid(startupScreen) && startupScreen == StartupScreen)
	{
		ShowProjectCreateScreen();
	}
}

void UPlatformRootWidget::HandleStartupProjectOpened(UStartupScreenWidget* startupScreen, const FString&)
{
	if (!IsValid(startupScreen) || startupScreen != StartupScreen)
	{
		return;
	}

	if (UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this))
	{
		platformUiSubsystem->RefreshFromProjectSession();
	}
	ShowProjectOverviewScreen();
}

void UPlatformRootWidget::HandleProjectCreateCancelRequested(UProjectCreateScreenWidget* projectCreateScreen)
{
	if (IsValid(projectCreateScreen) && projectCreateScreen == ProjectCreateScreen)
	{
		ShowStartupScreen();
	}
}

void UPlatformRootWidget::HandleProjectCreated(
	UProjectCreateScreenWidget* projectCreateScreen,
	const FString& projectPath)
{
	if (!IsValid(projectCreateScreen) || projectCreateScreen != ProjectCreateScreen)
	{
		return;
	}

	if (StartupScreen && !projectPath.TrimStartAndEnd().IsEmpty())
	{
		StartupScreen->RefreshFromViewModel();
		if (UStartupScreenViewModel* startupViewModel = StartupScreen->GetViewModel())
		{
			startupViewModel->RefreshRecentProjects();
			startupViewModel->SelectProject(projectPath);
		}
		if (StartupScreen->OpenProjectPath(projectPath))
		{
			return;
		}
	}

	ShowProjectOverviewScreen();
}

void UPlatformRootWidget::HandleOverviewScenarioRequested(UProjectOverviewScreenWidget* overviewScreen)
{
	if (IsValid(overviewScreen) && overviewScreen == ProjectOverviewScreen)
	{
		ShowScenarioEditorScreen();
	}
}

void UPlatformRootWidget::HandleOverviewRobotRequested(UProjectOverviewScreenWidget* overviewScreen)
{
	if (IsValid(overviewScreen) && overviewScreen == ProjectOverviewScreen)
	{
		ShowRobotConfigScreen();
	}
}

void UPlatformRootWidget::HandleOverviewExperimentRequested(UProjectOverviewScreenWidget* overviewScreen)
{
	if (IsValid(overviewScreen) && overviewScreen == ProjectOverviewScreen)
	{
		ShowRunListScreen();
	}
}

void UPlatformRootWidget::HandleRunDetailRequested(URunListScreenWidget* runListScreen, const FString& runId)
{
	if (IsValid(runListScreen) && runListScreen == RunListScreen)
	{
		ShowRunDetailScreen(runId);
	}
}

void UPlatformRootWidget::HandleStatusBarTabSelected(const FName tabId)
{
	if (tabId == UWindowTabBarWidget::GetStartupTabId())
	{
		ShowStartupScreen();
		return;
	}
	if (tabId == UWindowTabBarWidget::GetOverviewTabId())
	{
		ShowProjectOverviewScreen();
		return;
	}
	if (tabId == UWindowTabBarWidget::GetScenarioTabId())
	{
		ShowScenarioEditorScreen();
		return;
	}
	if (tabId == UWindowTabBarWidget::GetRobotTabId())
	{
		ShowRobotConfigScreen();
		return;
	}
	if (tabId == UWindowTabBarWidget::GetExperimentTabId())
	{
		ShowRunListScreen();
		return;
	}

	const FString runId = ExtractRunIdFromResultTabId(tabId);
	if (!runId.IsEmpty())
	{
		ShowRunDetailScreen(runId);
	}
}

void UPlatformRootWidget::HandleStatusBarActionRequested(const FName actionId)
{
	if (actionId == UWindowStatusBarWidget::GetConfirmActionId())
	{
		if (ActiveScreen == EPlatformRootScreen::ProjectCreate && ProjectCreateScreen)
		{
			ProjectCreateScreen->CreateCurrentProject();
		}
		else if (ActiveScreen == EPlatformRootScreen::ScenarioEditor)
		{
			if (UScenarioEditorRootWidget* scenarioEditorRoot = GetScenarioEditorRootWidget())
			{
				scenarioEditorRoot->SaveCurrentScenario();
			}
			else
			{
				LogScenarioEditorActionBlocked(TEXT("Save"));
			}
		}
		return;
	}

	if (actionId == UWindowStatusBarWidget::GetRunActionId())
	{
		if (ActiveScreen == EPlatformRootScreen::ScenarioEditor)
		{
			if (UScenarioEditorRootWidget* scenarioEditorRoot = GetScenarioEditorRootWidget())
			{
				scenarioEditorRoot->SaveCurrentScenario();
			}
			else
			{
				LogScenarioEditorActionBlocked(TEXT("Run"));
				return;
			}
			ShowRunListScreen();
			if (RunListScreen)
			{
				RunListScreen->StartNewRun();
			}
		}
		else if (ActiveScreen == EPlatformRootScreen::RunList && RunListScreen)
		{
			RunListScreen->StartNewRun();
		}
		return;
	}

	if (actionId == AnalyzeActionId)
	{
		if (ActiveScreen == EPlatformRootScreen::RunList && RunListScreen)
		{
			RunListScreen->RequestAnalysisForSelectedRun();
		}
		else if (ActiveScreen == EPlatformRootScreen::RunDetail && RunDetailScreen)
		{
			RunDetailScreen->RequestAiAnalysis();
		}
		return;
	}

	if (actionId == BackActionId && ActiveScreen == EPlatformRootScreen::RunDetail)
	{
		if (RunDetailScreen)
		{
			RunDetailScreen->ResetReplay();
		}
		ShowRunListScreen();
	}
}

void UPlatformRootWidget::HandleStatusBarResultTabCloseRequested(const FName tabId)
{
	const FString runId = ExtractRunIdFromResultTabId(tabId);
	if (runId.IsEmpty())
	{
		return;
	}

	if (RunDetailScreen)
	{
		RunDetailScreen->ResetReplay();
	}
	ShowRunListScreen();
}
