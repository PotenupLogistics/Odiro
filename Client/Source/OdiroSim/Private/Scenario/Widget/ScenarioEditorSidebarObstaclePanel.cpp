#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"
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

void UScenarioEditorSidebarObstaclePanel::HandlePlacementIdCommitted(
	const int32 placementIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementIdText(placementIndex, text);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementPropCommitted(
	const int32 placementIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementPropText(placementIndex, text);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementSegmentCommitted(
	const int32 placementIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementSegmentText(placementIndex, text);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementAlongCommitted(
	const int32 placementIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementAlongText(placementIndex, text);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementOffsetCommitted(
	const int32 placementIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementOffsetText(placementIndex, text);
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementAllowBlockingCommitted(
	const int32 placementIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitPlacementAllowBlockingText(placementIndex, text);
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

	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (!placementWidget)
		{
			continue;
		}

		placementWidget->OnPlacementIdCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementIdCommitted);
		placementWidget->OnPropCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementPropCommitted);
		placementWidget->OnSegmentCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementSegmentCommitted);
		placementWidget->OnAlongCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementAlongCommitted);
		placementWidget->OnOffsetCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementOffsetCommitted);
		placementWidget->OnAllowBlockingCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementAllowBlockingCommitted);
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
	PlacementsCountFieldRow = AddReadOnlyFieldRow(
		PlacementsBlockWidget.Get(),
		TEXT("count"),
		FString::FromInt(placements.Num()),
		EScenarioEditorSidebarFieldInputType::Integer);

	for (int32 placementIndex = 0; placementIndex < placements.Num(); ++placementIndex)
	{
		if (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget =
			AddPlacementWidget(placementIndex, placements[placementIndex], PlacementsBlockWidget.Get()))
		{
			PlacementWidgets.Add(placementWidget);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarObstaclePanel::AddReadOnlyFieldRow(
	UScenarioEditorSidebarBlockWidget* parentBlockWidget,
	const FString& label,
	const FString& value,
	const EScenarioEditorSidebarFieldInputType inputType) const
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
	fieldRow->SetEditable(false);
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
	placementWidget->OnPlacementIdCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementIdCommitted);
	placementWidget->OnPlacementIdCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementIdCommitted);
	placementWidget->OnPropCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementPropCommitted);
	placementWidget->OnPropCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementPropCommitted);
	placementWidget->OnSegmentCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementSegmentCommitted);
	placementWidget->OnSegmentCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementSegmentCommitted);
	placementWidget->OnAlongCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAlongCommitted);
	placementWidget->OnAlongCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAlongCommitted);
	placementWidget->OnOffsetCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementOffsetCommitted);
	placementWidget->OnOffsetCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementOffsetCommitted);
	placementWidget->OnAllowBlockingCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAllowBlockingCommitted);
	placementWidget->OnAllowBlockingCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAllowBlockingCommitted);
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
	double widthMeters = 0.0;
	if (!TryParseMeters(text, widthMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("min_clear_width_m must be a number in meters."));
		return;
	}

	CommitMinClearWidthValue(UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(widthMeters));
}

void UScenarioEditorSidebarObstaclePanel::CommitMinClearWidthRangeText(
	const FText& minText,
	const FText& maxText)
{
	double minWidthMeters = 0.0;
	double maxWidthMeters = 0.0;
	if (!TryParseMeters(minText, minWidthMeters) || !TryParseMeters(maxText, maxWidthMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("min_clear_width_m range must use numeric min/max meters."));
		return;
	}

	CommitMinClearWidthValue(
		UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(minWidthMeters, maxWidthMeters));
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

void UScenarioEditorSidebarObstaclePanel::CommitPlacementIdText(
	const int32 placementIndex,
	const FText& text)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements[placementIndex].PlacementId = text.ToString().TrimStartAndEnd();
	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementPropText(
	const int32 placementIndex,
	const FText& text)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements[placementIndex].PropId = text.ToString().TrimStartAndEnd();
	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementSegmentText(
	const int32 placementIndex,
	const FText& text)
{
	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements[placementIndex].At.SegmentId = text.ToString().TrimStartAndEnd();
	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementAlongText(
	const int32 placementIndex,
	const FText& text)
{
	double alongMeters = 0.0;
	if (!TryParseMeters(text, alongMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("at.along_m must be a finite number in meters."));
		return;
	}

	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements[placementIndex].At.AlongMeters =
		UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(alongMeters);
	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementOffsetText(
	const int32 placementIndex,
	const FText& text)
{
	double offsetMeters = 0.0;
	if (!TryParseMeters(text, offsetMeters))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("at.offset_m must be a finite number in meters."));
		return;
	}

	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements[placementIndex].At.OffsetMeters =
		UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(offsetMeters);
	CommitPlacements(placements);
}

void UScenarioEditorSidebarObstaclePanel::CommitPlacementAllowBlockingText(
	const int32 placementIndex,
	const FText& text)
{
	bool bAllowBlocking = false;
	if (!TryParseBool(text, bAllowBlocking))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("allow_blocking must be true or false."));
		return;
	}

	TArray<FScenarioTemplateObstaclePlacement> placements = GetDraftPlacements();
	if (!placements.IsValidIndex(placementIndex))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Obstacle placement index is no longer valid."));
		return;
	}

	placements[placementIndex].bAllowBlocking = bAllowBlocking;
	CommitPlacements(placements);
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

bool UScenarioEditorSidebarObstaclePanel::TryParseMeters(const FText& text, double& outMeters)
{
	FString meterText = text.ToString().TrimStartAndEnd();
	meterText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
	meterText.TrimStartAndEndInline();
	return LexTryParseString(outMeters, *meterText) && FMath::IsFinite(outMeters);
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

FString UScenarioEditorSidebarObstaclePanel::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

FString UScenarioEditorSidebarObstaclePanel::FormatEditableNumber(
	const FScenarioTemplateNumberValue& value)
{
	if (!value.bIsSet)
	{
		return FString();
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%.2f..%.2f"), value.MinValue, value.MaxValue);
	}
	return FormatEditableNumber(value.FixedValue);
}
