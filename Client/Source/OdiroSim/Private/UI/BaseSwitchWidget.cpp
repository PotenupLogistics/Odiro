#include "UI/BaseSwitchWidget.h"

#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/Widget.h"
#include "UI/BaseWidgetPrivate.h"

namespace
{
	// Moves the WBP-owned thumb slot without imposing track dimensions in C++.
	void ApplyThumbAlignment(UWidget* thumbSlotWidget, const bool bChecked)
	{
		if (!thumbSlotWidget || !thumbSlotWidget->Slot)
		{
			return;
		}

		const EHorizontalAlignment alignment = bChecked ? HAlign_Right : HAlign_Left;
		if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(thumbSlotWidget->Slot))
		{
			overlaySlot->SetHorizontalAlignment(alignment);
			return;
		}
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(thumbSlotWidget->Slot))
		{
			horizontalSlot->SetHorizontalAlignment(alignment);
		}
	}
}

void UBaseSwitchWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	const bool bChecked = CheckState == ECheckBoxState::Checked;
	ApplyThumbAlignment(ThumbSlot.Get(), bChecked);
	if (!colors || !sizes)
	{
		return;
	}

	const FLinearColor trackFill = bChecked ? colors->AccentColor : colors->SurfaceControlColor;
	const FLinearColor trackStroke = bChecked ? colors->AccentColor : colors->LineInsetColor;
	BaseWidgetPrivate::ApplyRoundedSurface(
		nullptr,
		TrackSurface.Get(),
		bDisabled ? colors->SurfaceChromeColor : trackFill,
		bDisabled ? colors->LineSubtleColor : trackStroke,
		sizes->Radius,
		sizes->BorderWidth);

	BaseWidgetPrivate::ApplyRoundedSurface(
		nullptr,
		ThumbSurface.Get(),
		bDisabled ? colors->TextFaintColor : colors->TextStrongColor,
		FLinearColor::Transparent,
		sizes->Radius,
		0.0f);
}

int32 UBaseSwitchWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(TrackSurface.Get(), TrackSurface ? TrackSurface->GetCachedGeometry().GetLocalSize() : AllottedGeometry.GetLocalSize());
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(ThumbSurface.Get(), ThumbSurface ? ThumbSurface->GetCachedGeometry().GetLocalSize() : AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseSwitchWidget::SetCheckState(const ECheckBoxState inCheckState)
{
	CheckState = inCheckState;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(CheckState == ECheckBoxState::Checked
		? EBaseWidgetState::Selected
		: EBaseWidgetState::Default);
}

void UBaseSwitchWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : EBaseWidgetState::Default);
}
