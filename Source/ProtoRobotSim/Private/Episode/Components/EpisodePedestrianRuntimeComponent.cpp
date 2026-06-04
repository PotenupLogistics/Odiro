
#include "Episode/Components/EpisodePedestrianRuntimeComponent.h"
#include "Episode/Actors/EpisodePedestrian.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodePedestrianRuntime, Log, All);

namespace
{
	constexpr double PedestrianBodyRadiusCm = 35.0;
	constexpr double ConflictSampleStepSeconds = 0.1;
	constexpr double MinRuntimeSpeedCmPerSecond = 1.0;
	constexpr double MinSlowdownSpeedScale = 0.15;
	constexpr double SidestepActivationThreshold = 0.45;

	FString GetPedestrianRuntimeStateName(EEpisodePedestrianRuntimeState state)
	{
		const UEnum* enumPtr = StaticEnum<EEpisodePedestrianRuntimeState>();
		if (!enumPtr) return TEXT("Unknown");

		return enumPtr->GetNameStringByValue(static_cast<int64>(state));
	}
}

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
	bAutoStart = bStartAutomatically;
	ResetRuntimeMetrics();
}

void UEpisodePedestrianRuntimeComponent::SetRobotActor(AActor* inRobotActor, const FString& inRobotInstanceId)
{
	RobotActor = inRobotActor;
	RobotInstanceId = inRobotInstanceId;
	CachedRobotRadiusCm = GetActorRadiusCm(inRobotActor);
}

void UEpisodePedestrianRuntimeComponent::StartFollowing()
{
	if (!HasPlan())
	{
		StopFollowing();
		return;
	}

	CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm, 0.0, TotalDistanceCm);
	ResetRuntimeMetrics();
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

	const double deltaSeconds = deltaTime;
	ActiveTimeSeconds += FMath::Max(deltaSeconds, 0.0);
	UpdateRobotDistanceMetrics();

	const FRobotConflict conflict = FindMostSevereRobotConflict();
	UpdateRuntimeState(conflict, deltaSeconds);
	UpdateLateralOffset(deltaSeconds);

	const double speedScale = ComputeSpeedScale(conflict);
	if (CurrentState == EEpisodePedestrianRuntimeState::YieldStop
		|| CurrentState == EEpisodePedestrianRuntimeState::Blocked)
	{
		ForcedWaitSeconds += deltaSeconds;
	}

	if (CurrentState == EEpisodePedestrianRuntimeState::Blocked)
	{
		BlockedDurationSeconds += deltaSeconds;
	}

	const double distanceDeltaCm = FMath::Max(SpeedCmPerSecond, 0.0) * speedScale * deltaSeconds;
	CurrentDistanceCm = FMath::Clamp(CurrentDistanceCm + distanceDeltaCm, 0.0, TotalDistanceCm);
	MoveOwnerToCurrentDistance(deltaTime);
	UpdateScheduleDelay();

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

FVector UEpisodePedestrianRuntimeComponent::GetRightAtDistance(double distanceCm) const
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

FVector UEpisodePedestrianRuntimeComponent::GetActualLocationAtDistance(double distanceCm, double lateralOffsetCm) const
{
	return GetLocationAtDistance(distanceCm) + GetRightAtDistance(distanceCm) * lateralOffsetCm;
}

void UEpisodePedestrianRuntimeComponent::MoveOwnerToCurrentDistance(double deltaSeconds)
{
	AActor* owner = GetOwner();
	if (!owner || !HasPlan()) return;

	const FVector previousLocation = owner->GetActorLocation();
	FVector targetLocation = GetActualLocationAtDistance(CurrentDistanceCm, ActualLateralOffsetCm);
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

	PathDeviationCm = FMath::Abs(ActualLateralOffsetCm);
	MaxPathDeviationCm = FMath::Max(MaxPathDeviationCm, PathDeviationCm);
}

UEpisodePedestrianRuntimeComponent::FRobotConflict UEpisodePedestrianRuntimeComponent::FindMostSevereRobotConflict() const
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
	const FVector robotVelocity = robotActor->GetVelocity();

	double closestDistanceCm = TNumericLimits<double>::Max();
	double closestTimeSeconds = 0.0;
	double closestPedestrianDistanceCm = CurrentDistanceCm;

	const int32 sampleCount = FMath::Max(1, FMath::CeilToInt(horizonSeconds / ConflictSampleStepSeconds));
	for (int32 sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const double sampleTimeSeconds = FMath::Min(sampleIndex * ConflictSampleStepSeconds, horizonSeconds);
		const double predictedPedestrianDistanceCm = FMath::Clamp(
			CurrentDistanceCm + FMath::Max(SpeedCmPerSecond, 0.0) * sampleTimeSeconds,
			0.0,
			TotalDistanceCm);
		const FVector pedestrianLocation = GetActualLocationAtDistance(predictedPedestrianDistanceCm, ActualLateralOffsetCm);
		const FVector predictedRobotLocation = robotLocation + robotVelocity * sampleTimeSeconds;
		const double distanceCm = FVector::Dist2D(pedestrianLocation, predictedRobotLocation);

		if (distanceCm < closestDistanceCm)
		{
			closestDistanceCm = distanceCm;
			closestTimeSeconds = sampleTimeSeconds;
			closestPedestrianDistanceCm = predictedPedestrianDistanceCm;
		}
	}

	const bool bHasConflict = closestDistanceCm <= warningDistanceCm;
	const bool bHardConflict = closestDistanceCm <= stopDistanceCm;
	if (!bHasConflict)
	{
		return bestConflict;
	}

	const double distanceSeverity = warningDistanceCm <= stopDistanceCm + KINDA_SMALL_NUMBER
		? 1.0
		: 1.0 - FMath::Clamp((closestDistanceCm - stopDistanceCm) / (warningDistanceCm - stopDistanceCm), 0.0, 1.0);
	const double timeSeverity = 1.0 - FMath::Clamp(closestTimeSeconds / horizonSeconds, 0.0, 1.0);
	const double severity = FMath::Clamp(distanceSeverity * 0.75 + timeSeverity * 0.25, 0.0, 1.0);

	bestConflict.RobotActor = robotActor;
	bestConflict.RobotInstanceId = RobotInstanceId.IsEmpty() ? robotActor->GetName() : RobotInstanceId;
	bestConflict.RobotLocation = robotLocation;
	bestConflict.RobotVelocity = robotVelocity;
	bestConflict.RobotRadiusCm = robotRadiusCm;
	bestConflict.TimeToClosestSeconds = closestTimeSeconds;
	bestConflict.ClosestDistanceCm = closestDistanceCm;
	bestConflict.PedestrianDistanceAtClosestCm = closestPedestrianDistanceCm;
	bestConflict.Severity = severity;
	bestConflict.bHasConflict = true;
	bestConflict.bHardConflict = bHardConflict;
	return bestConflict;
}

void UEpisodePedestrianRuntimeComponent::UpdateRobotDistanceMetrics()
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

double UEpisodePedestrianRuntimeComponent::GetActorRadiusCm(const AActor* actor) const
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

double UEpisodePedestrianRuntimeComponent::GetConflictStopDistanceCm(double robotRadiusCm) const
{
	const double cooperationLeadCm = FMath::Lerp(-20.0, 45.0, BehaviorParams.Cooperation);
	return FMath::Max(0.0, BehaviorParams.PersonalSpaceCm + PedestrianBodyRadiusCm + robotRadiusCm + cooperationLeadCm);
}

double UEpisodePedestrianRuntimeComponent::GetConflictWarningDistanceCm(double robotRadiusCm) const
{
	const double warningLeadCm = FMath::Lerp(60.0, 180.0, BehaviorParams.Cooperation);
	return GetConflictStopDistanceCm(robotRadiusCm) + warningLeadCm;
}

bool UEpisodePedestrianRuntimeComponent::ShouldSidestepForConflict(const FRobotConflict& conflict) const
{
	if (!conflict.bHasConflict || BehaviorParams.SidestepDistanceCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (BehaviorParams.Evasiveness < SidestepActivationThreshold)
	{
		return false;
	}

	return conflict.Severity >= FMath::Lerp(0.8, 0.35, BehaviorParams.Evasiveness);
}

double UEpisodePedestrianRuntimeComponent::ComputeSidestepTargetCm(const FRobotConflict& conflict) const
{
	const FVector baselineLocation = GetLocationAtDistance(CurrentDistanceCm);
	const FVector right = GetRightAtDistance(CurrentDistanceCm);
	const double sideDot = FVector::DotProduct(conflict.RobotLocation - baselineLocation, right);

	int32 side = sideDot >= 0.0 ? -1 : 1;
	if (FMath::IsNearlyZero(sideDot))
	{
		side = (GetTypeHash(InstanceId + conflict.RobotInstanceId) % 2) == 0 ? -1 : 1;
	}

	return static_cast<double>(side) * BehaviorParams.SidestepDistanceCm;
}

double UEpisodePedestrianRuntimeComponent::ComputeSpeedScale(const FRobotConflict& conflict) const
{
	switch (CurrentState)
	{
	case EEpisodePedestrianRuntimeState::YieldStop:
	case EEpisodePedestrianRuntimeState::Blocked:
		return 0.0;
	case EEpisodePedestrianRuntimeState::YieldSlowdown:
		return FMath::Clamp(1.0 - conflict.Severity * FMath::Lerp(0.35, 0.8, BehaviorParams.Cooperation), MinSlowdownSpeedScale, 1.0);
	case EEpisodePedestrianRuntimeState::Sidestep:
		return FMath::Clamp(FMath::Lerp(0.75, 0.45, BehaviorParams.Cooperation), MinSlowdownSpeedScale, 1.0);
	case EEpisodePedestrianRuntimeState::Recover:
	case EEpisodePedestrianRuntimeState::FollowBaseline:
	default:
		return 1.0;
	}
}

void UEpisodePedestrianRuntimeComponent::UpdateRuntimeState(const FRobotConflict& conflict, double deltaSeconds)
{
	if (!bEnableRobotReaction || !conflict.bHasConflict)
	{
		StoppedByRobotSeconds = 0.0;
		TargetLateralOffsetCm = 0.0;
		SetRuntimeState(FMath::Abs(ActualLateralOffsetCm) > 1.0
			? EEpisodePedestrianRuntimeState::Recover
			: EEpisodePedestrianRuntimeState::FollowBaseline, conflict);
		return;
	}

	LastConflictRobotInstanceId = conflict.RobotInstanceId;

	if (CurrentState == EEpisodePedestrianRuntimeState::Blocked)
	{
		TargetLateralOffsetCm = 0.0;
		SetRuntimeState(EEpisodePedestrianRuntimeState::Blocked, conflict);
		return;
	}

	if (conflict.bHardConflict)
	{
		if (StoppedByRobotSeconds >= BehaviorParams.MaxYieldWaitSeconds)
		{
			TargetLateralOffsetCm = 0.0;
			SetRuntimeState(EEpisodePedestrianRuntimeState::Blocked, conflict);
			return;
		}

		if (ShouldSidestepForConflict(conflict))
		{
			StoppedByRobotSeconds = 0.0;
			TargetLateralOffsetCm = ComputeSidestepTargetCm(conflict);
			SetRuntimeState(EEpisodePedestrianRuntimeState::Sidestep, conflict);
			return;
		}

		TargetLateralOffsetCm = 0.0;
		StoppedByRobotSeconds += FMath::Max(deltaSeconds, 0.0);
		SetRuntimeState(EEpisodePedestrianRuntimeState::YieldStop, conflict);
		return;
	}

	StoppedByRobotSeconds = 0.0;
	if (ShouldSidestepForConflict(conflict))
	{
		TargetLateralOffsetCm = ComputeSidestepTargetCm(conflict);
		SetRuntimeState(EEpisodePedestrianRuntimeState::Sidestep, conflict);
	}
	else
	{
		TargetLateralOffsetCm = 0.0;
		SetRuntimeState(EEpisodePedestrianRuntimeState::YieldSlowdown, conflict);
	}
}

void UEpisodePedestrianRuntimeComponent::SetRuntimeState(
	EEpisodePedestrianRuntimeState newState,
	const FRobotConflict& conflict)
{
	if (CurrentState == newState)
	{
		return;
	}

	const EEpisodePedestrianRuntimeState previousState = CurrentState;
	CurrentState = newState;
	UE_LOG(
		LogEpisodePedestrianRuntime,
		Verbose,
		TEXT("Pedestrian state transition | Pedestrian: %s, %s -> %s, Robot: %s, Severity: %.3f, ClosestCm: %.1f"),
		*InstanceId,
		*GetPedestrianRuntimeStateName(previousState),
		*GetPedestrianRuntimeStateName(newState),
		*conflict.RobotInstanceId,
		conflict.Severity,
		conflict.ClosestDistanceCm);
}

void UEpisodePedestrianRuntimeComponent::UpdateLateralOffset(double deltaSeconds)
{
	const double recoverySpeedCmPerSecond = FMath::Max(BehaviorParams.SidestepDistanceCm * 2.0, 80.0);
	const double offsetDeltaCm = TargetLateralOffsetCm - ActualLateralOffsetCm;
	const double maxStepCm = recoverySpeedCmPerSecond * FMath::Max(deltaSeconds, 0.0);
	if (FMath::Abs(offsetDeltaCm) <= maxStepCm)
	{
		ActualLateralOffsetCm = TargetLateralOffsetCm;
	}
	else
	{
		ActualLateralOffsetCm += FMath::Sign(offsetDeltaCm) * maxStepCm;
	}
}

void UEpisodePedestrianRuntimeComponent::UpdateScheduleDelay()
{
	const double speedCmPerSecond = FMath::Max(SpeedCmPerSecond, MinRuntimeSpeedCmPerSecond);
	const double nominalDistanceCm = FMath::Clamp(
		InitialDistanceCm + speedCmPerSecond * ActiveTimeSeconds,
		0.0,
		TotalDistanceCm);
	ScheduleDelaySeconds = FMath::Max(0.0, (nominalDistanceCm - CurrentDistanceCm) / speedCmPerSecond);
}

void UEpisodePedestrianRuntimeComponent::ResetRuntimeMetrics()
{
	CurrentState = EEpisodePedestrianRuntimeState::FollowBaseline;
	ActiveTimeSeconds = 0.0;
	ScheduleDelaySeconds = 0.0;
	ForcedWaitSeconds = 0.0;
	BlockedDurationSeconds = 0.0;
	PathDeviationCm = 0.0;
	MaxPathDeviationCm = 0.0;
	MinRobotDistanceCm = -1.0;
	LastConflictRobotInstanceId.Reset();
	ActualLateralOffsetCm = 0.0;
	TargetLateralOffsetCm = 0.0;
	StoppedByRobotSeconds = 0.0;
}
