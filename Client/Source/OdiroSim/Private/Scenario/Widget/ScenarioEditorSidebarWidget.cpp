#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float SidebarPadding = 10.0f;
	constexpr float SidebarPanelContentTopPadding = 4.0f;
	constexpr float BlockOutlineThickness = 1.0f;
	const FLinearColor PanelColor(0.11f, 0.13f, 0.15f, 0.92f);
	const FLinearColor BlockColor(0.14f, 0.17f, 0.20f, 0.96f);
	const FLinearColor NestedBlockColor(0.17f, 0.20f, 0.24f, 0.96f);
	const FLinearColor BlockOutlineColor(0.27f, 0.33f, 0.39f, 1.0f);
	const FLinearColor ActiveBlockOutlineColor(0.28f, 0.65f, 1.0f, 1.0f);

	FString JoinLines(const TArray<FString>& lines)
	{
		return lines.IsEmpty() ? FString(TEXT("None")) : FString::Join(lines, TEXT("\n"));
	}

	FString FormatMeters(const double value)
	{
		return FString::Printf(TEXT("%.2fm"), value);
	}

	FSlateBrush MakeColorBrush(const FLinearColor& color)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(color);
		return brush;
	}

	void ApplyBorderFill(UBorder* border, const FLinearColor& color, const FMargin& padding)
	{
		if (!border)
		{
			return;
		}

		border->SetBrush(MakeColorBrush(color));
		border->SetBrushColor(color);
		border->SetPadding(padding);
	}

	UTextBlock* MakeStyledText(
		UWidgetTree* widgetTree,
		const FString& text,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog,
		const EWidgetTextStyleRole role,
		const bool bWrap = false)
	{
		UTextBlock* textBlock = widgetTree
			? widgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
			: nullptr;
		if (!textBlock)
		{
			return nullptr;
		}

		textBlock->SetText(FText::FromString(text));
		textBlock->SetAutoWrapText(bWrap);
		UWidgetTextStyleCatalog::ApplyTextBlockStyle(textBlock, catalog, role);
		return textBlock;
	}

	void AddTextToHorizontalBox(
		UHorizontalBox* row,
		UTextBlock* textBlock,
		const ESlateSizeRule::Type sizeRule,
		const FMargin& padding = FMargin())
	{
		if (!row || !textBlock)
		{
			return;
		}

		if (UHorizontalBoxSlot* slot = row->AddChildToHorizontalBox(textBlock))
		{
			slot->SetPadding(padding);
			slot->SetVerticalAlignment(VAlign_Top);
			slot->SetSize(FSlateChildSize(sizeRule));
		}
	}

	void AddWidgetToVerticalBox(UVerticalBox* box, UWidget* widget, const FMargin& padding = FMargin())
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

	// Infers a conservative editor control type for generated read-only rows.
	EScenarioEditorSidebarFieldInputType InferGeneratedFieldInputType(const FString& label)
	{
		const FString normalizedLabel = label.ToLower();
		if (normalizedLabel.Contains(TEXT("range"))
			|| normalizedLabel.Contains(TEXT("along_m"))
			|| normalizedLabel.Contains(TEXT("offset_m")))
		{
			return EScenarioEditorSidebarFieldInputType::Range;
		}
		if (normalizedLabel.Contains(TEXT("count"))
			|| normalizedLabel.Contains(TEXT("version")))
		{
			return EScenarioEditorSidebarFieldInputType::Integer;
		}
		if (normalizedLabel.Contains(TEXT("_m"))
			|| normalizedLabel.Contains(TEXT("density"))
			|| normalizedLabel.Contains(TEXT("cooperation"))
			|| normalizedLabel.Contains(TEXT("speed")))
		{
			return EScenarioEditorSidebarFieldInputType::Number;
		}
		if (normalizedLabel.Contains(TEXT("type"))
			|| normalizedLabel.Contains(TEXT("kind"))
			|| normalizedLabel.Contains(TEXT("allow_")))
		{
			return EScenarioEditorSidebarFieldInputType::EnumText;
		}
		return EScenarioEditorSidebarFieldInputType::Text;
	}

	void AddFieldRow(
		UWidgetTree* widgetTree,
		UVerticalBox* body,
		const FString& label,
		const FString& value,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog)
	{
		if (!widgetTree || !body)
		{
			return;
		}

		UScenarioEditorSidebarFieldRow* row =
			widgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
				UScenarioEditorSidebarFieldRow::StaticClass());
		if (!row)
		{
			return;
		}

		row->SetTextStyleCatalog(catalog);
		row->SetFieldLabel(label);
		row->SetValueText(value);
		row->SetInputType(InferGeneratedFieldInputType(label));
		row->SetEditable(false);
		AddWidgetToVerticalBox(body, row);
	}

	UVerticalBox* AddBlock(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		const FString& name,
		const FString& path,
		const FString& badge,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog,
		const bool bHighlighted = false,
		const bool bNested = false)
	{
		if (!widgetTree || !parent)
		{
			return nullptr;
		}

		UBorder* outlineBorder = widgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ApplyBorderFill(
			outlineBorder,
			bHighlighted ? ActiveBlockOutlineColor : BlockOutlineColor,
			FMargin(BlockOutlineThickness));

		UBorder* contentBorder = widgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ApplyBorderFill(contentBorder, bNested ? NestedBlockColor : BlockColor, FMargin(SidebarPadding));
		outlineBorder->SetContent(contentBorder);

		UVerticalBox* blockBox = widgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		contentBorder->SetContent(blockBox);

		UHorizontalBox* headerRow = widgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddTextToHorizontalBox(
			headerRow,
			MakeStyledText(widgetTree, TEXT("▼"), catalog, EWidgetTextStyleRole::Label),
			ESlateSizeRule::Automatic,
			FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddTextToHorizontalBox(
			headerRow,
			MakeStyledText(widgetTree, name, catalog, EWidgetTextStyleRole::Label),
			ESlateSizeRule::Automatic,
			FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddTextToHorizontalBox(
			headerRow,
			MakeStyledText(widgetTree, path, catalog, EWidgetTextStyleRole::Caption),
			ESlateSizeRule::Fill,
			FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddTextToHorizontalBox(
			headerRow,
			MakeStyledText(widgetTree, badge, catalog, EWidgetTextStyleRole::Label),
			ESlateSizeRule::Automatic,
			FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		AddWidgetToVerticalBox(blockBox, headerRow);

		UVerticalBox* body = widgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddWidgetToVerticalBox(blockBox, body, FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		AddWidgetToVerticalBox(parent, outlineBorder, FMargin(0.0f, 0.0f, 0.0f, SidebarPadding));
		return body;
	}

	UVerticalBox* AddBlockWidget(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		const FString& name,
		const FString& path,
		const FString& badge,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog,
		const bool bHighlighted = false,
		const bool bNested = false,
		const bool bExpanded = true)
	{
		if (!widgetTree || !parent)
		{
			return nullptr;
		}

		UScenarioEditorSidebarBlockWidget* blockWidget =
			widgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
				UScenarioEditorSidebarBlockWidget::StaticClass());
		if (!blockWidget)
		{
			return nullptr;
		}

		blockWidget->SetTextStyleCatalog(catalog);
		blockWidget->SetBlockMetadata(name, path, badge);
		blockWidget->SetSelected(bHighlighted);
		blockWidget->SetShowNormalOutline(badge.Equals(TEXT("Main")) || badge.Equals(TEXT("Template")));
		blockWidget->SetNested(bNested);
		blockWidget->SetExpanded(bExpanded);
		AddWidgetToVerticalBox(parent, blockWidget, FMargin(0.0f, 0.0f, 0.0f, SidebarPadding));
		return blockWidget->GetBodyBox();
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	EnsurePanelSwitcherIsScrollable();
	ConfigureScrollBox();
	ApplyTextStyles();
	RefreshFromDraft();
}

void UScenarioEditorSidebarWidget::SetActivePanel(const EScenarioTemplateSidebarPanel panel)
{
	ActivePanel = panel;
	RefreshFromDraft();
}

void UScenarioEditorSidebarWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarWidget::RefreshFromDraft()
{
	UWorld* world = GetWorld();
	const UScenarioAuthoringSubsystem* authoringSubsystem = world
		? world->GetSubsystem<UScenarioAuthoringSubsystem>()
		: nullptr;
	if (!authoringSubsystem)
	{
		SetSidebarText(
			PanelToTitle(ActivePanel),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("ScenarioAuthoringSubsystem unavailable."));
		SetFallbackTextVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarWidget::RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate)
{
	FString primaryText;
	FString secondaryText;
	FString listText;

	switch (ActivePanel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		BuildMainPanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		BuildCorridorPanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		BuildObstaclePanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		BuildPedestrianPanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	default:
		break;
	}

	SetSidebarText(PanelToTitle(ActivePanel), primaryText, secondaryText, listText, TEXT(""));
	if (MainPanelWidget)
	{
		MainPanelWidget->SetTextStyleCatalog(TextStyleCatalog);
		MainPanelWidget->RefreshFromTemplate(scenarioTemplate);
	}
	RefreshGeneratedPanelContent(scenarioTemplate);
	RefreshPanelSwitcher();
	RefreshFallbackTextVisibility();
	ApplyTextStyles();
}

void UScenarioEditorSidebarWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedSidebarRoot"));
	if (!rootBox)
	{
		return;
	}
	WidgetTree->RootWidget = rootBox;

	UBorder* headerBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("GeneratedSidebarHeaderBorder"));
	ApplyBorderFill(headerBorder, PanelColor, FMargin(14.0f));
	PanelTitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PanelTitleTextBlock"));
	if (PanelTitleTextBlock)
	{
		headerBorder->SetContent(PanelTitleTextBlock);
	}
	AddWidgetToVerticalBox(rootBox, headerBorder);

	SidebarScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("SidebarScrollBox"));
	if (SidebarScrollBox)
	{
		PanelSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(
			UWidgetSwitcher::StaticClass(),
			TEXT("PanelSwitcher"));
		if (PanelSwitcher)
		{
			SidebarScrollBox->AddChild(PanelSwitcher);
		}
		if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(SidebarScrollBox))
		{
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	FallbackSummaryContainer = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("FallbackSummaryContainer"));
	if (UVerticalBox* fallbackBox = Cast<UVerticalBox>(FallbackSummaryContainer))
	{
		PrimaryFieldsTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("PrimaryFieldsTextBlock"));
		SecondaryFieldsTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("SecondaryFieldsTextBlock"));
		ListSummaryTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("ListSummaryTextBlock"));
		DiagnosticsTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("DiagnosticsTextBlock"));

		AddWidgetToVerticalBox(fallbackBox, PrimaryFieldsTextBlock.Get(), FMargin(SidebarPadding));
		AddWidgetToVerticalBox(fallbackBox, SecondaryFieldsTextBlock.Get(), FMargin(SidebarPadding));
		AddWidgetToVerticalBox(fallbackBox, ListSummaryTextBlock.Get(), FMargin(SidebarPadding));
		AddWidgetToVerticalBox(fallbackBox, DiagnosticsTextBlock.Get(), FMargin(SidebarPadding));
		AddWidgetToVerticalBox(rootBox, fallbackBox);
	}

	ConfigureScrollBox();
}

void UScenarioEditorSidebarWidget::ConfigureScrollBox() const
{
	if (!SidebarScrollBox)
	{
		return;
	}

	SidebarScrollBox->SetOrientation(Orient_Vertical);
	SidebarScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
	SidebarScrollBox->SetAlwaysShowScrollbar(false);
	SidebarScrollBox->SetAllowOverscroll(false);
	SidebarScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);

	if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(SidebarScrollBox->Slot))
	{
		verticalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		verticalSlot->SetHorizontalAlignment(HAlign_Fill);
		verticalSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

bool UScenarioEditorSidebarWidget::EnsurePanelSwitcherIsScrollable()
{
	if (SidebarScrollBox || !PanelSwitcher || !WidgetTree || !PanelSwitcher->Slot)
	{
		return false;
	}

	UPanelWidget* parentPanel = PanelSwitcher->Slot->Parent;
	if (!parentPanel)
	{
		return false;
	}

	FSlateChildSize previousVerticalSize(ESlateSizeRule::Fill);
	FMargin previousPadding = FMargin();
	EHorizontalAlignment previousHorizontalAlignment = HAlign_Fill;
	EVerticalAlignment previousVerticalAlignment = VAlign_Fill;
	if (const UVerticalBoxSlot* previousVerticalSlot = Cast<UVerticalBoxSlot>(PanelSwitcher->Slot))
	{
		previousVerticalSize = previousVerticalSlot->GetSize();
		previousPadding = previousVerticalSlot->GetPadding();
		previousHorizontalAlignment = previousVerticalSlot->GetHorizontalAlignment();
		previousVerticalAlignment = previousVerticalSlot->GetVerticalAlignment();
	}

	UScrollBox* generatedScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("GeneratedSidebarScrollBox"));
	if (!generatedScrollBox || !parentPanel->RemoveChild(PanelSwitcher.Get()))
	{
		return false;
	}

	UPanelSlot* newParentSlot = parentPanel->AddChild(generatedScrollBox);
	if (!newParentSlot)
	{
		parentPanel->AddChild(PanelSwitcher.Get());
		return false;
	}

	SidebarScrollBox = generatedScrollBox;
	if (UVerticalBoxSlot* newVerticalSlot = Cast<UVerticalBoxSlot>(newParentSlot))
	{
		newVerticalSlot->SetSize(previousVerticalSize);
		newVerticalSlot->SetPadding(previousPadding);
		newVerticalSlot->SetHorizontalAlignment(previousHorizontalAlignment);
		newVerticalSlot->SetVerticalAlignment(previousVerticalAlignment);
	}

	SidebarScrollBox->AddChild(PanelSwitcher.Get());
	ConfigureScrollBox();
	return true;
}

void UScenarioEditorSidebarWidget::RefreshGeneratedPanelContent(
	const FScenarioTemplateDocument& scenarioTemplate)
{
	if (!PanelSwitcher || !WidgetTree)
	{
		return;
	}

	for (const EScenarioTemplateSidebarPanel panel : {
		EScenarioTemplateSidebarPanel::Main,
		EScenarioTemplateSidebarPanel::Corridor,
		EScenarioTemplateSidebarPanel::Obstacle,
		EScenarioTemplateSidebarPanel::Pedestrian })
	{
		UWidget* panelWidget = ResolvePanelWidget(panel);
		if (!panelWidget)
		{
			panelWidget = EnsureGeneratedPanelWidget(panel);
		}
		if (!panelWidget || (panel == EScenarioTemplateSidebarPanel::Main && MainPanelWidget))
		{
			continue;
		}

		UWidget* contentWidget = BuildGeneratedPanelContent(panel, scenarioTemplate);
		if (!ApplyGeneratedContentToPanelWidget(panelWidget, contentWidget) && panel != EScenarioTemplateSidebarPanel::Main)
		{
			UWidget* generatedWidget = EnsureGeneratedPanelWidget(panel);
			if (generatedWidget && generatedWidget != panelWidget)
			{
				ApplyGeneratedContentToPanelWidget(generatedWidget, contentWidget);
			}
		}
	}
}

UWidget* UScenarioEditorSidebarWidget::EnsureGeneratedPanelWidget(
	const EScenarioTemplateSidebarPanel panel)
{
	if (UWidget* generatedWidget = ResolveGeneratedPanelWidget(panel))
	{
		return generatedWidget;
	}
	if (!PanelSwitcher || !WidgetTree)
	{
		return nullptr;
	}

	UVerticalBox* panelRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (!panelRoot)
	{
		return nullptr;
	}
	PanelSwitcher->AddChild(panelRoot);
	ApplyPanelContentPadding(panelRoot);

	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		GeneratedMainPanelWidget = panelRoot;
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		GeneratedCorridorPanelWidget = panelRoot;
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		GeneratedObstaclePanelWidget = panelRoot;
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		GeneratedPedestrianPanelWidget = panelRoot;
		break;
	default:
		break;
	}

	return panelRoot;
}

UWidget* UScenarioEditorSidebarWidget::ResolveGeneratedPanelWidget(
	const EScenarioTemplateSidebarPanel panel) const
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		return GeneratedMainPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Corridor:
		return GeneratedCorridorPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Obstacle:
		return GeneratedObstaclePanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return GeneratedPedestrianPanelWidget.Get();
	default:
		return nullptr;
	}
}

bool UScenarioEditorSidebarWidget::ApplyGeneratedContentToPanelWidget(
	UWidget* panelWidget,
	UWidget* contentWidget) const
{
	if (!panelWidget || !contentWidget)
	{
		return false;
	}

	if (UPanelWidget* panelContainer = Cast<UPanelWidget>(panelWidget))
	{
		panelContainer->ClearChildren();
		panelContainer->AddChild(contentWidget);
		return true;
	}

	if (UContentWidget* contentContainer = Cast<UContentWidget>(panelWidget))
	{
		contentContainer->SetContent(contentWidget);
		return true;
	}

	return false;
}

UWidget* UScenarioEditorSidebarWidget::BuildGeneratedPanelContent(
	const EScenarioTemplateSidebarPanel panel,
	const FScenarioTemplateDocument& scenarioTemplate)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	if (panel == EScenarioTemplateSidebarPanel::Corridor)
	{
		UScenarioEditorSidebarCorridorPanel* corridorPanel =
			WidgetTree->ConstructWidget<UScenarioEditorSidebarCorridorPanel>(
				UScenarioEditorSidebarCorridorPanel::StaticClass());
		if (!corridorPanel)
		{
			return nullptr;
		}

		corridorPanel->SetTextStyleCatalog(TextStyleCatalog);
		corridorPanel->RefreshFromTemplate(scenarioTemplate);
		return corridorPanel;
	}

	if (panel == EScenarioTemplateSidebarPanel::Obstacle)
	{
		UScenarioEditorSidebarObstaclePanel* obstaclePanel =
			WidgetTree->ConstructWidget<UScenarioEditorSidebarObstaclePanel>(
				UScenarioEditorSidebarObstaclePanel::StaticClass());
		if (!obstaclePanel)
		{
			return nullptr;
		}

		obstaclePanel->SetTextStyleCatalog(TextStyleCatalog);
		obstaclePanel->RefreshFromTemplate(scenarioTemplate);
		return obstaclePanel;
	}

	if (panel == EScenarioTemplateSidebarPanel::Pedestrian)
	{
		UScenarioEditorSidebarPedestrianPanel* pedestrianPanel =
			WidgetTree->ConstructWidget<UScenarioEditorSidebarPedestrianPanel>(
				UScenarioEditorSidebarPedestrianPanel::StaticClass());
		if (!pedestrianPanel)
		{
			return nullptr;
		}

		pedestrianPanel->SetTextStyleCatalog(TextStyleCatalog);
		pedestrianPanel->RefreshFromTemplate(scenarioTemplate);
		return pedestrianPanel;
	}

	UVerticalBox* panelRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (!panelRoot)
	{
		return nullptr;
	}

	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
	{
		UVerticalBox* rootBody = AddBlockWidget(
			WidgetTree,
			panelRoot,
			TEXT("Root"),
			TEXT("scenario_template"),
			TEXT("Main"),
			TextStyleCatalog,
			true);
		AddFieldRow(WidgetTree, rootBody, TEXT("schema"), scenarioTemplate.Schema, TextStyleCatalog);
		AddFieldRow(WidgetTree, rootBody, TEXT("version"), FString::FromInt(scenarioTemplate.Version), TextStyleCatalog);
		AddFieldRow(WidgetTree, rootBody, TEXT("template_id"), scenarioTemplate.TemplateId, TextStyleCatalog);
		AddFieldRow(WidgetTree, rootBody, TEXT("intent"), scenarioTemplate.Intent, TextStyleCatalog);

		UVerticalBox* robotBody = AddBlockWidget(
			WidgetTree,
			rootBody,
			TEXT("robot"),
			TEXT("root.robot"),
			TEXT("Template"),
			TextStyleCatalog,
			false,
			true);
		AddFieldRow(WidgetTree, robotBody, TEXT("start"), FormatRobotAnchor(scenarioTemplate.Robot.Start), TextStyleCatalog);
		AddFieldRow(WidgetTree, robotBody, TEXT("goal"), FormatRobotAnchor(scenarioTemplate.Robot.Goal), TextStyleCatalog);
		break;
	}
	case EScenarioTemplateSidebarPanel::Obstacle:
	{
		const FScenarioTemplateObstacleRules& obstacles = scenarioTemplate.Obstacles;
		UVerticalBox* obstacleBody = AddBlockWidget(
			WidgetTree,
			panelRoot,
			TEXT("obstacles"),
			TEXT("root.obstacles"),
			TEXT("Template"),
			TextStyleCatalog,
			true);
		AddFieldRow(WidgetTree, obstacleBody, TEXT("min_clear_width_m"), FormatNumberValue(obstacles.MinClearWidthMeters, TEXT("m")), TextStyleCatalog);

		UVerticalBox* placementsBody = AddBlockWidget(
			WidgetTree,
			obstacleBody,
			TEXT("placements"),
			TEXT("root.obstacles.placements[]"),
			TEXT("Property"),
			TextStyleCatalog,
			false,
			true);
		for (const FScenarioTemplateObstaclePlacement& placement : obstacles.Placements)
		{
			UVerticalBox* placementBody = AddBlockWidget(
				WidgetTree,
				placementsBody,
				placement.PlacementId.IsEmpty() ? FString(TEXT("(unnamed)")) : placement.PlacementId,
				ObstaclePlacementKindToString(placement.Kind),
				TEXT("Detail"),
				TextStyleCatalog,
				false,
				true);
			AddFieldRow(WidgetTree, placementBody, TEXT("kind"), ObstaclePlacementKindToString(placement.Kind), TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("prop"), placement.PropId, TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("pattern"), placement.PatternId, TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("at.segment"), placement.At.SegmentId, TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("at.along_m"), FormatNumberValue(placement.At.AlongMeters, TEXT("m")), TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("at.offset_m"), FormatNumberValue(placement.At.OffsetMeters, TEXT("m")), TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("zone.segments"), FormatStringList(placement.Zone.SegmentIds), TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("zone.lanes"), FormatStringList(placement.Zone.LaneIds), TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("density_per_10m"), FormatNumberValue(placement.DensityPer10Meters), TextStyleCatalog);
			AddFieldRow(WidgetTree, placementBody, TEXT("allow_blocking"), placement.bAllowBlocking ? TEXT("true") : TEXT("false"), TextStyleCatalog);
		}
		break;
	}
	case EScenarioTemplateSidebarPanel::Pedestrian:
	{
		const FScenarioTemplatePedestrianRules& pedestrians = scenarioTemplate.Pedestrians;
		UVerticalBox* pedestrianBody = AddBlockWidget(
			WidgetTree,
			panelRoot,
			TEXT("pedestrians"),
			TEXT("root.pedestrians"),
			TEXT("Template"),
			TextStyleCatalog,
			true);
		UVerticalBox* backgroundBody = AddBlockWidget(
			WidgetTree,
			pedestrianBody,
			TEXT("background"),
			TEXT("count / speed_mps"),
			TEXT("Property"),
			TextStyleCatalog,
			false,
			true);
		AddFieldRow(WidgetTree, backgroundBody, TEXT("count"), FormatIntegerValue(pedestrians.Background.Count), TextStyleCatalog);
		AddFieldRow(WidgetTree, backgroundBody, TEXT("speed_mps"), FormatNumberValue(pedestrians.Background.SpeedMetersPerSecond, TEXT("m/s")), TextStyleCatalog);
		AddFieldRow(WidgetTree, backgroundBody, TEXT("spawn_zone.segments"), FormatStringList(pedestrians.Background.SpawnSegmentIds), TextStyleCatalog);

		UVerticalBox* encountersBody = AddBlockWidget(
			WidgetTree,
			pedestrianBody,
			TEXT("encounters"),
			TEXT("root.pedestrians.encounters[]"),
			TEXT("Property"),
			TextStyleCatalog,
			false,
			true);
		for (const FScenarioTemplatePedestrianEncounter& encounter : pedestrians.Encounters)
		{
			UVerticalBox* encounterBody = AddBlockWidget(
				WidgetTree,
				encountersBody,
				encounter.EncounterId.IsEmpty() ? FString(TEXT("(unnamed)")) : encounter.EncounterId,
				EncounterTypeToString(encounter.Type),
				TEXT("Detail"),
				TextStyleCatalog,
				false,
				true);
			AddFieldRow(WidgetTree, encounterBody, TEXT("type"), EncounterTypeToString(encounter.Type), TextStyleCatalog);
			AddFieldRow(WidgetTree, encounterBody, TEXT("at"), encounter.AtSegmentId, TextStyleCatalog);
			AddFieldRow(WidgetTree, encounterBody, TEXT("persona"), encounter.PersonaId, TextStyleCatalog);
			AddFieldRow(WidgetTree, encounterBody, TEXT("meet_offset_m"), FormatNumberValue(encounter.MeetOffsetMeters, TEXT("m")), TextStyleCatalog);
			AddFieldRow(WidgetTree, encounterBody, TEXT("overrides.cooperation"), FormatNumberValue(encounter.Overrides.Cooperation), TextStyleCatalog);
		}
		break;
	}
	default:
		break;
	}

	return panelRoot;
}

void UScenarioEditorSidebarWidget::RefreshPanelSwitcher()
{
	if (!PanelSwitcher)
	{
		return;
	}

	if (UWidget* panelWidget = ResolvePanelWidget(ActivePanel))
	{
		ApplyPanelContentPadding(panelWidget);
		PanelSwitcher->SetActiveWidget(panelWidget);
	}
}

void UScenarioEditorSidebarWidget::ApplyPanelContentPadding(UWidget* panelWidget) const
{
	if (UWidgetSwitcherSlot* slot = panelWidget ? Cast<UWidgetSwitcherSlot>(panelWidget->Slot) : nullptr)
	{
		slot->SetPadding(FMargin(0.0f, SidebarPanelContentTopPadding, 0.0f, 0.0f));
		slot->SetHorizontalAlignment(HAlign_Fill);
		slot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UScenarioEditorSidebarWidget::RefreshFallbackTextVisibility() const
{
	const bool bHasActivePanelWidget = PanelSwitcher && ResolvePanelWidget(ActivePanel);
	const ESlateVisibility visibility = bHasActivePanelWidget
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible;

	SetFallbackTextVisibility(visibility);
}

void UScenarioEditorSidebarWidget::SetFallbackTextVisibility(const ESlateVisibility visibility) const
{
	if (FallbackSummaryContainer)
	{
		FallbackSummaryContainer->SetVisibility(visibility);
		return;
	}

	if (PrimaryFieldsTextBlock)
	{
		PrimaryFieldsTextBlock->SetVisibility(visibility);
	}
	if (SecondaryFieldsTextBlock)
	{
		SecondaryFieldsTextBlock->SetVisibility(visibility);
	}
	if (ListSummaryTextBlock)
	{
		ListSummaryTextBlock->SetVisibility(visibility);
	}
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(visibility);
	}
}

UWidget* UScenarioEditorSidebarWidget::ResolvePanelWidget(
	const EScenarioTemplateSidebarPanel panel) const
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		return MainPanelWidget ? Cast<UWidget>(MainPanelWidget.Get()) : GeneratedMainPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Corridor:
		return GeneratedCorridorPanelWidget ? GeneratedCorridorPanelWidget.Get() : CorridorPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Obstacle:
		return GeneratedObstaclePanelWidget ? GeneratedObstaclePanelWidget.Get() : ObstaclePanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return GeneratedPedestrianPanelWidget ? GeneratedPedestrianPanelWidget.Get() : PedestrianPanelWidget.Get();
	default:
		return nullptr;
	}
}

void UScenarioEditorSidebarWidget::BuildMainPanelText(
	const FScenarioTemplateDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	outPrimaryText = FString::Printf(
		TEXT("schema: %s\nversion: %d\ntemplate_id: %s"),
		*scenarioTemplate.Schema,
		scenarioTemplate.Version,
		scenarioTemplate.TemplateId.IsEmpty() ? TEXT("(unset)") : *scenarioTemplate.TemplateId);

	outSecondaryText = FString::Printf(
		TEXT("intent: %s"),
		scenarioTemplate.Intent.IsEmpty() ? TEXT("(unset)") : *scenarioTemplate.Intent);

	TArray<FString> robotLines;
	robotLines.Add(FString::Printf(TEXT("start: %s"), *FormatRobotAnchor(scenarioTemplate.Robot.Start)));
	robotLines.Add(FString::Printf(TEXT("goal: %s"), *FormatRobotAnchor(scenarioTemplate.Robot.Goal)));
	outListText = JoinLines(robotLines);
}

void UScenarioEditorSidebarWidget::BuildCorridorPanelText(
	const FScenarioTemplateDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;
	outPrimaryText = FString::Printf(
		TEXT("axis: %d point(s), %s\nwalkway_width: %s"),
		corridor.Axis.PointsMeters.Num(),
		*FormatMeters(MeasureAxisLengthMeters(corridor.Axis.PointsMeters)),
		*FormatNumberValue(corridor.WalkwayWidthMeters, TEXT("m")));

	TArray<FString> laneLines;
	laneLines.Add(FString::Printf(TEXT("building_side: %d lane(s)"), corridor.BuildingSide.Num()));
	for (const FScenarioTemplateLaneRule& lane : corridor.BuildingSide)
	{
		laneLines.Add(FString::Printf(TEXT("  - %s"), *FormatLaneRule(lane)));
	}
	laneLines.Add(FString::Printf(TEXT("curb_side: %d lane(s)"), corridor.CurbSide.Num()));
	for (const FScenarioTemplateLaneRule& lane : corridor.CurbSide)
	{
		laneLines.Add(FString::Printf(TEXT("  - %s"), *FormatLaneRule(lane)));
	}
	outSecondaryText = JoinLines(laneLines);

	TArray<FString> segmentLines;
	for (const FScenarioTemplateSegment& segment : corridor.Segments)
	{
		segmentLines.Add(FString::Printf(
			TEXT("%s | %s | %.2f..%.2fm | replace: %s"),
			segment.SegmentId.IsEmpty() ? TEXT("(unnamed)") : *segment.SegmentId,
			*SegmentTypeToString(segment.Type),
			segment.AlongRangeMeters.StartMeters,
			segment.AlongRangeMeters.EndMeters,
			*FormatStringValue(segment.ReplacedBySurfaceId)));
	}
	outListText = JoinLines(segmentLines);
}

void UScenarioEditorSidebarWidget::BuildObstaclePanelText(
	const FScenarioTemplateDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	const FScenarioTemplateObstacleRules& obstacles = scenarioTemplate.Obstacles;
	outPrimaryText = FString::Printf(
		TEXT("min_clear_width: %s\nplacements: %d"),
		*FormatNumberValue(obstacles.MinClearWidthMeters, TEXT("m")),
		obstacles.Placements.Num());

	outSecondaryText = TEXT("fixed/pattern/scatter placement rules");

	TArray<FString> placementLines;
	for (const FScenarioTemplateObstaclePlacement& placement : obstacles.Placements)
	{
		placementLines.Add(FString::Printf(
			TEXT("%s | %s | prop: %s | segment: %s | along: %s | offset: %s"),
			placement.PlacementId.IsEmpty() ? TEXT("(unnamed)") : *placement.PlacementId,
			*ObstaclePlacementKindToString(placement.Kind),
			placement.PropId.IsEmpty() ? TEXT("(unset)") : *placement.PropId,
			placement.At.SegmentId.IsEmpty() ? TEXT("(unset)") : *placement.At.SegmentId,
			*FormatNumberValue(placement.At.AlongMeters, TEXT("m")),
			*FormatNumberValue(placement.At.OffsetMeters, TEXT("m"))));
	}
	outListText = JoinLines(placementLines);
}

void UScenarioEditorSidebarWidget::BuildPedestrianPanelText(
	const FScenarioTemplateDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	const FScenarioTemplatePedestrianRules& pedestrians = scenarioTemplate.Pedestrians;
	outPrimaryText = FString::Printf(
		TEXT("background_count: %s\nbackground_speed: %s\nencounters: %d"),
		*FormatIntegerValue(pedestrians.Background.Count),
		*FormatNumberValue(pedestrians.Background.SpeedMetersPerSecond, TEXT("m/s")),
		pedestrians.Encounters.Num());

	outSecondaryText = FString::Printf(
		TEXT("spawn_segments: %s"),
		*FormatStringList(pedestrians.Background.SpawnSegmentIds));

	TArray<FString> encounterLines;
	for (const FScenarioTemplatePedestrianEncounter& encounter : pedestrians.Encounters)
	{
		encounterLines.Add(FString::Printf(
			TEXT("%s | %s | segment: %s | persona: %s | meet_offset: %s"),
			encounter.EncounterId.IsEmpty() ? TEXT("(unnamed)") : *encounter.EncounterId,
			*EncounterTypeToString(encounter.Type),
			encounter.AtSegmentId.IsEmpty() ? TEXT("(unset)") : *encounter.AtSegmentId,
			encounter.PersonaId.IsEmpty() ? TEXT("(unset)") : *encounter.PersonaId,
			*FormatNumberValue(encounter.MeetOffsetMeters, TEXT("m"))));
	}
	outListText = JoinLines(encounterLines);
}

void UScenarioEditorSidebarWidget::SetSidebarText(
	const FString& title,
	const FString& primaryText,
	const FString& secondaryText,
	const FString& listText,
	const FString& diagnosticsText)
{
	SetTextBlockText(PanelTitleTextBlock.Get(), title);
	SetTextBlockText(PrimaryFieldsTextBlock.Get(), primaryText);
	SetTextBlockText(SecondaryFieldsTextBlock.Get(), secondaryText);
	SetTextBlockText(ListSummaryTextBlock.Get(), listText);
	SetTextBlockText(DiagnosticsTextBlock.Get(), diagnosticsText);
}

void UScenarioEditorSidebarWidget::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}

void UScenarioEditorSidebarWidget::ApplyTextStyles()
{
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		PanelTitleTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Title);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		PrimaryFieldsTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		SecondaryFieldsTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		ListSummaryTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		DiagnosticsTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	if (MainPanelWidget)
	{
		MainPanelWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (WidgetTree)
	{
		UWidgetTextStyleCatalog::ApplyWidgetTreeTextStyles(WidgetTree, TextStyleCatalog);
	}
}

FString UScenarioEditorSidebarWidget::PanelToTitle(const EScenarioTemplateSidebarPanel panel)
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		return TEXT("Main");
	case EScenarioTemplateSidebarPanel::Corridor:
		return TEXT("Corridor");
	case EScenarioTemplateSidebarPanel::Obstacle:
		return TEXT("Obstacle");
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return TEXT("Pedestrian");
	default:
		return TEXT("Scenario Template");
	}
}

FString UScenarioEditorSidebarWidget::RobotAnchorTypeToString(const EScenarioTemplateRobotAnchorType type)
{
	switch (type)
	{
	case EScenarioTemplateRobotAnchorType::Entry:
		return TEXT("entry");
	case EScenarioTemplateRobotAnchorType::Exit:
		return TEXT("exit");
	case EScenarioTemplateRobotAnchorType::CorridorPose:
		return TEXT("corridor_pose");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::RobotHeadingToString(const EScenarioTemplateRobotHeading heading)
{
	switch (heading)
	{
	case EScenarioTemplateRobotHeading::Forward:
		return TEXT("forward");
	case EScenarioTemplateRobotHeading::Backward:
		return TEXT("backward");
	case EScenarioTemplateRobotHeading::Auto:
		return TEXT("auto");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::SegmentTypeToString(const EScenarioTemplateSegmentType type)
{
	switch (type)
	{
	case EScenarioTemplateSegmentType::Straight:
		return TEXT("straight");
	case EScenarioTemplateSegmentType::Narrowing:
		return TEXT("narrowing");
	case EScenarioTemplateSegmentType::Crosswalk:
		return TEXT("crosswalk");
	case EScenarioTemplateSegmentType::Entrance:
		return TEXT("entrance");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::ObstaclePlacementKindToString(
	const EScenarioTemplateObstaclePlacementKind kind)
{
	switch (kind)
	{
	case EScenarioTemplateObstaclePlacementKind::Fixed:
		return TEXT("fixed");
	case EScenarioTemplateObstaclePlacementKind::Pattern:
		return TEXT("pattern");
	case EScenarioTemplateObstaclePlacementKind::Scatter:
		return TEXT("scatter");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::EncounterTypeToString(const EScenarioTemplateEncounterType type)
{
	switch (type)
	{
	case EScenarioTemplateEncounterType::OncomingPass:
		return TEXT("oncoming_pass");
	case EScenarioTemplateEncounterType::Overtake:
		return TEXT("overtake");
	case EScenarioTemplateEncounterType::CrossPath:
		return TEXT("cross_path");
	case EScenarioTemplateEncounterType::StandingGroup:
		return TEXT("standing_group");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::FormatNumberValue(
	const FScenarioTemplateNumberValue& value,
	const FString& suffix)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%.2f..%.2f%s"), value.MinValue, value.MaxValue, *suffix);
	}

	return FString::Printf(TEXT("%.2f%s"), value.FixedValue, *suffix);
}

FString UScenarioEditorSidebarWidget::FormatIntegerValue(const FScenarioTemplateIntegerValue& value)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%d..%d"), value.MinValue, value.MaxValue);
	}

	return FString::FromInt(value.FixedValue);
}

FString UScenarioEditorSidebarWidget::FormatStringValue(const FScenarioTemplateStringValue& value)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateStringValueMode::Choices)
	{
		return FString::Printf(TEXT("[%s]"), *FormatStringList(value.Choices));
	}

	return value.FixedValue.IsEmpty() ? FString(TEXT("(empty)")) : value.FixedValue;
}

FString UScenarioEditorSidebarWidget::FormatRobotAnchor(const FScenarioTemplateRobotAnchor& anchor)
{
	if (anchor.Type != EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		return FString::Printf(
			TEXT("%s | heading: %s"),
			*RobotAnchorTypeToString(anchor.Type),
			*RobotHeadingToString(anchor.Heading));
	}

	return FString::Printf(
		TEXT("corridor_pose | segment: %s | along: %s | offset: %s | lane: %s | heading: %s"),
		anchor.SegmentId.IsEmpty() ? TEXT("(unset)") : *anchor.SegmentId,
		*FormatNumberValue(anchor.AlongMeters, TEXT("m")),
		*FormatNumberValue(anchor.OffsetMeters, TEXT("m")),
		anchor.LaneId.IsEmpty() ? TEXT("(unset)") : *anchor.LaneId,
		*RobotHeadingToString(anchor.Heading));
}

FString UScenarioEditorSidebarWidget::FormatLaneRule(const FScenarioTemplateLaneRule& lane)
{
	return FString::Printf(
		TEXT("%s | width: %s"),
		lane.SurfaceId.IsEmpty() ? TEXT("(unset)") : *lane.SurfaceId,
		*FormatNumberValue(lane.WidthMeters, TEXT("m")));
}

FString UScenarioEditorSidebarWidget::FormatStringList(const TArray<FString>& values)
{
	return values.IsEmpty() ? FString(TEXT("(none)")) : FString::Join(values, TEXT(", "));
}

double UScenarioEditorSidebarWidget::MeasureAxisLengthMeters(const TArray<FVector2D>& pointsMeters)
{
	double lengthMeters = 0.0;
	for (int32 index = 1; index < pointsMeters.Num(); ++index)
	{
		lengthMeters += FVector2D::Distance(pointsMeters[index - 1], pointsMeters[index]);
	}
	return lengthMeters;
}
