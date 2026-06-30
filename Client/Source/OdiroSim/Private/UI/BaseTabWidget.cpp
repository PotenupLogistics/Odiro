#include "UI/BaseTabWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseTabWidget::SynchronizeBaseProperties()
{
	const bool bRequestedDisabled = bDisabled || State == EBaseWidgetState::Disabled;
	const EBaseWidgetState requestedState = State;

	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (!colors || !sizes)
	{
		return;
	}

	const EBaseWidgetState effectiveState = GetEffectiveState();
	const bool bEnabled = !bRequestedDisabled;
	const bool bActive = bSelected
		|| requestedState == EBaseWidgetState::Selected
		|| effectiveState == EBaseWidgetState::Selected;
	FLinearColor surfaceColor = FLinearColor::Transparent;
	FLinearColor frameColor = FLinearColor::Transparent;
	float borderWidth = 0.0f;

	if (bActive)
	{
		surfaceColor = effectiveState == EBaseWidgetState::Pressed
			? colors->SurfaceControlActiveColor
			: colors->SurfacePanelColor;
		frameColor = colors->LineSubtleColor;
		borderWidth = sizes->BorderWidth;
	}
	else if ((effectiveState == EBaseWidgetState::Hovered || effectiveState == EBaseWidgetState::Pressed) && bEnabled)
	{
		surfaceColor = colors->SurfaceHoverSoftColor;
	}

	BaseWidgetPrivate::ApplyTopRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		sizes->Radius,
		borderWidth);

	const FLinearColor labelColor = !bEnabled
		? colors->TextFaintColor
		: (bActive ? colors->TextStrongColor : colors->TextSecondaryColor);
	if (LabelTextBlock)
	{
		LabelTextBlock->SetColorAndOpacity(FSlateColor(labelColor));
	}
	if (IconImage)
	{
		IconImage->SetColorAndOpacity(labelColor);
	}
	if (IconGlyph)
	{
		IconGlyph->SetColorAndOpacity(FSlateColor(labelColor));
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
}
