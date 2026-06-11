#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioAssetPaletteWidget.generated.h"

class UHorizontalBox;
class UScrollBox;
class USizeBox;
class UWidget;
class AScenarioEditorController;
class UScenarioAssetPaletteCatalog;
class UScenarioPlaceablePaletteItemWidget;

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UScenarioAssetPaletteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit UScenarioAssetPaletteWidget(const FObjectInitializer& objectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	TSubclassOf<UScenarioPlaceablePaletteItemWidget> PlaceableItemWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	TSoftObjectPtr<UScenarioAssetPaletteCatalog> AssetPaletteCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bRebuildOnConstruct = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bIncludePedestrianPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bIncludeRobotRoutePlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bIncludeGroundRegionDraw = true;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<USizeBox> PaletteSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UScrollBox> PaletteScrollBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UHorizontalBox> PlaceableItemContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UHorizontalBox> StaticObstacleItemContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UHorizontalBox> GroundRegionItemContainer;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	bool RebuildPalette();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	void ClearPalette();

protected:
	UFUNCTION()
	void HandlePaletteItemSelected(EScenarioPaletteItemType itemType, FName assetId);

	void RequestEditorWidgetInputMode();
	void ReleaseEditorWidgetInputMode();

private:
	const UScenarioAssetPaletteCatalog* GetPaletteCatalog() const;
	UHorizontalBox* ResolveStaticObstacleItemContainer() const;
	UHorizontalBox* ResolveGroundRegionItemContainer() const;
	UScenarioPlaceablePaletteItemWidget* CreatePaletteItemWidget(AScenarioEditorController* editorController) const;
	void BindPaletteItemWidget(UScenarioPlaceablePaletteItemWidget* itemWidget);
	bool AddPaletteItemWidget(
		AScenarioEditorController* editorController,
		UHorizontalBox* targetContainer,
		const FScenarioPaletteItemEntry& paletteItemEntry);
	int32 AddDefaultGroundRegionPaletteEntries(AScenarioEditorController* editorController);
	static bool ShouldIncludeSpecialEntry(
		const FScenarioPaletteItemEntry& entry,
		bool bIncludePedestrian,
		bool bIncludeRobotRoute);

	UWidget* ResolveInputModeFocusWidget() const;

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
