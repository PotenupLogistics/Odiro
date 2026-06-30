#include "UI/BaseProgressRowWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseProgressRowWidget::SynchronizeBaseProperties()
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
	if (ProgressTrack && colors && sizes)
	{
		const float clampedPercent = FMath::Clamp(ProgressPercent, 0.0f, 100.0f) / 100.0f;
		BaseWidgetPrivate::ApplyProgressSurface(
			ProgressTrack.Get(),
			colors->SurfaceWellColor,
			colors->GetStateColor(State),
			clampedPercent,
			sizes->RadiusPill);
	}
}

int32 UBaseProgressRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(ProgressTrack.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
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

void UBaseProgressRowWidget::SetContentVAlign(const EBaseVerticalContentAlign inContentVAlign)
{
	ContentVAlign = inContentVAlign;
	SynchronizeBaseProperties();
}
