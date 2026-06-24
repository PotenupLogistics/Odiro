#include "Platform/Widget/StartupMenuWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiDeveloperSettings.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/StartupMenuViewModel.h"
#include "Platform/Widget/ProjectTemplateCardWidget.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogStartupMenuWidget, Log, All);

namespace
{
	const TCHAR* DefaultScenarioPresetId = TEXT("blank");
	const TCHAR* DefaultProfilePresetId = TEXT("basic");
	const TCHAR* DefaultPolicyPresetId = TEXT("blank");

	FString NormalizeStartupMenuPath(FString path)
	{
		path = path.TrimStartAndEnd();
		if (path.IsEmpty())
		{
			return FString();
		}

		path = FPaths::IsRelative(path)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), path))
			: FPaths::ConvertRelativePathToFull(path);
		FPaths::NormalizeFilename(path);
		return path;
	}

	FString GetDefaultProjectParentFolder()
	{
		return NormalizeStartupMenuPath(FPlatformProcess::UserDir());
	}

	FString NormalizeProjectDirectoryName(const FString& projectName)
	{
		return FPaths::GetCleanFilename(projectName.TrimStartAndEnd());
	}

	FString BuildProjectPathFromInputs(const FString& parentFolder, const FString& projectName)
	{
		const FString normalizedParentFolder = NormalizeStartupMenuPath(parentFolder);
		const FString normalizedProjectName = NormalizeProjectDirectoryName(projectName);
		if (normalizedParentFolder.IsEmpty() || normalizedProjectName.IsEmpty())
		{
			return FString();
		}

		return NormalizeStartupMenuPath(FPaths::Combine(normalizedParentFolder, normalizedProjectName));
	}

	FString MakeProjectPresetDisplayName(const FString& presetId)
	{
		FString displayName = presetId.TrimStartAndEnd();
		displayName.ReplaceInline(TEXT("-"), TEXT(" "));
		displayName.ReplaceInline(TEXT("_"), TEXT(" "));
		return FName::NameToDisplayString(displayName, false);
	}

	FString MakeRecentProjectDisplayName(const FString& projectPath)
	{
		return FPaths::GetCleanFilename(projectPath);
	}

	FString MakeRecentProjectSubtitle(const FString& projectPath)
	{
		const FString parentFolderPath = FPaths::GetPath(NormalizeStartupMenuPath(projectPath));
		const FString parentFolderName = FPaths::GetCleanFilename(parentFolderPath);
		return parentFolderName.IsEmpty() ? parentFolderPath : parentFolderName;
	}

	void SetStartupMenuComboBoxSelection(UComboBoxString* comboBox, const FString& selectedItem)
	{
		if (!comboBox || selectedItem.IsEmpty() || comboBox->GetSelectedOption().Equals(selectedItem, ESearchCase::IgnoreCase))
		{
			return;
		}

		comboBox->SetSelectedOption(selectedItem);
	}

	bool PickStartupMenuFolder(const FString& dialogTitle, const FString& initialFolder, FString& outFolder)
	{
		outFolder.Reset();

#if WITH_EDITOR
		IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
		if (!desktopPlatform)
		{
			return false;
		}

		const FString normalizedInitialFolder = NormalizeStartupMenuPath(initialFolder);
		const void* parentWindowHandle = FSlateApplication::IsInitialized()
			? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
			: nullptr;
		FString selectedFolder;
		if (!desktopPlatform->OpenDirectoryDialog(
			parentWindowHandle,
			dialogTitle,
			normalizedInitialFolder,
			selectedFolder))
		{
			return false;
		}

		outFolder = NormalizeStartupMenuPath(selectedFolder);
		return !outFolder.IsEmpty();
#else
		(void)dialogTitle;
		(void)initialFolder;
		return false;
#endif
	}

}

UStartupMenuWidget* UStartupMenuWidget::ShowStartupMenu(UObject* WorldContextObject, const int32 ZOrder)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogStartupMenuWidget, Error, TEXT("StartupMenu world context is null."));
		return nullptr;
	}

	UWorld* world = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!world)
	{
		UE_LOG(LogStartupMenuWidget, Error, TEXT("StartupMenu world context has no world."));
		return nullptr;
	}

	APlayerController* playerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!playerController)
	{
		UE_LOG(LogStartupMenuWidget, Error, TEXT("StartupMenu could not resolve PlayerController 0."));
		return nullptr;
	}

	const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
	const TSubclassOf<UStartupMenuWidget> widgetClass = platformUiSettings
		? platformUiSettings->StartupMenuWidgetClass.LoadSynchronous()
		: nullptr;
	if (!widgetClass)
	{
		UE_LOG(
			LogStartupMenuWidget,
			Error,
			TEXT("StartupMenuWidgetClass is not configured in Platform UI project settings."));
		return nullptr;
	}

	UStartupMenuWidget* widget = CreateWidget<UStartupMenuWidget>(playerController, widgetClass);
	if (!widget)
	{
		UE_LOG(LogStartupMenuWidget, Error, TEXT("StartupMenu widget creation failed."));
		return nullptr;
	}

	widget->SetIsFocusable(true);
	widget->AddToViewport(ZOrder);

	FInputModeGameAndUI inputMode;
	inputMode.SetWidgetToFocus(widget->TakeWidget());
	inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	inputMode.SetHideCursorDuringCapture(false);
	playerController->SetInputMode(inputMode);
	playerController->bShowMouseCursor = true;

	return widget;
}

void UStartupMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		StartupMenuViewModel = platformUiSubsystem->GetStartupMenuViewModel();
	}
	if (!ValidateRequiredBindings())
	{
		return;
	}

	BindControls();
	HideRecentProjectDeleteDialog();
	LoadProjectOpenOptions();
	InitializeProjectPathInputs();
	RefreshProjectPresetOptions();
	RefreshRecentProjectCards();
	ShowRecentProjectsScreen();
	RefreshProjectOpenActions();
	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString());
}

void UStartupMenuWidget::NativeDestruct()
{
	for (UProjectTemplateCardWidget* cardWidget : RecentProjectCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
			cardWidget->OnContextRequested.RemoveAll(this);
		}
	}
	RecentProjectCards.Reset();

	const auto clearPresetCards =
		[this](TArray<TObjectPtr<UProjectTemplateCardWidget>>& cards)
		{
			for (UProjectTemplateCardWidget* cardWidget : cards)
			{
				if (cardWidget)
				{
					cardWidget->OnSelectedRequested.RemoveAll(this);
				}
			}
			cards.Reset();
		};
	clearPresetCards(ScenarioPresetCards);
	clearPresetCards(ProfilePresetCards);
	clearPresetCards(PolicyPresetCards);

	if (ScenarioPresetSelectionBox)
	{
		ScenarioPresetSelectionBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleScenarioPresetSelectionChanged);
	}
	if (ProfilePresetSelectionBox)
	{
		ProfilePresetSelectionBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleProfilePresetSelectionChanged);
	}
	if (PolicyPresetSelectionBox)
	{
		PolicyPresetSelectionBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandlePolicyPresetSelectionChanged);
	}
	if (RecentProjectAddButton)
	{
		RecentProjectAddButton->OnClicked.RemoveDynamic(this, &UStartupMenuWidget::HandleAddRecentProjectClicked);
	}

	Super::NativeDestruct();
}

void UStartupMenuWidget::SetProjectPathForPrototype(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupMenuPath(projectPath);
	if (UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel())
	{
		viewModel->SetProjectPathForPrototype(normalizedProjectPath);
	}
	SelectedProjectParentFolder = NormalizeStartupMenuPath(FPaths::GetPath(normalizedProjectPath));
	SelectedProjectName = FPaths::GetCleanFilename(normalizedProjectPath);

	if (ProjectParentFolderTextBox)
	{
		ProjectParentFolderTextBox->SetText(FText::FromString(SelectedProjectParentFolder));
	}
	if (ProjectNameTextBox)
	{
		ProjectNameTextBox->SetText(FText::FromString(SelectedProjectName));
	}
	RefreshProjectOpenActions();
}

FString UStartupMenuWidget::GetProjectPathForPrototype() const
{
	return GetSelectedProjectPath();
}

void UStartupMenuWidget::SelectProjectPresets(
	const FString& scenarioPresetId,
	const FString& profilePresetId,
	const FString& policyPresetId)
{
	SelectedScenarioPresetId = scenarioPresetId.TrimStartAndEnd().IsEmpty()
		? FString(DefaultScenarioPresetId)
		: scenarioPresetId.TrimStartAndEnd();
	SelectedProfilePresetId = profilePresetId.TrimStartAndEnd().IsEmpty()
		? FString(DefaultProfilePresetId)
		: profilePresetId.TrimStartAndEnd();
	SelectedPolicyPresetId = policyPresetId.TrimStartAndEnd().IsEmpty()
		? FString(DefaultPolicyPresetId)
		: policyPresetId.TrimStartAndEnd();
	if (UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel())
	{
		viewModel->SelectProjectPresets(
			SelectedScenarioPresetId,
			SelectedProfilePresetId,
			SelectedPolicyPresetId);
	}
	RefreshProjectPresetSelectionStates();
}

bool UStartupMenuWidget::ValidateSelectedProject(
	TArray<FString>& outDiagnostics,
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem)
{
	outDiagnostics.Reset();

	UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel(simulatorLaunchSubsystem);
	if (!viewModel)
	{
		outDiagnostics.Add(TEXT("StartupMenuViewModel을 사용할 수 없습니다."));
		return false;
	}

	return viewModel->ValidateProject(GetSelectedProjectPath(), outDiagnostics);
}

bool UStartupMenuWidget::AddRecentProjectForPrototype(
	const FString& projectPath,
	TArray<FString>& outDiagnostics,
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem)
{
	return AddRecentProjectIfValid(projectPath, outDiagnostics, simulatorLaunchSubsystem);
}

TArray<FString> UStartupMenuWidget::GetRecentProjectPathsForPrototype()
{
	if (UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel())
	{
		return viewModel->GetRecentProjectPaths();
	}
	return RecentProjectPaths;
}

void UStartupMenuWidget::HandleProjectOpenInputChanged(const FText&)
{
	CacheProjectOpenOptionsFromWidgets();
	RefreshProjectOpenActions();
}

void UStartupMenuWidget::HandleCreateNewProjectClicked()
{
	ShowCreateProjectScreen();
	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString());
	RefreshProjectOpenActions();
}

void UStartupMenuWidget::HandleOpenProjectClicked()
{
	FString selectedProjectFolder;
	if (!BrowseForExistingProjectFolder(selectedProjectFolder))
	{
		SetProjectOpenWarningText(TEXT("프로젝트 폴더를 선택하지 않았습니다."));
		return;
	}

	OpenExistingProject(selectedProjectFolder);
}

void UStartupMenuWidget::HandleAddRecentProjectClicked()
{
	FString selectedProjectFolder;
	if (!BrowseForExistingProjectFolder(selectedProjectFolder))
	{
		SetProjectOpenWarningText(TEXT("프로젝트 폴더를 선택하지 않았습니다."));
		return;
	}

	TArray<FString> diagnostics;
	AddRecentProjectIfValid(selectedProjectFolder, diagnostics);
}

void UStartupMenuWidget::HandleBackToRecentProjectsClicked()
{
	ShowRecentProjectsScreen();
	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString());
}

void UStartupMenuWidget::HandleCreateProjectClicked()
{
	if (!StartupMenuViewModel)
	{
		SetProjectOpenWarningText(TEXT("StartupMenuViewModel을 사용할 수 없습니다."));
		return;
	}

	const FString projectPath = GetSelectedProjectPath();
	if (projectPath.TrimStartAndEnd().IsEmpty())
	{
		SetProjectOpenWarningText(TEXT("프로젝트 이름을 입력하세요."));
		RefreshProjectOpenActions();
		return;
	}

	SaveProjectOpenOptions();

	const FProjectPresetSelection presets = GetSelectedProjectPresetSelection();
	if (!StartupMenuViewModel->CreateProject(SelectedProjectParentFolder, SelectedProjectName, presets))
	{
		SetProjectOpenWarningText(StartupMenuViewModel->GetProjectOpenWarningText());
		SetDiagnosticsText(StartupMenuViewModel->GetDiagnosticsText());
		return;
	}

	RecentProjectPaths = StartupMenuViewModel->GetRecentProjectPaths();
	if (!CommitActiveProjectAndOpenEditor())
	{
		return;
	}

	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString::Printf(TEXT("Project created: %s"), *projectPath));
}

void UStartupMenuWidget::HandleProjectParentFolderBrowseClicked()
{
	FString selectedFolder;
	if (!BrowseForProjectParentFolder(selectedFolder))
	{
		SetProjectOpenWarningText(TEXT("프로젝트 상위 폴더를 선택하지 않았습니다."));
		return;
	}

	SelectedProjectParentFolder = selectedFolder;
	if (ProjectParentFolderTextBox)
	{
		ProjectParentFolderTextBox->SetText(FText::FromString(SelectedProjectParentFolder));
	}
	RefreshProjectOpenActions();
	SetProjectOpenWarningText(FString());
}

void UStartupMenuWidget::HandleScenarioPresetSelectionChanged(
	FString selectedItem,
	ESelectInfo::Type)
{
	const FString selectedPresetId = selectedItem.TrimStartAndEnd();
	if (!selectedPresetId.IsEmpty())
	{
		SelectedScenarioPresetId = selectedPresetId;
		RefreshProjectPresetSelectionStates();
	}
}

void UStartupMenuWidget::HandleProfilePresetSelectionChanged(
	FString selectedItem,
	ESelectInfo::Type)
{
	const FString selectedPresetId = selectedItem.TrimStartAndEnd();
	if (!selectedPresetId.IsEmpty())
	{
		SelectedProfilePresetId = selectedPresetId;
		RefreshProjectPresetSelectionStates();
	}
}

void UStartupMenuWidget::HandlePolicyPresetSelectionChanged(
	FString selectedItem,
	ESelectInfo::Type)
{
	const FString selectedPresetId = selectedItem.TrimStartAndEnd();
	if (!selectedPresetId.IsEmpty())
	{
		SelectedPolicyPresetId = selectedPresetId;
		RefreshProjectPresetSelectionStates();
	}
}

void UStartupMenuWidget::BindControls()
{
	if (ProjectParentFolderTextBox)
	{
		ProjectParentFolderTextBox->OnTextChanged.RemoveDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
		ProjectParentFolderTextBox->OnTextChanged.AddDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
	}
	if (ProjectParentFolderBrowseButton)
	{
		ProjectParentFolderBrowseButton->OnClicked.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleProjectParentFolderBrowseClicked);
		ProjectParentFolderBrowseButton->OnClicked.AddDynamic(
			this,
			&UStartupMenuWidget::HandleProjectParentFolderBrowseClicked);
	}
	if (ProjectNameTextBox)
	{
		ProjectNameTextBox->OnTextChanged.RemoveDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
		ProjectNameTextBox->OnTextChanged.AddDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
	}
	if (ScenarioPresetSelectionBox)
	{
		ScenarioPresetSelectionBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleScenarioPresetSelectionChanged);
		ScenarioPresetSelectionBox->OnSelectionChanged.AddDynamic(
			this,
			&UStartupMenuWidget::HandleScenarioPresetSelectionChanged);
	}
	if (ProfilePresetSelectionBox)
	{
		ProfilePresetSelectionBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleProfilePresetSelectionChanged);
		ProfilePresetSelectionBox->OnSelectionChanged.AddDynamic(
			this,
			&UStartupMenuWidget::HandleProfilePresetSelectionChanged);
	}
	if (PolicyPresetSelectionBox)
	{
		PolicyPresetSelectionBox->OnSelectionChanged.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandlePolicyPresetSelectionChanged);
		PolicyPresetSelectionBox->OnSelectionChanged.AddDynamic(
			this,
			&UStartupMenuWidget::HandlePolicyPresetSelectionChanged);
	}
	if (CreateProjectButton)
	{
		CreateProjectButton->OnClicked.RemoveDynamic(this, &UStartupMenuWidget::HandleCreateProjectClicked);
		CreateProjectButton->OnClicked.AddDynamic(this, &UStartupMenuWidget::HandleCreateProjectClicked);
	}
	if (CreateNewProjectButton)
	{
		CreateNewProjectButton->OnClicked.RemoveDynamic(this, &UStartupMenuWidget::HandleCreateNewProjectClicked);
		CreateNewProjectButton->OnClicked.AddDynamic(this, &UStartupMenuWidget::HandleCreateNewProjectClicked);
	}
	if (OpenProjectButton)
	{
		OpenProjectButton->OnClicked.RemoveDynamic(this, &UStartupMenuWidget::HandleOpenProjectClicked);
		OpenProjectButton->OnClicked.AddDynamic(this, &UStartupMenuWidget::HandleOpenProjectClicked);
	}
	if (RecentProjectAddButton)
	{
		RecentProjectAddButton->OnClicked.RemoveDynamic(this, &UStartupMenuWidget::HandleAddRecentProjectClicked);
		RecentProjectAddButton->OnClicked.AddDynamic(this, &UStartupMenuWidget::HandleAddRecentProjectClicked);
	}
	if (BackToRecentProjectsButton)
	{
		BackToRecentProjectsButton->OnClicked.RemoveDynamic(this, &UStartupMenuWidget::HandleBackToRecentProjectsClicked);
		BackToRecentProjectsButton->OnClicked.AddDynamic(this, &UStartupMenuWidget::HandleBackToRecentProjectsClicked);
	}
	if (RecentProjectDeleteConfirmButton)
	{
		RecentProjectDeleteConfirmButton->OnClicked.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleConfirmRecentProjectDeleteClicked);
		RecentProjectDeleteConfirmButton->OnClicked.AddDynamic(
			this,
			&UStartupMenuWidget::HandleConfirmRecentProjectDeleteClicked);
	}
	if (RecentProjectDeleteCancelButton)
	{
		RecentProjectDeleteCancelButton->OnClicked.RemoveDynamic(
			this,
			&UStartupMenuWidget::HandleCancelRecentProjectDeleteClicked);
		RecentProjectDeleteCancelButton->OnClicked.AddDynamic(
			this,
			&UStartupMenuWidget::HandleCancelRecentProjectDeleteClicked);
	}
}

bool UStartupMenuWidget::ValidateRequiredBindings() const
{
	TArray<FString> missingWidgetNames;
	auto requireWidget = [&missingWidgetNames](const UObject* widget, const TCHAR* widgetName)
	{
		if (!IsValid(widget))
		{
			missingWidgetNames.Add(widgetName);
		}
	};

	requireWidget(StartupScreenSwitcher, TEXT("StartupScreenSwitcher"));
	requireWidget(RecentProjectsScreen, TEXT("RecentProjectsScreen"));
	requireWidget(ProjectCreateScreen, TEXT("ProjectCreateScreen"));
	requireWidget(RecentProjectCardWrapBox, TEXT("RecentProjectCardWrapBox"));
	requireWidget(RecentProjectsEmptyText, TEXT("RecentProjectsEmptyText"));
	requireWidget(RecentProjectOpenWarningText, TEXT("RecentProjectOpenWarningText"));
	requireWidget(RecentProjectDeleteDialog, TEXT("RecentProjectDeleteDialog"));
	requireWidget(RecentProjectDeleteDialogMessageText, TEXT("RecentProjectDeleteDialogMessageText"));
	requireWidget(RecentProjectDeleteConfirmButton, TEXT("RecentProjectDeleteConfirmButton"));
	requireWidget(RecentProjectDeleteCancelButton, TEXT("RecentProjectDeleteCancelButton"));
	requireWidget(CreateNewProjectButton, TEXT("CreateNewProjectButton"));
	requireWidget(OpenProjectButton, TEXT("OpenProjectButton"));
	requireWidget(RecentProjectAddButton, TEXT("RecentProjectAddButton"));
	requireWidget(BackToRecentProjectsButton, TEXT("BackToRecentProjectsButton"));
	requireWidget(ProjectParentFolderTextBox, TEXT("ProjectParentFolderTextBox"));
	requireWidget(ProjectNameTextBox, TEXT("ProjectNameTextBox"));
	if (!ScenarioPresetSelectionBox && !ScenarioPresetCardWrapBox)
	{
		missingWidgetNames.Add(TEXT("ScenarioPresetSelectionBox"));
	}
	if (!ProfilePresetSelectionBox && !ProfilePresetCardWrapBox)
	{
		missingWidgetNames.Add(TEXT("ProfilePresetSelectionBox"));
	}
	if (!PolicyPresetSelectionBox && !PolicyPresetCardWrapBox)
	{
		missingWidgetNames.Add(TEXT("PolicyPresetSelectionBox"));
	}
	requireWidget(CreateProjectButton, TEXT("CreateProjectButton"));
	requireWidget(ProjectOpenWarningText, TEXT("ProjectOpenWarningText"));

	if (missingWidgetNames.IsEmpty())
	{
		return true;
	}

	UE_LOG(
		LogStartupMenuWidget,
		Error,
		TEXT("WBP_StartupMenu binding is invalid. Missing widgets: %s"),
		*FString::Join(missingWidgetNames, TEXT(", ")));
	return false;
}

void UStartupMenuWidget::InitializeProjectPathInputs()
{
	if (SelectedProjectParentFolder.IsEmpty())
	{
		SelectedProjectParentFolder = GetDefaultProjectParentFolder();
	}
	if (SelectedProjectName.IsEmpty())
	{
		SelectedProjectName = TEXT("OdiroProject");
	}

	if (ProjectParentFolderTextBox && ProjectParentFolderTextBox->GetText().IsEmpty())
	{
		ProjectParentFolderTextBox->SetText(FText::FromString(SelectedProjectParentFolder));
	}
	if (ProjectNameTextBox && ProjectNameTextBox->GetText().IsEmpty())
	{
		ProjectNameTextBox->SetText(FText::FromString(SelectedProjectName));
	}
}

void UStartupMenuWidget::LoadProjectOpenOptions()
{
	UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel();
	if (!viewModel)
	{
		SelectedProjectParentFolder = GetDefaultProjectParentFolder();
		SelectedProjectName = TEXT("OdiroProject");
		SelectedScenarioPresetId = DefaultScenarioPresetId;
		SelectedProfilePresetId = DefaultProfilePresetId;
		SelectedPolicyPresetId = DefaultPolicyPresetId;
		return;
	}

	viewModel->LoadProjectOpenOptions();
	SelectedProjectParentFolder = viewModel->GetProjectParentFolder();
	SelectedProjectName = viewModel->GetProjectName();
	const FProjectPresetSelection selection = viewModel->GetSelectedProjectPresetSelection();
	SelectedScenarioPresetId = selection.ScenarioPresetId;
	SelectedProfilePresetId = selection.ProfilePresetId;
	SelectedPolicyPresetId = selection.PolicyPresetId;
}

void UStartupMenuWidget::SaveProjectOpenOptions()
{
	CacheProjectOpenOptionsFromWidgets();
	UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel();
	if (!viewModel)
	{
		return;
	}

	const FProjectPresetSelection selection = GetSelectedProjectPresetSelection();
	viewModel->SetProjectParentFolder(SelectedProjectParentFolder);
	viewModel->SetProjectName(SelectedProjectName);
	viewModel->SelectProjectPresets(selection.ScenarioPresetId, selection.ProfilePresetId, selection.PolicyPresetId);
	viewModel->SaveProjectOpenOptions();
}

bool UStartupMenuWidget::RemoveRecentProject(const FString& projectPath)
{
	if (UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel())
	{
		const bool bRemoved = viewModel->RemoveRecentProject(projectPath);
		RecentProjectPaths = viewModel->GetRecentProjectPaths();
		return bRemoved;
	}

	return false;
}

void UStartupMenuWidget::CacheProjectOpenOptionsFromWidgets()
{
	if (ProjectParentFolderTextBox)
	{
		SelectedProjectParentFolder = NormalizeStartupMenuPath(ProjectParentFolderTextBox->GetText().ToString());
	}
	if (ProjectNameTextBox)
	{
		SelectedProjectName = NormalizeProjectDirectoryName(ProjectNameTextBox->GetText().ToString());
	}
}

void UStartupMenuWidget::ShowRecentProjectsScreen()
{
	if (StartupScreenSwitcher && RecentProjectsScreen)
	{
		StartupScreenSwitcher->SetActiveWidget(RecentProjectsScreen);
	}
	RefreshRecentProjectCards();
}

void UStartupMenuWidget::ShowCreateProjectScreen()
{
	if (StartupScreenSwitcher && ProjectCreateScreen)
	{
		StartupScreenSwitcher->SetActiveWidget(ProjectCreateScreen);
	}
	RefreshProjectOpenActions();
}

void UStartupMenuWidget::RefreshRecentProjectCards()
{
	if (!RecentProjectCardWrapBox)
	{
		return;
	}

	for (UProjectTemplateCardWidget* cardWidget : RecentProjectCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
			cardWidget->OnContextRequested.RemoveAll(this);
		}
	}
	RecentProjectCards.Reset();
	RecentProjectCardWrapBox->ClearChildren();

	TSubclassOf<UProjectTemplateCardWidget> cardClass = ResolveProjectTemplateCardWidgetClass();
	TArray<FString> visibleProjectPaths = RecentProjectPaths;
	TArray<UOdiroListItemViewModel*> visibleProjectItems;
	if (StartupMenuViewModel)
	{
		StartupMenuViewModel->RefreshRecentProjects();
		visibleProjectPaths = StartupMenuViewModel->GetRecentProjectPaths();
		visibleProjectItems = StartupMenuViewModel->GetRecentProjectItems();
		RecentProjectPaths = visibleProjectPaths;
	}

	if (RecentProjectsEmptyText)
	{
		RecentProjectsEmptyText->SetVisibility(visibleProjectPaths.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (!cardClass)
	{
		return;
	}

	for (int32 cardIndex = 0; cardIndex < visibleProjectPaths.Num(); ++cardIndex)
	{
		const FString& recentProjectPath = visibleProjectPaths[cardIndex];
		UProjectTemplateCardWidget* cardWidget = CreateWidget<UProjectTemplateCardWidget>(GetOwningPlayer(), cardClass);
		if (!cardWidget)
		{
			continue;
		}

		if (visibleProjectItems.IsValidIndex(cardIndex))
		{
			cardWidget->InitializeFromItemViewModel(visibleProjectItems[cardIndex]);
		}
		else
		{
			cardWidget->InitializeCard(
				recentProjectPath,
				MakeRecentProjectDisplayName(recentProjectPath),
				MakeRecentProjectSubtitle(recentProjectPath));
		}
		cardWidget->OnSelectedRequested.AddUObject(this, &UStartupMenuWidget::HandleRecentProjectCardSelected);
		cardWidget->OnContextRequested.AddUObject(this, &UStartupMenuWidget::HandleRecentProjectCardContextRequested);
		RecentProjectCards.Add(cardWidget);
		RecentProjectCardWrapBox->AddChildToWrapBox(cardWidget);
	}
}

void UStartupMenuWidget::RefreshProjectPresetOptions()
{
	const bool bHasPresetComboBoxes =
		ScenarioPresetSelectionBox && ProfilePresetSelectionBox && PolicyPresetSelectionBox;
	const bool bHasPresetCardFallback =
		ScenarioPresetCardWrapBox && ProfilePresetCardWrapBox && PolicyPresetCardWrapBox;
	if (!bHasPresetComboBoxes && !bHasPresetCardFallback)
	{
		return;
	}

	const auto resetCards =
		[this](UWrapBox* container, TArray<TObjectPtr<UProjectTemplateCardWidget>>& cards)
		{
			for (UProjectTemplateCardWidget* cardWidget : cards)
			{
				if (cardWidget)
				{
					cardWidget->OnSelectedRequested.RemoveAll(this);
				}
			}
			cards.Reset();
			container->ClearChildren();
		};
	if (bHasPresetCardFallback)
	{
		resetCards(ScenarioPresetCardWrapBox, ScenarioPresetCards);
		resetCards(ProfilePresetCardWrapBox, ProfilePresetCards);
		resetCards(PolicyPresetCardWrapBox, PolicyPresetCards);
	}

	FProjectPresetCatalog catalog;
	TArray<UOdiroListItemViewModel*> scenarioPresetItems;
	TArray<UOdiroListItemViewModel*> profilePresetItems;
	TArray<UOdiroListItemViewModel*> policyPresetItems;
	if (StartupMenuViewModel)
	{
		StartupMenuViewModel->RefreshProjectPresets();
		catalog = StartupMenuViewModel->GetProjectPresetCatalog();
	}
	else if (const UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		catalog = platformUiSubsystem->ListProjectPresets();
	}
	else
	{
		catalog.ScenarioPresetIds.Add(DefaultScenarioPresetId);
		catalog.ProfilePresetIds.Add(DefaultProfilePresetId);
		catalog.PolicyPresetIds.Add(DefaultPolicyPresetId);
	}

	if (catalog.ScenarioPresetIds.IsEmpty())
	{
		catalog.ScenarioPresetIds.Add(DefaultScenarioPresetId);
	}
	if (catalog.ProfilePresetIds.IsEmpty())
	{
		catalog.ProfilePresetIds.Add(DefaultProfilePresetId);
	}
	if (catalog.PolicyPresetIds.IsEmpty())
	{
		catalog.PolicyPresetIds.Add(DefaultPolicyPresetId);
	}

	const auto containsPreset =
		[](const TArray<FString>& presetIds, const FString& selectedPresetId)
		{
			return presetIds.ContainsByPredicate(
				[&selectedPresetId](const FString& presetId)
				{
					return presetId.Equals(selectedPresetId, ESearchCase::IgnoreCase);
				});
		};
	if (!containsPreset(catalog.ScenarioPresetIds, SelectedScenarioPresetId))
	{
		SelectedScenarioPresetId = catalog.ScenarioPresetIds[0];
	}
	if (!containsPreset(catalog.ProfilePresetIds, SelectedProfilePresetId))
	{
		SelectedProfilePresetId = catalog.ProfilePresetIds[0];
	}
	if (!containsPreset(catalog.PolicyPresetIds, SelectedPolicyPresetId))
	{
		SelectedPolicyPresetId = catalog.PolicyPresetIds[0];
	}
	if (StartupMenuViewModel)
	{
		StartupMenuViewModel->SelectProjectPresets(
			SelectedScenarioPresetId,
			SelectedProfilePresetId,
			SelectedPolicyPresetId);
		scenarioPresetItems = StartupMenuViewModel->GetScenarioPresetItems();
		profilePresetItems = StartupMenuViewModel->GetProfilePresetItems();
		policyPresetItems = StartupMenuViewModel->GetPolicyPresetItems();
	}

	if (bHasPresetComboBoxes)
	{
		const auto populateComboBox =
			[](UComboBoxString* comboBox, const TArray<FString>& presetIds, const FString& selectedPresetId)
			{
				if (!comboBox)
				{
					return;
				}

				comboBox->ClearOptions();
				for (const FString& presetId : presetIds)
				{
					comboBox->AddOption(presetId);
				}
				SetStartupMenuComboBoxSelection(comboBox, selectedPresetId);
			};

		populateComboBox(ScenarioPresetSelectionBox, catalog.ScenarioPresetIds, SelectedScenarioPresetId);
		populateComboBox(ProfilePresetSelectionBox, catalog.ProfilePresetIds, SelectedProfilePresetId);
		populateComboBox(PolicyPresetSelectionBox, catalog.PolicyPresetIds, SelectedPolicyPresetId);
	}

	if (!bHasPresetCardFallback)
	{
		RefreshProjectPresetSelectionStates();
		return;
	}

	TSubclassOf<UProjectTemplateCardWidget> cardClass = ResolveProjectTemplateCardWidgetClass();
	const auto populateCards =
		[this, cardClass](
			UWrapBox* container,
			TArray<TObjectPtr<UProjectTemplateCardWidget>>& cards,
			const TArray<UOdiroListItemViewModel*>& presetItems,
			const TArray<FString>& presetIds,
			void (UStartupMenuWidget::*selectionHandler)(UProjectTemplateCardWidget*))
		{
			if (!cardClass)
			{
				return;
			}

			for (const FString& presetId : presetIds)
			{
				UProjectTemplateCardWidget* cardWidget = CreateWidget<UProjectTemplateCardWidget>(GetOwningPlayer(), cardClass);
				if (!cardWidget)
				{
					continue;
				}

				const int32 presetIndex = presetIds.IndexOfByKey(presetId);
				if (presetItems.IsValidIndex(presetIndex))
				{
					cardWidget->InitializeFromItemViewModel(presetItems[presetIndex]);
				}
				else
				{
					cardWidget->InitializeCard(presetId, MakeProjectPresetDisplayName(presetId));
				}
				cardWidget->OnSelectedRequested.AddUObject(this, selectionHandler);
				cards.Add(cardWidget);
				container->AddChildToWrapBox(cardWidget);
			}
		};
	populateCards(
		ScenarioPresetCardWrapBox,
		ScenarioPresetCards,
		scenarioPresetItems,
		catalog.ScenarioPresetIds,
		&UStartupMenuWidget::HandleScenarioPresetCardSelected);
	populateCards(
		ProfilePresetCardWrapBox,
		ProfilePresetCards,
		profilePresetItems,
		catalog.ProfilePresetIds,
		&UStartupMenuWidget::HandleProfilePresetCardSelected);
	populateCards(
		PolicyPresetCardWrapBox,
		PolicyPresetCards,
		policyPresetItems,
		catalog.PolicyPresetIds,
		&UStartupMenuWidget::HandlePolicyPresetCardSelected);

	RefreshProjectPresetSelectionStates();
}

void UStartupMenuWidget::RefreshProjectOpenActions()
{
	bool bCanCreateProject = false;
	if (StartupMenuViewModel)
	{
		StartupMenuViewModel->SetProjectParentFolder(GetSelectedProjectParentFolder());
		StartupMenuViewModel->SetProjectName(GetSelectedProjectName());
		bCanCreateProject = StartupMenuViewModel->CanCreateProject();
	}
	else
	{
		const FString selectedProjectPath = GetSelectedProjectPath();
		const bool bHasProjectPath = !selectedProjectPath.TrimStartAndEnd().IsEmpty();
		const bool bProjectDirectoryExists = UPlatformUiSubsystem::DoesResolvedDirectoryExist(selectedProjectPath);
		const bool bProjectFileExists = UPlatformUiSubsystem::DoesResolvedFileExist(selectedProjectPath);
		bCanCreateProject = bHasProjectPath && !bProjectDirectoryExists && !bProjectFileExists;
	}

	if (CreateProjectButton)
	{
		CreateProjectButton->SetIsEnabled(bCanCreateProject);
	}
}

void UStartupMenuWidget::RefreshProjectPresetSelectionStates()
{
	const FProjectPresetSelection selection = GetSelectedProjectPresetSelection();
	SetStartupMenuComboBoxSelection(ScenarioPresetSelectionBox, selection.ScenarioPresetId);
	SetStartupMenuComboBoxSelection(ProfilePresetSelectionBox, selection.ProfilePresetId);
	SetStartupMenuComboBoxSelection(PolicyPresetSelectionBox, selection.PolicyPresetId);

	const auto refreshCards =
		[](const TArray<TObjectPtr<UProjectTemplateCardWidget>>& cards, const FString& selectedPresetId)
		{
			for (UProjectTemplateCardWidget* cardWidget : cards)
			{
				if (cardWidget)
				{
					cardWidget->SetSelected(cardWidget->GetItemId().Equals(selectedPresetId, ESearchCase::IgnoreCase));
				}
			}
		};
	refreshCards(ScenarioPresetCards, selection.ScenarioPresetId);
	refreshCards(ProfilePresetCards, selection.ProfilePresetId);
	refreshCards(PolicyPresetCards, selection.PolicyPresetId);
}

void UStartupMenuWidget::SetProjectOpenWarningText(const FString& message)
{
	auto applyWarningText = [&message](UTextBlock* textBlock)
	{
		if (!textBlock)
		{
			return;
		}

		textBlock->SetText(FText::FromString(message));
		textBlock->SetVisibility(message.TrimStartAndEnd().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	};

	applyWarningText(ProjectOpenWarningText);
	applyWarningText(RecentProjectOpenWarningText);
}

void UStartupMenuWidget::SetDiagnosticsText(const FString& message)
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(message));
	}
}

bool UStartupMenuWidget::BrowseForProjectParentFolder(FString& outFolder) const
{
	outFolder.Reset();
	return PickStartupMenuFolder(TEXT("Select Project Parent Folder"), GetSelectedProjectParentFolder(), outFolder);
}

bool UStartupMenuWidget::BrowseForExistingProjectFolder(FString& outFolder) const
{
	outFolder.Reset();
	return PickStartupMenuFolder(TEXT("Open Project Folder"), GetSelectedProjectParentFolder(), outFolder);
}

bool UStartupMenuWidget::AddRecentProjectIfValid(
	const FString& projectPath,
	TArray<FString>& outDiagnostics,
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem)
{
	outDiagnostics.Reset();

	UStartupMenuViewModel* viewModel = EnsureStartupMenuViewModel(simulatorLaunchSubsystem);
	if (!viewModel)
	{
		outDiagnostics.Add(TEXT("StartupMenuViewModel을 사용할 수 없습니다."));
		SetProjectOpenWarningText(outDiagnostics[0]);
		return false;
	}

	const bool bAdded = viewModel->AddRecentProjectIfValid(projectPath, outDiagnostics);
	SetProjectOpenWarningText(viewModel->GetProjectOpenWarningText());
	SetDiagnosticsText(viewModel->GetDiagnosticsText());
	RecentProjectPaths = viewModel->GetRecentProjectPaths();
	if (bAdded)
	{
		RefreshRecentProjectCards();
	}
	return bAdded;
}

bool UStartupMenuWidget::OpenExistingProject(const FString& projectPath)
{
	TArray<FString> diagnostics;
	if (!AddRecentProjectIfValid(projectPath, diagnostics))
	{
		RefreshRecentProjectCards();
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupMenuPath(projectPath);
	SetProjectPathForPrototype(normalizedProjectPath);
	SaveProjectOpenOptions();
	if (!CommitActiveProjectAndOpenEditor())
	{
		return false;
	}

	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString::Printf(TEXT("Project opened: %s"), *normalizedProjectPath));
	return true;
}

bool UStartupMenuWidget::CommitActiveProjectAndOpenEditor()
{
	const FString projectPath = GetSelectedProjectPath();
	if (!StartupMenuViewModel)
	{
		SetProjectOpenWarningText(TEXT("StartupMenuViewModel을 사용할 수 없습니다."));
		return false;
	}

	if (!StartupMenuViewModel->OpenProject(projectPath))
	{
		SetProjectOpenWarningText(StartupMenuViewModel->GetProjectOpenWarningText());
		SetDiagnosticsText(StartupMenuViewModel->GetDiagnosticsText());
		return false;
	}

	return true;
}

void UStartupMenuWidget::HandleRecentProjectCardSelected(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	OpenExistingProject(cardWidget->GetItemId());
}

void UStartupMenuWidget::HandleRecentProjectCardContextRequested(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	ShowRecentProjectDeleteDialog(cardWidget->GetItemId());
}

void UStartupMenuWidget::HandleScenarioPresetCardSelected(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	SelectedScenarioPresetId = cardWidget->GetItemId();
	RefreshProjectPresetSelectionStates();
}

void UStartupMenuWidget::HandleProfilePresetCardSelected(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	SelectedProfilePresetId = cardWidget->GetItemId();
	RefreshProjectPresetSelectionStates();
}

void UStartupMenuWidget::HandlePolicyPresetCardSelected(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	SelectedPolicyPresetId = cardWidget->GetItemId();
	RefreshProjectPresetSelectionStates();
}

void UStartupMenuWidget::ShowRecentProjectDeleteDialog(const FString& projectPath)
{
	PendingRecentProjectDeletePath = NormalizeStartupMenuPath(projectPath);
	if (PendingRecentProjectDeletePath.IsEmpty())
	{
		HideRecentProjectDeleteDialog();
		return;
	}

	if (RecentProjectDeleteDialogMessageText)
	{
		const FString projectName = FPaths::GetCleanFilename(PendingRecentProjectDeletePath);
		RecentProjectDeleteDialogMessageText->SetText(FText::FromString(FString::Printf(
			TEXT("Remove \"%s\" from recent projects?\n%s\n\nThe project folder will not be deleted."),
			*projectName,
			*PendingRecentProjectDeletePath)));
	}
	if (RecentProjectDeleteDialog)
	{
		RecentProjectDeleteDialog->SetVisibility(ESlateVisibility::Visible);
	}
}

void UStartupMenuWidget::HideRecentProjectDeleteDialog()
{
	PendingRecentProjectDeletePath.Reset();
	if (RecentProjectDeleteDialog)
	{
		RecentProjectDeleteDialog->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UStartupMenuWidget::HandleConfirmRecentProjectDeleteClicked()
{
	const FString projectPath = PendingRecentProjectDeletePath;
	HideRecentProjectDeleteDialog();
	if (RemoveRecentProject(projectPath))
	{
		RefreshRecentProjectCards();
		SetDiagnosticsText(FString::Printf(TEXT("Recent project removed: %s"), *projectPath));
	}
}

void UStartupMenuWidget::HandleCancelRecentProjectDeleteClicked()
{
	HideRecentProjectDeleteDialog();
}

FString UStartupMenuWidget::GetSelectedProjectParentFolder() const
{
	FString parentFolder = ProjectParentFolderTextBox
		? ProjectParentFolderTextBox->GetText().ToString()
		: SelectedProjectParentFolder;

	if (parentFolder.TrimStartAndEnd().IsEmpty())
	{
		parentFolder = GetDefaultProjectParentFolder();
	}

	return NormalizeStartupMenuPath(parentFolder);
}

FString UStartupMenuWidget::GetSelectedProjectName() const
{
	const FString projectName = ProjectNameTextBox
		? ProjectNameTextBox->GetText().ToString()
		: SelectedProjectName;
	return NormalizeProjectDirectoryName(projectName);
}

FString UStartupMenuWidget::GetSelectedProjectPath() const
{
	return BuildProjectPathFromInputs(GetSelectedProjectParentFolder(), GetSelectedProjectName());
}

FProjectPresetSelection UStartupMenuWidget::GetSelectedProjectPresetSelection() const
{
	FProjectPresetSelection selection;
	selection.ScenarioPresetId = SelectedScenarioPresetId.TrimStartAndEnd().IsEmpty()
		? FString(DefaultScenarioPresetId)
		: SelectedScenarioPresetId.TrimStartAndEnd();
	selection.ProfilePresetId = SelectedProfilePresetId.TrimStartAndEnd().IsEmpty()
		? FString(DefaultProfilePresetId)
		: SelectedProfilePresetId.TrimStartAndEnd();
	selection.PolicyPresetId = SelectedPolicyPresetId.TrimStartAndEnd().IsEmpty()
		? FString(DefaultPolicyPresetId)
		: SelectedPolicyPresetId.TrimStartAndEnd();
	return selection;
}

TSubclassOf<UProjectTemplateCardWidget> UStartupMenuWidget::ResolveProjectTemplateCardWidgetClass() const
{
	if (ProjectTemplateCardWidgetClass)
	{
		return ProjectTemplateCardWidgetClass;
	}

	const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
	const TSubclassOf<UProjectTemplateCardWidget> configuredClass = platformUiSettings
		? platformUiSettings->ProjectTemplateCardWidgetClass.LoadSynchronous()
		: nullptr;
	if (!configuredClass)
	{
		UE_LOG(
			LogStartupMenuWidget,
			Warning,
			TEXT("ProjectTemplateCardWidgetClass is not configured on the StartupMenu widget asset or Platform UI project settings."));
		return nullptr;
	}

	return configuredClass;
}

UStartupMenuViewModel* UStartupMenuWidget::EnsureStartupMenuViewModel(
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem)
{
	if (!StartupMenuViewModel)
	{
		if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
		{
			StartupMenuViewModel = platformUiSubsystem->GetStartupMenuViewModel();
		}
	}
	if (!StartupMenuViewModel)
	{
		StartupMenuViewModel = NewObject<UStartupMenuViewModel>(this);
	}
	if (StartupMenuViewModel && simulatorLaunchSubsystem)
	{
		StartupMenuViewModel->SetSubsystemOverrides(simulatorLaunchSubsystem, nullptr, nullptr);
	}
	return StartupMenuViewModel;
}

UPlatformUiSubsystem* UStartupMenuWidget::GetPlatformUiSubsystem() const
{
	return UPlatformUiSubsystem::ResolveForWorldContext(this);
}
