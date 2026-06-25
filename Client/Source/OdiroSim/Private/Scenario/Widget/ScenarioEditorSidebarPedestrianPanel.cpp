#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace SidebarWidgetHelpers = ScenarioEditorSidebarWidgetHelpers;

void UScenarioEditorSidebarPedestrianPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	SidebarWidgetHelpers::ApplyPanelRootPadding(this, FName(TEXT("PedestrianPanelRootBox")));
	ConfigureFieldRows();
	RefreshFromDraft();

	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectBlockWidgets(blockWidgets);
	SidebarWidgetHelpers::ApplyPanelBlockSpacing(blockWidgets);
}

void UScenarioEditorSidebarPedestrianPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarPedestrianPanel::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarPedestrianPanel::RefreshFromDraft()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!templateSidebarViewModel || !templateSidebarViewModel->TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(
				failureReason.IsEmpty() ? TEXT("ScenarioTemplateSidebarViewModel unavailable.") : failureReason));
		}
		return;
	}

	RefreshFromTemplate(scenarioTemplate);
}

void UScenarioEditorSidebarPedestrianPanel::RefreshFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(TEXT("ScenarioTemplateSidebarViewModel unavailable.")));
		}
		return;
	}

	templateSidebarViewModel->RefreshPedestrianFieldItemsFromTemplate(scenarioTemplate);
	ApplyPedestrianFieldItems();
	RefreshEncounterRows(scenarioTemplate.Pedestrians.Encounters);
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(TEXT("Structure only: Pedestrian edits are not committed yet.")));
	}
	ApplySelectedBlockPath();
}

void UScenarioEditorSidebarPedestrianPanel::ApplySelectedBlockPath()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	const UScenarioEditorShellViewModel* shellViewModel = uiSubsystem ? uiSubsystem->GetShellViewModel() : nullptr;
	const FString selectedBlockPath = shellViewModel ? shellViewModel->GetSelectedTemplateBlockPath() : FString();

	SidebarWidgetHelpers::ApplySelectedBlockPath(PedestriansBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(BackgroundBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(SpawnZoneBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(EncountersBlockWidget.Get(), selectedBlockPath);

	if (EncountersBlockWidget && selectedBlockPath.StartsWith(TEXT("root.pedestrians.encounters[")))
	{
		EncountersBlockWidget->SetExpanded(true);
	}

	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(encounterWidget->EncounterBlockWidget.Get(), selectedBlockPath);
		}
	}
}

void UScenarioEditorSidebarPedestrianPanel::CollectBlockWidgets(
	TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		PedestriansBlockWidget.Get(),
		BackgroundBlockWidget.Get(),
		SpawnZoneBlockWidget.Get(),
		EncountersBlockWidget.Get() })
	{
		if (blockWidget)
		{
			outBlockWidgets.Add(blockWidget);
		}
	}
	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget && encounterWidget->EncounterBlockWidget)
		{
			outBlockWidgets.Add(encounterWidget->EncounterBlockWidget.Get());
		}
	}
}

UScenarioEditorSidebarBlockWidget* UScenarioEditorSidebarPedestrianPanel::FindBlockWidgetByPath(
	const FString& blockPath) const
{
	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectBlockWidgets(blockWidgets);
	for (UScenarioEditorSidebarBlockWidget* blockWidget : blockWidgets)
	{
		if (blockWidget && blockWidget->BlockPath == blockPath)
		{
			return blockWidget;
		}
	}
	return nullptr;
}

void UScenarioEditorSidebarPedestrianPanel::ConfigureFieldRows()
{
	if (PedestriansBlockWidget)
	{
		PedestriansBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PedestriansBlockWidget->SetBlockMetadata(TEXT("보행자"), TEXT("root.pedestrians"), TEXT("구성"));
		PedestriansBlockWidget->SetSelected(true);
	}
	if (BackgroundBlockWidget)
	{
		BackgroundBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		BackgroundBlockWidget->SetBlockMetadata(TEXT("배경 보행자"), TEXT("root.pedestrians.background"), TEXT("속성"));
		BackgroundBlockWidget->SetNested(true);
		BackgroundBlockWidget->SetShowNormalOutline(false);
	}
	if (SpawnZoneBlockWidget)
	{
		SpawnZoneBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		SpawnZoneBlockWidget->SetBlockMetadata(
			TEXT("스폰 구역"),
			TEXT("root.pedestrians.background.spawn_zone"),
			TEXT("세부"));
		SpawnZoneBlockWidget->SetNested(true);
		SpawnZoneBlockWidget->SetShowNormalOutline(false);
	}
	if (EncountersBlockWidget)
	{
		EncountersBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		EncountersBlockWidget->SetBlockMetadata(TEXT("상호작용 상황"), TEXT("root.pedestrians.encounters[]"), TEXT("속성"));
		EncountersBlockWidget->SetNested(true);
		EncountersBlockWidget->SetShowNormalOutline(false);
	}
	if (BackgroundCountFieldRow)
	{
		BackgroundCountFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (BackgroundSpeedFieldRow)
	{
		BackgroundSpeedFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (SpawnSegmentsFieldRow)
	{
		SpawnSegmentsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	ApplyPedestrianFieldItems();
}

void UScenarioEditorSidebarPedestrianPanel::ApplyTextStyles()
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		PedestriansBlockWidget.Get(),
		BackgroundBlockWidget.Get(),
		SpawnZoneBlockWidget.Get(),
		EncountersBlockWidget.Get() })
	{
		if (blockWidget)
		{
			blockWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		BackgroundCountFieldRow.Get(),
		BackgroundSpeedFieldRow.Get(),
		SpawnSegmentsFieldRow.Get(),
		EncountersCountFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget)
		{
			encounterWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(DiagnosticsTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UScenarioEditorSidebarPedestrianPanel::ApplyPedestrianFieldItems()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	if (BackgroundCountFieldRow)
	{
		BackgroundCountFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("BackgroundCount")));
	}
	if (BackgroundSpeedFieldRow)
	{
		BackgroundSpeedFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("BackgroundSpeed")));
	}
	if (SpawnSegmentsFieldRow)
	{
		SpawnSegmentsFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("SpawnSegments")));
	}
	if (EncountersCountFieldRow)
	{
		EncountersCountFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("EncountersCount")));
	}
}

void UScenarioEditorSidebarPedestrianPanel::RefreshEncounterRows(
	const TArray<FScenarioTemplatePedestrianEncounter>& encounters)
{
	if (!EncountersBlockWidget)
	{
		return;
	}

	EncounterWidgets.Reset();
	EncountersBlockWidget->ClearBodyChildren();
	EncountersCountFieldRow = AddFieldRow(
		EncountersBlockWidget.Get(),
		GetTemplateSidebarViewModel()
			? GetTemplateSidebarViewModel()->FindPedestrianFieldItem(TEXT("EncountersCount"))
			: nullptr);

	for (int32 encounterIndex = 0; encounterIndex < encounters.Num(); ++encounterIndex)
	{
		if (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget =
			AddEncounterWidget(encounterIndex, encounters[encounterIndex], EncountersBlockWidget.Get()))
		{
			EncounterWidgets.Add(encounterWidget);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarPedestrianPanel::AddFieldRow(
	UScenarioEditorSidebarBlockWidget* parentBlockWidget,
	UScenarioTemplateFieldRowViewModel* fieldItemViewModel) const
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow =
		CreateWidget<UScenarioEditorSidebarFieldRow>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(WidgetClassCatalog));
	if (!fieldRow)
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(TEXT("Scenario editor field row widget class is missing.")));
		}
		return nullptr;
	}

	fieldRow->SetTextStyleCatalog(TextStyleCatalog);
	fieldRow->InitializeFromItemViewModel(fieldItemViewModel);
	parentBlockWidget->AddBodyChild(fieldRow);
	return fieldRow;
}

UScenarioEditorSidebarPedestrianEncounterWidget* UScenarioEditorSidebarPedestrianPanel::AddEncounterWidget(
	const int32 encounterIndex,
	const FScenarioTemplatePedestrianEncounter& encounter,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget =
		CreateWidget<UScenarioEditorSidebarPedestrianEncounterWidget>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianEncounterWidgetClass(WidgetClassCatalog));
	if (!encounterWidget)
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(TEXT("Scenario editor pedestrian encounter widget class is missing.")));
		}
		return nullptr;
	}

	encounterWidget->SetTextStyleCatalog(TextStyleCatalog);
	encounterWidget->SetEncounterIndex(encounterIndex);
	encounterWidget->RefreshFromEncounter(encounter);
	parentBlockWidget->AddBodyChild(encounterWidget);
	return encounterWidget;
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarPedestrianPanel::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}
