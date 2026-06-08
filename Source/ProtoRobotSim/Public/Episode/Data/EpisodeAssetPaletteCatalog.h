#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Episode/Editor/EpisodeEditorTypes.h"
#include "EpisodeAssetPaletteCatalog.generated.h"

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeAssetPaletteCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	static TSoftObjectPtr<UEpisodeAssetPaletteCatalog> MakeDefaultCatalogReference();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Palette")
	TArray<FEpisodePaletteItemEntry> SpecialEntries;
};
