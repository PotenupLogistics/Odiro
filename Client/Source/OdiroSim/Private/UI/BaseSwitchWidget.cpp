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

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (!tokens)
	{
		return;
	}

	const bool bChecked = CheckState == ECheckBoxState::Checked;
	const FLinearColor trackFill = bChecked ? tokens->AccentColor : tokens->SurfaceControlColor;
	const FLinearColor trackStroke = bChecked ? tokens->AccentColor : tokens->LineInsetColor;
	BaseWidgetPrivate::ApplyRoundedSurface(
		nullptr,
		TrackSurface.Get(),
		bDisabled ? tokens->SurfaceChromeColor : trackFill,
		bDisabled ? tokens->LineSubtleColor : trackStroke,
		tokens->Radius,
		tokens->BorderWidth);

	ApplyThumbAlignment(ThumbSlot.Get(), bChecked);
	BaseWidgetPrivate::ApplyRoundedSurface(
		nullptr,
		ThumbSurface.Get(),
		bDisabled ? tokens->TextFaintColor : tokens->TextStrongColor,
		FLinearColor::Transparent,
		tokens->Radius,
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
}

void UBaseSwitchWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}
