#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	// Creates text-only button chrome for compact row actions.
	FButtonStyle MakeFieldActionButtonStyle()
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

	// Adds a compact control to a horizontal row without consuming fill width.
	void AddAutoWidthWidget(UHorizontalBox* row, UWidget* widget, const FMargin& padding)
	{
		if (!row || !widget)
		{
			return;
		}

		if (UHorizontalBoxSlot* slot = row->AddChildToHorizontalBox(widget))
		{
			slot->SetPadding(padding);
			slot->SetVerticalAlignment(VAlign_Center);
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarFieldRow::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarFieldRow::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarFieldRow::SetFieldLabel(const FString& label)
{
	FieldLabel = label;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetValueText(const FString& text)
{
	ValueText = text;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetRangeValueText(const FString& minText, const FString& maxText)
{
	MinValueText = minText;
	MaxValueText = maxText;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetInputType(const EScenarioEditorSidebarFieldInputType inInputType)
{
	InputType = inInputType;
	bMultilineValue = inInputType == EScenarioEditorSidebarFieldInputType::MultilineText;
	if (!IsRangeCapable())
	{
		bRangeInputEnabled = false;
	}
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetEditable(const bool bInEditable)
{
	bEditable = bInEditable;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetMultilineValue(const bool bInMultilineValue)
{
	bMultilineValue = bInMultilineValue;
	InputType = bInMultilineValue
		? EScenarioEditorSidebarFieldInputType::MultilineText
		: EScenarioEditorSidebarFieldInputType::Text;
	bRangeInputEnabled = false;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetRangeInputEnabled(const bool bInRangeInputEnabled)
{
	bRangeInputEnabled = bInRangeInputEnabled && IsRangeCapable();
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetArrayControlsEnabled(const bool bInArrayControlsEnabled)
{
	bArrayControlsEnabled = bInArrayControlsEnabled;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	RefreshRow();
}

FString UScenarioEditorSidebarFieldRow::GetValueText() const
{
	if (UsesRangeInput())
	{
		return FString::Printf(TEXT("%s..%s"), *GetMinValueText(), *GetMaxValueText());
	}
	if (bEditable && UsesMultilineInput() && ValueMultiLineEditableTextBox)
	{
		return ValueMultiLineEditableTextBox->GetText().ToString();
	}
	if (bEditable && !UsesMultilineInput() && ValueEditableTextBox)
	{
		return ValueEditableTextBox->GetText().ToString();
	}

	return ValueText;
}

FString UScenarioEditorSidebarFieldRow::GetMinValueText() const
{
	return MinValueEditableTextBox ? MinValueEditableTextBox->GetText().ToString() : MinValueText;
}

FString UScenarioEditorSidebarFieldRow::GetMaxValueText() const
{
	return MaxValueEditableTextBox ? MaxValueEditableTextBox->GetText().ToString() : MaxValueText;
}

void UScenarioEditorSidebarFieldRow::HandleValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	ValueText = text.ToString();
	OnValueTextCommitted.Broadcast(text, commitMethod);
}

void UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	MinValueText = text.ToString();
	OnRangeValueTextCommitted.Broadcast(text, FText::FromString(GetMaxValueText()), commitMethod);
}

void UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	MaxValueText = text.ToString();
	OnRangeValueTextCommitted.Broadcast(FText::FromString(GetMinValueText()), text, commitMethod);
}

void UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked()
{
	SetRangeInputEnabled(!bRangeInputEnabled);
}

void UScenarioEditorSidebarFieldRow::HandleAddItemClicked()
{
	OnAddItemRequested.Broadcast();
}

void UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked()
{
	OnRemoveItemRequested.Broadcast();
}

void UScenarioEditorSidebarFieldRow::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UHorizontalBox* rootBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("GeneratedFieldRowRoot"));
	if (!rootBox)
	{
		return;
	}

	WidgetTree->RootWidget = rootBox;

	LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("LabelTextBlock"));
	SeparatorTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SeparatorTextBlock"));
	ValueTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ValueTextBlock"));
	ValueEditableTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("ValueEditableTextBox"));
	ValueMultiLineSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("ValueMultiLineSizeBox"));
	ValueMultiLineEditableTextBox = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(
		UMultiLineEditableTextBox::StaticClass(),
		TEXT("ValueMultiLineEditableTextBox"));
	ValueRangeBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("ValueRangeBox"));
	MinValueEditableTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("MinValueEditableTextBox"));
	RangeSeparatorTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("RangeSeparatorTextBlock"));
	MaxValueEditableTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("MaxValueEditableTextBox"));
	RangeToggleButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("RangeToggleButton"));
	RangeToggleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("RangeToggleTextBlock"));
	AddItemButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("AddItemButton"));
	AddItemTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("AddItemTextBlock"));
	RemoveItemButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("RemoveItemButton"));
	RemoveItemTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("RemoveItemTextBlock"));

	if (LabelTextBlock)
	{
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(LabelTextBlock))
		{
			slot->SetPadding(FMargin(0.0f, 2.0f, 6.0f, 2.0f));
			slot->SetVerticalAlignment(VAlign_Top);
		}
	}
	if (SeparatorTextBlock)
	{
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(SeparatorTextBlock))
		{
			slot->SetPadding(FMargin(0.0f, 2.0f, 8.0f, 2.0f));
			slot->SetVerticalAlignment(VAlign_Top);
		}
	}
	if (ValueTextBlock)
	{
		ValueTextBlock->SetAutoWrapText(true);
		ValueTextBlock->SetJustification(ETextJustify::Right);
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(ValueTextBlock))
		{
			slot->SetPadding(FMargin(8.0f, 2.0f, 0.0f, 2.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->SetMinDesiredWidth(160.0f);
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(ValueEditableTextBox))
		{
			slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (ValueMultiLineSizeBox && ValueMultiLineEditableTextBox)
	{
		ValueMultiLineSizeBox->ClearHeightOverride();
		ValueMultiLineSizeBox->ClearMinDesiredWidth();
		ValueMultiLineSizeBox->SetMinDesiredHeight(MultilineValueHeight);
		ValueMultiLineEditableTextBox->SetAutoWrapText(true);
		ValueMultiLineSizeBox->SetContent(ValueMultiLineEditableTextBox);
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(ValueMultiLineSizeBox))
		{
			slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (ValueRangeBox && MinValueEditableTextBox && RangeSeparatorTextBlock && MaxValueEditableTextBox)
	{
		MinValueEditableTextBox->SetMinDesiredWidth(72.0f);
		MaxValueEditableTextBox->SetMinDesiredWidth(72.0f);
		AddAutoWidthWidget(ValueRangeBox.Get(), MinValueEditableTextBox.Get(), FMargin());
		AddAutoWidthWidget(ValueRangeBox.Get(), RangeSeparatorTextBlock.Get(), FMargin(6.0f, 2.0f));
		AddAutoWidthWidget(ValueRangeBox.Get(), MaxValueEditableTextBox.Get(), FMargin());
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(ValueRangeBox.Get()))
		{
			slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			slot->SetHorizontalAlignment(HAlign_Fill);
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (RangeToggleButton && RangeToggleTextBlock)
	{
		RangeToggleButton->SetStyle(MakeFieldActionButtonStyle());
		RangeToggleButton->SetContent(RangeToggleTextBlock.Get());
		AddAutoWidthWidget(rootBox, RangeToggleButton.Get(), FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}
	if (AddItemButton && AddItemTextBlock)
	{
		AddItemButton->SetStyle(MakeFieldActionButtonStyle());
		AddItemButton->SetContent(AddItemTextBlock.Get());
		AddAutoWidthWidget(rootBox, AddItemButton.Get(), FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}
	if (RemoveItemButton && RemoveItemTextBlock)
	{
		RemoveItemButton->SetStyle(MakeFieldActionButtonStyle());
		RemoveItemButton->SetContent(RemoveItemTextBlock.Get());
		AddAutoWidthWidget(rootBox, RemoveItemButton.Get(), FMargin(4.0f, 0.0f, 0.0f, 0.0f));
	}
}

void UScenarioEditorSidebarFieldRow::BindControls()
{
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
		ValueMultiLineEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (MinValueEditableTextBox)
	{
		MinValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted);
		MinValueEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted);
	}
	if (MaxValueEditableTextBox)
	{
		MaxValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted);
		MaxValueEditableTextBox->OnTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
		RangeToggleButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
	}
	if (AddItemButton)
	{
		AddItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
		AddItemButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
		RemoveItemButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
	}
}

void UScenarioEditorSidebarFieldRow::UnbindControls()
{
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
	if (MinValueEditableTextBox)
	{
		MinValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMinValueTextCommitted);
	}
	if (MaxValueEditableTextBox)
	{
		MaxValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleMaxValueTextCommitted);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRangeToggleClicked);
	}
	if (AddItemButton)
	{
		AddItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleAddItemClicked);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleRemoveItemClicked);
	}
}

void UScenarioEditorSidebarFieldRow::RefreshRow()
{
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		LabelTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		SeparatorTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		ValueTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(
		ValueEditableTextBox.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyMultiLineEditableTextBoxStyle(
		ValueMultiLineEditableTextBox.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(
		MinValueEditableTextBox.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(
		MaxValueEditableTextBox.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		RangeSeparatorTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		RangeToggleTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		AddItemTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		RemoveItemTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);

	SetTextBlockText(LabelTextBlock.Get(), FieldLabel);
	SetTextBlockText(SeparatorTextBlock.Get(), TEXT(":"));
	SetTextBlockText(ValueTextBlock.Get(), ValueText);
	SetTextBlockText(RangeSeparatorTextBlock.Get(), TEXT(".."));
	SetTextBlockText(RangeToggleTextBlock.Get(), bRangeInputEnabled ? TEXT("1") : TEXT("2"));
	SetTextBlockText(AddItemTextBlock.Get(), TEXT("+"));
	SetTextBlockText(RemoveItemTextBlock.Get(), TEXT("-"));

	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->SetText(FText::FromString(ValueText));
		ValueEditableTextBox->SetVisibility(bEditable && !UsesMultilineInput() && !UsesRangeInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->SetText(FText::FromString(ValueText));
		ValueMultiLineEditableTextBox->SetAutoWrapText(true);
		ValueMultiLineEditableTextBox->SetVisibility(bEditable && UsesMultilineInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueMultiLineSizeBox)
	{
		ValueMultiLineSizeBox->ClearHeightOverride();
		ValueMultiLineSizeBox->ClearMinDesiredWidth();
		ValueMultiLineSizeBox->SetMinDesiredHeight(MultilineValueHeight);
		ValueMultiLineSizeBox->SetVisibility(bEditable && UsesMultilineInput() && ValueMultiLineEditableTextBox
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (MinValueEditableTextBox)
	{
		MinValueEditableTextBox->SetText(FText::FromString(MinValueText));
	}
	if (MaxValueEditableTextBox)
	{
		MaxValueEditableTextBox->SetText(FText::FromString(MaxValueText));
	}
	if (ValueRangeBox)
	{
		ValueRangeBox->SetVisibility(UsesRangeInput()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (RangeToggleButton)
	{
		RangeToggleButton->SetVisibility(bEditable && IsRangeCapable()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (AddItemButton)
	{
		AddItemButton->SetVisibility(bArrayControlsEnabled
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (RemoveItemButton)
	{
		RemoveItemButton->SetVisibility(bArrayControlsEnabled
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueTextBlock)
	{
		ValueTextBlock->SetAutoWrapText(true);
		const bool bHasEditableControl = bEditable
			&& ((!UsesMultilineInput() && !UsesRangeInput() && ValueEditableTextBox)
				|| (UsesMultilineInput() && ValueMultiLineEditableTextBox)
				|| (UsesRangeInput() && ValueRangeBox));
		ValueTextBlock->SetVisibility(bHasEditableControl
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
}

bool UScenarioEditorSidebarFieldRow::UsesMultilineInput() const
{
	return bMultilineValue || InputType == EScenarioEditorSidebarFieldInputType::MultilineText;
}

bool UScenarioEditorSidebarFieldRow::IsRangeCapable() const
{
	return InputType == EScenarioEditorSidebarFieldInputType::Range;
}

bool UScenarioEditorSidebarFieldRow::UsesRangeInput() const
{
	return bEditable && IsRangeCapable() && bRangeInputEnabled;
}

void UScenarioEditorSidebarFieldRow::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}
