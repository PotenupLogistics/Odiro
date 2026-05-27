#include "Episode/Actors/EpisodePedestrianActor.h"

#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

AEpisodePedestrianActor::AEpisodePedestrianActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));
	PathFollowerComponent = CreateDefaultSubobject<UEpisodePathFollowerComponent>(TEXT("PathFollowerComponent"));
}

void AEpisodePedestrianActor::BeginPlay()
{
	Super::BeginPlay();
}

void AEpisodePedestrianActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}
