#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"

#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

void ScenarioEditorSidebarWidgetHelpers::ConfigureBlock(
	UScenarioEditorSidebarBlockWidget* blockWidget,
	TSoftObjectPtr<UWidgetTextStyleCatalog> textStyleCatalog,
	const FSidebarBlockConfig& config)
{
	if (!blockWidget)
	{
		return;
	}

	blockWidget->SetTextStyleCatalog(textStyleCatalog);
	blockWidget->SetBlockMetadata(config.Name, config.Path, config.Badge);
	blockWidget->SetSelected(config.bSelected);
	blockWidget->SetNested(config.bNested);
	blockWidget->SetShowNormalOutline(config.bShowNormalOutline);
}

FString ScenarioEditorSidebarWidgetHelpers::MakeIndexedBlockPath(
	const FString& listPath,
	const int32 itemIndex)
{
	return itemIndex >= 0
		? FString::Printf(TEXT("%s[%d]"), *listPath, itemIndex)
		: listPath;
}

void ScenarioEditorSidebarWidgetHelpers::ApplySelectedBlockPath(
	UScenarioEditorSidebarBlockWidget* blockWidget,
	const FString& selectedBlockPath)
{
	if (!blockWidget)
	{
		return;
	}

	blockWidget->SetSelected(!selectedBlockPath.IsEmpty() && blockWidget->BlockPath == selectedBlockPath);
}

void ScenarioEditorSidebarWidgetHelpers::InitializeFieldRow(
	UScenarioEditorSidebarFieldRow* fieldRow,
	TSoftObjectPtr<UWidgetTextStyleCatalog> textStyleCatalog,
	UScenarioTemplateFieldRowViewModel* fieldItemViewModel)
{
	if (!fieldRow)
	{
		return;
	}

	fieldRow->SetTextStyleCatalog(textStyleCatalog);
	fieldRow->InitializeFromItemViewModel(fieldItemViewModel);
}

UScenarioEditorSidebarFieldRow* ScenarioEditorSidebarWidgetHelpers::CreateFieldRow(
	UWorld* world,
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> widgetClassCatalog,
	TSoftObjectPtr<UWidgetTextStyleCatalog> textStyleCatalog,
	UScenarioTemplateFieldRowViewModel* fieldItemViewModel,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!world || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow = CreateWidget<UScenarioEditorSidebarFieldRow>(
		world,
		UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(widgetClassCatalog));
	if (!fieldRow)
	{
		return nullptr;
	}

	InitializeFieldRow(fieldRow, textStyleCatalog, fieldItemViewModel);
	parentBlockWidget->AddBodyChild(fieldRow);
	return fieldRow;
}

void ScenarioEditorSidebarWidgetHelpers::BindFieldRowActions(
	UScenarioEditorSidebarFieldRow* fieldRow,
	UObject* listener,
	const FName addFunctionName,
	const FName removeFunctionName)
{
	if (!fieldRow || !listener || addFunctionName.IsNone() || removeFunctionName.IsNone())
	{
		return;
	}

	FScriptDelegate addDelegate;
	addDelegate.BindUFunction(listener, addFunctionName);
	fieldRow->OnAddItemRequested.Remove(addDelegate);
	fieldRow->OnAddItemRequested.Add(addDelegate);

	FScriptDelegate removeDelegate;
	removeDelegate.BindUFunction(listener, removeFunctionName);
	fieldRow->OnRemoveItemRequested.Remove(removeDelegate);
	fieldRow->OnRemoveItemRequested.Add(removeDelegate);
}

void ScenarioEditorSidebarWidgetHelpers::UnbindFieldRowActions(
	UScenarioEditorSidebarFieldRow* fieldRow,
	UObject* listener)
{
	if (!fieldRow || !listener)
	{
		return;
	}

	fieldRow->OnAddItemRequested.RemoveAll(listener);
	fieldRow->OnRemoveItemRequested.RemoveAll(listener);
}
