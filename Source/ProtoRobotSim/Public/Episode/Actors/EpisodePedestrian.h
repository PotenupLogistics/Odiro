#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EpisodePedestrian.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePathFollowerComponent;
class UEpisodePedestrianRuntimeComponent;
class UEpisodePlaceableComponent;

// 보행자 Character 기반 actor임.
// 이동 방식은 legacy path follower 또는 planned trajectory runtime component가 담당한다.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodePedestrian : public ACharacter
{
	GENERATED_BODY()

public:
	AEpisodePedestrian();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodeObstacleCollisionComponent> ObstacleCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePathFollowerComponent> PathFollowerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePedestrianRuntimeComponent> PedestrianRuntimeComponent;
};
