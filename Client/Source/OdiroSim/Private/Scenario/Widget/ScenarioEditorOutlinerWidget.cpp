#include "Scenario/Widget/ScenarioEditorOutlinerWidget.h"

#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorOutlinerViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"

namespace
{
	const FString ScenarioKey = TEXT("Scenario");
	const FString CorridorKey = TEXT("Scenario/Corridor");
	const FString RobotKey = TEXT("Scenario/Robot");
	const FString ObstaclesKey = TEXT("Scenario/Obstacles");
	const FString GroundRegionsKey = TEXT("Scenario/GroundRegions");
	const FString PedestriansKey = TEXT("Scenario/Pedestrians");

	bool IsVisibleByExpandedParents(
		const FScenarioOutlinerItemViewModel& item,
		const TMap<FString, FScenarioOutlinerItemViewModel>& itemByKey)
	{
		FString parentKey = item.ParentKey;
		while (!parentKey.IsEmpty())
		{
			const FScenarioOutlinerItemViewModel* parentItem = itemByKey.Find(parentKey);
			if (!parentItem)
			{
				return true;
			}
			if (parentItem->bExpandable && !parentItem->bExpanded)
			{
				return false;
			}
			parentKey = parentItem->ParentKey;
		}
		return true;
	}

	bool IsHiddenLegacyGroundRegionPlaceable(const UScenarioPlaceableComponent* placeableComponent)
	{
		return placeableComponent
			&& placeableComponent->Category == EScenarioActorCategory::GroundRegion
			&& placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::Generic;
	}

	bool ShouldShowPlaceableInOutliner(const UScenarioPlaceableComponent* placeableComponent)
	{
		return placeableComponent
			&& placeableComponent->bAuthoringSelectable
			&& !placeableComponent->InstanceId.IsEmpty()
			&& !IsHiddenLegacyGroundRegionPlaceable(placeableComponent);
	}

	bool ShouldTrackPlaceableInOutlinerRegistry(const UScenarioPlaceableComponent* placeableComponent)
	{
		return placeableComponent && !IsHiddenLegacyGroundRegionPlaceable(placeableComponent);
	}

	FString ActorCategoryToText(const EScenarioActorCategory category)
	{
		switch (category)
		{
		case EScenarioActorCategory::DeliveryBot:
			return TEXT("Robot");
		case EScenarioActorCategory::StaticObstacle:
			return TEXT("Static obstacle");
		case EScenarioActorCategory::Pedestrian:
			return TEXT("Pedestrian");
		case EScenarioActorCategory::GroundRegion:
			return TEXT("Ground region");
		default:
			return TEXT("Placeable");
		}
	}

	FText MakePlaceableLabel(const UScenarioPlaceableComponent* placeableComponent)
	{
		if (!placeableComponent)
		{
			return FText::FromString(TEXT("Placeable"));
		}

		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotStartMarker)
		{
			return FText::FromString(TEXT("Robot Start"));
		}
		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotGoalMarker)
		{
			return FText::FromString(TEXT("Robot Goal"));
		}
		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle)
		{
			return FText::FromString(placeableComponent->InstanceId.IsEmpty()
				? FString(TEXT("Corridor Vertex"))
				: placeableComponent->InstanceId);
		}
		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle)
		{
			return FText::FromString(placeableComponent->InstanceId.IsEmpty()
				? FString(TEXT("Corridor Segment"))
				: placeableComponent->InstanceId);
		}

		return FText::FromString(placeableComponent->InstanceId.IsEmpty()
			? FString(TEXT("Placeable"))
			: placeableComponent->InstanceId);
	}

	FText MakePlaceableSubtitle(const UScenarioPlaceableComponent* placeableComponent)
	{
		if (!placeableComponent)
		{
			return FText::FromString(TEXT("Placeable"));
		}

		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle)
		{
			return FText::FromString(TEXT("Corridor vertex"));
		}

		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle)
		{
			return FText::FromString(TEXT("Corridor segment"));
		}

		return FText::FromString(ActorCategoryToText(placeableComponent->Category));
	}

	FString ResolveParentKey(const UScenarioPlaceableComponent* placeableComponent)
	{
		if (!placeableComponent)
		{
			return ScenarioKey;
		}

		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotStartMarker
			|| placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotGoalMarker)
		{
			return RobotKey;
		}
		if (placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle
			|| placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle)
		{
			return CorridorKey;
		}

		switch (placeableComponent->Category)
		{
		case EScenarioActorCategory::StaticObstacle:
			return ObstaclesKey;
		case EScenarioActorCategory::GroundRegion:
			return GroundRegionsKey;
		case EScenarioActorCategory::Pedestrian:
			return PedestriansKey;
		case EScenarioActorCategory::DeliveryBot:
			return RobotKey;
		default:
			return ScenarioKey;
		}
	}
}

void UScenarioEditorOutlinerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!WidgetClassCatalog.IsValid() && WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	AddDefaultExpandedKeys();
	RefreshFromEditorState();
}

void UScenarioEditorOutlinerWidget::NativeDestruct()
{
	for (UScenarioEditorOutlinerRowWidget* rowWidget : RowWidgets)
	{
		if (!rowWidget)
		{
			continue;
		}
		rowWidget->OnRowSelected.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowSelected);
		rowWidget->OnRowExpansionToggled.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowExpansionToggled);
	}
	RowWidgets.Reset();
	Super::NativeDestruct();
}

void UScenarioEditorOutlinerWidget::RefreshFromEditorState()
{
	AddDefaultExpandedKeys();

	FString selectedKey = SelectedItemKey.IsEmpty() ? ScenarioKey : SelectedItemKey;
	const UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	if (const UScenarioEditorShellViewModel* shellViewModel = uiSubsystem
			? uiSubsystem->GetShellViewModel()
			: nullptr)
	{
		if (!shellViewModel->GetSelectedPlaceableId().IsEmpty())
		{
			selectedKey = MakePlaceableItemKey(shellViewModel->GetSelectedPlaceableId());
		}
		else
		{
			selectedKey = MakeTemplateItemKey(shellViewModel->GetActiveSidebarPanel());
		}
	}

	TArray<FScenarioOutlinerItemViewModel> placeableItems;
	CollectPlaceableItems(placeableItems);
	BuildOutlinerItems(placeableItems, selectedKey, ExpandedItemKeys, CachedItems);
	SelectedItemKey = selectedKey;
	RebuildRows(CachedItems);
}

void UScenarioEditorOutlinerWidget::InvalidatePlaceableRegistry()
{
	PlaceableComponentRegistry.Reset();
	bPlaceableRegistryInitialized = false;
}

void UScenarioEditorOutlinerWidget::SetSelectedItemKey(const FString& itemKey)
{
	const FString resolvedItemKey = itemKey.IsEmpty() ? ScenarioKey : itemKey;
	if (SelectedItemKey == resolvedItemKey)
	{
		return;
	}

	SelectedItemKey = resolvedItemKey;
	for (UScenarioEditorOutlinerRowWidget* rowWidget : RowWidgets)
	{
		if (!rowWidget)
		{
			continue;
		}
		FScenarioOutlinerItemViewModel item = rowWidget->GetItem();
		rowWidget->SetSelected(item.ItemKey == SelectedItemKey);
	}
}

FString UScenarioEditorOutlinerWidget::MakePlaceableItemKey(const FString& instanceId)
{
	return instanceId.IsEmpty() ? FString() : FString::Printf(TEXT("Placeable:%s"), *instanceId);
}

void UScenarioEditorOutlinerWidget::BuildOutlinerItems(
	const TArray<FScenarioOutlinerItemViewModel>& placeableItems,
	const FString& selectedItemKey,
	const TSet<FString>& expandedItemKeys,
	TArray<FScenarioOutlinerItemViewModel>& outItems)
{
	TMultiMap<FString, FScenarioOutlinerItemViewModel> placeablesByParent;
	for (const FScenarioOutlinerItemViewModel& placeableItem : placeableItems)
	{
		placeablesByParent.Add(placeableItem.ParentKey, placeableItem);
	}

	TArray<FScenarioOutlinerItemViewModel> allItems;
	allItems.Add(MakeTemplateItem(
		ScenarioKey,
		FString(),
		FText::FromString(TEXT("Scenario")),
		0,
		EScenarioTemplateSidebarPanel::Main));

	auto appendGroup = [&allItems, &placeablesByParent](
		const FString& itemKey,
		const FText& label,
		const EScenarioTemplateSidebarPanel panel)
	{
		TArray<FScenarioOutlinerItemViewModel> groupPlaceables;
		placeablesByParent.MultiFind(itemKey, groupPlaceables);
		groupPlaceables.Sort([](
			const FScenarioOutlinerItemViewModel& lhs,
			const FScenarioOutlinerItemViewModel& rhs)
		{
			return lhs.Label.ToString() < rhs.Label.ToString();
		});

		allItems.Add(MakeTemplateItem(itemKey, ScenarioKey, label, 1, panel, !groupPlaceables.IsEmpty()));
		for (const FScenarioOutlinerItemViewModel& placeableItem : groupPlaceables)
		{
			allItems.Add(placeableItem);
		}
	};

	appendGroup(CorridorKey, FText::FromString(TEXT("Corridor")), EScenarioTemplateSidebarPanel::Corridor);
	appendGroup(RobotKey, FText::FromString(TEXT("Robot")), EScenarioTemplateSidebarPanel::Main);
	appendGroup(ObstaclesKey, FText::FromString(TEXT("Obstacles")), EScenarioTemplateSidebarPanel::Obstacle);
	appendGroup(PedestriansKey, FText::FromString(TEXT("Pedestrians")), EScenarioTemplateSidebarPanel::Pedestrian);

	TMap<FString, FScenarioOutlinerItemViewModel> itemByKey;
	for (FScenarioOutlinerItemViewModel& item : allItems)
	{
		item.bExpanded = !item.bExpandable || expandedItemKeys.Contains(item.ItemKey);
		item.bSelected = item.ItemKey == selectedItemKey;
		itemByKey.Add(item.ItemKey, item);
	}

	outItems.Reset();
	for (const FScenarioOutlinerItemViewModel& item : allItems)
	{
		if (IsVisibleByExpandedParents(item, itemByKey))
		{
			outItems.Add(item);
		}
	}
}

void UScenarioEditorOutlinerWidget::HandleRowSelected(FScenarioOutlinerItemViewModel item)
{
	const FString resolvedItemKey = item.ItemKey.IsEmpty() ? ScenarioKey : item.ItemKey;
	if (SelectedItemKey != resolvedItemKey)
	{
		SetSelectedItemKey(item.ItemKey);
		if (UScenarioEditorOutlinerViewModel* outlinerViewModel = GetOutlinerViewModel())
		{
			outlinerViewModel->SetSelectedItemKey(item.ItemKey);
		}
	}
	OnItemSelected.Broadcast(item);
}

void UScenarioEditorOutlinerWidget::HandleRowExpansionToggled(FScenarioOutlinerItemViewModel item)
{
	if (item.bExpanded)
	{
		ExpandedItemKeys.Add(item.ItemKey);
	}
	else
	{
		ExpandedItemKeys.Remove(item.ItemKey);
	}
	RefreshFromEditorState();
}

UScenarioEditorOutlinerViewModel* UScenarioEditorOutlinerWidget::GetOutlinerViewModel() const
{
	const UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetOutlinerViewModel() : nullptr;
}

void UScenarioEditorOutlinerWidget::RebuildRows(const TArray<FScenarioOutlinerItemViewModel>& items)
{
	if (!RowScrollBox)
	{
		return;
	}

	TMap<FString, UScenarioEditorOutlinerRowWidget*> reusableRowsByKey;
	TArray<UScenarioEditorOutlinerRowWidget*> retiredRows;
	for (UScenarioEditorOutlinerRowWidget* rowWidget : RowWidgets)
	{
		if (!rowWidget)
		{
			continue;
		}
		RowScrollBox->RemoveChild(rowWidget);

		const FString rowItemKey = rowWidget->GetItem().ItemKey;
		if (!rowItemKey.IsEmpty() && !reusableRowsByKey.Contains(rowItemKey))
		{
			reusableRowsByKey.Add(rowItemKey, rowWidget);
		}
		else
		{
			retiredRows.Add(rowWidget);
		}
	}
	RowWidgets.Reset();

	if (EmptyTextBlock)
	{
		RowScrollBox->RemoveChild(EmptyTextBlock.Get());
		EmptyTextBlock->SetVisibility(items.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (items.IsEmpty())
		{
			RowScrollBox->AddChild(EmptyTextBlock.Get());
		}
	}

	TSubclassOf<UScenarioEditorOutlinerRowWidget> rowClass = RowWidgetClass;
	if (!rowClass)
	{
		rowClass = UScenarioEditorWidgetClassCatalog::ResolveOutlinerRowWidgetClass(WidgetClassCatalog);
	}
	if (!rowClass)
	{
		if (EmptyTextBlock)
		{
			EmptyTextBlock->SetText(FText::FromString(TEXT("Outliner row widget class is missing.")));
			EmptyTextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (!EmptyTextBlock->GetParent())
			{
				RowScrollBox->AddChild(EmptyTextBlock.Get());
			}
		}
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Scenario editor outliner row WBP class is missing. Configure WidgetClassCatalog or RowWidgetClass."));
		return;
	}

	TArray<UScenarioEditorListItemViewModel*> itemViewModels;
	if (UScenarioEditorOutlinerViewModel* outlinerViewModel = GetOutlinerViewModel())
	{
		outlinerViewModel->SetItemsFromOutlinerRows(items);
		itemViewModels = outlinerViewModel->GetItems();
	}

	TSet<UScenarioEditorOutlinerRowWidget*> reusedRows;
	for (int32 itemIndex = 0; itemIndex < items.Num(); ++itemIndex)
	{
		UScenarioEditorOutlinerRowWidget* rowWidget = nullptr;
		if (UScenarioEditorOutlinerRowWidget** reusableRow = reusableRowsByKey.Find(items[itemIndex].ItemKey))
		{
			rowWidget = *reusableRow;
		}
		if (!rowWidget)
		{
			rowWidget = CreateWidget<UScenarioEditorOutlinerRowWidget>(this, rowClass);
		}
		if (!rowWidget)
		{
			continue;
		}

		rowWidget->TextStyleCatalog = TextStyleCatalog;
		if (itemViewModels.IsValidIndex(itemIndex))
		{
			rowWidget->InitializeFromItemViewModel(itemViewModels[itemIndex]);
		}
		else
		{
			rowWidget->InitializeRow(items[itemIndex]);
		}
		rowWidget->OnRowSelected.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowSelected);
		rowWidget->OnRowSelected.AddDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowSelected);
		rowWidget->OnRowExpansionToggled.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowExpansionToggled);
		rowWidget->OnRowExpansionToggled.AddDynamic(
			this,
			&UScenarioEditorOutlinerWidget::HandleRowExpansionToggled);
		RowScrollBox->AddChild(rowWidget);
		RowWidgets.Add(rowWidget);
		reusedRows.Add(rowWidget);
	}

	for (const TPair<FString, UScenarioEditorOutlinerRowWidget*>& reusableRowPair : reusableRowsByKey)
	{
		UScenarioEditorOutlinerRowWidget* rowWidget = reusableRowPair.Value;
		if (!rowWidget || reusedRows.Contains(rowWidget))
		{
			continue;
		}
		rowWidget->OnRowSelected.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowSelected);
		rowWidget->OnRowExpansionToggled.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowExpansionToggled);
	}
	for (UScenarioEditorOutlinerRowWidget* rowWidget : retiredRows)
	{
		if (!rowWidget)
		{
			continue;
		}
		rowWidget->OnRowSelected.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowSelected);
		rowWidget->OnRowExpansionToggled.RemoveDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowExpansionToggled);
	}
}

void UScenarioEditorOutlinerWidget::CollectPlaceableItems(
	TArray<FScenarioOutlinerItemViewModel>& outPlaceableItems)
{
	outPlaceableItems.Reset();

	if (!bPlaceableRegistryInitialized)
	{
		RebuildPlaceableRegistry();
	}
	else
	{
		CompactPlaceableRegistry();
		SyncPlaceableRegistryFromWorld();
	}

	for (const TWeakObjectPtr<UScenarioPlaceableComponent>& placeableComponentPtr : PlaceableComponentRegistry)
	{
		const UScenarioPlaceableComponent* placeableComponent = placeableComponentPtr.Get();
		if (!ShouldShowPlaceableInOutliner(placeableComponent))
		{
			continue;
		}

		outPlaceableItems.Add(MakePlaceableItem(
			ResolveParentKey(placeableComponent),
			MakePlaceableLabel(placeableComponent),
			MakePlaceableSubtitle(placeableComponent),
			2,
			placeableComponent->InstanceId,
			placeableComponent->Category));
	}

	outPlaceableItems.Sort([](
		const FScenarioOutlinerItemViewModel& lhs,
		const FScenarioOutlinerItemViewModel& rhs)
	{
		if (lhs.ParentKey != rhs.ParentKey)
		{
			return lhs.ParentKey < rhs.ParentKey;
		}
		return lhs.Label.ToString() < rhs.Label.ToString();
	});
}

void UScenarioEditorOutlinerWidget::RebuildPlaceableRegistry()
{
	PlaceableComponentRegistry.Reset();

	const UWorld* world = GetWorld();
	if (!world)
	{
		bPlaceableRegistryInitialized = false;
		return;
	}

	for (TActorIterator<AActor> actorIt(world); actorIt; ++actorIt)
	{
		const AActor* actor = *actorIt;
		UScenarioPlaceableComponent* placeableComponent =
			actor ? actor->FindComponentByClass<UScenarioPlaceableComponent>() : nullptr;
		if (ShouldTrackPlaceableInOutlinerRegistry(placeableComponent))
		{
			PlaceableComponentRegistry.Add(placeableComponent);
		}
	}

	bPlaceableRegistryInitialized = true;
}

void UScenarioEditorOutlinerWidget::CompactPlaceableRegistry()
{
	PlaceableComponentRegistry.RemoveAllSwap([](const TWeakObjectPtr<UScenarioPlaceableComponent>& placeableComponentPtr)
	{
		return !ShouldTrackPlaceableInOutlinerRegistry(placeableComponentPtr.Get());
	});
}

void UScenarioEditorOutlinerWidget::SyncPlaceableRegistryFromWorld()
{
	UWorld* world = GetWorld();
	if (!world)
	{
		return;
	}

	TSet<const UScenarioPlaceableComponent*> registeredComponents;
	for (const TWeakObjectPtr<UScenarioPlaceableComponent>& placeableComponentPtr : PlaceableComponentRegistry)
	{
		if (const UScenarioPlaceableComponent* placeableComponent = placeableComponentPtr.Get())
		{
			registeredComponents.Add(placeableComponent);
		}
	}

	for (TActorIterator<AActor> actorIt(world); actorIt; ++actorIt)
	{
		const AActor* actor = *actorIt;
		UScenarioPlaceableComponent* placeableComponent =
			actor ? actor->FindComponentByClass<UScenarioPlaceableComponent>() : nullptr;
		if (ShouldTrackPlaceableInOutlinerRegistry(placeableComponent)
			&& !registeredComponents.Contains(placeableComponent))
		{
			PlaceableComponentRegistry.Add(placeableComponent);
			registeredComponents.Add(placeableComponent);
		}
	}
}

void UScenarioEditorOutlinerWidget::AddDefaultExpandedKeys()
{
	if (!ExpandedItemKeys.IsEmpty())
	{
		return;
	}

	ExpandedItemKeys.Add(ScenarioKey);
	ExpandedItemKeys.Add(CorridorKey);
	ExpandedItemKeys.Add(RobotKey);
	ExpandedItemKeys.Add(ObstaclesKey);
	ExpandedItemKeys.Add(PedestriansKey);
}

FString UScenarioEditorOutlinerWidget::MakeTemplateItemKey(const EScenarioTemplateSidebarPanel panel)
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Corridor:
		return CorridorKey;
	case EScenarioTemplateSidebarPanel::Obstacle:
		return ObstaclesKey;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return PedestriansKey;
	case EScenarioTemplateSidebarPanel::Main:
	default:
		return ScenarioKey;
	}
}

FString UScenarioEditorOutlinerWidget::MakeCategoryGroupItemKey(const EScenarioActorCategory actorCategory)
{
	switch (actorCategory)
	{
	case EScenarioActorCategory::DeliveryBot:
		return RobotKey;
	case EScenarioActorCategory::StaticObstacle:
		return ObstaclesKey;
	case EScenarioActorCategory::GroundRegion:
		return GroundRegionsKey;
	case EScenarioActorCategory::Pedestrian:
		return PedestriansKey;
	default:
		return ScenarioKey;
	}
}

FScenarioOutlinerItemViewModel UScenarioEditorOutlinerWidget::MakeTemplateItem(
	const FString& itemKey,
	const FString& parentKey,
	const FText& label,
	const int32 depth,
	const EScenarioTemplateSidebarPanel panel,
	const bool bExpandable)
{
	FScenarioOutlinerItemViewModel item;
	item.ItemKey = itemKey;
	item.ParentKey = parentKey;
	item.Label = label;
	item.Depth = depth;
	item.bExpandable = bExpandable;
	item.ItemType = EScenarioEditorOutlinerItemType::TemplatePanel;
	item.TemplatePanel = panel;
	return item;
}

FScenarioOutlinerItemViewModel UScenarioEditorOutlinerWidget::MakePlaceableItem(
	const FString& parentKey,
	const FText& label,
	const FText& subtitle,
	const int32 depth,
	const FString& instanceId,
	const EScenarioActorCategory actorCategory)
{
	FScenarioOutlinerItemViewModel item;
	item.ItemKey = MakePlaceableItemKey(instanceId);
	item.ParentKey = parentKey;
	item.Label = label;
	item.Subtitle = subtitle;
	item.Depth = depth;
	item.ItemType = EScenarioEditorOutlinerItemType::Placeable;
	item.InstanceId = instanceId;
	item.ActorCategory = actorCategory;
	return item;
}
