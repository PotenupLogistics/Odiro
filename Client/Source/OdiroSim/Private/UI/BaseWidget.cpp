#include "UI/BaseWidget.h"

#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

const UBaseWidgetColorCatalog* UBaseWidget::GetResolvedBaseColors() const
{
	return BaseWidgetPrivate::ResolveBaseColorCatalog(ColorsOverride);
}

const UBaseWidgetSizeCatalog* UBaseWidget::GetResolvedBaseSizes() const
{
	return BaseWidgetPrivate::ResolveBaseSizeCatalog(SizesOverride);
}

FBaseTextStyleToken UBaseWidget::ResolveTextStyle(const EBaseTextRole role) const
{
	FBaseTextStyleToken style;
	BaseWidgetPrivate::ResolveTextStyle(ColorsOverride, SizesOverride, role, style);
	return style;
}

FLinearColor UBaseWidget::ResolveVariantColor(const EBaseWidgetVariant variant) const
{
	return BaseWidgetPrivate::ResolveVariantColor(ColorsOverride, variant);
}

FLinearColor UBaseWidget::ResolveStateColor(const EBaseWidgetState state) const
{
	return BaseWidgetPrivate::ResolveStateColor(ColorsOverride, state);
}

void UBaseWidget::SynchronizeBaseProperties()
{
	BaseWidgetPrivate::ApplySizeConstraints(RootSize.Get(), SizeConstraints);
	if (RootSizeBox.Get() != RootSize.Get())
	{
		BaseWidgetPrivate::ApplySizeConstraints(RootSizeBox.Get(), SizeConstraints);
	}

	if (BorderFrame)
	{
		const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
		if (colors)
		{
			ApplyBorderColor(BorderFrame.Get(), colors->LineFieldColor);
		}
	}
}

void UBaseWidget::NotifyBaseVisualStateChanged(const EBaseWidgetState state, const bool bForce)
{
	if (!bForce && bHasBroadcastVisualState && LastBroadcastVisualState == state)
	{
		return;
	}

	LastBroadcastVisualState = state;
	bHasBroadcastVisualState = true;
	ReceiveBaseVisualStateChanged(state);
}

void UBaseWidget::SetColorsOverride(const TSoftObjectPtr<UBaseWidgetColorCatalog> inColorsOverride)
{
	ColorsOverride = inColorsOverride;
	SynchronizeBaseProperties();
}

void UBaseWidget::SetSizesOverride(const TSoftObjectPtr<UBaseWidgetSizeCatalog> inSizesOverride)
{
	SizesOverride = inSizesOverride;
	SynchronizeBaseProperties();
}

void UBaseWidget::SetSizeConstraints(const FBaseWidgetSizeConstraints inSizeConstraints)
{
	SizeConstraints = BaseWidgetPrivate::NormalizeSizeConstraints(inSizeConstraints);
	SynchronizeBaseProperties();
}

void UBaseWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();
	SynchronizeBaseProperties();
}

void UBaseWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SynchronizeBaseProperties();
}

void UBaseWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SynchronizeBaseProperties();
}

void UBaseWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SynchronizeBaseProperties();
}

void UBaseWidget::ApplyTextStyle(UTextBlock* textBlock, const EBaseTextRole role) const
{
	FBaseTextStyleToken style;
	if (BaseWidgetPrivate::ResolveTextStyle(ColorsOverride, SizesOverride, role, style))
	{
		BaseWidgetPrivate::ApplyTextStyle(textBlock, style);
	}
}

void UBaseWidget::ApplyBorderColor(UBorder* border, const FLinearColor& color) const
{
	BaseWidgetPrivate::ApplyBorderBrushTint(border, color);
}
