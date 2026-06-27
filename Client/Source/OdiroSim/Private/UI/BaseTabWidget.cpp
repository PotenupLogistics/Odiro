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

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (!tokens)
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
			? tokens->SurfaceControlActiveColor
			: tokens->SurfacePanelColor;
		frameColor = tokens->LineSubtleColor;
		borderWidth = tokens->BorderWidth;
	}
	else if ((effectiveState == EBaseWidgetState::Hovered || effectiveState == EBaseWidgetState::Pressed) && bEnabled)
	{
		surfaceColor = tokens->SurfaceHoverSoftColor;
	}

	BaseWidgetPrivate::ApplyTopRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		tokens->Radius,
		borderWidth);

	if (LabelTextBlock)
	{
		const FLinearColor labelColor = !bEnabled
			? tokens->TextFaintColor
			: (bActive ? tokens->TextStrongColor : tokens->TextSecondaryColor);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(labelColor));
		if (IconImage)
		{
			IconImage->SetColorAndOpacity(labelColor);
		}
		if (IconGlyph)
		{
			IconGlyph->SetColorAndOpacity(FSlateColor(labelColor));
		}
	}

	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
}
