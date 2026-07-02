#include "Platform/Widget/StartupScreenWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Platform/ViewModel/StartupScreenViewModel.h"
#include "Platform/Widget/RecentProjectCardWidget.h"
#include "UI/BaseButtonWidget.h"

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

	// WBP-authored scroll style에서 가로 scrollbar가 차지하는 높이를 계산한다.
	float ResolveHorizontalScrollbarHeight(const UScrollBox& scrollBox)
	{
		const FVector2D scrollbarThickness = scrollBox.GetScrollbarThickness();
		const FMargin scrollbarPadding = scrollBox.GetScrollbarPadding();
		return FMath::Max(
			scrollBox.GetWidgetBarStyle().Thickness,
			scrollbarThickness.Y + scrollbarPadding.Top + scrollbarPadding.Bottom);
	}

	// WBP-authored scroll style에서 세로 scrollbar가 차지하는 너비를 계산한다.
	float ResolveVerticalScrollbarWidth(const UScrollBox& scrollBox)
	{
		const FVector2D scrollbarThickness = scrollBox.GetScrollbarThickness();
		const FMargin scrollbarPadding = scrollBox.GetScrollbarPadding();
		return FMath::Max(
			scrollBox.GetWidgetBarStyle().Thickness,
			scrollbarThickness.X + scrollbarPadding.Left + scrollbarPadding.Right);
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
	if (StartupPanelHorizontalScrollBox)
	{
		StartupPanelHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelContentHorizontalScrolled);
		StartupPanelHorizontalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelContentHorizontalScrolled);
	}
	if (StartupPanelVerticalScrollBox)
	{
		StartupPanelVerticalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelContentVerticalScrolled);
		StartupPanelVerticalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelContentVerticalScrolled);
	}
	if (StartupPanelStickyHorizontalScrollBox)
	{
		StartupPanelStickyHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelStickyHorizontalScrolled);
		StartupPanelStickyHorizontalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelStickyHorizontalScrolled);
	}
	if (StartupPanelStickyVerticalScrollBox)
	{
		StartupPanelStickyVerticalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelStickyVerticalScrolled);
		StartupPanelStickyVerticalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelStickyVerticalScrolled);
	}
	CaptureStartupPanelAuthoredLayout();
	RefreshFromViewModel();
}

void UStartupScreenWidget::NativeTick(const FGeometry& myGeometry, const float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);
	UpdateStartupPanelScrollPadding(myGeometry.GetLocalSize());
}

void UStartupScreenWidget::NativeDestruct()
{
	UnbindControls();
	if (StartupPanelHorizontalScrollBox)
	{
		StartupPanelHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelContentHorizontalScrolled);
	}
	if (StartupPanelStickyHorizontalScrollBox)
	{
		StartupPanelStickyHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelStickyHorizontalScrolled);
	}
	if (StartupPanelVerticalScrollBox)
	{
		StartupPanelVerticalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelContentVerticalScrolled);
	}
	if (StartupPanelStickyVerticalScrollBox)
	{
		StartupPanelStickyVerticalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UStartupScreenWidget::HandleStartupPanelStickyVerticalScrolled);
	}
	for (URecentProjectCardWidget* cardWidget : RecentProjectCards)
	{
		if (cardWidget)
		{
			cardWidget->OnSelectedRequested.RemoveAll(this);
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
		RecentProjectCardPanel->AddChild(cardWidget);
		RecentProjectCards.Add(cardWidget);
	}
}

TSubclassOf<URecentProjectCardWidget> UStartupScreenWidget::ResolveRecentProjectCardWidgetClass() const
{
	return RecentProjectCardWidgetClass;
}

void UStartupScreenWidget::UpdateStartupPanelScrollPadding(const FVector2D& screenSize)
{
	if (!StartupPanelVerticalScrollBox || !StartupPanelHorizontalScrollBox || !StartupPanelSurface)
	{
		return;
	}
	if (!bHasStartupPanelSurfaceBasePadding && !CaptureStartupPanelAuthoredLayout())
	{
		return;
	}

	FVector2D viewportSize = StartupPanelVerticalScrollBox->GetCachedGeometry().GetLocalSize();
	if (viewportSize.IsNearlyZero())
	{
		viewportSize = screenSize;
	}
	if (viewportSize.X <= 0.0f || viewportSize.Y <= 0.0f)
	{
		return;
	}

	StartupPanelSurface->ForceLayoutPrepass();
	const FVector2D panelDesiredSize = StartupPanelSurface->GetDesiredSize();
	if (panelDesiredSize.X <= 0.0f || panelDesiredSize.Y <= 0.0f)
	{
		return;
	}
	const FVector2D basePaddingSize(
		StartupPanelSurfaceBasePadding.Left + StartupPanelSurfaceBasePadding.Right,
		StartupPanelSurfaceBasePadding.Top + StartupPanelSurfaceBasePadding.Bottom);
	const float horizontalScrollbarHeight = ResolveHorizontalScrollbarHeight(*StartupPanelHorizontalScrollBox);
	const float verticalScrollbarWidth = ResolveVerticalScrollbarWidth(*StartupPanelVerticalScrollBox);
	const FVector2D paddedPanelSize = panelDesiredSize + basePaddingSize;

	const bool bUsesStickyHorizontalScroll = StartupPanelStickyHorizontalScrollBox
		&& StartupPanelStickyHorizontalScrollSpacer;
	const bool bUsesStickyVerticalScroll = StartupPanelStickyVerticalScrollBox
		&& StartupPanelStickyVerticalScrollSpacer;
	bool bNeedsHorizontalScroll = false;
	bool bNeedsVerticalScroll = false;
	for (int32 passIndex = 0; passIndex < 2; ++passIndex)
	{
		const float availableWidth = viewportSize.X
			- (!bUsesStickyVerticalScroll && bNeedsVerticalScroll ? verticalScrollbarWidth : 0.0f);
		bNeedsHorizontalScroll = availableWidth + KINDA_SMALL_NUMBER < paddedPanelSize.X;
		const float requiredHeight = paddedPanelSize.Y
			+ (!bUsesStickyHorizontalScroll && bNeedsHorizontalScroll ? horizontalScrollbarHeight : 0.0f);
		bNeedsVerticalScroll = viewportSize.Y + KINDA_SMALL_NUMBER < requiredHeight;
	}

	if (CachedStartupPanelPaddingInput.Equals(viewportSize, KINDA_SMALL_NUMBER)
		&& CachedStartupPanelDesiredSize.Equals(panelDesiredSize, KINDA_SMALL_NUMBER)
		&& bCachedStartupPanelNeedsHorizontalScroll == bNeedsHorizontalScroll
		&& bCachedStartupPanelNeedsVerticalScroll == bNeedsVerticalScroll)
	{
		return;
	}

	CachedStartupPanelPaddingInput = viewportSize;
	CachedStartupPanelDesiredSize = panelDesiredSize;
	bCachedStartupPanelNeedsHorizontalScroll = bNeedsHorizontalScroll;
	bCachedStartupPanelNeedsVerticalScroll = bNeedsVerticalScroll;

	StartupPanelHorizontalScrollBox->SetAlwaysShowScrollbar(false);
	StartupPanelHorizontalScrollBox->SetScrollBarVisibility(
		!bUsesStickyHorizontalScroll && bNeedsHorizontalScroll
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	StartupPanelVerticalScrollBox->SetAlwaysShowScrollbar(false);
	StartupPanelVerticalScrollBox->SetScrollBarVisibility(
		!bUsesStickyVerticalScroll && bNeedsVerticalScroll
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);

	const float availablePanelCenteringWidth = viewportSize.X
		- (!bUsesStickyVerticalScroll && bNeedsVerticalScroll ? verticalScrollbarWidth : 0.0f);
	const float horizontalExtraPadding = FMath::Max(0.0f, (availablePanelCenteringWidth - panelDesiredSize.X - basePaddingSize.X) * 0.5f);
	const FMargin adjustedPanelPadding(
		StartupPanelSurfaceBasePadding.Left + horizontalExtraPadding,
		StartupPanelSurfaceBasePadding.Top,
		StartupPanelSurfaceBasePadding.Right + horizontalExtraPadding,
		StartupPanelSurfaceBasePadding.Bottom);

	if (UScrollBoxSlot* panelSlot = Cast<UScrollBoxSlot>(StartupPanelSurface->Slot))
	{
		panelSlot->SetPadding(adjustedPanelPadding);
	}

	if (StartupPanelStickyHorizontalScrollBox)
	{
		StartupPanelStickyHorizontalScrollBox->SetVisibility(
			bUsesStickyHorizontalScroll && bNeedsHorizontalScroll
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);

		if (StartupPanelStickyHorizontalScrollSpacer)
		{
			const float stickyRangeCompensation = bUsesStickyVerticalScroll && bNeedsVerticalScroll
				? verticalScrollbarWidth
				: 0.0f;
			StartupPanelStickyHorizontalScrollSpacer->SetSize(FVector2D(
				FMath::Max(1.0f, panelDesiredSize.X + adjustedPanelPadding.Left + adjustedPanelPadding.Right - stickyRangeCompensation),
				1.0f));
		}
	}

	if (StartupPanelStickyVerticalScrollBox)
	{
		StartupPanelStickyVerticalScrollBox->SetVisibility(
			bUsesStickyVerticalScroll && bNeedsVerticalScroll
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);

		if (StartupPanelStickyVerticalScrollSpacer)
		{
			const float stickyRangeCompensation = bUsesStickyHorizontalScroll && bNeedsHorizontalScroll
				? horizontalScrollbarHeight
				: 0.0f;
			StartupPanelStickyVerticalScrollSpacer->SetSize(FVector2D(
				1.0f,
				FMath::Max(1.0f, panelDesiredSize.Y + basePaddingSize.Y - stickyRangeCompensation)));
		}
	}

	if (!bNeedsHorizontalScroll)
	{
		TGuardValue<bool> syncGuard(bSyncingStartupPanelHorizontalScroll, true);
		StartupPanelHorizontalScrollBox->SetScrollOffset(0.0f);
		if (StartupPanelStickyHorizontalScrollBox)
		{
			StartupPanelStickyHorizontalScrollBox->SetScrollOffset(0.0f);
		}
	}
	if (!bNeedsVerticalScroll)
	{
		TGuardValue<bool> syncGuard(bSyncingStartupPanelVerticalScroll, true);
		StartupPanelVerticalScrollBox->SetScrollOffset(0.0f);
		if (StartupPanelStickyVerticalScrollBox)
		{
			StartupPanelStickyVerticalScrollBox->SetScrollOffset(0.0f);
		}
	}
}

bool UStartupScreenWidget::CaptureStartupPanelAuthoredLayout()
{
	if (!StartupPanelSurface)
	{
		return false;
	}

	const UScrollBoxSlot* panelSlot = Cast<UScrollBoxSlot>(StartupPanelSurface->Slot);
	if (!panelSlot)
	{
		return false;
	}

	StartupPanelSurfaceBasePadding = panelSlot->GetPadding();
	bHasStartupPanelSurfaceBasePadding = true;
	return true;
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

void UStartupScreenWidget::HandleStartupPanelStickyHorizontalScrolled(const float currentOffset)
{
	if (bSyncingStartupPanelHorizontalScroll || !StartupPanelHorizontalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingStartupPanelHorizontalScroll, true);
	StartupPanelHorizontalScrollBox->SetScrollOffset(currentOffset);
}

void UStartupScreenWidget::HandleStartupPanelContentHorizontalScrolled(const float currentOffset)
{
	if (bSyncingStartupPanelHorizontalScroll || !StartupPanelStickyHorizontalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingStartupPanelHorizontalScroll, true);
	StartupPanelStickyHorizontalScrollBox->SetScrollOffset(currentOffset);
}

void UStartupScreenWidget::HandleStartupPanelStickyVerticalScrolled(const float currentOffset)
{
	if (bSyncingStartupPanelVerticalScroll || !StartupPanelVerticalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingStartupPanelVerticalScroll, true);
	StartupPanelVerticalScrollBox->SetScrollOffset(currentOffset);
}

void UStartupScreenWidget::HandleStartupPanelContentVerticalScrolled(const float currentOffset)
{
	if (bSyncingStartupPanelVerticalScroll || !StartupPanelStickyVerticalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingStartupPanelVerticalScroll, true);
	StartupPanelStickyVerticalScrollBox->SetScrollOffset(currentOffset);
}

void UStartupScreenWidget::HandleRecentProjectCardSelected(URecentProjectCardWidget* cardWidget)
{
	if (!cardWidget)
	{
		return;
	}

	OpenProjectPath(cardWidget->GetProjectPath());
}
