
#include "Episode/Components/EpisodePathFollowerComponent.h"
#include "Components/SplineComponent.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Episode/Actors/EpisodeSplinePath.h"
#include "GameFramework/Actor.h"

UEpisodePathFollowerComponent::UEpisodePathFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEpisodePathFollowerComponent::SetSplineComponent(USplineComponent* InSplineComponent)
{
	SplineComponent = InSplineComponent;

	if (!SplineComponent)
	{
		StopFollowing();
	}
}

void UEpisodePathFollowerComponent::SetSplinePath(AEpisodeSplinePath* InSplinePath)
{
	if (!InSplinePath)
	{
		PathId.Reset();
		SplineComponent = nullptr;
		StopFollowing();
		return;
	}

	PathId = InSplinePath->PathId;
	SetSplineComponent(InSplinePath->SplineComponent);
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

	if (AEpisodePedestrian* Pedestrian = Cast<AEpisodePedestrian>(GetOwner()))
	{
		Pedestrian->ResetVisualMotion();
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

void UEpisodePathFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SplineComponent)
	{
		StopFollowing();
		return;
	}

	const double SplineLength = SplineComponent->GetSplineLength();
	if (SplineLength <= KINDA_SMALL_NUMBER)
	{
		StopFollowing();
		return;
	}

	CurrentDistanceCm += SpeedCmPerSecond * GetPathNoiseSpeedScale(SplineLength) * static_cast<double>(DeltaTime);
	bool bReachedEnd = false;
	if (bLoop)
	{
		CurrentDistanceCm = FMath::Fmod(CurrentDistanceCm, SplineLength);
		if (CurrentDistanceCm < 0.0)
		{
			CurrentDistanceCm += SplineLength;
		}
	}
	else
	{
		CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm, 0.0, SplineLength);
		if (FMath::IsNearlyEqual(CurrentDistanceCm, SplineLength))
		{
			bReachedEnd = true;
		}
	}
	MoveOwnerToCurrentDistance(DeltaTime);

	if (bReachedEnd)
	{
		StopFollowing();
	}
}

void UEpisodePathFollowerComponent::InitializePathNoise()
{
	FRandomStream NoiseStream(PathNoiseSeed);
	LateralNoisePhase = NoiseStream.FRandRange(-10000.0f, 10000.0f);
	SpeedNoisePhase = NoiseStream.FRandRange(-10000.0f, 10000.0f);
}

double UEpisodePathFollowerComponent::GetPathNoiseFade(double SplineLength) const
{
	if (bLoop || PathNoiseEndpointFadeDistanceCm <= KINDA_SMALL_NUMBER)
	{
		return 1.0;
	}

	const double StartFade = CurrentDistanceCm / PathNoiseEndpointFadeDistanceCm;
	const double EndFade = (SplineLength - CurrentDistanceCm) / PathNoiseEndpointFadeDistanceCm;
	return FMath::Clamp(FMath::Min(StartFade, EndFade), 0.0, 1.0);
}

double UEpisodePathFollowerComponent::GetPathNoiseSpeedScale(double SplineLength) const
{
	if (!bUseSeededPathNoise || SpeedNoiseStrength <= KINDA_SMALL_NUMBER || SpeedNoiseWavelengthCm <= KINDA_SMALL_NUMBER)
	{
		return 1.0;
	}

	const double Fade = GetPathNoiseFade(SplineLength);
	if (Fade <= KINDA_SMALL_NUMBER)
	{
		return 1.0;
	}

	const double NoiseInput = (CurrentDistanceCm / SpeedNoiseWavelengthCm) + SpeedNoisePhase;
	const double NoiseValue = FMath::PerlinNoise1D(static_cast<float>(NoiseInput));
	const double Strength = FMath::Clamp(SpeedNoiseStrength, 0.0, 0.95);
	return FMath::Clamp(1.0 + NoiseValue * Strength * Fade, 1.0 - Strength, 1.0 + Strength);
}

FVector UEpisodePathFollowerComponent::ApplyPathNoise(double DistanceCm, double SplineLength, const FVector& BaseLocation) const
{
	if (!bUseSeededPathNoise || !SplineComponent || LateralNoiseAmplitudeCm <= KINDA_SMALL_NUMBER || LateralNoiseWavelengthCm <= KINDA_SMALL_NUMBER)
	{
		return BaseLocation;
	}

	const double Fade = GetPathNoiseFade(SplineLength);
	if (Fade <= KINDA_SMALL_NUMBER)
	{
		return BaseLocation;
	}

	FVector Forward = SplineComponent->GetDirectionAtDistanceAlongSpline(
		static_cast<float>(DistanceCm),
		ESplineCoordinateSpace::World);
	Forward.Z = 0.0;

	if (!Forward.Normalize())
	{
		return BaseLocation;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const double NoiseInput = (DistanceCm / LateralNoiseWavelengthCm) + LateralNoisePhase;
	const double NoiseValue = FMath::PerlinNoise1D(static_cast<float>(NoiseInput));
	const double LateralOffsetCm = NoiseValue * LateralNoiseAmplitudeCm * Fade;
	return BaseLocation + Right * LateralOffsetCm;
}

void UEpisodePathFollowerComponent::MoveOwnerToCurrentDistance(double DeltaSeconds)
{
	if (!SplineComponent) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	const FVector PreviousLocation = Owner->GetActorLocation();
	const FVector Location = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(CurrentDistanceCm),
		ESplineCoordinateSpace::World);
	const FVector NoisyLocation = ApplyPathNoise(CurrentDistanceCm, SplineComponent->GetSplineLength(), Location);
	FVector TargetLocation = NoisyLocation;
	TargetLocation.Z += VerticalOffsetCm;

	if (bOrientToSpline)
	{
		const FRotator Rotation = SplineComponent->GetRotationAtDistanceAlongSpline(
			static_cast<float>(CurrentDistanceCm),
			ESplineCoordinateSpace::World);
		FHitResult SweepHit;
		Owner->SetActorLocationAndRotation(TargetLocation, Rotation, true, &SweepHit, ETeleportType::None);
	}
	else
	{
		FHitResult SweepHit;
		Owner->SetActorLocation(TargetLocation, true, &SweepHit, ETeleportType::None);
	}

	if (AEpisodePedestrian* Pedestrian = Cast<AEpisodePedestrian>(Owner))
	{
		Pedestrian->UpdateVisualMotion(PreviousLocation, Owner->GetActorLocation(), DeltaSeconds);
	}
}
