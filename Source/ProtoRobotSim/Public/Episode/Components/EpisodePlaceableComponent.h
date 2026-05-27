#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePlaceableComponent.generated.h"

// 에피소드에 배치된 actor의 공통 식별 정보를 담는 component 파일임.
// spawned actor와 JSON 명세의 instance_id, asset_id를 연결하는 component임.
UCLASS(ClassGroup = (Episode), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UEpisodePlaceableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEpisodePlaceableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeActorCategory Category = EEpisodeActorCategory::StaticObstacle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeMobilityMode MobilityMode = EEpisodeMobilityMode::Static;
};
