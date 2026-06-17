#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"

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

FString UScenarioEditorSidebarFieldRow::GetValueText() const
{
	if (ValueEditableTextBox && bEditable)
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
	USpacer* spacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(),
		TEXT("ValueSpacer"));
	ValueTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ValueTextBlock"));
	ValueEditableTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("ValueEditableTextBox"));

	if (LabelTextBlock)
	{
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(LabelTextBlock))
		{
			slot->SetPadding(FMargin(0.0f, 2.0f, 6.0f, 2.0f));
		}
	}
	if (SeparatorTextBlock)
	{
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(SeparatorTextBlock))
		{
			slot->SetPadding(FMargin(0.0f, 2.0f, 8.0f, 2.0f));
		}
	}
	if (spacer)
	{
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(spacer))
		{
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (ValueTextBlock)
	{
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(ValueTextBlock))
		{
			slot->SetPadding(FMargin(8.0f, 2.0f, 0.0f, 2.0f));
		}
	}
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->SetMinDesiredWidth(160.0f);
		if (UHorizontalBoxSlot* slot = rootBox->AddChildToHorizontalBox(ValueEditableTextBox))
		{
			slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
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
}

void UScenarioEditorSidebarFieldRow::UnbindControls()
{
	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->OnTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarFieldRow::HandleValueTextCommitted);
	}
}

void UScenarioEditorSidebarFieldRow::RefreshRow()
{
	SetTextBlockText(LabelTextBlock.Get(), FieldLabel);
	SetTextBlockText(SeparatorTextBlock.Get(), TEXT(":"));
	SetTextBlockText(ValueTextBlock.Get(), ValueText);

	if (ValueEditableTextBox)
	{
		ValueEditableTextBox->SetText(FText::FromString(ValueText));
		ValueEditableTextBox->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ValueTextBlock)
	{
		ValueTextBlock->SetVisibility(bEditable && ValueEditableTextBox
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
