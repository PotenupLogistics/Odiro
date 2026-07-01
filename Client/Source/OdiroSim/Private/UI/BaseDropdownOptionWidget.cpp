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
	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (SurfaceBorder)
	{
		SurfaceBorder->SetVerticalAlignment(VAlign_Center);
	}
	if (colors && sizes)
	{
		const bool bRowSelected = IsBaseSelected();
		const EBaseWidgetState effectiveState = GetEffectiveState();
		const bool bRowHovered = effectiveState == EBaseWidgetState::Hovered;
		const bool bRowActive = bRowSelected
			|| effectiveState == EBaseWidgetState::Pressed
			|| effectiveState == EBaseWidgetState::Selected;
		const FLinearColor fillColor = bRowActive
			? colors->SurfaceControlActiveColor
			: (bRowHovered ? colors->SurfaceHoverColor : FLinearColor::Transparent);
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			FLinearColor::Transparent,
			sizes->Radius,
			0.0f);

		if (LabelTextBlock)
		{
			const FLinearColor labelColor = effectiveState == EBaseWidgetState::Disabled
				? colors->TextFaintColor
				: (bRowSelected ? colors->AccentColor : colors->TextPrimaryColor);
			LabelTextBlock->SetColorAndOpacity(FSlateColor(labelColor));
		}
	}
	if (CheckImage)
	{
		if (colors)
		{
			CheckImage->SetColorAndOpacity(colors->AccentColor);
		}
		CheckImage->SetVisibility(IsBaseSelected()
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}
