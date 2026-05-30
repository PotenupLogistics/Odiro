#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePersonalMobility.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePathFollowerComponent;
class UEpisodePlaceableComponent;
class USceneComponent;

// 인도 주변 이동체 actor(자전거, PM, 스쿠터 등) 파일임.
// 개인형 이동수단의 타입과 이동 모드를 표현하는 Pawn 기반 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodePersonalMobility : public APawn
{
	GENERATED_BODY()

public:
	AEpisodePersonalMobility();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodeObstacleCollisionComponent> ObstacleCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePathFollowerComponent> PathFollowerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodePersonalMobilityType MobilityType = EEpisodePersonalMobilityType::PM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeMobilityMode MobilityMode = EEpisodeMobilityMode::Moving;
};
