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

	FScenarioTemplateNumberValue MakeUnsetMainPanelNumberValue()
	{
		return FScenarioTemplateNumberValue();
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

void UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Type,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Segment,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Along,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Along,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Lane,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Heading,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Type,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Segment,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Along,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Along,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Lane,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Heading,
		text,
		commitMethod);
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
		TEXT("scenario"),
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

	if (RobotStartTypeFieldRow)
	{
		RobotStartTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted);
		RobotStartTypeFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted);
	}
	if (RobotStartSegmentFieldRow)
	{
		RobotStartSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted);
		RobotStartSegmentFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted);
	}
	if (RobotStartAlongFieldRow)
	{
		RobotStartAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted);
		RobotStartAlongFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted);
		RobotStartAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted);
		RobotStartAlongFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted);
	}
	if (RobotStartOffsetFieldRow)
	{
		RobotStartOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted);
		RobotStartOffsetFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted);
		RobotStartOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted);
		RobotStartOffsetFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted);
	}
	if (RobotStartLaneFieldRow)
	{
		RobotStartLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted);
		RobotStartLaneFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted);
	}
	if (RobotStartHeadingFieldRow)
	{
		RobotStartHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted);
		RobotStartHeadingFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted);
	}
	if (RobotGoalTypeFieldRow)
	{
		RobotGoalTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted);
		RobotGoalTypeFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted);
	}
	if (RobotGoalSegmentFieldRow)
	{
		RobotGoalSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted);
		RobotGoalSegmentFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted);
	}
	if (RobotGoalAlongFieldRow)
	{
		RobotGoalAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted);
		RobotGoalAlongFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted);
		RobotGoalAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted);
		RobotGoalAlongFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted);
	}
	if (RobotGoalOffsetFieldRow)
	{
		RobotGoalOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted);
		RobotGoalOffsetFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted);
		RobotGoalOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted);
		RobotGoalOffsetFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted);
	}
	if (RobotGoalLaneFieldRow)
	{
		RobotGoalLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted);
		RobotGoalLaneFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted);
	}
	if (RobotGoalHeadingFieldRow)
	{
		RobotGoalHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted);
		RobotGoalHeadingFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted);
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

	if (RobotStartTypeFieldRow)
	{
		RobotStartTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted);
	}
	if (RobotStartSegmentFieldRow)
	{
		RobotStartSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted);
	}
	if (RobotStartAlongFieldRow)
	{
		RobotStartAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted);
		RobotStartAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted);
	}
	if (RobotStartOffsetFieldRow)
	{
		RobotStartOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted);
		RobotStartOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted);
	}
	if (RobotStartLaneFieldRow)
	{
		RobotStartLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted);
	}
	if (RobotStartHeadingFieldRow)
	{
		RobotStartHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted);
	}
	if (RobotGoalTypeFieldRow)
	{
		RobotGoalTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted);
	}
	if (RobotGoalSegmentFieldRow)
	{
		RobotGoalSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted);
	}
	if (RobotGoalAlongFieldRow)
	{
		RobotGoalAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted);
		RobotGoalAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted);
	}
	if (RobotGoalOffsetFieldRow)
	{
		RobotGoalOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted);
		RobotGoalOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted);
	}
	if (RobotGoalLaneFieldRow)
	{
		RobotGoalLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted);
	}
	if (RobotGoalHeadingFieldRow)
	{
		RobotGoalHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted);
	}
}

void UScenarioEditorSidebarMainPanel::ConfigureFieldRows()
{
	if (RootBlockWidget)
	{
		RootBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RootBlockWidget->SetBlockMetadata(TEXT("Root"), TEXT("scenario"), TEXT("Main"));
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
		TemplateIdFieldRow->SetFieldLabel(TEXT("scenario_id"));
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
	TArray<FString> anchorTypeOptions;
	anchorTypeOptions.Add(TEXT("entry"));
	anchorTypeOptions.Add(TEXT("exit"));
	anchorTypeOptions.Add(TEXT("corridor_pose"));

	TArray<FString> headingOptions;
	headingOptions.Add(TEXT("forward"));
	headingOptions.Add(TEXT("backward"));
	headingOptions.Add(TEXT("auto"));

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
		FAnchorRowConfig{ typeRow, TEXT("type"), EScenarioEditorSidebarFieldInputType::ComboBox },
		FAnchorRowConfig{ segmentRow, TEXT("segment"), EScenarioEditorSidebarFieldInputType::Text },
		FAnchorRowConfig{ alongRow, TEXT("along_m"), EScenarioEditorSidebarFieldInputType::Range },
		FAnchorRowConfig{ offsetRow, TEXT("offset_m"), EScenarioEditorSidebarFieldInputType::Range },
		FAnchorRowConfig{ laneRow, TEXT("lane"), EScenarioEditorSidebarFieldInputType::EnumText },
		FAnchorRowConfig{ headingRow, TEXT("heading"), EScenarioEditorSidebarFieldInputType::ComboBox } })
	{
		if (!config.Row)
		{
			continue;
		}

		config.Row->SetTextStyleCatalog(TextStyleCatalog);
		config.Row->SetFieldLabel(config.Label);
		config.Row->SetInputType(config.InputType);
		if (config.Row == typeRow)
		{
			config.Row->SetComboOptions(anchorTypeOptions);
			config.Row->SetComboAllowsUnset(false, FString());
		}
		else if (config.Row == headingRow)
		{
			config.Row->SetComboOptions(headingOptions);
			config.Row->SetComboAllowsUnset(false, FString());
		}
		config.Row->SetEditable(true);
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
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed;

	if (typeRow)
	{
		typeRow->SetValueText(RobotAnchorTypeToString(anchor.Type));
		typeRow->SetVisibility(ESlateVisibility::Visible);
	}
	if (segmentRow)
	{
		segmentRow->SetValueText(anchor.SegmentId);
		segmentRow->SetVisibility(poseVisibility);
	}
	if (alongRow)
	{
		SetNumberRowValue(alongRow, anchor.AlongMeters);
		alongRow->SetVisibility(poseVisibility);
	}
	if (offsetRow)
	{
		SetNumberRowValue(offsetRow, anchor.OffsetMeters);
		offsetRow->SetVisibility(poseVisibility);
	}
	if (laneRow)
	{
		laneRow->SetValueText(anchor.LaneId);
		laneRow->SetVisibility(poseVisibility);
	}
	if (headingRow)
	{
		headingRow->SetValueText(RobotHeadingToString(anchor.Heading));
		headingRow->SetVisibility(ESlateVisibility::Visible);
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
	if (!authoringSubsystem->SetDraftScenarioId(text.ToString(), diagnostics))
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

void UScenarioEditorSidebarMainPanel::CommitRobotAnchorText(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& text)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	const FScenarioTemplateDocument scenarioTemplate = authoringSubsystem->GetDraftScenarioTemplate();
	FScenarioTemplateRobotAnchor anchor = target == EScenarioEditorSidebarRobotAnchorTarget::Start
		? scenarioTemplate.Robot.Start
		: scenarioTemplate.Robot.Goal;
	const FString trimmedText = text.ToString().TrimStartAndEnd();

	switch (field)
	{
	case EScenarioEditorSidebarRobotAnchorField::Type:
	{
		EScenarioTemplateRobotAnchorType anchorType = anchor.Type;
		if (!TryParseRobotAnchorType(text, anchorType))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("robot anchor type must be entry, exit, or corridor_pose."));
			return;
		}
		anchor.Type = anchorType;
		if (anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
		{
			if (anchor.SegmentId.IsEmpty() && !scenarioTemplate.Corridor.Segments.IsEmpty())
			{
				anchor.SegmentId = scenarioTemplate.Corridor.Segments[0].SegmentId;
			}
			if (!anchor.AlongMeters.bIsSet)
			{
				anchor.AlongMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
			}
			if (!anchor.OffsetMeters.bIsSet)
			{
				anchor.OffsetMeters = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(0.0);
			}
			if (anchor.LaneId.IsEmpty())
			{
				anchor.LaneId = TEXT("walkway");
			}
		}
		break;
	}
	case EScenarioEditorSidebarRobotAnchorField::Segment:
		anchor.SegmentId = trimmedText;
		break;
	case EScenarioEditorSidebarRobotAnchorField::Along:
	case EScenarioEditorSidebarRobotAnchorField::Offset:
	{
		FScenarioTemplateNumberValue numberValue;
		if (!TryParseOptionalNumber(text, numberValue))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("robot anchor numeric fields must be finite numbers or empty optional values."));
			return;
		}
		if (field == EScenarioEditorSidebarRobotAnchorField::Along)
		{
			anchor.AlongMeters = numberValue;
		}
		else
		{
			anchor.OffsetMeters = numberValue;
		}
		break;
	}
	case EScenarioEditorSidebarRobotAnchorField::Lane:
		anchor.LaneId = trimmedText;
		break;
	case EScenarioEditorSidebarRobotAnchorField::Heading:
	{
		EScenarioTemplateRobotHeading heading = anchor.Heading;
		if (!TryParseRobotHeading(text, heading))
		{
			RefreshFromDraft();
			SetDiagnosticsText(TEXT("robot heading must be forward, backward, or auto."));
			return;
		}
		anchor.Heading = heading;
		break;
	}
	default:
		break;
	}

	CommitRobotAnchorValue(target, anchor);
}

void UScenarioEditorSidebarMainPanel::CommitRobotAnchorRange(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& minText,
	const FText& maxText)
{
	if (field != EScenarioEditorSidebarRobotAnchorField::Along
		&& field != EScenarioEditorSidebarRobotAnchorField::Offset)
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("Only robot along_m and offset_m support range editing."));
		return;
	}

	FScenarioTemplateNumberValue numberValue;
	if (!TryParseOptionalNumberRange(minText, maxText, numberValue))
	{
		RefreshFromDraft();
		SetDiagnosticsText(TEXT("robot anchor range fields must use numeric min/max values."));
		return;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	const FScenarioTemplateDocument scenarioTemplate = authoringSubsystem->GetDraftScenarioTemplate();
	FScenarioTemplateRobotAnchor anchor = target == EScenarioEditorSidebarRobotAnchorTarget::Start
		? scenarioTemplate.Robot.Start
		: scenarioTemplate.Robot.Goal;
	if (field == EScenarioEditorSidebarRobotAnchorField::Along)
	{
		anchor.AlongMeters = numberValue;
	}
	else
	{
		anchor.OffsetMeters = numberValue;
	}

	CommitRobotAnchorValue(target, anchor);
}

void UScenarioEditorSidebarMainPanel::CommitRobotAnchorValue(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const FScenarioTemplateRobotAnchor& anchor)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	const bool bCommitted = target == EScenarioEditorSidebarRobotAnchorTarget::Start
		? authoringSubsystem->SetDraftRobotStartAnchor(anchor, diagnostics)
		: authoringSubsystem->SetDraftRobotGoalAnchor(anchor, diagnostics);
	if (!bCommitted)
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinMainPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::HandleRobotAnchorTextCommitted(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitRobotAnchorText(target, field, text);
}

void UScenarioEditorSidebarMainPanel::HandleRobotAnchorRangeCommitted(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitRobotAnchorRange(target, field, minText, maxText);
}

void UScenarioEditorSidebarMainPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}

void UScenarioEditorSidebarMainPanel::SetNumberRowValue(
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
		fieldRow->SetValueText(FormatNumberValue(UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue((value.MinValue + value.MaxValue) * 0.5)));
		fieldRow->SetRangeValueText(FormatNumberValue(UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(value.MinValue)), FormatNumberValue(UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(value.MaxValue)));
		fieldRow->SetRangeInputEnabled(true);
		return;
	}

	const FString fixedValueText = FormatNumberValue(value);
	fieldRow->SetValueText(fixedValueText);
	fieldRow->SetRangeValueText(fixedValueText, fixedValueText);
	fieldRow->SetRangeInputEnabled(false);
}

bool UScenarioEditorSidebarMainPanel::TryParseOptionalNumber(
	const FText& text,
	FScenarioTemplateNumberValue& outValue)
{
	FString numberText = text.ToString().TrimStartAndEnd();
	if (numberText.IsEmpty())
	{
		outValue = MakeUnsetMainPanelNumberValue();
		return true;
	}

	numberText.RemoveFromEnd(TEXT("m"), ESearchCase::IgnoreCase);
	numberText.TrimStartAndEndInline();
	double parsedValue = 0.0;
	if (!LexTryParseString(parsedValue, *numberText) || !FMath::IsFinite(parsedValue))
	{
		return false;
	}

	outValue = UScenarioAuthoringSubsystem::MakeFixedTemplateNumberValue(parsedValue);
	return true;
}

bool UScenarioEditorSidebarMainPanel::TryParseOptionalNumberRange(
	const FText& minText,
	const FText& maxText,
	FScenarioTemplateNumberValue& outValue)
{
	const bool bMinEmpty = minText.ToString().TrimStartAndEnd().IsEmpty();
	const bool bMaxEmpty = maxText.ToString().TrimStartAndEnd().IsEmpty();
	if (bMinEmpty && bMaxEmpty)
	{
		outValue = MakeUnsetMainPanelNumberValue();
		return true;
	}

	FScenarioTemplateNumberValue minValue;
	FScenarioTemplateNumberValue maxValue;
	if (!TryParseOptionalNumber(minText, minValue)
		|| !TryParseOptionalNumber(maxText, maxValue)
		|| !minValue.bIsSet
		|| !maxValue.bIsSet)
	{
		return false;
	}

	outValue = UScenarioAuthoringSubsystem::MakeRangeTemplateNumberValue(minValue.FixedValue, maxValue.FixedValue);
	return true;
}

bool UScenarioEditorSidebarMainPanel::TryParseRobotAnchorType(
	const FText& text,
	EScenarioTemplateRobotAnchorType& outType)
{
	const FString typeText = text.ToString().TrimStartAndEnd().ToLower();
	if (typeText == TEXT("entry"))
	{
		outType = EScenarioTemplateRobotAnchorType::Entry;
		return true;
	}
	if (typeText == TEXT("exit"))
	{
		outType = EScenarioTemplateRobotAnchorType::Exit;
		return true;
	}
	if (typeText == TEXT("corridor_pose"))
	{
		outType = EScenarioTemplateRobotAnchorType::CorridorPose;
		return true;
	}
	return false;
}

bool UScenarioEditorSidebarMainPanel::TryParseRobotHeading(
	const FText& text,
	EScenarioTemplateRobotHeading& outHeading)
{
	const FString headingText = text.ToString().TrimStartAndEnd().ToLower();
	if (headingText == TEXT("forward"))
	{
		outHeading = EScenarioTemplateRobotHeading::Forward;
		return true;
	}
	if (headingText == TEXT("backward"))
	{
		outHeading = EScenarioTemplateRobotHeading::Backward;
		return true;
	}
	if (headingText == TEXT("auto"))
	{
		outHeading = EScenarioTemplateRobotHeading::Auto;
		return true;
	}
	return false;
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
