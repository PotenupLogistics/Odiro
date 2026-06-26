#include "UI/BaseCardWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/BaseEmptyStateWidget.h"
#include "UI/BaseMetricCardWidget.h"
#include "UI/BaseToolbarWidget.h"
#include "UI/BaseWidgetPrivate.h"

UBaseCardWidget::UBaseCardWidget()
	: Label(FText::FromString(TEXT("Card title")))
	, Description(FText::FromString(TEXT("Supporting information for this surface")))
{
}

void UBaseCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	const bool bStateDisabled = bDisabled || State == EBaseWidgetState::Disabled;
	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		if (bStateDisabled)
		{
			LabelTextBlock->SetColorAndOpacity(FSlateColor(ResolveStateColor(EBaseWidgetState::Disabled)));
		}
	}
	if (DescriptionTextBlock)
	{
		DescriptionTextBlock->SetText(Description);
		ApplyTextStyle(DescriptionTextBlock.Get(), EBaseTextRole::Caption);
		if (bStateDisabled)
		{
			DescriptionTextBlock->SetColorAndOpacity(FSlateColor(ResolveStateColor(EBaseWidgetState::Disabled)));
		}
		DescriptionTextBlock->SetVisibility(Description.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	FLinearColor surfaceColor = tokens ? tokens->SurfacePanelColor : ResolveVariantColor(EBaseWidgetVariant::Neutral);
	FLinearColor frameColor = tokens ? tokens->LineFieldColor : ResolveVariantColor(EBaseWidgetVariant::Neutral);
	if (State == EBaseWidgetState::Hovered && tokens)
	{
		surfaceColor = tokens->SurfaceHoverColor;
	}
	else if (bStateDisabled && tokens)
	{
		surfaceColor = tokens->SurfaceControlColor;
	}
	if (bSelected)
	{
		frameColor = tokens ? tokens->AccentColor : ResolveStateColor(EBaseWidgetState::Selected);
	}
	else if (bStateDisabled && tokens)
	{
		frameColor = tokens->LineSubtleColor;
	}
	else if (State == EBaseWidgetState::Hovered && tokens)
	{
		frameColor = tokens->LineFieldHoverColor;
	}
	else if (State != EBaseWidgetState::Default && State != EBaseWidgetState::Hovered)
	{
		frameColor = ResolveStateColor(State);
	}

	BaseWidgetPrivate::ApplyRoundedSurface(
		BorderFrame.Get(),
		SurfaceBorder.Get(),
		surfaceColor,
		frameColor,
		tokens ? tokens->BorderWidth : 1.0f);

	if (StateMarker)
	{
		FLinearColor markerColor = ResolveVariantColor(Variant);
		bool bShowMarker = Variant != EBaseWidgetVariant::Neutral;
		if (bSelected)
		{
			markerColor = ResolveStateColor(EBaseWidgetState::Selected);
			bShowMarker = true;
		}
		if (bStateDisabled)
		{
			markerColor = ResolveStateColor(EBaseWidgetState::Disabled);
			bShowMarker = true;
		}
		else if (State != EBaseWidgetState::Default && State != EBaseWidgetState::Hovered)
		{
			markerColor = ResolveStateColor(State);
			bShowMarker = true;
		}
		ApplyBorderColor(StateMarker.Get(), markerColor);
		StateMarker->SetVisibility(bShowMarker
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UBaseCardWidget::SetLabel(const FText inLabel)
{
	Label = inLabel;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetDescription(const FText inDescription)
{
	Description = inDescription;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetVariant(const EBaseWidgetVariant inVariant)
{
	Variant = inVariant;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetBaseState(const EBaseWidgetState inState)
{
	State = inState;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	SynchronizeBaseProperties();
}

void UBaseCardWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}

UBaseToolbarWidget::UBaseToolbarWidget()
{
	Label = FText::FromString(TEXT("Toolbar"));
	Description = FText::FromString(TEXT("Command group surface"));
}

UBaseMetricCardWidget::UBaseMetricCardWidget()
	: ValueText(FText::FromString(TEXT("98%")))
{
	Label = FText::FromString(TEXT("Success rate"));
	Description = FText::FromString(TEXT("Across selected run"));
}

void UBaseMetricCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (ValueTextBlock)
	{
		ValueTextBlock->SetText(ValueText);
		ApplyTextStyle(ValueTextBlock.Get(), EBaseTextRole::Value);
	}
}

void UBaseMetricCardWidget::SetValueText(const FText inValueText)
{
	ValueText = inValueText;
	SynchronizeBaseProperties();
}

UBaseEmptyStateWidget::UBaseEmptyStateWidget()
{
	Label = FText::FromString(TEXT("No results"));
	Description = FText::FromString(TEXT("Try changing filters or running another scenario"));
}
