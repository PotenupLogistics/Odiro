#include "Platform/Widget/ProjectPresetCardWidget.h"

#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextWidget.h"

void UProjectPresetCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CardButton)
	{
		CardButton->OnBaseClicked.RemoveDynamic(this, &UProjectPresetCardWidget::HandleCardClicked);
		CardButton->OnBaseClicked.AddDynamic(this, &UProjectPresetCardWidget::HandleCardClicked);
	}
}

void UProjectPresetCardWidget::NativeDestruct()
{
	if (CardButton)
	{
		CardButton->OnBaseClicked.RemoveDynamic(this, &UProjectPresetCardWidget::HandleCardClicked);
	}

	Super::NativeDestruct();
}

FReply UProjectPresetCardWidget::NativeOnMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (!CardButton && inMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnSelectedRequested.Broadcast(this);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(inGeometry, inMouseEvent);
}

void UProjectPresetCardWidget::InitializeCard(const FProjectCreatePresetItem& item)
{
	Category = item.Category;
	PresetId = item.PresetId;

	if (PresetTitleText)
	{
		PresetTitleText->SetText(FText::FromString(item.Title));
	}
	if (PresetSubtitleText)
	{
		PresetSubtitleText->SetText(FText::FromString(item.Subtitle));
		PresetSubtitleText->SetVisibility(item.Subtitle.TrimStartAndEnd().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	ApplyPresetThumbnail(item.ThumbnailPath);
	SetSelected(item.bSelected);
}

void UProjectPresetCardWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	Super::SetSelected(bSelected);
	if (CardButton)
	{
		CardButton->SetSelected(bSelected);
	}
}

void UProjectPresetCardWidget::HandleCardClicked(UBaseButtonWidget*)
{
	OnSelectedRequested.Broadcast(this);
}

bool UProjectPresetCardWidget::ApplyPresetThumbnail(const FString& thumbnailPath)
{
	const FString normalizedThumbnailPath = thumbnailPath.TrimStartAndEnd();
	if (normalizedThumbnailPath.IsEmpty() || !FPaths::FileExists(normalizedThumbnailPath))
	{
		CardThumbnailTexture = nullptr;
		SetMediaTexture(nullptr);
		return false;
	}

	UTexture2D* thumbnailTexture = FImageUtils::ImportFileAsTexture2D(normalizedThumbnailPath);
	if (!thumbnailTexture)
	{
		CardThumbnailTexture = nullptr;
		SetMediaTexture(nullptr);
		return false;
	}

	CardThumbnailTexture = thumbnailTexture;
	SetMediaTexture(CardThumbnailTexture.Get());
	return true;
}
