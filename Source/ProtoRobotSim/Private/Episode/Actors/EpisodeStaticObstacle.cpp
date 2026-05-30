#include "Episode/Actors/EpisodeStaticObstacle.h"

#include "Components/SceneComponent.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

AEpisodeStaticObstacle::AEpisodeStaticObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));
}
