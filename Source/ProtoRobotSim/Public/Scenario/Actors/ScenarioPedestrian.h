#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ScenarioPedestrian.generated.h"

class UScenarioObstacleCollisionComponent;
class UScenarioPathFollowerComponent;
class UScenarioPedestrianRuntimeComponent;
class UScenarioPlaceableComponent;

// 보행자 Character 기반 actor임.
// 이동 방식은 legacy path follower 또는 planned trajectory runtime component가 담당한다.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API AScenarioPedestrian : public ACharacter
{
	GENERATED_BODY()

public:
	AScenarioPedestrian();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioPlaceableComponent> PlaceableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioObstacleCollisionComponent> ObstacleCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioPathFollowerComponent> PathFollowerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UScenarioPedestrianRuntimeComponent> PedestrianRuntimeComponent;
};
