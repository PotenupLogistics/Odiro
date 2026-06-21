#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "Styling/SlateBrush.h"

namespace
{
	constexpr float RowVerticalPadding = 6.0f;
	constexpr float RowHorizontalPadding = 8.0f;
	constexpr float RowIndentWidth = 14.0f;
	const FLinearColor RowColor(0.10f, 0.12f, 0.15f, 0.92f);
	const FLinearColor RowSelectedColor(0.16f, 0.25f, 0.34f, 0.98f);
	const FLinearColor RowOutlineColor(0.24f, 0.30f, 0.36f, 0.85f);
	const FLinearColor RowSelectedOutlineColor(0.28f, 0.65f, 1.0f, 1.0f);

	FSlateBrush MakeOutlinerBrush(const FLinearColor& color)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(color);
		return brush;
	}

	FButtonStyle MakeTransparentButtonStyle()
	{
		FSlateBrush emptyBrush;
		emptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		emptyBrush.TintColor = FSlateColor(FLinearColor::Transparent);

		return FButtonStyle()
			.SetNormal(emptyBrush)
			.SetHovered(emptyBrush)
			.SetPressed(emptyBrush)
			.SetDisabled(emptyBrush)
			.SetNormalPadding(FMargin())
			.SetPressedPadding(FMargin());
	}

	void AddHeaderChild(
		UHorizontalBox* row,
		UWidget* child,
		const ESlateSizeRule::Type sizeRule,
		const FMargin& padding = FMargin())
	{
		if (!row || !child)
		{
			return;
		}

		if (UHorizontalBoxSlot* slot = row->AddChildToHorizontalBox(child))
		{
			slot->SetSize(FSlateChildSize(sizeRule));
			slot->SetPadding(padding);
			slot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

TSharedRef<SWidget> UScenarioEditorOutlinerRowWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorOutlinerRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorOutlinerRowWidget::InitializeRow(const FScenarioOutlinerItemViewModel& viewModel)
{
	Item = viewModel;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::SetSelected(const bool bInSelected)
{
	Item.bSelected = bInSelected;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::SetExpanded(const bool bInExpanded)
{
	Item.bExpanded = bInExpanded;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::HandleRowClicked()
{
	OnRowSelected.Broadcast(Item);
}

void UScenarioEditorOutlinerRowWidget::HandleExpandClicked()
{
	if (!Item.bExpandable)
	{
		OnRowSelected.Broadcast(Item);
		return;
	}

	Item.bExpanded = !Item.bExpanded;
	RefreshRow();
	OnRowExpansionToggled.Broadcast(Item);
}

void UScenarioEditorOutlinerRowWidget::BindControls()
{
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleRowClicked);
		RowButton->OnClicked.AddDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleRowClicked);
	}
	if (ExpandButton)
	{
		ExpandButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleExpandClicked);
		ExpandButton->OnClicked.AddDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleExpandClicked);
	}
}

void UScenarioEditorOutlinerRowWidget::UnbindControls()
{
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleRowClicked);
	}
	if (ExpandButton)
	{
		ExpandButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleExpandClicked);
	}
}

void UScenarioEditorOutlinerRowWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	SelectionBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("SelectionBorder"));
	if (!SelectionBorder)
	{
		return;
	}
	WidgetTree->RootWidget = SelectionBorder.Get();
	SelectionBorder->SetPadding(FMargin(1.0f));

	UBorder* rowFill = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("GeneratedRowFill"));
	rowFill->SetPadding(FMargin(RowHorizontalPadding, RowVerticalPadding));
	SelectionBorder->SetContent(rowFill);

	RowButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("RowButton"));
	RowButton->SetStyle(MakeTransparentButtonStyle());
	rowFill->SetContent(RowButton.Get());

	UHorizontalBox* headerRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("GeneratedHeaderRow"));
	RowButton->SetContent(headerRow);

	RowIndentSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(),
		TEXT("RowIndentSpacer"));
	AddHeaderChild(headerRow, RowIndentSpacer.Get(), ESlateSizeRule::Automatic);

	ExpandButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("ExpandButton"));
	ExpandButton->SetStyle(MakeTransparentButtonStyle());
	ExpandGlyphText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ExpandGlyphText"));
	ExpandButton->SetContent(ExpandGlyphText.Get());
	AddHeaderChild(headerRow, ExpandButton.Get(), ESlateSizeRule::Automatic, FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	UVerticalBox* textBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedTextBox"));
	AddHeaderChild(headerRow, textBox, ESlateSizeRule::Fill);

	ItemLabelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ItemLabelText"));
	if (UVerticalBoxSlot* slot = textBox->AddChildToVerticalBox(ItemLabelText.Get()))
	{
		slot->SetHorizontalAlignment(HAlign_Fill);
	}

	ItemSubtitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ItemSubtitleText"));
	ItemSubtitleText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* slot = textBox->AddChildToVerticalBox(ItemSubtitleText.Get()))
	{
		slot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		slot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UScenarioEditorOutlinerRowWidget::RefreshRow()
{
	if (SelectionBorder)
	{
		const FLinearColor outlineColor = Item.bSelected ? RowSelectedOutlineColor : RowOutlineColor;
		SelectionBorder->SetBrush(MakeOutlinerBrush(outlineColor));
		SelectionBorder->SetBrushColor(outlineColor);
	}

	if (UBorder* rowFill = SelectionBorder ? Cast<UBorder>(SelectionBorder->GetContent()) : nullptr)
	{
		const FLinearColor fillColor = Item.bSelected ? RowSelectedColor : RowColor;
		rowFill->SetBrush(MakeOutlinerBrush(fillColor));
		rowFill->SetBrushColor(fillColor);
	}

	if (SelectionIndicator)
	{
		SelectionIndicator->SetVisibility(Item.bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	if (RowIndentSpacer)
	{
		RowIndentSpacer->SetSize(FVector2D(FMath::Max(0, Item.Depth) * RowIndentWidth, 1.0f));
	}

	if (ExpandGlyphText)
	{
		const FString glyph = Item.bExpandable
			? (Item.bExpanded ? FString(TEXT("v")) : FString(TEXT(">")))
			: FString(TEXT(""));
		ExpandGlyphText->SetText(FText::FromString(glyph));
	}
	if (ExpandButton)
	{
		ExpandButton->SetVisibility(Item.bExpandable ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	}

	if (ItemLabelText)
	{
		ItemLabelText->SetText(Item.Label);
	}
	if (ItemSubtitleText)
	{
		ItemSubtitleText->SetText(Item.Subtitle);
		ItemSubtitleText->SetVisibility(Item.Subtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	ApplyTextStyles();
}

void UScenarioEditorOutlinerRowWidget::ApplyTextStyles() const
{
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(ItemLabelText.Get(), TextStyleCatalog, EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(ItemSubtitleText.Get(), TextStyleCatalog, EWidgetTextStyleRole::Caption);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(ExpandGlyphText.Get(), TextStyleCatalog, EWidgetTextStyleRole::Caption);
}
