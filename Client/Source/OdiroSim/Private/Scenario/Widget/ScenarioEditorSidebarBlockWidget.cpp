#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float SidebarBlockPadding = 10.0f;
	constexpr float SidebarBlockOutlineThickness = 1.0f;
	const FLinearColor SidebarBlockColor(0.14f, 0.17f, 0.20f, 0.96f);
	const FLinearColor SidebarNestedBlockColor(0.17f, 0.20f, 0.24f, 0.96f);
	const FLinearColor SidebarNestedLeafBlockColor(0.10f, 0.13f, 0.17f, 0.98f);
	const FLinearColor SidebarBlockOutlineColor(0.27f, 0.33f, 0.39f, 1.0f);
	const FLinearColor SidebarTransparentOutlineColor(0.27f, 0.33f, 0.39f, 0.0f);
	const FLinearColor SidebarSelectedBlockOutlineColor(0.28f, 0.65f, 1.0f, 1.0f);

	FSlateBrush MakeBlockBrush(const FLinearColor& color)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(color);
		return brush;
	}

	FButtonStyle MakeIconOnlyButtonStyle()
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
			.SetPressedPadding(FMargin())
			.SetNormalForeground(FSlateColor(FLinearColor::White))
			.SetHoveredForeground(FSlateColor(FLinearColor::White))
			.SetPressedForeground(FSlateColor(FLinearColor::White));
	}

	void ApplyBorderFill(UBorder* border, const FLinearColor& color, const FMargin& padding)
	{
		if (!border)
		{
			return;
		}

		border->SetBrush(MakeBlockBrush(color));
		border->SetBrushColor(color);
		border->SetPadding(padding);
	}

	void AddTextToHeader(
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
			slot->SetVerticalAlignment(VAlign_Center);
			slot->SetSize(FSlateChildSize(sizeRule));
		}
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarBlockWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarBlockWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarBlockWidget::SetBlockMetadata(
	const FString& name,
	const FString& path,
	const FString& badge)
{
	BlockName = name;
	BlockPath = path;
	BadgeText = badge;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetExpanded(const bool bInExpanded)
{
	bExpanded = bInExpanded;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetShowNormalOutline(const bool bInShowNormalOutline)
{
	bShowNormalOutline = bInShowNormalOutline;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetNested(const bool bInNested)
{
	bNested = bInNested;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::AddBodyChild(UWidget* widget)
{
	if (!widget)
	{
		return;
	}

	if (UVerticalBox* bodyBox = GetBodyBox())
	{
		if (UVerticalBoxSlot* slot = bodyBox->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UScenarioEditorSidebarBlockWidget::ClearBodyChildren()
{
	if (UVerticalBox* bodyBox = GetBodyBox())
	{
		bodyBox->ClearChildren();
	}
}

UVerticalBox* UScenarioEditorSidebarBlockWidget::GetBodyBox()
{
	BuildDefaultWidgetTree();
	return BodyBox.Get();
}

void UScenarioEditorSidebarBlockWidget::HandleToggleClicked()
{
	SetExpanded(!bExpanded);
	OnBlockSelected.Broadcast(BlockPath);
}

void UScenarioEditorSidebarBlockWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	OutlineBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("OutlineBorder"));
	if (!OutlineBorder)
	{
		return;
	}

	WidgetTree->RootWidget = OutlineBorder;
	ApplyBorderFill(OutlineBorder.Get(), SidebarBlockOutlineColor, FMargin(SidebarBlockOutlineThickness));

	ContentBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("ContentBorder"));
	ApplyBorderFill(ContentBorder.Get(), SidebarBlockColor, FMargin(SidebarBlockPadding));
	OutlineBorder->SetContent(ContentBorder.Get());

	UVerticalBox* blockBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("BlockRootBox"));
	if (ContentBorder && blockBox)
	{
		ContentBorder->SetContent(blockBox);
	}

	UHorizontalBox* headerRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("HeaderRow"));
	if (blockBox && headerRow)
	{
		if (UVerticalBoxSlot* slot = blockBox->AddChildToVerticalBox(headerRow))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	ToggleButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("ToggleButton"));
	ToggleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ToggleTextBlock"));
	if (ToggleButton && ToggleTextBlock)
	{
		ToggleButton->SetStyle(MakeIconOnlyButtonStyle());
		ToggleButton->SetBackgroundColor(FLinearColor::Transparent);
		ToggleButton->SetContent(ToggleTextBlock.Get());
		if (headerRow)
		{
			if (UHorizontalBoxSlot* slot = headerRow->AddChildToHorizontalBox(ToggleButton.Get()))
			{
				slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
				slot->SetVerticalAlignment(VAlign_Center);
				slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}
	}

	NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("NameTextBlock"));
	PathTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PathTextBlock"));
	BadgeTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BadgeTextBlock"));

	AddTextToHeader(
		headerRow,
		NameTextBlock.Get(),
		ESlateSizeRule::Automatic,
		FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	AddTextToHeader(
		headerRow,
		PathTextBlock.Get(),
		ESlateSizeRule::Fill,
		FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	AddTextToHeader(
		headerRow,
		BadgeTextBlock.Get(),
		ESlateSizeRule::Automatic,
		FMargin());

	BodyBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("BodyBox"));
	if (blockBox && BodyBox)
	{
		if (UVerticalBoxSlot* slot = blockBox->AddChildToVerticalBox(BodyBox.Get()))
		{
			slot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UScenarioEditorSidebarBlockWidget::BindControls()
{
	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
		ToggleButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::UnbindControls()
{
	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::RefreshBlock()
{
	const FLinearColor contentColor = bNested
		? (bShowNormalOutline ? SidebarNestedBlockColor : SidebarNestedLeafBlockColor)
		: SidebarBlockColor;

	ApplyBorderFill(
		OutlineBorder.Get(),
		bSelected
			? SidebarSelectedBlockOutlineColor
			: (bShowNormalOutline ? SidebarBlockOutlineColor : SidebarTransparentOutlineColor),
		FMargin(SidebarBlockOutlineThickness));
	ApplyBorderFill(
		ContentBorder.Get(),
		contentColor,
		FMargin(SidebarBlockPadding));

	SetTextBlockText(ToggleTextBlock.Get(), bExpanded ? TEXT("\u25BC") : TEXT("\u25B6"));
	SetTextBlockText(NameTextBlock.Get(), BlockName);
	SetTextBlockText(PathTextBlock.Get(), BlockPath);
	SetTextBlockText(BadgeTextBlock.Get(), BadgeText);

	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		ToggleTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		NameTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		PathTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		BadgeTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);

	if (PathTextBlock)
	{
		PathTextBlock->SetAutoWrapText(false);
	}
	if (BodyBox)
	{
		BodyBox->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UScenarioEditorSidebarBlockWidget::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}
