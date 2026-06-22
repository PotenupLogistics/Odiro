#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"

#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorLaneWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorSegmentWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianEncounterWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultScenarioEditorWidgetClassCatalogPath(
		TEXT("/Game/Data/UI/DA_ScenarioEditorWidgetClassCatalog.DA_ScenarioEditorWidgetClassCatalog"));

	template <typename TWidget>
	TSubclassOf<TWidget> LoadWidgetClass(const TCHAR* classPath)
	{
		return LoadClass<TWidget>(nullptr, classPath);
	}

	template <typename TWidget>
	TSubclassOf<TWidget> ResolveCatalogClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference,
		TSubclassOf<TWidget> UScenarioEditorWidgetClassCatalog::*member,
		const TCHAR* defaultClassPath)
	{
		if (const UScenarioEditorWidgetClassCatalog* catalog = catalogReference.LoadSynchronous())
		{
			if (catalog->*member)
			{
				return catalog->*member;
			}
		}

		TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> defaultCatalog =
			UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
		if (defaultCatalog.ToSoftObjectPath() != catalogReference.ToSoftObjectPath())
		{
			if (const UScenarioEditorWidgetClassCatalog* catalog = defaultCatalog.LoadSynchronous())
			{
				if (catalog->*member)
				{
					return catalog->*member;
				}
			}
		}

		return LoadWidgetClass<TWidget>(defaultClassPath);
	}
}

TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>
UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>(DefaultScenarioEditorWidgetClassCatalogPath);
}

TSubclassOf<UScenarioEditorOutlinerRowWidget>
UScenarioEditorWidgetClassCatalog::ResolveOutlinerRowWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::OutlinerRowWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorOutlinerRow.WBP_ScenarioEditorOutlinerRow_C"));
}

TSubclassOf<UScenarioEditorSidebarBlockWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarBlockWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarBlockWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarBlock.WBP_ScenarioEditorSidebarBlock_C"));
}

TSubclassOf<UScenarioEditorSidebarFieldRow>
UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarFieldRowWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarFieldRow.WBP_ScenarioEditorSidebarFieldRow_C"));
}

TSubclassOf<UScenarioEditorSidebarMainPanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarMainPanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarMainPanelWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarMainPanel.WBP_ScenarioEditorSidebarMainPanel_C"));
}

TSubclassOf<UScenarioEditorSidebarCorridorPanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorPanelWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarCorridorPanel.WBP_ScenarioEditorSidebarCorridorPanel_C"));
}

TSubclassOf<UScenarioEditorSidebarCorridorPointWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPointWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorPointWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarCorridorPoint.WBP_ScenarioEditorSidebarCorridorPoint_C"));
}

TSubclassOf<UScenarioEditorSidebarCorridorLaneWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorLaneWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorLaneWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarCorridorLane.WBP_ScenarioEditorSidebarCorridorLane_C"));
}

TSubclassOf<UScenarioEditorSidebarCorridorSegmentWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorSegmentWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorSegmentWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarCorridorSegment.WBP_ScenarioEditorSidebarCorridorSegment_C"));
}

TSubclassOf<UScenarioEditorSidebarObstaclePanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarObstaclePanelWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarObstaclePanel.WBP_ScenarioEditorSidebarObstaclePanel_C"));
}

TSubclassOf<UScenarioEditorSidebarObstaclePlacementWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePlacementWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarObstaclePlacementWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarObstaclePlacement.WBP_ScenarioEditorSidebarObstaclePlacement_C"));
}

TSubclassOf<UScenarioEditorSidebarPedestrianPanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianPanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarPedestrianPanelWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarPedestrianPanel.WBP_ScenarioEditorSidebarPedestrianPanel_C"));
}

TSubclassOf<UScenarioEditorSidebarPedestrianEncounterWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianEncounterWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarPedestrianEncounterWidgetClass,
		TEXT("/Game/Widgets/Editor/WBP_ScenarioEditorSidebarPedestrianEncounter.WBP_ScenarioEditorSidebarPedestrianEncounter_C"));
}
