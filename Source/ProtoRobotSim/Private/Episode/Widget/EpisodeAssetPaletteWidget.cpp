#include "Episode/Widget/EpisodeAssetPaletteWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Episode/Data/EpisodeAssetPaletteCatalog.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/Widget/EpisodePlaceablePaletteItemWidget.h"
#include "Shared/EpisodeCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeAssetPaletteWidget, Log, All);

UEpisodeAssetPaletteWidget::UEpisodeAssetPaletteWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	AssetPaletteCatalog = UEpisodeAssetPaletteCatalog::MakeDefaultCatalogReference();
}

void UEpisodeAssetPaletteWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();

	if (PaletteScrollBox)
	{
		PaletteScrollBox->SetOrientation(Orient_Horizontal);
	}

	if (bRebuildOnConstruct)
	{
		RebuildPalette();
	}
}

void UEpisodeAssetPaletteWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

bool UEpisodeAssetPaletteWidget::RebuildPalette()
{
	ClearPalette();

	if (!PlaceableItemContainer)
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("PlaceableItemContainer is not bound."));
		return false;
	}

	if (!PlaceableItemWidgetClass)
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("PlaceableItemWidgetClass is not set."));
		return false;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return false;
	}

	TArray<FEpisodeStaticObstaclePropEntry> paletteEntries;
	editorController->GetStaticObstaclePaletteEntries(paletteEntries);
	int32 itemCount = 0;
	for (const FEpisodeStaticObstaclePropEntry& paletteEntry : paletteEntries)
	{
		UEpisodePlaceablePaletteItemWidget* itemWidget = CreateWidget<UEpisodePlaceablePaletteItemWidget>(
			editorController,
			PlaceableItemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->SetPropEntry(paletteEntry);
		itemWidget->OnSelected.RemoveDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
		itemWidget->OnSelected.AddDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
		PlaceableItemContainer->AddChildToHorizontalBox(itemWidget);
		++itemCount;
	}

	const UEpisodeAssetPaletteCatalog* paletteCatalog = GetPaletteCatalog();
	if (paletteCatalog)
	{
		for (const FEpisodePaletteItemEntry& specialEntry : paletteCatalog->SpecialEntries)
		{
			if (!ShouldIncludeSpecialEntry(specialEntry, bIncludePedestrianPlacement, bIncludeRobotRoutePlacement))
			{
				continue;
			}

			UEpisodePlaceablePaletteItemWidget* itemWidget = CreateWidget<UEpisodePlaceablePaletteItemWidget>(
				editorController,
				PlaceableItemWidgetClass);
			if (!itemWidget) continue;

			itemWidget->SetPaletteItemEntry(specialEntry);
			itemWidget->OnSelected.RemoveDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
			itemWidget->OnSelected.AddDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
			PlaceableItemContainer->AddChildToHorizontalBox(itemWidget);
			++itemCount;
		}
	}

	UE_LOG(LogEpisodeAssetPaletteWidget, Log, TEXT("Loaded %d placeable assets."), itemCount);
	return true;
}

void UEpisodeAssetPaletteWidget::ClearPalette()
{
	if (PlaceableItemContainer)
	{
		PlaceableItemContainer->ClearChildren();
	}
}

void UEpisodeAssetPaletteWidget::HandlePaletteItemSelected(EEpisodePaletteItemType itemType, FName assetId)
{
	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return;
	}

	if (!editorController->BeginPalettePlacement(itemType, assetId))
	{
		UE_LOG(
			LogEpisodeAssetPaletteWidget,
			Warning,
			TEXT("Failed to begin placement | Type: %d | AssetId: %s"),
			static_cast<int32>(itemType),
			*assetId.ToString());
		return;
	}

	UE_LOG(
		LogEpisodeAssetPaletteWidget,
		Log,
		TEXT("Placement selected | Type: %d | AssetId: %s"),
		static_cast<int32>(itemType),
		*assetId.ToString());
}

const UEpisodeAssetPaletteCatalog* UEpisodeAssetPaletteWidget::GetPaletteCatalog() const
{
	if (AssetPaletteCatalog.IsNull())
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("Episode asset palette catalog is not configured."));
		return nullptr;
	}

	UEpisodeAssetPaletteCatalog* catalog = AssetPaletteCatalog.LoadSynchronous();
	if (!IsValid(catalog))
	{
		UE_LOG(
			LogEpisodeAssetPaletteWidget,
			Warning,
			TEXT("Failed to load episode asset palette catalog: %s"),
			*AssetPaletteCatalog.ToSoftObjectPath().ToString());
		return nullptr;
	}

	return catalog;
}

bool UEpisodeAssetPaletteWidget::ShouldIncludeSpecialEntry(
	const FEpisodePaletteItemEntry& entry,
	bool bIncludePedestrian,
	bool bIncludeRobotRoute)
{
	switch (entry.ItemType)
	{
	case EEpisodePaletteItemType::Pedestrian:
		return bIncludePedestrian;
	case EEpisodePaletteItemType::RobotStart:
	case EEpisodePaletteItemType::RobotGoal:
		return bIncludeRobotRoute;
	case EEpisodePaletteItemType::StaticObstacle:
	default:
		return false;
	}
}

void UEpisodeAssetPaletteWidget::RequestEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UEpisodeAssetPaletteWidget::ReleaseEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}

		editorController->ReleaseEditorWidgetInputMode(focusWidget);
		RequestedInputModeFocusWidget.Reset();
	}
}

UWidget* UEpisodeAssetPaletteWidget::ResolveInputModeFocusWidget() const
{
	if (PaletteSizeBox)
	{
		return PaletteSizeBox.Get();
	}

	return const_cast<UEpisodeAssetPaletteWidget*>(this);
}
