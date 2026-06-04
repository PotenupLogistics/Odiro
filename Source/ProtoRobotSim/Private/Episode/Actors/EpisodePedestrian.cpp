
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Episode/Components/EpisodePedestrianRuntimeComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

AEpisodePedestrian::AEpisodePedestrian()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	PathFollowerComponent = CreateDefaultSubobject<UEpisodePathFollowerComponent>(TEXT("PathFollowerComponent"));
	PathFollowerComponent->bUseSeededPathNoise = true;
	PathFollowerComponent->VerticalOffsetCm = 90.0;

	PedestrianRuntimeComponent = CreateDefaultSubobject<UEpisodePedestrianRuntimeComponent>(TEXT("PedestrianRuntimeComponent"));
}

void AEpisodePedestrian::UpdateVisualMotion(const FVector& previousLocation, const FVector& newLocation, double deltaSeconds)
{
	if (deltaSeconds <= KINDA_SMALL_NUMBER)
	{
		ResetVisualMotion();
		return;
	}

	const FVector deltaLocation = newLocation - previousLocation;
	const FVector visualVelocity = deltaLocation / deltaSeconds;
	const FVector moveDirection = visualVelocity.GetSafeNormal2D();

	VisualSpeedCmPerSecond = static_cast<float>(visualVelocity.Size2D());
	bMoving = VisualSpeedCmPerSecond > 3.0f;

	if (!bMoving || moveDirection.IsNearlyZero())
	{
		VisualDirectionDegrees = 0.0f;
		return;
	}

	const FVector forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector right = GetActorRightVector().GetSafeNormal2D();
	const double forwardAmount = FVector::DotProduct(moveDirection, forward);
	const double rightAmount = FVector::DotProduct(moveDirection, right);
	VisualDirectionDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(rightAmount, forwardAmount)));
}

void AEpisodePedestrian::ResetVisualMotion()
{
	VisualSpeedCmPerSecond = 0.0f;
	VisualDirectionDegrees = 0.0f;
	bMoving = false;
}
