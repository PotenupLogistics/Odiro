#include "Scenario/Widget/ScenarioEditorRootWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorToolbarViewModel.h"
#include "Scenario/ViewModel/ScenarioPlaceableDetailsViewModel.h"
#include "Scenario/Widget/ScenarioAssetPaletteWidget.h"
#include "Scenario/Widget/ScenarioEditorOutlinerWidget.h"
#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"
#include "Scenario/Widget/ScenarioPlaceableContextMenuWidget.h"
#include "Scenario/Widget/ScenarioPlaceableDetailsWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Formats a repeated sidebar block path with a zero-based item index.
	FString MakeScenarioEditorIndexedBlockPath(const TCHAR* listPath, const int32 itemIndex)
	{
		return itemIndex >= 0
			? FString::Printf(TEXT("%s[%d]"), listPath, itemIndex)
			: FString(listPath);
	}

	// Parses an editor-only corridor handle id suffix without expanding authoring subsystem API surface.
	bool TryParseScenarioEditorIndexedHandleId(
		const FString& handleId,
		const TCHAR* prefix,
		int32& outHandleIndex)
	{
		outHandleIndex = INDEX_NONE;
		const FString prefixText(prefix);
		if (!handleId.StartsWith(prefixText))
		{
			return false;
		}

		const FString indexText = handleId.RightChop(prefixText.Len());
		if (indexText.IsEmpty() || !indexText.IsNumeric())
		{
			return false;
		}

		outHandleIndex = FCString::Atoi(*indexText);
		return outHandleIndex >= 0;
	}

	// Finds the static obstacle placement block that corresponds to a placeable instance id.
	bool TryResolveStaticObstacleSidebarBlockPath(
		const FScenarioDocument& draftScenario,
		const FString& instanceId,
		FString& outBlockPath)
	{
		for (int32 placementIndex = 0; placementIndex < draftScenario.Obstacles.Placements.Num(); ++placementIndex)
		{
			if (draftScenario.Obstacles.Placements[placementIndex].PlacementId == instanceId)
			{
				outBlockPath = MakeScenarioEditorIndexedBlockPath(TEXT("root.obstacles.placements"), placementIndex);
				return true;
			}
		}

		outBlockPath = TEXT("root.obstacles.placements[]");
		return !instanceId.IsEmpty();
	}

	// Maps one selectable editor placeable to the sidebar panel and block that should receive focus.
	bool TryResolvePlaceableSidebarFocusTarget(
		const UScenarioPlaceableComponent* selectedPlaceable,
		const FScenarioDocument& draftScenario,
		EScenarioTemplateSidebarPanel& outPanel,
		FString& outBlockPath)
	{
		if (!selectedPlaceable)
		{
			return false;
		}

		if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotStartMarker)
		{
			outPanel = EScenarioTemplateSidebarPanel::Main;
			outBlockPath = TEXT("root.robot.start");
			return true;
		}

		if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotGoalMarker)
		{
			outPanel = EScenarioTemplateSidebarPanel::Main;
			outBlockPath = TEXT("root.robot.goal");
			return true;
		}

		int32 handleIndex = INDEX_NONE;
		if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle)
		{
			outPanel = EScenarioTemplateSidebarPanel::Corridor;
			outBlockPath = TryParseScenarioEditorIndexedHandleId(
				selectedPlaceable->InstanceId,
				TEXT("corridor_vertex_"),
				handleIndex)
				? MakeScenarioEditorIndexedBlockPath(TEXT("root.corridor.axis.points_m"), handleIndex)
				: FString(TEXT("root.corridor.axis.points_m[]"));
			return true;
		}

		if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle)
		{
			outPanel = EScenarioTemplateSidebarPanel::Corridor;
			outBlockPath = TryParseScenarioEditorIndexedHandleId(
				selectedPlaceable->InstanceId,
				TEXT("corridor_segment_"),
				handleIndex)
				? MakeScenarioEditorIndexedBlockPath(TEXT("root.corridor.segments"), handleIndex)
				: FString(TEXT("root.corridor.segments[]"));
			return true;
		}

		if (selectedPlaceable->Category == EScenarioActorCategory::StaticObstacle)
		{
			outPanel = EScenarioTemplateSidebarPanel::Obstacle;
			return TryResolveStaticObstacleSidebarBlockPath(
				draftScenario,
				selectedPlaceable->InstanceId,
				outBlockPath);
		}

		return false;
	}
}

void UScenarioEditorRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeViewModel();
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
	if (ShellViewModel)
	{
		ShellViewModel->RefreshFromController();
		if (!bHasCachedViewMode || ShellViewModel->GetViewMode() != LastSeenViewMode)
		{
			RefreshViewModeButtons();
		}

		if (!bHasCachedPlacementSnapToGrid
			|| ShellViewModel->IsPlacementSnapToGridEnabled() != bLastSeenPlacementSnapToGrid)
		{
			RefreshPlacementSnapButton();
		}
	}
}

int32 UScenarioEditorRootWidget::NativePaint(
	const FPaintArgs& args,
	const FGeometry& allottedGeometry,
	const FSlateRect& myCullingRect,
	FSlateWindowElementList& outDrawElements,
	const int32 layerId,
	const FWidgetStyle& inWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 overlayLayerId = PaintRobotRouteMarkerOverlays(allottedGeometry, outDrawElements, layerId);
	return Super::NativePaint(
		args,
		allottedGeometry,
		myCullingRect,
		outDrawElements,
		overlayLayerId + 1,
		inWidgetStyle,
		bParentEnabled);
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
	if (!selectedPlaceable)
	{
		HidePlaceableDetails();
		return nullptr;
	}

	if (AScenarioEditorController* controller = GetEditorController();
		controller
		&& controller->GetSelectedPlaceableComponent() != selectedPlaceable
		&& !selectedPlaceable->InstanceId.IsEmpty()
		&& controller->SelectPlaceableByInstanceId(selectedPlaceable->InstanceId))
	{
		return nullptr;
	}

	FocusSidebarForSelectedPlaceable(selectedPlaceable);
	return nullptr;
}

void UScenarioEditorRootWidget::HidePlaceableDetails()
{
	if (PlaceableContextMenuWidget)
	{
		PlaceableContextMenuWidget->SetSelectedPlaceable(nullptr);
	}
	SetPanelVisibility(ResolvePlaceableDetailsVisibilityTarget(), false);
	SetPanelVisibility(PlaceableContextMenuWidget.Get(), false);
	if (UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this))
	{
		if (UScenarioPlaceableDetailsViewModel* detailsViewModel = uiSubsystem->GetPlaceableDetailsViewModel())
		{
			detailsViewModel->ClearSelectedPlaceable();
		}
	}
}

UScenarioPlaceableContextMenuWidget* UScenarioEditorRootWidget::ShowPlaceableContextMenu(
	UScenarioPlaceableComponent* selectedPlaceable)
{
	ShowPlaceableDetails(selectedPlaceable);
	return nullptr;
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
	if (ShellViewModel)
	{
		ShellViewModel->SetLlmPanelVisible(bVisible);
	}
	SetPanelVisibility(ResolveLlmPanelVisibilityTarget(), bVisible);
	SetPanelVisibility(ScenarioEditorLlmWidget.Get(), bVisible);
}

void UScenarioEditorRootWidget::ShowInspectorTab(const EScenarioEditorInspectorTab tab)
{
	ActiveInspectorTab = tab;
	if (ShellViewModel)
	{
		ShellViewModel->SelectInspectorTab(tab);
	}
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

bool UScenarioEditorRootWidget::FocusSidebarForSelectedPlaceable(
	UScenarioPlaceableComponent* selectedPlaceable)
{
	if (!selectedPlaceable)
	{
		HidePlaceableDetails();
		return false;
	}

	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	const UScenarioAuthoringSubsystem* authoringSubsystem = uiSubsystem
		? uiSubsystem->ResolveAuthoringSubsystem()
		: nullptr;
	if (!uiSubsystem || !authoringSubsystem)
	{
		HidePlaceableDetails();
		return false;
	}

	EScenarioTemplateSidebarPanel targetPanel = EScenarioTemplateSidebarPanel::Main;
	FString targetBlockPath;
	if (!TryResolvePlaceableSidebarFocusTarget(
		selectedPlaceable,
		authoringSubsystem->GetDraftScenario(),
		targetPanel,
		targetBlockPath))
	{
		HidePlaceableDetails();
		return false;
	}

	HidePlaceableDetails();
	ShowInspectorTab(EScenarioEditorInspectorTab::Detail);
	SetPanelVisibility(ResolveTemplateSidebarVisibilityTarget(), true);
	if (UScenarioEditorSidebarWidget* sidebarWidget = ResolveTemplateSidebarWidget())
	{
		SetPanelVisibility(sidebarWidget, true);
	}
	if (ShellViewModel)
	{
		ShellViewModel->FocusPlaceableTemplateBlock(targetPanel, targetBlockPath, selectedPlaceable->InstanceId);
	}
	ApplyTemplateSidebarPanel(targetPanel);
	RefreshTemplateSidebarWidget();
	SyncOutlinerSelectionToPlaceable(selectedPlaceable);
	return true;
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
	if (ShellViewModel)
	{
		ShellViewModel->RefreshFromController();
	}
	if (!ShellViewModel)
	{
		return;
	}

	const EScenarioEditorViewMode viewMode = ShellViewModel->GetViewMode();
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
	if (ShellViewModel)
	{
		ShellViewModel->RefreshFromController();
	}
	if (!ShellViewModel)
	{
		return;
	}

	const bool bSnapEnabled = ShellViewModel->IsPlacementSnapToGridEnabled();
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
	if (ShellViewModel)
	{
		ShellViewModel->SetTopDownOrthoViewMode();
		RefreshViewModeButtons();
	}
}

void UScenarioEditorRootWidget::HandlePerspectiveModeButtonClicked()
{
	if (ShellViewModel)
	{
		ShellViewModel->SetPerspectiveViewMode();
		RefreshViewModeButtons();
	}
}

void UScenarioEditorRootWidget::HandleSnapPlacementToGridButtonClicked()
{
	if (ShellViewModel)
	{
		ShellViewModel->TogglePlacementSnapToGrid();
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
	if (ToolbarWidget)
	{
		const bool bSaved = ToolbarWidget->SaveScenario();
		if (const UScenarioEditorToolbarViewModel* toolbarViewModel = ToolbarWidget->GetToolbarViewModel())
		{
			SetSaveStatusText(toolbarViewModel->GetStatusText());
		}
		if (bSaved)
		{
			RefreshScenarioInspector();
		}
		return;
	}

	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	UScenarioEditorToolbarViewModel* toolbarViewModel = uiSubsystem ? uiSubsystem->GetToolbarViewModel() : nullptr;
	if (!toolbarViewModel)
	{
		SetSaveStatusText(TEXT("Save failed: ScenarioEditorToolbarViewModel unavailable."));
		return;
	}

	const bool bSaved = toolbarViewModel->SaveScenario();
	SetSaveStatusText(toolbarViewModel->GetStatusText());
	if (bSaved)
	{
		RefreshScenarioInspector();
	}
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
	if (item.ItemType == EScenarioEditorOutlinerItemType::Placeable)
	{
		bool bSelectedPlaceable = false;
		if (ShellViewModel)
		{
			bSelectedPlaceable = ShellViewModel->SelectPlaceable(item.InstanceId);
		}
		if (bSelectedPlaceable)
		{
			if (AScenarioEditorController* controller = GetEditorController())
			{
				FocusSidebarForSelectedPlaceable(controller->GetSelectedPlaceableComponent());
			}
		}
		return;
	}

	if (ShellViewModel)
	{
		ShellViewModel->SelectTemplatePanel(item.TemplatePanel);
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

void UScenarioEditorRootWidget::InitializeViewModel()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	ShellViewModel = uiSubsystem ? uiSubsystem->GetShellViewModel() : nullptr;
	if (ShellViewModel)
	{
		ShellViewModel->RefreshFromController();
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
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	if (!uiSubsystem)
	{
		return;
	}

	if (!AutoStartCompletedHandle.IsValid())
	{
		AutoStartCompletedHandle = uiSubsystem->OnEditorAutoStartCompleted().AddUObject(
			this,
			&UScenarioEditorRootWidget::HandleAutoStartCompleted);
	}

	if (uiSubsystem->HasAutoStartedScenarioEditorSession())
	{
		HandleAutoStartCompleted(
			uiSubsystem->WasAutoStartedScenarioEditorSessionLoadedExistingScenario());
	}
}

void UScenarioEditorRootWidget::UnbindEditorLaunchSubsystem()
{
	if (!AutoStartCompletedHandle.IsValid())
	{
		return;
	}

	if (UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this))
	{
		uiSubsystem->OnEditorAutoStartCompleted().Remove(AutoStartCompletedHandle);
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
	if (ShellViewModel)
	{
		ShellViewModel->RefreshFromController();
	}

	if (!ScenarioEditorOutlinerWidget)
	{
		return;
	}

	ScenarioEditorOutlinerWidget->SetSelectedItemKey(
		selectedInstanceId.IsEmpty()
			? UScenarioEditorOutlinerWidget::MakeTemplateItemKey(
				ShellViewModel ? ShellViewModel->GetActiveSidebarPanel() : EScenarioTemplateSidebarPanel::Main)
			: UScenarioEditorOutlinerWidget::MakePlaceableItemKey(selectedInstanceId));
}

void UScenarioEditorRootWidget::SetAssetPaletteVisible(const bool bVisible, const bool bRebuildWhenShowing)
{
	if (ShellViewModel)
	{
		ShellViewModel->SetAssetPaletteVisible(bVisible);
	}

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

int32 UScenarioEditorRootWidget::PaintRobotRouteMarkerOverlays(
	const FGeometry& allottedGeometry,
	FSlateWindowElementList& outDrawElements,
	const int32 layerId) const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem =
		GetWorld() ? GetWorld()->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
	if (!authoringSubsystem)
	{
		return layerId;
	}

	TArray<FScenarioEditorRouteMarkerOverlayItem> items;
	authoringSubsystem->GetRobotRouteMarkerOverlayItems(items);

	int32 maxLayerId = layerId;
	for (const FScenarioEditorRouteMarkerOverlayItem& item : items)
	{
		FVector2D localPosition = FVector2D::ZeroVector;
		if (!TryProjectRouteMarkerOverlayPosition(item.WorldLocation, allottedGeometry, localPosition))
		{
			continue;
		}

		maxLayerId = FMath::Max(
			maxLayerId,
			PaintRobotRouteMarkerOverlayItem(item, localPosition, allottedGeometry, outDrawElements, layerId));
	}
	return maxLayerId;
}

int32 UScenarioEditorRootWidget::PaintRobotRouteMarkerOverlayItem(
	const FScenarioEditorRouteMarkerOverlayItem& item,
	const FVector2D& localPosition,
	const FGeometry& allottedGeometry,
	FSlateWindowElementList& outDrawElements,
	const int32 layerId) const
{
	const FVector2D safeMarkerSize(
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.X),
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.Y));
	const FVector2D markerSize = safeMarkerSize;
	const FVector2D safeAnchor(
		FMath::Clamp(RobotRouteMarkerOverlayAnchor.X, 0.0, 1.0),
		FMath::Clamp(RobotRouteMarkerOverlayAnchor.Y, 0.0, 1.0));
	const FVector2D topLeft = localPosition - markerSize * safeAnchor;
	const FVector2D localSize = allottedGeometry.GetLocalSize();
	if (topLeft.X > localSize.X
		|| topLeft.Y > localSize.Y
		|| topLeft.X + markerSize.X < 0.0
		|| topLeft.Y + markerSize.Y < 0.0)
	{
		return layerId;
	}

	UTexture2D* markerTexture = item.Kind == EScenarioEditorRouteMarkerKind::Start
		? RobotStartMarkerOverlayTexture.Get()
		: RobotGoalMarkerOverlayTexture.Get();
	const FLinearColor baseTint = item.Kind == EScenarioEditorRouteMarkerKind::Start
		? RobotStartMarkerOverlayTint
		: RobotGoalMarkerOverlayTint;

	if (markerTexture)
	{
		FSlateBrush markerBrush;
		markerBrush.SetResourceObject(markerTexture);
		markerBrush.ImageSize = markerSize;
		markerBrush.DrawAs = ESlateBrushDrawType::Image;
		FSlateDrawElement::MakeBox(
			outDrawElements,
			layerId,
			allottedGeometry.ToPaintGeometry(markerSize, FSlateLayoutTransform(topLeft)),
			&markerBrush,
			ESlateDrawEffect::None,
			baseTint);
		return layerId + 1;
	}

	const FSlateBrush* whiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (whiteBrush)
	{
		const FVector2D headSize(markerSize.X * 0.74, markerSize.X * 0.74);
		const FVector2D headTopLeft(
			topLeft.X + (markerSize.X - headSize.X) * 0.5,
			topLeft.Y + markerSize.Y * 0.08);
		FSlateDrawElement::MakeBox(
			outDrawElements,
			layerId,
			allottedGeometry.ToPaintGeometry(headSize, FSlateLayoutTransform(headTopLeft)),
			whiteBrush,
			ESlateDrawEffect::None,
			baseTint);

		const FVector2D tip(topLeft.X + markerSize.X * 0.5, topLeft.Y + markerSize.Y);
		const FVector2D leftBase(topLeft.X + markerSize.X * 0.24, topLeft.Y + markerSize.Y * 0.42);
		const FVector2D rightBase(topLeft.X + markerSize.X * 0.76, topLeft.Y + markerSize.Y * 0.42);
		TArray<FVector2D> markerLines;
		markerLines.Reserve(4);
		markerLines.Add(leftBase);
		markerLines.Add(tip);
		markerLines.Add(rightBase);
		markerLines.Add(leftBase);
		FSlateDrawElement::MakeLines(
			outDrawElements,
			layerId + 1,
			allottedGeometry.ToPaintGeometry(),
			markerLines,
			ESlateDrawEffect::None,
			baseTint,
			true,
			3.0f);
		return layerId + 2;
	}

	return layerId;
}

bool UScenarioEditorRootWidget::TryProjectRouteMarkerOverlayPosition(
	const FVector& worldLocation,
	const FGeometry& allottedGeometry,
	FVector2D& outLocalPosition) const
{
	APlayerController* owningPlayer = GetOwningPlayer();
	if (!owningPlayer)
	{
		return false;
	}

	FVector2D widgetPosition = FVector2D::ZeroVector;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		owningPlayer,
		worldLocation,
		widgetPosition,
		true))
	{
		return false;
	}

	const FGeometry viewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D rootOriginInViewport = viewportGeometry.AbsoluteToLocal(allottedGeometry.GetAbsolutePosition());
	outLocalPosition = widgetPosition - rootOriginInViewport;
	const FVector2D localSize = allottedGeometry.GetLocalSize();
	const FVector2D markerSize(
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.X),
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.Y));
	return outLocalPosition.X >= -markerSize.X
		&& outLocalPosition.Y >= -markerSize.Y
		&& outLocalPosition.X <= localSize.X + markerSize.X
		&& outLocalPosition.Y <= localSize.Y + markerSize.Y;
}
