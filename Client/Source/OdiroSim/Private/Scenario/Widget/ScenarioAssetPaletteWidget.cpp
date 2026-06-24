#include "Scenario/Widget/ScenarioAssetPaletteWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioAssetPaletteCatalog.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioAssetPaletteViewModel.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"
#include "Scenario/Widget/ScenarioPlaceablePaletteItemWidget.h"
#include "Shared/ScenarioCoreTypes.h"
#include "Shared/ScenarioSpecTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioAssetPaletteWidget, Log, All);

namespace
{
	const FName GroundRegionWalkableAssetId(TEXT("ground.walkable"));
	const FName GroundRegionPenaltyAssetId(TEXT("ground.penalty"));
	const FName GroundRegionBlockedAssetId(TEXT("ground.blocked"));

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
}

UScenarioAssetPaletteWidget::UScenarioAssetPaletteWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	AssetPaletteCatalog = UScenarioAssetPaletteCatalog::MakeDefaultCatalogReference();
}

void UScenarioAssetPaletteWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeViewModel();
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

	if (!AssetPaletteViewModel)
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("ScenarioAssetPaletteViewModel is not available."));
		return false;
	}

	TArray<FScenarioPaletteItemEntry> paletteItemEntries;
	TArray<FScenarioStaticObstaclePropEntry> paletteEntries;
	AssetPaletteViewModel->GetStaticObstaclePaletteEntries(paletteEntries);
	for (const FScenarioStaticObstaclePropEntry& paletteEntry : paletteEntries)
	{
		paletteItemEntries.Add(UScenarioPlaceablePaletteItemWidget::MakeStaticObstaclePaletteItemEntry(paletteEntry));
	}

	int32 groundRegionEntryCount = 0;
	const UScenarioAssetPaletteCatalog* paletteCatalog = GetPaletteCatalog();
	if (paletteCatalog)
	{
		for (const FScenarioPaletteItemEntry& specialEntry : paletteCatalog->SpecialEntries)
		{
			if (specialEntry.ItemType == EScenarioPaletteItemType::GroundRegion)
			{
				if (!bIncludeGroundRegionDraw)
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
				paletteItemEntries.Add(groundRegionEntry);
				++groundRegionEntryCount;
				continue;
			}

			if (!ShouldIncludeSpecialEntry(specialEntry, bIncludePedestrianPlacement, bIncludeRobotRoutePlacement))
			{
				continue;
			}

			paletteItemEntries.Add(specialEntry);
		}
	}

	if (bIncludeGroundRegionDraw && groundRegionEntryCount == 0)
	{
		paletteItemEntries.Add(MakeGroundRegionPaletteItemEntry(GroundRegionWalkableAssetId, TEXT("Walkable")));
		paletteItemEntries.Add(MakeGroundRegionPaletteItemEntry(GroundRegionPenaltyAssetId, TEXT("Penalty")));
		paletteItemEntries.Add(MakeGroundRegionPaletteItemEntry(GroundRegionBlockedAssetId, TEXT("Blocked")));
		groundRegionEntryCount += 3;
	}

	AssetPaletteViewModel->SetPaletteEntries(paletteItemEntries);

	int32 itemCount = 0;
	int32 groundRegionItemCount = 0;
	for (UScenarioEditorListItemViewModel* itemViewModel : AssetPaletteViewModel->GetItems())
	{
		if (!itemViewModel)
		{
			continue;
		}

		const bool bGroundRegion = itemViewModel->GetPaletteItemType() == EScenarioPaletteItemType::GroundRegion;
		UHorizontalBox* targetContainer = bGroundRegion ? groundRegionContainer : staticObstacleContainer;
		if (!targetContainer)
		{
			UE_LOG(
				LogScenarioAssetPaletteWidget,
				Warning,
				TEXT("Skipping palette item without target container | AssetId: %s"),
				*itemViewModel->GetAssetId().ToString());
			continue;
		}

		if (AddPaletteItemWidget(targetContainer, itemViewModel))
		{
			++itemCount;
			if (bGroundRegion)
			{
				++groundRegionItemCount;
			}
		}
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
	if (!AssetPaletteViewModel)
	{
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("ScenarioAssetPaletteViewModel is not available."));
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

		if (!AssetPaletteViewModel->BeginGroundRegionDraw(regionType))
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

	if (!AssetPaletteViewModel->BeginPalettePlacement(itemType, assetId))
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
		UE_LOG(LogScenarioAssetPaletteWidget, Warning, TEXT("Scenario asset palette catalog is not configured."));
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

UScenarioPlaceablePaletteItemWidget* UScenarioAssetPaletteWidget::CreatePaletteItemWidget() const
{
	if (!PlaceableItemWidgetClass)
	{
		return nullptr;
	}

	return CreateWidget<UScenarioPlaceablePaletteItemWidget>(
		GetOwningPlayer(),
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
	UHorizontalBox* targetContainer,
	const FScenarioPaletteItemEntry& paletteItemEntry)
{
	if (!targetContainer)
	{
		return false;
	}

	UScenarioPlaceablePaletteItemWidget* itemWidget = CreatePaletteItemWidget();
	if (!itemWidget)
	{
		return false;
	}

	itemWidget->SetPaletteItemEntry(paletteItemEntry);
	BindPaletteItemWidget(itemWidget);
	targetContainer->AddChildToHorizontalBox(itemWidget);
	return true;
}

bool UScenarioAssetPaletteWidget::AddPaletteItemWidget(
	UHorizontalBox* targetContainer,
	UScenarioEditorListItemViewModel* itemViewModel)
{
	if (!targetContainer || !itemViewModel)
	{
		return false;
	}

	UScenarioPlaceablePaletteItemWidget* itemWidget = CreatePaletteItemWidget();
	if (!itemWidget)
	{
		return false;
	}

	itemWidget->InitializeFromItemViewModel(itemViewModel);
	BindPaletteItemWidget(itemWidget);
	targetContainer->AddChildToHorizontalBox(itemWidget);
	return true;
}

int32 UScenarioAssetPaletteWidget::AddDefaultGroundRegionPaletteEntries()
{
	UHorizontalBox* groundRegionContainer = ResolveGroundRegionItemContainer();
	if (!groundRegionContainer)
	{
		return 0;
	}

	int32 itemCount = 0;
	if (AddPaletteItemWidget(
			groundRegionContainer,
			MakeGroundRegionPaletteItemEntry(GroundRegionWalkableAssetId, TEXT("Walkable"))))
	{
		++itemCount;
	}

	if (AddPaletteItemWidget(
			groundRegionContainer,
			MakeGroundRegionPaletteItemEntry(GroundRegionPenaltyAssetId, TEXT("Penalty"))))
	{
		++itemCount;
	}

	if (AddPaletteItemWidget(
			groundRegionContainer,
			MakeGroundRegionPaletteItemEntry(GroundRegionBlockedAssetId, TEXT("Blocked"))))
	{
		++itemCount;
	}

	return itemCount;
}

void UScenarioAssetPaletteWidget::SeedGroundRegionThumbnail(FScenarioPaletteItemEntry& entry) const
{
	if (!entry.ThumbnailTexture.IsNull())
	{
		return;
	}

	entry.ThumbnailTexture = ResolveGroundRegionThumbnail(entry.AssetId);
}

TSoftObjectPtr<UTexture2D> UScenarioAssetPaletteWidget::ResolveGroundRegionThumbnail(const FName assetId) const
{
	EScenarioGroundRegionType regionType = EScenarioGroundRegionType::Walkable;
	if (!TryResolveGroundRegionType(assetId, regionType))
	{
		return nullptr;
	}

	switch (regionType)
	{
	case EScenarioGroundRegionType::Walkable:
		return WalkableGroundRegionThumbnail;
	case EScenarioGroundRegionType::Penalty:
		return PenaltyGroundRegionThumbnail;
	case EScenarioGroundRegionType::Blocked:
		return BlockedGroundRegionThumbnail;
	default:
		return nullptr;
	}
}

FScenarioPaletteItemEntry UScenarioAssetPaletteWidget::MakeGroundRegionPaletteItemEntry(
	const FName assetId,
	const TCHAR* displayName) const
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
	if (AssetPaletteViewModel)
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		AssetPaletteViewModel->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioAssetPaletteWidget::ReleaseEditorWidgetInputMode()
{
	if (AssetPaletteViewModel)
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}

		AssetPaletteViewModel->ReleaseEditorWidgetInputMode(focusWidget);
		RequestedInputModeFocusWidget.Reset();
	}
}

void UScenarioAssetPaletteWidget::InitializeViewModel()
{
	if (AssetPaletteViewModel)
	{
		return;
	}

	if (UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this))
	{
		AssetPaletteViewModel = uiSubsystem->GetAssetPaletteViewModel();
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
