#include "UI/BaseTextWidget.h"

#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseTextWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	if (TextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(TextBlock.Get(), Text);
		ApplyTextStyle(TextBlock.Get(), TextRole);
		if (bDisabled && colors)
		{
			TextBlock->SetColorAndOpacity(FSlateColor(colors->GetStateColor(EBaseWidgetState::Disabled)));
		}
		else if (bHasColorAndOpacityOverride)
		{
			TextBlock->SetColorAndOpacity(FSlateColor(ColorAndOpacityOverride));
		}
		TextBlock->SetAutoWrapText(bAutoWrapText);
	}
}

void UBaseTextWidget::SetText(const FText inText)
{
	Text = inText;
	SynchronizeBaseProperties();
}

void UBaseTextWidget::SetTextRole(const EBaseTextRole inTextRole)
{
	TextRole = inTextRole;
	SynchronizeBaseProperties();
}

void UBaseTextWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : EBaseWidgetState::Default);
}

void UBaseTextWidget::SetColorAndOpacityOverride(const FLinearColor inColorAndOpacity)
{
	ColorAndOpacityOverride = inColorAndOpacity;
	bHasColorAndOpacityOverride = true;
	SynchronizeBaseProperties();
}

void UBaseTextWidget::ClearColorAndOpacityOverride()
{
	bHasColorAndOpacityOverride = false;
	SynchronizeBaseProperties();
}

void UBaseTextWidget::SetAutoWrapText(const bool bInAutoWrapText)
{
	bAutoWrapText = bInAutoWrapText;
	SynchronizeBaseProperties();
}
