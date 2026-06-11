
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Components/ScenarioObstacleCollisionComponent.h"
#include "Scenario/Components/ScenarioPathFollowerComponent.h"
#include "Scenario/Components/ScenarioPedestrianRuntimeComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"

AScenarioPedestrian::AScenarioPedestrian()
{
	PrimaryActorTick.bCanEverTick = false;
	Tags.AddUnique(FName(TEXT("ObjectType.pedestrian")));

	PlaceableComponent = CreateDefaultSubobject<UScenarioPlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UScenarioObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	PathFollowerComponent = CreateDefaultSubobject<UScenarioPathFollowerComponent>(TEXT("PathFollowerComponent"));
	PathFollowerComponent->bUseSeededPathNoise = true;
	PathFollowerComponent->VerticalOffsetCm = 90.0;

	PedestrianRuntimeComponent = CreateDefaultSubobject<UScenarioPedestrianRuntimeComponent>(TEXT("PedestrianRuntimeComponent"));
}
