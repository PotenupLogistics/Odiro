#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioPlaceablePaletteItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FScenarioPlaceablePaletteItemSelected,
	EScenarioPaletteItemType,
	ItemType,
	FName,
	AssetId);

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UScenarioPlaceablePaletteItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintAssignable, Category = "Episode|Editor|Palette")
	FScenarioPlaceablePaletteItemSelected OnSelected;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UTextBlock> DisplayNameTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UTextBlock> CategoryTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette", meta = (DeprecatedProperty, DeprecationMessage = "Palette thumbnails are now read from catalog data assets."))
	FString IconDirectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette", meta = (DeprecatedProperty, DeprecationMessage = "Palette thumbnails are now read from catalog data assets."))
	FString IconAssetPrefix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bMatchThumbnailImageSizeToTexture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bHideThumbnailImageWhenMissing = true;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void SetPropEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void SetPaletteItemEntry(const FScenarioPaletteItemEntry& paletteItemEntry);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FScenarioPaletteItemEntry GetPaletteItemEntry() const { return PaletteItemEntry; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FScenarioStaticObstaclePropEntry GetPropEntry() const { return PropEntry; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FName GetPropId() const { return PaletteItemEntry.AssetId; }

protected:
	UFUNCTION()
	void HandleSelectButtonClicked();

private:
	static FText CategoryToText(EScenarioStaticObstaclePropCategory category);
	static FString MakeDisplayNameFromPropId(FName propId);
	static FScenarioPaletteItemEntry MakeStaticObstaclePaletteItemEntry(const FScenarioStaticObstaclePropEntry& propEntry);

	void ApplyThumbnailImage();

	UPROPERTY(Transient)
	FScenarioStaticObstaclePropEntry PropEntry;

	UPROPERTY(Transient)
	FScenarioPaletteItemEntry PaletteItemEntry;
};
