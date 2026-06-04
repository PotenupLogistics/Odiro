#include "Episode/Components/EpisodePedestrianRuntimeComponent.h"

#include "Episode/Actors/EpisodePedestrian.h"

UEpisodePedestrianRuntimeComponent::UEpisodePedestrianRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEpisodePedestrianRuntimeComponent::ConfigurePlan(
	const FString& inInstanceId,
	const FEpisodePedestrianPlan& plan,
	double fallbackSpeedCmPerSecond,
	double initialDistanceCm,
	bool bStartAutomatically)
{
	InstanceId = inInstanceId;
	PlanId = plan.PlanId;
	PlanHash = plan.PlanHash;
	PlanPoints = plan.Points;
	SpeedCmPerSecond = fallbackSpeedCmPerSecond;
	if (PlanPoints.Num() > 0)
	{
		SpeedCmPerSecond = FMath::Max(PlanPoints[0].SpeedCmPerSecond, 1.0);
	}
	InitialDistanceCm = FMath::Max(0.0, initialDistanceCm);
	CurrentDistanceCm = InitialDistanceCm;
	TotalDistanceCm = PlanPoints.Num() > 0 ? PlanPoints.Last().DistanceCm : 0.0;
	bAutoStart = bStartAutomatically;
}

void UEpisodePedestrianRuntimeComponent::StartFollowing()
{
	if (!HasPlan())
	{
		StopFollowing();
		return;
	}

	CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm, 0.0, TotalDistanceCm);
	MoveOwnerToCurrentDistance();
	SetComponentTickEnabled(true);
}

void UEpisodePedestrianRuntimeComponent::StopFollowing()
{
	SetComponentTickEnabled(false);

	if (AEpisodePedestrian* pedestrian = Cast<AEpisodePedestrian>(GetOwner()))
	{
		pedestrian->ResetVisualMotion();
	}
}

bool UEpisodePedestrianRuntimeComponent::HasPlan() const
{
	return PlanPoints.Num() >= 2 && TotalDistanceCm > KINDA_SMALL_NUMBER;
}

void UEpisodePedestrianRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentDistanceCm = InitialDistanceCm;
	MoveOwnerToCurrentDistance();

	if (bAutoStart)
	{
		StartFollowing();
	}
}

void UEpisodePedestrianRuntimeComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaTime, tickType, thisTickFunction);

	if (!HasPlan())
	{
		StopFollowing();
		return;
	}

	const double distanceDeltaCm = FMath::Max(SpeedCmPerSecond, 0.0) * static_cast<double>(deltaTime);
	CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm + distanceDeltaCm, 0.0, TotalDistanceCm);
	MoveOwnerToCurrentDistance(deltaTime);

	if (FMath::IsNearlyEqual(CurrentDistanceCm, TotalDistanceCm))
	{
		StopFollowing();
	}
}

FVector UEpisodePedestrianRuntimeComponent::GetLocationAtDistance(double distanceCm) const
{
	if (PlanPoints.IsEmpty()) return FVector::ZeroVector;
	if (distanceCm <= 0.0) return PlanPoints[0].Location;
	if (distanceCm >= TotalDistanceCm) return PlanPoints.Last().Location;

	for (int32 pointIndex = 0; pointIndex < PlanPoints.Num() - 1; ++pointIndex)
	{
		const FEpisodePedestrianPlanPoint& segmentStart = PlanPoints[pointIndex];
		const FEpisodePedestrianPlanPoint& segmentEnd = PlanPoints[pointIndex + 1];
		if (distanceCm > segmentEnd.DistanceCm)
		{
			continue;
		}

		const double segmentLengthCm = segmentEnd.DistanceCm - segmentStart.DistanceCm;
		if (segmentLengthCm <= KINDA_SMALL_NUMBER)
		{
			return segmentEnd.Location;
		}

		const double alpha = FMath::Clamp((distanceCm - segmentStart.DistanceCm) / segmentLengthCm, 0.0, 1.0);
		return FMath::Lerp(segmentStart.Location, segmentEnd.Location, alpha);
	}

	return PlanPoints.Last().Location;
}

FVector UEpisodePedestrianRuntimeComponent::GetDirectionAtDistance(double distanceCm) const
{
	if (PlanPoints.Num() < 2) return FVector::ForwardVector;
	if (distanceCm <= 0.0) return PlanPoints[0].Direction.GetSafeNormal2D();
	if (distanceCm >= TotalDistanceCm) return PlanPoints.Last().Direction.GetSafeNormal2D();

	for (int32 pointIndex = 0; pointIndex < PlanPoints.Num() - 1; ++pointIndex)
	{
		if (distanceCm <= PlanPoints[pointIndex + 1].DistanceCm)
		{
			const FVector direction = (PlanPoints[pointIndex + 1].Location - PlanPoints[pointIndex].Location).GetSafeNormal2D();
			return direction.IsNearlyZero() ? FVector::ForwardVector : direction;
		}
	}

	return FVector::ForwardVector;
}

void UEpisodePedestrianRuntimeComponent::MoveOwnerToCurrentDistance(double deltaSeconds)
{
	AActor* owner = GetOwner();
	if (!owner || !HasPlan()) return;

	const FVector previousLocation = owner->GetActorLocation();
	FVector targetLocation = GetLocationAtDistance(CurrentDistanceCm);
	targetLocation.Z += VerticalOffsetCm;

	const FVector direction = GetDirectionAtDistance(CurrentDistanceCm);
	const FRotator targetRotation = direction.IsNearlyZero()
		? owner->GetActorRotation()
		: direction.Rotation();

	FHitResult sweepHit;
	owner->SetActorLocationAndRotation(targetLocation, targetRotation, true, &sweepHit, ETeleportType::None);

	if (AEpisodePedestrian* pedestrian = Cast<AEpisodePedestrian>(owner))
	{
		pedestrian->UpdateVisualMotion(previousLocation, owner->GetActorLocation(), deltaSeconds);
	}
}
