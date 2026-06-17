#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float PedestrianPanelBlockPadding = 10.0f;

	void AddPedestrianPanelWidgetToBox(
		UVerticalBox* box,
		UWidget* widget,
		const FMargin& padding = FMargin())
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(padding);
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	UVerticalBox* AddPedestrianPanelBlockWidget(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		TObjectPtr<UScenarioEditorSidebarBlockWidget>& blockWidget,
		const TCHAR* widgetName,
		const FString& name,
		const FString& path,
		const FString& badge,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog,
		const bool bHighlighted = false,
		const bool bNested = false,
		const bool bExpanded = true,
		const bool bShowNormalOutline = true)
	{
		if (!widgetTree || !parent)
		{
			return nullptr;
		}

		blockWidget = widgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
			UScenarioEditorSidebarBlockWidget::StaticClass(),
			FName(widgetName));
		if (!blockWidget)
		{
			return nullptr;
		}

		blockWidget->SetTextStyleCatalog(catalog);
		blockWidget->SetBlockMetadata(name, path, badge);
		blockWidget->SetSelected(bHighlighted);
		blockWidget->SetNested(bNested);
		blockWidget->SetExpanded(bExpanded);
		blockWidget->SetShowNormalOutline(bShowNormalOutline);
		AddPedestrianPanelWidgetToBox(
			parent,
			blockWidget.Get(),
			FMargin(0.0f, 0.0f, 0.0f, PedestrianPanelBlockPadding));
		return blockWidget->GetBodyBox();
	}

	void AddPedestrianPanelFieldRow(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		TObjectPtr<UScenarioEditorSidebarFieldRow>& fieldRow,
		const TCHAR* widgetName)
	{
		if (!widgetTree || !parent)
		{
			return;
		}

		fieldRow = widgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
			UScenarioEditorSidebarFieldRow::StaticClass(),
			FName(widgetName));
		if (!fieldRow)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = parent->AddChildToVerticalBox(fieldRow.Get()))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarPedestrianPanel::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarPedestrianPanel::NativeConstruct()
{
	Super::NativeConstruct();
	ConfigureFieldRows();
	RefreshFromDraft();
}

void UScenarioEditorSidebarPedestrianPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
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

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarPedestrianPanel::RefreshFromTemplate(
	const FScenarioTemplateDocument& scenarioTemplate)
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

void UScenarioEditorSidebarPedestrianPanel::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedPedestrianPanelRoot"));
	if (!rootBox)
	{
		return;
	}

	WidgetTree->RootWidget = rootBox;

	UVerticalBox* pedestriansBody = AddPedestrianPanelBlockWidget(
		WidgetTree,
		rootBox,
		PedestriansBlockWidget,
		TEXT("PedestriansBlockWidget"),
		TEXT("pedestrians"),
		TEXT("root.pedestrians"),
		TEXT("Template"),
		TextStyleCatalog,
		true);
	UVerticalBox* backgroundBody = AddPedestrianPanelBlockWidget(
		WidgetTree,
		pedestriansBody,
		BackgroundBlockWidget,
		TEXT("BackgroundBlockWidget"),
		TEXT("background"),
		TEXT("root.pedestrians.background"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddPedestrianPanelFieldRow(WidgetTree, backgroundBody, BackgroundCountFieldRow, TEXT("BackgroundCountFieldRow"));
	AddPedestrianPanelFieldRow(WidgetTree, backgroundBody, BackgroundSpeedFieldRow, TEXT("BackgroundSpeedFieldRow"));

	UVerticalBox* spawnZoneBody = AddPedestrianPanelBlockWidget(
		WidgetTree,
		backgroundBody,
		SpawnZoneBlockWidget,
		TEXT("SpawnZoneBlockWidget"),
		TEXT("spawn_zone"),
		TEXT("root.pedestrians.background.spawn_zone"),
		TEXT("Detail"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddPedestrianPanelFieldRow(WidgetTree, spawnZoneBody, SpawnSegmentsFieldRow, TEXT("SpawnSegmentsFieldRow"));

	AddPedestrianPanelBlockWidget(
		WidgetTree,
		pedestriansBody,
		EncountersBlockWidget,
		TEXT("EncountersBlockWidget"),
		TEXT("encounters"),
		TEXT("root.pedestrians.encounters[]"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
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

	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		DiagnosticsTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	if (WidgetTree)
	{
		UWidgetTextStyleCatalog::ApplyWidgetTreeTextStyles(WidgetTree, TextStyleCatalog);
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
			UScenarioEditorSidebarFieldRow::StaticClass());
	if (!fieldRow)
	{
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
			UScenarioEditorSidebarPedestrianEncounterWidget::StaticClass());
	if (!encounterWidget)
	{
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
