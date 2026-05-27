#include "Episode/Actors/EpisodePedestrianActor.h"

#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Components/SplineComponent.h"

AEpisodePedestrianActor::AEpisodePedestrianActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	MovementSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("MovementSplineComponent"));
	MovementSplineComponent->SetupAttachment(GetRootComponent());
	MovementSplineComponent->ClearSplinePoints(false);
	MovementSplineComponent->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
	MovementSplineComponent->AddSplinePoint(FVector(500.0, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
	MovementSplineComponent->UpdateSpline();

	PathFollowerComponent = CreateDefaultSubobject<UEpisodePathFollowerComponent>(TEXT("PathFollowerComponent"));
	PathFollowerComponent->SetSplineComponent(MovementSplineComponent);
	PathFollowerComponent->bUseSeededPathNoise = true;
}

void AEpisodePedestrianActor::BeginPlay()
{
	Super::BeginPlay();
}

void AEpisodePedestrianActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}
