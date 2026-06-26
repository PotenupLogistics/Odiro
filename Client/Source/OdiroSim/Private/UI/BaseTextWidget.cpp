#include "UI/BaseTextWidget.h"

#include "Components/TextBlock.h"

UBaseTextWidget::UBaseTextWidget()
	: Text(FText::FromString(TEXT("Base text")))
{
}

void UBaseTextWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (TextBlock)
	{
		TextBlock->SetText(Text);
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

void UBaseTextWidget::SetBaseSize(const EBaseWidgetSize inSize)
{
	Size = inSize;
	SynchronizeBaseProperties();
}

void UBaseTextWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}
