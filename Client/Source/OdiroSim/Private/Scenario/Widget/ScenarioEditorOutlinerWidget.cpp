#include "Scenario/Widget/ScenarioEditorOutlinerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"
#include "Styling/SlateBrush.h"

namespace
{
	const FString ScenarioKey = TEXT("Scenario");
	const FString CorridorKey = TEXT("Scenario/Corridor");
	const FString RobotKey = TEXT("Scenario/Robot");
	const FString ObstaclesKey = TEXT("Scenario/Obstacles");
	const FString GroundRegionsKey = TEXT("Scenario/GroundRegions");
	const FString PedestriansKey = TEXT("Scenario/Pedestrians");

	const FLinearColor PanelColor(0.08f, 0.10f, 0.13f, 0.94f);

	FSlateBrush MakeOutlinerPanelBrush()
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(PanelColor);
		return brush;
	}

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

TSharedRef<SWidget> UScenarioEditorOutlinerWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorOutlinerWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
	if (const AScenarioEditorController* controller = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		const UScenarioPlaceableComponent* selectedPlaceable = controller->GetSelectedPlaceableComponent();
		if (selectedPlaceable && !selectedPlaceable->InstanceId.IsEmpty())
		{
			selectedKey = MakePlaceableItemKey(selectedPlaceable->InstanceId);
		}
	}

	TArray<FScenarioOutlinerItemViewModel> placeableItems;
	CollectPlaceableItems(placeableItems);
	BuildOutlinerItems(placeableItems, selectedKey, ExpandedItemKeys, CachedItems);
	SelectedItemKey = selectedKey;
	RebuildRows(CachedItems);
}

void UScenarioEditorOutlinerWidget::SetSelectedItemKey(const FString& itemKey)
{
	SelectedItemKey = itemKey.IsEmpty() ? ScenarioKey : itemKey;
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
		allItems.Add(MakeTemplateItem(itemKey, ScenarioKey, label, 1, panel));

		TArray<FScenarioOutlinerItemViewModel> groupPlaceables;
		placeablesByParent.MultiFind(itemKey, groupPlaceables);
		groupPlaceables.Sort([](
			const FScenarioOutlinerItemViewModel& lhs,
			const FScenarioOutlinerItemViewModel& rhs)
		{
			return lhs.Label.ToString() < rhs.Label.ToString();
		});
		for (const FScenarioOutlinerItemViewModel& placeableItem : groupPlaceables)
		{
			allItems.Add(placeableItem);
		}
	};

	appendGroup(CorridorKey, FText::FromString(TEXT("Corridor")), EScenarioTemplateSidebarPanel::Corridor);
	appendGroup(RobotKey, FText::FromString(TEXT("Robot")), EScenarioTemplateSidebarPanel::Main);
	appendGroup(ObstaclesKey, FText::FromString(TEXT("Obstacles")), EScenarioTemplateSidebarPanel::Obstacle);
	appendGroup(GroundRegionsKey, FText::FromString(TEXT("Ground Regions")), EScenarioTemplateSidebarPanel::Main);
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
	SetSelectedItemKey(item.ItemKey);
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

void UScenarioEditorOutlinerWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UBorder* panelBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("GeneratedOutlinerPanel"));
	if (!panelBorder)
	{
		return;
	}
	panelBorder->SetBrush(MakeOutlinerPanelBrush());
	panelBorder->SetBrushColor(PanelColor);
	panelBorder->SetPadding(FMargin(10.0f));
	WidgetTree->RootWidget = panelBorder;

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedOutlinerRootBox"));
	panelBorder->SetContent(rootBox);

	UTextBlock* titleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("GeneratedOutlinerTitleText"));
	titleText->SetText(FText::FromString(TEXT("Outliner")));
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(titleText, TextStyleCatalog, EWidgetTextStyleRole::Label);
	if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(titleText))
	{
		slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	RowScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("RowScrollBox"));
	if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(RowScrollBox.Get()))
	{
		slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	EmptyTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("EmptyTextBlock"));
	EmptyTextBlock->SetText(FText::FromString(TEXT("No scenario objects")));
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(EmptyTextBlock.Get(), TextStyleCatalog, EWidgetTextStyleRole::Caption);
}

void UScenarioEditorOutlinerWidget::RebuildRows(const TArray<FScenarioOutlinerItemViewModel>& items)
{
	if (!RowScrollBox)
	{
		return;
	}

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
	RowScrollBox->ClearChildren();

	if (EmptyTextBlock)
	{
		EmptyTextBlock->SetVisibility(items.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (items.IsEmpty())
		{
			RowScrollBox->AddChild(EmptyTextBlock.Get());
		}
	}

	TSubclassOf<UScenarioEditorOutlinerRowWidget> rowClass = RowWidgetClass;
	if (!rowClass)
	{
		rowClass = UScenarioEditorOutlinerRowWidget::StaticClass();
	}
	for (const FScenarioOutlinerItemViewModel& item : items)
	{
		UScenarioEditorOutlinerRowWidget* rowWidget =
			CreateWidget<UScenarioEditorOutlinerRowWidget>(this, rowClass);
		if (!rowWidget)
		{
			continue;
		}

		rowWidget->TextStyleCatalog = TextStyleCatalog;
		rowWidget->InitializeRow(item);
		rowWidget->OnRowSelected.AddDynamic(this, &UScenarioEditorOutlinerWidget::HandleRowSelected);
		rowWidget->OnRowExpansionToggled.AddDynamic(
			this,
			&UScenarioEditorOutlinerWidget::HandleRowExpansionToggled);
		RowScrollBox->AddChild(rowWidget);
		RowWidgets.Add(rowWidget);
	}
}

void UScenarioEditorOutlinerWidget::CollectPlaceableItems(
	TArray<FScenarioOutlinerItemViewModel>& outPlaceableItems) const
{
	outPlaceableItems.Reset();

	const UWorld* world = GetWorld();
	if (!world)
	{
		return;
	}

	for (TActorIterator<AActor> actorIt(world); actorIt; ++actorIt)
	{
		const AActor* actor = *actorIt;
		const UScenarioPlaceableComponent* placeableComponent =
			actor ? actor->FindComponentByClass<UScenarioPlaceableComponent>() : nullptr;
		if (!placeableComponent || !placeableComponent->bAuthoringSelectable || placeableComponent->InstanceId.IsEmpty())
		{
			continue;
		}

		outPlaceableItems.Add(MakePlaceableItem(
			ResolveParentKey(placeableComponent),
			MakePlaceableLabel(placeableComponent),
			FText::FromString(ActorCategoryToText(placeableComponent->Category)),
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
	ExpandedItemKeys.Add(GroundRegionsKey);
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
