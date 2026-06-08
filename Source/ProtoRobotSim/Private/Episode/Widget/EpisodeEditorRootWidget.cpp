#include "Episode/Widget/EpisodeEditorRootWidget.h"

#include "Components/Widget.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/Llm/EpisodeLlmPromptWidget.h"
#include "Episode/Widget/EpisodeAssetPaletteWidget.h"
#include "Episode/Widget/EpisodeEditorToolbarWidget.h"
#include "Episode/Widget/EpisodePlaceableContextMenuWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Platform/EpisodeEditorLaunchSubsystem.h"

void UEpisodeEditorRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HidePlaceableContextMenu();
	HideAssetPaletteWidget();
	SetLlmPanelVisible(false);
	BindEditorLaunchSubsystem();
}

void UEpisodeEditorRootWidget::NativeDestruct()
{
	UnbindEditorLaunchSubsystem();
	Super::NativeDestruct();
}

void UEpisodeEditorRootWidget::NativeTick(const FGeometry& myGeometry, const float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);

	if (bAutoRevealLlmPanelOnRightEdge)
	{
		SetLlmPanelVisible(ShouldRevealLlmPanelFromMouseEdge());
	}
}

UEpisodeAssetPaletteWidget* UEpisodeEditorRootWidget::ShowAssetPaletteWidget()
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

void UEpisodeEditorRootWidget::HideAssetPaletteWidget()
{
	SetPanelVisibility(ResolveAssetPaletteVisibilityTarget(), false);
	SetPanelVisibility(AssetPaletteWidget.Get(), false);
}

UEpisodePlaceableContextMenuWidget* UEpisodeEditorRootWidget::ShowPlaceableContextMenu(
	UEpisodePlaceableComponent* selectedPlaceable)
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

void UEpisodeEditorRootWidget::HidePlaceableContextMenu()
{
	if (PlaceableContextMenuWidget)
	{
		PlaceableContextMenuWidget->SetSelectedPlaceable(nullptr);
	}
	SetPanelVisibility(ResolvePlaceableContextMenuVisibilityTarget(), false);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), false);
}

void UEpisodeEditorRootWidget::SetLlmPanelVisible(const bool bVisible)
{
	SetPanelVisibility(ResolveLlmPanelVisibilityTarget(), bVisible);
	SetPanelVisibility(EpisodeEditorLLMWidget.Get(), bVisible);
}

void UEpisodeEditorRootWidget::HandleEditorSessionStarted(const bool)
{
	if (bShowAssetPaletteOnEditorSessionStart)
	{
		ShowAssetPaletteWidget();
	}
}

void UEpisodeEditorRootWidget::BindEditorLaunchSubsystem()
{
	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	UEpisodeEditorLaunchSubsystem* launchSubsystem = gameInstance
		? gameInstance->GetSubsystem<UEpisodeEditorLaunchSubsystem>()
		: nullptr;
	if (!launchSubsystem)
	{
		return;
	}

	if (!AutoStartCompletedHandle.IsValid())
	{
		AutoStartCompletedHandle = launchSubsystem->OnAutoStartCompleted().AddUObject(
			this,
			&UEpisodeEditorRootWidget::HandleAutoStartCompleted);
	}

	if (launchSubsystem->HasAutoStartedEpisodeEditorSession())
	{
		HandleAutoStartCompleted(
			launchSubsystem->WasAutoStartedEpisodeEditorSessionLoadedExistingEpisode());
	}
}

void UEpisodeEditorRootWidget::UnbindEditorLaunchSubsystem()
{
	if (!AutoStartCompletedHandle.IsValid())
	{
		return;
	}

	UWorld* world = GetWorld();
	UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	if (UEpisodeEditorLaunchSubsystem* launchSubsystem = gameInstance
		? gameInstance->GetSubsystem<UEpisodeEditorLaunchSubsystem>()
		: nullptr)
	{
		launchSubsystem->OnAutoStartCompleted().Remove(AutoStartCompletedHandle);
	}

	AutoStartCompletedHandle.Reset();
}

void UEpisodeEditorRootWidget::HandleAutoStartCompleted(const bool bLoadedExistingEpisode)
{
	HandleEditorSessionStarted(bLoadedExistingEpisode);
}

UWidget* UEpisodeEditorRootWidget::ResolvePlaceableContextMenuVisibilityTarget() const
{
	return PlaceableContextMenuPanel ? PlaceableContextMenuPanel.Get() : Cast<UWidget>(PlaceableContextMenuWidget.Get());
}

UWidget* UEpisodeEditorRootWidget::ResolveAssetPaletteVisibilityTarget() const
{
	return AssetPalettePanel ? AssetPalettePanel.Get() : Cast<UWidget>(AssetPaletteWidget.Get());
}

UWidget* UEpisodeEditorRootWidget::ResolveLlmPanelVisibilityTarget() const
{
	return LlmPanel ? LlmPanel.Get() : Cast<UWidget>(EpisodeEditorLLMWidget.Get());
}

void UEpisodeEditorRootWidget::SetPanelVisibility(UWidget* targetWidget, const bool bVisible) const
{
	if (targetWidget)
	{
		targetWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UEpisodeEditorRootWidget::ShouldRevealLlmPanelFromMouseEdge() const
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

bool UEpisodeEditorRootWidget::IsMouseOverWidget(const UWidget* targetWidget) const
{
	if (!targetWidget || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	return targetWidget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}
