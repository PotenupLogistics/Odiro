#include "UI/BaseThumbnailCardWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseThumbnailCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	SetImageTexture(MediaImage.Get(), MediaTexture.Get());
	SetOptionalWidgetVisible(MediaOverlay.Get(), bShowMedia);
	SetOptionalWidgetVisible(ContentSlot.Get(), !bMediaOnly);
	if (RootSize && bMediaOnly)
	{
		RootSize->ClearWidthOverride();
		RootSize->ClearHeightOverride();
	}
	if (MediaSize && bMediaOnly)
	{
		MediaSize->ClearWidthOverride();
		MediaSize->ClearHeightOverride();
	}
	if (MediaOverlay && bMediaOnly)
	{
		if (UVerticalBoxSlot* mediaSlot = Cast<UVerticalBoxSlot>(MediaOverlay->Slot))
		{
			mediaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			mediaSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	if (MediaBorder && sizes)
	{
		MediaBorder->SetPadding(MediaPaddingMode == EBaseThumbnailMediaPaddingMode::Inset
			? FMargin(sizes->Space4)
			: FMargin());
	}
	if (colors && sizes)
	{
		FLinearColor fillColor = colors->SurfacePanelColor;
		FLinearColor strokeColor = colors->LineFieldColor;
		if (bDisabled)
		{
			fillColor = colors->SurfaceChromeColor;
			strokeColor = colors->LineSubtleColor;
		}
		else if (bSelected)
		{
			strokeColor = colors->AccentColor;
		}

		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			sizes->Radius,
			sizes->BorderWidth);
	}
}

int32 UBaseThumbnailCardWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseThumbnailCardWidget::SetMediaTexture(UTexture2D* inMediaTexture)
{
	MediaTexture = inMediaTexture;
	SynchronizeBaseProperties();
}

void UBaseThumbnailCardWidget::SetShowMedia(const bool bInShowMedia)
{
	bShowMedia = bInShowMedia;
	SynchronizeBaseProperties();
}

void UBaseThumbnailCardWidget::SetMediaPaddingMode(const EBaseThumbnailMediaPaddingMode inPaddingMode)
{
	MediaPaddingMode = inPaddingMode;
	SynchronizeBaseProperties();
}

void UBaseThumbnailCardWidget::SetMediaOnly(const bool bInMediaOnly)
{
	bMediaOnly = bInMediaOnly;
	SynchronizeBaseProperties();
}

void UBaseThumbnailCardWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	SynchronizeBaseProperties();
}

void UBaseThumbnailCardWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}
