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
	// Maps semantic component size to a stable square icon size.
	float ResolveIconPreviewSize(EBaseWidgetSize size);

	// Creates a resource-less brush for icon placeholders and state previews.
	FSlateBrush MakeColorBrush(const FLinearColor& color, FVector2D size);

	// Pins image desired size so imported UI textures cannot expand compact controls.
	void ApplyFixedImageBrushSize(UImage* image, float size);

	// Returns whether a WBP-authored image brush has a resource even when the C++ Icon property is empty.
	bool HasAssignedImageResource(const UImage* image);

	// Applies the same visibility to an optional icon image wrapper and the image it owns.
	void SetOptionalIconVisibility(UWidget* iconBox, UImage* iconImage, bool bVisible);

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

	// Configures a brush that contributes no visual area from CommonUI itself.
	void MakeNoDrawBrush(FSlateBrush& brush);

	// Makes an optional wrapper border visually inert and layout-neutral.
	void MakeBorderLayoutNeutral(UBorder* border);

	// Applies semantic color to a border without replacing its authored brush shape.
	void ApplyBorderBrushTint(UBorder* border, const FLinearColor& color);

	// Draws a crisp rounded surface from two stacked 9-slice fill textures:
	// the outer frame border carries the stroke color, and its padding insets the
	// inner surface border (fill color) by borderWidthPx so a clean rounded ring of
	// stroke shows. Corners come from the texture's high-res baked alpha — no native
	// RoundedBox softness, no material, no per-tick size. frame may be null (fill only).
	void ApplyRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		float borderWidthPx);
}
