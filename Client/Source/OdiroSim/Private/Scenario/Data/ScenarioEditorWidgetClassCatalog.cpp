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

DEFINE_LOG_CATEGORY_STATIC(LogScenarioEditorWidgetClassCatalog, Log, All);

namespace
{
	const FSoftObjectPath DefaultScenarioEditorWidgetClassCatalogPath(
		TEXT("/Game/Data/UI/DA_ScenarioEditorWidgetClassCatalog.DA_ScenarioEditorWidgetClassCatalog"));

	template <typename TWidget>
	TSubclassOf<TWidget> ResolveCatalogClass(
		const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference,
		TSubclassOf<TWidget> UScenarioEditorWidgetClassCatalog::*member,
		const TCHAR* classLabel)
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

		UE_LOG(
			LogScenarioEditorWidgetClassCatalog,
			Warning,
			TEXT("Scenario editor widget class is not configured in the widget class catalog: %s"),
			classLabel);
		return nullptr;
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
		TEXT("OutlinerRowWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarBlockWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarBlockWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarBlockWidgetClass,
		TEXT("SidebarBlockWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarFieldRow>
UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarFieldRowWidgetClass,
		TEXT("SidebarFieldRowWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarMainPanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarMainPanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarMainPanelWidgetClass,
		TEXT("SidebarMainPanelWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarCorridorPanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorPanelWidgetClass,
		TEXT("SidebarCorridorPanelWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarCorridorPointWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPointWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorPointWidgetClass,
		TEXT("SidebarCorridorPointWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarCorridorLaneWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorLaneWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorLaneWidgetClass,
		TEXT("SidebarCorridorLaneWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarCorridorSegmentWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorSegmentWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarCorridorSegmentWidgetClass,
		TEXT("SidebarCorridorSegmentWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarObstaclePanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarObstaclePanelWidgetClass,
		TEXT("SidebarObstaclePanelWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarObstaclePlacementWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePlacementWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarObstaclePlacementWidgetClass,
		TEXT("SidebarObstaclePlacementWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarPedestrianPanel>
UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianPanelWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarPedestrianPanelWidgetClass,
		TEXT("SidebarPedestrianPanelWidgetClass"));
}

TSubclassOf<UScenarioEditorSidebarPedestrianEncounterWidget>
UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianEncounterWidgetClass(
	const TSoftObjectPtr<UScenarioEditorWidgetClassCatalog>& catalogReference)
{
	return ResolveCatalogClass(
		catalogReference,
		&UScenarioEditorWidgetClassCatalog::SidebarPedestrianEncounterWidgetClass,
		TEXT("SidebarPedestrianEncounterWidgetClass"));
}
