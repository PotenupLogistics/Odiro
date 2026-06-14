#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ScenarioPedestrianAnimInstance.generated.h"

class AScenarioPedestrian;

UCLASS()
class ODIROSIM_API UScenarioPedestrianAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float deltaSeconds) override;

	// Baseline plan speed. 로봇 회피나 sidestep 때문에 줄거나 늘지 않는 기준 속도.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "cm/s"))
	float NominalSpeedCmPerSecond = 0.0f;

	// Animation frame에서 actor 위치 delta로 계산한 실제 화면상 이동 속도.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "cm/s"))
	float ActualVisualSpeedCmPerSecond = 0.0f;

	// 실제 화면 이동 속도가 threshold를 넘는지 여부. locomotion 전환 조건에 사용한다.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual")
	bool bActuallyMoving = false;

	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual")
	bool bMoving = false;

	// Baseline path를 따라 전진하는 속도. 감속/정지 state의 speed scale이 반영.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "cm/s"))
	float ProgressSpeedCmPerSecond = 0.0f;

	// Baseline path 기준 좌우 회피/복귀 속도. 좌우 방향은 부호로 구분.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "cm/s"))
	float LateralSpeedCmPerSecond = 0.0f;

	// Actor forward 기준 실제 이동 방향. sidestep/recover blendspace 입력에 사용.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "deg"))
	float MoveDirectionDegrees = 0.0f;

	// SkeletalMesh에 적용 중인 visual yaw offset. actor/capsule yaw는 baseline 방향을 유지.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "deg"))
	float VisualFacingOffsetDegrees = 0.0f;

	// 현재 state와 이동 방향에서 계산한 목표 visual yaw offset.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "deg"))
	float DesiredVisualFacingOffsetDegrees = 0.0f;

	// VisualFacingOffsetDegrees가 목표 offset을 따라가는 회전 속도.
	UPROPERTY(BlueprintReadOnly, Category="Scenario|Visual", meta = (Units = "deg/s"))
	float VisualFacingTurnRateDegreesPerSecond = 0.0f;

private:
	void ResetVisualMotion();
	void UpdateVisualMotionFromActor(float deltaSeconds);
	void UpdateRuntimeAnimationBridge(const FVector& visualVelocityCmPerSecond, const FVector& actorForward, const FVector& actorRight);
	void UpdateVisualFacing(float deltaSeconds);
	void ApplyVisualFacingOffset();

	UPROPERTY(Transient)
	TObjectPtr<AScenarioPedestrian> CachedPedestrian;

	FVector PreviousActorLocation = FVector::ZeroVector;
	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;
	bool bHasPreviousActorLocation = false;
	bool bHasInitialMeshRelativeRotation = false;
};
