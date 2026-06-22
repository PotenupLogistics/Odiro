#include "Scenario/Widget/ScenarioEditorRootWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Scenario/Widget/ScenarioAssetPaletteWidget.h"
#include "Scenario/Widget/ScenarioEditorOutlinerWidget.h"
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
	SetPanelVisibility(ToolbarWidget.Get(), false);
	HidePlaceableDetails();
	HideAssetPaletteWidget();
	CacheInspectorTabButtonStyles();
	ShowInspectorTab(EScenarioEditorInspectorTab::Detail);
	BindEditorModeButtons();
	BindSidebarControls();
	SetTemplateSidebarPanel(EScenarioTemplateSidebarPanel::Main);
	RefreshScenarioInspector();
	RefreshViewModeButtons();
	RefreshPlacementSnapButton();
	BindEditorLaunchSubsystem();
	SetSaveStatusText(TEXT("Ready"));
}

void UScenarioEditorRootWidget::NativeDestruct()
{
	UnbindSidebarControls();
	UnbindEditorModeButtons();
	UnbindEditorLaunchSubsystem();
	Super::NativeDestruct();
}

void UScenarioEditorRootWidget::NativeTick(const FGeometry& myGeometry, const float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);

	if (bAutoRevealAssetPaletteOnBottomEdge)
	{
		SetAssetPaletteVisible(ShouldRevealAssetPaletteFromMouseEdge(), true);
	}

	// Polls external view-mode changes from keyboard shortcuts and controller-owned transitions.
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
	ShowInspectorTab(EScenarioEditorInspectorTab::Detail);
	SetPanelVisibility(ResolveTemplateSidebarVisibilityTarget(), false);
	SetPanelVisibility(ResolvePlaceableDetailsVisibilityTarget(), true);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), true);
	SyncOutlinerSelectionToPlaceable(selectedPlaceable);
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

void UScenarioEditorRootWidget::ShowInspectorTab(const EScenarioEditorInspectorTab tab)
{
	ActiveInspectorTab = tab;
	const bool bShowDetail = tab == EScenarioEditorInspectorTab::Detail;
	ApplyInspectorTabVisualState();

	if (InspectorSwitcher)
	{
		UWidget* activeWidget = nullptr;
		if (bShowDetail && DetailInspectorPanel)
		{
			activeWidget = DetailInspectorPanel.Get();
			InspectorSwitcher->SetActiveWidget(activeWidget);
		}
		else if (!bShowDetail && LlmInspectorPanel)
		{
			activeWidget = LlmInspectorPanel.Get();
			InspectorSwitcher->SetActiveWidget(activeWidget);
		}
		else
		{
			const int32 activeIndex = bShowDetail ? 0 : 1;
			InspectorSwitcher->SetActiveWidgetIndex(activeIndex);
			activeWidget = InspectorSwitcher->GetWidgetAtIndex(activeIndex);
		}
		if (activeWidget)
		{
			SetPanelVisibility(activeWidget, true);
		}
		return;
	}

	SetPanelVisibility(ResolveDetailInspectorVisibilityTarget(), bShowDetail);
	SetLlmPanelVisible(!bShowDetail);
}

void UScenarioEditorRootWidget::CacheInspectorTabButtonStyles()
{
	if (DetailInspectorTabButton && !bHasDetailInspectorInactiveTabButtonStyle)
	{
		DetailInspectorInactiveTabButtonStyle = DetailInspectorTabButton->GetStyle();
		bHasDetailInspectorInactiveTabButtonStyle = true;
	}
	if (LlmInspectorTabButton && !bHasLlmInspectorInactiveTabButtonStyle)
	{
		LlmInspectorInactiveTabButtonStyle = LlmInspectorTabButton->GetStyle();
		bHasLlmInspectorInactiveTabButtonStyle = true;
	}
}

void UScenarioEditorRootWidget::ApplyInspectorTabVisualState()
{
	const FButtonStyle* activeStyleSource = InspectorActiveTabButtonStyleSource
		? &InspectorActiveTabButtonStyleSource->GetStyle()
		: nullptr;

	auto applyStyle = [activeStyleSource](
		UButton* button,
		const bool bIsActive,
		const FButtonStyle& inactiveStyle,
		const bool bHasInactiveStyle)
	{
		if (!button)
		{
			return;
		}

		if (bIsActive && activeStyleSource)
		{
			FButtonStyle activeStyle = *activeStyleSource;
			if (bHasInactiveStyle)
			{
				activeStyle.SetNormalPadding(inactiveStyle.NormalPadding);
				activeStyle.SetPressedPadding(inactiveStyle.PressedPadding);
			}
			button->SetStyle(activeStyle);
			return;
		}

		if (bHasInactiveStyle)
		{
			button->SetStyle(inactiveStyle);
		}
	};

	applyStyle(
		DetailInspectorTabButton.Get(),
		ActiveInspectorTab == EScenarioEditorInspectorTab::Detail,
		DetailInspectorInactiveTabButtonStyle,
		bHasDetailInspectorInactiveTabButtonStyle);
	applyStyle(
		LlmInspectorTabButton.Get(),
		ActiveInspectorTab == EScenarioEditorInspectorTab::Llm,
		LlmInspectorInactiveTabButtonStyle,
		bHasLlmInspectorInactiveTabButtonStyle);
}

void UScenarioEditorRootWidget::SetTemplateSidebarPanel(
	const EScenarioTemplateSidebarPanel activePanel,
	const bool bSyncOutlinerSelection)
{
	UScenarioEditorSidebarWidget* sidebarWidget = ResolveTemplateSidebarWidget();
	HidePlaceableDetails();
	ShowInspectorTab(EScenarioEditorInspectorTab::Detail);
	SetPanelVisibility(ResolveTemplateSidebarVisibilityTarget(), true);
	SetPanelVisibility(sidebarWidget, true);
	ApplyTemplateSidebarPanel(activePanel);
	if (bSyncOutlinerSelection && ScenarioEditorOutlinerWidget)
	{
		ScenarioEditorOutlinerWidget->SetSelectedItemKey(
			UScenarioEditorOutlinerWidget::MakeTemplateItemKey(activePanel));
	}
}

void UScenarioEditorRootWidget::RefreshTemplateSidebarWidget()
{
	UScenarioEditorSidebarWidget* sidebarWidget = ResolveTemplateSidebarWidget();
	if (sidebarWidget)
	{
		sidebarWidget->RefreshFromDraft();
	}
}

void UScenarioEditorRootWidget::RefreshScenarioInspector()
{
	RefreshTemplateSidebarWidget();
	if (ScenarioEditorOutlinerWidget)
	{
		ScenarioEditorOutlinerWidget->RefreshFromEditorState();
	}
}

void UScenarioEditorRootWidget::HandleEditorSessionStarted(const bool)
{
	RefreshScenarioInspector();

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

	// Only show the button for the view mode that is not currently active.
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

void UScenarioEditorRootWidget::HandleSaveButtonClicked()
{
	AScenarioEditorController* controller = GetEditorController();
	if (!controller)
	{
		SetSaveStatusText(TEXT("Save failed: ScenarioEditorController unavailable."));
		return;
	}

	FString resolvedPath;
	TArray<FString> diagnostics;
	if (!controller->SaveCurrentScenarioDraft(resolvedPath, diagnostics))
	{
		SetSaveStatusText(diagnostics.IsEmpty()
			? TEXT("Save failed.")
			: FString::Join(diagnostics, TEXT("\n")));
		return;
	}

	SetSaveStatusText(FString::Printf(TEXT("Saved: %s"), *resolvedPath));
	RefreshScenarioInspector();
}

void UScenarioEditorRootWidget::HandleDetailInspectorTabClicked()
{
	ShowInspectorTab(EScenarioEditorInspectorTab::Detail);
}

void UScenarioEditorRootWidget::HandleLlmInspectorTabClicked()
{
	ShowInspectorTab(EScenarioEditorInspectorTab::Llm);
}

void UScenarioEditorRootWidget::HandleOutlinerItemSelected(FScenarioOutlinerItemViewModel item)
{
	AScenarioEditorController* controller = GetEditorController();
	if (item.ItemType == EScenarioEditorOutlinerItemType::Placeable)
	{
		if (controller)
		{
			controller->SelectPlaceableByInstanceId(item.InstanceId);
		}
		return;
	}

	if (controller)
	{
		controller->ClearSelectedPlaceable();
	}
	SetTemplateSidebarPanel(item.TemplatePanel, false);
	if (ScenarioEditorOutlinerWidget)
	{
		ScenarioEditorOutlinerWidget->SetSelectedItemKey(item.ItemKey);
	}
}

void UScenarioEditorRootWidget::BindSidebarControls()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UScenarioEditorRootWidget::HandleSaveButtonClicked);
		SaveButton->OnClicked.AddDynamic(this, &UScenarioEditorRootWidget::HandleSaveButtonClicked);
	}
	if (DetailInspectorTabButton)
	{
		DetailInspectorTabButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleDetailInspectorTabClicked);
		DetailInspectorTabButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorRootWidget::HandleDetailInspectorTabClicked);
	}
	if (LlmInspectorTabButton)
	{
		LlmInspectorTabButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleLlmInspectorTabClicked);
		LlmInspectorTabButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorRootWidget::HandleLlmInspectorTabClicked);
	}
	if (ScenarioEditorOutlinerWidget)
	{
		ScenarioEditorOutlinerWidget->OnItemSelected.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleOutlinerItemSelected);
		ScenarioEditorOutlinerWidget->OnItemSelected.AddDynamic(
			this,
			&UScenarioEditorRootWidget::HandleOutlinerItemSelected);
	}
	if (AScenarioEditorController* controller = GetEditorController())
	{
		controller->OnSelectedPlaceableChanged().RemoveAll(this);
		controller->OnSelectedPlaceableChanged().AddUObject(
			this,
			&UScenarioEditorRootWidget::HandleControllerSelectedPlaceableChanged);
	}
}

void UScenarioEditorRootWidget::UnbindSidebarControls()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UScenarioEditorRootWidget::HandleSaveButtonClicked);
	}
	if (DetailInspectorTabButton)
	{
		DetailInspectorTabButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleDetailInspectorTabClicked);
	}
	if (LlmInspectorTabButton)
	{
		LlmInspectorTabButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleLlmInspectorTabClicked);
	}
	if (ScenarioEditorOutlinerWidget)
	{
		ScenarioEditorOutlinerWidget->OnItemSelected.RemoveDynamic(
			this,
			&UScenarioEditorRootWidget::HandleOutlinerItemSelected);
	}
	if (AScenarioEditorController* controller = GetEditorController())
	{
		controller->OnSelectedPlaceableChanged().RemoveAll(this);
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
	if (LlmInspectorPanel)
	{
		return LlmInspectorPanel.Get();
	}
	return LlmPanel ? LlmPanel.Get() : Cast<UWidget>(ScenarioEditorLlmWidget.Get());
}

UWidget* UScenarioEditorRootWidget::ResolveDetailInspectorVisibilityTarget() const
{
	if (DetailInspectorPanel)
	{
		return DetailInspectorPanel.Get();
	}
	return ResolveTemplateSidebarVisibilityTarget();
}

void UScenarioEditorRootWidget::SetSaveStatusText(const FString& message) const
{
	if (SaveStatusText)
	{
		SaveStatusText->SetText(FText::FromString(message));
	}
}

void UScenarioEditorRootWidget::SyncOutlinerSelectionToPlaceable(
	const UScenarioPlaceableComponent* selectedPlaceable) const
{
	if (!ScenarioEditorOutlinerWidget)
	{
		return;
	}

	const FString itemKey = selectedPlaceable && !selectedPlaceable->InstanceId.IsEmpty()
		? UScenarioEditorOutlinerWidget::MakePlaceableItemKey(selectedPlaceable->InstanceId)
		: UScenarioEditorOutlinerWidget::MakeTemplateItemKey(EScenarioTemplateSidebarPanel::Main);
	ScenarioEditorOutlinerWidget->SetSelectedItemKey(itemKey);
}

void UScenarioEditorRootWidget::HandleControllerSelectedPlaceableChanged(
	const FString& selectedInstanceId)
{
	if (!ScenarioEditorOutlinerWidget)
	{
		return;
	}

	ScenarioEditorOutlinerWidget->SetSelectedItemKey(
		selectedInstanceId.IsEmpty()
			? UScenarioEditorOutlinerWidget::MakeTemplateItemKey(EScenarioTemplateSidebarPanel::Main)
			: UScenarioEditorOutlinerWidget::MakePlaceableItemKey(selectedInstanceId));
}

void UScenarioEditorRootWidget::SetAssetPaletteVisible(const bool bVisible, const bool bRebuildWhenShowing)
{
	UWidget* visibilityTarget = ResolveAssetPaletteVisibilityTarget();
	const bool bWasVisible = visibilityTarget && visibilityTarget->GetVisibility() != ESlateVisibility::Collapsed;
	if (bVisible && bRebuildWhenShowing && !bWasVisible && AssetPaletteWidget)
	{
		AssetPaletteWidget->RebuildPalette();
	}

	SetPanelVisibility(visibilityTarget, bVisible);
	if (visibilityTarget != AssetPaletteWidget.Get())
	{
		SetPanelVisibility(AssetPaletteWidget.Get(), bVisible);
	}
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

bool UScenarioEditorRootWidget::IsMouseOverWidget(const UWidget* targetWidget) const
{
	if (!targetWidget || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	return targetWidget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}
