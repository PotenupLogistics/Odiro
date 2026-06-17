#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"
#include "Widget/WidgetTextStyleCatalog.h"

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

void UScenarioEditorSidebarFieldRow::SetEditable(const bool bInEditable)
{
	bEditable = bInEditable;
	RefreshRow();
}

void UScenarioEditorSidebarFieldRow::SetMultilineValue(const bool bInMultilineValue)
{
	bMultilineValue = bInMultilineValue;
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
	if (bEditable && bMultilineValue && ValueMultiLineEditableTextBox)
	{
		return ValueMultiLineEditableTextBox->GetText().ToString();
	}
	if (bEditable && !bMultilineValue && ValueEditableTextBox)
	{
		return ValueEditableTextBox->GetText().ToString();
	}

	return ValueText;
}

void UScenarioEditorSidebarFieldRow::HandleValueTextCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	ValueText = text.ToString();
	OnValueTextCommitted.Broadcast(text, commitMethod);
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

	SetTextBlockText(LabelTextBlock.Get(), FieldLabel);
	SetTextBlockText(SeparatorTextBlock.Get(), TEXT(":"));
	SetTextBlockText(ValueTextBlock.Get(), ValueText);

	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->SetText(FText::FromString(ValueText));
		ValueEditableTextBox->SetVisibility(bEditable && !bMultilineValue
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueMultiLineEditableTextBox)
	{
		ValueMultiLineEditableTextBox->SetText(FText::FromString(ValueText));
		ValueMultiLineEditableTextBox->SetAutoWrapText(true);
		ValueMultiLineEditableTextBox->SetVisibility(bEditable && bMultilineValue
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueMultiLineSizeBox)
	{
		ValueMultiLineSizeBox->ClearHeightOverride();
		ValueMultiLineSizeBox->ClearMinDesiredWidth();
		ValueMultiLineSizeBox->SetMinDesiredHeight(MultilineValueHeight);
		ValueMultiLineSizeBox->SetVisibility(bEditable && bMultilineValue && ValueMultiLineEditableTextBox
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ValueTextBlock)
	{
		ValueTextBlock->SetAutoWrapText(true);
		const bool bHasEditableControl = bEditable
			&& ((!bMultilineValue && ValueEditableTextBox)
				|| (bMultilineValue && ValueMultiLineEditableTextBox));
		ValueTextBlock->SetVisibility(bHasEditableControl
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
}

void UScenarioEditorSidebarFieldRow::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}
