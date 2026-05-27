#include "Episode/Components/EpisodePathFollowerComponent.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UEpisodePathFollowerComponent::UEpisodePathFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEpisodePathFollowerComponent::SetSplineComponent(USplineComponent* InSplineComponent)
{
	SplineComponent = InSplineComponent;
}

void UEpisodePathFollowerComponent::StartFollowing()
{
	ResolveSplineComponent();
	SetComponentTickEnabled(SplineComponent != nullptr);
}

void UEpisodePathFollowerComponent::StopFollowing()
{
	SetComponentTickEnabled(false);
}

void UEpisodePathFollowerComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentDistanceCm = InitialDistanceCm;
	ResolveSplineComponent();
	InitializePathNoise();
	ConfigureCharacterMovementTickDependency();
	FreezeOwnedSplineTransform();
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

	if (TryMoveOwnerAlongSpline(SplineLength)) return;

	CurrentDistanceCm += SpeedCmPerSecond * static_cast<double>(DeltaTime);

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
			StopFollowing();
		}
	}

	MoveOwnerToCurrentDistance();
}

void UEpisodePathFollowerComponent::ResolveSplineComponent()
{
	if (!SplineComponent)
	{
		if (AActor* Owner = GetOwner())
		{
			SplineComponent = Owner->FindComponentByClass<USplineComponent>();
		}
	}
}

void UEpisodePathFollowerComponent::ConfigureCharacterMovementTickDependency()
{
	if (!bUseCharacterMovement) return;

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
	{
		CharacterMovement->AddTickPrerequisiteComponent(this);
	}
}

void UEpisodePathFollowerComponent::FreezeOwnedSplineTransform()
{
	if (!SplineComponent || SplineComponent->GetOwner() != GetOwner()) return;

	SplineComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
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

bool UEpisodePathFollowerComponent::TryMoveOwnerAlongSpline(double SplineLength)
{
	if (!bUseCharacterMovement || !SplineComponent) return false;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return false;

	UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();
	if (!CharacterMovement) return false;

	const FVector OwnerLocation = Character->GetActorLocation();
	const float ClosestInputKey = SplineComponent->FindInputKeyClosestToWorldLocation(OwnerLocation);
	CurrentDistanceCm = SplineComponent->GetDistanceAlongSplineAtSplineInputKey(ClosestInputKey);

	if (!bLoop && SplineLength - CurrentDistanceCm <= StopDistanceToleranceCm)
	{
		CharacterMovement->StopMovementImmediately();
		StopFollowing();
		return true;
	}

	double TargetDistanceCm = CurrentDistanceCm + FMath::Max(CharacterMovementLookAheadCm, SpeedCmPerSecond * 0.1);
	if (bLoop)
	{
		TargetDistanceCm = FMath::Fmod(TargetDistanceCm, SplineLength);
		if (TargetDistanceCm < 0.0)
		{
			TargetDistanceCm += SplineLength;
		}
	}
	else
	{
		TargetDistanceCm = FMath::Clamp(TargetDistanceCm, 0.0, SplineLength);
	}

	const FVector TargetLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(TargetDistanceCm),
		ESplineCoordinateSpace::World);
	const FVector NoisyTargetLocation = ApplyPathNoise(TargetDistanceCm, SplineLength, TargetLocation);
	FVector DesiredDirection = NoisyTargetLocation - OwnerLocation;
	DesiredDirection.Z = 0.0;

	if (DesiredDirection.IsNearlyZero())
	{
		DesiredDirection = SplineComponent->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(CurrentDistanceCm),
			ESplineCoordinateSpace::World);
		DesiredDirection.Z = 0.0;
	}

	const double MaxSpeed = CharacterMovement->GetMaxSpeed();
	const double RequestedSpeed = SpeedCmPerSecond * GetPathNoiseSpeedScale(SplineLength);
	const float MovementScale = MaxSpeed > KINDA_SMALL_NUMBER
		? static_cast<float>(FMath::Clamp(RequestedSpeed / MaxSpeed, 0.0, 1.0))
		: 1.0f;
	Character->AddMovementInput(DesiredDirection.GetSafeNormal(), MovementScale, true);

	if (bOrientToSpline)
	{
		CharacterMovement->bOrientRotationToMovement = true;
	}

	return true;
}

void UEpisodePathFollowerComponent::MoveOwnerToCurrentDistance()
{
	if (!SplineComponent) return;


	AActor* Owner = GetOwner();
	if (!Owner) return;


	const FVector Location = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(CurrentDistanceCm),
		ESplineCoordinateSpace::World);
	const FVector NoisyLocation = ApplyPathNoise(CurrentDistanceCm, SplineComponent->GetSplineLength(), Location);

	if (bOrientToSpline)
	{
		const FRotator Rotation = SplineComponent->GetRotationAtDistanceAlongSpline(
			static_cast<float>(CurrentDistanceCm),
			ESplineCoordinateSpace::World);
		Owner->SetActorLocationAndRotation(NoisyLocation, Rotation, false, nullptr, ETeleportType::None);
	}
	else
	{
		Owner->SetActorLocation(NoisyLocation, false, nullptr, ETeleportType::None);
	}
}
