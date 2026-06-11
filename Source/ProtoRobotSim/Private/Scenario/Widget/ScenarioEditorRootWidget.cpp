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
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"

void UScenarioEditorRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HidePlaceableContextMenu();
	HideAssetPaletteWidget();
	SetLlmPanelVisible(false);
	BindEditorModeButtons();
	RefreshViewModeButtons();
	RefreshPlacementSnapButton();
	BindEditorLaunchSubsystem();
}

void UScenarioEditorRootWidget::NativeDestruct()
{
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

	// keyboard toggle 등 버튼 외 경로로 view mode가 바뀐 경우를 따라잡음.
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
	SetPanelVisibility(ResolveAssetPaletteVisibilityTarget(), true);
	SetPanelVisibility(AssetPaletteWidget.Get(), true);
	return AssetPaletteWidget.Get();
}

void UScenarioEditorRootWidget::HideAssetPaletteWidget()
{
	SetPanelVisibility(ResolveAssetPaletteVisibilityTarget(), false);
	SetPanelVisibility(AssetPaletteWidget.Get(), false);
}

UScenarioPlaceableContextMenuWidget* UScenarioEditorRootWidget::ShowPlaceableContextMenu(
	UScenarioPlaceableComponent* selectedPlaceable)
{
	if (!PlaceableContextMenuWidget || !selectedPlaceable)
	{
		HidePlaceableContextMenu();
		return nullptr;
	}

	PlaceableContextMenuWidget->SetSelectedPlaceable(selectedPlaceable);
	SetPanelVisibility(ResolvePlaceableContextMenuVisibilityTarget(), true);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), true);
	return PlaceableContextMenuWidget.Get();
}

void UScenarioEditorRootWidget::HidePlaceableContextMenu()
{
	if (PlaceableContextMenuWidget)
	{
		PlaceableContextMenuWidget->SetSelectedPlaceable(nullptr);
	}
	SetPanelVisibility(ResolvePlaceableContextMenuVisibilityTarget(), false);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), false);
}

void UScenarioEditorRootWidget::SetLlmPanelVisible(const bool bVisible)
{
	SetPanelVisibility(ResolveLlmPanelVisibilityTarget(), bVisible);
	SetPanelVisibility(EpisodeEditorLLMWidget.Get(), bVisible);
}

void UScenarioEditorRootWidget::HandleEditorSessionStarted(const bool)
{
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

	// 현재 모드가 아닌, 전환 대상 모드의 버튼만 노출함.
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

	if (launchSubsystem->HasAutoStartedEpisodeEditorSession())
	{
		HandleAutoStartCompleted(
			launchSubsystem->WasAutoStartedEpisodeEditorSessionLoadedExistingEpisode());
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

void UScenarioEditorRootWidget::HandleAutoStartCompleted(const bool bLoadedExistingEpisode)
{
	HandleEditorSessionStarted(bLoadedExistingEpisode);
}

UWidget* UScenarioEditorRootWidget::ResolvePlaceableContextMenuVisibilityTarget() const
{
	return PlaceableContextMenuPanel ? PlaceableContextMenuPanel.Get() : Cast<UWidget>(PlaceableContextMenuWidget.Get());
}

UWidget* UScenarioEditorRootWidget::ResolveAssetPaletteVisibilityTarget() const
{
	return AssetPalettePanel ? AssetPalettePanel.Get() : Cast<UWidget>(AssetPaletteWidget.Get());
}

UWidget* UScenarioEditorRootWidget::ResolveLlmPanelVisibilityTarget() const
{
	return LlmPanel ? LlmPanel.Get() : Cast<UWidget>(EpisodeEditorLLMWidget.Get());
}

void UScenarioEditorRootWidget::SetPanelVisibility(UWidget* targetWidget, const bool bVisible) const
{
	if (targetWidget)
	{
		targetWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
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
