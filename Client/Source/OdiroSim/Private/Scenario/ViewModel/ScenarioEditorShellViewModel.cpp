#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"

#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"

namespace
{
	// Returns the root template block path for a sidebar panel.
	FString ResolveDefaultTemplateBlockPath(const EScenarioTemplateSidebarPanel panel)
	{
		switch (panel)
		{
		case EScenarioTemplateSidebarPanel::Corridor:
			return TEXT("root.corridor");
		case EScenarioTemplateSidebarPanel::Obstacle:
			return TEXT("root.obstacles");
		case EScenarioTemplateSidebarPanel::Pedestrian:
			return TEXT("root.pedestrians");
		case EScenarioTemplateSidebarPanel::Main:
		default:
			return TEXT("scenario");
		}
	}

	// Uses a panel root path when a child block path is not available.
	FString ResolveTemplateBlockPath(
		const EScenarioTemplateSidebarPanel panel,
		const FString& blockPath)
	{
		return blockPath.IsEmpty() ? ResolveDefaultTemplateBlockPath(panel) : blockPath;
	}
}

void UScenarioEditorShellViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
	RefreshFromController();
}

void UScenarioEditorShellViewModel::RefreshFromController()
{
	const AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr;
	if (!controller)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(ViewMode, controller->GetEditorViewMode());
	UE_MVVM_SET_PROPERTY_VALUE(bPlacementSnapToGridEnabled, controller->IsPlacementSnapToGridEnabled());

	const UScenarioPlaceableComponent* selectedPlaceable = controller->GetSelectedPlaceableComponent();
	UE_MVVM_SET_PROPERTY_VALUE(
		SelectedPlaceableId,
		selectedPlaceable ? selectedPlaceable->InstanceId : FString());
	if (selectedPlaceable)
	{
		if (SelectedTemplateBlockPath.IsEmpty())
		{
			UE_MVVM_SET_PROPERTY_VALUE(SelectedTemplateBlockPath, ResolveDefaultTemplateBlockPath(ActiveSidebarPanel));
		}
	}
	else if (SelectedTemplateBlockPath.IsEmpty())
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedTemplateBlockPath, ResolveDefaultTemplateBlockPath(ActiveSidebarPanel));
	}
}

void UScenarioEditorShellViewModel::SelectInspectorTab(const EScenarioEditorInspectorTab tab)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveInspectorTab, tab);
	UE_MVVM_SET_PROPERTY_VALUE(bLlmPanelVisible, true);
}

void UScenarioEditorShellViewModel::SelectSidebarPanel(const EScenarioTemplateSidebarPanel panel)
{
	SelectTemplatePanel(panel);
}

void UScenarioEditorShellViewModel::SelectTemplatePanel(const EScenarioTemplateSidebarPanel panel)
{
	SelectTemplateBlock(panel, ResolveDefaultTemplateBlockPath(panel));
}

void UScenarioEditorShellViewModel::SelectTemplateBlock(
	const EScenarioTemplateSidebarPanel panel,
	const FString& blockPath)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveSidebarPanel, panel);
	UE_MVVM_SET_PROPERTY_VALUE(SelectedPlaceableId, FString());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedTemplateBlockPath, ResolveTemplateBlockPath(panel, blockPath));

	if (AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr;
		controller && controller->GetSelectedPlaceableComponent())
	{
		controller->ClearSelectedPlaceable(false);
	}
}

void UScenarioEditorShellViewModel::FocusPlaceableTemplateBlock(
	const EScenarioTemplateSidebarPanel panel,
	const FString& blockPath,
	const FString& placeableId)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveSidebarPanel, panel);
	UE_MVVM_SET_PROPERTY_VALUE(SelectedPlaceableId, placeableId);
	UE_MVVM_SET_PROPERTY_VALUE(SelectedTemplateBlockPath, ResolveTemplateBlockPath(panel, blockPath));
}

void UScenarioEditorShellViewModel::SetAssetPaletteVisible(const bool bVisible)
{
	UE_MVVM_SET_PROPERTY_VALUE(bAssetPaletteVisible, bVisible);
}

void UScenarioEditorShellViewModel::SetLlmPanelVisible(const bool)
{
	UE_MVVM_SET_PROPERTY_VALUE(bLlmPanelVisible, true);
}

bool UScenarioEditorShellViewModel::SetEditorViewMode(const EScenarioEditorViewMode viewMode)
{
	AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr;
	if (!controller)
	{
		return false;
	}

	controller->SetEditorViewMode(viewMode);
	UE_MVVM_SET_PROPERTY_VALUE(ViewMode, viewMode);
	return true;
}

bool UScenarioEditorShellViewModel::SetTopDownOrthoViewMode()
{
	return SetEditorViewMode(EScenarioEditorViewMode::TopDownOrtho);
}

bool UScenarioEditorShellViewModel::SetPerspectiveViewMode()
{
	return SetEditorViewMode(EScenarioEditorViewMode::Perspective);
}

bool UScenarioEditorShellViewModel::SetPlacementSnapToGridEnabled(const bool bEnabled)
{
	AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr;
	if (!controller)
	{
		return false;
	}

	controller->SetPlacementSnapToGridEnabled(bEnabled);
	UE_MVVM_SET_PROPERTY_VALUE(bPlacementSnapToGridEnabled, bEnabled);
	return true;
}

bool UScenarioEditorShellViewModel::TogglePlacementSnapToGrid()
{
	return SetPlacementSnapToGridEnabled(!bPlacementSnapToGridEnabled);
}

bool UScenarioEditorShellViewModel::SelectPlaceable(const FString& instanceId)
{
	AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr;
	if (!controller)
	{
		return false;
	}

	const bool bSelected = controller->SelectPlaceableByInstanceId(instanceId);
	if (bSelected)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedPlaceableId, instanceId);
	}
	return bSelected;
}

void UScenarioEditorShellViewModel::ClearSelectedPlaceable()
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedPlaceableId, FString());
	if (SelectedTemplateBlockPath.IsEmpty())
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedTemplateBlockPath, ResolveDefaultTemplateBlockPath(ActiveSidebarPanel));
	}

	if (AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr;
		controller && controller->GetSelectedPlaceableComponent())
	{
		controller->ClearSelectedPlaceable();
	}
}

void UScenarioEditorShellViewModel::ClearSelection()
{
	ClearSelectedPlaceable();
	UE_MVVM_SET_PROPERTY_VALUE(SelectedTemplateBlockPath, ResolveDefaultTemplateBlockPath(ActiveSidebarPanel));
}
