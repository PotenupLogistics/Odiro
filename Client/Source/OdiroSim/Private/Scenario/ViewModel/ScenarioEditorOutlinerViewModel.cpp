#include "Scenario/ViewModel/ScenarioEditorOutlinerViewModel.h"

#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"

void UScenarioEditorOutlinerViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
	RefreshTemplatePanels();
}

void UScenarioEditorOutlinerViewModel::RefreshTemplatePanels()
{
	Items.Reset();
	Items.Add(CreateTemplatePanelItem(TEXT("Template:Main"), TEXT("Scenario"), EScenarioTemplateSidebarPanel::Main, 0));
	Items.Add(CreateTemplatePanelItem(TEXT("Template:Corridor"), TEXT("Corridor"), EScenarioTemplateSidebarPanel::Corridor, 1));
	Items.Add(CreateTemplatePanelItem(TEXT("Template:Obstacle"), TEXT("Obstacles"), EScenarioTemplateSidebarPanel::Obstacle, 1));
	Items.Add(CreateTemplatePanelItem(TEXT("Template:Pedestrian"), TEXT("Pedestrians"), EScenarioTemplateSidebarPanel::Pedestrian, 1));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);

	if (SelectedItemKey.IsEmpty())
	{
		SetSelectedItemKey(TEXT("Template:Main"));
	}
}

void UScenarioEditorOutlinerViewModel::SetItemsFromOutlinerRows(
	const TArray<FScenarioOutlinerItemViewModel>& rowViewModels)
{
	Items.Reset();
	Items.Reserve(rowViewModels.Num());

	FString selectedItemKey;
	for (const FScenarioOutlinerItemViewModel& rowViewModel : rowViewModels)
	{
		UScenarioEditorListItemViewModel* item = NewObject<UScenarioEditorListItemViewModel>(this);
		if (!item)
		{
			continue;
		}

		item->InitializeFromOutlinerRow(rowViewModel);
		if (rowViewModel.bSelected)
		{
			selectedItemKey = rowViewModel.ItemKey;
		}
		Items.Add(item);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);
	if (!selectedItemKey.IsEmpty())
	{
		UE_MVVM_SET_PROPERTY_VALUE(SelectedItemKey, selectedItemKey);
	}
}

bool UScenarioEditorOutlinerViewModel::SelectItem(UScenarioEditorListItemViewModel* itemViewModel)
{
	if (!itemViewModel)
	{
		return false;
	}

	SetSelectedItemKey(itemViewModel->GetItemId());
	if (!UiSubsystem)
	{
		return true;
	}

	UScenarioEditorShellViewModel* shellViewModel = UiSubsystem->GetShellViewModel();
	if (!shellViewModel)
	{
		return true;
	}

	if (itemViewModel->GetOutlinerItemType() == EScenarioEditorOutlinerItemType::Placeable)
	{
		return shellViewModel->SelectPlaceable(itemViewModel->GetInstanceId());
	}

	shellViewModel->ClearSelectedPlaceable();
	shellViewModel->SelectSidebarPanel(itemViewModel->GetTemplatePanel());
	return true;
}

void UScenarioEditorOutlinerViewModel::SetSelectedItemKey(const FString& itemKey)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedItemKey, itemKey);
	for (UScenarioEditorListItemViewModel* item : Items)
	{
		if (item)
		{
			item->SetSelected(item->GetItemId() == itemKey);
		}
	}
}

TArray<UScenarioEditorListItemViewModel*> UScenarioEditorOutlinerViewModel::GetItems() const
{
	TArray<UScenarioEditorListItemViewModel*> result;
	result.Reserve(Items.Num());
	for (UScenarioEditorListItemViewModel* item : Items)
	{
		result.Add(item);
	}
	return result;
}

UScenarioEditorListItemViewModel* UScenarioEditorOutlinerViewModel::CreateTemplatePanelItem(
	const FString& itemKey,
	const FString& label,
	const EScenarioTemplateSidebarPanel panel,
	const int32 depth)
{
	UScenarioEditorListItemViewModel* item = NewObject<UScenarioEditorListItemViewModel>(this);
	if (item)
	{
		item->InitializeOutlinerItem(
			itemKey,
			label,
			EScenarioEditorOutlinerItemType::TemplatePanel,
			panel,
			FString(),
			depth);
	}
	return item;
}
