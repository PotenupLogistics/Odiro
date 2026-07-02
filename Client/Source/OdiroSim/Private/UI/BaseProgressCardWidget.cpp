#include "UI/BaseProgressCardWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseProgressBarWidget.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseProgressCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (colors && sizes)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			colors->SurfacePanelColor,
			colors->LineFieldColor,
			sizes->Radius,
			sizes->BorderWidth);
	}
	if (SurfaceBorder)
	{
		SurfaceBorder->SetVerticalAlignment(BaseWidgetPrivate::ToSlateVerticalAlignment(ContentVAlign));
	}

	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
	}
	if (DescriptionTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(DescriptionTextBlock.Get(), Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
	}
	if (ValueTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(ValueTextBlock.Get(), ValueText);
		ApplyTextStyle(ValueTextBlock.Get(), EBaseTextRole::Caption);
	}
	if (ProgressBar)
	{
		ProgressBar->SetColorsOverride(ColorsOverride);
		ProgressBar->SetSizesOverride(SizesOverride);
		ProgressBar->SetProgressPercent(ProgressPercent);
		ProgressBar->SetBaseState(State);
	}
}

int32 UBaseProgressCardWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseProgressCardWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseProgressCardWidget::SetDescription(const FText inDescription)
{
	Description = inDescription;
	SynchronizeBaseProperties();
}

void UBaseProgressCardWidget::SetProgressPercent(const float inProgressPercent)
{
	ProgressPercent = FMath::Clamp(inProgressPercent, 0.0f, 100.0f);
	SynchronizeBaseProperties();
}

void UBaseProgressCardWidget::SetValueText(const FText inValueText)
{
	ValueText = inValueText;
	SynchronizeBaseProperties();
}

void UBaseProgressCardWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseProgressCardWidget::SetContentVAlign(const EBaseVerticalContentAlign inContentVAlign)
{
	ContentVAlign = inContentVAlign;
	SynchronizeBaseProperties();
}
