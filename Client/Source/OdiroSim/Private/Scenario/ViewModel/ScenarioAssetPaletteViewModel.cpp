#include "Scenario/ViewModel/ScenarioAssetPaletteViewModel.h"

#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"

void UScenarioAssetPaletteViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
}

void UScenarioAssetPaletteViewModel::SetPaletteEntries(const TArray<FScenarioPaletteItemEntry>& entries)
{
	Items.Reset();
	for (const FScenarioPaletteItemEntry& entry : entries)
	{
		UScenarioEditorListItemViewModel* item = NewObject<UScenarioEditorListItemViewModel>(this);
		if (!item)
		{
			continue;
		}

		item->InitializePaletteItem(
			entry.AssetId,
			entry.DisplayName.ToString(),
			entry.CategoryText.ToString(),
			entry.ItemType);
		item->SetThumbnailTexture(entry.ThumbnailTexture);
		item->SetSelected(entry.AssetId == SelectedAssetId);
		Items.Add(item);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);
}

void UScenarioAssetPaletteViewModel::SelectAsset(const FName assetId)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedAssetId, assetId);
	for (UScenarioEditorListItemViewModel* item : Items)
	{
		if (item)
		{
			item->SetSelected(item->GetAssetId() == assetId);
		}
	}
}

TArray<UScenarioEditorListItemViewModel*> UScenarioAssetPaletteViewModel::GetItems() const
{
	TArray<UScenarioEditorListItemViewModel*> result;
	result.Reserve(Items.Num());
	for (UScenarioEditorListItemViewModel* item : Items)
	{
		result.Add(item);
	}
	return result;
}

void UScenarioAssetPaletteViewModel::RequestEditorWidgetInputMode(UWidget* requestingWidget)
{
	if (UiSubsystem)
	{
		UiSubsystem->RequestEditorWidgetInputMode(requestingWidget);
	}
}

void UScenarioAssetPaletteViewModel::ReleaseEditorWidgetInputMode(UWidget* requestingWidget)
{
	if (UiSubsystem)
	{
		UiSubsystem->ReleaseEditorWidgetInputMode(requestingWidget);
	}
}

void UScenarioAssetPaletteViewModel::GetStaticObstaclePaletteEntries(
	TArray<FScenarioStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();
	if (UiSubsystem)
	{
		UiSubsystem->GetStaticObstaclePaletteEntries(outEntries);
	}
}

bool UScenarioAssetPaletteViewModel::BeginPalettePlacement(
	const EScenarioPaletteItemType itemType,
	const FName assetId)
{
	SelectAsset(assetId);
	return UiSubsystem && UiSubsystem->BeginPalettePlacement(itemType, assetId);
}

bool UScenarioAssetPaletteViewModel::BeginGroundRegionDraw(
	const EScenarioGroundRegionType regionType)
{
	return UiSubsystem && UiSubsystem->BeginGroundRegionDraw(regionType);
}
