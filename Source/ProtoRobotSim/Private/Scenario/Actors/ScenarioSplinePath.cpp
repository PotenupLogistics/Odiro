#include "Scenario/Actors/ScenarioSplinePath.h"

#include "Components/SplineComponent.h"

AScenarioSplinePath::AScenarioSplinePath()
{
	PrimaryActorTick.bCanEverTick = false;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);
}

void AScenarioSplinePath::ConfigurePath(const FString& inPathId, const TArray<FVector>& points, bool bClosedLoop)
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
