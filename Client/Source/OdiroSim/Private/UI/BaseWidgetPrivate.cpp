#include "UI/BaseWidgetPrivate.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"

namespace BaseWidgetPrivate
{
	float ResolveIconPreviewSize(const EBaseWidgetSize size)
	{
		switch (size)
		{
		case EBaseWidgetSize::Small:
			return 14.0f;
		case EBaseWidgetSize::Large:
			return 22.0f;
		case EBaseWidgetSize::Medium:
		default:
			return 18.0f;
		}
	}

	FSlateBrush MakeColorBrush(const FLinearColor& color, const FVector2D size)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.ImageSize = size;
		brush.TintColor = FSlateColor(color);
		return brush;
	}

	void ApplyFixedImageBrushSize(UImage* image, const float size)
	{
		if (!IsValid(image))
		{
			return;
		}

		FSlateBrush brush = image->GetBrush();
		brush.ImageSize = FVector2D(size, size);
		image->SetBrush(brush);
	}

	bool HasAssignedImageResource(const UImage* image)
	{
		return IsValid(image) && image->GetBrush().GetResourceObject() != nullptr;
	}

	void SetOptionalIconVisibility(UWidget* iconBox, UImage* iconImage, const bool bVisible)
	{
		const ESlateVisibility visibility = bVisible
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed;
		if (IsValid(iconBox))
		{
			iconBox->SetVisibility(visibility);
		}
		if (IsValid(iconImage))
		{
			iconImage->SetVisibility(visibility);
		}
	}

	const UBaseWidgetTokenCatalog* ResolveBaseTokenCatalog(
		const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens)
	{
		if (const UBaseWidgetTokenCatalog* catalog = UBaseWidgetTokenCatalog::ResolveCatalog(baseTokens))
		{
			return catalog;
		}

		return GetDefault<UBaseWidgetTokenCatalog>();
	}

	FBaseTextStyleToken ResolveTextStyle(
		const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens,
		const EBaseTextRole role)
	{
		if (const UBaseWidgetTokenCatalog* catalog = ResolveBaseTokenCatalog(baseTokens))
		{
			return catalog->GetTextStyle(role);
		}

		return UBaseWidgetTokenCatalog::MakeDefaultTextStyle(role);
	}

	FLinearColor ResolveVariantColor(
		const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens,
		const EBaseWidgetVariant variant)
	{
		if (const UBaseWidgetTokenCatalog* catalog = ResolveBaseTokenCatalog(baseTokens))
		{
			return catalog->GetVariantColor(variant);
		}

		return FLinearColor::White;
	}

	FLinearColor ResolveStateColor(
		const TSoftObjectPtr<UBaseWidgetTokenCatalog>& baseTokens,
		const EBaseWidgetState state)
	{
		if (const UBaseWidgetTokenCatalog* catalog = ResolveBaseTokenCatalog(baseTokens))
		{
			return catalog->GetStateColor(state);
		}

		return FLinearColor::White;
	}

	void ApplyTextStyle(UTextBlock* textBlock, const FBaseTextStyleToken& style)
	{
		if (!IsValid(textBlock))
		{
			return;
		}

		textBlock->SetFont(style.Font);
		textBlock->SetColorAndOpacity(FSlateColor(style.Color));
	}

	void MakeNoDrawBrush(FSlateBrush& brush)
	{
		brush = FSlateBrush();
		brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		brush.TintColor = FSlateColor(FLinearColor::Transparent);
		brush.ImageSize = FVector2D::ZeroVector;
	}

	void MakeBorderLayoutNeutral(UBorder* border)
	{
		if (!IsValid(border))
		{
			return;
		}

		FSlateBrush brush;
		MakeNoDrawBrush(brush);
		border->SetBrush(brush);
		border->SetBrushColor(FLinearColor::Transparent);
		border->SetPadding(FMargin());
	}

	void ApplyBorderBrushTint(UBorder* border, const FLinearColor& color)
	{
		if (!IsValid(border))
		{
			return;
		}

		// Set both tints to the authored color. UBorder multiplies the brush
		// TintColor by the Border BrushColor, so matching them renders the
		// true color; SetBrushColor alone would leave a stale TintColor.
		FSlateBrush brush = border->Background;
		brush.TintColor = FSlateColor(color);
		border->SetBrush(brush);
		border->SetBrushColor(color);
	}

	namespace
	{
		// High-res rounded-rect mask (white RGB, straight alpha) authored with a
		// 16px corner in a 64px texture -> 0.25 nine-slice margin. Drawn small via a
		// Box brush so the corner downscales to a crisp ~5px radius.
		const TCHAR* RoundedFillTexturePath =
			TEXT("/Game/Widgets/Common/T_BaseRoundedFill.T_BaseRoundedFill");
		const float RoundedSliceMargin = 0.25f;
		const float RoundedImageSize = 16.0f;

		// A textured Slate brush samples the tint as linear and writes to an sRGB
		// target, brightening authored sRGB tokens (stored via ReinterpretAsLinear).
		// Pre-encode pow 2.2 so it round-trips to the intended color. (Solid Box
		// brushes, ApplyBorderBrushTint, skip this — they take the direct color.)
		FLinearColor EncodeTexturedColor(const FLinearColor& color)
		{
			return FLinearColor(
				FMath::Pow(FMath::Clamp(color.R, 0.0f, 1.0f), 2.2f),
				FMath::Pow(FMath::Clamp(color.G, 0.0f, 1.0f), 2.2f),
				FMath::Pow(FMath::Clamp(color.B, 0.0f, 1.0f), 2.2f),
				color.A);
		}

		// Tints the rounded-fill texture as a 9-slice Box brush on one border.
		// Slate multiplies texture x brush TintColor x Border BrushColor, so the
		// color goes in TintColor and BrushColor stays White (neutral) — otherwise
		// a textured brush double-darkens to color^2. The white mask's alpha-faded
		// corners then blend cleanly at the authored color.
		void SetRoundedFillBrush(UBorder* border, const FLinearColor& color)
		{
			if (!IsValid(border))
			{
				return;
			}

			FSlateBrush brush;
			brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, RoundedFillTexturePath));
			brush.DrawAs = ESlateBrushDrawType::Box;
			brush.Margin = FMargin(RoundedSliceMargin);
			brush.ImageSize = FVector2D(RoundedImageSize, RoundedImageSize);
			brush.TintColor = FSlateColor(EncodeTexturedColor(color));
			border->SetBrush(brush);
			border->SetBrushColor(FLinearColor::White);
		}
	}

	void ApplyRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		const float borderWidthPx)
	{
		// Outer frame paints the stroke color; its padding insets the inner surface
		// by the border width, exposing a clean rounded ring of stroke around the fill.
		if (IsValid(frameBorder))
		{
			SetRoundedFillBrush(frameBorder, strokeColor);
			frameBorder->SetPadding(FMargin(FMath::Max(borderWidthPx, 0.0f)));
		}
		// Inner surface paints the fill color on top, covering all but the ring.
		SetRoundedFillBrush(surfaceBorder, fillColor);
	}
}
