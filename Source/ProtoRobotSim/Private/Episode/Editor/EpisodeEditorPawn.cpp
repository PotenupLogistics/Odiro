#include "Episode/Editor/EpisodeEditorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

AEpisodeEditorPawn::AEpisodeEditorPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->bUsePawnControlRotation = false;

	FloatingMovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovementComponent"));
	FloatingMovementComponent->SetUpdatedComponent(SceneRoot);

	AutoPossessAI = EAutoPossessAI::Disabled;
}

UPawnMovementComponent* AEpisodeEditorPawn::GetMovementComponent() const
{
	return FloatingMovementComponent;
}

void AEpisodeEditorPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyMovementSettings();
}

void AEpisodeEditorPawn::ApplyMoveInput(float forwardValue, float rightValue, float upValue)
{
	if (!Controller) return;

	AddMovementInput(GetActorForwardVector(), forwardValue);
	AddMovementInput(GetActorRightVector(), rightValue);
	AddMovementInput(FVector::UpVector, upValue);
}

void AEpisodeEditorPawn::ApplyLookInput(float yawDeltaDegrees, float pitchDeltaDegrees)
{
	FRotator newRotation = GetActorRotation().GetNormalized();
	newRotation.Yaw += yawDeltaDegrees;
	newRotation.Pitch = FMath::Clamp(newRotation.Pitch + pitchDeltaDegrees, MinPitchDegrees, MaxPitchDegrees);
	newRotation.Roll = 0.0;
	SetActorRotation(newRotation);
}

void AEpisodeEditorPawn::ApplyMovementSettings()
{
	if (!FloatingMovementComponent) return;

	FloatingMovementComponent->MaxSpeed = MaxMoveSpeed;
	FloatingMovementComponent->Acceleration = Acceleration;
	FloatingMovementComponent->Deceleration = Deceleration;
}
