#include "Episode/Actors/EpisodeSplinePath.h"

#include "Components/SplineComponent.h"

AEpisodeSplinePath::AEpisodeSplinePath()
{
	PrimaryActorTick.bCanEverTick = false;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);
}

void AEpisodeSplinePath::ConfigurePath(const FString& inPathId, const TArray<FVector>& points, bool bClosedLoop)
{
	PathId = inPathId;

	if (!SplineComponent) return;

	SplineComponent->ClearSplinePoints(false);
	for (const FVector& point : points)
	{
		SplineComponent->AddSplinePoint(point, ESplineCoordinateSpace::World, false);
	}

	SplineComponent->SetClosedLoop(bClosedLoop, false);
	SplineComponent->UpdateSpline();
}
