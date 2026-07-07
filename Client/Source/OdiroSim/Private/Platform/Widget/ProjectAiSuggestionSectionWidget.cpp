#include "Platform/Widget/ProjectAiSuggestionSectionWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/BaseTextWidget.h"

void UProjectAiSuggestionListItemWidget::InitializeListItem(const FString& itemText)
{
	SetItemText(FText::FromString(itemText));
}

void UProjectAiSuggestionListItemWidget::SetItemText(const FText itemText)
{
	SetRuntimeText(ItemText.Get(), itemText.ToString(), true);
}

void UProjectAiSuggestionListItemWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!PreviewItemText.IsEmptyOrWhitespace())
	{
		SetItemText(PreviewItemText);
	}
}

void UProjectAiSuggestionListItemWidget::SetRuntimeText(
	UWidget* textWidget,
	const FString& text,
	const bool bAutoWrap)
{
	if (!textWidget)
	{
		return;
	}

	const FString displayText = text.TrimStartAndEnd();
	if (UBaseTextWidget* baseTextWidget = Cast<UBaseTextWidget>(textWidget))
	{
		baseTextWidget->SetText(FText::FromString(displayText));
		baseTextWidget->SetAutoWrapText(bAutoWrap);
	}
	else if (UTextBlock* textBlock = Cast<UTextBlock>(textWidget))
	{
		textBlock->SetText(FText::FromString(displayText));
		textBlock->SetAutoWrapText(bAutoWrap);
	}

	textWidget->SetVisibility(displayText.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UProjectAiSuggestionSectionWidget::InitializeSection(
	const FString& headerText,
	const TArray<FString>& listItems)
{
	SetRuntimeText(HeaderText.Get(), headerText);

	const bool bRenderedItemWidgets = RebuildListItemWidgets(listItems);
	SetRuntimeText(
		ListText.Get(),
		bRenderedItemWidgets ? FString() : BuildFallbackListText(listItems),
		true);
}

void UProjectAiSuggestionSectionWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!PreviewHeaderText.IsEmptyOrWhitespace())
	{
		SetRuntimeText(HeaderText.Get(), PreviewHeaderText.ToString());
	}
	ApplyPreviewListItemText();
}

bool UProjectAiSuggestionSectionWidget::RebuildListItemWidgets(const TArray<FString>& listItems)
{
	if (!ListItemBox)
	{
		return false;
	}

	const TSubclassOf<UProjectAiSuggestionListItemWidget> itemWidgetClass = ResolveListItemWidgetClass();
	ListItemBox->ClearChildren();
	if (!itemWidgetClass)
	{
		SetOptionalWidgetVisible(ListItemBox.Get(), false);
		return false;
	}

	bool bAddedItem = false;
	for (const FString& rawItem : listItems)
	{
		const FString item = rawItem.TrimStartAndEnd();
		if (item.IsEmpty())
		{
			continue;
		}

		UProjectAiSuggestionListItemWidget* itemWidget =
			CreateWidget<UProjectAiSuggestionListItemWidget>(this, itemWidgetClass);
		if (!itemWidget)
		{
			continue;
		}

		itemWidget->InitializeListItem(item);
		ListItemBox->AddChild(itemWidget);
		bAddedItem = true;
	}

	SetOptionalWidgetVisible(ListItemBox.Get(), bAddedItem);
	return bAddedItem;
}

void UProjectAiSuggestionSectionWidget::ApplyPreviewListItemText()
{
	if (PreviewListItemText.IsEmptyOrWhitespace() || !ListItemBox)
	{
		return;
	}

	for (int32 childIndex = 0; childIndex < ListItemBox->GetChildrenCount(); ++childIndex)
	{
		if (UProjectAiSuggestionListItemWidget* previewItem =
			Cast<UProjectAiSuggestionListItemWidget>(ListItemBox->GetChildAt(childIndex)))
		{
			previewItem->SetItemText(PreviewListItemText);
			return;
		}
	}
}

TSubclassOf<UProjectAiSuggestionListItemWidget> UProjectAiSuggestionSectionWidget::ResolveListItemWidgetClass() const
{
	if (ListItemWidgetClass)
	{
		return ListItemWidgetClass;
	}

	if (!ListItemBox)
	{
		return nullptr;
	}

	for (int32 childIndex = 0; childIndex < ListItemBox->GetChildrenCount(); ++childIndex)
	{
		if (const UProjectAiSuggestionListItemWidget* previewItem =
			Cast<UProjectAiSuggestionListItemWidget>(ListItemBox->GetChildAt(childIndex)))
		{
			return previewItem->GetClass();
		}
	}

	return nullptr;
}

FString UProjectAiSuggestionSectionWidget::BuildFallbackListText(const TArray<FString>& listItems) const
{
	FString output;
	for (const FString& rawItem : listItems)
	{
		const FString item = rawItem.TrimStartAndEnd();
		if (item.IsEmpty())
		{
			continue;
		}

		if (!output.IsEmpty())
		{
			output += TEXT("\n");
		}
		output += FallbackBulletPrefix;
		output += item;
	}
	return output;
}

void UProjectAiSuggestionSectionWidget::SetRuntimeText(
	UWidget* textWidget,
	const FString& text,
	const bool bAutoWrap)
{
	if (!textWidget)
	{
		return;
	}

	const FString displayText = text.TrimStartAndEnd();
	if (UBaseTextWidget* baseTextWidget = Cast<UBaseTextWidget>(textWidget))
	{
		baseTextWidget->SetText(FText::FromString(displayText));
		baseTextWidget->SetAutoWrapText(bAutoWrap);
	}
	else if (UTextBlock* textBlock = Cast<UTextBlock>(textWidget))
	{
		textBlock->SetText(FText::FromString(displayText));
		textBlock->SetAutoWrapText(bAutoWrap);
	}

	textWidget->SetVisibility(displayText.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UProjectAiSuggestionSectionWidget::SetOptionalWidgetVisible(UWidget* widget, const bool bVisible)
{
	if (widget)
	{
		widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
