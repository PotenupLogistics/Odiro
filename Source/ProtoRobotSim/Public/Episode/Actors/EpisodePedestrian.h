#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePedestrian.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePathFollowerComponent;
class UEpisodePlaceableComponent;

// 보행자 actor(아이, 성인, 노인 등) 파일임.
// profile과 path follower로 보행자 차이를 표현하는 Character 기반 actor임.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodePedestrianProfile PedestrianProfile = EEpisodePedestrianProfile::Adult;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|Visual")
	float VisualSpeedCmPerSecond = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|Visual")
	float VisualDirectionDegrees = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Episode|Visual")
	bool bMoving = false;

	void UpdateVisualMotion(const FVector& PreviousLocation, const FVector& NewLocation, double DeltaSeconds);
	void ResetVisualMotion();
};
