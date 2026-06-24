#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"

#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"

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
}

void UScenarioEditorShellViewModel::SelectInspectorTab(const EScenarioEditorInspectorTab tab)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveInspectorTab, tab);
	UE_MVVM_SET_PROPERTY_VALUE(bLlmPanelVisible, tab == EScenarioEditorInspectorTab::Llm);
}

void UScenarioEditorShellViewModel::SelectSidebarPanel(const EScenarioTemplateSidebarPanel panel)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveSidebarPanel, panel);
}

void UScenarioEditorShellViewModel::SetAssetPaletteVisible(const bool bVisible)
{
	UE_MVVM_SET_PROPERTY_VALUE(bAssetPaletteVisible, bVisible);
}

void UScenarioEditorShellViewModel::SetLlmPanelVisible(const bool bVisible)
{
	UE_MVVM_SET_PROPERTY_VALUE(bLlmPanelVisible, bVisible);
	if (bVisible)
	{
		UE_MVVM_SET_PROPERTY_VALUE(ActiveInspectorTab, EScenarioEditorInspectorTab::Llm);
	}
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
	if (AScenarioEditorController* controller = UiSubsystem ? UiSubsystem->ResolveEditorController() : nullptr)
	{
		controller->ClearSelectedPlaceable();
	}
	UE_MVVM_SET_PROPERTY_VALUE(SelectedPlaceableId, FString());
}
