#include "UI/BaseWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

const UBaseWidgetTokenCatalog* UBaseWidget::GetResolvedBaseTokens() const
{
	return BaseWidgetPrivate::ResolveBaseTokenCatalog(BaseTokens);
}

FBaseTextStyleToken UBaseWidget::ResolveTextStyle(const EBaseTextRole role) const
{
	return BaseWidgetPrivate::ResolveTextStyle(BaseTokens, role);
}

FLinearColor UBaseWidget::ResolveVariantColor(const EBaseWidgetVariant variant) const
{
	return BaseWidgetPrivate::ResolveVariantColor(BaseTokens, variant);
}

FLinearColor UBaseWidget::ResolveStateColor(const EBaseWidgetState state) const
{
	return BaseWidgetPrivate::ResolveStateColor(BaseTokens, state);
}

void UBaseWidget::SynchronizeBaseProperties()
{
	if (BorderFrame)
	{
		const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
		ApplyBorderColor(BorderFrame.Get(), tokens ? tokens->LineFieldColor : FLinearColor::White);
	}
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
	BaseWidgetPrivate::ApplyTextStyle(textBlock, ResolveTextStyle(role));
}

void UBaseWidget::ApplyBorderColor(UBorder* border, const FLinearColor& color) const
{
	BaseWidgetPrivate::ApplyBorderBrushTint(border, color);
}
