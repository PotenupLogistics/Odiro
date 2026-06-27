#include "UI/BaseStatusBadgeWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseStatusBadgeWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const FLinearColor badgeColor = bDisabled
		? ResolveStateColor(EBaseWidgetState::Disabled)
		: ResolveStateColor(State);
	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Caption);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(bDisabled && tokens
			? tokens->TextFaintColor
			: badgeColor));
	}

	BaseWidgetPrivate::MakeBorderVisualTransparent(BorderFrame.Get());
	BaseWidgetPrivate::MakeBorderVisualTransparent(SurfaceBorder.Get());
	ApplyBorderColor(StatusDot.Get(), badgeColor);
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
