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

	if (TryMoveOwnerWithCharacterMovementInput(SplineLength))
	{
		return;
	}

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
	if (!bUseCharacterMovement)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
	{
		CharacterMovement->AddTickPrerequisiteComponent(this);
	}
}

void UEpisodePathFollowerComponent::FreezeOwnedSplineTransform()
{
	if (!bFreezeOwnedSplineOnBeginPlay || !SplineComponent || SplineComponent->GetOwner() != GetOwner())
	{
		return;
	}

	SplineComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}

bool UEpisodePathFollowerComponent::TryMoveOwnerWithCharacterMovementInput(double SplineLength)
{
	if (!bUseCharacterMovement || !SplineComponent)
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();
	if (!CharacterMovement)
	{
		return false;
	}

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
	FVector DesiredDirection = TargetLocation - OwnerLocation;
	DesiredDirection.Z = 0.0;

	if (DesiredDirection.IsNearlyZero())
	{
		DesiredDirection = SplineComponent->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(CurrentDistanceCm),
			ESplineCoordinateSpace::World);
		DesiredDirection.Z = 0.0;
	}

	const double MaxSpeed = CharacterMovement->GetMaxSpeed();
	const float MovementScale = MaxSpeed > KINDA_SMALL_NUMBER
		? static_cast<float>(FMath::Clamp(SpeedCmPerSecond / MaxSpeed, 0.0, 1.0))
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
	if (!SplineComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector Location = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(CurrentDistanceCm),
		ESplineCoordinateSpace::World);

	if (bOrientToSpline)
	{
		const FRotator Rotation = SplineComponent->GetRotationAtDistanceAlongSpline(
			static_cast<float>(CurrentDistanceCm),
			ESplineCoordinateSpace::World);
		Owner->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::None);
	}
	else
	{
		Owner->SetActorLocation(Location, false, nullptr, ETeleportType::None);
	}
}
