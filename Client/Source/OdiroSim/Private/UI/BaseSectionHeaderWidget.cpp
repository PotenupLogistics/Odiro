#include "UI/BaseSectionHeaderWidget.h"

#include "Components/TextBlock.h"

UBaseSectionHeaderWidget::UBaseSectionHeaderWidget()
	: Label(FText::FromString(TEXT("Section")))
	, Description(FText::FromString(TEXT("Grouped controls and dashboard content")))
{
}

void UBaseSectionHeaderWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Title);
	}
	if (DescriptionTextBlock)
	{
		DescriptionTextBlock->SetText(Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
		DescriptionTextBlock->SetVisibility(Description.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
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
