
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Components/SplineComponent.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"

UEpisodePathFollowerComponent::UEpisodePathFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEpisodePathFollowerComponent::SetSplineComponent(USplineComponent* inSplineComponent)
{
	SplineComponent = inSplineComponent;

	if (!SplineComponent)
	{
		StopFollowing();
	}
}

void UEpisodePathFollowerComponent::SetSplinePath(AEpisodeSplinePath* inSplinePath)
{
	if (!inSplinePath)
	{
		PathId.Reset();
		SplineComponent = nullptr;
		StopFollowing();
		return;
	}

	PathId = inSplinePath->PathId;
	SetSplineComponent(inSplinePath->SplineComponent);
}

void UEpisodePathFollowerComponent::StartFollowing()
{
	if (!SplineComponent)
	{
		StopFollowing();
		return;
	}

	MoveOwnerToCurrentDistance();
	SetComponentTickEnabled(true);
}

void UEpisodePathFollowerComponent::StopFollowing()
{
	SetComponentTickEnabled(false);

	if (AEpisodePedestrian* pedestrian = Cast<AEpisodePedestrian>(GetOwner()))
	{
		pedestrian->ResetVisualMotion();
	}
}

void UEpisodePathFollowerComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentDistanceCm = InitialDistanceCm;
	InitializePathNoise();
	MoveOwnerToCurrentDistance();

	if (bAutoStart)
	{
		StartFollowing();
	}
}

void UEpisodePathFollowerComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaTime, tickType, thisTickFunction);

	if (!SplineComponent)
	{
		StopFollowing();
		return;
	}

	const double splineLength = SplineComponent->GetSplineLength();
	if (splineLength <= KINDA_SMALL_NUMBER)
	{
		StopFollowing();
		return;
	}

	const double previousDistanceCm = CurrentDistanceCm;
	const double distanceDeltaCm = SpeedCmPerSecond * GetPathNoiseSpeedScale(splineLength) * static_cast<double>(deltaTime);
	const double desiredDistanceCm = CurrentDistanceCm + distanceDeltaCm;

	CurrentDistanceCm = desiredDistanceCm;
	bool bReachedEnd = false;
	if (bLoop)
	{
		CurrentDistanceCm = FMath::Fmod(CurrentDistanceCm, splineLength);
		if (CurrentDistanceCm < 0.0)
		{
			CurrentDistanceCm += splineLength;
		}
	}
	else
	{
		CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm, 0.0, splineLength);
		if (FMath::IsNearlyEqual(CurrentDistanceCm, splineLength))
		{
			bReachedEnd = true;
		}
	}

	const double appliedDistanceDeltaCm = bLoop ? distanceDeltaCm : CurrentDistanceCm - previousDistanceCm;
	FHitResult sweepHit;
	MoveOwnerToCurrentDistance(deltaTime, &sweepHit);

	// sweep된 이후 순식간에 route를 따라잡지 않도록 처리.
	if (sweepHit.bBlockingHit
		&& sweepHit.Time < 1.0f - KINDA_SMALL_NUMBER
		&& FMath::Abs(appliedDistanceDeltaCm) > KINDA_SMALL_NUMBER)
	{
		CurrentDistanceCm = previousDistanceCm
			+ appliedDistanceDeltaCm * FMath::Clamp(static_cast<double>(sweepHit.Time), 0.0, 1.0);
		if (bLoop)
		{
			CurrentDistanceCm = FMath::Fmod(CurrentDistanceCm, splineLength);
			if (CurrentDistanceCm < 0.0)
			{
				CurrentDistanceCm += splineLength;
			}
		}
		else
		{
			CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm, 0.0, splineLength);
		}
		bReachedEnd = false;
	}

	if (bReachedEnd)
	{
		StopFollowing();
	}
}

void UEpisodePathFollowerComponent::InitializePathNoise()
{
	FRandomStream noiseStream(PathNoiseSeed);
	LateralNoisePhase = noiseStream.FRandRange(-10000.0f, 10000.0f);
	SpeedNoisePhase = noiseStream.FRandRange(-10000.0f, 10000.0f);
}

double UEpisodePathFollowerComponent::GetPathNoiseFade(double splineLength) const
{
	if (bLoop || PathNoiseEndpointFadeDistanceCm <= KINDA_SMALL_NUMBER) return 1.0;

	const double startFade = CurrentDistanceCm / PathNoiseEndpointFadeDistanceCm;
	const double endFade = (splineLength - CurrentDistanceCm) / PathNoiseEndpointFadeDistanceCm;
	return FMath::Clamp(FMath::Min(startFade, endFade), 0.0, 1.0);
}

double UEpisodePathFollowerComponent::GetPathNoiseSpeedScale(double splineLength) const
{
	if (!bUseSeededPathNoise || SpeedNoiseStrength <= KINDA_SMALL_NUMBER || SpeedNoiseWavelengthCm <= KINDA_SMALL_NUMBER) return 1.0;

	const double fade = GetPathNoiseFade(splineLength);
	if (fade <= KINDA_SMALL_NUMBER) return 1.0;

	const double noiseInput = (CurrentDistanceCm / SpeedNoiseWavelengthCm) + SpeedNoisePhase;
	const double noiseValue = FMath::PerlinNoise1D(static_cast<float>(noiseInput));
	const double strength = FMath::Clamp(SpeedNoiseStrength, 0.0, 0.95);
	return FMath::Clamp(1.0 + noiseValue * strength * fade, 1.0 - strength, 1.0 + strength);
}

FVector UEpisodePathFollowerComponent::ApplyPathNoise(double distanceCm, double splineLength, const FVector& baseLocation) const
{
	if (!bUseSeededPathNoise || !SplineComponent || LateralNoiseAmplitudeCm <= KINDA_SMALL_NUMBER || LateralNoiseWavelengthCm <= KINDA_SMALL_NUMBER) return baseLocation;

	const double fade = GetPathNoiseFade(splineLength);
	if (fade <= KINDA_SMALL_NUMBER) return baseLocation;

	FVector forward = SplineComponent->GetDirectionAtDistanceAlongSpline(
		static_cast<float>(distanceCm),
		ESplineCoordinateSpace::World);
	forward.Z = 0.0;

	if (!forward.Normalize()) return baseLocation;

	const FVector right = FVector::CrossProduct(FVector::UpVector, forward).GetSafeNormal();
	const double noiseInput = (distanceCm / LateralNoiseWavelengthCm) + LateralNoisePhase;
	const double noiseValue = FMath::PerlinNoise1D(static_cast<float>(noiseInput));
	const double lateralOffsetCm = noiseValue * LateralNoiseAmplitudeCm * fade;
	return baseLocation + right * lateralOffsetCm;
}

void UEpisodePathFollowerComponent::MoveOwnerToCurrentDistance(double deltaSeconds, FHitResult* outSweepHit)
{
	if (outSweepHit)
	{
		*outSweepHit = FHitResult();
	}

	if (!SplineComponent) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	const FVector previousLocation = owner->GetActorLocation();
	const FVector location = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(CurrentDistanceCm),
		ESplineCoordinateSpace::World);
	const FVector noisyLocation = ApplyPathNoise(CurrentDistanceCm, SplineComponent->GetSplineLength(), location);
	FVector targetLocation = noisyLocation;
	targetLocation.Z += VerticalOffsetCm;

	FHitResult sweepHit;
	if (bOrientToSpline)
	{
		const FRotator rotation = SplineComponent->GetRotationAtDistanceAlongSpline(
			static_cast<float>(CurrentDistanceCm),
			ESplineCoordinateSpace::World);
		owner->SetActorLocationAndRotation(targetLocation, rotation, true, &sweepHit, ETeleportType::None);
	}
	else
	{
		owner->SetActorLocation(targetLocation, true, &sweepHit, ETeleportType::None);
	}

	if (outSweepHit)
	{
		*outSweepHit = sweepHit;
	}

	if (AEpisodePedestrian* pedestrian = Cast<AEpisodePedestrian>(owner))
	{
		pedestrian->UpdateVisualMotion(previousLocation, owner->GetActorLocation(), deltaSeconds);
	}
}
