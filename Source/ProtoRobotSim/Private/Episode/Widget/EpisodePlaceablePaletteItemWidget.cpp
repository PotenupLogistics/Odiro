#include "Episode/Widget/EpisodePlaceablePaletteItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

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
	SetPaletteItemEntry(MakeStaticObstaclePaletteItemEntry(propEntry));
}

void UEpisodePlaceablePaletteItemWidget::SetPaletteItemEntry(const FEpisodePaletteItemEntry& paletteItemEntry)
{
	PaletteItemEntry = paletteItemEntry;

	if (DisplayNameTextBlock)
	{
		DisplayNameTextBlock->SetText(PaletteItemEntry.DisplayName);
	}

	if (CategoryTextBlock)
	{
		CategoryTextBlock->SetText(PaletteItemEntry.CategoryText);
	}

	ApplyThumbnailImage();
}

void UEpisodePlaceablePaletteItemWidget::HandleSelectButtonClicked()
{
	if (PaletteItemEntry.AssetId.IsNone()) return;

	OnSelected.Broadcast(PaletteItemEntry.ItemType, PaletteItemEntry.AssetId);
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

FEpisodePaletteItemEntry UEpisodePlaceablePaletteItemWidget::MakeStaticObstaclePaletteItemEntry(
	const FEpisodeStaticObstaclePropEntry& propEntry)
{
	FEpisodePaletteItemEntry paletteItemEntry;
	paletteItemEntry.ItemType = EEpisodePaletteItemType::StaticObstacle;
	paletteItemEntry.AssetId = propEntry.PropId;
	paletteItemEntry.DisplayName = propEntry.DisplayName.IsEmpty()
		? FText::FromString(MakeDisplayNameFromPropId(propEntry.PropId))
		: propEntry.DisplayName;
	paletteItemEntry.CategoryText = CategoryToText(propEntry.Category);
	paletteItemEntry.ThumbnailTexture = propEntry.ThumbnailTexture;
	return paletteItemEntry;
}

void UEpisodePlaceablePaletteItemWidget::ApplyThumbnailImage()
{
	if (!ThumbnailImage)
	{
		return;
	}

	UTexture2D* thumbnailTexture = PaletteItemEntry.ThumbnailTexture.IsNull()
		? nullptr
		: PaletteItemEntry.ThumbnailTexture.LoadSynchronous();

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
