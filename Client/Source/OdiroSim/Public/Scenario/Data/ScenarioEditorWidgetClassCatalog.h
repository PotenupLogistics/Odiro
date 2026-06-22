#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "ScenarioEditorWidgetClassCatalog.generated.h"

class UScenarioEditorOutlinerRowWidget;
class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorSidebarCorridorLaneWidget;
class UScenarioEditorSidebarCorridorPanel;
class UScenarioEditorSidebarCorridorPointWidget;
class UScenarioEditorSidebarCorridorSegmentWidget;
class UScenarioEditorSidebarFieldRow;
class UScenarioEditorSidebarMainPanel;
class UScenarioEditorSidebarObstaclePanel;
class UScenarioEditorSidebarObstaclePlacementWidget;
class UScenarioEditorSidebarPedestrianEncounterWidget;
class UScenarioEditorSidebarPedestrianPanel;

// Widget Blueprint class catalog used by Scenario Editor C++ renderers.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorWidgetClassCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// Default asset location for Scenario Editor WBP class references.
	static TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> MakeDefaultCatalogReference();

	// Resolves the configured outliner row WBP class.
	static TSubclassOf<UScenarioEditorOutlinerRowWidget> ResolveOutlinerRowWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured sidebar block WBP class.
	static TSubclassOf<UScenarioEditorSidebarBlockWidget> ResolveSidebarBlockWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured sidebar field row WBP class.
	static TSubclassOf<UScenarioEditorSidebarFieldRow> ResolveSidebarFieldRowWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured main detail panel WBP class.
	static TSubclassOf<UScenarioEditorSidebarMainPanel> ResolveSidebarMainPanelWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured corridor detail panel WBP class.
	static TSubclassOf<UScenarioEditorSidebarCorridorPanel> ResolveSidebarCorridorPanelWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured corridor point row WBP class.
	static TSubclassOf<UScenarioEditorSidebarCorridorPointWidget> ResolveSidebarCorridorPointWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured corridor lane row WBP class.
	static TSubclassOf<UScenarioEditorSidebarCorridorLaneWidget> ResolveSidebarCorridorLaneWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured corridor segment row WBP class.
	static TSubclassOf<UScenarioEditorSidebarCorridorSegmentWidget> ResolveSidebarCorridorSegmentWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured obstacle detail panel WBP class.
	static TSubclassOf<UScenarioEditorSidebarObstaclePanel> ResolveSidebarObstaclePanelWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured obstacle placement row WBP class.
	static TSubclassOf<UScenarioEditorSidebarObstaclePlacementWidget> ResolveSidebarObstaclePlacementWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured pedestrian detail panel WBP class.
	static TSubclassOf<UScenarioEditorSidebarPedestrianPanel> ResolveSidebarPedestrianPanelWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// Resolves the configured pedestrian encounter row WBP class.
	static TSubclassOf<UScenarioEditorSidebarPedestrianEncounterWidget> ResolveSidebarPedestrianEncounterWidgetClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference);

	// WBP class for a single scenario outliner row.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorOutlinerRowWidget> OutlinerRowWidgetClass;

	// WBP class for a reusable detail block.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarBlockWidget> SidebarBlockWidgetClass;

	// WBP class for a reusable detail field row.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarFieldRow> SidebarFieldRowWidgetClass;

	// WBP class for the scenario root detail panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarMainPanel> SidebarMainPanelWidgetClass;

	// WBP class for the corridor detail panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarCorridorPanel> SidebarCorridorPanelWidgetClass;

	// WBP class for a corridor point entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarCorridorPointWidget> SidebarCorridorPointWidgetClass;

	// WBP class for a corridor lane entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarCorridorLaneWidget> SidebarCorridorLaneWidgetClass;

	// WBP class for a corridor segment entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarCorridorSegmentWidget> SidebarCorridorSegmentWidgetClass;

	// WBP class for the obstacle detail panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarObstaclePanel> SidebarObstaclePanelWidgetClass;

	// WBP class for one obstacle placement entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarObstaclePlacementWidget> SidebarObstaclePlacementWidgetClass;

	// WBP class for the pedestrian detail panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarPedestrianPanel> SidebarPedestrianPanelWidgetClass;

	// WBP class for one pedestrian encounter entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Widgets")
	TSubclassOf<UScenarioEditorSidebarPedestrianEncounterWidget> SidebarPedestrianEncounterWidgetClass;
};
