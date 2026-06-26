#include "UI/BaseListItemWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/BaseNotificationRowWidget.h"
#include "UI/BaseWidgetPrivate.h"

UBaseListItemWidget::UBaseListItemWidget()
{
	Label = FText::FromString(TEXT("List item"));
	Description = FText::FromString(TEXT("Secondary row metadata"));
}

void UBaseListItemWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	if (!IconImage)
	{
		return;
	}

	const float iconSize = BaseWidgetPrivate::ResolveIconPreviewSize(EBaseWidgetSize::Medium);
	if (Icon)
	{
		IconImage->SetBrushFromTexture(Icon, false);
		BaseWidgetPrivate::ApplyFixedImageBrushSize(IconImage.Get(), iconSize);
	}
	else
	{
		BaseWidgetPrivate::ApplyFixedImageBrushSize(IconImage.Get(), iconSize);
	}
	const bool bShowIcon = Icon != nullptr || BaseWidgetPrivate::HasAssignedImageResource(IconImage.Get());
	BaseWidgetPrivate::SetOptionalIconVisibility(IconBox.Get(), IconImage.Get(), bShowIcon);
}

void UBaseListItemWidget::SetIcon(UTexture2D* inIcon)
{
	Icon = inIcon;
	SynchronizeBaseProperties();
}

UBaseNotificationRowWidget::UBaseNotificationRowWidget()
{
	Label = FText::FromString(TEXT("Optimization complete"));
	Description = FText::FromString(TEXT("New recommendation is ready for review"));
}
