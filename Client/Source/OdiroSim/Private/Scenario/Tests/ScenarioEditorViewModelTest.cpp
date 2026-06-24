#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ViewModel/ScenarioAssetPaletteViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorOutlinerViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorToolbarViewModel.h"
#include "Scenario/ViewModel/ScenarioLlmPromptViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorShellViewModelTest,
	"OdiroSim.ScenarioEditor.ViewModel.Shell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorShellViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UScenarioEditorShellViewModel* viewModel = NewObject<UScenarioEditorShellViewModel>();
	TestNotNull(TEXT("Shell ViewModel is created"), viewModel);

	viewModel->SelectInspectorTab(EScenarioEditorInspectorTab::Llm);
	viewModel->SelectSidebarPanel(EScenarioTemplateSidebarPanel::Obstacle);
	viewModel->SetAssetPaletteVisible(true);

	TestEqual(TEXT("Inspector tab updates"), viewModel->GetActiveInspectorTab(), EScenarioEditorInspectorTab::Llm);
	TestEqual(TEXT("Sidebar panel updates"), viewModel->GetActiveSidebarPanel(), EScenarioTemplateSidebarPanel::Obstacle);
	TestEqual(
		TEXT("Sidebar panel selects default template block"),
		viewModel->GetSelectedTemplateBlockPath(),
		FString(TEXT("root.obstacles")));
	TestTrue(TEXT("Asset palette visibility updates"), viewModel->IsAssetPaletteVisible());
	TestFalse(TEXT("Controller command fails without subsystem"), viewModel->SetPerspectiveViewMode());

	viewModel->SelectTemplateBlock(EScenarioTemplateSidebarPanel::Corridor, TEXT("root.corridor.axis"));
	TestEqual(
		TEXT("Template block selection updates active panel"),
		viewModel->GetActiveSidebarPanel(),
		EScenarioTemplateSidebarPanel::Corridor);
	TestEqual(
		TEXT("Template block path updates"),
		viewModel->GetSelectedTemplateBlockPath(),
		FString(TEXT("root.corridor.axis")));

	viewModel->ClearSelection();
	TestTrue(TEXT("Placeable selection clears"), viewModel->GetSelectedPlaceableId().IsEmpty());
	TestEqual(
		TEXT("Clear selection restores active panel root block"),
		viewModel->GetSelectedTemplateBlockPath(),
		FString(TEXT("root.corridor")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorToolbarViewModelTest,
	"OdiroSim.ScenarioEditor.ViewModel.Toolbar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorToolbarViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UScenarioEditorToolbarViewModel* viewModel = NewObject<UScenarioEditorToolbarViewModel>();
	TestNotNull(TEXT("Toolbar ViewModel is created"), viewModel);

	viewModel->SelectSidebarPanel(EScenarioTemplateSidebarPanel::Corridor);
	viewModel->SetStatusText(TEXT("Ready"));

	TestEqual(TEXT("Active panel updates"), viewModel->GetActiveSidebarPanel(), EScenarioTemplateSidebarPanel::Corridor);
	TestEqual(TEXT("Status updates"), viewModel->GetStatusText(), FString(TEXT("Ready")));
	TestFalse(TEXT("Save command fails without subsystem"), viewModel->SaveScenario());
	TestTrue(TEXT("Save failure reports missing subsystem"), viewModel->GetStatusText().Contains(TEXT("unavailable")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorSidebarViewModelTest,
	"OdiroSim.ScenarioEditor.ViewModel.Sidebar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorSidebarViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UScenarioTemplateSidebarViewModel* viewModel = NewObject<UScenarioTemplateSidebarViewModel>();
	TestNotNull(TEXT("Sidebar ViewModel is created"), viewModel);

	viewModel->RefreshDefaultFields();
	viewModel->SelectPanel(EScenarioTemplateSidebarPanel::Pedestrian);

	TestEqual(TEXT("Active panel updates"), viewModel->GetActivePanel(), EScenarioTemplateSidebarPanel::Pedestrian);
	TestTrue(TEXT("Main fields exist"), viewModel->GetMainFieldItems().Num() > 0);
	TestTrue(TEXT("Visible fields come from active panel"), viewModel->GetVisibleFieldItems().Num() == viewModel->GetPedestrianFieldItems().Num());

	FScenarioDocument scenarioTemplate;
	scenarioTemplate.Corridor.Axis.Type = EScenarioCorridorAxisType::Polyline;
	scenarioTemplate.Corridor.Axis.PointsMeters = { FVector2D(0.0, 0.0), FVector2D(3.0, 4.0) };
	scenarioTemplate.Corridor.WalkwayWidthMeters =
		UScenarioTemplateSidebarViewModel::MakeRangeTemplateNumberValue(1.2, 2.4);
	FScenarioTemplateLaneRule buildingLane;
	buildingLane.SurfaceId = TEXT("sidewalk");
	buildingLane.WidthMeters = UScenarioTemplateSidebarViewModel::MakeFixedTemplateNumberValue(0.75);
	scenarioTemplate.Corridor.BuildingSide = { buildingLane };
	FScenarioTemplateSegment narrowSegment;
	narrowSegment.SegmentId = TEXT("narrow");
	narrowSegment.Type = EScenarioTemplateSegmentType::Narrowing;
	narrowSegment.AlongRangeMeters.StartMeters = 1.0;
	narrowSegment.AlongRangeMeters.EndMeters = 2.0;
	narrowSegment.ReplacedBySurfaceId.bIsSet = true;
	narrowSegment.ReplacedBySurfaceId.Mode = EScenarioTemplateStringValueMode::Fixed;
	narrowSegment.ReplacedBySurfaceId.FixedValue = TEXT("tile");
	scenarioTemplate.Corridor.Segments = { narrowSegment };
	scenarioTemplate.Robot.Start.Type = EScenarioTemplateRobotAnchorType::CorridorPose;
	scenarioTemplate.Robot.Start.SegmentId = TEXT("narrow");
	scenarioTemplate.Robot.Start.LaneId = TEXT("walkway");
	viewModel->RefreshMainFieldItemsFromTemplate(scenarioTemplate);
	viewModel->RefreshCorridorFieldItemsFromTemplate(scenarioTemplate);

	TestNull(TEXT("Main schema field is hidden from editor rows"), viewModel->FindMainFieldItem(TEXT("Schema")));
	TestNull(TEXT("Main version field is hidden from editor rows"), viewModel->FindMainFieldItem(TEXT("Version")));
	UScenarioTemplateFieldRowViewModel* robotStartSegmentItem = viewModel->FindMainFieldItem(TEXT("RobotStartSegment"));
	TestNotNull(TEXT("Robot start segment field item exists"), robotStartSegmentItem);
	TestEqual(TEXT("Robot start segment uses combo input"), robotStartSegmentItem ? robotStartSegmentItem->GetInputType() : EScenarioEditorSidebarFieldInputType::Text, EScenarioEditorSidebarFieldInputType::ComboBox);
	TestTrue(TEXT("Robot start segment has corridor id option"), robotStartSegmentItem && robotStartSegmentItem->GetComboOptions().Contains(TEXT("narrow")));
	UScenarioTemplateFieldRowViewModel* robotStartLaneItem = viewModel->FindMainFieldItem(TEXT("RobotStartLane"));
	TestNotNull(TEXT("Robot start lane field item exists"), robotStartLaneItem);
	TestEqual(TEXT("Robot start lane uses combo input"), robotStartLaneItem ? robotStartLaneItem->GetInputType() : EScenarioEditorSidebarFieldInputType::Text, EScenarioEditorSidebarFieldInputType::ComboBox);
	TestTrue(TEXT("Robot start lane has lane option"), robotStartLaneItem && robotStartLaneItem->GetComboOptions().Contains(TEXT("walkway")));

	FString corridorCommandStatus;
	TestFalse(
		TEXT("Corridor walkway command rejects invalid number"),
		viewModel->CommitCorridorWalkwayWidthText(
			FText::FromString(TEXT("not-a-number")),
			corridorCommandStatus));
	TestTrue(
		TEXT("Corridor invalid command reports parse status"),
		corridorCommandStatus.Contains(TEXT("number")));
	TestEqual(
		TEXT("Corridor axis length is measured"),
		UScenarioTemplateSidebarViewModel::MeasureAxisLengthMeters(scenarioTemplate.Corridor.Axis.PointsMeters),
		5.0);

	UScenarioTemplateFieldRowViewModel* axisTypeItem = viewModel->FindCorridorFieldItem(TEXT("AxisType"));
	TestNotNull(TEXT("Corridor axis type field item exists"), axisTypeItem);
	TestEqual(TEXT("Corridor axis type is formatted"), axisTypeItem ? axisTypeItem->GetValueText() : FString(), FString(TEXT("polyline")));

	UScenarioTemplateFieldRowViewModel* walkwayWidthItem = viewModel->FindCorridorFieldItem(TEXT("WalkwayWidth"));
	TestNotNull(TEXT("Corridor walkway width field item exists"), walkwayWidthItem);
	TestTrue(TEXT("Corridor walkway width enables range input"), walkwayWidthItem && walkwayWidthItem->IsRangeInputEnabled());
	TestEqual(TEXT("Corridor walkway width min is formatted"), walkwayWidthItem ? walkwayWidthItem->GetMinValueText() : FString(), FString(TEXT("1.20")));

	UScenarioTemplateFieldRowViewModel* buildingCountItem = viewModel->FindCorridorFieldItem(TEXT("BuildingSideCount"));
	TestNotNull(TEXT("Corridor building side count field item exists"), buildingCountItem);
	TestEqual(TEXT("Corridor building side count is formatted"), buildingCountItem ? buildingCountItem->GetValueText() : FString(), FString(TEXT("1")));
	TestEqual(
		TEXT("Corridor building side count uses object array input"),
		buildingCountItem ? buildingCountItem->GetInputType() : EScenarioEditorSidebarFieldInputType::Text,
		EScenarioEditorSidebarFieldInputType::ObjectArray);

	TArray<UScenarioTemplateFieldRowViewModel*> pointItems =
		viewModel->CreateCorridorPointFieldItems(0, scenarioTemplate.Corridor.Axis.PointsMeters[0]);
	TestEqual(TEXT("Corridor point field items are created"), pointItems.Num(), 2);
	TestEqual(TEXT("Corridor point x is formatted"), pointItems.IsValidIndex(0) ? pointItems[0]->GetValueText() : FString(), FString(TEXT("0.00")));
	TestEqual(
		TEXT("Corridor point x keeps number input"),
		pointItems.IsValidIndex(0) ? pointItems[0]->GetInputType() : EScenarioEditorSidebarFieldInputType::Text,
		EScenarioEditorSidebarFieldInputType::Number);
	TestTrue(TEXT("Corridor point x exposes array controls"), pointItems.IsValidIndex(0) && pointItems[0]->HasArrayControls());

	const TArray<FString> surfaceOptions = { TEXT("sidewalk"), TEXT("tile") };
	TArray<UScenarioTemplateFieldRowViewModel*> laneItems =
		viewModel->CreateCorridorLaneFieldItems(
			EScenarioEditorCorridorSide::Building,
			0,
			buildingLane,
			surfaceOptions);
	TestEqual(TEXT("Corridor lane field items are created"), laneItems.Num(), 2);
	TestEqual(TEXT("Corridor lane surface is formatted"), laneItems.IsValidIndex(0) ? laneItems[0]->GetValueText() : FString(), FString(TEXT("sidewalk")));
	TestTrue(TEXT("Corridor lane surface has combo option"), laneItems.IsValidIndex(0) && laneItems[0]->GetComboOptions().Contains(TEXT("tile")));
	TestEqual(TEXT("Corridor lane width is formatted"), laneItems.IsValidIndex(1) ? laneItems[1]->GetValueText() : FString(), FString(TEXT("0.75")));

	TArray<UScenarioTemplateFieldRowViewModel*> segmentItems =
		viewModel->CreateCorridorSegmentFieldItems(0, narrowSegment, surfaceOptions);
	TestEqual(TEXT("Corridor segment field items are created"), segmentItems.Num(), 4);
	TestEqual(TEXT("Corridor segment type is formatted"), segmentItems.IsValidIndex(1) ? segmentItems[1]->GetValueText() : FString(), FString(TEXT("narrowing")));
	TestEqual(TEXT("Corridor segment along min is formatted"), segmentItems.IsValidIndex(2) ? segmentItems[2]->GetMinValueText() : FString(), FString(TEXT("1.00")));
	TestEqual(TEXT("Corridor segment replacement is formatted"), segmentItems.IsValidIndex(3) ? segmentItems[3]->GetValueText() : FString(), FString(TEXT("tile")));
	TestTrue(TEXT("Corridor segment replacement allows unset"), segmentItems.IsValidIndex(3) && segmentItems[3]->AllowsComboUnset());

	scenarioTemplate.Pedestrians.Background.Count = UScenarioTemplateSidebarViewModel::MakeRangeTemplateIntegerValue(2, 4);
	scenarioTemplate.Pedestrians.Background.SpeedMetersPerSecond =
		UScenarioTemplateSidebarViewModel::MakeFixedTemplateNumberValue(1.25);
	scenarioTemplate.Pedestrians.Background.SpawnSegmentIds = { TEXT("north"), TEXT("south") };
	scenarioTemplate.Pedestrians.Encounters.AddDefaulted();
	scenarioTemplate.Pedestrians.Encounters[0].EncounterId = TEXT("meet_runner");
	scenarioTemplate.Pedestrians.Encounters[0].Type = EScenarioTemplateEncounterType::CrossPath;
	scenarioTemplate.Pedestrians.Encounters[0].AtSegmentId = TEXT("narrow");
	scenarioTemplate.Pedestrians.Encounters[0].PersonaId = TEXT("normal");
	scenarioTemplate.Pedestrians.Encounters[0].MeetOffsetMeters =
		UScenarioTemplateSidebarViewModel::MakeRangeTemplateNumberValue(0.5, 1.5);
	viewModel->RefreshPedestrianFieldItemsFromTemplate(scenarioTemplate);

	UScenarioTemplateFieldRowViewModel* countItem = viewModel->FindPedestrianFieldItem(TEXT("BackgroundCount"));
	TestNotNull(TEXT("Pedestrian count field item exists"), countItem);
	TestEqual(TEXT("Pedestrian count field uses range input"), countItem ? countItem->GetInputType() : EScenarioEditorSidebarFieldInputType::Text, EScenarioEditorSidebarFieldInputType::Range);
	TestTrue(TEXT("Pedestrian count field enables range input"), countItem && countItem->IsRangeInputEnabled());
	TestEqual(TEXT("Pedestrian count range min is formatted"), countItem ? countItem->GetMinValueText() : FString(), FString(TEXT("2")));
	TestEqual(TEXT("Pedestrian count range max is formatted"), countItem ? countItem->GetMaxValueText() : FString(), FString(TEXT("4")));

	UScenarioTemplateFieldRowViewModel* spawnSegmentsItem = viewModel->FindPedestrianFieldItem(TEXT("SpawnSegments"));
	TestNotNull(TEXT("Pedestrian spawn segments field item exists"), spawnSegmentsItem);
	TestEqual(TEXT("Pedestrian spawn segments join as display text"), spawnSegmentsItem ? spawnSegmentsItem->GetValueText() : FString(), FString(TEXT("north, south")));
	TestEqual(
		TEXT("Pedestrian spawn segments use string list input"),
		spawnSegmentsItem ? spawnSegmentsItem->GetInputType() : EScenarioEditorSidebarFieldInputType::Text,
		EScenarioEditorSidebarFieldInputType::StringList);

	TArray<UScenarioTemplateFieldRowViewModel*> encounterItems =
		viewModel->CreatePedestrianEncounterFieldItems(0, scenarioTemplate.Pedestrians.Encounters[0]);
	TestEqual(TEXT("Pedestrian encounter field items are created"), encounterItems.Num(), 11);
	TestEqual(TEXT("Pedestrian encounter id is formatted"), encounterItems.IsValidIndex(0) ? encounterItems[0]->GetValueText() : FString(), FString(TEXT("meet_runner")));
	TestEqual(TEXT("Pedestrian encounter type is formatted"), encounterItems.IsValidIndex(1) ? encounterItems[1]->GetValueText() : FString(), FString(TEXT("cross_path")));
	TestEqual(TEXT("Pedestrian encounter type uses combo input"), encounterItems.IsValidIndex(1) ? encounterItems[1]->GetInputType() : EScenarioEditorSidebarFieldInputType::Text, EScenarioEditorSidebarFieldInputType::ComboBox);
	TestTrue(TEXT("Pedestrian encounter type has standing group option"), encounterItems.IsValidIndex(1) && encounterItems[1]->GetComboOptions().Contains(TEXT("standing_group")));
	TestTrue(TEXT("Pedestrian encounter segment has corridor id option"), encounterItems.IsValidIndex(2) && encounterItems[2]->GetComboOptions().Contains(TEXT("narrow")));
	TestTrue(TEXT("Pedestrian encounter persona has normal option"), encounterItems.IsValidIndex(3) && encounterItems[3]->GetComboOptions().Contains(TEXT("normal")));
	TestEqual(TEXT("Pedestrian encounter meet offset min is formatted"), encounterItems.IsValidIndex(4) ? encounterItems[4]->GetMinValueText() : FString(), FString(TEXT("0.50")));
	TestTrue(TEXT("Pedestrian encounter meet offset enables range input"), encounterItems.IsValidIndex(4) && encounterItems[4]->IsRangeInputEnabled());

	scenarioTemplate.Obstacles.MinClearWidthMeters =
		UScenarioTemplateSidebarViewModel::MakeRangeTemplateNumberValue(0.8, 1.2);
	FScenarioTemplateObstaclePlacement scatterPlacement;
	scatterPlacement.PlacementId = TEXT("scatter_bins");
	scatterPlacement.Kind = EScenarioTemplateObstaclePlacementKind::Scatter;
	scatterPlacement.Zone.SegmentIds = { TEXT("north") };
	scatterPlacement.Zone.LaneIds = { TEXT("walkway") };
	scatterPlacement.DensityPer10Meters =
		UScenarioTemplateSidebarViewModel::MakeRangeTemplateNumberValue(1.0, 2.0);
	scenarioTemplate.Obstacles.Placements = { scatterPlacement };
	viewModel->RefreshObstacleFieldItemsFromTemplate(scenarioTemplate);

	FString obstacleCommandStatus;
	TestFalse(
		TEXT("Obstacle min clear width command rejects invalid number"),
		viewModel->CommitObstacleMinClearWidthText(
			FText::FromString(TEXT("not-a-number")),
			obstacleCommandStatus));
	TestTrue(
		TEXT("Obstacle invalid command reports parse status"),
		obstacleCommandStatus.Contains(TEXT("number")));

	UScenarioTemplateFieldRowViewModel* minClearWidthItem = viewModel->FindObstacleFieldItem(TEXT("MinClearWidth"));
	TestNotNull(TEXT("Obstacle min clear width field item exists"), minClearWidthItem);
	TestEqual(TEXT("Obstacle min clear width min is formatted"), minClearWidthItem ? minClearWidthItem->GetMinValueText() : FString(), FString(TEXT("0.80")));
	UScenarioTemplateFieldRowViewModel* placementsCountItem = viewModel->FindObstacleFieldItem(TEXT("PlacementsCount"));
	TestNotNull(TEXT("Obstacle placements count field item exists"), placementsCountItem);
	TestEqual(
		TEXT("Obstacle placements count uses object array input"),
		placementsCountItem ? placementsCountItem->GetInputType() : EScenarioEditorSidebarFieldInputType::Text,
		EScenarioEditorSidebarFieldInputType::ObjectArray);

	TArray<UScenarioTemplateFieldRowViewModel*> placementItems =
		viewModel->CreateObstaclePlacementFieldItems(0, scatterPlacement);
	TestEqual(TEXT("Obstacle placement field items are created"), placementItems.Num(), 18);
	TestEqual(TEXT("Obstacle placement kind is formatted"), placementItems.IsValidIndex(1) ? placementItems[1]->GetValueText() : FString(), FString(TEXT("scatter")));
	TestEqual(TEXT("Obstacle placement kind uses combo input"), placementItems.IsValidIndex(1) ? placementItems[1]->GetInputType() : EScenarioEditorSidebarFieldInputType::Text, EScenarioEditorSidebarFieldInputType::ComboBox);
	TestTrue(TEXT("Obstacle placement kind has fixed option"), placementItems.IsValidIndex(1) && placementItems[1]->GetComboOptions().Contains(TEXT("fixed")));
	TestFalse(TEXT("Obstacle fixed prop field hides for scatter"), placementItems.IsValidIndex(2) && placementItems[2]->IsFieldVisible());
	TestTrue(TEXT("Obstacle pattern has line option"), placementItems.IsValidIndex(3) && placementItems[3]->GetComboOptions().Contains(TEXT("line")));
	TestTrue(TEXT("Obstacle segment has corridor id option"), placementItems.IsValidIndex(4) && placementItems[4]->GetComboOptions().Contains(TEXT("narrow")));
	TestTrue(TEXT("Obstacle lane has walkway option"), placementItems.IsValidIndex(5) && placementItems[5]->GetComboOptions().Contains(TEXT("walkway")));
	TestEqual(
		TEXT("Obstacle zone segments use string list input"),
		placementItems.IsValidIndex(8) ? placementItems[8]->GetInputType() : EScenarioEditorSidebarFieldInputType::Text,
		EScenarioEditorSidebarFieldInputType::StringList);
	TestEqual(
		TEXT("Obstacle palette categories use string list input"),
		placementItems.IsValidIndex(10) ? placementItems[10]->GetInputType() : EScenarioEditorSidebarFieldInputType::Text,
		EScenarioEditorSidebarFieldInputType::StringList);
	TestTrue(TEXT("Obstacle scatter density field shows"), placementItems.IsValidIndex(15) && placementItems[15]->IsFieldVisible());
	TestEqual(TEXT("Obstacle scatter density min is formatted"), placementItems.IsValidIndex(15) ? placementItems[15]->GetMinValueText() : FString(), FString(TEXT("1.00")));
	TestEqual(TEXT("Obstacle allow blocking uses combo input"), placementItems.IsValidIndex(17) ? placementItems[17]->GetInputType() : EScenarioEditorSidebarFieldInputType::Text, EScenarioEditorSidebarFieldInputType::ComboBox);
	TestTrue(TEXT("Obstacle allow blocking has true option"), placementItems.IsValidIndex(17) && placementItems[17]->GetComboOptions().Contains(TEXT("true")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorOutlinerViewModelTest,
	"OdiroSim.ScenarioEditor.ViewModel.Outliner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorOutlinerViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UScenarioEditorOutlinerViewModel* viewModel = NewObject<UScenarioEditorOutlinerViewModel>();
	TestNotNull(TEXT("Outliner ViewModel is created"), viewModel);

	viewModel->RefreshTemplatePanels();
	TArray<UScenarioEditorListItemViewModel*> items = viewModel->GetItems();
	TestEqual(TEXT("Template panel rows are created"), items.Num(), 4);

	UScenarioEditorListItemViewModel* corridorItem = items.IsValidIndex(1) ? items[1] : nullptr;
	TestNotNull(TEXT("Corridor item exists"), corridorItem);
	TestTrue(TEXT("Select item succeeds without subsystem"), viewModel->SelectItem(corridorItem));
	TestEqual(TEXT("Selected item key updates"), viewModel->GetSelectedItemKey(), corridorItem ? corridorItem->GetItemId() : FString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorAssetPaletteViewModelTest,
	"OdiroSim.ScenarioEditor.ViewModel.AssetPalette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorAssetPaletteViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UScenarioAssetPaletteViewModel* viewModel = NewObject<UScenarioAssetPaletteViewModel>();
	TestNotNull(TEXT("Asset palette ViewModel is created"), viewModel);

	FScenarioPaletteItemEntry entry;
	entry.AssetId = FName(TEXT("Bench"));
	entry.DisplayName = FText::FromString(TEXT("Bench"));
	entry.CategoryText = FText::FromString(TEXT("Static"));
	entry.ItemType = EScenarioPaletteItemType::StaticObstacle;

	TArray<FScenarioPaletteItemEntry> entries;
	entries.Add(entry);
	viewModel->SetPaletteEntries(entries);
	viewModel->SelectAsset(entry.AssetId);

	TestEqual(TEXT("Palette item is created"), viewModel->GetItems().Num(), 1);
	TestEqual(TEXT("Selected asset updates"), viewModel->GetSelectedAssetId(), entry.AssetId);
	TestTrue(TEXT("Palette item selection updates"), viewModel->GetItems()[0]->IsSelected());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEditorLlmPromptViewModelTest,
	"OdiroSim.ScenarioEditor.ViewModel.LlmPrompt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEditorLlmPromptViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UScenarioLlmPromptViewModel* viewModel = NewObject<UScenarioLlmPromptViewModel>();
	TestNotNull(TEXT("LLM prompt ViewModel is created"), viewModel);

	viewModel->SetPromptText(TEXT("Create a quiet delivery scenario."));
	viewModel->SetEpisodeCount(0);
	viewModel->SetProjectScenarioJsonPath(TEXT(""));

	TestEqual(TEXT("Prompt updates"), viewModel->GetPromptText(), FString(TEXT("Create a quiet delivery scenario.")));
	TestEqual(TEXT("Episode count clamps"), viewModel->GetEpisodeCount(), 1);
	TestFalse(TEXT("Request fails without project path"), viewModel->RequestGeneration());
	TestTrue(TEXT("Failure status explains missing path"), viewModel->GetStatusText().Contains(TEXT("path")));

	return true;
}

#endif
