#include "UI/BaseWidget.h"

#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

const UBaseWidgetTokenCatalog* UBaseWidget::GetResolvedBaseTokens() const
{
	return BaseWidgetPrivate::ResolveBaseTokenCatalog(BaseTokens);
}

FBaseTextStyleToken UBaseWidget::ResolveTextStyle(const EBaseTextRole role) const
{
	return BaseWidgetPrivate::ResolveTextStyle(
		BaseTokens,
		BaseWidgetPrivate::ResolveSizedTextRole(role, Size));
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
	BaseWidgetPrivate::ApplySizeConstraints(RootSize.Get(), SizeConstraints);
	if (RootSizeBox.Get() != RootSize.Get())
	{
		BaseWidgetPrivate::ApplySizeConstraints(RootSizeBox.Get(), SizeConstraints);
	}

	if (BorderFrame)
	{
		const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
		ApplyBorderColor(BorderFrame.Get(), tokens ? tokens->LineFieldColor : FLinearColor::White);
	}
}

void UBaseWidget::SetBaseSize(const EBaseWidgetSize inSize)
{
	Size = inSize;
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
	BaseWidgetPrivate::ApplyTextStyle(textBlock, ResolveTextStyle(role));
}

void UBaseWidget::ApplyBorderColor(UBorder* border, const FLinearColor& color) const
{
	BaseWidgetPrivate::ApplyBorderBrushTint(border, color);
}
