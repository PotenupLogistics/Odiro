
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePedestrianRuntimeComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

AEpisodePedestrian::AEpisodePedestrian()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	PathFollowerComponent = CreateDefaultSubobject<UEpisodePathFollowerComponent>(TEXT("PathFollowerComponent"));
	PathFollowerComponent->bUseSeededPathNoise = true;
	PathFollowerComponent->VerticalOffsetCm = 90.0;

	PedestrianRuntimeComponent = CreateDefaultSubobject<UEpisodePedestrianRuntimeComponent>(TEXT("PedestrianRuntimeComponent"));
}
