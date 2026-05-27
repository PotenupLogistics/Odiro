#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodePedestrianActor.generated.h"

class UEpisodeObstacleCollisionComponent;
class UEpisodePathFollowerComponent;
class UEpisodePlaceableComponent;

// 보행자 actor(아이, 성인, 노인 등) 파일임.
// profile과 path follower로 보행자 차이를 표현하는 Character 기반 actor임.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodePedestrianActor : public ACharacter
{
	GENERATED_BODY()

public:
	AEpisodePedestrianActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodeObstacleCollisionComponent> ObstacleCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode")
	TObjectPtr<UEpisodePathFollowerComponent> PathFollowerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodePedestrianProfile PedestrianProfile = EEpisodePedestrianProfile::Adult;
	
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};
