#include "Episode/Actors/EpisodeSplinePath.h"

#include "Components/SplineComponent.h"

AEpisodeSplinePath::AEpisodeSplinePath()
{
	PrimaryActorTick.bCanEverTick = false;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);
}
