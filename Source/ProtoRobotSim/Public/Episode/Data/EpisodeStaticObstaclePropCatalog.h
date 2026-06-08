#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodeStaticObstaclePropCatalog.generated.h"

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeStaticObstaclePropCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	static TSoftObjectPtr<UEpisodeStaticObstaclePropCatalog> MakeDefaultCatalogReference();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Static Obstacle")
	TArray<FEpisodeStaticObstaclePropEntry> Entries;

	UFUNCTION(BlueprintPure, Category = "Episode|Static Obstacle")
	bool FindPropEntryById(FName propId, FEpisodeStaticObstaclePropEntry& outPropEntry) const;

	const TArray<FEpisodeStaticObstaclePropEntry>& GetEntries() const { return Entries; }
};
