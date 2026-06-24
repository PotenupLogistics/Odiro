#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioAssetPaletteWidget.generated.h"

class UHorizontalBox;
class UScrollBox;
class USizeBox;
class UWidget;
class UScenarioAssetPaletteCatalog;
class UScenarioAssetPaletteViewModel;
class UScenarioEditorListItemViewModel;
class UScenarioPlaceablePaletteItemWidget;

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioAssetPaletteWidget : public UUserWidget
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

	// Legacy toggle retained for asset compatibility; pedestrian authoring is not palette-backed yet.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bIncludePedestrianPlacement = false;

	// Legacy toggle retained for asset compatibility; robot anchors are authored through route controls.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bIncludeRobotRoutePlacement = false;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<USizeBox> PaletteSizeBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UScrollBox> PaletteScrollBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UHorizontalBox> PlaceableItemContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UHorizontalBox> StaticObstacleItemContainer;

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
	UScenarioPlaceablePaletteItemWidget* CreatePaletteItemWidget() const;
	void BindPaletteItemWidget(UScenarioPlaceablePaletteItemWidget* itemWidget);
	bool AddPaletteItemWidget(
		UHorizontalBox* targetContainer,
		const FScenarioPaletteItemEntry& paletteItemEntry);
	bool AddPaletteItemWidget(
		UHorizontalBox* targetContainer,
		UScenarioEditorListItemViewModel* itemViewModel);
	static bool ShouldIncludeSpecialEntry(
		const FScenarioPaletteItemEntry& entry,
		bool bIncludePedestrian,
		bool bIncludeRobotRoute);

	// Resolves the subsystem-owned palette ViewModel for command forwarding.
	void InitializeViewModel();
	UWidget* ResolveInputModeFocusWidget() const;

	// Palette command and item state owned by ScenarioEditorUiSubsystem.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioAssetPaletteViewModel> AssetPaletteViewModel;

	TWeakObjectPtr<UWidget> RequestedInputModeFocusWidget;
};
