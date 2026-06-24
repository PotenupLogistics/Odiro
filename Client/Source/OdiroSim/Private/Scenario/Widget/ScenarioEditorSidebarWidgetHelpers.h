#pragma once

#include "CoreMinimal.h"

class UScenarioEditorSidebarBlockWidget;
class UScenarioEditorSidebarFieldRow;
class UScenarioEditorWidgetClassCatalog;
class UScenarioTemplateFieldRowViewModel;
class UWidgetTextStyleCatalog;
class UWorld;

namespace ScenarioEditorSidebarWidgetHelpers
{
	// Shared metadata used to initialize one sidebar block header.
	struct FSidebarBlockConfig
	{
		// User-facing block name.
		FString Name;

		// Stable scenario template path represented by the block.
		FString Path;

		// Header badge text shown beside the path.
		FString Badge;

		// Whether the block should use selected visual treatment.
		bool bSelected = false;

		// Whether the block is nested under another sidebar block.
		bool bNested = false;

		// Whether unselected normal outline should be visible.
		bool bShowNormalOutline = true;
	};

	// Applies standard metadata, nesting, selection, and style catalog to a sidebar block.
	void ConfigureBlock(
		UScenarioEditorSidebarBlockWidget* blockWidget,
		TSoftObjectPtr<UWidgetTextStyleCatalog> textStyleCatalog,
		const FSidebarBlockConfig& config);

	// Applies standard style and item data to a field row.
	void InitializeFieldRow(
		UScenarioEditorSidebarFieldRow* fieldRow,
		TSoftObjectPtr<UWidgetTextStyleCatalog> textStyleCatalog,
		UScenarioTemplateFieldRowViewModel* fieldItemViewModel);

	// Creates a standard sidebar field row and attaches it to the parent block body.
	UScenarioEditorSidebarFieldRow* CreateFieldRow(
		UWorld* world,
		TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> widgetClassCatalog,
		TSoftObjectPtr<UWidgetTextStyleCatalog> textStyleCatalog,
		UScenarioTemplateFieldRowViewModel* fieldItemViewModel,
		UScenarioEditorSidebarBlockWidget* parentBlockWidget);

	// Binds no-argument add/remove row actions to one listener.
	void BindFieldRowActions(
		UScenarioEditorSidebarFieldRow* fieldRow,
		UObject* listener,
		const FName addFunctionName,
		const FName removeFunctionName);

	// Removes action bindings from one listener on a field row.
	void UnbindFieldRowActions(
		UScenarioEditorSidebarFieldRow* fieldRow,
		UObject* listener);
}
