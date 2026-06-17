#include "Scenario/Widget/ScenarioEditorRootWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Scenario/Widget/ScenarioAssetPaletteWidget.h"
#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"
#include "Scenario/Widget/ScenarioPlaceableContextMenuWidget.h"
#include "Scenario/Widget/ScenarioPlaceableDetailsWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"

void UScenarioEditorRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HidePlaceableDetails();
	HideAssetPaletteWidget();
	SetLlmPanelVisible(false);
	BindEditorModeButtons();
	BindTemplateSidebarToolbar();
	RefreshTemplateSidebarWidget();
	RefreshViewModeButtons();
	RefreshPlacementSnapButton();
	BindEditorLaunchSubsystem();
}

void UScenarioEditorRootWidget::NativeDestruct()
{
	UnbindTemplateSidebarToolbar();
	UnbindEditorModeButtons();
	UnbindEditorLaunchSubsystem();
	Super::NativeDestruct();
}

void UScenarioEditorRootWidget::NativeTick(const FGeometry& myGeometry, const float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);

	if (bAutoRevealLlmPanelOnRightEdge)
	{
		SetLlmPanelVisible(ShouldRevealLlmPanelFromMouseEdge());
	}
	if (bAutoRevealAssetPaletteOnBottomEdge)
	{
		SetAssetPaletteVisible(ShouldRevealAssetPaletteFromMouseEdge(), true);
	}

	// keyboard toggle ??ë²„íŠ¼ ??ê²½ë¡œë¡?view modeê°€ ë°”ë€?ê²½ìš°ë¥??°ë¼?¡ìŒ.
	if (const AScenarioEditorController* controller = GetEditorController())
	{
		if (!bHasCachedViewMode || controller->GetEditorViewMode() != LastSeenViewMode)
		{
			RefreshViewModeButtons();
		}

		if (!bHasCachedPlacementSnapToGrid
			|| controller->IsPlacementSnapToGridEnabled() != bLastSeenPlacementSnapToGrid)
		{
			RefreshPlacementSnapButton();
		}
	}
}

UScenarioAssetPaletteWidget* UScenarioEditorRootWidget::ShowAssetPaletteWidget()
{
	if (!AssetPaletteWidget)
	{
		return nullptr;
	}

	AssetPaletteWidget->RebuildPalette();
	SetAssetPaletteVisible(true);
	return AssetPaletteWidget.Get();
}

void UScenarioEditorRootWidget::HideAssetPaletteWidget()
{
	SetAssetPaletteVisible(false);
}

UScenarioPlaceableDetailsWidget* UScenarioEditorRootWidget::ShowPlaceableDetails(
	UScenarioPlaceableComponent* selectedPlaceable)
{
	if (!PlaceableContextMenuWidget || !selectedPlaceable)
	{
		HidePlaceableDetails();
		return nullptr;
	}

	PlaceableContextMenuWidget->SetSelectedPlaceable(selectedPlaceable);
	SetPanelVisibility(ResolvePlaceableDetailsVisibilityTarget(), true);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), true);
	return PlaceableContextMenuWidget.Get();
}

void UScenarioEditorRootWidget::HidePlaceableDetails()
{
	if (PlaceableContextMenuWidget)
	{
		PlaceableContextMenuWidget->SetSelectedPlaceable(nullptr);
	}
	SetPanelVisibility(ResolvePlaceableDetailsVisibilityTarget(), false);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), false);
}

UScenarioPlaceableContextMenuWidget* UScenarioEditorRootWidget::ShowPlaceableContextMenu(
	UScenarioPlaceableComponent* selectedPlaceable)
{
	ShowPlaceableDetails(selectedPlaceable);
	return Cast<UScenarioPlaceableContextMenuWidget>(PlaceableContextMenuWidget.Get());
}

void UScenarioEditorRootWidget::HidePlaceableContextMenu()
{
	HidePlaceableDetails();
}

UScenarioPlaceableContextMenuWidget* UScenarioEditorRootWidget::GetPlaceableContextMenuWidget() const
{
	return Cast<UScenarioPlaceableContextMenuWidget>(PlaceableContextMenuWidget.Get());
}

void UScenarioEditorRootWidget::SetLlmPanelVisible(const bool bVisible)
{
	SetPanelVisibility(ResolveLlmPanelVisibilityTarget(), bVisible);
	SetPanelVisibility(ScenarioEditorLlmWidget.Get(), bVisible);
}

void UScenarioEditorRootWidget::SetTemplateSidebarPanel(const EScenarioTemplateSidebarPanel activePanel)
{
	ApplyTemplateSidebarPanel(activePanel);
	RefreshTemplateSidebarWidget();
}

void UScenarioEditorRootWidget::RefreshTemplateSidebarWidget()
{
	UScenarioEditorSidebarWidget* sidebarWidget = ResolveTemplateSidebarWidget();
	SetPanelVisibility(ResolveTemplateSidebarVisibilityTarget(), true);
	SetPanelVisibility(sidebarWidget, true);
	if (sidebarWidget)
	{
		sidebarWidget->RefreshFromDraft();
	}
}

void UScenarioEditorRootWidget::HandleEditorSessionStarted(const bool)
{
	RefreshTemplateSidebarWidget();

	if (bAutoRevealAssetPaletteOnBottomEdge)
	{
		SetAssetPaletteVisible(ShouldRevealAssetPaletteFromMouseEdge(), true);
		return;
	}

	if (bShowAssetPaletteOnEditorSessionStart)
	{
		ShowAssetPaletteWidget();
	}
}

void UScenarioEditorRootWidget::RefreshViewModeButtons()
{
	const AScenarioEditorController* controller = GetEditorController();
	if (!controller)
	{
		return;
	}

	const EScenarioEditorViewMode viewMode = controller->GetEditorViewMode();
	LastSeenViewMode = viewMode;
	bHasCachedViewMode = true;

	// ?„ìž¬ ëª¨ë“œê°€ ?„ë‹Œ, ?„í™˜ ?€??ëª¨ë“œ??ë²„íŠ¼ë§??¸ì¶œ??
	const bool bTopDownActive = viewMode == EScenarioEditorViewMode::TopDownOrtho;
	if (TopDownOrthoModeButton)
	{
		TopDownOrthoModeButton->SetVisibility(
			bTopDownActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (PerspectiveModeButton)
	{
		PerspectiveModeButton->SetVisibility(
			bTopDownActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UScenarioEditorRootWidget::RefreshPlacementSnapButton()
{
	const AScenarioEditorController* controller = GetEditorController();
	if (!controller)
	{
		return;
	}

	const bool bSnapEnabled = controller->IsPlacementSnapToGridEnabled();
	bLastSeenPlacementSnapToGrid = bSnapEnabled;
	bHasCachedPlacementSnapToGrid = true;

	if (SnapPlacementToGridButton)
	{
		SnapPlacementToGridButton->SetRenderOpacity(bSnapEnabled ? 1.0f : 0.55f);
		SnapPlacementToGridButton->SetToolTipText(
			bSnapEnabled
				? FText::FromString(TEXT("Disable placement grid snap"))
				: FText::FromString(TEXT("Enable placement grid snap")));
	}

	if (SnapPlacementToGridButtonText)
	{
		SnapPlacementToGridButtonText->SetText(
			bSnapEnabled
				? FText::FromString(TEXT("Snap On"))
				: FText::FromString(TEXT("Snap Off")));
	}
}

void UScenarioEditorRootWidget::HandleTopDownOrthoModeButtonClicked()
{
	if (AScenarioEditorController* controller = GetEditorController())
	{
		controller->SetEditorViewMode(EScenarioEditorViewMode::TopDownOrtho);
		RefreshViewModeButtons();
	}
}

void UScenarioEditorRootWidget::HandlePerspectiveModeButtonClicked()
{
	if (AScenarioEditorController* controller = GetEditorController())
	{
		controller->SetEditorViewMode(EScenarioEditorViewMode::Perspective);
		RefreshViewModeButtons();
	}
}

void UScenarioEditorRootWidget::HandleSnapPlacementToGridButtonClicked()
{
	if (AScenarioEditorController* controller = GetEditorController())
	{
		controller->TogglePlacementSnapToGrid();
		RefreshPlacementSnapButton();
	}
}

void UScenarioEditorRootWidget::HandleTemplateSidebarPanelChanged(
	const EScenarioTemplateSidebarPanel activePanel)
{
	SetTemplateSidebarPanel(activePanel);
}

void UScenarioEditorRootWidget::BindEditorModeButtons()
{
	if (TopDownOrthoModeButton)
	{
		TopDownOrthoModeButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorRootWidget::HandleTopDownOrthoModeButtonClicked);
		TopDownOrthoModeButton->OnClicked.AddDynamic(
			this, &UScenarioEditorRootWidget::HandleTopDownOrthoModeButtonClicked);
	}
	if (PerspectiveModeButton)
	{
		PerspectiveModeButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorRootWidget::HandlePerspectiveModeButtonClicked);
		PerspectiveModeButton->OnClicked.AddDynamic(
			this, &UScenarioEditorRootWidget::HandlePerspectiveModeButtonClicked);
	}
	if (SnapPlacementToGridButton)
	{
		SnapPlacementToGridButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorRootWidget::HandleSnapPlacementToGridButtonClicked);
		SnapPlacementToGridButton->OnClicked.AddDynamic(
			this, &UScenarioEditorRootWidget::HandleSnapPlacementToGridButtonClicked);
	}
}

void UScenarioEditorRootWidget::UnbindEditorModeButtons()
{
	if (TopDownOrthoModeButton)
	{
		TopDownOrthoModeButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorRootWidget::HandleTopDownOrthoModeButtonClicked);
	}
	if (PerspectiveModeButton)
	{
		PerspectiveModeButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorRootWidget::HandlePerspectiveModeButtonClicked);
	}
	if (SnapPlacementToGridButton)
	{
		SnapPlacementToGridButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorRootWidget::HandleSnapPlacementToGridButtonClicked);
	}
}

void UScenarioEditorRootWidget::BindTemplateSidebarToolbar()
{
	if (!ToolbarWidget)
	{
		return;
	}

	ToolbarWidget->OnSidebarPanelChanged.RemoveDynamic(
		this,
		&UScenarioEditorRootWidget::HandleTemplateSidebarPanelChanged);
	ToolbarWidget->OnSidebarPanelChanged.AddDynamic(
		this,
		&UScenarioEditorRootWidget::HandleTemplateSidebarPanelChanged);
	ApplyTemplateSidebarPanel(ToolbarWidget->GetActiveSidebarPanel());
}

void UScenarioEditorRootWidget::UnbindTemplateSidebarToolbar()
{
	if (ToolbarWidget)
	{
		ToolbarWidget->OnSidebarPanelChanged.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleTemplateSidebarPanelChanged);
	}
}

void UScenarioEditorRootWidget::ApplyTemplateSidebarPanel(const EScenarioTemplateSidebarPanel activePanel)
{
	UScenarioEditorSidebarWidget* sidebarWidget = ResolveTemplateSidebarWidget();
	if (sidebarWidget)
	{
		sidebarWidget->SetActivePanel(activePanel);
	}
}

AScenarioEditorController* UScenarioEditorRootWidget::GetEditorController() const
{
	return Cast<AScenarioEditorController>(GetOwningPlayer());
}

void UScenarioEditorRootWidget::BindEditorLaunchSubsystem()
{
	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	UScenarioEditorLaunchSubsystem* launchSubsystem = gameInstance
		? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>()
		: nullptr;
	if (!launchSubsystem)
	{
		return;
	}

	if (!AutoStartCompletedHandle.IsValid())
	{
		AutoStartCompletedHandle = launchSubsystem->OnAutoStartCompleted().AddUObject(
			this,
			&UScenarioEditorRootWidget::HandleAutoStartCompleted);
	}

	if (launchSubsystem->HasAutoStartedScenarioEditorSession())
	{
		HandleAutoStartCompleted(
			launchSubsystem->WasAutoStartedScenarioEditorSessionLoadedExistingScenario());
	}
}

void UScenarioEditorRootWidget::UnbindEditorLaunchSubsystem()
{
	if (!AutoStartCompletedHandle.IsValid())
	{
		return;
	}

	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	if (UScenarioEditorLaunchSubsystem* launchSubsystem = gameInstance
		? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>()
		: nullptr)
	{
		launchSubsystem->OnAutoStartCompleted().Remove(AutoStartCompletedHandle);
	}

	AutoStartCompletedHandle.Reset();
}

void UScenarioEditorRootWidget::HandleAutoStartCompleted(const bool bLoadedExistingScenario)
{
	HandleEditorSessionStarted(bLoadedExistingScenario);
}

UWidget* UScenarioEditorRootWidget::ResolvePlaceableDetailsVisibilityTarget() const
{
	return PlaceableContextMenuPanel ? PlaceableContextMenuPanel.Get() : Cast<UWidget>(PlaceableContextMenuWidget.Get());
}

UScenarioEditorSidebarWidget* UScenarioEditorRootWidget::ResolveTemplateSidebarWidget() const
{
	if (ScenarioEditorSidebarWidget)
	{
		return ScenarioEditorSidebarWidget.Get();
	}
	if (SidebarWidget)
	{
		return SidebarWidget.Get();
	}
	return ScenarioTemplateSidebarWidget.Get();
}

UWidget* UScenarioEditorRootWidget::ResolveTemplateSidebarVisibilityTarget() const
{
	return TemplateSidebarPanel ? TemplateSidebarPanel.Get() : Cast<UWidget>(ResolveTemplateSidebarWidget());
}

UWidget* UScenarioEditorRootWidget::ResolveAssetPaletteVisibilityTarget() const
{
	return AssetPalettePanel ? AssetPalettePanel.Get() : Cast<UWidget>(AssetPaletteWidget.Get());
}

UWidget* UScenarioEditorRootWidget::ResolveLlmPanelVisibilityTarget() const
{
	return LlmPanel ? LlmPanel.Get() : Cast<UWidget>(ScenarioEditorLlmWidget.Get());
}

void UScenarioEditorRootWidget::SetAssetPaletteVisible(const bool bVisible, const bool bRebuildWhenShowing)
{
	const UWidget* visibilityTarget = ResolveAssetPaletteVisibilityTarget();
	const bool bWasVisible = visibilityTarget && visibilityTarget->GetVisibility() != ESlateVisibility::Collapsed;
	if (bVisible && bRebuildWhenShowing && !bWasVisible && AssetPaletteWidget)
	{
		AssetPaletteWidget->RebuildPalette();
	}

	SetPanelVisibility(ResolveAssetPaletteVisibilityTarget(), bVisible);
	SetPanelVisibility(AssetPaletteWidget.Get(), bVisible);
}

void UScenarioEditorRootWidget::SetPanelVisibility(UWidget* targetWidget, const bool bVisible) const
{
	if (targetWidget)
	{
		targetWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UScenarioEditorRootWidget::ShouldRevealAssetPaletteFromMouseEdge() const
{
	const APlayerController* owningPlayer = GetOwningPlayer();
	if (!owningPlayer)
	{
		return false;
	}

	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (!owningPlayer->GetMousePosition(mouseX, mouseY))
	{
		return false;
	}
	(void)mouseX;

	int32 viewportSizeX = 0;
	int32 viewportSizeY = 0;
	owningPlayer->GetViewportSize(viewportSizeX, viewportSizeY);
	(void)viewportSizeX;
	if (viewportSizeY <= 0)
	{
		return false;
	}

	const UWidget* paletteVisibilityTarget = ResolveAssetPaletteVisibilityTarget();
	const bool bPanelVisible = paletteVisibilityTarget && paletteVisibilityTarget->GetVisibility() != ESlateVisibility::Collapsed;
	if (bPanelVisible && IsMouseOverWidget(paletteVisibilityTarget))
	{
		return true;
	}

	const float hideThreshold = FMath::Max(AssetPaletteRevealBottomEdgePixels, AssetPaletteHideBottomEdgePixels);
	const float threshold = bPanelVisible ? hideThreshold : AssetPaletteRevealBottomEdgePixels;
	return (static_cast<float>(viewportSizeY) - mouseY) <= threshold;
}

bool UScenarioEditorRootWidget::ShouldRevealLlmPanelFromMouseEdge() const
{
	const APlayerController* owningPlayer = GetOwningPlayer();
	if (!owningPlayer)
	{
		return false;
	}

	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (!owningPlayer->GetMousePosition(mouseX, mouseY))
	{
		return false;
	}

	int32 viewportSizeX = 0;
	int32 viewportSizeY = 0;
	owningPlayer->GetViewportSize(viewportSizeX, viewportSizeY);
	if (viewportSizeX <= 0)
	{
		return false;
	}

	const UWidget* llmVisibilityTarget = ResolveLlmPanelVisibilityTarget();
	const bool bPanelVisible = llmVisibilityTarget && llmVisibilityTarget->GetVisibility() != ESlateVisibility::Collapsed;
	if (bPanelVisible && IsMouseOverWidget(llmVisibilityTarget))
	{
		return true;
	}

	const float hideThreshold = FMath::Max(LlmPanelRevealRightEdgePixels, LlmPanelHideRightEdgePixels);
	const float threshold = bPanelVisible ? hideThreshold : LlmPanelRevealRightEdgePixels;
	return (static_cast<float>(viewportSizeX) - mouseX) <= threshold;
}

bool UScenarioEditorRootWidget::IsMouseOverWidget(const UWidget* targetWidget) const
{
	if (!targetWidget || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	return targetWidget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}
