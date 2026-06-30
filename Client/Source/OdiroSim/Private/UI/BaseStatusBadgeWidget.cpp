#include "UI/BaseStatusBadgeWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseStatusBadgeWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Caption);
	}

	BaseWidgetPrivate::MakeBorderVisualTransparent(BorderFrame.Get());
	BaseWidgetPrivate::MakeBorderVisualTransparent(SurfaceBorder.Get());
	if (colors)
	{
		const FLinearColor badgeColor = bDisabled
			? colors->GetStateColor(EBaseWidgetState::Disabled)
			: colors->GetStateColor(State);
		if (LabelTextBlock)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(bDisabled
				? colors->TextFaintColor
				: badgeColor));
		}
		ApplyBorderColor(StatusDot.Get(), badgeColor);
	}
}

void UBaseStatusBadgeWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}
