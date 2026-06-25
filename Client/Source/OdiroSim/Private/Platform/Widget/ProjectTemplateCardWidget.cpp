
#include "Platform/Widget/ProjectTemplateCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Styling/SlateBrush.h"

namespace
{
	FLinearColor MakeProjectTemplateCardSrgbColor(const uint8 red, const uint8 green, const uint8 blue, const float alpha = 1.0f)
	{
		const uint8 alphaByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(alpha * 255.0f), 0, 255));
		return FLinearColor::FromSRGBColor(FColor(red, green, blue, alphaByte));
	}

	void TintProjectTemplateCardBrush(FSlateBrush& brush, const FLinearColor& color)
	{
		brush.TintColor = FSlateColor(color);
	}

	FLinearColor WithProjectTemplateCardAlpha(const FLinearColor& color, const float alpha)
	{
		FLinearColor result = color;
		result.A = alpha;
		return result;
	}

	// User project root 경로에서 preview.png가 존재할 때만 절대 경로를 반환한다.
	FString ResolveProjectTemplateCardPreviewPath(const FString& projectPath)
	{
		FString normalizedProjectPath = projectPath.TrimStartAndEnd();
		normalizedProjectPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		FPaths::NormalizeDirectoryName(normalizedProjectPath);
		if (normalizedProjectPath.IsEmpty()
			|| FPaths::IsRelative(normalizedProjectPath)
			|| !FPaths::DirectoryExists(normalizedProjectPath))
		{
			return FString();
		}

		FString previewPath = FPaths::Combine(normalizedProjectPath, TEXT("preview.png"));
		FPaths::NormalizeFilename(previewPath);
		return FPaths::FileExists(previewPath) ? previewPath : FString();
	}
}

UProjectTemplateCardWidget::UProjectTemplateCardWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
	, DefaultBackgroundColor(MakeProjectTemplateCardSrgbColor(0x1b, 0x1b, 0x1b))
	, HoverBackgroundColor(MakeProjectTemplateCardSrgbColor(0x38, 0x38, 0x38))
	, PressedBackgroundColor(MakeProjectTemplateCardSrgbColor(0x2f, 0x2f, 0x2f))
	, ActiveBackgroundColor(MakeProjectTemplateCardSrgbColor(0x00, 0x70, 0xe0))
	, ActiveHoverBackgroundColor(MakeProjectTemplateCardSrgbColor(0x0a, 0x86, 0xff))
	, ActivePressedBackgroundColor(MakeProjectTemplateCardSrgbColor(0x00, 0x50, 0xa0))
	, DefaultTextColor(MakeProjectTemplateCardSrgbColor(0xc0, 0xc0, 0xc0))
	, ActiveTextColor(MakeProjectTemplateCardSrgbColor(0xff, 0xff, 0xff))
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

FReply UProjectTemplateCardWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (inMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnContextRequested.Broadcast(this);
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(inGeometry, inMouseEvent);
}

void UProjectTemplateCardWidget::InitializeCard(
	const FString& itemId,
	const FString& displayName,
	const FString& subtitle)
{
	ItemViewModel = nullptr;
	ItemId = itemId.TrimStartAndEnd();

	const FString visibleName = displayName.TrimStartAndEnd().IsEmpty()
		? ItemId
		: displayName.TrimStartAndEnd();
	if (CardNameLabel)
	{
		CardNameLabel->SetText(FText::FromString(visibleName));
	}
	if (CardSubtitleLabel)
	{
		const FString visibleSubtitle = subtitle.TrimStartAndEnd();
		CardSubtitleLabel->SetText(FText::FromString(visibleSubtitle));
		CardSubtitleLabel->SetVisibility(visibleSubtitle.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	RefreshVisualState();
}

void UProjectTemplateCardWidget::InitializeFromItemViewModel(UOdiroListItemViewModel* itemViewModel)
{
	ItemViewModel = itemViewModel;
	if (!ItemViewModel)
	{
		InitializeCard(FString(), FString(), FString());
		SetSelected(false);
		if (CardButton)
		{
			CardButton->SetIsEnabled(false);
		}
		return;
	}

	InitializeCard(ItemViewModel->GetItemId(), ItemViewModel->GetTitle(), ItemViewModel->GetSubtitle());
	ItemViewModel = itemViewModel;
	ApplyProjectPreviewThumbnail(ItemViewModel->GetPayloadPath());
	SetSelected(ItemViewModel->IsSelected());
	if (CardButton)
	{
		CardButton->SetIsEnabled(ItemViewModel->IsEnabled());
	}
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

// User project preview.png를 transient texture로 읽어 thumbnail image에 적용한다.
bool UProjectTemplateCardWidget::ApplyProjectPreviewThumbnail(const FString& projectPath)
{
	if (!CardThumbnailImage)
	{
		return false;
	}

	const FString previewPath = ResolveProjectTemplateCardPreviewPath(projectPath);
	if (previewPath.IsEmpty())
	{
		return false;
	}

	UTexture2D* thumbnailTexture = FImageUtils::ImportFileAsTexture2D(previewPath);
	if (!thumbnailTexture)
	{
		return false;
	}

	CardThumbnailTexture = thumbnailTexture;
	CardThumbnailImage->SetBrushFromTexture(CardThumbnailTexture.Get(), false);
	CardThumbnailImage->SetColorAndOpacity(FLinearColor::White);
	CardThumbnailImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UProjectTemplateCardWidget::RefreshVisualState()
{
	const FLinearColor textColor = ResolveTextColor();

	if (CardButton)
	{
		FButtonStyle buttonStyle = CardButton->GetStyle();
		TintProjectTemplateCardBrush(buttonStyle.Normal, ResolveNormalBackgroundColor());
		TintProjectTemplateCardBrush(buttonStyle.Hovered, ResolveHoveredBackgroundColor());
		TintProjectTemplateCardBrush(buttonStyle.Pressed, ResolvePressedBackgroundColor());
		TintProjectTemplateCardBrush(buttonStyle.Disabled, WithProjectTemplateCardAlpha(DefaultBackgroundColor, 0.45f));
		buttonStyle.SetNormalForeground(FSlateColor(DefaultTextColor));
		buttonStyle.SetHoveredForeground(FSlateColor(ActiveTextColor));
		buttonStyle.SetPressedForeground(FSlateColor(ActiveTextColor));
		buttonStyle.SetDisabledForeground(FSlateColor(WithProjectTemplateCardAlpha(DefaultTextColor, 0.45f)));
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
	if (CardSubtitleLabel)
	{
		CardSubtitleLabel->SetColorAndOpacity(FSlateColor(textColor));
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
