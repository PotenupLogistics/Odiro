#include "UI/BaseButtonWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
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

	// Blends a color toward another token color for derived interaction states.
	FLinearColor MixColor(const FLinearColor& from, const FLinearColor& to, const float amount)
	{
		return FMath::Lerp(from, to, FMath::Clamp(amount, 0.0f, 1.0f));
	}

	// Resolves a button fill without falling back to the neutral secondary palette.
	FLinearColor ResolveButtonSurfaceColor(
		const UBaseWidgetTokenCatalog& tokens,
		const EBaseWidgetVariant variant,
		const EBaseWidgetState state,
		const bool bSelected)
	{
		if (state == EBaseWidgetState::Disabled)
		{
			return tokens.SurfaceControlColor;
		}

		if (bSelected || state == EBaseWidgetState::Selected)
		{
			if (state == EBaseWidgetState::Hovered)
			{
				return tokens.AccentHoverColor;
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return tokens.AccentActiveColor;
			}
			return tokens.AccentColor;
		}

		if (variant == EBaseWidgetVariant::Primary)
		{
			if (state == EBaseWidgetState::Hovered)
			{
				return tokens.AccentHoverColor;
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return tokens.AccentActiveColor;
			}
			return tokens.AccentColor;
		}

		if (IsColoredActionVariant(variant))
		{
			const FLinearColor baseColor = tokens.GetVariantColor(variant);
			if (state == EBaseWidgetState::Hovered)
			{
				return MixColor(baseColor, tokens.TextStrongColor, 0.12f);
			}
			if (state == EBaseWidgetState::Pressed)
			{
				return MixColor(baseColor, tokens.SurfaceWellColor, 0.18f);
			}
			return baseColor;
		}

		if (state == EBaseWidgetState::Hovered)
		{
			return tokens.SurfaceControlHoverColor;
		}
		if (state == EBaseWidgetState::Pressed)
		{
			return tokens.SurfaceControlActiveColor;
		}
		if (state != EBaseWidgetState::Default)
		{
			return tokens.GetStateColor(state);
		}
		return tokens.GetVariantColor(variant);
	}

	// Resolves the button outline in the same palette as the fill.
	FLinearColor ResolveButtonFrameColor(
		const UBaseWidgetTokenCatalog& tokens,
		const EBaseWidgetVariant variant,
		const EBaseWidgetState state,
		const bool bSelected)
	{
		if (state == EBaseWidgetState::Disabled)
		{
			return tokens.LineSubtleColor;
		}

		if (bSelected || state == EBaseWidgetState::Selected)
		{
			return state == EBaseWidgetState::Pressed ? tokens.AccentActiveColor : tokens.AccentColor;
		}

		if (variant == EBaseWidgetVariant::Primary)
		{
			return state == EBaseWidgetState::Pressed ? tokens.AccentActiveColor : tokens.AccentColor;
		}

		if (IsColoredActionVariant(variant))
		{
			const FLinearColor baseColor = tokens.GetVariantColor(variant);
			return state == EBaseWidgetState::Pressed
				? MixColor(baseColor, tokens.SurfaceWellColor, 0.24f)
				: baseColor;
		}

		if (state == EBaseWidgetState::Hovered || state == EBaseWidgetState::Pressed)
		{
			return tokens.LineFieldHoverColor;
		}
		return tokens.LineInsetColor;
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

const UBaseWidgetTokenCatalog* UBaseButtonWidget::GetResolvedBaseTokens() const
{
	return BaseWidgetPrivate::ResolveBaseTokenCatalog(BaseTokens);
}

FBaseTextStyleToken UBaseButtonWidget::ResolveTextStyle(const EBaseTextRole role) const
{
	return BaseWidgetPrivate::ResolveTextStyle(BaseTokens, role);
}

FLinearColor UBaseButtonWidget::ResolveVariantColor(const EBaseWidgetVariant variant) const
{
	return BaseWidgetPrivate::ResolveVariantColor(BaseTokens, variant);
}

FLinearColor UBaseButtonWidget::ResolveStateColor(const EBaseWidgetState state) const
{
	return BaseWidgetPrivate::ResolveStateColor(BaseTokens, state);
}

void UBaseButtonWidget::SynchronizeBaseProperties()
{
	const bool bEnabled = !bDisabled && State != EBaseWidgetState::Disabled;
	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const EBaseWidgetVariant effectiveVariant = GetEffectiveVariant();
	const EBaseWidgetState effectiveState = GetEffectiveState();
	const bool bHighContrastForeground = effectiveVariant == EBaseWidgetVariant::Primary
		|| effectiveVariant == EBaseWidgetVariant::Danger
		|| effectiveVariant == EBaseWidgetVariant::Success
		|| effectiveVariant == EBaseWidgetVariant::Info
		|| bSelected
		|| effectiveState == EBaseWidgetState::Selected;
	FLinearColor foregroundColor = tokens ? tokens->TextPrimaryColor : FLinearColor::White;
	if (!bEnabled)
	{
		foregroundColor = ResolveStateColor(EBaseWidgetState::Disabled);
	}
	else if (tokens
		&& effectiveVariant == EBaseWidgetVariant::Warning
		&& !bSelected
		&& effectiveState != EBaseWidgetState::Selected)
	{
		foregroundColor = tokens->SurfaceWellColor;
	}
	else if (tokens && bHighContrastForeground)
	{
		foregroundColor = tokens->TextStrongColor;
	}
	SetIsEnabled(bEnabled);
	if (GetSelected() != bSelected)
	{
		SetIsSelected(bSelected, false);
	}

	const bool bHasVisibleLabel = LabelTextBlock && !Label.IsEmpty();
	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		LabelTextBlock->SetVisibility(bHasVisibleLabel
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(foregroundColor));
	}
	BaseWidgetPrivate::ApplyIconSize(IconBox.Get(), IconImage.Get(), IconSize);
	bool bHasIconImage = false;
	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon, false);
		}
		bHasIconImage = Icon != nullptr;
		IconImage->SetColorAndOpacity(foregroundColor);
		IconImage->SetVisibility(bHasIconImage
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	const bool bHasFallbackGlyph = IconGlyph && !IconGlyphText.IsEmpty();
	if (IconGlyph)
	{
		if (bHasFallbackGlyph)
		{
			IconGlyph->SetText(IconGlyphText);
		}
		IconGlyph->SetVisibility(!bHasIconImage && bHasFallbackGlyph
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
		IconGlyph->SetColorAndOpacity(foregroundColor);
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

	if (tokens)
	{
		const FLinearColor surfaceColor =
			ResolveButtonSurfaceColor(*tokens, effectiveVariant, effectiveState, bSelected);
		const FLinearColor frameColor =
			ResolveButtonFrameColor(*tokens, effectiveVariant, effectiveState, bSelected);
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			surfaceColor,
			frameColor,
			tokens->Radius,
			tokens->BorderWidth);
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
	IconSize = FMath::Max(inIconSize, 1.0f);
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

void UBaseButtonWidget::SetBaseSize(const EBaseWidgetSize inSize)
{
	Size = inSize;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
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
	OnBaseHovered.Broadcast(this);
}

void UBaseButtonWidget::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	InteractionState = EBaseWidgetState::Default;
	SynchronizeBaseProperties();
	OnBaseUnhovered.Broadcast(this);
}

void UBaseButtonWidget::NativeOnPressed()
{
	Super::NativeOnPressed();
	InteractionState = EBaseWidgetState::Pressed;
	SynchronizeBaseProperties();
	OnBasePressed.Broadcast(this);
}

void UBaseButtonWidget::NativeOnReleased()
{
	Super::NativeOnReleased();
	InteractionState = EBaseWidgetState::Hovered;
	SynchronizeBaseProperties();
	OnBaseReleased.Broadcast(this);
}

void UBaseButtonWidget::NativeOnSelected(const bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	bSelected = true;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::NativeOnDeselected(const bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	bSelected = false;
	SynchronizeBaseProperties();
}

void UBaseButtonWidget::ApplyTextStyle(UTextBlock* textBlock, const EBaseTextRole role) const
{
	BaseWidgetPrivate::ApplyTextStyle(textBlock, ResolveTextStyle(role));
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

void UBaseButtonWidget::UseTransparentCommonStyle()
{
	Style = UBaseTransparentButtonStyle::StaticClass();
#if WITH_EDITORONLY_DATA
	bStyleNoLongerNeedsConversion = true;
#endif
}
