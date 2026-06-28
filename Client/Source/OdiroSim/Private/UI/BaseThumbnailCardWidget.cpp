#include "UI/BaseThumbnailCardWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseThumbnailCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	SetImageTexture(MediaImage.Get(), MediaTexture.Get());
	SetOptionalWidgetVisible(MediaOverlay.Get(), bShowMedia);
	if (MediaBorder && tokens)
	{
		MediaBorder->SetPadding(MediaPaddingMode == EBaseThumbnailMediaPaddingMode::Inset
			? FMargin(tokens->Space4)
			: FMargin());
	}
	if (tokens)
	{
		FLinearColor fillColor = tokens->SurfacePanelColor;
		FLinearColor strokeColor = tokens->LineFieldColor;
		if (bDisabled)
		{
			fillColor = tokens->SurfaceChromeColor;
			strokeColor = tokens->LineSubtleColor;
		}
		else if (bSelected)
		{
			strokeColor = tokens->AccentColor;
		}

		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			tokens->Radius,
			tokens->BorderWidth);
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
