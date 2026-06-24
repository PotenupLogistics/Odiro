#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Widget/ScenarioEditorOutlinerWidget.h"

#include "Misc/AutomationTest.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"

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

	template <typename TWidget>
	void TestResolvesCatalogBlueprintClass(
		FAutomationTestBase& test,
		const FString& label,
		const TSubclassOf<TWidget> resolvedClass)
	{
		test.TestNotNull(*FString::Printf(TEXT("%s resolves"), *label), resolvedClass.Get());
		test.TestTrue(
			*FString::Printf(TEXT("%s resolves to a Blueprint class"), *label),
			resolvedClass && resolvedClass.Get() != TWidget::StaticClass());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorWidgetClassCatalogDefaultsTest,
	"OdiroSim.ScenarioEditor.WidgetClassCatalog.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorWidgetClassCatalogDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> emptyCatalog;

	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Outliner row"),
		UScenarioEditorWidgetClassCatalog::ResolveOutlinerRowWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Sidebar block"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarBlockWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Sidebar field row"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Sidebar main panel"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarMainPanelWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Sidebar corridor panel"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPanelWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Corridor point row"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPointWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Sidebar obstacle panel"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePanelWidgetClass(emptyCatalog));
	TestResolvesCatalogBlueprintClass(
		*this,
		TEXT("Sidebar pedestrian panel"),
		UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianPanelWidgetClass(emptyCatalog));

	return true;
}

#endif
