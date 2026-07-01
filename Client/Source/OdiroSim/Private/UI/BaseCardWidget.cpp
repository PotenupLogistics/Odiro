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

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	const bool bStateDisabled = bDisabled || State == EBaseWidgetState::Disabled;
	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		if (bStateDisabled && colors)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(colors->GetStateColor(EBaseWidgetState::Disabled)));
		}
	}
	if (DescriptionTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(DescriptionTextBlock.Get(), Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
		if (bStateDisabled && colors)
		{
			DescriptionTextBlock->SetColorAndOpacity(FSlateColor(colors->GetStateColor(EBaseWidgetState::Disabled)));
		}
	}

	if (!colors || !sizes)
	{
		return;
	}

	FLinearColor surfaceColor = colors->SurfacePanelColor;
	FLinearColor frameColor = colors->LineFieldColor;
	const bool bVariantColored = Variant != EBaseWidgetVariant::Neutral && Variant != EBaseWidgetVariant::Ghost;
	const FLinearColor variantColor = colors->GetVariantColor(Variant);
	if (bStateDisabled)
	{
		surfaceColor = colors->SurfaceControlColor;
	}
	if (bSelected)
	{
		frameColor = colors->AccentColor;
	}
	else if (bStateDisabled)
	{
		frameColor = colors->LineSubtleColor;
	}
	else if (State != EBaseWidgetState::Default && State != EBaseWidgetState::Hovered)
	{
		frameColor = colors->GetStateColor(State);
	}
	else if (bVariantColored)
	{
		frameColor = MixCardLineColor(colors->LineFieldColor, variantColor);
	}

	BaseWidgetPrivate::ApplyRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		sizes->Radius,
		sizes->BorderWidth);

	if (StateMarker)
	{
		FLinearColor markerColor = variantColor;
		bool bShowMarker = bVariantColored;
		if (bSelected)
		{
			markerColor = colors->GetStateColor(EBaseWidgetState::Selected);
			bShowMarker = true;
		}
		if (bStateDisabled)
		{
			markerColor = colors->GetStateColor(EBaseWidgetState::Disabled);
			bShowMarker = true;
		}
		else if (State != EBaseWidgetState::Default && State != EBaseWidgetState::Hovered)
		{
			markerColor = colors->GetStateColor(State);
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
	NotifyBaseVisualStateChanged(State);
}

void UBaseCardWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bSelected ? EBaseWidgetState::Selected : State);
}

void UBaseCardWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : State);
}

void UBaseMetricCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	if (SurfaceBorder)
	{
		SurfaceBorder->SetVerticalAlignment(BaseWidgetPrivate::ToSlateVerticalAlignment(ContentVAlign));
	}

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

void UBaseMetricCardWidget::SetContentVAlign(const EBaseVerticalContentAlign inContentVAlign)
{
	ContentVAlign = inContentVAlign;
	SynchronizeBaseProperties();
}
