#include "UI/BaseStatusBadgeWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseStatusBadgeWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	if (LabelTextBlock)
	{
		BaseWidgetPrivate::ApplyTextIfSet(LabelTextBlock.Get(), Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Caption);
		if (bHasLabelFontOverride)
		{
			LabelTextBlock->SetFont(LabelFontOverride);
		}
	}

	BaseWidgetPrivate::MakeBorderVisualTransparent(BorderFrame.Get());
	BaseWidgetPrivate::MakeBorderVisualTransparent(SurfaceBorder.Get());
	if (colors)
	{
		const FLinearColor badgeColor = bDisabled
			? colors->GetStateColor(EBaseWidgetState::Disabled)
			: colors->GetStateColor(State);
		if (LabelTextBlock)
		{
			const FLinearColor labelColor = bDisabled
				? colors->TextFaintColor
				: (bHasLabelColorOverride ? LabelColorOverride : badgeColor);
			LabelTextBlock->SetColorAndOpacity(FSlateColor(labelColor));
		}
		ApplyBorderColor(StatusDot.Get(), badgeColor);
	}
	else if (LabelTextBlock && bHasLabelColorOverride)
	{
		LabelTextBlock->SetColorAndOpacity(FSlateColor(LabelColorOverride));
	}
}

void UBaseStatusBadgeWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::SetLabelColorOverride(const FLinearColor inLabelColor)
{
	LabelColorOverride = inLabelColor;
	bHasLabelColorOverride = true;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::ClearLabelColorOverride()
{
	bHasLabelColorOverride = false;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::SetLabelFontOverride(const FSlateFontInfo inLabelFont)
{
	LabelFontOverride = inLabelFont;
	bHasLabelFontOverride = true;
	SynchronizeBaseProperties();
}

void UBaseStatusBadgeWidget::ClearLabelFontOverride()
{
	bHasLabelFontOverride = false;
	SynchronizeBaseProperties();
}
