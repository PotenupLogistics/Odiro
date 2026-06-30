#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/BaseWidgetTypes.h"

class UBorder;
class UImage;
class USizeBox;
class UTextBlock;
class UWidget;

namespace BaseWidgetPrivate
{
	// Resolves a nullable color catalog reference through the project default color DA.
	const UBaseWidgetColorCatalog* ResolveBaseColorCatalog(const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors);

	// Resolves a nullable size catalog reference through the project default medium-size DA.
	const UBaseWidgetSizeCatalog* ResolveBaseSizeCatalog(const TSoftObjectPtr<UBaseWidgetSizeCatalog>& baseSizes);

	// Resolves a text token through the split color and size catalog path.
	bool ResolveTextStyle(
		const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors,
		const TSoftObjectPtr<UBaseWidgetSizeCatalog>& baseSizes,
		EBaseTextRole role,
		FBaseTextStyleToken& outStyle);

	// Clamps negative desired-size constraints and keeps min/max pairs ordered.
	FBaseWidgetSizeConstraints NormalizeSizeConstraints(FBaseWidgetSizeConstraints constraints);

	// Applies responsive root constraints while clearing fixed root size overrides.
	void ApplySizeConstraints(USizeBox* sizeBox, const FBaseWidgetSizeConstraints& constraints);

	// Maps base component horizontal alignment options to Slate alignment values.
	EHorizontalAlignment ToSlateHorizontalAlignment(EBaseHorizontalContentAlign alignment);

	// Maps base component vertical alignment options to Slate alignment values.
	EVerticalAlignment ToSlateVerticalAlignment(EBaseVerticalContentAlign alignment);

	// Applies horizontal alignment to common UMG panel slots when present.
	void ApplySlotHorizontalAlignment(UWidget* widget, EBaseHorizontalContentAlign alignment);

	// Applies vertical alignment to common UMG panel slots when present.
	void ApplySlotVerticalAlignment(UWidget* widget, EBaseVerticalContentAlign alignment);

	// Applies equal fill sizing to common box panel slots when present.
	void ApplySlotFill(UWidget* widget, float fillWeight = 1.0f);

	// Returns a variant color through the shared base color resolution path.
	FLinearColor ResolveVariantColor(const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors, EBaseWidgetVariant variant);

	// Returns a state color through the shared base color resolution path.
	FLinearColor ResolveStateColor(const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors, EBaseWidgetState state);

	// Applies one resolved semantic text token to a text block.
	void ApplyTextStyle(UTextBlock* textBlock, const FBaseTextStyleToken& style);

	// Applies explicit component text while preserving WBP-authored text when the property is unset.
	void ApplyTextIfSet(UTextBlock* textBlock, const FText& text);

	// Returns whether an image has a WBP-authored or runtime-assigned brush resource.
	bool HasAssignedImageResource(const UImage* image);

	// Owns optional icon visibility from image assignment instead of letting WBP visibility drift.
	void SetOptionalIconVisibility(UWidget* iconBox, UImage* iconImage, bool bVisible);

	// Applies one square pixel size to an optional icon wrapper and its image brush.
	void ApplyIconSize(UWidget* iconBox, UImage* iconImage, float iconSize);

	// Configures a brush that contributes no visual area from CommonUI itself.
	void MakeNoDrawBrush(FSlateBrush& brush);

	// Makes an optional wrapper border visually inert and layout-neutral.
	void MakeBorderLayoutNeutral(UBorder* border);

	// Applies semantic color to a border without replacing its authored brush shape.
	void ApplyBorderBrushTint(UBorder* border, const FLinearColor& color);

	// Draws a crisp rounded surface (fill + inner stroke) on surfaceBorder via an
	// analytic SDF UI material. NativePaint feeds the material's ElementSize
	// parameter so it stays sharp at the WBP-authored size.
	void ApplyRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		float radiusPx,
		float borderWidthPx);

	// Draws a tab surface with only the top corners rounded and no bottom stroke.
	void ApplyTopRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		float radiusPx,
		float borderWidthPx);

	// Draws a rounded progress bar on a border via the progress SDF material:
	// a rounded track plus a fill clipped to percent. NativePaint feeds the
	// material's ElementSize parameter for pixel-accurate corners.
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
	// before the surface has a cached geometry.
	void UpdateRoundedSurfaceSize(UBorder* surfaceBorder, const FVector2D& fallbackSize);

	// Clears border drawing while keeping WBP-authored padding and child layout.
	void MakeBorderVisualTransparent(UBorder* border);
}
