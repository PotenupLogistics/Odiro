#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Episode/Editor/EpisodeEditorTypes.h"
#include "EpisodeAssetPaletteWidget.generated.h"

class UHorizontalBox;
class UScrollBox;
class USizeBox;
class UWidget;
class AEpisodeEditorController;
class UEpisodeAssetPaletteCatalog;
class UEpisodePlaceablePaletteItemWidget;

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UEpisodeAssetPaletteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit UEpisodeAssetPaletteWidget(const FObjectInitializer& objectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	TSubclassOf<UEpisodePlaceablePaletteItemWidget> PlaceableItemWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	TSoftObjectPtr<UEpisodeAssetPaletteCatalog> AssetPaletteCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bRebuildOnConstruct = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bIncludePedestrianPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bIncludeRobotRoutePlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bIncludeGroundRegionDraw = true;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<USizeBox> PaletteSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UScrollBox> PaletteScrollBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UHorizontalBox> PlaceableItemContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UHorizontalBox> StaticObstacleItemContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UHorizontalBox> GroundRegionItemContainer;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	bool RebuildPalette();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void ClearPalette();

protected:
	UFUNCTION()
	void HandlePaletteItemSelected(EEpisodePaletteItemType itemType, FName assetId);

	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();

private:
	const UEpisodeAssetPaletteCatalog* GetPaletteCatalog() const;
	UHorizontalBox* ResolveStaticObstacleItemContainer() const;
	UHorizontalBox* ResolveGroundRegionItemContainer() const;
	UEpisodePlaceablePaletteItemWidget* CreatePaletteItemWidget(AEpisodeEditorController* editorController) const;
	void BindPaletteItemWidget(UEpisodePlaceablePaletteItemWidget* itemWidget);
	bool AddPaletteItemWidget(
		AEpisodeEditorController* editorController,
		UHorizontalBox* targetContainer,
		const FEpisodePaletteItemEntry& paletteItemEntry);
	int32 AddDefaultGroundRegionPaletteEntries(AEpisodeEditorController* editorController);
	static bool ShouldIncludeSpecialEntry(
		const FEpisodePaletteItemEntry& entry,
		bool bIncludePedestrian,
		bool bIncludeRobotRoute);

	UWidget* ResolveInputModeFocusWidget() const;

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
