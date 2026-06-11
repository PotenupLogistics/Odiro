
#include "Scenario/Components/ScenarioPedestrianRuntimeComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioPedestrianRuntime, Log, All);

namespace
{
	// 보행자 충돌 예측에 사용하는 단순 몸통 반경이다.
	constexpr double PedestrianBodyRadiusCm = 35.0;
	// 로봇-보행자 충돌 예측을 샘플링하는 시간 간격이다.
	constexpr double ConflictSampleStepSeconds = 0.1;
	// 지연 계산에서 0으로 나누는 것을 막기 위한 최소 진행 속도다.
	constexpr double MinRuntimeSpeedCmPerSecond = 1.0;
	// YieldSlowdown 상태에서 허용하는 최소 속도 scale이다.
	constexpr double MinSlowdownSpeedScale = 0.15;
	// Sidestep 회피를 고려하기 시작하는 evasiveness 임계값이다.
	constexpr double SidestepActivationThreshold = 0.45;
	// soft conflict가 해제되기 전까지 유지하는 거리 여유값이다.
	constexpr double SoftConflictExitHysteresisCm = 45.0;
	// hard conflict가 해제되기 전까지 유지하는 거리 여유값이다.
	constexpr double HardConflictExitHysteresisCm = 30.0;
	// 상태 흔들림을 줄이기 위한 기본 최소 state 유지 시간이다.
	constexpr double MinimumStateHoldSeconds = 0.35;
	// Sidestep 상태가 너무 빨리 풀리지 않게 하는 최소 유지 시간이다.
	constexpr double SidestepMinimumStateHoldSeconds = 2.0;
	// YieldStop 상태가 한 프레임성 정지로 보이지 않게 하는 최소 유지 시간이다.
	constexpr double YieldStopMinimumStateHoldSeconds = 0.85;
	// Recover 상태가 baseline 복귀 동작으로 보이도록 유지하는 최소 시간이다.
	constexpr double RecoverMinimumStateHoldSeconds = 1.0;
	// conflict가 해제된 뒤에도 clear 상태가 안정적으로 유지되어야 하는 시간이다.
	constexpr double ConflictClearHoldSeconds = 0.65;
	// sidestep/recover lateral curve의 기준 이동 속도다.
	constexpr double LateralCurveSpeedCmPerSecond = 80.0;
	// actor yaw가 목표 이동 방향을 따라가는 보간 속도다.
	constexpr double ActorRotationInterpSpeed = 6.0;
	// lateral curve가 지나치게 짧아지지 않도록 하는 최소 재생 시간이다.
	constexpr double MinimumLateralCurveDurationSeconds = 0.65;
	// lateral curve가 지나치게 길어지지 않도록 하는 최대 재생 시간이다.
	constexpr double MaximumLateralCurveDurationSeconds = 1.75;
	// actor velocity가 비어 있을 때 위치 변화 기반 속도를 신뢰하기 시작하는 기준이다.
	constexpr double ObservedRobotVelocityMinCmPerSecond = 5.0;
	// sidestep 목표가 로봇 안전 반경을 살짝 더 벗어나도록 더하는 여유 거리다.
	constexpr double SidestepClearanceMarginCm = 25.0;
	// JSON sidestep 값이 작아도 평가 장치 보행자가 회피 가능하도록 허용하는 내부 최대 offset이다.
	constexpr double AutomaticSidestepLimitCm = 220.0;
	// 로봇이 baseline 중앙에 가까울 때 방향 선택을 보류하는 lateral 오차 범위다.
	constexpr double CenterlineSideEpsilonCm = 5.0;
	// sidestep 후보 평가 점수가 이 값 안이면 deterministic fallback으로 tie-break한다.
	constexpr double SidestepCandidateScoreTieTolerance = 5.0;
	// 보행자가 로봇 전방 가까이 끼어드는 후보에 적용하는 penalty scale이다.
	constexpr double CutInFrontPenaltyScale = 0.75;

	FString GetPedestrianRuntimeStateName(EScenarioPedestrianRuntimeState state)
	{
		const UEnum* enumPtr = StaticEnum<EScenarioPedestrianRuntimeState>();
		if (!enumPtr) return TEXT("Unknown");

		return enumPtr->GetNameStringByValue(static_cast<int64>(state));
	}

	double SmootherStep(double alpha)
	{
		const double clampedAlpha = FMath::Clamp(alpha, 0.0, 1.0);
		return clampedAlpha * clampedAlpha * clampedAlpha * (clampedAlpha * (clampedAlpha * 6.0 - 15.0) + 10.0);
	}

	int32 GetRuntimeStatePriority(EScenarioPedestrianRuntimeState state)
	{
		switch (state)
		{
		case EScenarioPedestrianRuntimeState::YieldSlowdown:
			return 1;
		case EScenarioPedestrianRuntimeState::Sidestep:
			return 2;
		case EScenarioPedestrianRuntimeState::YieldStop:
			return 3;
		case EScenarioPedestrianRuntimeState::Blocked:
			return 4;
		case EScenarioPedestrianRuntimeState::Recover:
		case EScenarioPedestrianRuntimeState::FollowBaseline:
		default:
			return 0;
		}
	}
}

UScenarioPedestrianRuntimeComponent::UScenarioPedestrianRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UScenarioPedestrianRuntimeComponent::ConfigurePlan(
	const FString& inInstanceId,
	const FScenarioPedestrianPlan& plan,
	double fallbackSpeedCmPerSecond,
	double initialDistanceCm,
	bool bStartAutomatically)
{
	InstanceId = inInstanceId;
	PlanId = plan.PlanId;
	PlanHash = plan.PlanHash;
	BehaviorHash = plan.BehaviorHash;
	PedestrianScenarioHash = plan.PedestrianScenarioHash;
	BehaviorParams = plan.BehaviorParams;
	PlanPoints = plan.Points;
	SpeedCmPerSecond = fallbackSpeedCmPerSecond;
	if (PlanPoints.Num() > 0)
	{
		SpeedCmPerSecond = FMath::Max(PlanPoints[0].SpeedCmPerSecond, 1.0);
	}
	InitialDistanceCm = FMath::Max(0.0, initialDistanceCm);
	CurrentDistanceCm = InitialDistanceCm;
	TotalDistanceCm = PlanPoints.Num() > 0 ? PlanPoints.Last().DistanceCm : 0.0;
	NominalSpeedCmPerSecond = SpeedCmPerSecond;
	ProgressSpeedCmPerSecond = SpeedCmPerSecond;
	LateralSpeedCmPerSecond = 0.0;
	bAutoStart = bStartAutomatically;
	ResetRuntimeMetrics();
}

void UScenarioPedestrianRuntimeComponent::SetRobotActor(AActor* inRobotActor)
{
	RobotActor = inRobotActor;
	CachedRobotRadiusCm = GetActorRadiusCm(inRobotActor);
	LastRobotLocation = FVector::ZeroVector;
	ObservedRobotVelocityCmPerSecond = FVector::ZeroVector;
	bHasLastRobotLocation = false;
}

void UScenarioPedestrianRuntimeComponent::StartFollowing()
{
	if (!HasPlan())
	{
		StopFollowing();
		return;
	}

	CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm, 0.0, TotalDistanceCm);
	ResetRuntimeMetrics();
	MoveOwnerToCurrentDistance(0.0, true);
	SetComponentTickEnabled(true);
}

void UScenarioPedestrianRuntimeComponent::StopFollowing()
{
	SetComponentTickEnabled(false);
}

bool UScenarioPedestrianRuntimeComponent::HasPlan() const
{
	return PlanPoints.Num() >= 2 && TotalDistanceCm > KINDA_SMALL_NUMBER;
}

void UScenarioPedestrianRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentDistanceCm = InitialDistanceCm;
	MoveOwnerToCurrentDistance(0.0, true);

	if (bAutoStart)
	{
		StartFollowing();
	}
}

void UScenarioPedestrianRuntimeComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaTime, tickType, thisTickFunction);

	if (!HasPlan())
	{
		StopFollowing();
		return;
	}

	const double deltaSeconds = deltaTime;
	ActiveTimeSeconds += FMath::Max(deltaSeconds, 0.0);
	StateElapsedSeconds += FMath::Max(deltaSeconds, 0.0);
	UpdateRobotDistanceMetrics();
	UpdateRobotKinematics(deltaSeconds);

	const FRobotConflict conflict = FindMostSevereRobotConflict();
	UpdateRuntimeState(conflict, deltaSeconds);
	const double speedScale = ComputeSpeedScale(conflict);
	const double previousLateralOffsetCm = ActualLateralOffsetCm;
	UpdateLateralOffset(deltaSeconds);
	UpdateAnimationKinematics(speedScale, previousLateralOffsetCm, deltaSeconds);

	if (CurrentState == EScenarioPedestrianRuntimeState::YieldStop
		|| CurrentState == EScenarioPedestrianRuntimeState::Blocked)
	{
		ForcedWaitSeconds += deltaSeconds;
	}

	if (CurrentState == EScenarioPedestrianRuntimeState::Blocked)
	{
		BlockedDurationSeconds += deltaSeconds;
	}

	const double distanceDeltaCm = FMath::Max(SpeedCmPerSecond, 0.0) * speedScale * deltaSeconds;
	CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm + distanceDeltaCm, 0.0, TotalDistanceCm);
	MoveOwnerToCurrentDistance(deltaSeconds);
	UpdateScheduleDelay();
	
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.1f, FColor::Red, FString::Printf(TEXT("CurrentState: %s"), *GetPedestrianRuntimeStateName(CurrentState)));

	if (FMath::IsNearlyEqual(CurrentDistanceCm, TotalDistanceCm))
	{
		StopFollowing();
	}
}

FVector UScenarioPedestrianRuntimeComponent::GetLocationAtDistance(double distanceCm) const
{
	if (PlanPoints.IsEmpty()) return FVector::ZeroVector;
	if (distanceCm <= 0.0) return PlanPoints[0].Location;
	if (distanceCm >= TotalDistanceCm) return PlanPoints.Last().Location;

	for (int32 pointIndex = 0; pointIndex < PlanPoints.Num() - 1; ++pointIndex)
	{
		const FScenarioPedestrianPlanPoint& segmentStart = PlanPoints[pointIndex];
		const FScenarioPedestrianPlanPoint& segmentEnd = PlanPoints[pointIndex + 1];
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

FVector UScenarioPedestrianRuntimeComponent::GetDirectionAtDistance(double distanceCm) const
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

FVector UScenarioPedestrianRuntimeComponent::GetRightAtDistance(double distanceCm) const
{
	FVector direction = GetDirectionAtDistance(distanceCm);
	direction.Z = 0.0;
	if (!direction.Normalize())
	{
		return FVector::RightVector;
	}

	const FVector right = FVector::CrossProduct(FVector::UpVector, direction).GetSafeNormal();
	return right.IsNearlyZero() ? FVector::RightVector : right;
}

FVector UScenarioPedestrianRuntimeComponent::GetActualLocationAtDistance(double distanceCm, double lateralOffsetCm) const
{
	return GetLocationAtDistance(distanceCm) + GetRightAtDistance(distanceCm) * lateralOffsetCm;
}

void UScenarioPedestrianRuntimeComponent::MoveOwnerToCurrentDistance(double deltaSeconds, bool bSnapRotation)
{
	AActor* owner = GetOwner();
	if (!owner || !HasPlan()) return;

	const FVector previousLocation = owner->GetActorLocation();
	FVector targetLocation = GetActualLocationAtDistance(CurrentDistanceCm, ActualLateralOffsetCm);
	targetLocation.Z += VerticalOffsetCm;

	FVector facingDirection = targetLocation - previousLocation;
	facingDirection.Z = 0.0;
	if (!facingDirection.Normalize())
	{
		facingDirection = GetDirectionAtDistance(CurrentDistanceCm);
	}

	const FRotator targetRotation = facingDirection.IsNearlyZero()
		? owner->GetActorRotation()
		: facingDirection.Rotation();
	const FRotator nextRotation = bSnapRotation || deltaSeconds <= KINDA_SMALL_NUMBER
		? targetRotation
		: FMath::RInterpTo(
			owner->GetActorRotation(),
			targetRotation,
			static_cast<float>(deltaSeconds),
			ActorRotationInterpSpeed);

	FHitResult sweepHit;
	owner->SetActorLocationAndRotation(targetLocation, nextRotation, true, &sweepHit, ETeleportType::None);

	PathDeviationCm = FMath::Abs(ActualLateralOffsetCm);
	MaxPathDeviationCm = FMath::Max(MaxPathDeviationCm, PathDeviationCm);
}

UScenarioPedestrianRuntimeComponent::FRobotConflict UScenarioPedestrianRuntimeComponent::FindMostSevereRobotConflict() const
{
	FRobotConflict bestConflict;
	AActor* robotActor = RobotActor.Get();
	if (!bEnableRobotReaction || !HasPlan() || !IsValid(robotActor))
	{
		return bestConflict;
	}

	const AActor* owner = GetOwner();
	if (!owner || robotActor == owner)
	{
		return bestConflict;
	}

	const double horizonSeconds = FMath::Max(BehaviorParams.AwarenessHorizonSeconds, 0.0);
	if (horizonSeconds <= KINDA_SMALL_NUMBER)
	{
		return bestConflict;
	}

	const double robotRadiusCm = CachedRobotRadiusCm > KINDA_SMALL_NUMBER
		? CachedRobotRadiusCm
		: GetActorRadiusCm(robotActor);
	const double stopDistanceCm = GetConflictStopDistanceCm(robotRadiusCm);
	const double warningDistanceCm = GetConflictWarningDistanceCm(robotRadiusCm);
	const FVector robotLocation = robotActor->GetActorLocation();
	const FVector robotVelocity = ResolveRobotVelocity(robotActor);
	const double predictionProgressSpeedCmPerSecond = FMath::Max(ProgressSpeedCmPerSecond, 0.0);

	double closestDistanceCm = TNumericLimits<double>::Max();
	double closestTimeSeconds = 0.0;
	double closestPedestrianDistanceCm = CurrentDistanceCm;
	FVector closestRobotLocation = robotLocation;
	FVector closestPedestrianLocation = GetActualLocationAtDistance(CurrentDistanceCm, ActualLateralOffsetCm);

	const int32 sampleCount = FMath::Max(1, FMath::CeilToInt(horizonSeconds / ConflictSampleStepSeconds));
	for (int32 sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const double sampleTimeSeconds = FMath::Min(sampleIndex * ConflictSampleStepSeconds, horizonSeconds);
		const double predictedPedestrianDistanceCm = FMath::Clamp(
			CurrentDistanceCm + predictionProgressSpeedCmPerSecond * sampleTimeSeconds,
			0.0,
			TotalDistanceCm);
		const FVector pedestrianLocation = GetActualLocationAtDistance(predictedPedestrianDistanceCm, ActualLateralOffsetCm);
		const FVector predictedRobotLocation = PredictRobotLocation(
			robotLocation,
			robotVelocity,
			sampleTimeSeconds);
		const double distanceCm = FVector::Dist2D(pedestrianLocation, predictedRobotLocation);

		if (distanceCm < closestDistanceCm)
		{
			closestDistanceCm = distanceCm;
			closestTimeSeconds = sampleTimeSeconds;
			closestPedestrianDistanceCm = predictedPedestrianDistanceCm;
			closestRobotLocation = predictedRobotLocation;
			closestPedestrianLocation = pedestrianLocation;
		}
	}

	const bool bHasConflict = closestDistanceCm <= warningDistanceCm;
	const bool bHardConflict = closestDistanceCm <= stopDistanceCm;
	const double distanceSeverity = warningDistanceCm <= stopDistanceCm + KINDA_SMALL_NUMBER
		? 1.0
		: 1.0 - FMath::Clamp((closestDistanceCm - stopDistanceCm) / (warningDistanceCm - stopDistanceCm), 0.0, 1.0);
	const double timeSeverity = 1.0 - FMath::Clamp(closestTimeSeconds / horizonSeconds, 0.0, 1.0);
	const double severity = FMath::Clamp(distanceSeverity * 0.75 + timeSeverity * 0.25, 0.0, 1.0);

	bestConflict.RobotActor = robotActor;
	bestConflict.RobotLocation = robotLocation;
	bestConflict.RobotVelocity = robotVelocity;
	bestConflict.ClosestRobotLocation = closestRobotLocation;
	bestConflict.ClosestPedestrianLocation = closestPedestrianLocation;
	bestConflict.RobotRadiusCm = robotRadiusCm;
	bestConflict.WarningDistanceCm = warningDistanceCm;
	bestConflict.StopDistanceCm = stopDistanceCm;
	bestConflict.TimeToClosestSeconds = closestTimeSeconds;
	bestConflict.ClosestDistanceCm = closestDistanceCm;
	bestConflict.PedestrianDistanceAtClosestCm = closestPedestrianDistanceCm;
	bestConflict.Severity = severity;
	bestConflict.bHasConflict = bHasConflict;
	bestConflict.bHardConflict = bHardConflict;
	return bestConflict;
}

void UScenarioPedestrianRuntimeComponent::UpdateRobotDistanceMetrics()
{
	const AActor* owner = GetOwner();
	const AActor* robotActor = RobotActor.Get();
	if (!owner || !IsValid(robotActor) || robotActor == owner)
	{
		return;
	}

	const double distanceCm = FVector::Dist2D(owner->GetActorLocation(), robotActor->GetActorLocation());
	if (MinRobotDistanceCm < 0.0 || distanceCm < MinRobotDistanceCm)
	{
		MinRobotDistanceCm = distanceCm;
	}
}

void UScenarioPedestrianRuntimeComponent::UpdateRobotKinematics(double deltaSeconds)
{
	const AActor* robotActor = RobotActor.Get();
	if (!IsValid(robotActor))
	{
		LastRobotLocation = FVector::ZeroVector;
		ObservedRobotVelocityCmPerSecond = FVector::ZeroVector;
		bHasLastRobotLocation = false;
		return;
	}

	const FVector currentRobotLocation = robotActor->GetActorLocation();
	if (bHasLastRobotLocation && deltaSeconds > KINDA_SMALL_NUMBER)
	{
		ObservedRobotVelocityCmPerSecond = (currentRobotLocation - LastRobotLocation) / deltaSeconds;
		ObservedRobotVelocityCmPerSecond.Z = 0.0;
	}
	else
	{
		ObservedRobotVelocityCmPerSecond = FVector::ZeroVector;
	}

	LastRobotLocation = currentRobotLocation;
	bHasLastRobotLocation = true;
}

FVector UScenarioPedestrianRuntimeComponent::ResolveRobotVelocity(const AActor* robotActor) const
{
	if (!IsValid(robotActor))
	{
		return FVector::ZeroVector;
	}

	FVector observedVelocity = ObservedRobotVelocityCmPerSecond;
	observedVelocity.Z = 0.0;
	if (observedVelocity.Size2D() >= ObservedRobotVelocityMinCmPerSecond)
	{
		return observedVelocity;
	}

	FVector actorVelocity = robotActor->GetVelocity();
	actorVelocity.Z = 0.0;
	return actorVelocity;
}

FVector UScenarioPedestrianRuntimeComponent::PredictRobotLocation(
	const FVector& robotLocation,
	const FVector& robotVelocity,
	double sampleTimeSeconds) const
{
	return robotLocation + robotVelocity * sampleTimeSeconds;
}

double UScenarioPedestrianRuntimeComponent::GetActorRadiusCm(const AActor* actor) const
{
	if (!IsValid(actor))
	{
		return 50.0;
	}

	FVector origin = FVector::ZeroVector;
	FVector extent = FVector::ZeroVector;
	actor->GetActorBounds(true, origin, extent);
	const double radiusCm = FMath::Max(extent.X, extent.Y);
	return radiusCm > KINDA_SMALL_NUMBER ? radiusCm : 50.0;
}

double UScenarioPedestrianRuntimeComponent::GetConflictStopDistanceCm(double robotRadiusCm) const
{
	const double cooperationLeadCm = FMath::Lerp(-20.0, 45.0, BehaviorParams.Cooperation);
	return FMath::Max(0.0, BehaviorParams.PersonalSpaceCm + PedestrianBodyRadiusCm + robotRadiusCm + cooperationLeadCm);
}

double UScenarioPedestrianRuntimeComponent::GetConflictWarningDistanceCm(double robotRadiusCm) const
{
	const double warningLeadCm = FMath::Lerp(60.0, 180.0, BehaviorParams.Cooperation);
	return GetConflictStopDistanceCm(robotRadiusCm) + warningLeadCm;
}

double UScenarioPedestrianRuntimeComponent::GetSidestepLimitCm() const
{
	return FMath::Max(BehaviorParams.SidestepDistanceCm, AutomaticSidestepLimitCm);
}

bool UScenarioPedestrianRuntimeComponent::ShouldSidestepForConflict(const FRobotConflict& conflict) const
{
	if (!conflict.bHasConflict || BehaviorParams.SidestepDistanceCm <= KINDA_SMALL_NUMBER) return false;
	if (BehaviorParams.Evasiveness < SidestepActivationThreshold) return false;

	return conflict.Severity >= FMath::Lerp(0.8, 0.35, BehaviorParams.Evasiveness);
}

double UScenarioPedestrianRuntimeComponent::ComputeSidestepTargetForSide(
	const FRobotConflict& conflict,
	int32 side) const
{
	const double referenceDistanceCm = FMath::Clamp(conflict.PedestrianDistanceAtClosestCm, 0.0, TotalDistanceCm);
	const FVector baselineLocation = GetLocationAtDistance(referenceDistanceCm);
	const FVector right = GetRightAtDistance(referenceDistanceCm);
	const double robotLateralOffsetCm = FVector::DotProduct(conflict.ClosestRobotLocation - baselineLocation, right);
	const int32 normalizedSide = side < 0 ? -1 : 1;

	const double requiredClearanceCm = conflict.StopDistanceCm + SidestepClearanceMarginCm;
	const double clearanceTargetOffsetCm = robotLateralOffsetCm + static_cast<double>(normalizedSide) * requiredClearanceCm;
	const double preferredTargetOffsetCm = static_cast<double>(normalizedSide) * BehaviorParams.SidestepDistanceCm;
	double targetOffsetCm = FMath::Abs(clearanceTargetOffsetCm) > FMath::Abs(preferredTargetOffsetCm)
		? clearanceTargetOffsetCm
		: preferredTargetOffsetCm;

	if (targetOffsetCm * static_cast<double>(normalizedSide) <= 0.0)
	{
		targetOffsetCm = preferredTargetOffsetCm;
	}

	return FMath::Clamp(targetOffsetCm, -GetSidestepLimitCm(), GetSidestepLimitCm());
}

int32 UScenarioPedestrianRuntimeComponent::ComputeDefaultSidestepSide(const FRobotConflict& conflict) const
{
	const double referenceDistanceCm = FMath::Clamp(conflict.PedestrianDistanceAtClosestCm, 0.0, TotalDistanceCm);
	const FVector baselineLocation = GetLocationAtDistance(referenceDistanceCm);
	const FVector right = GetRightAtDistance(referenceDistanceCm);
	const double robotLateralOffsetCm = FVector::DotProduct(conflict.ClosestRobotLocation - baselineLocation, right);
	if (FMath::Abs(robotLateralOffsetCm) > CenterlineSideEpsilonCm)
	{
		return robotLateralOffsetCm >= 0.0 ? -1 : 1;
	}

	const double robotLateralVelocityCmPerSecond = FVector::DotProduct(conflict.RobotVelocity, right);
	if (FMath::Abs(robotLateralVelocityCmPerSecond) > ObservedRobotVelocityMinCmPerSecond)
	{
		return robotLateralVelocityCmPerSecond >= 0.0 ? -1 : 1;
	}

	const FString sideSeed = InstanceId.IsEmpty() ? PlanHash : InstanceId;
	return GetTypeHash(sideSeed) % 2 == 0 ? -1 : 1;
}

UScenarioPedestrianRuntimeComponent::FSidestepCandidate UScenarioPedestrianRuntimeComponent::EvaluateSidestepCandidate(
	const FRobotConflict& conflict,
	int32 side) const
{
	FSidestepCandidate candidate;
	candidate.Side = side < 0 ? -1 : 1;
	candidate.TargetOffsetCm = ComputeSidestepTargetForSide(conflict, candidate.Side);
	candidate.bValid = HasPlan() && conflict.RobotActor.IsValid();
	if (!candidate.bValid)
	{
		return candidate;
	}

	const double horizonSeconds = FMath::Max(BehaviorParams.AwarenessHorizonSeconds, ConflictSampleStepSeconds);
	const int32 sampleCount = FMath::Max(1, FMath::CeilToInt(horizonSeconds / ConflictSampleStepSeconds));
	const double speedScale = FMath::Clamp(
		FMath::Lerp(0.75, 0.45, BehaviorParams.Cooperation),
		MinSlowdownSpeedScale,
		1.0);
	const double predictionProgressSpeedCmPerSecond = FMath::Max(SpeedCmPerSecond, 0.0) * speedScale;
	const double lateralCurveDurationSeconds = ComputeLateralCurveDurationSeconds(
		ActualLateralOffsetCm,
		candidate.TargetOffsetCm);
	const FVector robotForward = conflict.RobotVelocity.GetSafeNormal2D();

	double cutInFrontPenalty = 0.0;
	double endDistanceCm = candidate.MinDistanceCm;
	for (int32 sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const double sampleTimeSeconds = FMath::Min(sampleIndex * ConflictSampleStepSeconds, horizonSeconds);
		const double predictedPedestrianDistanceCm = FMath::Clamp(
			CurrentDistanceCm + predictionProgressSpeedCmPerSecond * sampleTimeSeconds,
			0.0,
			TotalDistanceCm);

		double predictedLateralOffsetCm = candidate.TargetOffsetCm;
		if (lateralCurveDurationSeconds > KINDA_SMALL_NUMBER)
		{
			const double lateralAlpha = FMath::Clamp(sampleTimeSeconds / lateralCurveDurationSeconds, 0.0, 1.0);
			predictedLateralOffsetCm = FMath::Lerp(
				ActualLateralOffsetCm,
				candidate.TargetOffsetCm,
				SmootherStep(lateralAlpha));
		}

		const FVector pedestrianLocation = GetActualLocationAtDistance(
			predictedPedestrianDistanceCm,
			predictedLateralOffsetCm);
		const FVector robotLocation = PredictRobotLocation(
			conflict.RobotLocation,
			conflict.RobotVelocity,
			sampleTimeSeconds);
		const double distanceCm = FVector::Dist2D(pedestrianLocation, robotLocation);
		candidate.MinDistanceCm = FMath::Min(candidate.MinDistanceCm, distanceCm);
		endDistanceCm = distanceCm;

		if (!robotForward.IsNearlyZero())
		{
			const FVector relativePedestrianLocation = pedestrianLocation - robotLocation;
			const double forwardDistanceCm = FVector::DotProduct(relativePedestrianLocation, robotForward);
			if (forwardDistanceCm > 0.0 && forwardDistanceCm <= conflict.WarningDistanceCm)
			{
				const double distancePenaltyCm = FMath::Max(0.0, conflict.WarningDistanceCm - distanceCm);
				const double frontPenaltyCm = FMath::Max(
					0.0,
					conflict.StopDistanceCm + SidestepClearanceMarginCm - forwardDistanceCm);
				cutInFrontPenalty += (distancePenaltyCm + frontPenaltyCm) * CutInFrontPenaltyScale;
			}
		}
	}

	const double referenceDistanceCm = FMath::Clamp(conflict.PedestrianDistanceAtClosestCm, 0.0, TotalDistanceCm);
	const FVector baselineLocation = GetLocationAtDistance(referenceDistanceCm);
	const FVector right = GetRightAtDistance(referenceDistanceCm);
	const double robotLateralOffsetCm = FVector::DotProduct(conflict.ClosestRobotLocation - baselineLocation, right);
	const double targetClearanceCm = FMath::Abs(candidate.TargetOffsetCm - robotLateralOffsetCm);

	candidate.bMeetsClearance = candidate.MinDistanceCm >= conflict.StopDistanceCm;
	candidate.Score = candidate.MinDistanceCm
		+ endDistanceCm * 0.15
		+ targetClearanceCm * 0.25
		- cutInFrontPenalty;
	if (candidate.bMeetsClearance)
	{
		candidate.Score += conflict.WarningDistanceCm;
	}

	return candidate;
}

UScenarioPedestrianRuntimeComponent::FSidestepCandidate UScenarioPedestrianRuntimeComponent::ChooseSidestepCandidate(
	const FRobotConflict& conflict) const
{
	const FSidestepCandidate leftCandidate = EvaluateSidestepCandidate(conflict, -1);
	const FSidestepCandidate rightCandidate = EvaluateSidestepCandidate(conflict, 1);

	if (!leftCandidate.bValid) return rightCandidate;
	if (!rightCandidate.bValid) return leftCandidate;

	if (leftCandidate.bMeetsClearance != rightCandidate.bMeetsClearance)
	{
		return leftCandidate.bMeetsClearance ? leftCandidate : rightCandidate;
	}

	const double scoreDelta = leftCandidate.Score - rightCandidate.Score;
	if (FMath::Abs(scoreDelta) > SidestepCandidateScoreTieTolerance)
	{
		return scoreDelta > 0.0 ? leftCandidate : rightCandidate;
	}

	const int32 defaultSide = ComputeDefaultSidestepSide(conflict);
	return defaultSide < 0 ? leftCandidate : rightCandidate;
}

bool UScenarioPedestrianRuntimeComponent::RequestSidestep(const FRobotConflict& conflict)
{
	if (!bSidestepTargetLocked || ActiveSidestepSide == 0)
	{
		const FSidestepCandidate candidate = ChooseSidestepCandidate(conflict);
		if (!candidate.bValid)
		{
			ResetSidestepLock();
			return false;
		}

		ActiveSidestepSide = candidate.Side;
		ActiveSidestepTargetOffsetCm = candidate.TargetOffsetCm;
		bSidestepTargetLocked = true;
		RequestLateralOffset(ActiveSidestepTargetOffsetCm);
		return true;
	}

	const FSidestepCandidate lockedCandidate = EvaluateSidestepCandidate(conflict, ActiveSidestepSide);
	if (!lockedCandidate.bValid)
	{
		ResetSidestepLock();
		return false;
	}

	if (conflict.bHardConflict
		&& !lockedCandidate.bMeetsClearance
		&& lockedCandidate.MinDistanceCm < conflict.StopDistanceCm)
	{
		return false;
	}

	if (FMath::Abs(lockedCandidate.TargetOffsetCm) > FMath::Abs(ActiveSidestepTargetOffsetCm) + 1.0)
	{
		ActiveSidestepTargetOffsetCm = lockedCandidate.TargetOffsetCm;
	}

	RequestLateralOffset(ActiveSidestepTargetOffsetCm);
	return true;
}

void UScenarioPedestrianRuntimeComponent::ResetSidestepLock()
{
	bSidestepTargetLocked = false;
	ActiveSidestepSide = 0;
	ActiveSidestepTargetOffsetCm = 0.0;
}

double UScenarioPedestrianRuntimeComponent::ComputeSpeedScale(const FRobotConflict& conflict) const
{
	switch (CurrentState)
	{
	case EScenarioPedestrianRuntimeState::YieldStop:
	case EScenarioPedestrianRuntimeState::Blocked:
		return 0.0;
	case EScenarioPedestrianRuntimeState::YieldSlowdown:
		return FMath::Clamp(1.0 - conflict.Severity * FMath::Lerp(0.35, 0.8, BehaviorParams.Cooperation), MinSlowdownSpeedScale, 1.0);
	case EScenarioPedestrianRuntimeState::Sidestep:
		return FMath::Clamp(FMath::Lerp(0.75, 0.45, BehaviorParams.Cooperation), MinSlowdownSpeedScale, 1.0);
	case EScenarioPedestrianRuntimeState::Recover:
	case EScenarioPedestrianRuntimeState::FollowBaseline:
	default:
		return 1.0;
	}
}

void UScenarioPedestrianRuntimeComponent::UpdateRuntimeState(const FRobotConflict& conflict, double deltaSeconds)
{
	const bool bValidConflictSample = conflict.RobotActor.IsValid()
		&& conflict.ClosestDistanceCm < TNumericLimits<double>::Max()
		&& conflict.WarningDistanceCm > KINDA_SMALL_NUMBER;
	const bool bSoftConflictLatched = bConflictLatched
		&& bValidConflictSample
		&& conflict.ClosestDistanceCm <= conflict.WarningDistanceCm + SoftConflictExitHysteresisCm;
	const bool bHardConflictLatched = bValidConflictSample
		&& (CurrentState == EScenarioPedestrianRuntimeState::YieldStop || CurrentState == EScenarioPedestrianRuntimeState::Blocked)
		&& conflict.ClosestDistanceCm <= conflict.StopDistanceCm + HardConflictExitHysteresisCm;
	const bool bRawEffectiveConflict = bEnableRobotReaction && bValidConflictSample && (conflict.bHasConflict || bSoftConflictLatched);
	if (bRawEffectiveConflict)
	{
		ClearConflictElapsedSeconds = 0.0;
	}
	else
	{
		ClearConflictElapsedSeconds += FMath::Max(deltaSeconds, 0.0);
	}

	const bool bClearHoldConflict = bValidConflictSample
		&& bConflictLatched
		&& ClearConflictElapsedSeconds < ConflictClearHoldSeconds;
	const bool bEffectiveConflict = bRawEffectiveConflict || bClearHoldConflict;
	const bool bEffectiveHardConflict = bEnableRobotReaction && bValidConflictSample && (conflict.bHardConflict || bHardConflictLatched);

	bConflictLatched = bEffectiveConflict;

	const auto trySetState = [this, &conflict](EScenarioPedestrianRuntimeState newState)
	{
		if (CurrentState == newState || CanLeaveCurrentState() || IsStateEscalation(newState))
		{
			SetRuntimeState(newState, conflict);
			return true;
		}

		return false;
	};

	if (!bEffectiveConflict)
	{
		StoppedByRobotSeconds = 0.0;
		if (!CanLeaveCurrentState()) return;


		RequestLateralOffset(0.0);
		trySetState(FMath::Abs(ActualLateralOffsetCm) > 1.0 || bLateralCurveActive
			? EScenarioPedestrianRuntimeState::Recover
			: EScenarioPedestrianRuntimeState::FollowBaseline);
		return;
	}

	if (CurrentState == EScenarioPedestrianRuntimeState::Blocked)
	{
		RequestLateralOffset(0.0);
		SetRuntimeState(EScenarioPedestrianRuntimeState::Blocked, conflict);
		return;
	}

	if (bEffectiveHardConflict)
	{
		if (StoppedByRobotSeconds >= BehaviorParams.MaxYieldWaitSeconds)
		{
			if (trySetState(EScenarioPedestrianRuntimeState::Blocked))
			{
				RequestLateralOffset(0.0);
			}
			return;
		}

		if (ShouldSidestepForConflict(conflict))
		{
			if (trySetState(EScenarioPedestrianRuntimeState::Sidestep))
			{
				StoppedByRobotSeconds = 0.0;
				if (!RequestSidestep(conflict) && trySetState(EScenarioPedestrianRuntimeState::YieldStop))
				{
					ResetSidestepLock();
					RequestLateralOffset(0.0);
				}
			}
			return;
		}

		if (trySetState(EScenarioPedestrianRuntimeState::YieldStop))
		{
			RequestLateralOffset(0.0);
			StoppedByRobotSeconds += FMath::Max(deltaSeconds, 0.0);
		}
		return;
	}

	StoppedByRobotSeconds = 0.0;
	if (ShouldSidestepForConflict(conflict))
	{
		if (trySetState(EScenarioPedestrianRuntimeState::Sidestep))
		{
			if (!RequestSidestep(conflict))
			{
				ResetSidestepLock();
				RequestLateralOffset(0.0);
			}
		}
	}
	else
	{
		if (trySetState(EScenarioPedestrianRuntimeState::YieldSlowdown))
		{
			RequestLateralOffset(0.0);
		}
	}
}

void UScenarioPedestrianRuntimeComponent::SetRuntimeState(
	EScenarioPedestrianRuntimeState newState,
	const FRobotConflict& conflict)
{
	if (CurrentState == newState) return;


	const EScenarioPedestrianRuntimeState previousState = CurrentState;
	CurrentState = newState;
	StateElapsedSeconds = 0.0;
	if (previousState == EScenarioPedestrianRuntimeState::Sidestep || newState == EScenarioPedestrianRuntimeState::Sidestep)
	{
		ResetSidestepLock();
	}

	const FString robotDebugName = conflict.RobotActor.IsValid() ? conflict.RobotActor->GetName() : TEXT("None");
	UE_LOG(
		LogScenarioPedestrianRuntime,
		Verbose,
		TEXT("Pedestrian state transition | Pedestrian: %s, %s -> %s, Robot: %s, Severity: %.3f, ClosestCm: %.1f"),
		*InstanceId,
		*GetPedestrianRuntimeStateName(previousState),
		*GetPedestrianRuntimeStateName(newState),
		*robotDebugName,
		conflict.Severity,
		conflict.ClosestDistanceCm);
}

bool UScenarioPedestrianRuntimeComponent::CanLeaveCurrentState() const
{
	if (CurrentState == EScenarioPedestrianRuntimeState::FollowBaseline)
	{
		return true;
	}

	return StateElapsedSeconds >= GetMinimumStateHoldSeconds();
}

double UScenarioPedestrianRuntimeComponent::GetMinimumStateHoldSeconds() const
{
	switch (CurrentState)
	{
	case EScenarioPedestrianRuntimeState::Sidestep:
		return SidestepMinimumStateHoldSeconds;
	case EScenarioPedestrianRuntimeState::YieldStop:
		return YieldStopMinimumStateHoldSeconds;
	case EScenarioPedestrianRuntimeState::Recover:
		return RecoverMinimumStateHoldSeconds;
	case EScenarioPedestrianRuntimeState::YieldSlowdown:
	case EScenarioPedestrianRuntimeState::Blocked:
	default:
		return MinimumStateHoldSeconds;
	}
}

bool UScenarioPedestrianRuntimeComponent::IsStateEscalation(EScenarioPedestrianRuntimeState newState) const
{
	if (CurrentState == EScenarioPedestrianRuntimeState::Sidestep
		&& StateElapsedSeconds < SidestepMinimumStateHoldSeconds)
	{
		return newState == EScenarioPedestrianRuntimeState::Blocked;
	}

	return GetRuntimeStatePriority(newState) > GetRuntimeStatePriority(CurrentState);
}

void UScenarioPedestrianRuntimeComponent::RequestLateralOffset(double targetOffsetCm)
{
	const double sidestepLimitCm = GetSidestepLimitCm();
	const double clampedTargetOffsetCm = FMath::Clamp(
		targetOffsetCm,
		-sidestepLimitCm,
		sidestepLimitCm);

	if (bLateralCurveActive && FMath::IsNearlyEqual(LateralCurveTargetOffsetCm, clampedTargetOffsetCm, 1.0))
	{
		TargetLateralOffsetCm = clampedTargetOffsetCm;
		return;
	}

	if (!bLateralCurveActive && FMath::IsNearlyEqual(TargetLateralOffsetCm, clampedTargetOffsetCm, 1.0))
	{
		return;
	}

	TargetLateralOffsetCm = clampedTargetOffsetCm;
	LateralCurveStartOffsetCm = ActualLateralOffsetCm;
	LateralCurveTargetOffsetCm = clampedTargetOffsetCm;
	LateralCurveElapsedSeconds = 0.0;
	LateralCurveDurationSeconds = ComputeLateralCurveDurationSeconds(
		LateralCurveStartOffsetCm,
		LateralCurveTargetOffsetCm);
	bLateralCurveActive = LateralCurveDurationSeconds > KINDA_SMALL_NUMBER
		&& !FMath::IsNearlyEqual(LateralCurveStartOffsetCm, LateralCurveTargetOffsetCm, 1.0);

	if (!bLateralCurveActive)
	{
		ActualLateralOffsetCm = LateralCurveTargetOffsetCm;
	}
}

double UScenarioPedestrianRuntimeComponent::ComputeLateralCurveDurationSeconds(
	double startOffsetCm,
	double targetOffsetCm) const
{
	const double offsetDistanceCm = FMath::Abs(targetOffsetCm - startOffsetCm);
	if (offsetDistanceCm <= KINDA_SMALL_NUMBER)
	{
		return 0.0;
	}

	return FMath::Clamp(
		offsetDistanceCm / LateralCurveSpeedCmPerSecond,
		MinimumLateralCurveDurationSeconds,
		MaximumLateralCurveDurationSeconds);
}

void UScenarioPedestrianRuntimeComponent::UpdateLateralOffset(double deltaSeconds)
{
	if (!bLateralCurveActive)
	{
		ActualLateralOffsetCm = TargetLateralOffsetCm;
		return;
	}

	LateralCurveElapsedSeconds += FMath::Max(deltaSeconds, 0.0);
	const double alpha = LateralCurveDurationSeconds <= KINDA_SMALL_NUMBER
		? 1.0
		: FMath::Clamp(LateralCurveElapsedSeconds / LateralCurveDurationSeconds, 0.0, 1.0);
	const double curvedAlpha = SmootherStep(alpha);
	ActualLateralOffsetCm = FMath::Lerp(LateralCurveStartOffsetCm, LateralCurveTargetOffsetCm, curvedAlpha);

	if (alpha >= 1.0)
	{
		ActualLateralOffsetCm = LateralCurveTargetOffsetCm;
		bLateralCurveActive = false;
	}
}

void UScenarioPedestrianRuntimeComponent::UpdateAnimationKinematics(
	double speedScale,
	double previousLateralOffsetCm,
	double deltaSeconds)
{
	NominalSpeedCmPerSecond = FMath::Max(SpeedCmPerSecond, 0.0);
	ProgressSpeedCmPerSecond = NominalSpeedCmPerSecond * FMath::Clamp(speedScale, 0.0, 1.0);
	if (deltaSeconds <= KINDA_SMALL_NUMBER)
	{
		LateralSpeedCmPerSecond = 0.0;
		return;
	}

	LateralSpeedCmPerSecond = (ActualLateralOffsetCm - previousLateralOffsetCm) / deltaSeconds;
}

void UScenarioPedestrianRuntimeComponent::UpdateScheduleDelay()
{
	const double speedCmPerSecond = FMath::Max(SpeedCmPerSecond, MinRuntimeSpeedCmPerSecond);
	const double nominalDistanceCm = FMath::Clamp(
		InitialDistanceCm + speedCmPerSecond * ActiveTimeSeconds,
		0.0,
		TotalDistanceCm);
	ScheduleDelaySeconds = FMath::Max(0.0, (nominalDistanceCm - CurrentDistanceCm) / speedCmPerSecond);
}

void UScenarioPedestrianRuntimeComponent::ResetRuntimeMetrics()
{
	CurrentState = EScenarioPedestrianRuntimeState::FollowBaseline;
	ActiveTimeSeconds = 0.0;
	ScheduleDelaySeconds = 0.0;
	ForcedWaitSeconds = 0.0;
	BlockedDurationSeconds = 0.0;
	PathDeviationCm = 0.0;
	MaxPathDeviationCm = 0.0;
	MinRobotDistanceCm = -1.0;
	ActualLateralOffsetCm = 0.0;
	TargetLateralOffsetCm = 0.0;
	LateralCurveStartOffsetCm = 0.0;
	LateralCurveTargetOffsetCm = 0.0;
	LateralCurveElapsedSeconds = 0.0;
	LateralCurveDurationSeconds = 0.0;
	StateElapsedSeconds = 0.0;
	ClearConflictElapsedSeconds = 0.0;
	StoppedByRobotSeconds = 0.0;
	LastRobotLocation = FVector::ZeroVector;
	ObservedRobotVelocityCmPerSecond = FVector::ZeroVector;
	NominalSpeedCmPerSecond = FMath::Max(SpeedCmPerSecond, 0.0);
	ProgressSpeedCmPerSecond = NominalSpeedCmPerSecond;
	LateralSpeedCmPerSecond = 0.0;
	bLateralCurveActive = false;
	bConflictLatched = false;
	bHasLastRobotLocation = false;
	ResetSidestepLock();
}
