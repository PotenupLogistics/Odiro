#include "UI/BaseStatusBadgeWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

UBaseStatusBadgeWidget::UBaseStatusBadgeWidget()
	: Label(FText::FromString(TEXT("Ready")))
	, State(EBaseWidgetState::Success)
{
}

void UBaseStatusBadgeWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const FLinearColor badgeColor = bDisabled
		? ResolveStateColor(EBaseWidgetState::Disabled)
		: ResolveStateColor(State);
	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Caption);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(bDisabled && tokens
			? tokens->TextFaintColor
			: badgeColor));
	}

	if (tokens)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			tokens->SurfaceControlColor,
			tokens->LineFieldColor,
			tokens->BorderWidth);
	}
	else
	{
		ApplyBorderColor(SurfaceBorder.Get(), ResolveVariantColor(EBaseWidgetVariant::Secondary));
	}
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
