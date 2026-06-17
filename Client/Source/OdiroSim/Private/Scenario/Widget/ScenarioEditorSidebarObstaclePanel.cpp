#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Shared/ScenarioCoreTypes.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float ObstaclePanelBlockPadding = 10.0f;

	FString JoinObstaclePanelDiagnostics(const TArray<FString>& diagnostics)
	{
		return diagnostics.IsEmpty()
			? FString(TEXT("Unknown Obstacle edit failure."))
			: FString::Join(diagnostics, TEXT("\n"));
	}

	void AddObstaclePanelWidgetToBox(
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

	UVerticalBox* AddObstaclePanelBlockWidget(
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
		AddObstaclePanelWidgetToBox(
			parent,
			blockWidget.Get(),
			FMargin(0.0f, 0.0f, 0.0f, ObstaclePanelBlockPadding));
		return blockWidget->GetBodyBox();
	}

	void AddObstaclePanelFieldRow(
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

	FScenarioTemplateNumberValue MakeUnsetNumberValue()
	{
		return FScenarioTemplateNumberValue();
	}

	FScenarioTemplateIntegerValue MakeUnsetIntegerValue()
	{
		return FScenarioTemplateIntegerValue();
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarObstaclePanel::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarObstaclePanel::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFromDraft();
}

void UScenarioEditorSidebarObstaclePanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarObstaclePanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarObstaclePanel::RefreshFromDraft()
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarObstaclePanel::RefreshFromTemplate(
	const FScenarioTemplateDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	const FScenarioTemplateObstacleRules& obstacles = scenarioTemplate.Obstacles;
	if (MinClearWidthFieldRow)
	{
		const FScenarioTemplateNumberValue& minClearWidth = obstacles.MinClearWidthMeters;
		const double fixedDisplayValue = minClearWidth.Mode == EScenarioTemplateNumberValueMode::Range
			? (minClearWidth.MinValue + minClearWidth.MaxValue) * 0.5
			: minClearWidth.FixedValue;
		const double minDisplayValue = minClearWidth.Mode == EScenarioTemplateNumberValueMode::Range
			? minClearWidth.MinValue
			: fixedDisplayValue;
		const double maxDisplayValue = minClearWidth.Mode == EScenarioTemplateNumberValueMode::Range
			? minClearWidth.MaxValue
			: fixedDisplayValue;

		MinClearWidthFieldRow->SetValueText(
			minClearWidth.bIsSet ? FormatEditableNumber(fixedDisplayValue) : FString());
		MinClearWidthFieldRow->SetRangeValueText(
			minClearWidth.bIsSet ? FormatEditableNumber(minDisplayValue) : FString(),
			minClearWidth.bIsSet ? FormatEditableNumber(maxDisplayValue) : FString());
		MinClearWidthFieldRow->SetRangeInputEnabled(
			minClearWidth.bIsSet
			&& minClearWidth.Mode == EScenarioTemplateNumberValueMode::Range);
	}

	RefreshPlacementRows(obstacles.Placements);
	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitMinClearWidthText(text);
}

void UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitMinClearWidthRangeText(minText, maxText);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountAddRequested()
{
	AddPlacementAfter(GetDraftPlacements().Num() - 1);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountRemoveRequested()
{
	RemovePlacementAt(GetDraftPlacements().Num() - 1);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementText(placementIndex, field, text);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementRange(placementIndex, field, minText, maxText);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested(const int32 placementIndex)
{
	AddPlacementAfter(placementIndex);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested(const int32 placementIndex)
{
	RemovePlacementAt(placementIndex);
}

void UScenarioEditorSidebarObstaclePanel::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedObstaclePanelRoot"));
	if (!rootBox)
	{
		return;
	}

	WidgetTree->RootWidget = rootBox;

	UVerticalBox* obstacleBody = AddObstaclePanelBlockWidget(
		WidgetTree,
		rootBox,
		ObstacleBlockWidget,
		TEXT("ObstacleBlockWidget"),
		TEXT("obstacles"),
		TEXT("root.obstacles"),
		TEXT("Template"),
		TextStyleCatalog,
		true);
	UVerticalBox* minClearWidthBody = AddObstaclePanelBlockWidget(
		WidgetTree,
		obstacleBody,
		MinClearWidthBlockWidget,
		TEXT("MinClearWidthBlockWidget"),
		TEXT("min_clear_width_m"),
		TEXT("root.obstacles.min_clear_width_m"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	AddObstaclePanelFieldRow(WidgetTree, minClearWidthBody, MinClearWidthFieldRow, TEXT("MinClearWidthFieldRow"));

	AddObstaclePanelBlockWidget(
		WidgetTree,
		obstacleBody,
		PlacementsBlockWidget,
		TEXT("PlacementsBlockWidget"),
		TEXT("placements"),
		TEXT("root.obstacles.placements[]"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
}

void UScenarioEditorSidebarObstaclePanel::BindFieldRows()
{
	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted);
		MinClearWidthFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted);
		MinClearWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted);
		MinClearWidthFieldRow->OnRangeValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted);
	}
}

void UScenarioEditorSidebarObstaclePanel::UnbindFieldRows()
{
	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted);
		MinClearWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted);
	}
	if (PlacementsCountFieldRow)
	{
		PlacementsCountFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountAddRequested);
		PlacementsCountFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountRemoveRequested);
	}

	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (!placementWidget)
		{
			continue;
		}

		placementWidget->OnFieldTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted);
		placementWidget->OnFieldRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted);
		placementWidget->OnAddRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested);
		placementWidget->OnRemoveRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested);
	}
}

void UScenarioEditorSidebarObstaclePanel::ConfigureFieldRows()
{
	if (ObstacleBlockWidget)
	{
		ObstacleBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		ObstacleBlockWidget->SetBlockMetadata(TEXT("obstacles"), TEXT("root.obstacles"), TEXT("Template"));
		ObstacleBlockWidget->SetSelected(true);
	}
	if (MinClearWidthBlockWidget)
	{
		MinClearWidthBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		MinClearWidthBlockWidget->SetBlockMetadata(
			TEXT("min_clear_width_m"),
			TEXT("root.obstacles.min_clear_width_m"),
			TEXT("Property"));
		MinClearWidthBlockWidget->SetNested(true);
		MinClearWidthBlockWidget->SetShowNormalOutline(false);
	}
	if (PlacementsBlockWidget)
	{
		PlacementsBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PlacementsBlockWidget->SetBlockMetadata(
			TEXT("placements"),
			TEXT("root.obstacles.placements[]"),
			TEXT("Property"));
		PlacementsBlockWidget->SetNested(true);
		PlacementsBlockWidget->SetShowNormalOutline(false);
	}

	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		MinClearWidthFieldRow->SetFieldLabel(TEXT("value"));
		MinClearWidthFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
		MinClearWidthFieldRow->SetEditable(true);
	}
}

void UScenarioEditorSidebarObstaclePanel::ApplyTextStyles()
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		ObstacleBlockWidget.Get(),
		MinClearWidthBlockWidget.Get(),
		PlacementsBlockWidget.Get() })
	{
		if (blockWidget)
		{
			blockWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (PlacementsCountFieldRow)
	{
		PlacementsCountFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (placementWidget)
		{
			placementWidget->SetTextStyleCatalog(TextStyleCatalog);
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

void UScenarioEditorSidebarObstaclePanel::RefreshPlacementRows(
	const TArray<FScenarioTemplateObstaclePlacement>& placements)
{
	if (!PlacementsBlockWidget)
	{
		return;
	}

	PlacementWidgets.Reset();
	PlacementsBlockWidget->ClearBodyChildren();
	PlacementsCountFieldRow = AddFieldRow(
		PlacementsBlockWidget.Get(),
		TEXT("count"),
		FString::FromInt(placements.Num()),
		EScenarioEditorSidebarFieldInputType::Integer,
		false,
		true);
	if (PlacementsCountFieldRow)
	{
		PlacementsCountFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountAddRequested);
		PlacementsCountFieldRow->OnAddItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountAddRequested);
		PlacementsCountFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountRemoveRequested);
		PlacementsCountFieldRow->OnRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountRemoveRequested);
	}

	for (int32 placementIndex = 0; placementIndex < placements.Num(); ++placementIndex)
	{
		if (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget =
			AddPlacementWidget(placementIndex, placements[placementIndex], PlacementsBlockWidget.Get()))
		{
			PlacementWidgets.Add(placementWidget);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarObstaclePanel::AddFieldRow(
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

UScenarioEditorSidebarObstaclePlacementWidget* UScenarioEditorSidebarObstaclePanel::AddPlacementWidget(
	const int32 placementIndex,
	const FScenarioTemplateObstaclePlacement& placement,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!WidgetTree || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarObstaclePlacementWidget* placementWidget =
		WidgetTree->ConstructWidget<UScenarioEditorSidebarObstaclePlacementWidget>(
			UScenarioEditorSidebarObstaclePlacementWidget::StaticClass());
	if (!placementWidget)
	{
		return nullptr;
	}

	placementWidget->SetTextStyleCatalog(TextStyleCatalog);
	placementWidget->SetPlacementIndex(placementIndex);
	placementWidget->RefreshFromPlacement(placement);
	placementWidget->OnFieldTextCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted);
	placementWidget->OnFieldTextCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted);
	placementWidget->OnFieldRangeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted);
	placementWidget->OnFieldRangeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted);
	placementWidget->OnAddRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested);
	placementWidget->OnAddRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested);
	placementWidget->OnRemoveRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested);
	placementWidget->OnRemoveRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested);
	parentBlockWidget->AddBodyChild(placementWidget);
	return placementWidget;
}

UScenarioAuthoringSubsystem* UScenarioEditorSidebarObstaclePanel::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

TArray<FScenarioTemplateObstaclePlacement> UScenarioEditorSidebarObstaclePanel::GetDraftPlacements() const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return {};
	}

	return authoringSubsystem->GetDraftScenarioTemplate().Obstacles.Placements;
}

void UScenarioEditorSidebarObstaclePanel::CommitMinClearWidthText(const FText& text)
{
	FScenarioTemplateNumberValue widthMeters;
	if (!TryParseOptionalNumber(text, widthMeters) || !widthMeters.bIsSet)
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("min_clear_width_m must be a number in meters."));
		return;
	}

	CommitMinClearWidthValue(widthMeters);
}

void UScenarioEditorSidebarObstaclePanel::CommitMinClearWidthRangeText(
	const FText& minText,
	const FText& maxText)
{
	FScenarioTemplateNumberValue widthMeters;
	if (!TryParseOptionalNumberRange(minText, maxText, widthMeters) || !widthMeters.bIsSet)
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("min_clear_width_m range must use numeric min/max meters."));
		return;
	}

	CommitMinClearWidthValue(widthMeters);
}

void UScenarioEditorSidebarObstaclePanel::CommitMinClearWidthValue(
	const FScenarioTemplateNumberValue& widthMeters)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetObstacleMinClearWidthMeters(widthMeters, diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinObstaclePanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementText(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& text)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	FScenarioTemplateObstaclePlacement& placement = placements[placementIndex];
	const FString trimmedText = text.ToString().TrimStartAndEnd();
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::PlacementId:
		placement.PlacementId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Kind:
	{
		EScenarioTemplateObstaclePlacementKind placementKind = placement.Kind;
		if (!TryParsePlacementKind(text, placementKind))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("kind must be fixed, pattern, or scatter."));
			return;
		}
		placement.Kind = placementKind;
		if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern)
		{
			if (placement.PatternId.IsEmpty())
			{
				placement.PatternId = TEXT("line");
			}
			if (!placement.Count.bIsSet)
			{
				placement.Count = UScenarioAuthoringSubsystem::MakeFixedTemplateIntegerValue(2);
			}
			if (!placement.SpacingMeters.bIsSet)
			{
				placement.SpacingMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(1.0);
			}
			if (placement.At.LaneId.IsEmpty())
			{
				placement.At.LaneId = TEXT("across");
			}
		}
		else if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Scatter)
		{
			if (placement.Zone.SegmentIds.IsEmpty())
			{
				placement.Zone.SegmentIds.Add(MakeDefaultPlacement(placements, placementIndex).At.SegmentId);
			}
			if (placement.Zone.LaneIds.IsEmpty())
			{
				placement.Zone.LaneIds.Add(TEXT("walkway"));
			}
			if (!placement.DensityPer10Meters.bIsSet)
			{
				placement.DensityPer10Meters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(1.0);
			}
		}
		else
		{
			if (placement.PropId.IsEmpty())
			{
				placement.PropId = MakeDefaultPlacement(placements, placementIndex).PropId;
			}
			if (placement.At.SegmentId.IsEmpty())
			{
				placement.At.SegmentId = MakeDefaultPlacement(placements, placementIndex).At.SegmentId;
			}
			if (!placement.At.AlongMeters.bIsSet)
			{
				placement.At.AlongMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
			}
			if (!placement.At.OffsetMeters.bIsSet)
			{
				placement.At.OffsetMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
			}
			if (placement.At.LaneId.IsEmpty())
			{
				placement.At.LaneId = TEXT("walkway");
			}
		}
		break;
	}
	case EScenarioEditorSidebarObstaclePlacementField::Prop:
		placement.PropId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Pattern:
		placement.PatternId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Segment:
		placement.At.SegmentId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Lane:
		placement.At.LaneId = trimmedText;
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Along:
	case EScenarioEditorSidebarObstaclePlacementField::Offset:
	case EScenarioEditorSidebarObstaclePlacementField::Spacing:
	case EScenarioEditorSidebarObstaclePlacementField::GapWidth:
	case EScenarioEditorSidebarObstaclePlacementField::Density:
	case EScenarioEditorSidebarObstaclePlacementField::Yaw:
	{
		FScenarioTemplateNumberValue numberValue;
		if (!TryParseOptionalNumber(text, numberValue) || !SetPlacementNumberField(placement, field, numberValue))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("Obstacle numeric fields must be finite numbers or empty optional values."));
			return;
		}
		break;
	}
	case EScenarioEditorSidebarObstaclePlacementField::ZoneSegments:
		placement.Zone.SegmentIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::ZoneLanes:
		placement.Zone.LaneIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteCategories:
		placement.Palette.CategoryIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::PaletteClasses:
		placement.Palette.ClassIds = ParseStringList(trimmedText);
		break;
	case EScenarioEditorSidebarObstaclePlacementField::Count:
	{
		FScenarioTemplateIntegerValue integerValue;
		if (!TryParseOptionalInteger(text, integerValue) || !SetPlacementIntegerField(placement, field, integerValue))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("count must be an integer, an integer range, or empty."));
			return;
		}
		break;
	}
	case EScenarioEditorSidebarObstaclePlacementField::AllowBlocking:
	{
		bool bAllowBlocking = false;
		if (!TryParseBool(text, bAllowBlocking))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("allow_blocking must be true or false."));
			return;
		}
		placement.bAllowBlocking = bAllowBlocking;
		break;
	}
	default:
		break;
	}

	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementRange(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& minText,
	const FText& maxText)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	if (field == EScenarioEditorSidebarObstaclePlacementField::Count)
	{
		FScenarioTemplateIntegerValue integerValue;
		if (!TryParseOptionalIntegerRange(minText, maxText, integerValue)
			|| !SetPlacementIntegerField(placements[placementIndex], field, integerValue))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("count range must use integer min/max values."));
			return;
		}
		CommitPlacements(placements);
		return;
	}

	FScenarioTemplateNumberValue numberValue;
	if (!TryParseOptionalNumberRange(minText, maxText, numberValue)
		|| !SetPlacementNumberField(placements[placementIndex], field, numberValue))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle range fields must use numeric min/max values."));
		return;
	}

	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::AddPlacementAfter(const int32 placementIndex)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	const int32 insertIndex = placements.IsValidIndex(placementIndex)
		? placementIndex + 1
		: placements.Num();
	placements.Insert(MakeDefaultPlacement(placements, placementIndex), insertIndex);
	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::RemovePlacementAt(const int32 placementIndex)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements.RemoveAt(placementIndex);
	CommitPlacements(placements);
}

FScenarioTemplateObstaclePlacement UScenarioEditorSidebarObstaclePanel::MakeDefaultPlacement(
	const TArray<FScenarioTemplateObstaclePlacement>& existingPlacements,
	const int32 neighborIndex) const
{
	FScenarioTemplateObstaclePlacement placement;
	placement.Kind = EScenarioTemplateObstaclePlacementKind::Fixed;

	FString baseId = TEXT("obstacle");
	for (int32 candidateIndex = 1; candidateIndex < 1000; ++candidateIndex)
	{
		const FString candidateId = FString::Printf(TEXT("%s_%03d"), *baseId, candidateIndex);
		const bool bExists = existingPlacements.ContainsByPredicate(
			[&candidateId](const FScenarioTemplateObstaclePlacement& existingPlacement)
			{
				return existingPlacement.PlacementId == candidateId;
			});
		if (!bExists)
		{
			placement.PlacementId = candidateId;
			break;
		}
	}

	if (existingPlacements.IsValidIndex(neighborIndex) && !existingPlacements[neighborIndex].PropId.IsEmpty())
	{
		placement.PropId = existingPlacements[neighborIndex].PropId;
	}
	else if (UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		TArray<FScenarioStaticObstaclePropEntry> propEntries;
		authoringSubsystem->GetStaticObstaclePaletteEntries(propEntries);
		if (!propEntries.IsEmpty() && !propEntries[0].PropId.IsNone())
		{
			placement.PropId = propEntries[0].PropId.ToString();
		}
	}

	if (existingPlacements.IsValidIndex(neighborIndex) && !existingPlacements[neighborIndex].At.SegmentId.IsEmpty())
	{
		placement.At.SegmentId = existingPlacements[neighborIndex].At.SegmentId;
	}
	else if (const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		const FScenarioTemplateDocument scenarioTemplate = authoringSubsystem->GetDraftScenarioTemplate();
		const TArray<FScenarioTemplateSegment>& segments = scenarioTemplate.Corridor.Segments;
		if (!segments.IsEmpty())
		{
			placement.At.SegmentId = segments[0].SegmentId;
		}
	}
	placement.At.AlongMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
	placement.At.OffsetMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
	placement.At.LaneId = TEXT("walkway");
	placement.YawDegrees = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
	placement.bAllowBlocking = false;
	return placement;
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacements(
	const TArray<FScenarioTemplateObstaclePlacement>& placements)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetObstaclePlacements(placements, diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinObstaclePanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarObstaclePanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}

bool UScenarioEditorSidebarObstaclePanel::SetPlacementNumberField(
	FScenarioTemplateObstaclePlacement& placement,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FScenarioTemplateNumberValue& value)
{
	switch (field)
	{
	case EScenarioEditorSidebarObstaclePlacementField::Along:
		placement.At.AlongMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Offset:
		placement.At.OffsetMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Spacing:
		placement.SpacingMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::GapWidth:
		placement.GapWidthMeters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Density:
		placement.DensityPer10Meters = value;
		return true;
	case EScenarioEditorSidebarObstaclePlacementField::Yaw:
		placement.YawDegrees = value;
		return true;
	default:
		return false;
	}
}

bool UScenarioEditorSidebarObstaclePanel::SetPlacementIntegerField(
	FScenarioTemplateObstaclePlacement& placement,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FScenarioTemplateIntegerValue& value)
{
	if (field != EScenarioEditorSidebarObstaclePlacementField::Count)
	{
		return false;
	}

	placement.Count = value;
	return true;
}

bool UScenarioEditorSidebarObstaclePanel::TryParsePlacementKind(
	const FText& text,
	EScenarioTemplateObstaclePlacementKind& outKind)
{
	const FString kindText = text.ToString().TrimStartAndEnd().ToLower();
	if (kindText == TEXT("fixed"))
	{
		outKind = EScenarioTemplateObstaclePlacementKind::Fixed;
		return true;
	}
	if (kindText == TEXT("pattern"))
	{
		outKind = EScenarioTemplateObstaclePlacementKind::Pattern;
		return true;
	}
	if (kindText == TEXT("scatter"))
	{
		outKind = EScenarioTemplateObstaclePlacementKind::Scatter;
		return true;
	}
	return false;
}

bool UScenarioEditorSidebarObstaclePanel::TryParseScalar(const FText& text, double& outValue)
{
	FString scalarText = text.ToString().TrimStartAndEnd();
	scalarText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
	scalarText.RemoveFromEnd(TEXT("deg"), ESearchCase::IgnoreCase);
	scalarText.TrimStartAndEndInline();
	return LexTryParseString(outValue, *scalarText) && FMath::IsFinite(outValue);
}

bool UScenarioEditorSidebarObstaclePanel::TryParseOptionalNumber(
	const FText& text,
	FScenarioTemplateNumberValue& outValue)
{
	const FString trimmedText = text.ToString().TrimStartAndEnd();
	if (trimmedText.IsEmpty())
	{
		outValue = MakeUnsetNumberValue();
		return true;
	}

	double parsedValue = 0.0;
	if (!TryParseScalar(text, parsedValue))
	{
		return false;
	}

	outValue = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(parsedValue);
	return true;
}

bool UScenarioEditorSidebarObstaclePanel::TryParseOptionalNumberRange(
	const FText& minText,
	const FText& maxText,
	FScenarioTemplateNumberValue& outValue)
{
	const bool bMinEmpty = minText.ToString().TrimStartAndEnd().IsEmpty();
	const bool bMaxEmpty = maxText.ToString().TrimStartAndEnd().IsEmpty();
	if (bMinEmpty && bMaxEmpty)
	{
		outValue = MakeUnsetNumberValue();
		return true;
	}

	double minValue = 0.0;
	double maxValue = 0.0;
	if (!TryParseScalar(minText, minValue) || !TryParseScalar(maxText, maxValue))
	{
		return false;
	}

	outValue = UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(minValue, maxValue);
	return true;
}

bool UScenarioEditorSidebarObstaclePanel::TryParseOptionalInteger(
	const FText& text,
	FScenarioTemplateIntegerValue& outValue)
{
	const FString trimmedText = text.ToString().TrimStartAndEnd();
	if (trimmedText.IsEmpty())
	{
		outValue = MakeUnsetIntegerValue();
		return true;
	}

	int32 parsedValue = 0;
	if (!LexTryParseString(parsedValue, *trimmedText))
	{
		return false;
	}

	outValue = UScenarioAuthoringSubsystem::MakeFixedTemplateIntegerValue(parsedValue);
	return true;
}

bool UScenarioEditorSidebarObstaclePanel::TryParseOptionalIntegerRange(
	const FText& minText,
	const FText& maxText,
	FScenarioTemplateIntegerValue& outValue)
{
	const bool bMinEmpty = minText.ToString().TrimStartAndEnd().IsEmpty();
	const bool bMaxEmpty = maxText.ToString().TrimStartAndEnd().IsEmpty();
	if (bMinEmpty && bMaxEmpty)
	{
		outValue = MakeUnsetIntegerValue();
		return true;
	}

	int32 minValue = 0;
	int32 maxValue = 0;
	const FString trimmedMinText = minText.ToString().TrimStartAndEnd();
	const FString trimmedMaxText = maxText.ToString().TrimStartAndEnd();
	if (!LexTryParseString(minValue, *trimmedMinText)
		|| !LexTryParseString(maxValue, *trimmedMaxText))
	{
		return false;
	}

	outValue = UScenarioAuthoringSubsystem::MakeRangeTemplateIntegerValue(minValue, maxValue);
	return true;
}

bool UScenarioEditorSidebarObstaclePanel::TryParseBool(const FText& text, bool& outValue)
{
	const FString boolText = text.ToString().TrimStartAndEnd().ToLower();
	if (boolText == TEXT("true") || boolText == TEXT("1") || boolText == TEXT("yes"))
	{
		outValue = true;
		return true;
	}
	if (boolText == TEXT("false") || boolText == TEXT("0") || boolText == TEXT("no"))
	{
		outValue = false;
		return true;
	}
	return false;
}

TArray<FString> UScenarioEditorSidebarObstaclePanel::ParseStringList(const FString& text)
{
	TArray<FString> values;
	text.ParseIntoArray(values, TEXT(","), true);
	for (FString& value : values)
	{
		value.TrimStartAndEndInline();
	}
	values.RemoveAll(
		[](const FString& value)
		{
			return value.IsEmpty();
		});
	return values;
}

FString UScenarioEditorSidebarObstaclePanel::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
