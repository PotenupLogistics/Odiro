#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Episode/Editor/EpisodeEditorTypes.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePlaceablePaletteItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FEpisodePlaceablePaletteItemSelected,
	EEpisodePaletteItemType,
	ItemType,
	FName,
	AssetId);

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API UEpisodePlaceablePaletteItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintAssignable, Category = "Episode|Editor|Palette")
	FEpisodePlaceablePaletteItemSelected OnSelected;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UTextBlock> DisplayNameTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UTextBlock> CategoryTextBlock;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Episode|Editor|Palette")
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	FString IconDirectory = TEXT("/Game/Widgets/Thumbnail");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	FString IconAssetPrefix = TEXT("icon_");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bMatchThumbnailImageSizeToTexture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bHideThumbnailImageWhenMissing = true;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void SetPropEntry(const FEpisodeStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void SetPaletteItemEntry(const FEpisodePaletteItemEntry& paletteItemEntry);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FEpisodePaletteItemEntry GetPaletteItemEntry() const { return PaletteItemEntry; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FEpisodeStaticObstaclePropEntry GetPropEntry() const { return PropEntry; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FName GetPropId() const { return PaletteItemEntry.AssetId; }

protected:
	UFUNCTION()
	void HandleSelectButtonClicked();

private:
	static FText CategoryToText(EEpisodeStaticObstaclePropCategory category);
	static FString MakeDisplayNameFromPropId(FName propId);
	static FString MakeIconSuffixFromPropId(FName propId);
	static FEpisodePaletteItemEntry MakeStaticObstaclePaletteItemEntry(const FEpisodeStaticObstaclePropEntry& propEntry);

	FString BuildThumbnailTextureObjectPath() const;
	void ApplyThumbnailImage();

	UPROPERTY(Transient)
	FEpisodeStaticObstaclePropEntry PropEntry;

	UPROPERTY(Transient)
	FEpisodePaletteItemEntry PaletteItemEntry;
};
