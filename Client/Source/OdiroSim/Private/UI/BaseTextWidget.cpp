#include "UI/BaseTextWidget.h"

#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseTextWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (TextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(TextBlock.Get(), Text);
		ApplyTextStyle(TextBlock.Get(), TextRole);
		if (bDisabled)
		{
			TextBlock->SetColorAndOpacity(FSlateColor(ResolveStateColor(EBaseWidgetState::Disabled)));
		}
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
}
