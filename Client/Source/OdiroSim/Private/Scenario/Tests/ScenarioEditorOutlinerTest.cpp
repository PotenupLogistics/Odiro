#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Widget/ScenarioEditorOutlinerWidget.h"

#include "Misc/AutomationTest.h"

namespace
{
	FScenarioOutlinerItemViewModel MakeOutlinerLeaf(
		const FString& instanceId,
		const FString& parentKey,
		const FString& label)
	{
		FScenarioOutlinerItemViewModel item;
		item.ItemKey = UScenarioEditorOutlinerWidget::MakePlaceableItemKey(instanceId);
		item.ParentKey = parentKey;
		item.Label = FText::FromString(label);
		item.Depth = 2;
		item.ItemType = EScenarioEditorOutlinerItemType::Placeable;
		item.InstanceId = instanceId;
		item.ActorCategory = EScenarioActorCategory::StaticObstacle;
		return item;
	}

	bool ContainsItemKey(
		const TArray<FScenarioOutlinerItemViewModel>& items,
		const FString& itemKey)
	{
		return items.ContainsByPredicate(
			[&itemKey](const FScenarioOutlinerItemViewModel& item)
			{
				return item.ItemKey == itemKey;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorOutlinerModelTest,
	"OdiroSim.ScenarioEditor.Outliner.Model",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorOutlinerModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<FScenarioOutlinerItemViewModel> placeableItems;
	const FString benchKey = UScenarioEditorOutlinerWidget::MakePlaceableItemKey(TEXT("bench_01"));
	const FString robotStartKey = UScenarioEditorOutlinerWidget::MakePlaceableItemKey(TEXT("robot_start"));
	const FString scenarioKey = UScenarioEditorOutlinerWidget::MakeTemplateItemKey(
		EScenarioTemplateSidebarPanel::Main);
	const FString obstacleGroupKey = UScenarioEditorOutlinerWidget::MakeCategoryGroupItemKey(
		EScenarioActorCategory::StaticObstacle);
	const FString robotGroupKey = UScenarioEditorOutlinerWidget::MakeCategoryGroupItemKey(
		EScenarioActorCategory::DeliveryBot);

	placeableItems.Add(MakeOutlinerLeaf(TEXT("bench_01"), obstacleGroupKey, TEXT("Bench")));
	placeableItems.Add(MakeOutlinerLeaf(TEXT("robot_start"), robotGroupKey, TEXT("Robot Start")));

	TSet<FString> expandedKeys;
	expandedKeys.Add(scenarioKey);
	expandedKeys.Add(UScenarioEditorOutlinerWidget::MakeTemplateItemKey(EScenarioTemplateSidebarPanel::Corridor));
	expandedKeys.Add(robotGroupKey);
	expandedKeys.Add(obstacleGroupKey);
	expandedKeys.Add(UScenarioEditorOutlinerWidget::MakeCategoryGroupItemKey(EScenarioActorCategory::GroundRegion));
	expandedKeys.Add(UScenarioEditorOutlinerWidget::MakeTemplateItemKey(EScenarioTemplateSidebarPanel::Pedestrian));

	TArray<FScenarioOutlinerItemViewModel> builtItems;
	UScenarioEditorOutlinerWidget::BuildOutlinerItems(
		placeableItems,
		benchKey,
		expandedKeys,
		builtItems);

	TestTrue(TEXT("Scenario root is first"), builtItems.Num() > 0 && builtItems[0].ItemKey == scenarioKey);
	TestTrue(TEXT("Robot child is present under expanded Robot group"), ContainsItemKey(builtItems, robotStartKey));
	TestTrue(TEXT("Obstacle child is present under expanded Obstacles group"), ContainsItemKey(builtItems, benchKey));

	const FScenarioOutlinerItemViewModel* selectedItem = builtItems.FindByPredicate(
		[&benchKey](const FScenarioOutlinerItemViewModel& item)
		{
			return item.ItemKey == benchKey;
		});
	TestTrue(TEXT("Selected outliner item is marked selected"), selectedItem && selectedItem->bSelected);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorOutlinerSelectionTest,
	"OdiroSim.ScenarioEditor.Outliner.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorOutlinerSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<FScenarioOutlinerItemViewModel> placeableItems;
	const FString benchKey = UScenarioEditorOutlinerWidget::MakePlaceableItemKey(TEXT("bench_01"));
	const FString scenarioKey = UScenarioEditorOutlinerWidget::MakeTemplateItemKey(
		EScenarioTemplateSidebarPanel::Main);
	const FString obstacleGroupKey = UScenarioEditorOutlinerWidget::MakeCategoryGroupItemKey(
		EScenarioActorCategory::StaticObstacle);

	placeableItems.Add(MakeOutlinerLeaf(TEXT("bench_01"), obstacleGroupKey, TEXT("Bench")));

	TSet<FString> expandedKeys;
	expandedKeys.Add(scenarioKey);

	TArray<FScenarioOutlinerItemViewModel> builtItems;
	UScenarioEditorOutlinerWidget::BuildOutlinerItems(
		placeableItems,
		benchKey,
		expandedKeys,
		builtItems);

	TestTrue(TEXT("Collapsed Obstacles group hides its leaf"), !ContainsItemKey(builtItems, benchKey));
	TestTrue(TEXT("Collapsed group row remains selectable"), ContainsItemKey(builtItems, obstacleGroupKey));
	TestEqual(
		TEXT("Placeable item key maps from instance id"),
		benchKey,
		FString(TEXT("Placeable:bench_01")));

	return true;
}

#endif
