#include "Episode/Widget/EpisodeAssetPaletteWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Episode/Data/EpisodeAssetPaletteCatalog.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/Widget/EpisodePlaceablePaletteItemWidget.h"
#include "Shared/EpisodeCoreTypes.h"
#include "Shared/EpisodeSpecTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeAssetPaletteWidget, Log, All);

namespace
{
	const FName GroundRegionWalkableAssetId(TEXT("ground.walkable"));
	const FName GroundRegionPenaltyAssetId(TEXT("ground.penalty"));
	const FName GroundRegionBlockedAssetId(TEXT("ground.blocked"));
	const TCHAR* GroundRegionWalkableThumbnailPath = TEXT("/Game/Widgets/Thumbnail/icon_walkable.icon_walkable");
	const TCHAR* GroundRegionPenaltyThumbnailPath = TEXT("/Game/Widgets/Thumbnail/icon_panalty.icon_panalty");
	const TCHAR* GroundRegionBlockedThumbnailPath = TEXT("/Game/Widgets/Thumbnail/icon_block.icon_block");

	bool TryResolveGroundRegionType(FName assetId, EEpisodeGroundRegionType& outRegionType)
	{
		const FString normalizedId = assetId.ToString().ToLower();
		if (normalizedId == TEXT("ground.walkable") || normalizedId == TEXT("walkable"))
		{
			outRegionType = EEpisodeGroundRegionType::Walkable;
			return true;
		}

		if (normalizedId == TEXT("ground.penalty") || normalizedId == TEXT("penalty"))
		{
			outRegionType = EEpisodeGroundRegionType::Penalty;
			return true;
		}

		if (normalizedId == TEXT("ground.blocked") || normalizedId == TEXT("blocked") || normalizedId == TEXT("block"))
		{
			outRegionType = EEpisodeGroundRegionType::Blocked;
			return true;
		}

		return false;
	}

	const TCHAR* ResolveGroundRegionThumbnailPath(FName assetId)
	{
		EEpisodeGroundRegionType regionType = EEpisodeGroundRegionType::Walkable;
		if (!TryResolveGroundRegionType(assetId, regionType))
		{
			return nullptr;
		}

		switch (regionType)
		{
		case EEpisodeGroundRegionType::Walkable:
			return GroundRegionWalkableThumbnailPath;
		case EEpisodeGroundRegionType::Penalty:
			return GroundRegionPenaltyThumbnailPath;
		case EEpisodeGroundRegionType::Blocked:
			return GroundRegionBlockedThumbnailPath;
		default:
			return nullptr;
		}
	}

	void SeedGroundRegionThumbnail(FEpisodePaletteItemEntry& entry)
	{
		if (!entry.ThumbnailTexture.IsNull())
		{
			return;
		}

		if (const TCHAR* thumbnailPath = ResolveGroundRegionThumbnailPath(entry.AssetId))
		{
			entry.ThumbnailTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(thumbnailPath));
		}
	}

	FEpisodePaletteItemEntry MakeGroundRegionPaletteItemEntry(
		FName assetId,
		const TCHAR* displayName)
	{
		FEpisodePaletteItemEntry entry;
		entry.ItemType = EEpisodePaletteItemType::GroundRegion;
		entry.AssetId = assetId;
		entry.DisplayName = FText::FromString(displayName);
		entry.CategoryText = FText::FromString(TEXT("Ground Region"));
		entry.IconName = assetId.ToString();
		SeedGroundRegionThumbnail(entry);
		return entry;
	}
}

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

	UHorizontalBox* staticObstacleContainer = ResolveStaticObstacleItemContainer();
	UHorizontalBox* groundRegionContainer = ResolveGroundRegionItemContainer();
	if (!staticObstacleContainer && !groundRegionContainer)
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("No palette item container is bound."));
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
	if (staticObstacleContainer)
	{
		for (const FEpisodeStaticObstaclePropEntry& paletteEntry : paletteEntries)
		{
			UEpisodePlaceablePaletteItemWidget* itemWidget = CreatePaletteItemWidget(editorController);
			if (!itemWidget) continue;

			itemWidget->SetPropEntry(paletteEntry);
			BindPaletteItemWidget(itemWidget);
			staticObstacleContainer->AddChildToHorizontalBox(itemWidget);
			++itemCount;
		}
	}
	else if (!paletteEntries.IsEmpty())
	{
		UE_LOG(LogEpisodeAssetPaletteWidget, Warning, TEXT("Static obstacle palette container is not bound."));
	}

	int32 groundRegionItemCount = 0;
	const UEpisodeAssetPaletteCatalog* paletteCatalog = GetPaletteCatalog();
	if (paletteCatalog)
	{
		for (const FEpisodePaletteItemEntry& specialEntry : paletteCatalog->SpecialEntries)
		{
			if (specialEntry.ItemType == EEpisodePaletteItemType::GroundRegion)
			{
				if (!bIncludeGroundRegionDraw || !groundRegionContainer)
				{
					continue;
				}

				EEpisodeGroundRegionType unusedRegionType = EEpisodeGroundRegionType::Walkable;
				if (!TryResolveGroundRegionType(specialEntry.AssetId, unusedRegionType))
				{
					UE_LOG(
						LogEpisodeAssetPaletteWidget,
						Warning,
						TEXT("Skipping ground region palette entry with unsupported asset id: %s"),
						*specialEntry.AssetId.ToString());
					continue;
				}

				FEpisodePaletteItemEntry groundRegionEntry = specialEntry;
				SeedGroundRegionThumbnail(groundRegionEntry);
				if (AddPaletteItemWidget(editorController, groundRegionContainer, groundRegionEntry))
				{
					++itemCount;
					++groundRegionItemCount;
				}
				continue;
			}

			if (!ShouldIncludeSpecialEntry(specialEntry, bIncludePedestrianPlacement, bIncludeRobotRoutePlacement))
			{
				continue;
			}

			if (AddPaletteItemWidget(editorController, staticObstacleContainer, specialEntry))
			{
				++itemCount;
			}
		}
	}

	if (bIncludeGroundRegionDraw && groundRegionContainer && groundRegionItemCount == 0)
	{
		const int32 defaultGroundRegionCount = AddDefaultGroundRegionPaletteEntries(editorController);
		itemCount += defaultGroundRegionCount;
		groundRegionItemCount += defaultGroundRegionCount;
	}

	UE_LOG(
		LogEpisodeAssetPaletteWidget,
		Log,
		TEXT("Loaded %d palette assets | GroundRegions: %d"),
		itemCount,
		groundRegionItemCount);
	return true;
}

void UEpisodeAssetPaletteWidget::ClearPalette()
{
	if (PlaceableItemContainer)
	{
		PlaceableItemContainer->ClearChildren();
	}

	if (StaticObstacleItemContainer && StaticObstacleItemContainer != PlaceableItemContainer)
	{
		StaticObstacleItemContainer->ClearChildren();
	}

	if (GroundRegionItemContainer
		&& GroundRegionItemContainer != PlaceableItemContainer
		&& GroundRegionItemContainer != StaticObstacleItemContainer)
	{
		GroundRegionItemContainer->ClearChildren();
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

	if (itemType == EEpisodePaletteItemType::GroundRegion)
	{
		EEpisodeGroundRegionType regionType = EEpisodeGroundRegionType::Walkable;
		if (!TryResolveGroundRegionType(assetId, regionType))
		{
			UE_LOG(
				LogEpisodeAssetPaletteWidget,
				Warning,
				TEXT("Unsupported ground region palette asset id: %s"),
				*assetId.ToString());
			return;
		}

		if (!editorController->BeginGroundRegionDraw(regionType))
		{
			UE_LOG(
				LogEpisodeAssetPaletteWidget,
				Warning,
				TEXT("Failed to begin ground region draw | AssetId: %s"),
				*assetId.ToString());
			return;
		}

		UE_LOG(
			LogEpisodeAssetPaletteWidget,
			Log,
			TEXT("Ground region draw selected | AssetId: %s"),
			*assetId.ToString());
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

UHorizontalBox* UEpisodeAssetPaletteWidget::ResolveStaticObstacleItemContainer() const
{
	return StaticObstacleItemContainer ? StaticObstacleItemContainer.Get() : PlaceableItemContainer.Get();
}

UHorizontalBox* UEpisodeAssetPaletteWidget::ResolveGroundRegionItemContainer() const
{
	return GroundRegionItemContainer ? GroundRegionItemContainer.Get() : PlaceableItemContainer.Get();
}

UEpisodePlaceablePaletteItemWidget* UEpisodeAssetPaletteWidget::CreatePaletteItemWidget(
	AEpisodeEditorController* editorController) const
{
	if (!editorController || !PlaceableItemWidgetClass)
	{
		return nullptr;
	}

	return CreateWidget<UEpisodePlaceablePaletteItemWidget>(
		editorController,
		PlaceableItemWidgetClass);
}

void UEpisodeAssetPaletteWidget::BindPaletteItemWidget(UEpisodePlaceablePaletteItemWidget* itemWidget)
{
	if (!itemWidget)
	{
		return;
	}

	itemWidget->OnSelected.RemoveDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
	itemWidget->OnSelected.AddDynamic(this, &UEpisodeAssetPaletteWidget::HandlePaletteItemSelected);
}

bool UEpisodeAssetPaletteWidget::AddPaletteItemWidget(
	AEpisodeEditorController* editorController,
	UHorizontalBox* targetContainer,
	const FEpisodePaletteItemEntry& paletteItemEntry)
{
	if (!targetContainer)
	{
		return false;
	}

	UEpisodePlaceablePaletteItemWidget* itemWidget = CreatePaletteItemWidget(editorController);
	if (!itemWidget)
	{
		return false;
	}

	itemWidget->SetPaletteItemEntry(paletteItemEntry);
	BindPaletteItemWidget(itemWidget);
	targetContainer->AddChildToHorizontalBox(itemWidget);
	return true;
}

int32 UEpisodeAssetPaletteWidget::AddDefaultGroundRegionPaletteEntries(AEpisodeEditorController* editorController)
{
	UHorizontalBox* groundRegionContainer = ResolveGroundRegionItemContainer();
	if (!groundRegionContainer)
	{
		return 0;
	}

	int32 itemCount = 0;
	if (AddPaletteItemWidget(
			editorController,
			groundRegionContainer,
			MakeGroundRegionPaletteItemEntry(GroundRegionWalkableAssetId, TEXT("Walkable"))))
	{
		++itemCount;
	}

	if (AddPaletteItemWidget(
			editorController,
			groundRegionContainer,
			MakeGroundRegionPaletteItemEntry(GroundRegionPenaltyAssetId, TEXT("Penalty"))))
	{
		++itemCount;
	}

	if (AddPaletteItemWidget(
			editorController,
			groundRegionContainer,
			MakeGroundRegionPaletteItemEntry(GroundRegionBlockedAssetId, TEXT("Blocked"))))
	{
		++itemCount;
	}

	return itemCount;
}

bool UEpisodeAssetPaletteWidget::ShouldIncludeSpecialEntry(
	const FEpisodePaletteItemEntry& entry,
	bool bIncludePedestrian,
	bool bIncludeRobotRoute)
{
	(void)bIncludeRobotRoute;
	switch (entry.ItemType)
	{
	case EEpisodePaletteItemType::Pedestrian:
		return bIncludePedestrian;
	case EEpisodePaletteItemType::RobotStart:
	case EEpisodePaletteItemType::RobotGoal:
		return false;
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
