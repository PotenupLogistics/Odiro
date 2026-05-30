#include "Episode/Actors/EpisodeSplinePath.h"

#include "Components/SplineComponent.h"

AEpisodeSplinePath::AEpisodeSplinePath()
{
	PrimaryActorTick.bCanEverTick = false;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);
}

void AEpisodeSplinePath::ConfigurePath(const FString& InPathId, const TArray<FVector>& Points, bool bClosedLoop)
{
	PathId = InPathId;

	if (!SplineComponent) return;

	SplineComponent->ClearSplinePoints(false);
	for (const FVector& Point : Points)
	{
		SplineComponent->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}

	SplineComponent->SetClosedLoop(bClosedLoop, false);
	SplineComponent->UpdateSpline();
}
