#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePlaceablePaletteItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEpisodePlaceablePaletteItemSelected, FName, PropId);

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
	FString StaticObstacleIconDirectory = TEXT("/Game/Widgets/Icon");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	FString StaticObstacleIconAssetPrefix = TEXT("icon_");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bMatchThumbnailImageSizeToTexture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Palette")
	bool bHideThumbnailImageWhenMissing = true;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void SetPropEntry(const FEpisodeStaticObstaclePropEntry& propEntry);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FEpisodeStaticObstaclePropEntry GetPropEntry() const { return PropEntry; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Palette")
	FName GetPropId() const { return PropEntry.PropId; }

protected:
	UFUNCTION()
	void HandleSelectButtonClicked();

private:
	static FText CategoryToText(EEpisodeStaticObstaclePropCategory category);
	static FString MakeIconSuffixFromPropId(FName propId);

	FString BuildThumbnailTextureObjectPath() const;
	void ApplyThumbnailImage();

	UPROPERTY(Transient)
	FEpisodeStaticObstaclePropEntry PropEntry;
};
