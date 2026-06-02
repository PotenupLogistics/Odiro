#include "Episode/EpisodeEvaluationSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Episode/Actors/EpisodeGroundRegion.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeEvaluation, Log, All);

namespace
{
	template <typename TEnum>
	FString ToEvaluationEnumString(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}
}

bool UEpisodeEvaluationSubsystem::StartEvaluation(
	const FEpisodeEvaluationConfig& EvaluationConfig,
	const FEpisodeRuntimeContext& RuntimeContext,
	double InTimeLimitSeconds)
{
	if (bEvaluating)
	{
		StopEvaluation();
	}

	ActiveEvaluationConfig = EvaluationConfig;
	ActiveRuntimeContext = RuntimeContext;

	CurrentResult = FEpisodeEvaluationResult{};
	CurrentResult.EpisodeId = RuntimeContext.EpisodeId;
	CurrentResult.Outcome = EEpisodeEvaluationOutcome::Running;
	CurrentResult.TerminalReason = EEpisodeEvaluationTerminalReason::None;

	UWorld* World = GetWorld();
	EvaluationStartTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	TimeLimitSeconds = FMath::Max(0.0, InTimeLimitSeconds);
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
		TEXT("Evaluation started | Episode: %s, TimeLimit: %.2fs, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d, NearMissDistance: %.1fcm"),
		*RuntimeContext.EpisodeId,
		TimeLimitSeconds,
		*RuntimeContext.RobotInstanceId,
		RuntimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		RuntimeContext.RuntimeActors.Num(),
		RuntimeContext.GroundRegionActors.Num(),
		RuntimeContext.StaticObstacleActors.Num(),
		RuntimeContext.PedestrianActors.Num(),
		ActiveEvaluationConfig.NearMissDistanceCm);

	if (!IsValid(RuntimeContext.RobotActor))
	{
		UE_LOG(LogEpisodeEvaluation, Warning, TEXT("Evaluation started without a valid robot actor | Episode: %s"), *RuntimeContext.EpisodeId);
	}

	return true;
}

void UEpisodeEvaluationSubsystem::StopEvaluation()
{
	if (bEvaluating)
	{
		UE_LOG(LogEpisodeEvaluation, Log, TEXT("Evaluation stopped | Episode: %s"), *ActiveRuntimeContext.EpisodeId);
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

void UEpisodeEvaluationSubsystem::RequestEndEpisode(const FEpisodeEvaluationResult& Result)
{
	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	const TArray<FEpisodeEvaluationEvent> ExistingEvents = CurrentResult.Events;
	const TMap<FString, FEpisodeParamValue> ExistingMetrics = CurrentResult.Metrics;
	CurrentResult = Result;

	if (ExistingEvents.Num() > 0)
	{
		bool bResultAlreadyContainsExistingEvents = CurrentResult.Events.Num() >= ExistingEvents.Num();
		if (bResultAlreadyContainsExistingEvents)
		{
			for (int32 Index = 0; Index < ExistingEvents.Num(); ++Index)
			{
				if (CurrentResult.Events[Index].EventType != ExistingEvents[Index].EventType
					|| CurrentResult.Events[Index].ElapsedTimeSeconds != ExistingEvents[Index].ElapsedTimeSeconds)
				{
					bResultAlreadyContainsExistingEvents = false;
					break;
				}
			}
		}

		if (CurrentResult.Events.Num() == 0)
		{
			CurrentResult.Events = ExistingEvents;
		}
		else if (!bResultAlreadyContainsExistingEvents)
		{
			TArray<FEpisodeEvaluationEvent> MergedEvents = ExistingEvents;
			for (FEpisodeEvaluationEvent Event : CurrentResult.Events)
			{
				Event.EventIndex = MergedEvents.Num();
				MergedEvents.Add(Event);
			}
			CurrentResult.Events = MergedEvents;
		}
	}

	for (const TPair<FString, FEpisodeParamValue>& Pair : ExistingMetrics)
	{
		if (!CurrentResult.Metrics.Contains(Pair.Key))
		{
			CurrentResult.Metrics.Add(Pair.Key, Pair.Value);
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
		TEXT("Evaluation ended | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Score: %.2f, Events: %d, Metrics: %d"),
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

void UEpisodeEvaluationSubsystem::Tick(float DeltaTime)
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

FEpisodeParamValue UEpisodeEvaluationSubsystem::MakeFloatParam(double Value)
{
	FEpisodeParamValue ParamValue;
	ParamValue.Type = EEpisodeParamValueType::Float;
	ParamValue.FloatValue = Value;
	return ParamValue;
}

FEpisodeParamValue UEpisodeEvaluationSubsystem::MakeStringParam(const FString& Value)
{
	FEpisodeParamValue ParamValue;
	ParamValue.Type = EEpisodeParamValueType::String;
	ParamValue.StringValue = Value;
	return ParamValue;
}

void UEpisodeEvaluationSubsystem::AddEvaluationEvent(
	EEpisodeEvaluationEventType EventType,
	EEpisodeEvaluationEventSeverity Severity,
	const FString& Message)
{
	AddEvaluationEventWithDetails(
		EventType,
		Severity,
		Message,
		FString(),
		IsValid(ActiveRuntimeContext.RobotActor)
			? ActiveRuntimeContext.RobotActor->GetActorLocation()
			: FVector::ZeroVector,
		0.0,
		TMap<FString, FEpisodeParamValue>());
}

void UEpisodeEvaluationSubsystem::AddEvaluationEventWithDetails(
	EEpisodeEvaluationEventType EventType,
	EEpisodeEvaluationEventSeverity Severity,
	const FString& Message,
	const FString& TargetInstanceId,
	const FVector& Location,
	double Value,
	const TMap<FString, FEpisodeParamValue>& Properties)
{
	FEpisodeEvaluationEvent Event;
	Event.EventIndex = CurrentResult.Events.Num();
	Event.ElapsedTimeSeconds = GetElapsedTimeSeconds();
	Event.EventType = EventType;
	Event.Severity = Severity;
	Event.Message = Message;
	Event.TargetInstanceId = TargetInstanceId;
	Event.Location = Location;
	Event.Value = Value;
	Event.Properties = Properties;
	if (IsValid(ActiveRuntimeContext.RobotActor))
	{
		Event.SubjectInstanceId = ActiveRuntimeContext.RobotInstanceId;
	}

	CurrentResult.Events.Add(Event);

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("Evaluation event | Episode: %s, Index: %d, Type: %s, Severity: %s, Subject: %s, Target: %s, Time: %.2fs, Value: %.2f, Message: %s"),
		*CurrentResult.EpisodeId,
		Event.EventIndex,
		*ToEvaluationEnumString(Event.EventType),
		*ToEvaluationEnumString(Event.Severity),
		*Event.SubjectInstanceId,
		*Event.TargetInstanceId,
		Event.ElapsedTimeSeconds,
		Event.Value,
		*Event.Message);
}

void UEpisodeEvaluationSubsystem::BindEvaluationHitDelegates()
{
	UnbindEvaluationHitDelegates();

	BindActorHitDelegates(ActiveRuntimeContext.RobotActor.Get());

	for (const TObjectPtr<AActor>& StaticObstacleActor : ActiveRuntimeContext.StaticObstacleActors)
	{
		BindActorHitDelegates(StaticObstacleActor.Get());
	}

	for (const TObjectPtr<AActor>& PedestrianActor : ActiveRuntimeContext.PedestrianActors)
	{
		BindActorHitDelegates(PedestrianActor.Get());
	}

	for (const TObjectPtr<AActor>& GroundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		BindActorHitDelegates(GroundRegionActor.Get());
	}
}

void UEpisodeEvaluationSubsystem::BindActorHitDelegates(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent) || IsHitComponentBound(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->OnComponentHit.RemoveDynamic(this, &UEpisodeEvaluationSubsystem::HandleObservedComponentHit);
		PrimitiveComponent->OnComponentHit.AddDynamic(this, &UEpisodeEvaluationSubsystem::HandleObservedComponentHit);
		PrimitiveComponent->SetNotifyRigidBodyCollision(true);
		BoundHitComponents.Add(PrimitiveComponent);
	}
}

void UEpisodeEvaluationSubsystem::UnbindEvaluationHitDelegates()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& BoundComponent : BoundHitComponents)
	{
		if (UPrimitiveComponent* PrimitiveComponent = BoundComponent.Get())
		{
			PrimitiveComponent->OnComponentHit.RemoveDynamic(this, &UEpisodeEvaluationSubsystem::HandleObservedComponentHit);
		}
	}

	BoundHitComponents.Reset();
}

bool UEpisodeEvaluationSubsystem::IsHitComponentBound(const UPrimitiveComponent* PrimitiveComponent) const
{
	if (!PrimitiveComponent)
	{
		return false;
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& BoundComponent : BoundHitComponents)
	{
		if (BoundComponent.Get() == PrimitiveComponent)
		{
			return true;
		}
	}

	return false;
}

void UEpisodeEvaluationSubsystem::HandleObservedComponentHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	(void)OtherComp;
	(void)NormalImpulse;

	if (!bEvaluating || !HitComponent)
	{
		return;
	}

	AActor* HitOwner = HitComponent->GetOwner();
	AActor* TargetActor = nullptr;
	if (IsRobotActor(HitOwner))
	{
		TargetActor = OtherActor;
	}
	else if (IsRobotActor(OtherActor))
	{
		TargetActor = HitOwner;
	}

	if (!IsValid(TargetActor) || IsRobotActor(TargetActor))
	{
		return;
	}

	const FVector ImpactPoint(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
	const FVector EventLocation = ImpactPoint.IsNearlyZero()
		? TargetActor->GetActorLocation()
		: ImpactPoint;

	if (AEpisodeGroundRegion* GroundRegion = Cast<AEpisodeGroundRegion>(TargetActor))
	{
		if (GroundRegion->RegionSpec.RegionType == EEpisodeGroundRegionType::Blocked)
		{
			RecordCollisionEvent(
				EEpisodeEvaluationEventType::BlockedRegionCollision,
				TargetActor,
				EventLocation,
				ActiveEvaluationConfig.BlockedRegionCollisionScore,
				TEXT("Robot collided with a blocked region."));
		}
		return;
	}

	if (ContainsRuntimeActor(ActiveRuntimeContext.StaticObstacleActors, TargetActor))
	{
		RecordCollisionEvent(
			EEpisodeEvaluationEventType::StaticObstacleCollision,
			TargetActor,
			EventLocation,
			ActiveEvaluationConfig.StaticObstacleCollisionScore,
			TEXT("Robot collided with a static obstacle."));
		return;
	}

	if (ContainsRuntimeActor(ActiveRuntimeContext.PedestrianActors, TargetActor))
	{
		RecordCollisionEvent(
			EEpisodeEvaluationEventType::PedestrianCollision,
			TargetActor,
			EventLocation,
			ActiveEvaluationConfig.PedestrianCollisionScore,
			TEXT("Robot collided with a pedestrian."));
	}
}

bool UEpisodeEvaluationSubsystem::CheckGoalReached()
{
	if (!ActiveRuntimeContext.bHasGoalLocation || !IsValid(ActiveRuntimeContext.RobotActor))
	{
		return false;
	}

	const double AcceptanceRadiusCm = FMath::Max(0.0, ActiveEvaluationConfig.GoalAcceptanceRadiusCm);
	if (AcceptanceRadiusCm <= 0.0)
	{
		return false;
	}

	const FVector RobotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	const double GoalDistanceCm = FVector::Dist2D(RobotLocation, ActiveRuntimeContext.GoalLocation);
	if (GoalDistanceCm > AcceptanceRadiusCm)
	{
		return false;
	}

	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	++GoalReachedCount;
	SetFloatMetric(TEXT("goal_reached"), GoalReachedCount);
	SetFloatMetric(TEXT("goal_distance_m"), GoalDistanceCm / 100.0);

	FinishEpisode(
		true,
		HasWarningEventsOrScore() ? EEpisodeEvaluationOutcome::Warning : EEpisodeEvaluationOutcome::Success,
		EEpisodeEvaluationTerminalReason::GoalReached);
	return true;
}

bool UEpisodeEvaluationSubsystem::CheckRobotFall()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor))
	{
		return false;
	}

	const double FallAngleDegrees = FMath::Max(0.0, ActiveEvaluationConfig.FallAngleDegrees);
	if (FallAngleDegrees <= 0.0)
	{
		return false;
	}

	const FVector RobotUp = ActiveRuntimeContext.RobotActor->GetActorUpVector().GetSafeNormal();
	const double UpDot = FMath::Clamp(FVector::DotProduct(RobotUp, FVector::UpVector), -1.0, 1.0);
	const double TiltAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(UpDot));
	if (TiltAngleDegrees < FallAngleDegrees)
	{
		return false;
	}

	++RobotFallCount;
	SetFloatMetric(TEXT("robot_fall_count"), RobotFallCount);
	SetFloatMetric(TEXT("robot_fall_angle_deg"), TiltAngleDegrees);

	TMap<FString, FEpisodeParamValue> Properties;
	Properties.Add(TEXT("tilt_angle_deg"), MakeFloatParam(TiltAngleDegrees));
	Properties.Add(TEXT("fall_angle_threshold_deg"), MakeFloatParam(FallAngleDegrees));
	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::RobotFall,
		EEpisodeEvaluationEventSeverity::Failure,
		TEXT("Robot exceeded the fall angle threshold."),
		FString(),
		ActiveRuntimeContext.RobotActor->GetActorLocation(),
		TiltAngleDegrees,
		Properties);

	FinishEpisode(false, EEpisodeEvaluationOutcome::Failure, EEpisodeEvaluationTerminalReason::RobotFall);
	return true;
}

void UEpisodeEvaluationSubsystem::UpdateBlockedRegionViolations()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor))
	{
		return;
	}

	const FVector RobotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> ObservedBlockedRegionIds;

	for (const TObjectPtr<AActor>& GroundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		AEpisodeGroundRegion* GroundRegion = Cast<AEpisodeGroundRegion>(GroundRegionActor.Get());
		if (!IsValid(GroundRegion)
			|| GroundRegion->RegionSpec.RegionType != EEpisodeGroundRegionType::Blocked)
		{
			continue;
		}

		const FString RegionId = GetActorInstanceId(GroundRegion);
		ObservedBlockedRegionIds.Add(RegionId);

		FBlockedRegionState& RegionState = BlockedRegionStates.FindOrAdd(RegionId);
		const bool bInside = GroundRegion->ContainsWorldLocation2D(RobotLocation);
		if (!bInside)
		{
			RegionState.bInside = false;
			continue;
		}

		if (RegionState.bInside)
		{
			continue;
		}

		RegionState.bInside = true;

		RecordCollisionEvent(
			EEpisodeEvaluationEventType::BlockedRegionCollision,
			GroundRegion,
			RobotLocation,
			ActiveEvaluationConfig.BlockedRegionCollisionScore,
			TEXT("Robot entered a blocked region."));
	}

	TArray<FString> MissingRegionIds;
	for (const TPair<FString, FBlockedRegionState>& Pair : BlockedRegionStates)
	{
		if (!ObservedBlockedRegionIds.Contains(Pair.Key))
		{
			MissingRegionIds.Add(Pair.Key);
		}
	}

	for (const FString& MissingRegionId : MissingRegionIds)
	{
		BlockedRegionStates.Remove(MissingRegionId);
	}
}

void UEpisodeEvaluationSubsystem::UpdatePenaltyRegionViolations()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor))
	{
		return;
	}

	const double ElapsedTimeSeconds = GetElapsedTimeSeconds();
	const FVector RobotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> ObservedPenaltyRegionIds;

	for (const TObjectPtr<AActor>& GroundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		AEpisodeGroundRegion* GroundRegion = Cast<AEpisodeGroundRegion>(GroundRegionActor.Get());
		if (!IsValid(GroundRegion) || GroundRegion->RegionSpec.RegionType != EEpisodeGroundRegionType::Penalty)
		{
			continue;
		}

		const FString RegionId = GetActorInstanceId(GroundRegion);
		ObservedPenaltyRegionIds.Add(RegionId);
		FPenaltyRegionState& RegionState = PenaltyRegionStates.FindOrAdd(RegionId);
		const bool bInside = GroundRegion->ContainsWorldLocation2D(RobotLocation);
		if (!bInside)
		{
			RegionState.bInside = false;
			RegionState.bEventRecorded = false;
			RegionState.EnterTimeSeconds = 0.0;
			continue;
		}

		if (!RegionState.bInside)
		{
			RegionState.bInside = true;
			RegionState.bEventRecorded = false;
			RegionState.EnterTimeSeconds = ElapsedTimeSeconds;
		}

		const double RequiredDurationSeconds = FMath::Max(0.0, GroundRegion->RegionSpec.ViolationAfterSeconds);
		if (RegionState.bEventRecorded || ElapsedTimeSeconds - RegionState.EnterTimeSeconds < RequiredDurationSeconds)
		{
			continue;
		}

		const double ScoreDelta = ActiveEvaluationConfig.PenaltyRegionViolationScore;
		TMap<FString, FEpisodeParamValue> Properties;
		Properties.Add(TEXT("region_id"), MakeStringParam(RegionId));
		Properties.Add(TEXT("enter_time_s"), MakeFloatParam(RegionState.EnterTimeSeconds));
		Properties.Add(TEXT("duration_s"), MakeFloatParam(ElapsedTimeSeconds - RegionState.EnterTimeSeconds));
		Properties.Add(TEXT("violation_after_s"), MakeFloatParam(RequiredDurationSeconds));
		AddEvaluationEventWithDetails(
			EEpisodeEvaluationEventType::PenaltyRegionViolation,
			EEpisodeEvaluationEventSeverity::Warning,
			TEXT("Robot violated a penalty region."),
			RegionId,
			RobotLocation,
			ScoreDelta,
			Properties);
		AddScore(ScoreDelta);

		++PenaltyRegionViolationCount;
		SetFloatMetric(TEXT("penalty_region_violation_count"), PenaltyRegionViolationCount);
		RegionState.bEventRecorded = true;
	}

	TArray<FString> MissingRegionIds;
	for (const TPair<FString, FPenaltyRegionState>& Pair : PenaltyRegionStates)
	{
		if (!ObservedPenaltyRegionIds.Contains(Pair.Key))
		{
			MissingRegionIds.Add(Pair.Key);
		}
	}

	for (const FString& MissingRegionId : MissingRegionIds)
	{
		PenaltyRegionStates.Remove(MissingRegionId);
	}
}

void UEpisodeEvaluationSubsystem::UpdateNearMisses()
{
	const FEpisodeEvaluationConfig& EvaluationConfig = ActiveEvaluationConfig;
	if (EvaluationConfig.NearMissDistanceCm <= 0.0 || !IsValid(ActiveRuntimeContext.RobotActor))
	{
		return;
	}

	const double ElapsedTimeSeconds = GetElapsedTimeSeconds();
	const FVector RobotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> ObservedPedestrianIds;

	for (int32 Index = 0; Index < ActiveRuntimeContext.PedestrianActors.Num(); ++Index)
	{
		AActor* PedestrianActor = ActiveRuntimeContext.PedestrianActors[Index].Get();
		if (!IsValid(PedestrianActor))
		{
			continue;
		}

		const FString PedestrianInstanceId = ActiveRuntimeContext.PedestrianInstanceIds.IsValidIndex(Index)
			? ActiveRuntimeContext.PedestrianInstanceIds[Index]
			: PedestrianActor->GetName();
		ObservedPedestrianIds.Add(PedestrianInstanceId);

		const FVector PedestrianLocation = PedestrianActor->GetActorLocation();
		const double DistanceCm = FVector::Dist2D(RobotLocation, PedestrianLocation);
		FNearMissIntervalState* ActiveState = ActiveNearMisses.Find(PedestrianInstanceId);

		if (DistanceCm <= EvaluationConfig.NearMissDistanceCm)
		{
			if (!ActiveState)
			{
				ActiveState = &ActiveNearMisses.Add(PedestrianInstanceId);
				ActiveState->StartTimeSeconds = ElapsedTimeSeconds;
			}

			ActiveState->LastInsideTimeSeconds = ElapsedTimeSeconds;
			if (DistanceCm < ActiveState->MinDistanceCm)
			{
				ActiveState->MinDistanceCm = DistanceCm;
				ActiveState->ClosestRobotLocation = RobotLocation;
				ActiveState->ClosestPedestrianLocation = PedestrianLocation;
			}
			continue;
		}

		if (ActiveState && ElapsedTimeSeconds - ActiveState->LastInsideTimeSeconds >= NearMissClearanceGraceSeconds)
		{
			const FNearMissIntervalState State = *ActiveState;
			ActiveNearMisses.Remove(PedestrianInstanceId);
			CloseNearMissInterval(PedestrianInstanceId, State, State.LastInsideTimeSeconds);
		}
	}

	TArray<FString> MissingPedestrianIds;
	for (const TPair<FString, FNearMissIntervalState>& Pair : ActiveNearMisses)
	{
		if (!ObservedPedestrianIds.Contains(Pair.Key))
		{
			MissingPedestrianIds.Add(Pair.Key);
		}
	}

	for (const FString& PedestrianInstanceId : MissingPedestrianIds)
	{
		if (const FNearMissIntervalState* ActiveState = ActiveNearMisses.Find(PedestrianInstanceId))
		{
			const FNearMissIntervalState State = *ActiveState;
			ActiveNearMisses.Remove(PedestrianInstanceId);
			CloseNearMissInterval(PedestrianInstanceId, State, ElapsedTimeSeconds);
		}
	}
}

void UEpisodeEvaluationSubsystem::FlushActiveNearMisses()
{
	const double EndTimeSeconds = GetElapsedTimeSeconds();
	TArray<FString> PedestrianInstanceIds;
	ActiveNearMisses.GetKeys(PedestrianInstanceIds);

	for (const FString& PedestrianInstanceId : PedestrianInstanceIds)
	{
		if (const FNearMissIntervalState* ActiveState = ActiveNearMisses.Find(PedestrianInstanceId))
		{
			CloseNearMissInterval(PedestrianInstanceId, *ActiveState, EndTimeSeconds);
		}
	}

	ActiveNearMisses.Reset();
}

void UEpisodeEvaluationSubsystem::CloseNearMissInterval(
	const FString& PedestrianInstanceId,
	const FNearMissIntervalState& State,
	double EndTimeSeconds)
{
	if (State.MinDistanceCm == TNumericLimits<double>::Max())
	{
		return;
	}

	const double DurationSeconds = FMath::Max(0.0, EndTimeSeconds - State.StartTimeSeconds);
	const double ScoreDelta = ActiveEvaluationConfig.PedestrianNearMissScore;

	FEpisodeEvaluationEvent Event;
	Event.EventIndex = CurrentResult.Events.Num();
	Event.ElapsedTimeSeconds = EndTimeSeconds;
	Event.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
	Event.Severity = EEpisodeEvaluationEventSeverity::Warning;
	Event.SubjectInstanceId = ActiveRuntimeContext.RobotInstanceId;
	Event.TargetInstanceId = PedestrianInstanceId;
	Event.Location = State.ClosestRobotLocation;
	Event.Value = ScoreDelta;
	Event.Message = TEXT("Pedestrian near-miss interval.");
	Event.Properties.Add(TEXT("start_time_s"), MakeFloatParam(State.StartTimeSeconds));
	Event.Properties.Add(TEXT("end_time_s"), MakeFloatParam(EndTimeSeconds));
	Event.Properties.Add(TEXT("duration_s"), MakeFloatParam(DurationSeconds));
	Event.Properties.Add(TEXT("min_distance_m"), MakeFloatParam(State.MinDistanceCm / 100.0));
	Event.Properties.Add(TEXT("pedestrian_id"), MakeStringParam(PedestrianInstanceId));
	CurrentResult.Events.Add(Event);

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("Evaluation event | Episode: %s, Index: %d, Type: %s, Severity: %s, Subject: %s, Target: %s, Duration: %.2fs, MinDistance: %.2fm, ScoreDelta: %.2f"),
		*CurrentResult.EpisodeId,
		Event.EventIndex,
		*ToEvaluationEnumString(Event.EventType),
		*ToEvaluationEnumString(Event.Severity),
		*Event.SubjectInstanceId,
		*Event.TargetInstanceId,
		DurationSeconds,
		State.MinDistanceCm / 100.0,
		ScoreDelta);

	AddScore(ScoreDelta);
	++NearMissCount;
	NearMissTotalDurationSeconds += DurationSeconds;
	NearMissMinDistanceCm = FMath::Min(NearMissMinDistanceCm, State.MinDistanceCm);

	SetFloatMetric(TEXT("near_miss_count"), NearMissCount);
	SetFloatMetric(TEXT("near_miss_total_duration_s"), NearMissTotalDurationSeconds);
	SetFloatMetric(TEXT("near_miss_min_distance_m"), NearMissMinDistanceCm / 100.0);
}

void UEpisodeEvaluationSubsystem::SetFloatMetric(const FString& Key, double Value)
{
	CurrentResult.Metrics.Add(Key, MakeFloatParam(Value));
}

void UEpisodeEvaluationSubsystem::AddScore(double ScoreDelta)
{
	CurrentScore += ScoreDelta;
	SetFloatMetric(TEXT("score"), CurrentScore);
}

void UEpisodeEvaluationSubsystem::FinishEpisode(
	bool bSuccess,
	EEpisodeEvaluationOutcome Outcome,
	EEpisodeEvaluationTerminalReason TerminalReason)
{
	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	FEpisodeEvaluationResult Result = CurrentResult;
	Result.EpisodeId = ActiveRuntimeContext.EpisodeId;
	Result.bSuccess = bSuccess;
	Result.Outcome = Outcome;
	Result.TerminalReason = TerminalReason;
	Result.DurationSeconds = GetElapsedTimeSeconds();
	RequestEndEpisode(Result);
}

void UEpisodeEvaluationSubsystem::RecordCollisionEvent(
	EEpisodeEvaluationEventType EventType,
	AActor* TargetActor,
	const FVector& Location,
	double ScoreDelta,
	const FString& Message)
{
	if (!bEvaluating)
	{
		return;
	}

	const FString TargetInstanceId = GetActorInstanceId(TargetActor);
	const FString CollisionEventKey = FString::Printf(TEXT("%s:%s"), *ToEvaluationEnumString(EventType), *TargetInstanceId);
	const double ElapsedTimeSeconds = GetElapsedTimeSeconds();
	if (const double* LastRecordedTime = LastCollisionEventTimes.Find(CollisionEventKey))
	{
		if (ElapsedTimeSeconds - *LastRecordedTime < CollisionEventCooldownSeconds)
		{
			return;
		}
	}

	LastCollisionEventTimes.Add(CollisionEventKey, ElapsedTimeSeconds);

	TMap<FString, FEpisodeParamValue> Properties;
	Properties.Add(TEXT("target_id"), MakeStringParam(TargetInstanceId));
	if (TargetActor)
	{
		Properties.Add(TEXT("target_actor"), MakeStringParam(TargetActor->GetName()));
	}
	if (const AEpisodeGroundRegion* GroundRegion = Cast<AEpisodeGroundRegion>(TargetActor))
	{
		Properties.Add(TEXT("region_id"), MakeStringParam(GetActorInstanceId(GroundRegion)));
	}

	AddEvaluationEventWithDetails(
		EventType,
		EEpisodeEvaluationEventSeverity::Warning,
		Message,
		TargetInstanceId,
		Location,
		ScoreDelta,
		Properties);
	AddScore(ScoreDelta);

	switch (EventType)
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
	if (CurrentScore < 0.0)
	{
		return true;
	}

	for (const FEpisodeEvaluationEvent& Event : CurrentResult.Events)
	{
		if (Event.Severity == EEpisodeEvaluationEventSeverity::Warning)
		{
			return true;
		}
	}

	return false;
}

bool UEpisodeEvaluationSubsystem::IsRobotActor(const AActor* Actor) const
{
	return IsValid(Actor) && Actor == ActiveRuntimeContext.RobotActor.Get();
}

bool UEpisodeEvaluationSubsystem::ContainsRuntimeActor(const TArray<TObjectPtr<AActor>>& Actors, const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	for (const TObjectPtr<AActor>& RuntimeActor : Actors)
	{
		if (RuntimeActor.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

FString UEpisodeEvaluationSubsystem::GetActorInstanceId(const AActor* Actor) const
{
	if (!Actor)
	{
		return FString();
	}

	if (IsRobotActor(Actor))
	{
		return ActiveRuntimeContext.RobotInstanceId;
	}

	if (const AEpisodeGroundRegion* GroundRegion = Cast<AEpisodeGroundRegion>(Actor))
	{
		if (!GroundRegion->RegionSpec.RegionId.IsEmpty())
		{
			return GroundRegion->RegionSpec.RegionId;
		}
	}

	if (const UEpisodePlaceableComponent* PlaceableComponent = Actor->FindComponentByClass<UEpisodePlaceableComponent>())
	{
		if (!PlaceableComponent->InstanceId.IsEmpty())
		{
			return PlaceableComponent->InstanceId;
		}
	}

	return Actor->GetName();
}

double UEpisodeEvaluationSubsystem::GetElapsedTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.0;

	return FMath::Max(0.0, World->GetTimeSeconds() - EvaluationStartTimeSeconds);
}

void UEpisodeEvaluationSubsystem::EndForTimeout()
{
	FlushActiveNearMisses();

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("Evaluation timeout reached | Episode: %s, Elapsed: %.2fs, Limit: %.2fs"),
		*ActiveRuntimeContext.EpisodeId,
		GetElapsedTimeSeconds(),
		TimeLimitSeconds);

	AddEvaluationEvent(
		EEpisodeEvaluationEventType::Timeout,
		EEpisodeEvaluationEventSeverity::Failure,
		TEXT("Time limit exceeded."));

	FinishEpisode(false, EEpisodeEvaluationOutcome::Failure, EEpisodeEvaluationTerminalReason::Timeout);
}
