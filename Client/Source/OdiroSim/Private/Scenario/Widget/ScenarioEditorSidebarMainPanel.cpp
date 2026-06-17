#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Styling/SlateBrush.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float MainPanelBlockPadding = 10.0f;
	constexpr float MainPanelOutlineThickness = 1.0f;
	const FLinearColor MainPanelBlockColor(0.14f, 0.17f, 0.20f, 0.96f);
	const FLinearColor MainPanelNestedBlockColor(0.17f, 0.20f, 0.24f, 0.96f);
	const FLinearColor MainPanelOutlineColor(0.27f, 0.33f, 0.39f, 1.0f);
	const FLinearColor MainPanelActiveOutlineColor(0.28f, 0.65f, 1.0f, 1.0f);

	FString JoinMainPanelDiagnostics(const TArray<FString>& diagnostics)
	{
		return diagnostics.IsEmpty() ? FString(TEXT("Unknown edit failure.")) : FString::Join(diagnostics, TEXT("\n"));
	}

	FSlateBrush MakeMainPanelColorBrush(const FLinearColor& color)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(color);
		return brush;
	}

	void ApplyMainPanelBorderFill(UBorder* border, const FLinearColor& color, const FMargin& padding)
	{
		if (!border)
		{
			return;
		}

		border->SetBrush(MakeMainPanelColorBrush(color));
		border->SetBrushColor(color);
		border->SetPadding(padding);
	}

	UTextBlock* MakeMainPanelText(
		UWidgetTree* widgetTree,
		const FString& text,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalog,
		const EWidgetTextStyleRole role)
	{
		UTextBlock* textBlock = widgetTree
			? widgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
			: nullptr;
		if (!textBlock)
		{
			return nullptr;
		}

		textBlock->SetText(FText::FromString(text));
		UWidgetTextStyleCatalog::ApplyTextBlockStyle(textBlock, catalog, role);
		return textBlock;
	}

	void AddMainPanelTextToRow(
		UHorizontalBox* row,
		UTextBlock* textBlock,
		const ESlateSizeRule::Type sizeRule,
		const FMargin& padding)
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

	void AddMainPanelWidgetToBox(UVerticalBox* box, UWidget* widget, const FMargin& padding = FMargin())
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

	UVerticalBox* AddMainPanelBlock(
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
		ApplyMainPanelBorderFill(
			outlineBorder,
			bHighlighted ? MainPanelActiveOutlineColor : MainPanelOutlineColor,
			FMargin(MainPanelOutlineThickness));

		UBorder* contentBorder = widgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ApplyMainPanelBorderFill(
			contentBorder,
			bNested ? MainPanelNestedBlockColor : MainPanelBlockColor,
			FMargin(MainPanelBlockPadding));
		outlineBorder->SetContent(contentBorder);

		UVerticalBox* blockBox = widgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		contentBorder->SetContent(blockBox);

		UHorizontalBox* headerRow = widgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddMainPanelTextToRow(
			headerRow,
			MakeMainPanelText(widgetTree, TEXT("▼"), catalog, EWidgetTextStyleRole::Label),
			ESlateSizeRule::Automatic,
			FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddMainPanelTextToRow(
			headerRow,
			MakeMainPanelText(widgetTree, name, catalog, EWidgetTextStyleRole::Label),
			ESlateSizeRule::Automatic,
			FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddMainPanelTextToRow(
			headerRow,
			MakeMainPanelText(widgetTree, path, catalog, EWidgetTextStyleRole::Caption),
			ESlateSizeRule::Fill,
			FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddMainPanelTextToRow(
			headerRow,
			MakeMainPanelText(widgetTree, badge, catalog, EWidgetTextStyleRole::Label),
			ESlateSizeRule::Automatic,
			FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		AddMainPanelWidgetToBox(blockBox, headerRow);

		UVerticalBox* body = widgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddMainPanelWidgetToBox(blockBox, body, FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		AddMainPanelWidgetToBox(parent, outlineBorder, FMargin(0.0f, 0.0f, 0.0f, MainPanelBlockPadding));
		return body;
	}

	UVerticalBox* AddMainPanelBlockWidget(
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
		blockWidget->SetShowNormalOutline(bShowNormalOutline);
		blockWidget->SetNested(bNested);
		blockWidget->SetExpanded(bExpanded);
		AddMainPanelWidgetToBox(parent, blockWidget.Get(), FMargin(0.0f, 0.0f, 0.0f, MainPanelBlockPadding));
		return blockWidget->GetBodyBox();
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarMainPanel::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarMainPanel::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarMainPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarMainPanel::RefreshFromDraft()
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarMainPanel::RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	if (SchemaFieldRow)
	{
		SchemaFieldRow->SetValueText(scenarioTemplate.Schema);
	}
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->SetValueText(scenarioTemplate.TemplateId);
	}
	if (VersionFieldRow)
	{
		VersionFieldRow->SetValueText(FString::FromInt(scenarioTemplate.Version));
	}
	if (IntentFieldRow)
	{
		IntentFieldRow->SetValueText(scenarioTemplate.Intent);
	}
	if (RobotStartFieldRow)
	{
		RobotStartFieldRow->SetValueText(FormatRobotAnchor(scenarioTemplate.Robot.Start));
	}
	if (RobotGoalFieldRow)
	{
		RobotGoalFieldRow->SetValueText(FormatRobotAnchor(scenarioTemplate.Robot.Goal));
	}
	RefreshRobotAnchorRows(
		scenarioTemplate.Robot.Start,
		RobotStartTypeFieldRow.Get(),
		RobotStartSegmentFieldRow.Get(),
		RobotStartAlongFieldRow.Get(),
		RobotStartOffsetFieldRow.Get(),
		RobotStartLaneFieldRow.Get(),
		RobotStartHeadingFieldRow.Get());
	RefreshRobotAnchorRows(
		scenarioTemplate.Robot.Goal,
		RobotGoalTypeFieldRow.Get(),
		RobotGoalSegmentFieldRow.Get(),
		RobotGoalAlongFieldRow.Get(),
		RobotGoalOffsetFieldRow.Get(),
		RobotGoalLaneFieldRow.Get(),
		RobotGoalHeadingFieldRow.Get());

	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitTemplateIdText(text);
}

void UScenarioEditorSidebarMainPanel::HandleIntentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitIntentText(text);
}

void UScenarioEditorSidebarMainPanel::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedMainPanelRoot"));
	if (!rootBox)
	{
		return;
	}

	WidgetTree->RootWidget = rootBox;

	UVerticalBox* rootBody = AddMainPanelBlockWidget(
		WidgetTree,
		rootBox,
		RootBlockWidget,
		TEXT("RootBlockWidget"),
		TEXT("Root"),
		TEXT("scenario_template"),
		TEXT("Main"),
		TextStyleCatalog,
		true);
	auto addFieldRow = [this](
		UVerticalBox* parentBox,
		TObjectPtr<UScenarioEditorSidebarFieldRow>& fieldRow,
		const TCHAR* widgetName)
	{
		if (!parentBox)
		{
			return;
		}

		fieldRow = WidgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
			UScenarioEditorSidebarFieldRow::StaticClass(),
			FName(widgetName));
		if (!fieldRow)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = parentBox->AddChildToVerticalBox(fieldRow))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	};

	addFieldRow(rootBody, SchemaFieldRow, TEXT("SchemaFieldRow"));
	addFieldRow(rootBody, VersionFieldRow, TEXT("VersionFieldRow"));
	addFieldRow(rootBody, TemplateIdFieldRow, TEXT("TemplateIdFieldRow"));
	addFieldRow(rootBody, IntentFieldRow, TEXT("IntentFieldRow"));

	UVerticalBox* robotBody = AddMainPanelBlockWidget(
		WidgetTree,
		rootBody,
		RobotBlockWidget,
		TEXT("RobotBlockWidget"),
		TEXT("robot"),
		TEXT("root.robot"),
		TEXT("Template"),
		TextStyleCatalog,
		false,
		true);
	UVerticalBox* robotStartBody = AddMainPanelBlockWidget(
		WidgetTree,
		robotBody,
		RobotStartBlockWidget,
		TEXT("RobotStartBlockWidget"),
		TEXT("start"),
		TEXT("root.robot.start"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);
	UVerticalBox* robotGoalBody = AddMainPanelBlockWidget(
		WidgetTree,
		robotBody,
		RobotGoalBlockWidget,
		TEXT("RobotGoalBlockWidget"),
		TEXT("goal"),
		TEXT("root.robot.goal"),
		TEXT("Property"),
		TextStyleCatalog,
		false,
		true,
		true,
		false);

	addFieldRow(robotStartBody, RobotStartTypeFieldRow, TEXT("RobotStartTypeFieldRow"));
	addFieldRow(robotStartBody, RobotStartSegmentFieldRow, TEXT("RobotStartSegmentFieldRow"));
	addFieldRow(robotStartBody, RobotStartAlongFieldRow, TEXT("RobotStartAlongFieldRow"));
	addFieldRow(robotStartBody, RobotStartOffsetFieldRow, TEXT("RobotStartOffsetFieldRow"));
	addFieldRow(robotStartBody, RobotStartLaneFieldRow, TEXT("RobotStartLaneFieldRow"));
	addFieldRow(robotStartBody, RobotStartHeadingFieldRow, TEXT("RobotStartHeadingFieldRow"));
	addFieldRow(robotGoalBody, RobotGoalTypeFieldRow, TEXT("RobotGoalTypeFieldRow"));
	addFieldRow(robotGoalBody, RobotGoalSegmentFieldRow, TEXT("RobotGoalSegmentFieldRow"));
	addFieldRow(robotGoalBody, RobotGoalAlongFieldRow, TEXT("RobotGoalAlongFieldRow"));
	addFieldRow(robotGoalBody, RobotGoalOffsetFieldRow, TEXT("RobotGoalOffsetFieldRow"));
	addFieldRow(robotGoalBody, RobotGoalLaneFieldRow, TEXT("RobotGoalLaneFieldRow"));
	addFieldRow(robotGoalBody, RobotGoalHeadingFieldRow, TEXT("RobotGoalHeadingFieldRow"));

	DiagnosticsTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DiagnosticsTextBlock"));
	if (DiagnosticsTextBlock)
	{
		if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(DiagnosticsTextBlock))
		{
			slot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}
	}
}

void UScenarioEditorSidebarMainPanel::BindFieldRows()
{
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted);
		TemplateIdFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted);
	}

	if (IntentFieldRow)
	{
		IntentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
		IntentFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
	}
}

void UScenarioEditorSidebarMainPanel::UnbindFieldRows()
{
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted);
	}

	if (IntentFieldRow)
	{
		IntentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
	}
}

void UScenarioEditorSidebarMainPanel::ConfigureFieldRows()
{
	if (RootBlockWidget)
	{
		RootBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RootBlockWidget->SetBlockMetadata(TEXT("Root"), TEXT("scenario_template"), TEXT("Main"));
		RootBlockWidget->SetSelected(true);
		RootBlockWidget->SetNested(false);
	}
	if (RobotBlockWidget)
	{
		RobotBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RobotBlockWidget->SetBlockMetadata(TEXT("robot"), TEXT("root.robot"), TEXT("Template"));
		RobotBlockWidget->SetSelected(false);
		RobotBlockWidget->SetNested(true);
	}
	if (RobotStartBlockWidget)
	{
		RobotStartBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RobotStartBlockWidget->SetBlockMetadata(TEXT("start"), TEXT("root.robot.start"), TEXT("Property"));
		RobotStartBlockWidget->SetNested(true);
	}
	if (RobotGoalBlockWidget)
	{
		RobotGoalBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RobotGoalBlockWidget->SetBlockMetadata(TEXT("goal"), TEXT("root.robot.goal"), TEXT("Property"));
		RobotGoalBlockWidget->SetNested(true);
	}
	if (SchemaFieldRow)
	{
		SchemaFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		SchemaFieldRow->SetFieldLabel(TEXT("schema"));
		SchemaFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		SchemaFieldRow->SetEditable(false);
	}
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		TemplateIdFieldRow->SetFieldLabel(TEXT("template_id"));
		TemplateIdFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		TemplateIdFieldRow->SetEditable(true);
	}
	if (VersionFieldRow)
	{
		VersionFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		VersionFieldRow->SetFieldLabel(TEXT("version"));
		VersionFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Integer);
		VersionFieldRow->SetEditable(false);
	}
	if (IntentFieldRow)
	{
		IntentFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		IntentFieldRow->SetFieldLabel(TEXT("intent"));
		IntentFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::MultilineText);
		IntentFieldRow->SetEditable(true);
	}
	if (RobotStartFieldRow)
	{
		RobotStartFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		RobotStartFieldRow->SetFieldLabel(TEXT("robot.start"));
		RobotStartFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		RobotStartFieldRow->SetEditable(false);
	}
	if (RobotGoalFieldRow)
	{
		RobotGoalFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		RobotGoalFieldRow->SetFieldLabel(TEXT("robot.goal"));
		RobotGoalFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		RobotGoalFieldRow->SetEditable(false);
	}
	ConfigureRobotAnchorRows(
		RobotStartTypeFieldRow.Get(),
		RobotStartSegmentFieldRow.Get(),
		RobotStartAlongFieldRow.Get(),
		RobotStartOffsetFieldRow.Get(),
		RobotStartLaneFieldRow.Get(),
		RobotStartHeadingFieldRow.Get());
	ConfigureRobotAnchorRows(
		RobotGoalTypeFieldRow.Get(),
		RobotGoalSegmentFieldRow.Get(),
		RobotGoalAlongFieldRow.Get(),
		RobotGoalOffsetFieldRow.Get(),
		RobotGoalLaneFieldRow.Get(),
		RobotGoalHeadingFieldRow.Get());
	ApplyTextStyles();
}

void UScenarioEditorSidebarMainPanel::ApplyTextStyles()
{
	if (RootBlockWidget)
	{
		RootBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotBlockWidget)
	{
		RobotBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotStartBlockWidget)
	{
		RobotStartBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotGoalBlockWidget)
	{
		RobotGoalBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (SchemaFieldRow)
	{
		SchemaFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (VersionFieldRow)
	{
		VersionFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (IntentFieldRow)
	{
		IntentFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotStartFieldRow)
	{
		RobotStartFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotGoalFieldRow)
	{
		RobotGoalFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		RobotStartTypeFieldRow.Get(),
		RobotStartSegmentFieldRow.Get(),
		RobotStartAlongFieldRow.Get(),
		RobotStartOffsetFieldRow.Get(),
		RobotStartLaneFieldRow.Get(),
		RobotStartHeadingFieldRow.Get(),
		RobotGoalTypeFieldRow.Get(),
		RobotGoalSegmentFieldRow.Get(),
		RobotGoalAlongFieldRow.Get(),
		RobotGoalOffsetFieldRow.Get(),
		RobotGoalLaneFieldRow.Get(),
		RobotGoalHeadingFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	if (DiagnosticsTextBlock)
	{
		UWidgetTextStyleCatalog::ApplyTextBlockStyle(
			DiagnosticsTextBlock.Get(),
			TextStyleCatalog,
			EWidgetTextStyleRole::Value);
	}
	if (WidgetTree)
	{
		UWidgetTextStyleCatalog::ApplyWidgetTreeTextStyles(WidgetTree, TextStyleCatalog);
	}
}

void UScenarioEditorSidebarMainPanel::ConfigureRobotAnchorRows(
	UScenarioEditorSidebarFieldRow* typeRow,
	UScenarioEditorSidebarFieldRow* segmentRow,
	UScenarioEditorSidebarFieldRow* alongRow,
	UScenarioEditorSidebarFieldRow* offsetRow,
	UScenarioEditorSidebarFieldRow* laneRow,
	UScenarioEditorSidebarFieldRow* headingRow)
{
	struct FAnchorRowConfig
	{
		// Row widget receiving label and editability setup.
		UScenarioEditorSidebarFieldRow* Row = nullptr;
		// Scenario Template detail field label.
		const TCHAR* Label = TEXT("");
		// Preferred editor type for this detail field.
		EScenarioEditorSidebarFieldInputType InputType = EScenarioEditorSidebarFieldInputType::Text;
	};

	for (const FAnchorRowConfig& config : {
		FAnchorRowConfig{ typeRow, TEXT("type"), EScenarioEditorSidebarFieldInputType::EnumText },
		FAnchorRowConfig{ segmentRow, TEXT("segment"), EScenarioEditorSidebarFieldInputType::Text },
		FAnchorRowConfig{ alongRow, TEXT("along_m"), EScenarioEditorSidebarFieldInputType::Range },
		FAnchorRowConfig{ offsetRow, TEXT("offset_m"), EScenarioEditorSidebarFieldInputType::Range },
		FAnchorRowConfig{ laneRow, TEXT("lane"), EScenarioEditorSidebarFieldInputType::EnumText },
		FAnchorRowConfig{ headingRow, TEXT("heading"), EScenarioEditorSidebarFieldInputType::EnumText } })
	{
		if (!config.Row)
		{
			continue;
		}

		config.Row->SetTextStyleCatalog(TextStyleCatalog);
		config.Row->SetFieldLabel(config.Label);
		config.Row->SetInputType(config.InputType);
		config.Row->SetEditable(false);
	}
}

void UScenarioEditorSidebarMainPanel::RefreshRobotAnchorRows(
	const FScenarioTemplateRobotAnchor& anchor,
	UScenarioEditorSidebarFieldRow* typeRow,
	UScenarioEditorSidebarFieldRow* segmentRow,
	UScenarioEditorSidebarFieldRow* alongRow,
	UScenarioEditorSidebarFieldRow* offsetRow,
	UScenarioEditorSidebarFieldRow* laneRow,
	UScenarioEditorSidebarFieldRow* headingRow) const
{
	const bool bUsesCorridorPose = anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose;
	const ESlateVisibility poseVisibility = bUsesCorridorPose
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	if (typeRow)
	{
		typeRow->SetValueText(RobotAnchorTypeToString(anchor.Type));
		typeRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (segmentRow)
	{
		segmentRow->SetValueText(anchor.SegmentId.IsEmpty() ? FString(TEXT("(unset)")) : anchor.SegmentId);
		segmentRow->SetVisibility(poseVisibility);
	}
	if (alongRow)
	{
		alongRow->SetValueText(FormatNumberValue(anchor.AlongMeters, TEXT("m")));
		alongRow->SetVisibility(poseVisibility);
	}
	if (offsetRow)
	{
		offsetRow->SetValueText(FormatNumberValue(anchor.OffsetMeters, TEXT("m")));
		offsetRow->SetVisibility(poseVisibility);
	}
	if (laneRow)
	{
		laneRow->SetValueText(anchor.LaneId.IsEmpty() ? FString(TEXT("(unset)")) : anchor.LaneId);
		laneRow->SetVisibility(poseVisibility);
	}
	if (headingRow)
	{
		headingRow->SetValueText(RobotHeadingToString(anchor.Heading));
		headingRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

UScenarioAuthoringSubsystem* UScenarioEditorSidebarMainPanel::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

void UScenarioEditorSidebarMainPanel::CommitTemplateIdText(const FText& text)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetDraftTemplateId(text.ToString(), diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinMainPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::CommitIntentText(const FText& text)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetDraftIntent(text.ToString(), diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinMainPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}

FString UScenarioEditorSidebarMainPanel::RobotAnchorTypeToString(
	const EScenarioTemplateRobotAnchorType type)
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

FString UScenarioEditorSidebarMainPanel::RobotHeadingToString(
	const EScenarioTemplateRobotHeading heading)
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

FString UScenarioEditorSidebarMainPanel::FormatNumberValue(
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

FString UScenarioEditorSidebarMainPanel::FormatRobotAnchor(
	const FScenarioTemplateRobotAnchor& anchor)
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
