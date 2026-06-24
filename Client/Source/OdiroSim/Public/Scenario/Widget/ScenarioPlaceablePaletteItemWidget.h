#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioPlaceablePaletteItemWidget.generated.h"

class UButton;
class UImage;
class UScenarioEditorListItemViewModel;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FScenarioPlaceablePaletteItemSelected,
	EScenarioPaletteItemType,
	ItemType,
	FName,
	AssetId);

UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioPlaceablePaletteItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	// Applies shared text styling after the UMG tree is constructed.
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Palette")
	FScenarioPlaceablePaletteItemSelected OnSelected;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UTextBlock> DisplayNameTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UTextBlock> CategoryTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Palette")
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette", meta = (DeprecatedProperty, DeprecationMessage = "Palette thumbnails are now read from catalog data assets."))
	FString IconDirectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette", meta = (DeprecatedProperty, DeprecationMessage = "Palette thumbnails are now read from catalog data assets."))
	FString IconAssetPrefix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bMatchThumbnailImageSizeToTexture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Palette")
	bool bHideThumbnailImageWhenMissing = true;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	void SetPropEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	void SetPaletteItemEntry(const FScenarioPaletteItemEntry& paletteItemEntry);

	// 공통 palette item ViewModel의 표시/command 상태를 tile UI에 반영한다.
	void InitializeFromItemViewModel(UScenarioEditorListItemViewModel* itemViewModel);

	// Static obstacle catalog entry를 공통 palette entry로 변환한다.
	static FScenarioPaletteItemEntry MakeStaticObstaclePaletteItemEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Palette")
	FScenarioPaletteItemEntry GetPaletteItemEntry() const { return PaletteItemEntry; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Palette")
	FScenarioStaticObstaclePropEntry GetPropEntry() const { return PropEntry; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Palette")
	FName GetPropId() const { return PaletteItemEntry.AssetId; }

protected:
	UFUNCTION()
	void HandleSelectButtonClicked();

private:
	static FText CategoryToText(EScenarioStaticObstaclePropCategory category);
	static FString MakeDisplayNameFromPropId(FName propId);

	void ApplyThumbnailImage();

	// Tile 표시 데이터를 제공하는 palette item ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorListItemViewModel> ItemViewModel;

	UPROPERTY(Transient)
	FScenarioStaticObstaclePropEntry PropEntry;

	UPROPERTY(Transient)
	FScenarioPaletteItemEntry PaletteItemEntry;
};
