#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shared/ScenarioCoreTypes.h"
#include "ScenarioStaticObstaclePropCatalog.generated.h"

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioStaticObstaclePropCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	static TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> MakeDefaultCatalogReference();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode|Static Obstacle")
	TArray<FScenarioStaticObstaclePropEntry> Entries;

	UFUNCTION(BlueprintPure, Category = "Episode|Static Obstacle")
	bool FindPropEntryById(FName propId, FScenarioStaticObstaclePropEntry& outPropEntry) const;

	const TArray<FScenarioStaticObstaclePropEntry>& GetEntries() const { return Entries; }
};
