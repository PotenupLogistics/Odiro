
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Components/SplineComponent.h"

AEpisodePedestrian::AEpisodePedestrian()
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

void AEpisodePedestrian::UpdateVisualMotion(const FVector& PreviousLocation, const FVector& NewLocation, double DeltaSeconds)
{
	if (DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector DeltaLocation = NewLocation - PreviousLocation;
	const FVector VisualVelocity = DeltaLocation / DeltaSeconds;
	const FVector MoveDirection = VisualVelocity.GetSafeNormal2D();

	VisualSpeedCmPerSecond = static_cast<float>(VisualVelocity.Size2D());
	bMoving = VisualSpeedCmPerSecond > 3.0f;

	if (!bMoving || MoveDirection.IsNearlyZero())
	{
		VisualDirectionDegrees = 0.0f;
		return;
	}

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();
	const double ForwardAmount = FVector::DotProduct(MoveDirection, Forward);
	const double RightAmount = FVector::DotProduct(MoveDirection, Right);
	VisualDirectionDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(RightAmount, ForwardAmount)));
}