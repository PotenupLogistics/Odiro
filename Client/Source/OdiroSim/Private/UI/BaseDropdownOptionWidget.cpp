#include "UI/BaseDropdownOptionWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/BaseWidgetPrivate.h"

UBaseDropdownOptionWidget::UBaseDropdownOptionWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	Variant = EBaseWidgetVariant::Ghost;
}

void UBaseDropdownOptionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The parent dropdown owns the single active row; the row must not toggle
	// its own CommonUI selected state on click.
	SetIsSelectable(false);
}

void UBaseDropdownOptionWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (!tokens)
	{
		return;
	}

	const bool bRowSelected = IsBaseSelected();
	const EBaseWidgetState effectiveState = GetEffectiveState();
	const bool bRowHovered = effectiveState == EBaseWidgetState::Hovered;
	const bool bRowActive = bRowSelected
		|| effectiveState == EBaseWidgetState::Pressed
		|| effectiveState == EBaseWidgetState::Selected;
	const FLinearColor fillColor = bRowActive
		? tokens->SurfaceControlActiveColor
		: (bRowHovered ? tokens->SurfaceHoverColor : FLinearColor::Transparent);

	// Borderless row: idle stays transparent; hover/active draw only the row fill.
	BaseWidgetPrivate::ApplyRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		fillColor,
		FLinearColor::Transparent,
		tokens->Radius,
		0.0f);

	if (LabelTextBlock)
	{
		const FLinearColor labelColor = effectiveState == EBaseWidgetState::Disabled
			? tokens->TextFaintColor
			: (bRowSelected ? tokens->AccentColor : tokens->TextPrimaryColor);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(labelColor));
	}
	if (CheckImage)
	{
		CheckImage->SetColorAndOpacity(tokens->AccentColor);
		CheckImage->SetVisibility(bRowSelected
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}
