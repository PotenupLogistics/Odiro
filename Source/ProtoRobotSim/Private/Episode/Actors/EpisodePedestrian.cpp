
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

AEpisodePedestrian::AEpisodePedestrian()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	PathFollowerComponent = CreateDefaultSubobject<UEpisodePathFollowerComponent>(TEXT("PathFollowerComponent"));
	PathFollowerComponent->bUseSeededPathNoise = true;
}

void AEpisodePedestrian::UpdateVisualMotion(const FVector& PreviousLocation, const FVector& NewLocation, double DeltaSeconds)
{
	if (DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		ResetVisualMotion();
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

void AEpisodePedestrian::ResetVisualMotion()
{
	VisualSpeedCmPerSecond = 0.0f;
	VisualDirectionDegrees = 0.0f;
	bMoving = false;
}
