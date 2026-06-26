#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseWidgetTypes.h"

class UBorder;
class UImage;
class UTextBlock;
class UWidget;

namespace BaseWidgetPrivate
{
	// Resolves a nullable token reference through the project default token catalog.
	const UBaseWidgetTokenCatalog* ResolveBaseTokenCatalog(const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens);

	// Returns a text token through the shared base token resolution path.
	FBaseTextStyleToken ResolveTextStyle(const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens, EBaseTextRole role);

	// Returns a variant color through the shared base token resolution path.
	FLinearColor ResolveVariantColor(const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens, EBaseWidgetVariant variant);

	// Returns a state color through the shared base token resolution path.
	FLinearColor ResolveStateColor(const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens, EBaseWidgetState state);

	// Applies one resolved semantic text token to a text block.
	void ApplyTextStyle(UTextBlock* textBlock, const FBaseTextStyleToken& style);

	// Applies explicit component text while preserving WBP-authored text when the property is unset.
	void ApplyTextIfSet(UTextBlock* textBlock, const FText& text);

	// Returns whether an image has a WBP-authored or runtime-assigned brush resource.
	bool HasAssignedImageResource(const UImage* image);

	// Owns optional icon visibility from image assignment instead of letting WBP visibility drift.
	void SetOptionalIconVisibility(UWidget* iconBox, UImage* iconImage, bool bVisible);

	// Configures a brush that contributes no visual area from CommonUI itself.
	void MakeNoDrawBrush(FSlateBrush& brush);

	// Makes an optional wrapper border visually inert and layout-neutral.
	void MakeBorderLayoutNeutral(UBorder* border);

	// Applies semantic color to a border without replacing its authored brush shape.
	void ApplyBorderBrushTint(UBorder* border, const FLinearColor& color);

	// Draws a crisp rounded surface (fill + inner stroke) on surfaceBorder via the
	// analytic SDF UI material as a dynamic instance. The shader derives element
	// size from screen-space UV derivatives, so it stays sharp at any size with no
	// per-tick element size. frameBorder is the legacy stroke layer and is made
	// inert (the one material draws both fill and stroke); it may be null.
	void ApplyRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		float radiusPx,
		float borderWidthPx);

	// Draws a rounded progress bar on a border via the progress SDF material:
	// a rounded track plus a fill clipped to percent (left end follows the track
	// rounding, right end straight). The WBP-authored brush ImageSize is preserved;
	// pixel size for the shader is fed via UpdateRoundedSurfaceSize.
	void ApplyProgressSurface(
		UBorder* trackBorder,
		const FLinearColor& trackColor,
		const FLinearColor& fillColor,
		float percent,
		float radiusPx);

	// Feeds a material surface its painted pixel size (ElementSize). Slate gives no
	// reliable screen-space derivatives for brush materials, so the SDF needs the
	// size explicitly; call from NativePaint so it also runs under FWidgetRenderer
	// capture. fallbackSize (the owning widget's geometry) covers the first paint
	// before the surface has a cached geometry. (Overloads for the two host widgets.)
	void UpdateRoundedSurfaceSize(UBorder* surfaceBorder, const FVector2D& fallbackSize);
	void UpdateRoundedSurfaceSize(UImage* surfaceImage, const FVector2D& fallbackSize);
}
