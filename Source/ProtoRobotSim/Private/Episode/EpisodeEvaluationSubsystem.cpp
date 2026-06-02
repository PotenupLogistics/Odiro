#include "Episode/EpisodeEvaluationSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeEvaluation, Log, All);

namespace
{
	template <typename TEnum>
	FString ToEvaluationEnumString(TEnum value)
	{
		if (const UEnum* enumValue = StaticEnum<TEnum>()) return enumValue->GetNameStringByValue(static_cast<int64>(value));

		return TEXT("Unknown");
	}
}

bool UEpisodeEvaluationSubsystem::StartEvaluation(
	const FEpisodeEvaluationConfig& evaluationConfig,
	const FEpisodeRuntimeContext& runtimeContext,
	double inTimeLimitSeconds)
{
	if (bEvaluating)
	{
		StopEvaluation();
	}

	if (!IsValid(runtimeContext.RobotActor))
	{
		UE_LOG(LogEpisodeEvaluation, Warning, TEXT("평가 시작 거부: 런타임 컨텍스트에 유효한 로봇 액터가 없음 | Episode: %s"), *runtimeContext.EpisodeId);
		return false;
	}

	ActiveEvaluationConfig = evaluationConfig;
	ActiveRuntimeContext = runtimeContext;

	CurrentResult = FEpisodeEvaluationResult{};
	CurrentResult.EpisodeId = runtimeContext.EpisodeId;
	CurrentResult.Outcome = EEpisodeEvaluationOutcome::Running;
	CurrentResult.TerminalReason = EEpisodeEvaluationTerminalReason::None;

	UWorld* world = GetWorld();
	EvaluationStartTimeSeconds = world ? world->GetTimeSeconds() : 0.0;
	TimeLimitSeconds = FMath::Max(0.0, inTimeLimitSeconds);
	CurrentScore = 0.0;
	NearMissCount = 0;
	NearMissTotalDurationSeconds = 0.0;
	NearMissMinDistanceCm = TNumericLimits<double>::Max();
	ActiveNearMisses.Reset();
	PenaltyRegionStates.Reset();
	BlockedRegionStates.Reset();
	LastCollisionEventTimes.Reset();
	GoalReachedCount = 0;
	RobotFallCount = 0;
	StaticObstacleCollisionCount = 0;
	BlockedRegionCollisionCount = 0;
	PenaltyRegionViolationCount = 0;
	PedestrianCollisionCount = 0;
	SetFloatMetric(TEXT("score"), CurrentScore);
	SetFloatMetric(TEXT("goal_reached"), 0.0);
	SetFloatMetric(TEXT("robot_fall_count"), 0.0);
	SetFloatMetric(TEXT("static_obstacle_collision_count"), 0.0);
	SetFloatMetric(TEXT("blocked_region_collision_count"), 0.0);
	SetFloatMetric(TEXT("penalty_region_violation_count"), 0.0);
	SetFloatMetric(TEXT("pedestrian_collision_count"), 0.0);
	SetFloatMetric(TEXT("near_miss_count"), 0.0);
	SetFloatMetric(TEXT("near_miss_total_duration_s"), 0.0);
	BindEvaluationHitDelegates();
	bEvaluating = true;

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("평가 시작 | Episode: %s, TimeLimit: %.2fs, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d, NearMissDistance: %.1fcm"),
		*runtimeContext.EpisodeId,
		TimeLimitSeconds,
		*runtimeContext.RobotInstanceId,
		runtimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		runtimeContext.RuntimeActors.Num(),
		runtimeContext.GroundRegionActors.Num(),
		runtimeContext.StaticObstacleActors.Num(),
		runtimeContext.PedestrianActors.Num(),
		ActiveEvaluationConfig.NearMissDistanceCm);

	return true;
}

void UEpisodeEvaluationSubsystem::StopEvaluation()
{
	if (bEvaluating)
	{
		UE_LOG(LogEpisodeEvaluation, Log, TEXT("평가 중지 | Episode: %s"), *ActiveRuntimeContext.EpisodeId);
	}

	bEvaluating = false;
	UnbindEvaluationHitDelegates();
	ActiveEvaluationConfig = FEpisodeEvaluationConfig{};
	ActiveRuntimeContext = FEpisodeRuntimeContext{};
	TimeLimitSeconds = 0.0;
	EvaluationStartTimeSeconds = 0.0;
	CurrentScore = 0.0;
	NearMissCount = 0;
	NearMissTotalDurationSeconds = 0.0;
	NearMissMinDistanceCm = TNumericLimits<double>::Max();
	ActiveNearMisses.Reset();
	PenaltyRegionStates.Reset();
	BlockedRegionStates.Reset();
	LastCollisionEventTimes.Reset();
	GoalReachedCount = 0;
	RobotFallCount = 0;
	StaticObstacleCollisionCount = 0;
	BlockedRegionCollisionCount = 0;
	PenaltyRegionViolationCount = 0;
	PedestrianCollisionCount = 0;
}

void UEpisodeEvaluationSubsystem::RequestEndEpisode(const FEpisodeEvaluationResult& result)
{
	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	const TArray<FEpisodeEvaluationEvent> existingEvents = CurrentResult.Events;
	const TMap<FString, FEpisodeParamValue> existingMetrics = CurrentResult.Metrics;
	CurrentResult = result;

	if (existingEvents.Num() > 0)
	{
		bool bResultAlreadyContainsExistingEvents = CurrentResult.Events.Num() >= existingEvents.Num();
		if (bResultAlreadyContainsExistingEvents)
		{
			for (int32 index = 0; index < existingEvents.Num(); ++index)
			{
				if (CurrentResult.Events[index].EventType != existingEvents[index].EventType
					|| CurrentResult.Events[index].ElapsedTimeSeconds != existingEvents[index].ElapsedTimeSeconds)
				{
					bResultAlreadyContainsExistingEvents = false;
					break;
				}
			}
		}

		if (CurrentResult.Events.Num() == 0)
		{
			CurrentResult.Events = existingEvents;
		}
		else if (!bResultAlreadyContainsExistingEvents)
		{
			TArray<FEpisodeEvaluationEvent> mergedEvents = existingEvents;
			for (FEpisodeEvaluationEvent event : CurrentResult.Events)
			{
				event.EventIndex = mergedEvents.Num();
				mergedEvents.Add(event);
			}
			CurrentResult.Events = mergedEvents;
		}
	}

	for (const TPair<FString, FEpisodeParamValue>& pair : existingMetrics)
	{
		if (!CurrentResult.Metrics.Contains(pair.Key))
		{
			CurrentResult.Metrics.Add(pair.Key, pair.Value);
		}
	}

	CurrentResult.bCompleted = true;

	if (CurrentResult.DurationSeconds <= 0.0)
	{
		CurrentResult.DurationSeconds = GetElapsedTimeSeconds();
	}

	SetFloatMetric(TEXT("duration_s"), CurrentResult.DurationSeconds);
	UnbindEvaluationHitDelegates();
	bEvaluating = false;

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("평가 종료 | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Score: %.2f, Events: %d, Metrics: %d"),
		*CurrentResult.EpisodeId,
		CurrentResult.bSuccess ? TEXT("true") : TEXT("false"),
		*ToEvaluationEnumString(CurrentResult.Outcome),
		*ToEvaluationEnumString(CurrentResult.TerminalReason),
		CurrentResult.DurationSeconds,
		CurrentScore,
		CurrentResult.Events.Num(),
		CurrentResult.Metrics.Num());

	OnEpisodeEnded.Broadcast(CurrentResult);
}

void UEpisodeEvaluationSubsystem::Tick(float deltaTime)
{
	if (!bEvaluating) return;

	if (CheckGoalReached()) return;
	if (CheckRobotFall()) return;
	UpdateBlockedRegionViolations();
	UpdatePenaltyRegionViolations();

	UpdateNearMisses();

	if (TimeLimitSeconds > 0.0 && GetElapsedTimeSeconds() >= TimeLimitSeconds)
	{
		EndForTimeout();
	}
}

bool UEpisodeEvaluationSubsystem::IsTickable() const
{
	return bEvaluating;
}

TStatId UEpisodeEvaluationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEpisodeEvaluationSubsystem, STATGROUP_Tickables);
}

FEpisodeParamValue UEpisodeEvaluationSubsystem::MakeFloatParam(double value)
{
	FEpisodeParamValue paramValue;
	paramValue.Type = EEpisodeParamValueType::Float;
	paramValue.FloatValue = value;
	return paramValue;
}

FEpisodeParamValue UEpisodeEvaluationSubsystem::MakeStringParam(const FString& value)
{
	FEpisodeParamValue paramValue;
	paramValue.Type = EEpisodeParamValueType::String;
	paramValue.StringValue = value;
	return paramValue;
}

void UEpisodeEvaluationSubsystem::AddEvaluationEvent(
	EEpisodeEvaluationEventType eventType,
	EEpisodeEvaluationEventSeverity severity,
	const FString& message)
{
	AddEvaluationEventWithDetails(
		eventType,
		severity,
		message,
		FString(),
		IsValid(ActiveRuntimeContext.RobotActor)
			? ActiveRuntimeContext.RobotActor->GetActorLocation()
			: FVector::ZeroVector,
		0.0,
		TMap<FString, FEpisodeParamValue>());
}

void UEpisodeEvaluationSubsystem::AddEvaluationEventWithDetails(
	EEpisodeEvaluationEventType eventType,
	EEpisodeEvaluationEventSeverity severity,
	const FString& message,
	const FString& targetInstanceId,
	const FVector& location,
	double value,
	const TMap<FString, FEpisodeParamValue>& properties)
{
	FEpisodeEvaluationEvent event;
	event.EventIndex = CurrentResult.Events.Num();
	event.ElapsedTimeSeconds = GetElapsedTimeSeconds();
	event.EventType = eventType;
	event.Severity = severity;
	event.Message = message;
	event.TargetInstanceId = targetInstanceId;
	event.Location = location;
	event.Value = value;
	event.Properties = properties;
	if (IsValid(ActiveRuntimeContext.RobotActor))
	{
		event.SubjectInstanceId = ActiveRuntimeContext.RobotInstanceId;
	}

	CurrentResult.Events.Add(event);

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("평가 이벤트 기록 | Episode: %s, Index: %d, Type: %s, Severity: %s, Subject: %s, Target: %s, Time: %.2fs, Value: %.2f, Message: %s"),
		*CurrentResult.EpisodeId,
		event.EventIndex,
		*ToEvaluationEnumString(event.EventType),
		*ToEvaluationEnumString(event.Severity),
		*event.SubjectInstanceId,
		*event.TargetInstanceId,
		event.ElapsedTimeSeconds,
		event.Value,
		*event.Message);
}

void UEpisodeEvaluationSubsystem::BindEvaluationHitDelegates()
{
	UnbindEvaluationHitDelegates();

	BindActorHitDelegates(ActiveRuntimeContext.RobotActor.Get());

	for (const TObjectPtr<AActor>& staticObstacleActor : ActiveRuntimeContext.StaticObstacleActors)
	{
		BindActorHitDelegates(staticObstacleActor.Get());
	}

	for (const TObjectPtr<AActor>& pedestrianActor : ActiveRuntimeContext.PedestrianActors)
	{
		BindActorHitDelegates(pedestrianActor.Get());
	}

	for (const TObjectPtr<AActor>& groundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		BindActorHitDelegates(groundRegionActor.Get());
	}
}

void UEpisodeEvaluationSubsystem::BindActorHitDelegates(AActor* actor)
{
	if (!IsValid(actor)) return;

	TArray<UPrimitiveComponent*> primitiveComponents;
	actor->GetComponents<UPrimitiveComponent>(primitiveComponents);
	for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
	{
		if (!IsValid(primitiveComponent) || IsHitComponentBound(primitiveComponent)) continue;

		primitiveComponent->OnComponentHit.RemoveDynamic(this, &UEpisodeEvaluationSubsystem::HandleObservedComponentHit);
		primitiveComponent->OnComponentHit.AddDynamic(this, &UEpisodeEvaluationSubsystem::HandleObservedComponentHit);
		primitiveComponent->SetNotifyRigidBodyCollision(true);
		BoundHitComponents.Add(primitiveComponent);
	}
}

void UEpisodeEvaluationSubsystem::UnbindEvaluationHitDelegates()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& boundComponent : BoundHitComponents)
	{
		if (UPrimitiveComponent* primitiveComponent = boundComponent.Get())
		{
			primitiveComponent->OnComponentHit.RemoveDynamic(this, &UEpisodeEvaluationSubsystem::HandleObservedComponentHit);
		}
	}

	BoundHitComponents.Reset();
}

bool UEpisodeEvaluationSubsystem::IsHitComponentBound(const UPrimitiveComponent* primitiveComponent) const
{
	if (!primitiveComponent) return false;

	for (const TWeakObjectPtr<UPrimitiveComponent>& boundComponent : BoundHitComponents)
	{
		if (boundComponent.Get() == primitiveComponent) return true;
	}

	return false;
}

void UEpisodeEvaluationSubsystem::HandleObservedComponentHit(
	UPrimitiveComponent* hitComponent,
	AActor* otherActor,
	UPrimitiveComponent* otherComp,
	FVector normalImpulse,
	const FHitResult& hit)
{
	(void)otherComp;
	(void)normalImpulse;

	if (!bEvaluating || !hitComponent) return;

	AActor* hitOwner = hitComponent->GetOwner();
	AActor* targetActor = nullptr;
	if (IsRobotActor(hitOwner))
	{
		targetActor = otherActor;
	}
	else if (IsRobotActor(otherActor))
	{
		targetActor = hitOwner;
	}

	if (!IsValid(targetActor) || IsRobotActor(targetActor)) return;

	const FVector impactPoint(hit.ImpactPoint.X, hit.ImpactPoint.Y, hit.ImpactPoint.Z);
	const FVector eventLocation = impactPoint.IsNearlyZero()
		? targetActor->GetActorLocation()
		: impactPoint;

	if (AEpisodeGroundRegion* groundRegion = Cast<AEpisodeGroundRegion>(targetActor))
	{
		if (groundRegion->RegionSpec.RegionType == EEpisodeGroundRegionType::Blocked)
		{
			RecordCollisionEvent(
				EEpisodeEvaluationEventType::BlockedRegionCollision,
				targetActor,
				eventLocation,
				ActiveEvaluationConfig.BlockedRegionCollisionScore,
				TEXT("로봇이 blocked region과 충돌함."));
		}
		return;
	}

	if (ContainsRuntimeActor(ActiveRuntimeContext.StaticObstacleActors, targetActor))
	{
		RecordCollisionEvent(
			EEpisodeEvaluationEventType::StaticObstacleCollision,
			targetActor,
			eventLocation,
			ActiveEvaluationConfig.StaticObstacleCollisionScore,
			TEXT("로봇이 정적 장애물과 충돌함."));
		return;
	}

	if (ContainsRuntimeActor(ActiveRuntimeContext.PedestrianActors, targetActor))
	{
		RecordCollisionEvent(
			EEpisodeEvaluationEventType::PedestrianCollision,
			targetActor,
			eventLocation,
			ActiveEvaluationConfig.PedestrianCollisionScore,
			TEXT("로봇이 보행자와 충돌함."));
	}
}

bool UEpisodeEvaluationSubsystem::CheckGoalReached()
{
	if (!ActiveRuntimeContext.bHasGoalLocation || !IsValid(ActiveRuntimeContext.RobotActor)) return false;

	const double acceptanceRadiusCm = FMath::Max(0.0, ActiveEvaluationConfig.GoalAcceptanceRadiusCm);
	if (acceptanceRadiusCm <= 0.0) return false;

	const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	const double goalDistanceCm = FVector::Dist2D(robotLocation, ActiveRuntimeContext.GoalLocation);
	if (goalDistanceCm > acceptanceRadiusCm) return false;

	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	++GoalReachedCount;
	SetFloatMetric(TEXT("goal_reached"), GoalReachedCount);
	SetFloatMetric(TEXT("goal_distance_m"), goalDistanceCm / 100.0);

	FinishEpisode(
		true,
		HasWarningEventsOrScore() ? EEpisodeEvaluationOutcome::Warning : EEpisodeEvaluationOutcome::Success,
		EEpisodeEvaluationTerminalReason::GoalReached);
	return true;
}

bool UEpisodeEvaluationSubsystem::CheckRobotFall()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor)) return false;

	const double fallAngleDegrees = FMath::Max(0.0, ActiveEvaluationConfig.FallAngleDegrees);
	if (fallAngleDegrees <= 0.0) return false;

	const FVector robotUp = ActiveRuntimeContext.RobotActor->GetActorUpVector().GetSafeNormal();
	const double upDot = FMath::Clamp(FVector::DotProduct(robotUp, FVector::UpVector), -1.0, 1.0);
	const double tiltAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(upDot));
	if (tiltAngleDegrees < fallAngleDegrees) return false;

	++RobotFallCount;
	SetFloatMetric(TEXT("robot_fall_count"), RobotFallCount);
	SetFloatMetric(TEXT("robot_fall_angle_deg"), tiltAngleDegrees);

	TMap<FString, FEpisodeParamValue> properties;
	properties.Add(TEXT("tilt_angle_deg"), MakeFloatParam(tiltAngleDegrees));
	properties.Add(TEXT("fall_angle_threshold_deg"), MakeFloatParam(fallAngleDegrees));
	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::RobotFall,
		EEpisodeEvaluationEventSeverity::Failure,
		TEXT("로봇이 낙상 각도 임계값을 초과함."),
		FString(),
		ActiveRuntimeContext.RobotActor->GetActorLocation(),
		tiltAngleDegrees,
		properties);

	FinishEpisode(false, EEpisodeEvaluationOutcome::Failure, EEpisodeEvaluationTerminalReason::RobotFall);
	return true;
}

void UEpisodeEvaluationSubsystem::UpdateBlockedRegionViolations()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor)) return;

	const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> observedBlockedRegionIds;

	for (const TObjectPtr<AActor>& groundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		AEpisodeGroundRegion* groundRegion = Cast<AEpisodeGroundRegion>(groundRegionActor.Get());
		if (!IsValid(groundRegion)
			|| groundRegion->RegionSpec.RegionType != EEpisodeGroundRegionType::Blocked)
		{
			continue;
		}

		const FString regionId = GetActorInstanceId(groundRegion);
		observedBlockedRegionIds.Add(regionId);

		FBlockedRegionState& regionState = BlockedRegionStates.FindOrAdd(regionId);
		const bool bInside = groundRegion->ContainsWorldLocation2D(robotLocation);
		if (!bInside)
		{
			regionState.bInside = false;
			continue;
		}

		if (regionState.bInside) continue;

		regionState.bInside = true;

		RecordCollisionEvent(
			EEpisodeEvaluationEventType::BlockedRegionCollision,
			groundRegion,
			robotLocation,
			ActiveEvaluationConfig.BlockedRegionCollisionScore,
			TEXT("로봇이 blocked region에 진입함."));
	}

	TArray<FString> missingRegionIds;
	for (const TPair<FString, FBlockedRegionState>& pair : BlockedRegionStates)
	{
		if (!observedBlockedRegionIds.Contains(pair.Key))
		{
			missingRegionIds.Add(pair.Key);
		}
	}

	for (const FString& missingRegionId : missingRegionIds)
	{
		BlockedRegionStates.Remove(missingRegionId);
	}
}

void UEpisodeEvaluationSubsystem::UpdatePenaltyRegionViolations()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor)) return;

	const double elapsedTimeSeconds = GetElapsedTimeSeconds();
	const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> observedPenaltyRegionIds;

	for (const TObjectPtr<AActor>& groundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		AEpisodeGroundRegion* groundRegion = Cast<AEpisodeGroundRegion>(groundRegionActor.Get());
		if (!IsValid(groundRegion) || groundRegion->RegionSpec.RegionType != EEpisodeGroundRegionType::Penalty) continue;

		const FString regionId = GetActorInstanceId(groundRegion);
		observedPenaltyRegionIds.Add(regionId);
		FPenaltyRegionState& regionState = PenaltyRegionStates.FindOrAdd(regionId);
		const bool bInside = groundRegion->ContainsWorldLocation2D(robotLocation);
		if (!bInside)
		{
			regionState.bInside = false;
			regionState.bEventRecorded = false;
			regionState.EnterTimeSeconds = 0.0;
			continue;
		}

		if (!regionState.bInside)
		{
			regionState.bInside = true;
			regionState.bEventRecorded = false;
			regionState.EnterTimeSeconds = elapsedTimeSeconds;
		}

		const double requiredDurationSeconds = FMath::Max(0.0, groundRegion->RegionSpec.ViolationAfterSeconds);
		if (regionState.bEventRecorded || elapsedTimeSeconds - regionState.EnterTimeSeconds < requiredDurationSeconds) continue;

		const double scoreDelta = ActiveEvaluationConfig.PenaltyRegionViolationScore;
		TMap<FString, FEpisodeParamValue> properties;
		properties.Add(TEXT("region_id"), MakeStringParam(regionId));
		properties.Add(TEXT("enter_time_s"), MakeFloatParam(regionState.EnterTimeSeconds));
		properties.Add(TEXT("duration_s"), MakeFloatParam(elapsedTimeSeconds - regionState.EnterTimeSeconds));
		properties.Add(TEXT("violation_after_s"), MakeFloatParam(requiredDurationSeconds));
		AddEvaluationEventWithDetails(
			EEpisodeEvaluationEventType::PenaltyRegionViolation,
			EEpisodeEvaluationEventSeverity::Warning,
			TEXT("로봇이 penalty region 조건을 위반함."),
			regionId,
			robotLocation,
			scoreDelta,
			properties);
		AddScore(scoreDelta);

		++PenaltyRegionViolationCount;
		SetFloatMetric(TEXT("penalty_region_violation_count"), PenaltyRegionViolationCount);
		regionState.bEventRecorded = true;
	}

	TArray<FString> missingRegionIds;
	for (const TPair<FString, FPenaltyRegionState>& pair : PenaltyRegionStates)
	{
		if (!observedPenaltyRegionIds.Contains(pair.Key))
		{
			missingRegionIds.Add(pair.Key);
		}
	}

	for (const FString& missingRegionId : missingRegionIds)
	{
		PenaltyRegionStates.Remove(missingRegionId);
	}
}

void UEpisodeEvaluationSubsystem::UpdateNearMisses()
{
	const FEpisodeEvaluationConfig& evaluationConfig = ActiveEvaluationConfig;
	if (evaluationConfig.NearMissDistanceCm <= 0.0 || !IsValid(ActiveRuntimeContext.RobotActor)) return;

	const double elapsedTimeSeconds = GetElapsedTimeSeconds();
	const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> observedPedestrianIds;

	for (int32 index = 0; index < ActiveRuntimeContext.PedestrianActors.Num(); ++index)
	{
		AActor* pedestrianActor = ActiveRuntimeContext.PedestrianActors[index].Get();
		if (!IsValid(pedestrianActor)) continue;

		const FString pedestrianInstanceId = ActiveRuntimeContext.PedestrianInstanceIds.IsValidIndex(index)
			? ActiveRuntimeContext.PedestrianInstanceIds[index]
			: pedestrianActor->GetName();
		observedPedestrianIds.Add(pedestrianInstanceId);

		const FVector pedestrianLocation = pedestrianActor->GetActorLocation();
		const double distanceCm = FVector::Dist2D(robotLocation, pedestrianLocation);
		FNearMissIntervalState* activeState = ActiveNearMisses.Find(pedestrianInstanceId);

		if (distanceCm <= evaluationConfig.NearMissDistanceCm)
		{
			if (!activeState)
			{
				activeState = &ActiveNearMisses.Add(pedestrianInstanceId);
				activeState->StartTimeSeconds = elapsedTimeSeconds;
			}

			activeState->LastInsideTimeSeconds = elapsedTimeSeconds;
			if (distanceCm < activeState->MinDistanceCm)
			{
				activeState->MinDistanceCm = distanceCm;
				activeState->ClosestRobotLocation = robotLocation;
				activeState->ClosestPedestrianLocation = pedestrianLocation;
			}
			continue;
		}

		if (activeState && elapsedTimeSeconds - activeState->LastInsideTimeSeconds >= NearMissClearanceGraceSeconds)
		{
			const FNearMissIntervalState state = *activeState;
			ActiveNearMisses.Remove(pedestrianInstanceId);
			CloseNearMissInterval(pedestrianInstanceId, state, state.LastInsideTimeSeconds);
		}
	}

	TArray<FString> missingPedestrianIds;
	for (const TPair<FString, FNearMissIntervalState>& pair : ActiveNearMisses)
	{
		if (!observedPedestrianIds.Contains(pair.Key))
		{
			missingPedestrianIds.Add(pair.Key);
		}
	}

	for (const FString& pedestrianInstanceId : missingPedestrianIds)
	{
		if (const FNearMissIntervalState* activeState = ActiveNearMisses.Find(pedestrianInstanceId))
		{
			const FNearMissIntervalState state = *activeState;
			ActiveNearMisses.Remove(pedestrianInstanceId);
			CloseNearMissInterval(pedestrianInstanceId, state, elapsedTimeSeconds);
		}
	}
}

void UEpisodeEvaluationSubsystem::FlushActiveNearMisses()
{
	const double endTimeSeconds = GetElapsedTimeSeconds();
	TArray<FString> pedestrianInstanceIds;
	ActiveNearMisses.GetKeys(pedestrianInstanceIds);

	for (const FString& pedestrianInstanceId : pedestrianInstanceIds)
	{
		if (const FNearMissIntervalState* activeState = ActiveNearMisses.Find(pedestrianInstanceId))
		{
			CloseNearMissInterval(pedestrianInstanceId, *activeState, endTimeSeconds);
		}
	}

	ActiveNearMisses.Reset();
}

void UEpisodeEvaluationSubsystem::CloseNearMissInterval(
	const FString& pedestrianInstanceId,
	const FNearMissIntervalState& state,
	double endTimeSeconds)
{
	if (state.MinDistanceCm == TNumericLimits<double>::Max()) return;

	const double durationSeconds = FMath::Max(0.0, endTimeSeconds - state.StartTimeSeconds);
	const double scoreDelta = ActiveEvaluationConfig.PedestrianNearMissScore;

	FEpisodeEvaluationEvent event;
	event.EventIndex = CurrentResult.Events.Num();
	event.ElapsedTimeSeconds = endTimeSeconds;
	event.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
	event.Severity = EEpisodeEvaluationEventSeverity::Warning;
	event.SubjectInstanceId = ActiveRuntimeContext.RobotInstanceId;
	event.TargetInstanceId = pedestrianInstanceId;
	event.Location = state.ClosestRobotLocation;
	event.Value = scoreDelta;
	event.Message = TEXT("보행자 near-miss 구간.");
	event.Properties.Add(TEXT("start_time_s"), MakeFloatParam(state.StartTimeSeconds));
	event.Properties.Add(TEXT("end_time_s"), MakeFloatParam(endTimeSeconds));
	event.Properties.Add(TEXT("duration_s"), MakeFloatParam(durationSeconds));
	event.Properties.Add(TEXT("min_distance_m"), MakeFloatParam(state.MinDistanceCm / 100.0));
	event.Properties.Add(TEXT("pedestrian_id"), MakeStringParam(pedestrianInstanceId));
	CurrentResult.Events.Add(event);

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("평가 이벤트 기록 | Episode: %s, Index: %d, Type: %s, Severity: %s, Subject: %s, Target: %s, Duration: %.2fs, MinDistance: %.2fm, ScoreDelta: %.2f"),
		*CurrentResult.EpisodeId,
		event.EventIndex,
		*ToEvaluationEnumString(event.EventType),
		*ToEvaluationEnumString(event.Severity),
		*event.SubjectInstanceId,
		*event.TargetInstanceId,
		durationSeconds,
		state.MinDistanceCm / 100.0,
		scoreDelta);

	AddScore(scoreDelta);
	++NearMissCount;
	NearMissTotalDurationSeconds += durationSeconds;
	NearMissMinDistanceCm = FMath::Min(NearMissMinDistanceCm, state.MinDistanceCm);

	SetFloatMetric(TEXT("near_miss_count"), NearMissCount);
	SetFloatMetric(TEXT("near_miss_total_duration_s"), NearMissTotalDurationSeconds);
	SetFloatMetric(TEXT("near_miss_min_distance_m"), NearMissMinDistanceCm / 100.0);
}

void UEpisodeEvaluationSubsystem::SetFloatMetric(const FString& key, double value)
{
	CurrentResult.Metrics.Add(key, MakeFloatParam(value));
}

void UEpisodeEvaluationSubsystem::AddScore(double scoreDelta)
{
	CurrentScore += scoreDelta;
	SetFloatMetric(TEXT("score"), CurrentScore);
}

void UEpisodeEvaluationSubsystem::FinishEpisode(
	bool bSuccess,
	EEpisodeEvaluationOutcome outcome,
	EEpisodeEvaluationTerminalReason terminalReason)
{
	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	FEpisodeEvaluationResult result = CurrentResult;
	result.EpisodeId = ActiveRuntimeContext.EpisodeId;
	result.bSuccess = bSuccess;
	result.Outcome = outcome;
	result.TerminalReason = terminalReason;
	result.DurationSeconds = GetElapsedTimeSeconds();
	RequestEndEpisode(result);
}

void UEpisodeEvaluationSubsystem::RecordCollisionEvent(
	EEpisodeEvaluationEventType eventType,
	AActor* targetActor,
	const FVector& location,
	double scoreDelta,
	const FString& message)
{
	if (!bEvaluating) return;

	const FString targetInstanceId = GetActorInstanceId(targetActor);
	const FString collisionEventKey = FString::Printf(TEXT("%s:%s"), *ToEvaluationEnumString(eventType), *targetInstanceId);
	const double elapsedTimeSeconds = GetElapsedTimeSeconds();
	if (const double* lastRecordedTime = LastCollisionEventTimes.Find(collisionEventKey))
	{
		if (elapsedTimeSeconds - *lastRecordedTime < CollisionEventCooldownSeconds) return;
	}

	LastCollisionEventTimes.Add(collisionEventKey, elapsedTimeSeconds);

	TMap<FString, FEpisodeParamValue> properties;
	properties.Add(TEXT("target_id"), MakeStringParam(targetInstanceId));
	if (targetActor)
	{
		properties.Add(TEXT("target_actor"), MakeStringParam(targetActor->GetName()));
	}
	if (const AEpisodeGroundRegion* groundRegion = Cast<AEpisodeGroundRegion>(targetActor))
	{
		properties.Add(TEXT("region_id"), MakeStringParam(GetActorInstanceId(groundRegion)));
	}

	AddEvaluationEventWithDetails(
		eventType,
		EEpisodeEvaluationEventSeverity::Warning,
		message,
		targetInstanceId,
		location,
		scoreDelta,
		properties);
	AddScore(scoreDelta);

	switch (eventType)
	{
	case EEpisodeEvaluationEventType::StaticObstacleCollision:
		++StaticObstacleCollisionCount;
		SetFloatMetric(TEXT("static_obstacle_collision_count"), StaticObstacleCollisionCount);
		break;
	case EEpisodeEvaluationEventType::BlockedRegionCollision:
		++BlockedRegionCollisionCount;
		SetFloatMetric(TEXT("blocked_region_collision_count"), BlockedRegionCollisionCount);
		break;
	case EEpisodeEvaluationEventType::PedestrianCollision:
		++PedestrianCollisionCount;
		SetFloatMetric(TEXT("pedestrian_collision_count"), PedestrianCollisionCount);
		break;
	default:
		break;
	}
}

bool UEpisodeEvaluationSubsystem::HasWarningEventsOrScore() const
{
	if (CurrentScore < 0.0) return true;

	for (const FEpisodeEvaluationEvent& event : CurrentResult.Events)
	{
		if (event.Severity == EEpisodeEvaluationEventSeverity::Warning) return true;
	}

	return false;
}

bool UEpisodeEvaluationSubsystem::IsRobotActor(const AActor* actor) const
{
	return IsValid(actor) && actor == ActiveRuntimeContext.RobotActor.Get();
}

bool UEpisodeEvaluationSubsystem::ContainsRuntimeActor(const TArray<TObjectPtr<AActor>>& actors, const AActor* actor) const
{
	if (!IsValid(actor)) return false;

	for (const TObjectPtr<AActor>& runtimeActor : actors)
	{
		if (runtimeActor.Get() == actor) return true;
	}

	return false;
}

FString UEpisodeEvaluationSubsystem::GetActorInstanceId(const AActor* actor) const
{
	if (!actor) return FString();

	if (IsRobotActor(actor)) return ActiveRuntimeContext.RobotInstanceId;

	if (const AEpisodeGroundRegion* groundRegion = Cast<AEpisodeGroundRegion>(actor))
	{
		if (!groundRegion->RegionSpec.RegionId.IsEmpty()) return groundRegion->RegionSpec.RegionId;
	}

	if (const UEpisodePlaceableComponent* placeableComponent = actor->FindComponentByClass<UEpisodePlaceableComponent>())
	{
		if (!placeableComponent->InstanceId.IsEmpty()) return placeableComponent->InstanceId;
	}

	return actor->GetName();
}

double UEpisodeEvaluationSubsystem::GetElapsedTimeSeconds() const
{
	const UWorld* world = GetWorld();
	if (!world) return 0.0;

	return FMath::Max(0.0, world->GetTimeSeconds() - EvaluationStartTimeSeconds);
}

void UEpisodeEvaluationSubsystem::EndForTimeout()
{
	FlushActiveNearMisses();

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("평가 제한 시간 도달 | Episode: %s, Elapsed: %.2fs, Limit: %.2fs"),
		*ActiveRuntimeContext.EpisodeId,
		GetElapsedTimeSeconds(),
		TimeLimitSeconds);

	AddEvaluationEvent(
		EEpisodeEvaluationEventType::Timeout,
		EEpisodeEvaluationEventSeverity::Failure,
		TEXT("제한 시간을 초과함."));

	FinishEpisode(false, EEpisodeEvaluationOutcome::Failure, EEpisodeEvaluationTerminalReason::Timeout);
}
