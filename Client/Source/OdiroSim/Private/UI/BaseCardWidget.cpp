#include "UI/BaseCardWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseMetricCardWidget.h"
#include "UI/BaseWidgetPrivate.h"

namespace
{
	// Blends a neutral card line toward a semantic marker color.
	FLinearColor MixCardLineColor(const FLinearColor& neutralColor, const FLinearColor& markerColor)
	{
		return FMath::Lerp(neutralColor, markerColor, 0.65f);
	}
}

void UBaseCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const bool bStateDisabled = bDisabled || State == EBaseWidgetState::Disabled;
	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		if (bStateDisabled)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(ResolveStateColor(EBaseWidgetState::Disabled)));
		}
	}
	if (DescriptionTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(DescriptionTextBlock.Get(), Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
		if (bStateDisabled)
		{
			DescriptionTextBlock->SetColorAndOpacity(FSlateColor(ResolveStateColor(EBaseWidgetState::Disabled)));
		}
	}

	if (!tokens)
	{
		return;
	}

	FLinearColor surfaceColor = tokens->SurfacePanelColor;
	FLinearColor frameColor = tokens->LineFieldColor;
	const bool bVariantColored = Variant != EBaseWidgetVariant::Neutral && Variant != EBaseWidgetVariant::Ghost;
	const FLinearColor variantColor = ResolveVariantColor(Variant);
	if (State == EBaseWidgetState::Hovered)
	{
		surfaceColor = tokens->SurfaceHoverColor;
	}
	else if (bStateDisabled)
	{
		surfaceColor = tokens->SurfaceControlColor;
	}
	if (bSelected)
	{
		frameColor = tokens->AccentColor;
	}
	else if (bStateDisabled)
	{
		frameColor = tokens->LineSubtleColor;
	}
	else if (State == EBaseWidgetState::Hovered)
	{
		frameColor = tokens->LineFieldHoverColor;
	}
	else if (State != EBaseWidgetState::Default && State != EBaseWidgetState::Hovered)
	{
		frameColor = ResolveStateColor(State);
	}
	else if (bVariantColored)
	{
		frameColor = MixCardLineColor(tokens->LineFieldColor, variantColor);
	}

	BaseWidgetPrivate::ApplyRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		tokens->Radius,
		tokens->BorderWidth);

	if (StateMarker)
	{
		FLinearColor markerColor = variantColor;
		bool bShowMarker = bVariantColored;
		if (bSelected)
		{
			markerColor = ResolveStateColor(EBaseWidgetState::Selected);
			bShowMarker = true;
		}
		if (bStateDisabled)
		{
			markerColor = ResolveStateColor(EBaseWidgetState::Disabled);
			bShowMarker = true;
		}
		else if (State != EBaseWidgetState::Default && State != EBaseWidgetState::Hovered)
		{
			markerColor = ResolveStateColor(State);
			bShowMarker = true;
		}
		ApplyBorderColor(StateMarker.Get(), markerColor);
		StateMarker->SetVisibility(bShowMarker
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

int32 UBaseCardWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseCardWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetDescription(const FText inDescription)
{
	Description = inDescription;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetVariant(const EBaseWidgetVariant inVariant)
{
	Variant = inVariant;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseMetricCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (ValueTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(ValueTextBlock.Get(), ValueText);
		ApplyTextStyle(ValueTextBlock.Get(), EBaseTextRole::Value);
	}
}

void UBaseMetricCardWidget::SetValueText(const FText inValueText)
{
	ValueText = inValueText;
	SynchronizeBaseProperties();
}
