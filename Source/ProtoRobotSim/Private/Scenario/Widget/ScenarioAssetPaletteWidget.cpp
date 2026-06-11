#include "Scenario/Widget/ScenarioAssetPaletteWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Scenario/Data/ScenarioAssetPaletteCatalog.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Widget/ScenarioPlaceablePaletteItemWidget.h"
#include "Shared/ScenarioCoreTypes.h"
#include "Shared/ScenarioSpecTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioAssetPaletteWidget, Log, All);

namespace
{
	const FName GroundRegionWalkableAssetId(TEXT("ground.walkable"));
	const FName GroundRegionPenaltyAssetId(TEXT("ground.penalty"));
	const FName GroundRegionBlockedAssetId(TEXT("ground.blocked"));
	const TCHAR* GroundRegionWalkableThumbnailPath = TEXT("/Game/Widgets/Thumbnail/icon_walkable.icon_walkable");
	const TCHAR* GroundRegionPenaltyThumbnailPath = TEXT("/Game/Widgets/Thumbnail/icon_panalty.icon_panalty");
	const TCHAR* GroundRegionBlockedThumbnailPath = TEXT("/Game/Widgets/Thumbnail/icon_block.icon_block");

	bool TryResolveGroundRegionType(FName assetId, EScenarioGroundRegionType& outRegionType)
	{
		const FString normalizedId = assetId.ToString().ToLower();
		if (normalizedId == TEXT("ground.walkable") || normalizedId == TEXT("walkable"))
		{
			outRegionType = EScenarioGroundRegionType::Walkable;
			return true;
		}

		if (normalizedId == TEXT("ground.penalty") || normalizedId == TEXT("penalty"))
		{
			outRegionType = EScenarioGroundRegionType::Penalty;
			return true;
		}

		if (normalizedId == TEXT("ground.blocked") || normalizedId == TEXT("blocked") || normalizedId == TEXT("block"))
		{
			outRegionType = EScenarioGroundRegionType::Blocked;
			return true;
		}

		return false;
	}

	const TCHAR* ResolveGroundRegionThumbnailPath(FName assetId)
	{
		EScenarioGroundRegionType regionType = EScenarioGroundRegionType::Walkable;
		if (!TryResolveGroundRegionType(assetId, regionType))
		{
			return nullptr;
		}

		switch (regionType)
		{
		case EScenarioGroundRegionType::Walkable:
			return GroundRegionWalkableThumbnailPath;
		case EScenarioGroundRegionType::Penalty:
			return GroundRegionPenaltyThumbnailPath;
		case EScenarioGroundRegionType::Blocked:
			return GroundRegionBlockedThumbnailPath;
		default:
			return nullptr;
		}
	}

	void SeedGroundRegionThumbnail(FScenarioPaletteItemEntry& entry)
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

	FScenarioPaletteItemEntry MakeGroundRegionPaletteItemEntry(
		FName assetId,
		const TCHAR* displayName)
	{
		FScenarioPaletteItemEntry entry;
		entry.ItemType = EScenarioPaletteItemType::GroundRegion;
		entry.AssetId = assetId;
		entry.DisplayName = FText::FromString(displayName);
		entry.CategoryText = FText::FromString(TEXT("Ground Region"));
		entry.IconName = assetId.ToString();
		SeedGroundRegionThumbnail(entry);
		return entry;
	}
}

UScenarioAssetPaletteWidget::UScenarioAssetPaletteWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	AssetPaletteCatalog = UScenarioAssetPaletteCatalog::MakeDefaultCatalogReference();
}

void UScenarioAssetPaletteWidget::NativeConstruct()
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

void UScenarioAssetPaletteWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

bool UScenarioAssetPaletteWidget::RebuildPalette()
{
	ClearPalette();

	UHorizontalBox* staticObstacleContainer = ResolveStaticObstacleItemContainer();
	UHorizontalBox* groundRegionContainer = ResolveGroundRegionItemContainer();
	if (!staticObstacleContainer && !groundRegionContainer)
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("No palette item container is bound."));
		return false;
	}

	if (!PlaceableItemWidgetClass)
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("PlaceableItemWidgetClass is not set."));
		return false;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return false;
	}

	TArray<FScenarioStaticObstaclePropEntry> paletteEntries;
	editorController->GetStaticObstaclePaletteEntries(paletteEntries);
	int32 itemCount = 0;
	if (staticObstacleContainer)
	{
		for (const FScenarioStaticObstaclePropEntry& paletteEntry : paletteEntries)
		{
			UScenarioPlaceablePaletteItemWidget* itemWidget = CreatePaletteItemWidget(editorController);
			if (!itemWidget) continue;

			itemWidget->SetPropEntry(paletteEntry);
			BindPaletteItemWidget(itemWidget);
			staticObstacleContainer->AddChildToHorizontalBox(itemWidget);
			++itemCount;
		}
	}
	else if (!paletteEntries.IsEmpty())
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("Static obstacle palette container is not bound."));
	}

	int32 groundRegionItemCount = 0;
	const UScenarioAssetPaletteCatalog* paletteCatalog = GetPaletteCatalog();
	if (paletteCatalog)
	{
		for (const FScenarioPaletteItemEntry& specialEntry : paletteCatalog->SpecialEntries)
		{
			if (specialEntry.ItemType == EScenarioPaletteItemType::GroundRegion)
			{
				if (!bIncludeGroundRegionDraw || !groundRegionContainer)
				{
					continue;
				}

				EScenarioGroundRegionType unusedRegionType = EScenarioGroundRegionType::Walkable;
				if (!TryResolveGroundRegionType(specialEntry.AssetId, unusedRegionType))
				{
					UE_LOG(
						LogScenarioAssetPaletteWidget,
						Warning,
						TEXT("Skipping ground region palette entry with unsupported asset id: %s"),
						*specialEntry.AssetId.ToString());
					continue;
				}

				FScenarioPaletteItemEntry groundRegionEntry = specialEntry;
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
		LogScenarioAssetPaletteWidget,
		Log,
		TEXT("Loaded %d palette assets | GroundRegions: %d"),
		itemCount,
		groundRegionItemCount);
	return true;
}

void UScenarioAssetPaletteWidget::ClearPalette()
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

void UScenarioAssetPaletteWidget::HandlePaletteItemSelected(EScenarioPaletteItemType itemType, FName assetId)
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return;
	}

	if (itemType == EScenarioPaletteItemType::GroundRegion)
	{
		EScenarioGroundRegionType regionType = EScenarioGroundRegionType::Walkable;
		if (!TryResolveGroundRegionType(assetId, regionType))
		{
			UE_LOG(
				LogScenarioAssetPaletteWidget,
				Warning,
				TEXT("Unsupported ground region palette asset id: %s"),
				*assetId.ToString());
			return;
		}

		if (!editorController->BeginGroundRegionDraw(regionType))
		{
			UE_LOG(
				LogScenarioAssetPaletteWidget,
				Warning,
				TEXT("Failed to begin ground region draw | AssetId: %s"),
				*assetId.ToString());
			return;
		}

		UE_LOG(
			LogScenarioAssetPaletteWidget,
			Log,
			TEXT("Ground region draw selected | AssetId: %s"),
			*assetId.ToString());
		return;
	}

	if (!editorController->BeginPalettePlacement(itemType, assetId))
	{
		UE_LOG(
			LogScenarioAssetPaletteWidget,
			Warning,
			TEXT("Failed to begin placement | Type: %d | AssetId: %s"),
			static_cast<int32>(itemType),
			*assetId.ToString());
		return;
	}

	UE_LOG(
		LogScenarioAssetPaletteWidget,
		Log,
		TEXT("Placement selected | Type: %d | AssetId: %s"),
		static_cast<int32>(itemType),
		*assetId.ToString());
}

const UScenarioAssetPaletteCatalog* UScenarioAssetPaletteWidget::GetPaletteCatalog() const
{
	if (AssetPaletteCatalog.IsNull())
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("Episode asset palette catalog is not configured."));
		return nullptr;
	}

	UScenarioAssetPaletteCatalog* catalog = AssetPaletteCatalog.LoadSynchronous();
	if (!IsValid(catalog))
	{
		UE_LOG(
			LogScenarioAssetPaletteWidget,
			Warning,
			TEXT("Failed to load episode asset palette catalog: %s"),
			*AssetPaletteCatalog.ToSoftObjectPath().ToString());
		return nullptr;
	}

	return catalog;
}

UHorizontalBox* UScenarioAssetPaletteWidget::ResolveStaticObstacleItemContainer() const
{
	return StaticObstacleItemContainer ? StaticObstacleItemContainer.Get() : PlaceableItemContainer.Get();
}

UHorizontalBox* UScenarioAssetPaletteWidget::ResolveGroundRegionItemContainer() const
{
	return GroundRegionItemContainer ? GroundRegionItemContainer.Get() : PlaceableItemContainer.Get();
}

UScenarioPlaceablePaletteItemWidget* UScenarioAssetPaletteWidget::CreatePaletteItemWidget(
	AScenarioEditorController* editorController) const
{
	if (!editorController || !PlaceableItemWidgetClass)
	{
		return nullptr;
	}

	return CreateWidget<UScenarioPlaceablePaletteItemWidget>(
		editorController,
		PlaceableItemWidgetClass);
}

void UScenarioAssetPaletteWidget::BindPaletteItemWidget(UScenarioPlaceablePaletteItemWidget* itemWidget)
{
	if (!itemWidget)
	{
		return;
	}

	itemWidget->OnSelected.RemoveDynamic(this, &UScenarioAssetPaletteWidget::HandlePaletteItemSelected);
	itemWidget->OnSelected.AddDynamic(this, &UScenarioAssetPaletteWidget::HandlePaletteItemSelected);
}

bool UScenarioAssetPaletteWidget::AddPaletteItemWidget(
	AScenarioEditorController* editorController,
	UHorizontalBox* targetContainer,
	const FScenarioPaletteItemEntry& paletteItemEntry)
{
	if (!targetContainer)
	{
		return false;
	}

	UScenarioPlaceablePaletteItemWidget* itemWidget = CreatePaletteItemWidget(editorController);
	if (!itemWidget)
	{
		return false;
	}

	itemWidget->SetPaletteItemEntry(paletteItemEntry);
	BindPaletteItemWidget(itemWidget);
	targetContainer->AddChildToHorizontalBox(itemWidget);
	return true;
}

int32 UScenarioAssetPaletteWidget::AddDefaultGroundRegionPaletteEntries(AScenarioEditorController* editorController)
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

bool UScenarioAssetPaletteWidget::ShouldIncludeSpecialEntry(
	const FScenarioPaletteItemEntry& entry,
	bool bIncludePedestrian,
	bool bIncludeRobotRoute)
{
	(void)bIncludeRobotRoute;
	switch (entry.ItemType)
	{
	case EScenarioPaletteItemType::Pedestrian:
		return bIncludePedestrian;
	case EScenarioPaletteItemType::RobotStart:
	case EScenarioPaletteItemType::RobotGoal:
		return false;
	case EScenarioPaletteItemType::StaticObstacle:
	default:
		return false;
	}
}

void UScenarioAssetPaletteWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioAssetPaletteWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
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

UWidget* UScenarioAssetPaletteWidget::ResolveInputModeFocusWidget() const
{
	if (PaletteSizeBox)
	{
		return PaletteSizeBox.Get();
	}

	return const_cast<UScenarioAssetPaletteWidget*>(this);
}
