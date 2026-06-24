#include "Scenario/Editor/ScenarioEditorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

AScenarioEditorPawn::AScenarioEditorPawn()
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

UPawnMovementComponent* AScenarioEditorPawn::GetMovementComponent() const
{
	return FloatingMovementComponent;
}

void AScenarioEditorPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyMovementSettings();
}

void AScenarioEditorPawn::ApplyMoveInput(float forwardValue, float rightValue, float upValue)
{
	if (!Controller) return;

	AddMovementInput(GetActorForwardVector(), forwardValue);
	AddMovementInput(GetActorRightVector(), rightValue);
	AddMovementInput(FVector::UpVector, upValue);
}

void AScenarioEditorPawn::ApplyWorldHeightInput(float upValue)
{
	if (!Controller || FMath::IsNearlyZero(upValue)) return;

	const UWorld* world = GetWorld();
	const double deltaSeconds = world ? world->GetDeltaSeconds() : 0.0;
	if (FMath::IsNearlyZero(deltaSeconds)) return;

	FVector location = GetActorLocation();
	location.Z += static_cast<double>(upValue) * MaxMoveSpeed * deltaSeconds;
	SetActorLocation(location);
}

void AScenarioEditorPawn::ApplyLookInput(float yawDeltaDegrees, float pitchDeltaDegrees)
{
	FRotator newRotation = GetActorRotation().GetNormalized();
	newRotation.Yaw += yawDeltaDegrees;
	newRotation.Pitch = FMath::Clamp(newRotation.Pitch + pitchDeltaDegrees, MinPitchDegrees, MaxPitchDegrees);
	newRotation.Roll = 0.0;
	SetActorRotation(newRotation);
}

void AScenarioEditorPawn::EnterTopDownView()
{
	if (bTopDownViewActive) return;
	if (!CameraComponent || !FloatingMovementComponent) return;

	SavedPerspectiveTransform = GetActorTransform();
	bTopDownViewActive = true;

	// pitch -90은 ApplyLookInput의 pitch clamp(-89~89) 밖이므로 직접 회전을 설정함.
	// yaw를 0으로 고정해 항상 north-up 정렬된 화면을 보장함(틀어진 화면 방지).
	const FRotator topDownRotation(-90.0, 0.0, 0.0);

	const FVector location = GetActorLocation();
	SetActorLocationAndRotation(
		FVector(location.X, location.Y, TopDownCameraHeightCm),
		topDownRotation);

	CurrentOrthoWidthCm = FMath::Clamp(TopDownOrthoWidthCm, TopDownOrthoWidthMinCm, TopDownOrthoWidthMaxCm);
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Orthographic);
	CameraComponent->SetOrthoWidth(CurrentOrthoWidthCm);

	FloatingMovementComponent->StopMovementImmediately();
	ApplyTopDownPanSpeed();
}

void AScenarioEditorPawn::EnterPerspectiveView()
{
	if (!bTopDownViewActive) return;
	if (!CameraComponent || !FloatingMovementComponent) return;

	bTopDownViewActive = false;
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
	SetActorTransform(SavedPerspectiveTransform);

	FloatingMovementComponent->StopMovementImmediately();
	ApplyMovementSettings();
}

void AScenarioEditorPawn::ApplyTopDownPanInput(float forwardValue, float rightValue)
{
	if (!Controller) return;

	// pitch -90에서는 forward vector가 지면을 향하므로 yaw 기준 수평 기저로 패닝함.
	const FRotationMatrix yawBasis(FRotator(0.0, GetActorRotation().Yaw, 0.0));
	AddMovementInput(yawBasis.GetUnitAxis(EAxis::X), forwardValue);
	AddMovementInput(yawBasis.GetUnitAxis(EAxis::Y), rightValue);
}

void AScenarioEditorPawn::ApplyTopDownDragPanInput(float rightValue, float upValue)
{
	if (!bTopDownViewActive) return;

	const FRotationMatrix yawBasis(FRotator(0.0, GetActorRotation().Yaw, 0.0));
	const double panCmPerInputUnit = CurrentOrthoWidthCm * TopDownDragPanSensitivity;
	const FVector panDelta =
		(yawBasis.GetUnitAxis(EAxis::Y) * -rightValue + yawBasis.GetUnitAxis(EAxis::X) * -upValue)
		* panCmPerInputUnit;
	AddActorWorldOffset(panDelta);
}

void AScenarioEditorPawn::ApplyTopDownZoomInput(float zoomValue)
{
	if (!bTopDownViewActive || !CameraComponent) return;
	if (FMath::IsNearlyZero(zoomValue)) return;

	const double zoomFactor = FMath::Pow(1.0 + TopDownZoomStepRatio, -static_cast<double>(zoomValue));
	CurrentOrthoWidthCm = FMath::Clamp(
		CurrentOrthoWidthCm * zoomFactor,
		TopDownOrthoWidthMinCm,
		TopDownOrthoWidthMaxCm);
	CameraComponent->SetOrthoWidth(CurrentOrthoWidthCm);
	ApplyTopDownPanSpeed();
}

void AScenarioEditorPawn::ApplyMovementSettings()
{
	if (!FloatingMovementComponent) return;

	FloatingMovementComponent->MaxSpeed = MaxMoveSpeed;
	FloatingMovementComponent->Acceleration = Acceleration;
	FloatingMovementComponent->Deceleration = Deceleration;
}

void AScenarioEditorPawn::ApplyTopDownPanSpeed()
{
	if (!FloatingMovementComponent) return;

	// zoom out할수록 화면상 체감 속도가 일정하도록 ortho width 비율로 스케일함.
	const double widthRatio = CurrentOrthoWidthCm / FMath::Max(TopDownOrthoWidthCm, 1.0);
	FloatingMovementComponent->MaxSpeed = MaxMoveSpeed * widthRatio;
	FloatingMovementComponent->Acceleration = Acceleration * widthRatio;
	FloatingMovementComponent->Deceleration = Deceleration * widthRatio;
}
