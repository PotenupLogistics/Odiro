#include "Platform/Widget/StartupMenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/Widget/ProjectTemplateCardWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartupMenuWidget, Log, All);

namespace
{
	const TCHAR* ProjectOpenOptionsConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");
	const TCHAR* ProjectOpenParentFolderConfigKey = TEXT("ParentFolder");
	const TCHAR* ProjectOpenProjectNameConfigKey = TEXT("ProjectName");
	const TCHAR* ProjectOpenTemplateIdConfigKey = TEXT("TemplateId");
	const TCHAR* RecentProjectPathsConfigKey = TEXT("RecentProjectPaths");
	const TCHAR* StartupMenuDefaultWidgetBlueprintClassPath =
		TEXT("/Game/Widgets/MainMenu/WBP_StartupMenu.WBP_StartupMenu_C");
	const TCHAR* ProjectTemplateCardWidgetBlueprintClassPath =
		TEXT("/Game/Widgets/MainMenu/WBP_ProjectTemplateCard.WBP_ProjectTemplateCard_C");

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

	FString MakeProjectTemplateDisplayName(const FString& templateId)
	{
		FString displayName = templateId.TrimStartAndEnd();
		displayName.ReplaceInline(TEXT("-"), TEXT(" "));
		displayName.ReplaceInline(TEXT("_"), TEXT(" "));
		return FName::NameToDisplayString(displayName, false);
	}

	FString MakeRecentProjectDisplayName(const FString& projectPath)
	{
		const FString projectName = FPaths::GetCleanFilename(projectPath);
		return FString::Printf(TEXT("%s\n%s"), *projectName, *projectPath);
	}

	bool IsSameStartupMenuPath(const FString& left, const FString& right)
	{
		return NormalizeStartupMenuPath(left).Equals(NormalizeStartupMenuPath(right), ESearchCase::IgnoreCase);
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

	UClass* widgetClass = LoadClass<UStartupMenuWidget>(nullptr, StartupMenuDefaultWidgetBlueprintClassPath);
	if (!widgetClass)
	{
		UE_LOG(
			LogStartupMenuWidget,
			Error,
			TEXT("StartupMenu widget class is missing: %s"),
			StartupMenuDefaultWidgetBlueprintClassPath);
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
	if (!ValidateRequiredBindings())
	{
		return;
	}

	BindControls();
	HideRecentProjectDeleteDialog();
	LoadProjectOpenOptions();
	InitializeProjectPathInputs();
	RefreshProjectTemplateOptions();
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

	for (UProjectTemplateCardWidget* cardWidget : ProjectTemplateCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
		}
	}
	ProjectTemplateCards.Reset();

	Super::NativeDestruct();
}

void UStartupMenuWidget::SetProjectPathForPrototype(const FString& projectPath)
{
	const FString normalizedProjectPath = NormalizeStartupMenuPath(projectPath);
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

void UStartupMenuWidget::SelectProjectTemplate(const FString& templateId)
{
	const FString trimmedTemplateId = templateId.TrimStartAndEnd();
	SelectedProjectTemplateId = trimmedTemplateId.IsEmpty() ? FString(TEXT("blank")) : trimmedTemplateId;
	RefreshProjectTemplateCardStates();
	RefreshProjectOpenActions();
}

bool UStartupMenuWidget::CreateSelectedProject(
	TArray<FString>& outDiagnostics,
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem)
{
	outDiagnostics.Reset();

	USimulatorLaunchSubsystem* subsystem = simulatorLaunchSubsystem ? simulatorLaunchSubsystem : GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	if (!subsystem->CreateProjectFromTemplate(GetSelectedProjectPath(), GetSelectedProjectTemplateId(), outDiagnostics))
	{
		return false;
	}

	return subsystem->ValidateUserProject(GetSelectedProjectPath(), outDiagnostics);
}

bool UStartupMenuWidget::ValidateSelectedProject(
	TArray<FString>& outDiagnostics,
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem) const
{
	outDiagnostics.Reset();

	USimulatorLaunchSubsystem* subsystem = simulatorLaunchSubsystem ? simulatorLaunchSubsystem : GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	return subsystem->ValidateUserProject(GetSelectedProjectPath(), outDiagnostics);
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

void UStartupMenuWidget::HandleBackToRecentProjectsClicked()
{
	ShowRecentProjectsScreen();
	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString());
}

void UStartupMenuWidget::HandleCreateProjectClicked()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetProjectOpenWarningText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return;
	}

	const FString projectPath = GetSelectedProjectPath();
	if (projectPath.TrimStartAndEnd().IsEmpty())
	{
		SetProjectOpenWarningText(TEXT("프로젝트 이름을 입력하세요."));
		RefreshProjectOpenActions();
		return;
	}
	if (IFileManager::Get().DirectoryExists(*projectPath))
	{
		SetProjectOpenWarningText(TEXT("이미 존재하는 프로젝트입니다."));
		RefreshProjectOpenActions();
		return;
	}
	if (FPaths::FileExists(projectPath))
	{
		SetProjectOpenWarningText(TEXT("같은 이름의 파일이 있습니다."));
		RefreshProjectOpenActions();
		return;
	}

	SaveProjectOpenOptions();

	TArray<FString> diagnostics;
	if (!CreateSelectedProject(diagnostics, subsystem))
	{
		SetProjectOpenWarningText(diagnostics.IsEmpty() ? TEXT("프로젝트 생성 실패") : diagnostics[0]);
		SetDiagnosticsText(FString::Join(diagnostics, TEXT("\n")));
		return;
	}

	RememberRecentProject(projectPath);
	if (!CommitActiveProjectAndOpenEditor())
	{
		return;
	}

	SetProjectOpenWarningText(FString());
	SetDiagnosticsText(FString::Printf(TEXT("Project created: %s"), *projectPath));
}

void UStartupMenuWidget::BindControls()
{
	if (ProjectParentFolderTextBox)
	{
		ProjectParentFolderTextBox->OnTextChanged.RemoveDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
		ProjectParentFolderTextBox->OnTextChanged.AddDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
	}
	if (ProjectNameTextBox)
	{
		ProjectNameTextBox->OnTextChanged.RemoveDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
		ProjectNameTextBox->OnTextChanged.AddDynamic(this, &UStartupMenuWidget::HandleProjectOpenInputChanged);
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
	requireWidget(RecentDiagnosticsTextBlock, TEXT("RecentDiagnosticsTextBlock"));
	requireWidget(RecentProjectDeleteDialog, TEXT("RecentProjectDeleteDialog"));
	requireWidget(RecentProjectDeleteDialogMessageText, TEXT("RecentProjectDeleteDialogMessageText"));
	requireWidget(RecentProjectDeleteConfirmButton, TEXT("RecentProjectDeleteConfirmButton"));
	requireWidget(RecentProjectDeleteCancelButton, TEXT("RecentProjectDeleteCancelButton"));
	requireWidget(CreateNewProjectButton, TEXT("CreateNewProjectButton"));
	requireWidget(BackToRecentProjectsButton, TEXT("BackToRecentProjectsButton"));
	requireWidget(ProjectParentFolderTextBox, TEXT("ProjectParentFolderTextBox"));
	requireWidget(ProjectNameTextBox, TEXT("ProjectNameTextBox"));
	requireWidget(ProjectTemplateCardBox, TEXT("ProjectTemplateCardBox"));
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
	FString parentFolder;
	FString projectName;
	FString templateId;
	if (GConfig)
	{
		GConfig->GetString(ProjectOpenOptionsConfigSection, ProjectOpenParentFolderConfigKey, parentFolder, GGameUserSettingsIni);
		GConfig->GetString(ProjectOpenOptionsConfigSection, ProjectOpenProjectNameConfigKey, projectName, GGameUserSettingsIni);
		GConfig->GetString(ProjectOpenOptionsConfigSection, ProjectOpenTemplateIdConfigKey, templateId, GGameUserSettingsIni);
	}

	SelectedProjectParentFolder = parentFolder.TrimStartAndEnd().IsEmpty()
		? GetDefaultProjectParentFolder()
		: NormalizeStartupMenuPath(parentFolder);
	SelectedProjectName = projectName.TrimStartAndEnd().IsEmpty()
		? FString(TEXT("OdiroProject"))
		: NormalizeProjectDirectoryName(projectName);
	SelectedProjectTemplateId = templateId.TrimStartAndEnd().IsEmpty() ? FString(TEXT("blank")) : templateId.TrimStartAndEnd();
	LoadRecentProjectPaths();
}

void UStartupMenuWidget::SaveProjectOpenOptions()
{
	CacheProjectOpenOptionsFromWidgets();
	if (!GConfig)
	{
		return;
	}

	GConfig->SetString(ProjectOpenOptionsConfigSection, ProjectOpenParentFolderConfigKey, *SelectedProjectParentFolder, GGameUserSettingsIni);
	GConfig->SetString(ProjectOpenOptionsConfigSection, ProjectOpenProjectNameConfigKey, *SelectedProjectName, GGameUserSettingsIni);
	GConfig->SetString(ProjectOpenOptionsConfigSection, ProjectOpenTemplateIdConfigKey, *GetSelectedProjectTemplateId(), GGameUserSettingsIni);
	GConfig->SetArray(ProjectOpenOptionsConfigSection, RecentProjectPathsConfigKey, RecentProjectPaths, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UStartupMenuWidget::LoadRecentProjectPaths()
{
	RecentProjectPaths.Reset();

	TArray<FString> storedPaths;
	if (GConfig)
	{
		GConfig->GetArray(ProjectOpenOptionsConfigSection, RecentProjectPathsConfigKey, storedPaths, GGameUserSettingsIni);
	}

	for (const FString& storedPath : storedPaths)
	{
		const FString normalizedPath = NormalizeStartupMenuPath(storedPath);
		if (normalizedPath.IsEmpty())
		{
			continue;
		}

		const bool bAlreadyPresent = RecentProjectPaths.ContainsByPredicate(
			[&normalizedPath](const FString& existingPath)
			{
				return IsSameStartupMenuPath(existingPath, normalizedPath);
			});
		if (!bAlreadyPresent)
		{
			RecentProjectPaths.Add(normalizedPath);
		}
	}

	if (RecentProjectPaths.IsEmpty())
	{
		const FString lastProjectPath = BuildProjectPathFromInputs(SelectedProjectParentFolder, SelectedProjectName);
		if (!lastProjectPath.IsEmpty() && IFileManager::Get().DirectoryExists(*lastProjectPath))
		{
			RecentProjectPaths.Add(lastProjectPath);
		}
	}
}

void UStartupMenuWidget::SaveRecentProjectPaths() const
{
	if (!GConfig)
	{
		return;
	}

	GConfig->SetArray(ProjectOpenOptionsConfigSection, RecentProjectPathsConfigKey, RecentProjectPaths, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UStartupMenuWidget::RememberRecentProject(const FString& projectPath)
{
	const FString normalizedPath = NormalizeStartupMenuPath(projectPath);
	if (normalizedPath.IsEmpty())
	{
		return;
	}

	RecentProjectPaths.RemoveAll(
		[&normalizedPath](const FString& existingPath)
		{
			return IsSameStartupMenuPath(existingPath, normalizedPath);
		});
	RecentProjectPaths.Insert(normalizedPath, 0);

	while (RecentProjectPaths.Num() > MaxRecentProjectCount)
	{
		RecentProjectPaths.RemoveAt(RecentProjectPaths.Num() - 1);
	}

	SaveRecentProjectPaths();
}

bool UStartupMenuWidget::RemoveRecentProject(const FString& projectPath)
{
	const FString normalizedPath = NormalizeStartupMenuPath(projectPath);
	if (normalizedPath.IsEmpty())
	{
		return false;
	}

	const int32 removedCount = RecentProjectPaths.RemoveAll(
		[&normalizedPath](const FString& existingPath)
		{
			return IsSameStartupMenuPath(existingPath, normalizedPath);
		});
	if (removedCount <= 0)
	{
		return false;
	}

	SaveRecentProjectPaths();
	return true;
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
	TArray<FString> visibleProjectPaths;
	for (const FString& recentProjectPath : RecentProjectPaths)
	{
		const FString normalizedProjectPath = NormalizeStartupMenuPath(recentProjectPath);
		if (!normalizedProjectPath.IsEmpty() && IFileManager::Get().DirectoryExists(*normalizedProjectPath))
		{
			visibleProjectPaths.Add(normalizedProjectPath);
		}
	}

	const bool bPrunedMissingProjects = visibleProjectPaths.Num() != RecentProjectPaths.Num();
	RecentProjectPaths = visibleProjectPaths;
	if (bPrunedMissingProjects)
	{
		SaveRecentProjectPaths();
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

		cardWidget->InitializeCard(recentProjectPath, MakeRecentProjectDisplayName(recentProjectPath));
		cardWidget->OnSelectedRequested.AddUObject(this, &UStartupMenuWidget::HandleRecentProjectCardSelected);
		cardWidget->OnContextRequested.AddUObject(this, &UStartupMenuWidget::HandleRecentProjectCardContextRequested);
		RecentProjectCards.Add(cardWidget);
		RecentProjectCardWrapBox->AddChildToWrapBox(cardWidget);
	}
}

void UStartupMenuWidget::RefreshProjectTemplateOptions()
{
	if (!ProjectTemplateCardBox)
	{
		return;
	}

	for (UProjectTemplateCardWidget* cardWidget : ProjectTemplateCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
		}
	}
	ProjectTemplateCards.Reset();
	ProjectTemplateCardBox->ClearChildren();

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SelectProjectTemplate(TEXT("blank"));
		return;
	}

	TArray<FString> templateIds = subsystem->ListProjectTemplates();
	if (templateIds.IsEmpty())
	{
		templateIds.Add(TEXT("blank"));
	}

	TSubclassOf<UProjectTemplateCardWidget> cardClass = ResolveProjectTemplateCardWidgetClass();
	for (const FString& templateId : templateIds)
	{
		if (!cardClass)
		{
			break;
		}

		UProjectTemplateCardWidget* cardWidget = CreateWidget<UProjectTemplateCardWidget>(GetOwningPlayer(), cardClass);
		if (!cardWidget)
		{
			continue;
		}

		cardWidget->InitializeCard(templateId, MakeProjectTemplateDisplayName(templateId));
		cardWidget->OnSelectedRequested.AddUObject(this, &UStartupMenuWidget::HandleProjectTemplateCardSelected);
		ProjectTemplateCards.Add(cardWidget);
		if (UHorizontalBoxSlot* cardSlot = ProjectTemplateCardBox->AddChildToHorizontalBox(cardWidget))
		{
			cardSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}
	}

	if (GetSelectedProjectTemplateId().IsEmpty() && !templateIds.IsEmpty())
	{
		SelectProjectTemplate(templateIds[0]);
	}
	RefreshProjectTemplateCardStates();
}

void UStartupMenuWidget::RefreshProjectOpenActions()
{
	const FString selectedProjectPath = GetSelectedProjectPath();
	const bool bHasProjectPath = !selectedProjectPath.TrimStartAndEnd().IsEmpty();
	const bool bProjectDirectoryExists = IFileManager::Get().DirectoryExists(*selectedProjectPath);
	const bool bProjectFileExists = FPaths::FileExists(selectedProjectPath);

	if (CreateProjectButton)
	{
		CreateProjectButton->SetIsEnabled(bHasProjectPath && !bProjectDirectoryExists && !bProjectFileExists);
	}
}

void UStartupMenuWidget::RefreshProjectTemplateCardStates()
{
	const FString selectedTemplateId = GetSelectedProjectTemplateId();
	for (UProjectTemplateCardWidget* cardWidget : ProjectTemplateCards)
	{
		if (cardWidget)
		{
			cardWidget->SetSelected(cardWidget->GetTemplateId().Equals(selectedTemplateId, ESearchCase::IgnoreCase));
		}
	}
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
	if (RecentDiagnosticsTextBlock)
	{
		RecentDiagnosticsTextBlock->SetText(FText::FromString(message));
	}
}

bool UStartupMenuWidget::OpenExistingProject(const FString& projectPath)
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetProjectOpenWarningText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupMenuPath(projectPath);
	if (normalizedProjectPath.IsEmpty())
	{
		SetProjectOpenWarningText(TEXT("프로젝트를 선택하세요."));
		return false;
	}
	if (!IFileManager::Get().DirectoryExists(*normalizedProjectPath))
	{
		SetProjectOpenWarningText(TEXT("프로젝트 폴더가 없습니다."));
		RefreshRecentProjectCards();
		return false;
	}

	SetProjectPathForPrototype(normalizedProjectPath);

	TArray<FString> diagnostics;
	if (!ValidateSelectedProject(diagnostics, subsystem))
	{
		SetProjectOpenWarningText(diagnostics.IsEmpty() ? TEXT("프로젝트 검증 실패") : diagnostics[0]);
		SetDiagnosticsText(FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	RememberRecentProject(normalizedProjectPath);
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
	UProjectSessionSubsystem* projectSession = GetProjectSessionSubsystem();
	if (!projectSession)
	{
		SetProjectOpenWarningText(TEXT("ProjectSessionSubsystem을 사용할 수 없습니다."));
		return false;
	}

	projectSession->SetActiveProjectPath(projectPath);

	UScenarioEditorLaunchSubsystem* scenarioEditorLaunch = GetScenarioEditorLaunchSubsystem();
	if (!scenarioEditorLaunch)
	{
		SetProjectOpenWarningText(TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString scenarioPath = projectSession->GetActiveProjectScenarioPath();
	if (!scenarioEditorLaunch->OpenScenarioEditor(scenarioPath))
	{
		SetProjectOpenWarningText(TEXT("ScenarioEditorMap 열기 실패."));
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

	OpenExistingProject(cardWidget->GetTemplateId());
}

void UStartupMenuWidget::HandleRecentProjectCardContextRequested(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	ShowRecentProjectDeleteDialog(cardWidget->GetTemplateId());
}

void UStartupMenuWidget::HandleProjectTemplateCardSelected(UProjectTemplateCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	SelectProjectTemplate(cardWidget->GetTemplateId());
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

FString UStartupMenuWidget::GetSelectedProjectTemplateId() const
{
	if (!SelectedProjectTemplateId.TrimStartAndEnd().IsEmpty())
	{
		return SelectedProjectTemplateId;
	}

	return FString(TEXT("blank"));
}

TSubclassOf<UProjectTemplateCardWidget> UStartupMenuWidget::ResolveProjectTemplateCardWidgetClass() const
{
	if (ProjectTemplateCardWidgetClass)
	{
		return ProjectTemplateCardWidgetClass;
	}

	UClass* loadedClass = LoadClass<UProjectTemplateCardWidget>(nullptr, ProjectTemplateCardWidgetBlueprintClassPath);
	if (!loadedClass)
	{
		UE_LOG(
			LogStartupMenuWidget,
			Warning,
			TEXT("Project template card widget class load failed | Path: %s"),
			ProjectTemplateCardWidgetBlueprintClassPath);
		return nullptr;
	}

	return TSubclassOf<UProjectTemplateCardWidget>(loadedClass);
}

USimulatorLaunchSubsystem* UStartupMenuWidget::GetSimulatorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UScenarioEditorLaunchSubsystem* UStartupMenuWidget::GetScenarioEditorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UStartupMenuWidget::GetProjectSessionSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}
