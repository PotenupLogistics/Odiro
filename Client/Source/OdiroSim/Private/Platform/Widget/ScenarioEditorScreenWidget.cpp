#include "Platform/Widget/ScenarioEditorScreenWidget.h"

#include "Scenario/Widget/ScenarioEditorRootWidget.h"

void UScenarioEditorScreenWidget::SaveCurrentScenario()
{
	if (ScenarioEditorRootWidget)
	{
		ScenarioEditorRootWidget->SaveCurrentScenario();
	}
}
