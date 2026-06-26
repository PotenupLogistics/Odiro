#include "UI/BaseButtonWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "UI/BaseIconButtonWidget.h"
#include "UI/BaseTabWidget.h"
#include "UI/BaseWidgetPrivate.h"

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
	, Label(FText::FromString(TEXT("Run action")))
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
		|| effectiveVariant == EBaseWidgetVariant::Success;
	FLinearColor foregroundColor = tokens ? tokens->TextPrimaryColor : FLinearColor::White;
	if (!bEnabled)
	{
		foregroundColor = ResolveStateColor(EBaseWidgetState::Disabled);
	}
	else if (tokens && effectiveVariant == EBaseWidgetVariant::Warning)
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

	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(foregroundColor));
	}
	bool bShowIcon = false;
	if (IconImage)
	{
		const float iconSize = BaseWidgetPrivate::ResolveIconPreviewSize(Size);
		bShowIcon = Icon != nullptr;
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon, false);
			BaseWidgetPrivate::ApplyFixedImageBrushSize(IconImage.Get(), iconSize);
		}
		else
		{
			BaseWidgetPrivate::ApplyFixedImageBrushSize(IconImage.Get(), iconSize);
			bShowIcon = BaseWidgetPrivate::HasAssignedImageResource(IconImage.Get());
		}
		BaseWidgetPrivate::SetOptionalIconVisibility(IconBox.Get(), IconImage.Get(), bShowIcon);
		IconImage->SetColorAndOpacity(foregroundColor);
	}
	else if (IconBox)
	{
		IconBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IconGlyph)
	{
		IconGlyph->SetVisibility(bShowIcon
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
		IconGlyph->SetColorAndOpacity(foregroundColor);
	}

	FLinearColor surfaceColor = ResolveVariantColor(effectiveVariant);
	FLinearColor frameColor = tokens ? tokens->LineInsetColor : ResolveVariantColor(EBaseWidgetVariant::Secondary);
	if (tokens)
	{
		if (effectiveState == EBaseWidgetState::Hovered)
		{
			surfaceColor = effectiveVariant == EBaseWidgetVariant::Primary
				? tokens->AccentHoverColor
				: tokens->SurfaceControlHoverColor;
		}
		else if (effectiveState == EBaseWidgetState::Pressed)
		{
			surfaceColor = effectiveVariant == EBaseWidgetVariant::Primary
				? tokens->AccentActiveColor
				: tokens->SurfaceControlActiveColor;
		}
		else if (effectiveState == EBaseWidgetState::Disabled)
		{
			surfaceColor = tokens->SurfaceControlColor;
		}
		else if (effectiveState != EBaseWidgetState::Default)
		{
			surfaceColor = ResolveStateColor(effectiveState);
		}
	}
	if (!bEnabled && tokens)
	{
		frameColor = tokens->LineSubtleColor;
	}
	else if (effectiveState == EBaseWidgetState::Hovered && tokens)
	{
		frameColor = tokens->LineFieldHoverColor;
	}
	else if (effectiveState == EBaseWidgetState::Pressed && tokens)
	{
		frameColor = tokens->AccentActiveColor;
	}
	else if (effectiveState == EBaseWidgetState::Selected || bSelected)
	{
		frameColor = tokens ? tokens->AccentColor : ResolveStateColor(EBaseWidgetState::Selected);
	}
	else if (effectiveVariant == EBaseWidgetVariant::Primary && tokens)
	{
		frameColor = tokens->AccentColor;
	}

	BaseWidgetPrivate::ApplyRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		tokens ? tokens->BorderWidth : 1.0f);
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

void UBaseButtonWidget::NativePreConstruct()
{
	UseTransparentCommonStyle();
	Super::NativePreConstruct();
	SynchronizeBaseProperties();
}

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

UBaseIconButtonWidget::UBaseIconButtonWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	Label = FText::GetEmpty();
}

UBaseTabWidget::UBaseTabWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	Label = FText::FromString(TEXT("Overview"));
}

void UBaseTabWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(bSelected
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
		ApplyBorderColor(SelectedIndicator.Get(), ResolveStateColor(EBaseWidgetState::Selected));
	}
}
