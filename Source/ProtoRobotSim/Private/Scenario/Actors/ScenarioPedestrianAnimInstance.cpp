
#include "Scenario/Actors/ScenarioPedestrianAnimInstance.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Components/ScenarioPedestrianRuntimeComponent.h"

namespace
{
	// 이 값 이하의 화면상 이동 속도는 정지 상태로 본다.
	constexpr float VisualMovingThresholdCmPerSecond = 3.0f;
	// 실제 화면상 이동 속도를 animation 변수로 부드럽게 따라가게 하는 보간 속도다.
	constexpr float VisualSpeedSmoothingRate = 12.0f;
	// 실제 이동 방향을 animation 방향 변수로 부드럽게 따라가게 하는 보간 속도다.
	constexpr float VisualDirectionSmoothingRate = 18.0f;
	// SkeletalMesh visual facing offset이 목표 방향을 따라가는 최대 회전 속도다.
	constexpr float VisualFacingTurnRateDegreesPerSecondDefault = 240.0f;
	// YieldSlowdown 상태에서 실제 이동 방향을 visual facing에 반영하는 비율이다.
	constexpr float VisualFacingSlowdownAlpha = 0.25f;
	// Sidestep 상태에서 실제 이동 방향을 visual facing에 반영하는 비율이다.
	constexpr float VisualFacingSidestepAlpha = 0.7f;
	// Recover 상태에서 실제 이동 방향을 visual facing에 반영하는 비율이다.
	constexpr float VisualFacingRecoverAlpha = 0.4f;
	// SkeletalMesh가 actor forward에서 벗어날 수 있는 최대 yaw offset이다.
	constexpr float MaxVisualFacingOffsetDegrees = 70.0f;
}

void UScenarioPedestrianAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedPedestrian = Cast<AScenarioPedestrian>(TryGetPawnOwner());
	RootMotionMode = ERootMotionMode::NoRootMotionExtraction;
	if (CachedPedestrian && CachedPedestrian->GetMesh())
	{
		InitialMeshRelativeRotation = CachedPedestrian->GetMesh()->GetRelativeRotation();
		bHasInitialMeshRelativeRotation = true;
	}
	ResetVisualMotion();
}

void UScenarioPedestrianAnimInstance::NativeUpdateAnimation(float deltaSeconds)
{
	Super::NativeUpdateAnimation(deltaSeconds);

	if (!CachedPedestrian)
	{
		CachedPedestrian = Cast<AScenarioPedestrian>(TryGetPawnOwner());
	}

	if (!CachedPedestrian)
	{
		ResetVisualMotion();
		return;
	}

	UpdateVisualMotionFromActor(deltaSeconds);
}

void UScenarioPedestrianAnimInstance::ResetVisualMotion()
{
	NominalSpeedCmPerSecond = 0.0f;
	ActualVisualSpeedCmPerSecond = 0.0f;
	bActuallyMoving = false;
	bMoving = false;
	ProgressSpeedCmPerSecond = 0.0f;
	LateralSpeedCmPerSecond = 0.0f;
	MoveDirectionDegrees = 0.0f;
	DesiredVisualFacingOffsetDegrees = 0.0f;
	VisualFacingOffsetDegrees = 0.0f;
	VisualFacingTurnRateDegreesPerSecond = 0.0f;
	PreviousActorLocation = CachedPedestrian ? CachedPedestrian->GetActorLocation() : FVector::ZeroVector;
	bHasPreviousActorLocation = CachedPedestrian != nullptr;
	ApplyVisualFacingOffset();
}

void UScenarioPedestrianAnimInstance::UpdateVisualMotionFromActor(float deltaSeconds)
{
	if (!CachedPedestrian)
	{
		ResetVisualMotion();
		return;
	}

	const FVector currentActorLocation = CachedPedestrian->GetActorLocation();
	if (!bHasPreviousActorLocation || deltaSeconds <= KINDA_SMALL_NUMBER)
	{
		PreviousActorLocation = currentActorLocation;
		bHasPreviousActorLocation = true;
		NominalSpeedCmPerSecond = 0.0f;
		ActualVisualSpeedCmPerSecond = 0.0f;
		bActuallyMoving = false;
		bMoving = false;
		ProgressSpeedCmPerSecond = 0.0f;
		LateralSpeedCmPerSecond = 0.0f;
		MoveDirectionDegrees = 0.0f;
		DesiredVisualFacingOffsetDegrees = 0.0f;
		VisualFacingOffsetDegrees = 0.0f;
		VisualFacingTurnRateDegreesPerSecond = 0.0f;
		ApplyVisualFacingOffset();
		return;
	}

	FVector deltaLocation = currentActorLocation - PreviousActorLocation;
	deltaLocation.Z = 0.0f;
	PreviousActorLocation = currentActorLocation;

	const FVector visualVelocityCmPerSecond = deltaLocation / deltaSeconds;
	const float rawActualVisualSpeedCmPerSecond = visualVelocityCmPerSecond.Size2D();
	ActualVisualSpeedCmPerSecond = FMath::FInterpTo(
		ActualVisualSpeedCmPerSecond,
		rawActualVisualSpeedCmPerSecond,
		deltaSeconds,
		VisualSpeedSmoothingRate);
	bActuallyMoving = ActualVisualSpeedCmPerSecond > VisualMovingThresholdCmPerSecond;
	bMoving = bActuallyMoving;

	const FVector moveDirection = deltaLocation.GetSafeNormal2D();
	if (!bActuallyMoving || moveDirection.IsNearlyZero())
	{
		MoveDirectionDegrees = FMath::FInterpTo(
			MoveDirectionDegrees,
			0.0f,
			deltaSeconds,
			VisualDirectionSmoothingRate);
		UpdateRuntimeAnimationBridge(
			visualVelocityCmPerSecond,
			CachedPedestrian->GetActorForwardVector().GetSafeNormal2D(),
			CachedPedestrian->GetActorRightVector().GetSafeNormal2D());
		UpdateVisualFacing(deltaSeconds);
		return;
	}

	const FVector forward = CachedPedestrian->GetActorForwardVector().GetSafeNormal2D();
	const FVector right = CachedPedestrian->GetActorRightVector().GetSafeNormal2D();
	const double forwardAmount = FVector::DotProduct(moveDirection, forward);
	const double rightAmount = FVector::DotProduct(moveDirection, right);
	const float rawDirectionDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(rightAmount, forwardAmount)));
	MoveDirectionDegrees = FMath::FInterpTo(
		MoveDirectionDegrees,
		rawDirectionDegrees,
		deltaSeconds,
		VisualDirectionSmoothingRate);
	UpdateRuntimeAnimationBridge(visualVelocityCmPerSecond, forward, right);
	UpdateVisualFacing(deltaSeconds);
}

void UScenarioPedestrianAnimInstance::UpdateRuntimeAnimationBridge(
	const FVector& visualVelocityCmPerSecond,
	const FVector& actorForward,
	const FVector& actorRight)
{
	if (const UScenarioPedestrianRuntimeComponent* runtimeComponent = CachedPedestrian ? CachedPedestrian->PedestrianRuntimeComponent : nullptr;
		runtimeComponent && runtimeComponent->HasPlan())
	{
		NominalSpeedCmPerSecond = static_cast<float>(runtimeComponent->NominalSpeedCmPerSecond);
		ProgressSpeedCmPerSecond = static_cast<float>(runtimeComponent->ProgressSpeedCmPerSecond);
		LateralSpeedCmPerSecond = static_cast<float>(runtimeComponent->LateralSpeedCmPerSecond);
		return;
	}

	NominalSpeedCmPerSecond = visualVelocityCmPerSecond.Size2D();
	ProgressSpeedCmPerSecond = static_cast<float>(FVector::DotProduct(visualVelocityCmPerSecond, actorForward));
	LateralSpeedCmPerSecond = static_cast<float>(FVector::DotProduct(visualVelocityCmPerSecond, actorRight));
}

void UScenarioPedestrianAnimInstance::UpdateVisualFacing(float deltaSeconds)
{
	float facingAlpha = 0.0f;
	if (const UScenarioPedestrianRuntimeComponent* runtimeComponent = CachedPedestrian ? CachedPedestrian->PedestrianRuntimeComponent : nullptr;
		runtimeComponent && runtimeComponent->HasPlan())
	{
		switch (runtimeComponent->CurrentState)
		{
		case EScenarioPedestrianRuntimeState::YieldSlowdown:
			facingAlpha = VisualFacingSlowdownAlpha;
			break;
		case EScenarioPedestrianRuntimeState::Sidestep:
			facingAlpha = VisualFacingSidestepAlpha;
			break;
		case EScenarioPedestrianRuntimeState::Recover:
			facingAlpha = VisualFacingRecoverAlpha;
			break;
		case EScenarioPedestrianRuntimeState::YieldStop:
		case EScenarioPedestrianRuntimeState::Blocked:
		case EScenarioPedestrianRuntimeState::FollowBaseline:
		default:
			facingAlpha = 0.0f;
			break;
		}
	}

	DesiredVisualFacingOffsetDegrees = FMath::Clamp(
		MoveDirectionDegrees * facingAlpha,
		-MaxVisualFacingOffsetDegrees,
		MaxVisualFacingOffsetDegrees);

	const float previousFacingOffsetDegrees = VisualFacingOffsetDegrees;
	VisualFacingOffsetDegrees = FMath::FInterpConstantTo(
		VisualFacingOffsetDegrees,
		DesiredVisualFacingOffsetDegrees,
		deltaSeconds,
		VisualFacingTurnRateDegreesPerSecondDefault);
	VisualFacingTurnRateDegreesPerSecond = deltaSeconds > KINDA_SMALL_NUMBER
		? FMath::Abs(VisualFacingOffsetDegrees - previousFacingOffsetDegrees) / deltaSeconds
		: 0.0f;

	ApplyVisualFacingOffset();
}

void UScenarioPedestrianAnimInstance::ApplyVisualFacingOffset()
{
	if (!CachedPedestrian || !CachedPedestrian->GetMesh())
	{
		return;
	}

	USkeletalMeshComponent* meshComponent = CachedPedestrian->GetMesh();
	if (!bHasInitialMeshRelativeRotation)
	{
		InitialMeshRelativeRotation = meshComponent->GetRelativeRotation();
		bHasInitialMeshRelativeRotation = true;
	}

	FRotator targetRelativeRotation = InitialMeshRelativeRotation;
	targetRelativeRotation.Yaw += VisualFacingOffsetDegrees;
	meshComponent->SetRelativeRotation(targetRelativeRotation);
}
