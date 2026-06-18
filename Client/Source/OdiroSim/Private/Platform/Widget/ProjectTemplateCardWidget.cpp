
#include "Platform/Widget/ProjectTemplateCardWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"

namespace
{
	FLinearColor MakeSrgbColor(const uint8 red, const uint8 green, const uint8 blue, const float alpha = 1.0f)
	{
		const uint8 alphaByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(alpha * 255.0f), 0, 255));
		return FLinearColor::FromSRGBColor(FColor(red, green, blue, alphaByte));
	}

	void TintBrush(FSlateBrush& brush, const FLinearColor& color)
	{
		brush.TintColor = FSlateColor(color);
	}

	FLinearColor WithAlpha(const FLinearColor& color, const float alpha)
	{
		FLinearColor result = color;
		result.A = alpha;
		return result;
	}
}

UProjectTemplateCardWidget::UProjectTemplateCardWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
	, DefaultBackgroundColor(MakeSrgbColor(0x2f, 0x2f, 0x2f))
	, HoverBackgroundColor(MakeSrgbColor(0x57, 0x57, 0x57))
	, PressedBackgroundColor(MakeSrgbColor(0x27, 0x27, 0x27))
	, ActiveBackgroundColor(MakeSrgbColor(0x00, 0x70, 0xe0))
	, ActiveHoverBackgroundColor(MakeSrgbColor(0x0e, 0x86, 0xff))
	, ActivePressedBackgroundColor(MakeSrgbColor(0x00, 0x50, 0xa0))
	, DefaultTextColor(MakeSrgbColor(0xc0, 0xc0, 0xc0))
	, ActiveTextColor(MakeSrgbColor(0xff, 0xff, 0xff))
{
}

void UProjectTemplateCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CardButton)
	{
		CardButton->OnClicked.RemoveDynamic(this, &UProjectTemplateCardWidget::HandleCardClicked);
		CardButton->OnClicked.AddDynamic(this, &UProjectTemplateCardWidget::HandleCardClicked);
		CardButton->OnHovered.RemoveDynamic(this, &UProjectTemplateCardWidget::HandleCardHovered);
		CardButton->OnHovered.AddDynamic(this, &UProjectTemplateCardWidget::HandleCardHovered);
		CardButton->OnUnhovered.RemoveDynamic(this, &UProjectTemplateCardWidget::HandleCardUnhovered);
		CardButton->OnUnhovered.AddDynamic(this, &UProjectTemplateCardWidget::HandleCardUnhovered);
	}

	RefreshVisualState();
}

void UProjectTemplateCardWidget::InitializeCard(const FString& templateId, const FString& displayName)
{
	TemplateId = templateId.TrimStartAndEnd();

	const FString visibleName = displayName.TrimStartAndEnd().IsEmpty()
		? TemplateId
		: displayName.TrimStartAndEnd();
	if (CardNameLabel)
	{
		CardNameLabel->SetText(FText::FromString(visibleName));
	}

	RefreshVisualState();
}

void UProjectTemplateCardWidget::SetSelected(const bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	RefreshVisualState();
}

void UProjectTemplateCardWidget::HandleCardClicked()
{
	OnSelectedRequested.Broadcast(this);
}

void UProjectTemplateCardWidget::HandleCardHovered()
{
	bHovered = true;
	RefreshVisualState();
}

void UProjectTemplateCardWidget::HandleCardUnhovered()
{
	bHovered = false;
	RefreshVisualState();
}

void UProjectTemplateCardWidget::RefreshVisualState()
{
	const FLinearColor textColor = ResolveTextColor();

	if (CardButton)
	{
		FButtonStyle buttonStyle = CardButton->GetStyle();
		TintBrush(buttonStyle.Normal, ResolveNormalBackgroundColor());
		TintBrush(buttonStyle.Hovered, ResolveHoveredBackgroundColor());
		TintBrush(buttonStyle.Pressed, ResolvePressedBackgroundColor());
		TintBrush(buttonStyle.Disabled, WithAlpha(DefaultBackgroundColor, 0.45f));
		buttonStyle.SetNormalForeground(FSlateColor(DefaultTextColor));
		buttonStyle.SetHoveredForeground(FSlateColor(ActiveTextColor));
		buttonStyle.SetPressedForeground(FSlateColor(ActiveTextColor));
		buttonStyle.SetDisabledForeground(FSlateColor(WithAlpha(DefaultTextColor, 0.45f)));
		buttonStyle.SetNormalPadding(FMargin(0.0f));
		buttonStyle.SetPressedPadding(FMargin(0.0f));
		CardButton->SetStyle(buttonStyle);
		CardButton->SetBackgroundColor(FLinearColor::White);
		CardButton->SetColorAndOpacity(FLinearColor::White);
	}
	if (CardNameLabel)
	{
		CardNameLabel->SetColorAndOpacity(FSlateColor(textColor));
	}
}

FLinearColor UProjectTemplateCardWidget::ResolveNormalBackgroundColor() const
{
	return bSelected ? ActiveBackgroundColor : DefaultBackgroundColor;
}

FLinearColor UProjectTemplateCardWidget::ResolveHoveredBackgroundColor() const
{
	return bSelected ? ActiveHoverBackgroundColor : HoverBackgroundColor;
}

FLinearColor UProjectTemplateCardWidget::ResolvePressedBackgroundColor() const
{
	return bSelected ? ActivePressedBackgroundColor : PressedBackgroundColor;
}

FLinearColor UProjectTemplateCardWidget::ResolveTextColor() const
{
	return (bSelected || bHovered) ? ActiveTextColor : DefaultTextColor;
}
