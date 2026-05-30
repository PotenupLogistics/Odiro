#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EpisodeStaticObstacle.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePlaceableComponent;
class USceneComponent;

// 정적 장애물(쓰레기봉투, 가로수, 입간판, 방치 PM 등) actor 파일임.
// 움직이지 않는 에피소드 장애물의 기본 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeStaticObstacle : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeStaticObstacle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodeObstacleCollisionComponent> ObstacleCollisionComponent;
};
