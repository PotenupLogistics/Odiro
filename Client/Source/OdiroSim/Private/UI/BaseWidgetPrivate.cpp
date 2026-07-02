#include "UI/BaseWidgetPrivate.h"

#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace BaseWidgetPrivate
{
	const UBaseWidgetColorCatalog* ResolveBaseColorCatalog(
		const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors)
	{
		return UBaseWidgetColorCatalog::ResolveCatalog(baseColors);
	}

	const UBaseWidgetSizeCatalog* ResolveBaseSizeCatalog(
		const TSoftObjectPtr<UBaseWidgetSizeCatalog>& baseSizes)
	{
		return UBaseWidgetSizeCatalog::ResolveCatalog(baseSizes);
	}

	bool ResolveTextStyle(
		const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors,
		const TSoftObjectPtr<UBaseWidgetSizeCatalog>& baseSizes,
		const EBaseTextRole role,
		FBaseTextStyleToken& outStyle)
	{
		const UBaseWidgetSizeCatalog* sizes = ResolveBaseSizeCatalog(baseSizes);
		if (!sizes)
		{
			return false;
		}

		const UBaseWidgetColorCatalog* colors = ResolveBaseColorCatalog(baseColors);
		if (!colors)
		{
			return false;
		}

		const FBaseTypographyToken typography = sizes->GetTypography(role);
		outStyle = FBaseTextStyleToken(
			typography.Font,
			colors->GetTextColor(role),
			typography.LineHeightPercentage);
		return true;
	}

	FBaseWidgetSizeConstraints NormalizeSizeConstraints(FBaseWidgetSizeConstraints constraints)
	{
		constraints.MinWidth = FMath::Max(0.0f, constraints.MinWidth);
		constraints.MinHeight = FMath::Max(0.0f, constraints.MinHeight);
		constraints.MaxWidth = FMath::Max(0.0f, constraints.MaxWidth);
		constraints.MaxHeight = FMath::Max(0.0f, constraints.MaxHeight);
		if (constraints.MaxWidth > 0.0f && constraints.MinWidth > constraints.MaxWidth)
		{
			Swap(constraints.MinWidth, constraints.MaxWidth);
		}
		if (constraints.MaxHeight > 0.0f && constraints.MinHeight > constraints.MaxHeight)
		{
			Swap(constraints.MinHeight, constraints.MaxHeight);
		}
		return constraints;
	}

	void ApplySizeConstraints(USizeBox* sizeBox, const FBaseWidgetSizeConstraints& constraints)
	{
		if (!IsValid(sizeBox))
		{
			return;
		}

		const FBaseWidgetSizeConstraints normalized = NormalizeSizeConstraints(constraints);
		sizeBox->ClearWidthOverride();
		sizeBox->ClearHeightOverride();
		if (normalized.MinWidth > 0.0f)
		{
			sizeBox->SetMinDesiredWidth(normalized.MinWidth);
		}
		else
		{
			sizeBox->ClearMinDesiredWidth();
		}
		if (normalized.MinHeight > 0.0f)
		{
			sizeBox->SetMinDesiredHeight(normalized.MinHeight);
		}
		else
		{
			sizeBox->ClearMinDesiredHeight();
		}
		if (normalized.MaxWidth > 0.0f)
		{
			sizeBox->SetMaxDesiredWidth(normalized.MaxWidth);
		}
		else
		{
			sizeBox->ClearMaxDesiredWidth();
		}
		if (normalized.MaxHeight > 0.0f)
		{
			sizeBox->SetMaxDesiredHeight(normalized.MaxHeight);
		}
		else
		{
			sizeBox->ClearMaxDesiredHeight();
		}
	}

	EHorizontalAlignment ToSlateHorizontalAlignment(const EBaseHorizontalContentAlign alignment)
	{
		switch (alignment)
		{
		case EBaseHorizontalContentAlign::Left:
			return HAlign_Left;
		case EBaseHorizontalContentAlign::Right:
			return HAlign_Right;
		case EBaseHorizontalContentAlign::Center:
		default:
			return HAlign_Center;
		}
	}

	EVerticalAlignment ToSlateVerticalAlignment(const EBaseVerticalContentAlign alignment)
	{
		switch (alignment)
		{
		case EBaseVerticalContentAlign::Top:
			return VAlign_Top;
		case EBaseVerticalContentAlign::Bottom:
			return VAlign_Bottom;
		case EBaseVerticalContentAlign::Middle:
		default:
			return VAlign_Center;
		}
	}

	void ApplySlotHorizontalAlignment(UWidget* widget, const EBaseHorizontalContentAlign alignment)
	{
		if (!IsValid(widget) || !widget->Slot)
		{
			return;
		}

		const EHorizontalAlignment slateAlignment = ToSlateHorizontalAlignment(alignment);
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(widget->Slot))
		{
			horizontalSlot->SetHorizontalAlignment(slateAlignment);
		}
		else if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(widget->Slot))
		{
			verticalSlot->SetHorizontalAlignment(slateAlignment);
		}
		else if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(widget->Slot))
		{
			overlaySlot->SetHorizontalAlignment(slateAlignment);
		}
		else if (UButtonSlot* buttonSlot = Cast<UButtonSlot>(widget->Slot))
		{
			buttonSlot->SetHorizontalAlignment(slateAlignment);
		}
		else if (UScrollBoxSlot* scrollBoxSlot = Cast<UScrollBoxSlot>(widget->Slot))
		{
			scrollBoxSlot->SetHorizontalAlignment(slateAlignment);
		}
	}

	void ApplySlotVerticalAlignment(UWidget* widget, const EBaseVerticalContentAlign alignment)
	{
		if (!IsValid(widget) || !widget->Slot)
		{
			return;
		}

		const EVerticalAlignment slateAlignment = ToSlateVerticalAlignment(alignment);
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(widget->Slot))
		{
			horizontalSlot->SetVerticalAlignment(slateAlignment);
		}
		else if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(widget->Slot))
		{
			verticalSlot->SetVerticalAlignment(slateAlignment);
		}
		else if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(widget->Slot))
		{
			overlaySlot->SetVerticalAlignment(slateAlignment);
		}
		else if (UButtonSlot* buttonSlot = Cast<UButtonSlot>(widget->Slot))
		{
			buttonSlot->SetVerticalAlignment(slateAlignment);
		}
		else if (UScrollBoxSlot* scrollBoxSlot = Cast<UScrollBoxSlot>(widget->Slot))
		{
			scrollBoxSlot->SetVerticalAlignment(slateAlignment);
		}
	}

	void ApplySlotFill(UWidget* widget, const float fillWeight)
	{
		if (!IsValid(widget) || !widget->Slot)
		{
			return;
		}

		FSlateChildSize childSize;
		childSize.SizeRule = ESlateSizeRule::Fill;
		childSize.Value = FMath::Max(fillWeight, 0.0f);
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(widget->Slot))
		{
			horizontalSlot->SetSize(childSize);
			horizontalSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		else if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(widget->Slot))
		{
			verticalSlot->SetSize(childSize);
			verticalSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	FLinearColor ResolveVariantColor(
		const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors,
		const EBaseWidgetVariant variant)
	{
		if (const UBaseWidgetColorCatalog* catalog = ResolveBaseColorCatalog(baseColors))
		{
			return catalog->GetVariantColor(variant);
		}

		return FLinearColor::Transparent;
	}

	FLinearColor ResolveStateColor(
		const TSoftObjectPtr<UBaseWidgetColorCatalog>& baseColors,
		const EBaseWidgetState state)
	{
		if (const UBaseWidgetColorCatalog* catalog = ResolveBaseColorCatalog(baseColors))
		{
			return catalog->GetStateColor(state);
		}

		return FLinearColor::Transparent;
	}

	void ApplyTextStyle(UTextBlock* textBlock, const FBaseTextStyleToken& style)
	{
		if (!IsValid(textBlock))
		{
			return;
		}

		textBlock->SetFont(style.Font);
		textBlock->SetLineHeightPercentage(style.LineHeightPercentage);
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

	void ApplyIconSize(UWidget* iconBox, UImage* iconImage, const float iconSize)
	{
		const float resolvedSize = FMath::Max(iconSize, 1.0f);
		if (USizeBox* sizeBox = Cast<USizeBox>(iconBox))
		{
			sizeBox->SetWidthOverride(resolvedSize);
			sizeBox->SetHeightOverride(resolvedSize);
		}
		if (IsValid(iconImage))
		{
			FSlateBrush brush = iconImage->GetBrush();
			brush.ImageSize = FVector2D(resolvedSize, resolvedSize);
			iconImage->SetBrush(brush);
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
		// because Slate gives no reliable screen-space derivatives for these brushes.
		const TCHAR* RoundedMaterialPath =
			TEXT("/Game/Widgets/Common/M_BaseRoundedSurface_UI.M_BaseRoundedSurface_UI");
		const TCHAR* TabMaterialPath =
			TEXT("/Game/Widgets/Common/M_BaseTabSurface_UI.M_BaseTabSurface_UI");
		const TCHAR* ProgressMaterialPath =
			TEXT("/Game/Widgets/Common/M_BaseProgressSurface_UI.M_BaseProgressSurface_UI");
		const FVector2D MaterialBrushPlaceholderSize(16.0f, 16.0f);

		// The material's emissive color is written to an sRGB target, so an authored
		// sRGB token (stored via ReinterpretAsLinear) would be gamma-brightened.
		// Decode through the exact sRGB transfer so low gray tokens like #242424
		// round-trip to the same visible value. Solid Box brushes and
		// ApplyBorderBrushTint skip this because they take the direct color.
		float DecodeSrgbChannel(const float value)
		{
			const float clamped = FMath::Clamp(value, 0.0f, 1.0f);
			return clamped <= 0.04045f
				? clamped / 12.92f
				: FMath::Pow((clamped + 0.055f) / 1.055f, 2.4f);
		}

		FLinearColor EncodeTexturedColor(const FLinearColor& color)
		{
			return FLinearColor(
				DecodeSrgbChannel(color.R),
				DecodeSrgbChannel(color.G),
				DecodeSrgbChannel(color.B),
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

			auto applyMaterialBrush = [border](UMaterialInstanceDynamic* material)
				-> UMaterialInstanceDynamic*
			{
				FSlateBrush brush = border->Background;
				brush.DrawAs = ESlateBrushDrawType::Image;
				brush.ImageSize = MaterialBrushPlaceholderSize;
				brush.Margin = FMargin(0.25f);
				brush.TintColor = FSlateColor(FLinearColor::White);
				brush.SetResourceObject(material);
				border->SetBrush(brush);
				border->SetBrushColor(FLinearColor::White);
				return material;
			};

			if (UMaterialInstanceDynamic* existingMaterial =
				Cast<UMaterialInstanceDynamic>(border->Background.GetResourceObject()))
			{
				if (existingMaterial->Parent.Get() == baseMaterial)
				{
					return applyMaterialBrush(existingMaterial);
				}
			}

			UMaterialInstanceDynamic* material = UMaterialInstanceDynamic::Create(baseMaterial, border);
			if (!material)
			{
				return nullptr;
			}

			return applyMaterialBrush(material);
		}

		// Drives the rounded fill+stroke SDF material on one border.
		void ApplyRoundedMaterial(
			UBorder* border,
			const TCHAR* materialPath,
			const FLinearColor& fillColor,
			const FLinearColor& strokeColor,
			const float radiusPx,
			const float borderWidthPx)
		{
			if (UMaterialInstanceDynamic* material = EnsureMaterialBrush(border, materialPath))
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
		if (UMaterialInstanceDynamic* material = EnsureMaterialBrush(trackBorder, ProgressMaterialPath))
		{
			material->SetVectorParameterValue(TEXT("TrackColor"), EncodeTexturedColor(trackColor));
			material->SetVectorParameterValue(TEXT("FillColor"), EncodeTexturedColor(fillColor));
			material->SetScalarParameterValue(TEXT("Percent"), FMath::Clamp(percent, 0.0f, 1.0f));
			material->SetScalarParameterValue(TEXT("RadiusPx"), FMath::Max(radiusPx, 0.0f));
		}
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
		if (fillColor.A <= KINDA_SMALL_NUMBER && strokeColor.A <= KINDA_SMALL_NUMBER)
		{
			MakeBorderVisualTransparent(surfaceBorder);
			return;
		}
		ApplyRoundedMaterial(surfaceBorder, RoundedMaterialPath, fillColor, strokeColor, radiusPx, borderWidthPx);
	}

	void ApplyTopRoundedSurface(
		UBorder* frameBorder,
		UBorder* surfaceBorder,
		const FLinearColor& fillColor,
		const FLinearColor& strokeColor,
		const float radiusPx,
		const float borderWidthPx)
	{
		if (IsValid(frameBorder))
		{
			MakeBorderLayoutNeutral(frameBorder);
		}
		if (fillColor.A <= KINDA_SMALL_NUMBER && strokeColor.A <= KINDA_SMALL_NUMBER)
		{
			MakeBorderVisualTransparent(surfaceBorder);
			return;
		}
		ApplyRoundedMaterial(surfaceBorder, TabMaterialPath, fillColor, strokeColor, radiusPx, borderWidthPx);
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
		const float maxRadius = FMath::Max(0.0f, FMath::Min(size.X, size.Y) * 0.5f - 0.5f);
		float radiusPx = 0.0f;
		if (maxRadius > 0.0f && material->GetScalarParameterValue(TEXT("RadiusPx"), radiusPx))
		{
			material->SetScalarParameterValue(TEXT("RadiusPx"), FMath::Min(radiusPx, maxRadius));
		}
	}

	void MakeBorderVisualTransparent(UBorder* border)
	{
		if (!IsValid(border))
		{
			return;
		}

		FSlateBrush brush = border->Background;
		MakeNoDrawBrush(brush);
		border->SetBrush(brush);
		border->SetBrushColor(FLinearColor::Transparent);
	}
}
