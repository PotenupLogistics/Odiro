#include "Platform/Widget/RecentProjectCardWidget.h"

#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextWidget.h"

URecentProjectCardWidget::URecentProjectCardWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
}

void URecentProjectCardWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
}

void URecentProjectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CardButton)
	{
		CardButton->OnBaseClicked.RemoveDynamic(this, &URecentProjectCardWidget::HandleCardClicked);
		CardButton->OnBaseClicked.AddDynamic(this, &URecentProjectCardWidget::HandleCardClicked);
	}
}

void URecentProjectCardWidget::NativeDestruct()
{
	if (CardButton)
	{
		CardButton->OnBaseClicked.RemoveDynamic(this, &URecentProjectCardWidget::HandleCardClicked);
	}

	Super::NativeDestruct();
}

FReply URecentProjectCardWidget::NativeOnMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (inMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnContextMenuRequested.Broadcast(this, inMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	if (!CardButton && inMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnSelectedRequested.Broadcast(this);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(inGeometry, inMouseEvent);
}

FReply URecentProjectCardWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (inMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnContextMenuRequested.Broadcast(this, inMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(inGeometry, inMouseEvent);
}

void URecentProjectCardWidget::InitializeCard(const FStartupScreenRecentProjectItem& item)
{
	ProjectPath = item.ProjectPath;
	if (ProjectNameText)
	{
		ProjectNameText->SetText(FText::FromString(item.Title));
	}
	if (ProjectSubtitleText)
	{
		ProjectSubtitleText->SetText(FText::FromString(item.Subtitle));
		ProjectSubtitleText->SetVisibility(item.Subtitle.TrimStartAndEnd().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if (CardButton)
	{
		CardButton->SetDisabled(!item.bEnabled);
	}

	ApplyProjectPreviewThumbnail(item.PreviewImagePath);
	SetDisabled(!item.bEnabled);
	SetSelected(item.bSelected);
}

void URecentProjectCardWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	Super::SetSelected(bSelected);
	if (CardButton)
	{
		CardButton->SetSelected(bSelected);
	}
}

void URecentProjectCardWidget::HandleCardClicked(UBaseButtonWidget*)
{
	OnSelectedRequested.Broadcast(this);
}

bool URecentProjectCardWidget::ApplyProjectPreviewThumbnail(const FString& previewPath)
{
	const FString normalizedPreviewPath = previewPath.TrimStartAndEnd();
	if (normalizedPreviewPath.IsEmpty() || !FPaths::FileExists(normalizedPreviewPath))
	{
		CardThumbnailTexture = nullptr;
		SetMediaTexture(nullptr);
		return false;
	}

	UTexture2D* thumbnailTexture = FImageUtils::ImportFileAsTexture2D(normalizedPreviewPath);
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
