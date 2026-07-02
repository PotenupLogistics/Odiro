#include "UI/BaseProgressBarWidget.h"

#include "Components/Border.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseProgressBarWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (ProgressTrack && sizes)
	{
		const float clampedPercent = FMath::Clamp(ProgressPercent, 0.0f, 100.0f) / 100.0f;
		const FLinearColor trackColor = bOverrideTrackColor
			? TrackColorOverride
			: (colors ? colors->SurfaceWellColor : FLinearColor::Transparent);
		const FLinearColor fillColor = bOverrideFillColor
			? FillColorOverride
			: (colors ? colors->GetStateColor(State) : FLinearColor::Transparent);
		BaseWidgetPrivate::ApplyProgressSurface(
			ProgressTrack.Get(),
			trackColor,
			fillColor,
			clampedPercent,
			sizes->RadiusPill);
	}
}

int32 UBaseProgressBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(ProgressTrack.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseProgressBarWidget::SetProgressPercent(const float inProgressPercent)
{
	ProgressPercent = FMath::Clamp(inProgressPercent, 0.0f, 100.0f);
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::SetProgressColors(
	const FLinearColor inTrackColor,
	const FLinearColor inFillColor)
{
	bOverrideTrackColor = true;
	TrackColorOverride = inTrackColor;
	bOverrideFillColor = true;
	FillColorOverride = inFillColor;
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::ClearProgressColorOverrides()
{
	bOverrideTrackColor = false;
	bOverrideFillColor = false;
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::SetTrackColorOverride(const FLinearColor inTrackColor)
{
	bOverrideTrackColor = true;
	TrackColorOverride = inTrackColor;
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::ClearTrackColorOverride()
{
	bOverrideTrackColor = false;
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::SetFillColorOverride(const FLinearColor inFillColor)
{
	bOverrideFillColor = true;
	FillColorOverride = inFillColor;
	SynchronizeBaseProperties();
}

void UBaseProgressBarWidget::ClearFillColorOverride()
{
	bOverrideFillColor = false;
	SynchronizeBaseProperties();
}
