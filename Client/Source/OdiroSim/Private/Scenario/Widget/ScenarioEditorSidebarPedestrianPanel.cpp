#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace
{
}

void UScenarioEditorSidebarPedestrianPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	ConfigureFieldRows();
	RefreshFromDraft();
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
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(TEXT("ScenarioAuthoringSubsystem unavailable.")));
		}
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenario());
}

void UScenarioEditorSidebarPedestrianPanel::RefreshFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	const FScenarioTemplatePedestrianRules& pedestrians = scenarioTemplate.Pedestrians;
	SetIntegerRowValue(BackgroundCountFieldRow.Get(), pedestrians.Background.Count);
	SetNumberRowValue(BackgroundSpeedFieldRow.Get(), pedestrians.Background.SpeedMetersPerSecond);
	if (SpawnSegmentsFieldRow)
	{
		SpawnSegmentsFieldRow->SetValueText(JoinStringList(pedestrians.Background.SpawnSegmentIds));
	}

	RefreshEncounterRows(pedestrians.Encounters);
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(TEXT("Structure only: Pedestrian edits are not committed yet.")));
	}
}

void UScenarioEditorSidebarPedestrianPanel::ConfigureFieldRows()
{
	if (PedestriansBlockWidget)
	{
		PedestriansBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PedestriansBlockWidget->SetBlockMetadata(TEXT("pedestrians"), TEXT("root.pedestrians"), TEXT("Template"));
		PedestriansBlockWidget->SetSelected(true);
	}
	if (BackgroundBlockWidget)
	{
		BackgroundBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		BackgroundBlockWidget->SetBlockMetadata(TEXT("background"), TEXT("root.pedestrians.background"), TEXT("Property"));
		BackgroundBlockWidget->SetNested(true);
		BackgroundBlockWidget->SetShowNormalOutline(false);
	}
	if (SpawnZoneBlockWidget)
	{
		SpawnZoneBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		SpawnZoneBlockWidget->SetBlockMetadata(
			TEXT("spawn_zone"),
			TEXT("root.pedestrians.background.spawn_zone"),
			TEXT("Detail"));
		SpawnZoneBlockWidget->SetNested(true);
		SpawnZoneBlockWidget->SetShowNormalOutline(false);
	}
	if (EncountersBlockWidget)
	{
		EncountersBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		EncountersBlockWidget->SetBlockMetadata(TEXT("encounters"), TEXT("root.pedestrians.encounters[]"), TEXT("Property"));
		EncountersBlockWidget->SetNested(true);
		EncountersBlockWidget->SetShowNormalOutline(false);
	}
	if (BackgroundCountFieldRow)
	{
		BackgroundCountFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		BackgroundCountFieldRow->SetFieldLabel(TEXT("count"));
		BackgroundCountFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
		BackgroundCountFieldRow->SetEditable(true);
	}
	if (BackgroundSpeedFieldRow)
	{
		BackgroundSpeedFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		BackgroundSpeedFieldRow->SetFieldLabel(TEXT("speed_mps"));
		BackgroundSpeedFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
		BackgroundSpeedFieldRow->SetEditable(true);
	}
	if (SpawnSegmentsFieldRow)
	{
		SpawnSegmentsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		SpawnSegmentsFieldRow->SetFieldLabel(TEXT("segments"));
		SpawnSegmentsFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		SpawnSegmentsFieldRow->SetEditable(true);
		SpawnSegmentsFieldRow->SetArrayControlsEnabled(true);
	}
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
		TEXT("count"),
		FString::FromInt(encounters.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false,
		true);

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
	const FString& label,
	const FString& value,
	const EScenarioEditorSidebarFieldInputType inputType,
	const bool bEditable,
	const bool bArrayControlsEnabled) const
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
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
	fieldRow->SetFieldLabel(label);
	fieldRow->SetValueText(value);
	fieldRow->SetInputType(inputType);
	fieldRow->SetEditable(bEditable);
	fieldRow->SetArrayControlsEnabled(bArrayControlsEnabled);
	parentBlockWidget->AddBodyChild(fieldRow);
	return fieldRow;
}

UScenarioEditorSidebarPedestrianEncounterWidget* UScenarioEditorSidebarPedestrianPanel::AddEncounterWidget(
	const int32 encounterIndex,
	const FScenarioTemplatePedestrianEncounter& encounter,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarPedestrianEncounterWidget>(
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

UScenarioAuthoringSubsystem* UScenarioEditorSidebarPedestrianPanel::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

void UScenarioEditorSidebarPedestrianPanel::SetNumberRowValue(
	UScenarioEditorSidebarFieldRow* fieldRow,
	const FScenarioTemplateNumberValue& value)
{
	if (!fieldRow)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldRow->SetValueText(FString());
		fieldRow->SetRangeValueText(FString(), FString());
		fieldRow->SetRangeInputEnabled(false);
		return;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		fieldRow->SetValueText(FormatEditableNumber((value.MinValue + value.MaxValue) * 0.5));
		fieldRow->SetRangeValueText(FormatEditableNumber(value.MinValue), FormatEditableNumber(value.MaxValue));
		fieldRow->SetRangeInputEnabled(true);
		return;
	}
	fieldRow->SetValueText(FormatEditableNumber(value.FixedValue));
	fieldRow->SetRangeValueText(FormatEditableNumber(value.FixedValue), FormatEditableNumber(value.FixedValue));
	fieldRow->SetRangeInputEnabled(false);
}

void UScenarioEditorSidebarPedestrianPanel::SetIntegerRowValue(
	UScenarioEditorSidebarFieldRow* fieldRow,
	const FScenarioTemplateIntegerValue& value)
{
	if (!fieldRow)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldRow->SetValueText(FString());
		fieldRow->SetRangeValueText(FString(), FString());
		fieldRow->SetRangeInputEnabled(false);
		return;
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		fieldRow->SetValueText(FormatEditableInteger(FMath::RoundToInt((value.MinValue + value.MaxValue) * 0.5f)));
		fieldRow->SetRangeValueText(FormatEditableInteger(value.MinValue), FormatEditableInteger(value.MaxValue));
		fieldRow->SetRangeInputEnabled(true);
		return;
	}
	fieldRow->SetValueText(FormatEditableInteger(value.FixedValue));
	fieldRow->SetRangeValueText(FormatEditableInteger(value.FixedValue), FormatEditableInteger(value.FixedValue));
	fieldRow->SetRangeInputEnabled(false);
}

FString UScenarioEditorSidebarPedestrianPanel::JoinStringList(const TArray<FString>& values)
{
	return FString::Join(values, TEXT(", "));
}

FString UScenarioEditorSidebarPedestrianPanel::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

FString UScenarioEditorSidebarPedestrianPanel::FormatEditableInteger(const int32 value)
{
	return FString::FromInt(value);
}
