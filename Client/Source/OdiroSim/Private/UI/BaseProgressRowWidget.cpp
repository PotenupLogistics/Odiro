#include "UI/BaseProgressRowWidget.h"

#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

UBaseProgressRowWidget::UBaseProgressRowWidget()
	: Label(FText::FromString(TEXT("Episode progress")))
	, Description(FText::FromString(TEXT("24 of 32 episodes")))
	, ProgressPercent(72.0f)
	, ValueText(FText::FromString(TEXT("72%")))
	, State(EBaseWidgetState::Success)
{
}

void UBaseProgressRowWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens())
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			tokens->SurfacePanelColor,
			tokens->LineFieldColor,
			tokens->BorderWidth);
	}

	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
	}
	if (DescriptionTextBlock)
	{
		DescriptionTextBlock->SetText(Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
		DescriptionTextBlock->SetVisibility(Description.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if (ValueTextBlock)
	{
		ValueTextBlock->SetText(ValueText);
		ApplyTextStyle(ValueTextBlock.Get(), EBaseTextRole::Caption);
	}
	if (ProgressBar)
	{
		const float clampedPercent = FMath::Clamp(ProgressPercent, 0.0f, 100.0f) / 100.0f;
		ProgressBar->SetPercent(clampedPercent);
		ProgressBar->SetFillColorAndOpacity(ResolveStateColor(State));
	}
}

void UBaseProgressRowWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseProgressRowWidget::SetDescription(const FText inDescription)
{
	Description = inDescription;
	SynchronizeBaseProperties();
}

void UBaseProgressRowWidget::SetProgressPercent(const float inProgressPercent)
{
	ProgressPercent = FMath::Clamp(inProgressPercent, 0.0f, 100.0f);
	SynchronizeBaseProperties();
}

void UBaseProgressRowWidget::SetValueText(const FText inValueText)
{
	ValueText = inValueText;
	SynchronizeBaseProperties();
}

void UBaseProgressRowWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}
