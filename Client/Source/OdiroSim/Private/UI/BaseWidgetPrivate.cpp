#include "UI/BaseWidgetPrivate.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace BaseWidgetPrivate
{
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

	void ApplyTextIfSet(UTextBlock* textBlock, const FText& text)
	{
		if (IsValid(textBlock) && !text.IsEmpty())
		{
			textBlock->SetText(text);
		}
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
		// Analytic SDF UI materials. Each computes a crisp ~0.6px antialiased rounded
		// surface in pixel space, taking the painted size as an ElementSize parameter
		// (Slate gives no reliable screen-space derivatives). Authored by
		// Client/Tools/build_rounded_material.py and build_progress_material.py.
		const TCHAR* RoundedMaterialPath =
			TEXT("/Game/Widgets/Common/M_BaseRoundedSurface_UI.M_BaseRoundedSurface_UI");
		const TCHAR* ProgressMaterialPath =
			TEXT("/Game/Widgets/Common/M_BaseProgressSurface_UI.M_BaseProgressSurface_UI");

		// The material's emissive color is written to an sRGB target, so an authored
		// sRGB token (stored via ReinterpretAsLinear) would be gamma-brightened.
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

		// Ensures a border draws the given material and returns its cached dynamic
		// instance (reused across calls so only its parameters change per frame).
		UMaterialInstanceDynamic* EnsureMaterialBrush(UBorder* border, const TCHAR* materialPath)
		{
			if (!IsValid(border))
			{
				return nullptr;
			}

			UMaterialInterface* baseMaterial = LoadObject<UMaterialInterface>(nullptr, materialPath);
			if (!baseMaterial)
			{
				return nullptr;
			}

			UMaterialInstanceDynamic* material = UMaterialInstanceDynamic::Create(baseMaterial, border);
			if (!material)
			{
				return nullptr;
			}

			FSlateBrush brush = border->Background;
			brush.DrawAs = ESlateBrushDrawType::Image;
			brush.ImageSize = FVector2D(16.0f, 16.0f);
			brush.TintColor = FSlateColor(FLinearColor::White);
			brush.SetResourceObject(material);
			border->SetBrush(brush);
			border->SetBrushColor(FLinearColor::White);
			return material;
		}

		// Drives the rounded fill+stroke SDF material on one border.
		void ApplyRoundedMaterial(
			UBorder* border,
			const FLinearColor& fillColor,
			const FLinearColor& strokeColor,
			const float radiusPx,
			const float borderWidthPx)
		{
			if (UMaterialInstanceDynamic* material = EnsureMaterialBrush(border, RoundedMaterialPath))
			{
				material->SetVectorParameterValue(TEXT("FillColor"), EncodeTexturedColor(fillColor));
				material->SetVectorParameterValue(TEXT("StrokeColor"), EncodeTexturedColor(strokeColor));
				material->SetScalarParameterValue(TEXT("RadiusPx"), FMath::Max(radiusPx, 0.0f));
				material->SetScalarParameterValue(TEXT("BorderWidthPx"), FMath::Max(borderWidthPx, 0.0f));
			}
		}
	}

	void ApplyProgressSurface(
		UBorder* trackBorder,
		const FLinearColor& trackColor,
		const FLinearColor& fillColor,
		const float percent,
		const float radiusPx)
	{
		if (!IsValid(trackBorder))
		{
			return;
		}

		UMaterialInterface* baseMaterial = LoadObject<UMaterialInterface>(nullptr, ProgressMaterialPath);
		if (!baseMaterial)
		{
			return;
		}

		UMaterialInstanceDynamic* material = UMaterialInstanceDynamic::Create(baseMaterial, trackBorder);
		if (!material)
		{
			return;
		}

		FSlateBrush brush = trackBorder->Background;
		brush.DrawAs = ESlateBrushDrawType::Image;
		brush.ImageSize = FVector2D(16.0f, 16.0f);
		brush.TintColor = FSlateColor(FLinearColor::White);
		brush.SetResourceObject(material);
		trackBorder->SetBrush(brush);
		trackBorder->SetBrushColor(FLinearColor::White);

		material->SetVectorParameterValue(TEXT("TrackColor"), EncodeTexturedColor(trackColor));
		material->SetVectorParameterValue(TEXT("FillColor"), EncodeTexturedColor(fillColor));
		material->SetScalarParameterValue(TEXT("Percent"), FMath::Clamp(percent, 0.0f, 1.0f));
		material->SetScalarParameterValue(TEXT("RadiusPx"), FMath::Max(radiusPx, 0.0f));
	}

	void ApplyRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		const float radiusPx,
		const float borderWidthPx)
	{
		// One material draws both the fill and the inner stroke ring; the legacy
		// frame border is no longer needed for a separate stroke layer.
		if (IsValid(frameBorder))
		{
			MakeBorderLayoutNeutral(frameBorder);
		}
		ApplyRoundedMaterial(surfaceBorder, fillColor, strokeColor, radiusPx, borderWidthPx);
	}

	void UpdateRoundedSurfaceSize(UBorder* surfaceBorder, const FVector2D& fallbackSize)
	{
		if (!IsValid(surfaceBorder))
		{
			return;
		}

		UMaterialInstanceDynamic* material =
			Cast<UMaterialInstanceDynamic>(surfaceBorder->Background.GetResourceObject());
		if (!material)
		{
			return;
		}

		FVector2D size = surfaceBorder->GetCachedGeometry().GetLocalSize();
		if (size.X < 1.0f || size.Y < 1.0f)
		{
			size = fallbackSize;
		}
		material->SetVectorParameterValue(TEXT("ElementSize"), FLinearColor(size.X, size.Y, 0.0f, 0.0f));
	}

	void UpdateRoundedSurfaceSize(UImage* surfaceImage, const FVector2D& fallbackSize)
	{
		if (!IsValid(surfaceImage))
		{
			return;
		}

		UMaterialInstanceDynamic* material =
			Cast<UMaterialInstanceDynamic>(surfaceImage->GetBrush().GetResourceObject());
		if (!material)
		{
			return;
		}

		FVector2D size = surfaceImage->GetCachedGeometry().GetLocalSize();
		if (size.X < 1.0f || size.Y < 1.0f)
		{
			size = fallbackSize;
		}
		material->SetVectorParameterValue(TEXT("ElementSize"), FLinearColor(size.X, size.Y, 0.0f, 0.0f));
	}
}
