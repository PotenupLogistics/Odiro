#include "UI/BaseIconWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

namespace
{
	// A resource-free preview brush stays invisible to runtime image-assignment checks.
	FSlateBrush MakeDesignTimePlaceholderBrush(const float iconSize)
	{
		const float resolvedSize = FMath::Max(iconSize, 1.0f);
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.ImageSize = FVector2D(resolvedSize, resolvedSize);
		brush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
		return brush;
	}
}

void UBaseIconWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (!IconImage)
	{
		return;
	}

	if (Icon)
	{
		IconImage->SetBrushFromTexture(Icon, false);
	}
	else if (IsDesignTime() && !BaseWidgetPrivate::HasAssignedImageResource(IconImage.Get()))
	{
		IconImage->SetBrush(MakeDesignTimePlaceholderBrush(IconSize));
	}
	const bool bHasIcon = Icon != nullptr || BaseWidgetPrivate::HasAssignedImageResource(IconImage.Get());
	BaseWidgetPrivate::ApplyIconSize(IconBox.Get(), IconImage.Get(), IconSize);
	const bool bShowIcon = bHasIcon || IsDesignTime();
	BaseWidgetPrivate::SetOptionalIconVisibility(IconBox.Get(), IconImage.Get(), bShowIcon);
	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	if (colors)
	{
		FLinearColor resolvedIconColor = bDisabled
			? colors->GetStateColor(EBaseWidgetState::Disabled)
			: colors->GetVariantColor(Variant);
		if (!bHasIcon && IsDesignTime())
		{
			resolvedIconColor = colors->GetStateColor(EBaseWidgetState::Disabled);
			resolvedIconColor.A = 0.65f;
		}
		IconImage->SetColorAndOpacity(resolvedIconColor);
	}
}

void UBaseIconWidget::SetIcon(UTexture2D* inIcon)
{
	Icon = inIcon;
	SynchronizeBaseProperties();
}

void UBaseIconWidget::SetVariant(const EBaseWidgetVariant inVariant)
{
	Variant = inVariant;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(EBaseWidgetState::Default);
}

void UBaseIconWidget::SetIconSize(const float inIconSize)
{
	IconSize = FMath::Max(inIconSize, 1.0f);
	SynchronizeBaseProperties();
}

void UBaseIconWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
	NotifyBaseVisualStateChanged(bDisabled ? EBaseWidgetState::Disabled : EBaseWidgetState::Default);
}
