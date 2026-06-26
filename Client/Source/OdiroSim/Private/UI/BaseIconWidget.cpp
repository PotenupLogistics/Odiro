#include "UI/BaseIconWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

UBaseIconWidget::UBaseIconWidget()
	: Variant(EBaseWidgetVariant::Info)
{
}

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
	const FLinearColor iconColor = bDisabled
		? ResolveStateColor(EBaseWidgetState::Disabled)
		: ResolveVariantColor(Variant);
	const float iconSize = BaseWidgetPrivate::ResolveIconPreviewSize(Size);
	if (!Icon)
	{
		IconImage->SetBrush(BaseWidgetPrivate::MakeColorBrush(iconColor, FVector2D(iconSize, iconSize)));
	}
	BaseWidgetPrivate::ApplyFixedImageBrushSize(IconImage.Get(), iconSize);
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

void UBaseIconWidget::SetBaseSize(const EBaseWidgetSize inSize)
{
	Size = inSize;
	SynchronizeBaseProperties();
}

void UBaseIconWidget::SetDisabled(const bool bInDisabled)
{
	bDisabled = bInDisabled;
	SynchronizeBaseProperties();
}
