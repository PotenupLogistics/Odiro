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

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Styling/SlateTypes.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseWidgetTypes.h"

namespace
{
	// Returns the editor world used by WBP-backed Scenario editor widget tests.
	UWorld* GetScenarioWidgetAutomationWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

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
	const FString groundRegionGroupKey = UScenarioEditorOutlinerWidget::MakeCategoryGroupItemKey(
		EScenarioActorCategory::GroundRegion);
	const FString legacyGroundRegionKey = UScenarioEditorOutlinerWidget::MakePlaceableItemKey(TEXT("region_001"));

	placeableItems.Add(MakeOutlinerLeaf(TEXT("bench_01"), obstacleGroupKey, TEXT("Bench")));
	placeableItems.Add(MakeOutlinerLeaf(TEXT("robot_start"), robotGroupKey, TEXT("Robot Start")));
	placeableItems.Add(MakeOutlinerLeaf(TEXT("region_001"), groundRegionGroupKey, TEXT("Legacy Ground Region")));

	TSet<FString> expandedKeys;
	expandedKeys.Add(scenarioKey);
	expandedKeys.Add(UScenarioEditorOutlinerWidget::MakeTemplateItemKey(EScenarioTemplateSidebarPanel::Corridor));
	expandedKeys.Add(robotGroupKey);
	expandedKeys.Add(obstacleGroupKey);
	expandedKeys.Add(groundRegionGroupKey);
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
	TestFalse(TEXT("Legacy Ground Regions group is omitted"), ContainsItemKey(builtItems, groundRegionGroupKey));
	TestFalse(TEXT("Legacy GroundRegion leaf is omitted"), ContainsItemKey(builtItems, legacyGroundRegionKey));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorSidebarBlockHeaderCustomizationTest,
	"OdiroSim.ScenarioEditor.SidebarBlock.HeaderCustomization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorSidebarBlockHeaderCustomizationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* world = GetScenarioWidgetAutomationWorld();
	TestNotNull(TEXT("editor world exists"), world);
	if (!world)
	{
		return false;
	}

	UClass* blockClass = LoadClass<UScenarioEditorSidebarBlockWidget>(
		nullptr,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarBlock.WBP_ScenarioEditorSidebarBlock_C"));
	TestNotNull(TEXT("sidebar block class loads"), blockClass);
	if (!blockClass)
	{
		return false;
	}

	UScenarioEditorSidebarBlockWidget* blockWidget =
		CreateWidget<UScenarioEditorSidebarBlockWidget>(world, blockClass);
	TestNotNull(TEXT("sidebar block widget creates"), blockWidget);
	if (!blockWidget || !blockWidget->WidgetTree)
	{
		return false;
	}
	blockWidget->TakeWidget();

	UBaseButtonWidget* addButton = Cast<UBaseButtonWidget>(
		blockWidget->WidgetTree->FindWidget(TEXT("AddActionButton")));
	UBaseButtonWidget* removeButton = Cast<UBaseButtonWidget>(
		blockWidget->WidgetTree->FindWidget(TEXT("RemoveActionButton")));
	UTextBlock* assetTypeText = Cast<UTextBlock>(
		blockWidget->WidgetTree->FindWidget(TEXT("AssetHeaderTypeTextBlock")));
	UTextBlock* assetNameText = Cast<UTextBlock>(
		blockWidget->WidgetTree->FindWidget(TEXT("AssetHeaderNameTextBlock")));
	UTextBlock* badgeText = Cast<UTextBlock>(
		blockWidget->WidgetTree->FindWidget(TEXT("BadgeTextBlock")));
	UWidget* headerActionSpacer =
		blockWidget->WidgetTree->FindWidget(TEXT("HeaderActionSpacer"));
	UHorizontalBox* headerActionBox = Cast<UHorizontalBox>(
		blockWidget->WidgetTree->FindWidget(TEXT("HeaderActionBox")));
	TestNotNull(TEXT("WBP-authored add action button exists"), addButton);
	TestNotNull(TEXT("WBP-authored remove action button exists"), removeButton);
	TestNotNull(TEXT("WBP-authored asset type text exists"), assetTypeText);
	TestNotNull(TEXT("WBP-authored asset name text exists"), assetNameText);
	TestNotNull(TEXT("WBP-authored badge text exists"), badgeText);
	TestNotNull(TEXT("WBP-authored header action spacer exists"), headerActionSpacer);
	TestNotNull(TEXT("WBP-authored header action box exists"), headerActionBox);
	if (!addButton || !removeButton || !assetTypeText || !assetNameText
		|| !badgeText || !headerActionSpacer || !headerActionBox)
	{
		return false;
	}
	UHorizontalBoxSlot* badgeSlot = Cast<UHorizontalBoxSlot>(badgeText->Slot);
	UHorizontalBoxSlot* actionBoxSlot = Cast<UHorizontalBoxSlot>(headerActionBox->Slot);
	TestNotNull(TEXT("badge text uses a horizontal slot"), badgeSlot);
	TestNotNull(TEXT("header action box uses a horizontal slot"), actionBoxSlot);
	if (!badgeSlot || !actionBoxSlot)
	{
		return false;
	}
	TestEqual(
		TEXT("add action uses WBP-authored Ghost variant"),
		addButton->GetVariant(),
		EBaseWidgetVariant::Ghost);
	TestEqual(
		TEXT("remove action uses WBP-authored Ghost variant"),
		removeButton->GetVariant(),
		EBaseWidgetVariant::Ghost);
	TestTrue(
		TEXT("badge slot fills remaining header width"),
		badgeSlot->GetSize().SizeRule == ESlateSizeRule::Fill);
	TestTrue(
		TEXT("badge slot aligns to the right edge"),
		badgeSlot->GetHorizontalAlignment() == HAlign_Right);
	TestTrue(
		TEXT("header action box keeps automatic width"),
		actionBoxSlot->GetSize().SizeRule == ESlateSizeRule::Automatic);
	TestTrue(
		TEXT("header action box aligns to the right edge"),
		actionBoxSlot->GetHorizontalAlignment() == HAlign_Right);
	TestEqual(
		TEXT("asset header spacer stays collapsed in badge mode"),
		headerActionSpacer->GetVisibility(),
		ESlateVisibility::Collapsed);

	const float authoredTypeFontSize = assetTypeText->GetFont().Size;
	const float authoredNameFontSize = assetNameText->GetFont().Size;
	blockWidget->SetAddActionVisible(true);
	blockWidget->SetRemoveActionVisible(true);
	TestEqual(
		TEXT("add action visibility follows runtime state"),
		addButton->GetVisibility(),
		ESlateVisibility::Visible);
	TestEqual(
		TEXT("remove action visibility follows runtime state"),
		removeButton->GetVisibility(),
		ESlateVisibility::Visible);
	TestFalse(
		TEXT("remove action is enabled when visible"),
		removeButton->IsDisabled());

	blockWidget->SetAssetHeaderSummary(
		FText::FromString(TEXT("Bench")),
		FText::FromString(TEXT("bench_01")),
		TSoftObjectPtr<UTexture2D>(),
		true);
	TestEqual(
		TEXT("asset type text is runtime data"),
		assetTypeText->GetText().ToString(),
		FString(TEXT("Bench")));
	TestEqual(
		TEXT("asset name text is runtime data"),
		assetNameText->GetText().ToString(),
		FString(TEXT("bench_01")));
	TestEqual(
		TEXT("badge hides while asset summary owns the header"),
		badgeText->GetVisibility(),
		ESlateVisibility::Collapsed);
	TestEqual(
		TEXT("asset header spacer pushes actions right when badge is hidden"),
		headerActionSpacer->GetVisibility(),
		ESlateVisibility::SelfHitTestInvisible);
	TestEqual(
		TEXT("asset type font size remains WBP-authored"),
		assetTypeText->GetFont().Size,
		authoredTypeFontSize);
	TestEqual(
		TEXT("asset name font size remains WBP-authored"),
		assetNameText->GetFont().Size,
		authoredNameFontSize);

	return true;
}

#endif
