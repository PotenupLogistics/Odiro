#include "UI/BaseListItemWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/BaseWidgetPrivate.h"

void UBaseListItemWidget::SynchronizeBaseProperties()
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
}

void UBaseListItemWidget::SetIcon(UTexture2D* inIcon)
{
	Icon = inIcon;
	SynchronizeBaseProperties();
}

void UBaseListItemWidget::SetIconSize(const float inIconSize)
{
	IconSize = FMath::Max(inIconSize, 1.0f);
	SynchronizeBaseProperties();
}
