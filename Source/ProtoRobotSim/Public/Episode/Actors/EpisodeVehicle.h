#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodeVehicle.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePathFollowerComponent;
class UEpisodePlaceableComponent;
class USceneComponent;

// 차량 actor(주차, 주행, 출차 상태)을 표현하는 파일임.
// 차량 상태를 parked, driving, pulling out으로 구분하는 Pawn 기반 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeVehicle : public APawn
{
	GENERATED_BODY()

public:
	AEpisodeVehicle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodeObstacleCollisionComponent> ObstacleCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePathFollowerComponent> PathFollowerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeRoadVehicleState VehicleState = EEpisodeRoadVehicleState::Parked;
};
