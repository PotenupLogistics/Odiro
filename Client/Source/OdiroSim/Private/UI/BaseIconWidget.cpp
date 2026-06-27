#include "UI/BaseIconWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseIconWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (!IconImage)
	{
		return;
	}

	if (Icon)
	{
		IconImage->SetBrushFromTexture(Icon, false);
	}
	const bool bHasIcon = Icon != nullptr || BaseWidgetPrivate::HasAssignedImageResource(IconImage.Get());
	BaseWidgetPrivate::ApplyIconSize(IconBox.Get(), IconImage.Get(), IconSize);
	BaseWidgetPrivate::SetOptionalIconVisibility(IconBox.Get(), IconImage.Get(), bHasIcon);
	const FLinearColor iconColor = bDisabled
		? ResolveStateColor(EBaseWidgetState::Disabled)
		: ResolveVariantColor(Variant);
	IconImage->SetColorAndOpacity(iconColor);
}

void UBaseIconWidget::SetIcon(UTexture2D* inIcon)
{
	Icon = inIcon;
	SynchronizeBaseProperties();
}

void UBaseIconWidget::SetVariant(const EBaseWidgetVariant inVariant)
{
	Variant = inVariant;
	SynchronizeBaseProperties();
}

void UBaseIconWidget::SetIconSize(const float inIconSize)
{
	IconSize = FMath::Max(inIconSize, 1.0f);
	SynchronizeBaseProperties();
}

void UBaseIconWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}
