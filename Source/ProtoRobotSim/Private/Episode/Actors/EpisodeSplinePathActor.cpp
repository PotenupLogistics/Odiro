#include "Episode/Actors/EpisodeSplinePathActor.h"

#include "Components/SplineComponent.h"

AEpisodeSplinePathActor::AEpisodeSplinePathActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);
}
