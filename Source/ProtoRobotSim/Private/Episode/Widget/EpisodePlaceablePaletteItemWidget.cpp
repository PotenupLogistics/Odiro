#include "Episode/Widget/EpisodePlaceablePaletteItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"

void UEpisodePlaceablePaletteItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UEpisodePlaceablePaletteItemWidget::HandleSelectButtonClicked);
		SelectButton->OnClicked.AddDynamic(this, &UEpisodePlaceablePaletteItemWidget::HandleSelectButtonClicked);
	}
}

void UEpisodePlaceablePaletteItemWidget::SetPropEntry(const FEpisodeStaticObstaclePropEntry& propEntry)
{
	PropEntry = propEntry;

	if (DisplayNameTextBlock)
	{
		DisplayNameTextBlock->SetText(FText::FromString(MakeDisplayNameFromPropId(PropEntry.PropId)));
	}

	if (CategoryTextBlock)
	{
		CategoryTextBlock->SetText(CategoryToText(PropEntry.Category));
	}

	ApplyThumbnailImage();
}

void UEpisodePlaceablePaletteItemWidget::HandleSelectButtonClicked()
{
	if (PropEntry.PropId.IsNone()) return;

	OnSelected.Broadcast(PropEntry.PropId);
}

FText UEpisodePlaceablePaletteItemWidget::CategoryToText(EEpisodeStaticObstaclePropCategory category)
{
	switch (category)
	{
	case EEpisodeStaticObstaclePropCategory::StreetFurniture:
		return FText::FromString(TEXT("Street Furniture"));
	case EEpisodeStaticObstaclePropCategory::TrafficControl:
		return FText::FromString(TEXT("Traffic Control"));
	case EEpisodeStaticObstaclePropCategory::DeliveryItem:
		return FText::FromString(TEXT("Delivery Item"));
	case EEpisodeStaticObstaclePropCategory::Utility:
		return FText::FromString(TEXT("Utility"));
	case EEpisodeStaticObstaclePropCategory::SurfaceObject:
		return FText::FromString(TEXT("Surface Object"));
	case EEpisodeStaticObstaclePropCategory::Unknown:
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

FString UEpisodePlaceablePaletteItemWidget::MakeDisplayNameFromPropId(FName propId)
{
	FString propIdString = propId.ToString();
	const FString obstaclePrefix(TEXT("obstacle."));
	if (propIdString.StartsWith(obstaclePrefix))
	{
		propIdString.RightChopInline(obstaclePrefix.Len());
	}

	return propIdString;
}

FString UEpisodePlaceablePaletteItemWidget::MakeIconSuffixFromPropId(FName propId)
{
	FString propIdString = propId.ToString();
	const FString obstaclePrefix(TEXT("obstacle."));
	if (propIdString.StartsWith(obstaclePrefix))
	{
		propIdString.RightChopInline(obstaclePrefix.Len());
	}

	propIdString.ReplaceInline(TEXT("."), TEXT("_"));
	propIdString.ToLowerInline();
	return propIdString;
}

FString UEpisodePlaceablePaletteItemWidget::BuildThumbnailTextureObjectPath() const
{
	if (PropEntry.PropId.IsNone())
	{
		return FString();
	}

	FString iconDirectory = StaticObstacleIconDirectory;
	iconDirectory.TrimStartAndEndInline();
	FPaths::NormalizeFilename(iconDirectory);
	iconDirectory.RemoveFromEnd(TEXT("/"));
	if (iconDirectory.IsEmpty())
	{
		return FString();
	}

	const FString iconSuffix = MakeIconSuffixFromPropId(PropEntry.PropId);
	if (iconSuffix.IsEmpty())
	{
		return FString();
	}

	const FString iconAssetName = StaticObstacleIconAssetPrefix + iconSuffix;
	return FString::Printf(TEXT("%s/%s.%s"), *iconDirectory, *iconAssetName, *iconAssetName);
}

void UEpisodePlaceablePaletteItemWidget::ApplyThumbnailImage()
{
	if (!ThumbnailImage)
	{
		return;
	}

	const FString thumbnailTexturePath = BuildThumbnailTextureObjectPath();
	UTexture2D* thumbnailTexture = thumbnailTexturePath.IsEmpty()
		? nullptr
		: LoadObject<UTexture2D>(nullptr, *thumbnailTexturePath);

	if (!thumbnailTexture)
	{
		if (bHideThumbnailImageWhenMissing)
		{
			ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	ThumbnailImage->SetBrushFromTexture(thumbnailTexture, bMatchThumbnailImageSizeToTexture);
	ThumbnailImage->SetVisibility(ESlateVisibility::Visible);
}
