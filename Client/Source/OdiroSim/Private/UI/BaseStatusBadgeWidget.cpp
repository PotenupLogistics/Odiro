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

	if (tokens)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			tokens->SurfaceControlColor,
			tokens->LineFieldColor,
			tokens->Radius,
			tokens->BorderWidth);
	}
	else
	{
		ApplyBorderColor(SurfaceBorder.Get(), ResolveVariantColor(EBaseWidgetVariant::Secondary));
	}
	ApplyBorderColor(StatusDot.Get(), badgeColor);
}

int32 UBaseStatusBadgeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
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
