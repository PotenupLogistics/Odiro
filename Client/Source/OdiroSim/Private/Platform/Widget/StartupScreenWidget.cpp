#include "Platform/Widget/StartupScreenWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/Widget/RecentProjectContextMenuWidget.h"
#include "Platform/Widget/RecentProjectDeleteConfirmDialogWidget.h"
#include "Platform/ViewModel/StartupScreenViewModel.h"
#include "Platform/Widget/RecentProjectCardWidget.h"
#include "UI/BaseButtonWidget.h"
#include "Blueprint/SlateBlueprintLibrary.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#endif

namespace
{
	// Dialog 초기 위치로 쓸 수 있는 project 또는 user folder를 반환한다.
	FString ResolveStartupScreenInitialFolder(const UStartupScreenViewModel* viewModel)
	{
		const FString selectedProjectPath = viewModel ? viewModel->GetSelectedProjectPath() : FString();
		if (!selectedProjectPath.IsEmpty())
		{
			const FString parentFolder = FPaths::GetPath(selectedProjectPath);
			return parentFolder.IsEmpty() ? selectedProjectPath : parentFolder;
		}

		return FPlatformProcess::UserDir();
	}

	// 경로 입력을 dialog/open command용 absolute normalized path로 맞춘다.
	FString NormalizeStartupScreenWidgetPath(FString path)
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
}

void UStartupScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UStartupScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureStartupScreenViewModel();
	ApplyDiagnosticMessagesToViewModel();
	BindControls();
	RefreshFromViewModel();
}

void UStartupScreenWidget::NativeDestruct()
{
	UnbindControls();
	CloseTransientPopups();
	for (URecentProjectCardWidget* cardWidget : RecentProjectCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
			cardWidget->OnContextMenuRequested.RemoveAll(this);
		}
	}
	RecentProjectCards.Reset();

	Super::NativeDestruct();
}

void UStartupScreenWidget::SetViewModel(UStartupScreenViewModel* viewModel)
{
	StartupScreenViewModel = viewModel;
	if (StartupScreenViewModel)
	{
		ApplyDiagnosticMessagesToViewModel();
		StartupScreenViewModel->InitializeForGameInstance(GetGameInstance());
	}
	RefreshFromViewModel();
}

void UStartupScreenWidget::RefreshFromViewModel()
{
	if (UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel())
	{
		viewModel->RefreshRecentProjects();

		const FString diagnosticsText = viewModel->GetDiagnosticsText();
		const ESlateVisibility diagnosticsVisibility = diagnosticsText.TrimStartAndEnd().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible;
		if (DiagnosticsTextBox)
		{
			DiagnosticsTextBox->SetVisibility(diagnosticsVisibility);
		}
		if (DiagnosticsText)
		{
			DiagnosticsText->SetText(FText::FromString(diagnosticsText));
			DiagnosticsText->SetVisibility(diagnosticsVisibility);
		}
		RefreshRecentProjectCards();
	}
}

void UStartupScreenWidget::CloseTransientPopups()
{
	CloseRecentProjectContextMenu();
	CloseDeleteConfirmDialog();
}

bool UStartupScreenWidget::OpenSelectedProject()
{
	UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel();
	if (!viewModel)
	{
		return false;
	}

	const FString selectedProjectPath = viewModel->GetSelectedProjectPath();
	if (selectedProjectPath.TrimStartAndEnd().IsEmpty())
	{
		viewModel->SetDiagnosticsText(DiagnosticMessages.ProjectRequired);
		RefreshFromViewModel();
		return false;
	}

	return OpenProjectPath(selectedProjectPath);
}

bool UStartupScreenWidget::OpenProjectPath(const FString& projectPath)
{
	UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel();
	if (!viewModel)
	{
		return false;
	}

	const FString normalizedProjectPath = NormalizeStartupScreenWidgetPath(projectPath);
	const bool bOpened = viewModel->OpenProject(normalizedProjectPath);
	RefreshFromViewModel();
	if (bOpened)
	{
		OnProjectOpened.Broadcast(this, normalizedProjectPath);
	}
	return bOpened;
}

UStartupScreenViewModel* UStartupScreenWidget::EnsureStartupScreenViewModel()
{
	if (!StartupScreenViewModel)
	{
		StartupScreenViewModel = NewObject<UStartupScreenViewModel>(this);
		if (StartupScreenViewModel)
		{
			ApplyDiagnosticMessagesToViewModel();
			StartupScreenViewModel->InitializeForGameInstance(GetGameInstance());
		}
	}
	return StartupScreenViewModel;
}

void UStartupScreenWidget::BindControls()
{
	if (OpenProjectButton)
	{
		OpenProjectButton->OnBaseClicked.RemoveDynamic(this, &UStartupScreenWidget::HandleOpenProjectClicked);
		OpenProjectButton->OnBaseClicked.AddDynamic(this, &UStartupScreenWidget::HandleOpenProjectClicked);
	}
	if (CreateProjectButton)
	{
		CreateProjectButton->OnBaseClicked.RemoveDynamic(this, &UStartupScreenWidget::HandleCreateProjectClicked);
		CreateProjectButton->OnBaseClicked.AddDynamic(this, &UStartupScreenWidget::HandleCreateProjectClicked);
	}
}

void UStartupScreenWidget::UnbindControls()
{
	if (OpenProjectButton)
	{
		OpenProjectButton->OnBaseClicked.RemoveDynamic(this, &UStartupScreenWidget::HandleOpenProjectClicked);
	}
	if (CreateProjectButton)
	{
		CreateProjectButton->OnBaseClicked.RemoveDynamic(this, &UStartupScreenWidget::HandleCreateProjectClicked);
	}
}

void UStartupScreenWidget::ApplyDiagnosticMessagesToViewModel()
{
	if (StartupScreenViewModel)
	{
		StartupScreenViewModel->SetDiagnosticMessages(DiagnosticMessages);
	}
}

void UStartupScreenWidget::RefreshRecentProjectCards()
{
	if (!RecentProjectCardPanel)
	{
		return;
	}

	for (URecentProjectCardWidget* cardWidget : RecentProjectCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
			cardWidget->OnContextMenuRequested.RemoveAll(this);
		}
	}
	RecentProjectCards.Reset();
	RecentProjectCardPanel->ClearChildren();

	UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel();
	if (viewModel && viewModel->GetSelectedProjectPath().TrimStartAndEnd().IsEmpty())
	{
		const TArray<FStartupScreenRecentProjectItem> candidateProjects = viewModel->GetRecentProjects();
		if (!candidateProjects.IsEmpty())
		{
			viewModel->SelectProject(candidateProjects[0].ProjectPath);
		}
	}

	const TArray<FStartupScreenRecentProjectItem> recentProjects = viewModel
		? viewModel->GetRecentProjects()
		: TArray<FStartupScreenRecentProjectItem>();
	if (RecentProjectsEmptyState)
	{
		RecentProjectsEmptyState->SetVisibility(recentProjects.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	const TSubclassOf<URecentProjectCardWidget> cardClass = ResolveRecentProjectCardWidgetClass();
	if (!cardClass || !GetWorld())
	{
		return;
	}

	for (const FStartupScreenRecentProjectItem& recentProject : recentProjects)
	{
		URecentProjectCardWidget* cardWidget = CreateWidget<URecentProjectCardWidget>(GetWorld(), cardClass);
		if (!cardWidget)
		{
			continue;
		}

		cardWidget->InitializeCard(recentProject);
		cardWidget->OnSelectedRequested.AddUObject(this, &UStartupScreenWidget::HandleRecentProjectCardSelected);
		cardWidget->OnContextMenuRequested.AddUObject(
			this,
			&UStartupScreenWidget::HandleRecentProjectCardContextMenuRequested);
		RecentProjectCardPanel->AddChild(cardWidget);
		RecentProjectCards.Add(cardWidget);
	}
}

TSubclassOf<URecentProjectCardWidget> UStartupScreenWidget::ResolveRecentProjectCardWidgetClass() const
{
	return RecentProjectCardWidgetClass;
}

void UStartupScreenWidget::OpenRecentProjectContextMenu(
	URecentProjectCardWidget* cardWidget,
	const FVector2D& screenPosition)
{
	CloseRecentProjectContextMenu();

	if (!cardWidget || !RecentProjectContextMenuWidgetClass || !GetWorld())
	{
		return;
	}

	ContextMenuProjectPath = NormalizeStartupScreenWidgetPath(cardWidget->GetProjectPath());
	if (ContextMenuProjectPath.IsEmpty())
	{
		return;
	}

	ActiveRecentProjectContextMenu = CreateWidget<URecentProjectContextMenuWidget>(
		GetWorld(),
		RecentProjectContextMenuWidgetClass);
	if (!ActiveRecentProjectContextMenu)
	{
		ContextMenuProjectPath.Reset();
		return;
	}

	ActiveRecentProjectContextMenu->OnRemoveFromListSelected.RemoveAll(this);
	ActiveRecentProjectContextMenu->OnRemoveFromListSelected.AddUObject(
		this,
		&UStartupScreenWidget::HandleRemoveRecentProjectFromList);
	ActiveRecentProjectContextMenu->OnDeleteProjectSelected.RemoveAll(this);
	ActiveRecentProjectContextMenu->OnDeleteProjectSelected.AddUObject(
		this,
		&UStartupScreenWidget::HandleDeleteRecentProjectRequested);
	ActiveRecentProjectContextMenu->OnDismissRequested.RemoveAll(this);
	ActiveRecentProjectContextMenu->OnDismissRequested.AddUObject(
		this,
		&UStartupScreenWidget::HandleRecentProjectContextMenuDismissed);
	ActiveRecentProjectContextMenu->AddToViewport(RecentProjectContextMenuZOrder);

	FVector2D pixelPosition = FVector2D::ZeroVector;
	FVector2D viewportPosition = FVector2D::ZeroVector;
	USlateBlueprintLibrary::AbsoluteToViewport(this, screenPosition, pixelPosition, viewportPosition);

	ActiveRecentProjectContextMenu->OpenAtViewportPosition(viewportPosition);
}

void UStartupScreenWidget::CloseRecentProjectContextMenu()
{
	if (ActiveRecentProjectContextMenu)
	{
		ActiveRecentProjectContextMenu->OnRemoveFromListSelected.RemoveAll(this);
		ActiveRecentProjectContextMenu->OnDeleteProjectSelected.RemoveAll(this);
		ActiveRecentProjectContextMenu->OnDismissRequested.RemoveAll(this);
		ActiveRecentProjectContextMenu->RemoveFromParent();
		ActiveRecentProjectContextMenu = nullptr;
	}
	ContextMenuProjectPath.Reset();
}

void UStartupScreenWidget::OpenDeleteConfirmDialog(const FString& projectPath)
{
	CloseDeleteConfirmDialog();

	if (!DeleteConfirmDialogWidgetClass || !GetWorld())
	{
		return;
	}

	PendingDeleteProjectPath = NormalizeStartupScreenWidgetPath(projectPath);
	if (PendingDeleteProjectPath.IsEmpty())
	{
		return;
	}

	ActiveDeleteConfirmDialog = CreateWidget<URecentProjectDeleteConfirmDialogWidget>(
		GetWorld(),
		DeleteConfirmDialogWidgetClass);
	if (!ActiveDeleteConfirmDialog)
	{
		PendingDeleteProjectPath.Reset();
		return;
	}

	ActiveDeleteConfirmDialog->OnConfirmed.RemoveAll(this);
	ActiveDeleteConfirmDialog->OnConfirmed.AddUObject(
		this,
		&UStartupScreenWidget::HandleDeleteConfirmAccepted);
	ActiveDeleteConfirmDialog->OnCanceled.RemoveAll(this);
	ActiveDeleteConfirmDialog->OnCanceled.AddUObject(
		this,
		&UStartupScreenWidget::HandleDeleteConfirmCanceled);
	ActiveDeleteConfirmDialog->SetDeleteTarget(PendingDeleteProjectPath);
	ActiveDeleteConfirmDialog->AddToViewport(DeleteConfirmDialogZOrder);
}

void UStartupScreenWidget::CloseDeleteConfirmDialog()
{
	if (ActiveDeleteConfirmDialog)
	{
		ActiveDeleteConfirmDialog->OnConfirmed.RemoveAll(this);
		ActiveDeleteConfirmDialog->OnCanceled.RemoveAll(this);
		ActiveDeleteConfirmDialog->RemoveFromParent();
		ActiveDeleteConfirmDialog = nullptr;
	}
	PendingDeleteProjectPath.Reset();
}

bool UStartupScreenWidget::BrowseForExistingProjectFolder(FString& outFolder) const
{
	outFolder.Reset();

#if WITH_EDITOR
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
	if (!desktopPlatform)
	{
		return false;
	}

	const FString initialFolder = NormalizeStartupScreenWidgetPath(ResolveStartupScreenInitialFolder(StartupScreenViewModel));
	const void* parentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;
	FString selectedFolder;
	if (!desktopPlatform->OpenDirectoryDialog(
		parentWindowHandle,
		BrowseDialogTitle.ToString(),
		initialFolder,
		selectedFolder))
	{
		return false;
	}

	outFolder = NormalizeStartupScreenWidgetPath(selectedFolder);
	return !outFolder.IsEmpty();
#else
	return false;
#endif
}

void UStartupScreenWidget::HandleOpenProjectClicked(UBaseButtonWidget*)
{
	UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel();
	if (!viewModel)
	{
		return;
	}

	FString selectedProjectFolder;
	if (!BrowseForExistingProjectFolder(selectedProjectFolder))
	{
		viewModel->ClearDiagnostics();
		RefreshFromViewModel();
		return;
	}

	OpenProjectPath(selectedProjectFolder);
}

void UStartupScreenWidget::HandleCreateProjectClicked(UBaseButtonWidget*)
{
	OnCreateProjectRequested.Broadcast(this);
}

void UStartupScreenWidget::HandleRecentProjectCardSelected(URecentProjectCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	CloseRecentProjectContextMenu();
	OpenProjectPath(cardWidget->GetProjectPath());
}

void UStartupScreenWidget::HandleRecentProjectCardContextMenuRequested(
	URecentProjectCardWidget* cardWidget,
	const FVector2D screenPosition)
{
	OpenRecentProjectContextMenu(cardWidget, screenPosition);
}

void UStartupScreenWidget::HandleRemoveRecentProjectFromList(URecentProjectContextMenuWidget*)
{
	const FString projectPath = ContextMenuProjectPath;
	CloseRecentProjectContextMenu();

	UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel();
	if (!viewModel || projectPath.IsEmpty())
	{
		return;
	}

	viewModel->RemoveRecentProject(projectPath);
	RefreshFromViewModel();
}

void UStartupScreenWidget::HandleDeleteRecentProjectRequested(URecentProjectContextMenuWidget*)
{
	const FString projectPath = ContextMenuProjectPath;
	CloseRecentProjectContextMenu();

	if (!projectPath.IsEmpty())
	{
		OpenDeleteConfirmDialog(projectPath);
	}
}

void UStartupScreenWidget::HandleRecentProjectContextMenuDismissed(URecentProjectContextMenuWidget*)
{
	CloseRecentProjectContextMenu();
}

void UStartupScreenWidget::HandleDeleteConfirmAccepted(URecentProjectDeleteConfirmDialogWidget*)
{
	const FString projectPath = PendingDeleteProjectPath;
	CloseDeleteConfirmDialog();

	UStartupScreenViewModel* viewModel = EnsureStartupScreenViewModel();
	if (!viewModel || projectPath.IsEmpty())
	{
		return;
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	if (!platformUiSubsystem)
	{
		viewModel->SetDiagnosticsText(DiagnosticMessages.ProjectDeleteFailed);
		return;
	}

	const FString activeProjectPath = NormalizeStartupScreenWidgetPath(platformUiSubsystem->GetActiveProjectPath());
	if (!activeProjectPath.IsEmpty() && projectPath.Equals(activeProjectPath, ESearchCase::IgnoreCase))
	{
		FString startupErrorText;
		if (!platformUiSubsystem->ReturnToStartupMap(startupErrorText))
		{
			viewModel->SetDiagnosticsText(startupErrorText.IsEmpty()
				? DiagnosticMessages.ProjectDeleteFailed
				: startupErrorText);
			RefreshFromViewModel();
			return;
		}
	}

	const EPlatformUserProjectDeleteResult deleteResult =
		platformUiSubsystem->DeleteUserProjectDirectory(projectPath);
	if (deleteResult == EPlatformUserProjectDeleteResult::Unsafe)
	{
		viewModel->SetDiagnosticsText(DiagnosticMessages.ProjectDeleteUnsafe);
		RefreshFromViewModel();
		return;
	}
	if (deleteResult == EPlatformUserProjectDeleteResult::Failed)
	{
		viewModel->SetDiagnosticsText(DiagnosticMessages.ProjectDeleteFailed);
		RefreshFromViewModel();
		return;
	}

	if (deleteResult == EPlatformUserProjectDeleteResult::Deleted
		|| deleteResult == EPlatformUserProjectDeleteResult::Missing)
	{
		viewModel->RemoveRecentProject(projectPath);
		viewModel->ClearDiagnostics();
	}
	RefreshFromViewModel();
}

void UStartupScreenWidget::HandleDeleteConfirmCanceled(URecentProjectDeleteConfirmDialogWidget*)
{
	CloseDeleteConfirmDialog();
}
