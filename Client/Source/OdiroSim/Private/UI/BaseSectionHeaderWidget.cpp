#include "UI/BaseSectionHeaderWidget.h"

#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseSectionHeaderWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Title);
	}
	if (DescriptionTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(DescriptionTextBlock.Get(), Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
	}
}

void UBaseSectionHeaderWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseSectionHeaderWidget::SetDescription(const FText inDescription)
{
	Description = inDescription;
	SynchronizeBaseProperties();
}
