#include "Episode/Actors/EpisodeVehicle.h"

#include "Components/SceneComponent.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

AEpisodeVehicle::AEpisodeVehicle()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));
	PathFollowerComponent = CreateDefaultSubobject<UEpisodePathFollowerComponent>(TEXT("PathFollowerComponent"));
}
