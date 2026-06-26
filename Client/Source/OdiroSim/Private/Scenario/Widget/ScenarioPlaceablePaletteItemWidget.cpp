#include "Scenario/Widget/ScenarioPlaceablePaletteItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"

void UScenarioPlaceablePaletteItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceablePaletteItemWidget::HandleSelectButtonClicked);
		SelectButton->OnClicked.AddDynamic(this, &UScenarioPlaceablePaletteItemWidget::HandleSelectButtonClicked);
	}
}

void UScenarioPlaceablePaletteItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(CategoryTextBlock.Get(), EWidgetTextStyleRole::Label);
}

void UScenarioPlaceablePaletteItemWidget::SetPropEntry(const FScenarioStaticObstaclePropEntry& propEntry)
{
	ItemViewModel = nullptr;
	PropEntry = propEntry;
	SetPaletteItemEntry(MakeStaticObstaclePaletteItemEntry(propEntry));
}

void UScenarioPlaceablePaletteItemWidget::SetPaletteItemEntry(const FScenarioPaletteItemEntry& paletteItemEntry)
{
	ItemViewModel = nullptr;
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

void UScenarioPlaceablePaletteItemWidget::InitializeFromItemViewModel(
	UScenarioEditorListItemViewModel* itemViewModel)
{
	ItemViewModel = itemViewModel;

	FScenarioPaletteItemEntry paletteItemEntry;
	if (ItemViewModel)
	{
		paletteItemEntry.ItemType = ItemViewModel->GetPaletteItemType();
		paletteItemEntry.AssetId = ItemViewModel->GetAssetId();
		paletteItemEntry.DisplayName = FText::FromString(ItemViewModel->GetTitle());
		paletteItemEntry.CategoryText = FText::FromString(ItemViewModel->GetSubtitle());
		paletteItemEntry.ThumbnailTexture = ItemViewModel->GetThumbnailTexture();
	}

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

void UScenarioPlaceablePaletteItemWidget::HandleSelectButtonClicked()
{
	if (PaletteItemEntry.AssetId.IsNone()) return;

	OnSelected.Broadcast(PaletteItemEntry.ItemType, PaletteItemEntry.AssetId);
}

FText UScenarioPlaceablePaletteItemWidget::CategoryToText(EScenarioStaticObstaclePropCategory category)
{
	switch (category)
	{
	case EScenarioStaticObstaclePropCategory::StreetFurniture:
		return FText::FromString(TEXT("Street Furniture"));
	case EScenarioStaticObstaclePropCategory::TrafficControl:
		return FText::FromString(TEXT("Traffic Control"));
	case EScenarioStaticObstaclePropCategory::DeliveryItem:
		return FText::FromString(TEXT("Delivery Item"));
	case EScenarioStaticObstaclePropCategory::Utility:
		return FText::FromString(TEXT("Utility"));
	case EScenarioStaticObstaclePropCategory::SurfaceObject:
		return FText::FromString(TEXT("Surface Object"));
	case EScenarioStaticObstaclePropCategory::Unknown:
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

FString UScenarioPlaceablePaletteItemWidget::MakeDisplayNameFromPropId(FName propId)
{
	FString propIdString = propId.ToString();
	const FString obstaclePrefix(TEXT("obstacle."));
	if (propIdString.StartsWith(obstaclePrefix))
	{
		propIdString.RightChopInline(obstaclePrefix.Len());
	}

	return propIdString;
}

FScenarioPaletteItemEntry UScenarioPlaceablePaletteItemWidget::MakeStaticObstaclePaletteItemEntry(
	const FScenarioStaticObstaclePropEntry& propEntry)
{
	FScenarioPaletteItemEntry paletteItemEntry;
	paletteItemEntry.ItemType = EScenarioPaletteItemType::StaticObstacle;
	paletteItemEntry.AssetId = propEntry.PropId;
	paletteItemEntry.DisplayName = propEntry.DisplayName.IsEmpty()
		? FText::FromString(MakeDisplayNameFromPropId(propEntry.PropId))
		: propEntry.DisplayName;
	paletteItemEntry.CategoryText = CategoryToText(propEntry.Category);
	paletteItemEntry.ThumbnailTexture = propEntry.ThumbnailTexture;
	return paletteItemEntry;
}

void UScenarioPlaceablePaletteItemWidget::ApplyThumbnailImage()
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
