#include "UI/BaseButtonWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

namespace
{
	// Returns whether a variant owns its own semantic action color.
	bool IsColoredActionVariant(const EBaseWidgetVariant variant)
	{
		return variant == EBaseWidgetVariant::Primary
			|| variant == EBaseWidgetVariant::Success
			|| variant == EBaseWidgetVariant::Warning
			|| variant == EBaseWidgetVariant::Danger
			|| variant == EBaseWidgetVariant::Info;
	}

	// Blends a semantic color toward another token color for derived interaction states.
	FLinearColor MixTokenColor(const FLinearColor& from, const FLinearColor& to, const float amount)
	{
		return FMath::Lerp(from, to, FMath::Clamp(amount, 0.0f, 1.0f));
	}

	// Neutral/secondary controls darken when pressed and brighten only on hover.
	FLinearColor ResolveNeutralPressedSurfaceColor(
		const UBaseWidgetColorCatalog& colors,
		const EBaseWidgetVariant variant)
	{
		return variant == EBaseWidgetVariant::Secondary
			? colors.SurfaceHoverColor
			: colors.SurfacePanelColor;
	}

	// Resolves the DA-driven button fill for the current variant and interaction state.
	FLinearColor ResolveButtonSurfaceColor(
		const UBaseWidgetColorCatalog& colors,
		const EBaseWidgetVariant variant,
		const EBaseWidgetState state,
		const bool bSelected)
	{
		if (state == EBaseWidgetState::Disabled)
		{
			return colors.SurfaceControlColor;
		}

		if (bSelected || state == EBaseWidgetState::Selected)
		{
			if (state == EBaseWidgetState::Hovered)
			{
				return colors.AccentHoverColor;
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return colors.AccentActiveColor;
			}
			return colors.AccentColor;
		}

		if (variant == EBaseWidgetVariant::Primary)
		{
			if (state == EBaseWidgetState::Hovered)
			{
				return colors.AccentHoverColor;
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return colors.AccentActiveColor;
			}
			return colors.AccentColor;
		}

		if (variant == EBaseWidgetVariant::Ghost)
		{
			if (state == EBaseWidgetState::Hovered)
			{
				return colors.SurfaceControlHoverColor;
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return colors.SurfaceHoverColor;
			}
			return FLinearColor::Transparent;
		}

		if (IsColoredActionVariant(variant))
		{
			const FLinearColor baseColor = colors.GetVariantColor(variant);
			if (state == EBaseWidgetState::Hovered)
			{
				return MixTokenColor(baseColor, colors.TextStrongColor, 0.12f);
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return MixTokenColor(baseColor, colors.SurfaceWellColor, 0.18f);
			}
			return baseColor;
		}

		if (state == EBaseWidgetState::Hovered)
		{
			return variant == EBaseWidgetVariant::Neutral
				? colors.SurfaceControlActiveColor
				: colors.SurfaceControlHoverColor;
		}
		if (state == EBaseWidgetState::Pressed)
		{
			return ResolveNeutralPressedSurfaceColor(colors, variant);
		}
		if (state != EBaseWidgetState::Default)
		{
			return colors.GetStateColor(state);
		}
		if (variant == EBaseWidgetVariant::Neutral)
		{
			return colors.SurfaceControlHoverColor;
		}
		return colors.GetVariantColor(variant);
	}

	// Resolves the DA-driven button outline for the current variant and interaction state.
	FLinearColor ResolveButtonFrameColor(
		const UBaseWidgetColorCatalog& colors,
		const EBaseWidgetVariant variant,
		const EBaseWidgetState state,
		const bool bSelected)
	{
		if (state == EBaseWidgetState::Disabled)
		{
			return colors.LineSubtleColor;
		}

		if (bSelected || state == EBaseWidgetState::Selected)
		{
			return state == EBaseWidgetState::Pressed ? colors.AccentActiveColor : colors.AccentColor;
		}

		if (variant == EBaseWidgetVariant::Ghost)
		{
			return FLinearColor::Transparent;
		}

		if (variant == EBaseWidgetVariant::Primary)
		{
			return state == EBaseWidgetState::Pressed ? colors.AccentActiveColor : colors.AccentColor;
		}

		if (IsColoredActionVariant(variant))
		{
			const FLinearColor baseColor = colors.GetVariantColor(variant);
			return state == EBaseWidgetState::Pressed
				? MixTokenColor(baseColor, colors.SurfaceWellColor, 0.24f)
				: baseColor;
		}

		if (state == EBaseWidgetState::Hovered || state == EBaseWidgetState::Pressed)
		{
			return colors.LineFieldHoverColor;
		}
		return colors.LineInsetColor;
	}

	// Maps base button alignment to text justification for label-only buttons.
	ETextJustify::Type ToTextJustify(const EBaseHorizontalContentAlign alignment)
	{
		switch (alignment)
		{
		case EBaseHorizontalContentAlign::Left:
			return ETextJustify::Left;
		case EBaseHorizontalContentAlign::Right:
			return ETextJustify::Right;
		case EBaseHorizontalContentAlign::Center:
		default:
			return ETextJustify::Center;
		}
	}
}

UBaseTransparentButtonStyle::UBaseTransparentButtonStyle()
{
	bSingleMaterial = false;
	ButtonPadding = FMargin();
	CustomPadding = FMargin();
	MinWidth = 0;
	MinHeight = 0;
	MaxWidth = 0;
	MaxHeight = 0;
	BaseWidgetPrivate::MakeNoDrawBrush(NormalBase);
	BaseWidgetPrivate::MakeNoDrawBrush(NormalHovered);
	BaseWidgetPrivate::MakeNoDrawBrush(NormalPressed);
	BaseWidgetPrivate::MakeNoDrawBrush(SelectedBase);
	BaseWidgetPrivate::MakeNoDrawBrush(SelectedHovered);
	BaseWidgetPrivate::MakeNoDrawBrush(SelectedPressed);
	BaseWidgetPrivate::MakeNoDrawBrush(Disabled);
}

UBaseButtonWidget::UBaseButtonWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	bApplyAlphaOnDisable = false;
	UseTransparentCommonStyle();
}

const UBaseWidgetColorCatalog* UBaseButtonWidget::GetResolvedBaseColors() const
{
	return BaseWidgetPrivate::ResolveBaseColorCatalog(ColorsOverride);
}

const UBaseWidgetSizeCatalog* UBaseButtonWidget::GetResolvedBaseSizes() const
{
	return BaseWidgetPrivate::ResolveBaseSizeCatalog(SizesOverride);
}

FBaseTextStyleToken UBaseButtonWidget::ResolveTextStyle(const EBaseTextRole role) const
{
	FBaseTextStyleToken style;
	BaseWidgetPrivate::ResolveTextStyle(ColorsOverride, SizesOverride, role, style);
	return style;
}

FLinearColor UBaseButtonWidget::ResolveVariantColor(const EBaseWidgetVariant variant) const
{
	return BaseWidgetPrivate::ResolveVariantColor(ColorsOverride, variant);
}

FLinearColor UBaseButtonWidget::ResolveStateColor(const EBaseWidgetState state) const
{
	return BaseWidgetPrivate::ResolveStateColor(ColorsOverride, state);
}

void UBaseButtonWidget::SynchronizeBaseProperties()
{
	BaseWidgetPrivate::ApplySizeConstraints(RootSize.Get(), SizeConstraints);
	if (RootSizeBox.Get() != RootSize.Get())
	{
		BaseWidgetPrivate::ApplySizeConstraints(RootSizeBox.Get(), SizeConstraints);
	}

	const bool bEnabled = !bDisabled && State != EBaseWidgetState::Disabled;
	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	const EBaseWidgetVariant effectiveVariant = GetEffectiveVariant();
	const EBaseWidgetState effectiveState = GetEffectiveState();
	const bool bHighContrastForeground = effectiveVariant == EBaseWidgetVariant::Primary
		|| effectiveVariant == EBaseWidgetVariant::Danger
		|| effectiveVariant == EBaseWidgetVariant::Success
		|| effectiveVariant == EBaseWidgetVariant::Info
		|| bSelected
		|| effectiveState == EBaseWidgetState::Selected;
	FLinearColor foregroundColor = FLinearColor::Transparent;
	if (colors)
	{
		foregroundColor = colors->TextPrimaryColor;
		if (!bEnabled)
		{
			foregroundColor = colors->GetStateColor(EBaseWidgetState::Disabled);
		}
		else if (effectiveVariant == EBaseWidgetVariant::Warning
			&& !bSelected
			&& effectiveState != EBaseWidgetState::Selected)
		{
			foregroundColor = colors->SurfaceWellColor;
		}
		else if (bHighContrastForeground)
		{
			foregroundColor = colors->TextStrongColor;
		}
	}
	SetIsEnabled(bEnabled);
	if (GetSelected() != bSelected)
	{
		SetIsSelected(bSelected, false);
	}
	if (SurfaceBorder)
	{
		SurfaceBorder->SetHorizontalAlignment(BaseWidgetPrivate::ToSlateHorizontalAlignment(ContentAlign));
	}

	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		LabelTextBlock->SetJustification(ToTextJustify(ContentAlign));
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		if (colors)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(foregroundColor));
		}
	}
	const bool bHasVisibleLabel = LabelTextBlock && !Label.IsEmpty();
	if (LabelTextBlock)
	{
		LabelTextBlock->SetVisibility(bHasVisibleLabel
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	const float resolvedIconSize = IconSize > 0.0f ? IconSize : (sizes ? sizes->IconSize : 0.0f);
	if (resolvedIconSize > 0.0f)
	{
		BaseWidgetPrivate::ApplyIconSize(IconBox.Get(), IconImage.Get(), resolvedIconSize);
	}
	bool bHasIconImage = false;
	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon, false);
		}
		bHasIconImage = Icon != nullptr;
		if (colors)
		{
			IconImage->SetColorAndOpacity(foregroundColor);
		}
		IconImage->SetVisibility(bHasIconImage
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (IconGlyph)
	{
		IconGlyph->SetText(IconGlyphText);
	}
	const bool bHasFallbackGlyph = IconGlyph && !IconGlyphText.IsEmpty();
	if (IconGlyph)
	{
		IconGlyph->SetVisibility(!bHasIconImage && bHasFallbackGlyph
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
		if (colors)
		{
			IconGlyph->SetColorAndOpacity(FSlateColor(foregroundColor));
		}
	}
	if (IconBox)
	{
		IconBox->SetVisibility((bHasIconImage || bHasFallbackGlyph)
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	const bool bHasVisibleIcon = bHasIconImage || bHasFallbackGlyph;
	if (IconLabelSpacer)
	{
		IconLabelSpacer->SetVisibility(bHasVisibleIcon && bHasVisibleLabel
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (colors && sizes)
	{
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			ResolveButtonSurfaceColor(*colors, effectiveVariant, effectiveState, bSelected),
			ResolveButtonFrameColor(*colors, effectiveVariant, effectiveState, bSelected),
			sizes->Radius,
			sizes->BorderWidth);
	}

	InvalidateLayoutAndVolatility();
}

int32 UBaseButtonWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseButtonWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetIcon(UTexture2D* inIcon)
{
	Icon = inIcon;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetIconGlyphText(const FText inIconGlyphText)
{
	IconGlyphText = inIconGlyphText;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetIconSize(const float inIconSize)
{
	IconSize = FMath::Max(inIconSize, 0.0f);
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetVariant(const EBaseWidgetVariant inVariant)
{
	Variant = inVariant;
	if (inVariant != EBaseWidgetVariant::Primary)
	{
		bPrimary = false;
	}
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetPrimary(const bool bInPrimary)
{
	bPrimary = bInPrimary;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetColorsOverride(const TSoftObjectPtr<UBaseWidgetColorCatalog> inColorsOverride)
{
	ColorsOverride = inColorsOverride;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetSizesOverride(const TSoftObjectPtr<UBaseWidgetSizeCatalog> inSizesOverride)
{
	SizesOverride = inSizesOverride;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetSizeConstraints(const FBaseWidgetSizeConstraints inSizeConstraints)
{
	SizeConstraints = BaseWidgetPrivate::NormalizeSizeConstraints(inSizeConstraints);
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
}

void UBaseButtonWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
}

void UBaseButtonWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
}

void UBaseButtonWidget::SetContentAlign(const EBaseHorizontalContentAlign inContentAlign)
{
	ContentAlign = inContentAlign;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SynchronizeProperties()
{
	UseTransparentCommonStyle();
	Super::SynchronizeProperties();
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();
	UseTransparentCommonStyle();
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(true);
}

void UBaseButtonWidget::NativePreConstruct()
{
	UseTransparentCommonStyle();
	Super::NativePreConstruct();
	SynchronizeBaseProperties();
}

#if WITH_EDITOR
void UBaseButtonWidget::PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent)
{
	Super::PostEditChangeProperty(propertyChangedEvent);
	SynchronizeBaseProperties();
}
#endif

void UBaseButtonWidget::PostLoad()
{
	Super::PostLoad();
	UseTransparentCommonStyle();
}

void UBaseButtonWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnBaseClicked.Broadcast(this);
}

void UBaseButtonWidget::NativeOnHovered()
{
	Super::NativeOnHovered();
	InteractionState = EBaseWidgetState::Hovered;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
	OnBaseHovered.Broadcast(this);
}

void UBaseButtonWidget::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	InteractionState = EBaseWidgetState::Default;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
	OnBaseUnhovered.Broadcast(this);
}

void UBaseButtonWidget::NativeOnPressed()
{
	Super::NativeOnPressed();
	InteractionState = EBaseWidgetState::Pressed;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
	OnBasePressed.Broadcast(this);
}

void UBaseButtonWidget::NativeOnReleased()
{
	Super::NativeOnReleased();
	InteractionState = EBaseWidgetState::Hovered;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
	OnBaseReleased.Broadcast(this);
}

void UBaseButtonWidget::NativeOnSelected(const bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	bSelected = true;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
}

void UBaseButtonWidget::NativeOnDeselected(const bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	bSelected = false;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged();
}

void UBaseButtonWidget::ApplyTextStyle(UTextBlock* textBlock, const EBaseTextRole role) const
{
	FBaseTextStyleToken style;
	if (BaseWidgetPrivate::ResolveTextStyle(ColorsOverride, SizesOverride, role, style))
	{
		BaseWidgetPrivate::ApplyTextStyle(textBlock, style);
	}
}

void UBaseButtonWidget::ApplyBorderColor(UBorder* border, const FLinearColor& color) const
{
	BaseWidgetPrivate::ApplyBorderBrushTint(border, color);
}

EBaseWidgetVariant UBaseButtonWidget::GetEffectiveVariant() const
{
	return bPrimary ? EBaseWidgetVariant::Primary : Variant;
}

EBaseWidgetState UBaseButtonWidget::GetEffectiveState() const
{
	if (bDisabled || State == EBaseWidgetState::Disabled)
	{
		return EBaseWidgetState::Disabled;
	}
	if (InteractionState != EBaseWidgetState::Default)
	{
		return InteractionState;
	}
	if (bSelected)
	{
		return EBaseWidgetState::Selected;
	}
	return State;
}

void UBaseButtonWidget::NotifyBaseVisualStateChanged(const bool bForce)
{
	const EBaseWidgetState effectiveState = GetEffectiveState();
	if (!bForce && bHasBroadcastVisualState && LastBroadcastVisualState == effectiveState)
	{
		return;
	}

	LastBroadcastVisualState = effectiveState;
	bHasBroadcastVisualState = true;
	ReceiveBaseVisualStateChanged(effectiveState);
}

void UBaseButtonWidget::UseTransparentCommonStyle()
{
	Style = UBaseTransparentButtonStyle::StaticClass();
#if WITH_EDITORONLY_DATA
	bStyleNoLongerNeedsConversion = true;
#endif
}
