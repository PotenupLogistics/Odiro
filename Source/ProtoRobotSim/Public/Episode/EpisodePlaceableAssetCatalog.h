#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePlaceableAssetCatalog.generated.h"

// asset_id를 실제 spawn 가능한 Actor class와 연결하는 카탈로그 파일임.
// 에피소드에서 배치 가능한 asset 하나의 메타데이터임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePlaceableAssetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	EEpisodeActorCategory Category = EEpisodeActorCategory::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	TMap<FString, FEpisodeParamValue> DefaultProperties;
};

// WorldSpec의 asset_id를 Actor class로 해석하기 위한 PrimaryDataAsset임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodePlaceableAssetCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Episode")
	TArray<FEpisodePlaceableAssetEntry> Entries;
};
