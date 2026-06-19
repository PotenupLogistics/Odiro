
#include "Scenario/ScenarioEvaluationSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPolicyEventSnapshot.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioEvaluation, Log, All);

namespace
{
	template <typename TEnum>
	FString ToEvaluationEnumString(TEnum value)
	{
		if (const UEnum* enumValue = StaticEnum<TEnum>()) return enumValue->GetNameStringByValue(static_cast<int64>(value));

		return TEXT("Unknown");
	}

	bool ShouldRecordDeliveryBotSimulationFailure(EDeliveryBotSimulationFailureType failureType)
	{
		switch (failureType)
		{
		case EDeliveryBotSimulationFailureType::RobotTipOver:
		case EDeliveryBotSimulationFailureType::PathFindingFailed:
		case EDeliveryBotSimulationFailureType::Stuck:
			return true;
		default:
			return false;
		}
	}
}

bool UScenarioEvaluationSubsystem::StartEvaluation(
	const FScenarioEvaluationConfig& evaluationConfig,
	const FScenarioRuntimeContext& runtimeContext,
	double inTimeLimitSeconds)
{
	if (bEvaluating)
	{
		StopEvaluation();
	}

	if (!IsValid(runtimeContext.RobotActor))
	{
		UE_LOG(LogScenarioEvaluation, Warning, TEXT("평가 시작 거부: 런타임 컨텍스트에 유효한 로봇 액터가 없음 | Episode: %s"), *runtimeContext.EpisodeId);
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
	NearMissCount = 0;
	NearMissTotalDurationSeconds = 0.0;
	NearMissMinDistanceCm = TNumericLimits<double>::Max();
	ActiveNearMisses.Reset();
	PenaltyRegionStates.Reset();
	BlockedRegionStates.Reset();
	LastCollisionEventTimes.Reset();
	GoalReachedCount = 0;
	RobotTipOverCount = 0;
	StaticObstacleCollisionCount = 0;
	BlockedRegionCollisionCount = 0;
	PenaltyRegionViolationCount = 0;
	PedestrianCollisionCount = 0;
	SetFloatMetric(TEXT("goal_reached"), 0.0);
	SetFloatMetric(TEXT("robot_tip_over_count"), 0.0);
	SetFloatMetric(TEXT("static_obstacle_collision_count"), 0.0);
	SetFloatMetric(TEXT("blocked_region_collision_count"), 0.0);
	SetFloatMetric(TEXT("penalty_region_violation_count"), 0.0);
	SetFloatMetric(TEXT("pedestrian_collision_count"), 0.0);
	SetFloatMetric(TEXT("near_miss_count"), 0.0);
	SetFloatMetric(TEXT("near_miss_total_duration_s"), 0.0);
	BindEvaluationHitDelegates();
	bEvaluating = true;

	UE_LOG(
		LogScenarioEvaluation,
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

void UScenarioEvaluationSubsystem::StopEvaluation()
{
	if (bEvaluating)
	{
		UE_LOG(LogScenarioEvaluation, Log, TEXT("평가 중지 | Episode: %s"), *ActiveRuntimeContext.EpisodeId);
	}

	bEvaluating = false;
	UnbindEvaluationHitDelegates();
	ActiveEvaluationConfig = FScenarioEvaluationConfig{};
	ActiveRuntimeContext = FScenarioRuntimeContext{};
	TimeLimitSeconds = 0.0;
	EvaluationStartTimeSeconds = 0.0;
	NearMissCount = 0;
	NearMissTotalDurationSeconds = 0.0;
	NearMissMinDistanceCm = TNumericLimits<double>::Max();
	ActiveNearMisses.Reset();
	PenaltyRegionStates.Reset();
	BlockedRegionStates.Reset();
	LastCollisionEventTimes.Reset();
	GoalReachedCount = 0;
	RobotTipOverCount = 0;
	StaticObstacleCollisionCount = 0;
	BlockedRegionCollisionCount = 0;
	PenaltyRegionViolationCount = 0;
	PedestrianCollisionCount = 0;
}

void UScenarioEvaluationSubsystem::RequestEndEpisode(const FEpisodeEvaluationResult& result)
{
	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	const TArray<FEpisodeEvaluationEvent> existingEvents = CurrentResult.Events;
	const TMap<FString, FScenarioParamValue> existingMetrics = CurrentResult.Metrics;
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

	for (const TPair<FString, FScenarioParamValue>& pair : existingMetrics)
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
		LogScenarioEvaluation,
		Warning,
		TEXT("평가 종료 | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Events: %d, Metrics: %d"),
		*CurrentResult.EpisodeId,
		CurrentResult.bSuccess ? TEXT("true") : TEXT("false"),
		*ToEvaluationEnumString(CurrentResult.Outcome),
		*ToEvaluationEnumString(CurrentResult.TerminalReason),
		CurrentResult.DurationSeconds,
		CurrentResult.Events.Num(),
		CurrentResult.Metrics.Num());

	OnEpisodeEnded.Broadcast(CurrentResult);
}


void UScenarioEvaluationSubsystem::Tick(float deltaTime)
{
	if (!bEvaluating) return;

	if (CheckGoalReached()) return;
	if (CheckRobotTipOver()) return;
	UpdateBlockedRegionViolations();
	UpdatePenaltyRegionViolations();

	UpdateNearMisses();

	if (TimeLimitSeconds > 0.0 && GetElapsedTimeSeconds() >= TimeLimitSeconds)
	{
		EndForTimeout();
	}
}

bool UScenarioEvaluationSubsystem::IsTickable() const
{
	return bEvaluating;
}

TStatId UScenarioEvaluationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UScenarioEvaluationSubsystem, STATGROUP_Tickables);
}

FScenarioParamValue UScenarioEvaluationSubsystem::MakeFloatParam(double value)
{
	FScenarioParamValue paramValue;
	paramValue.Type = EScenarioParamValueType::Float;
	paramValue.FloatValue = value;
	return paramValue;
}

FScenarioParamValue UScenarioEvaluationSubsystem::MakeBoolParam(bool value)
{
	FScenarioParamValue paramValue;
	paramValue.Type = EScenarioParamValueType::Bool;
	paramValue.BoolValue = value;
	return paramValue;
}

FScenarioParamValue UScenarioEvaluationSubsystem::MakeIntegerParam(int32 value)
{
	FScenarioParamValue paramValue;
	paramValue.Type = EScenarioParamValueType::Integer;
	paramValue.IntegerValue = value;
	return paramValue;
}

FScenarioParamValue UScenarioEvaluationSubsystem::MakeStringParam(const FString& value)
{
	FScenarioParamValue paramValue;
	paramValue.Type = EScenarioParamValueType::String;
	paramValue.StringValue = value;
	return paramValue;
}

FScenarioParamValue UScenarioEvaluationSubsystem::MakeVectorParam(const FVector& value)
{
	FScenarioParamValue paramValue;
	paramValue.Type = EScenarioParamValueType::Vector;
	paramValue.VectorValue = value;
	return paramValue;
}

void UScenarioEvaluationSubsystem::HandleDeliveryBotSimulationFailed(
	ADeliveryBot* DeliveryBotActor,
	const FDeliveryBotSimulationFailureInfo& FailureInfo)
{
	if (!bEvaluating) return;

	if (!IsValid(DeliveryBotActor) || DeliveryBotActor != ActiveRuntimeContext.RobotActor.Get())
	{
		UE_LOG(
			LogScenarioEvaluation,
			Warning,
			TEXT("DeliveryBot 실패 이벤트 무시: 현재 평가 로봇이 아님 | Episode: %s, Actor: %s"),
			*ActiveRuntimeContext.EpisodeId,
			IsValid(DeliveryBotActor) ? *DeliveryBotActor->GetName() : TEXT("null"));
		return;
	}

	const FString failureTypeName = ToEvaluationEnumString(FailureInfo.FailureType);
	if (!ShouldRecordDeliveryBotSimulationFailure(FailureInfo.FailureType))
	{
		UE_LOG(
			LogScenarioEvaluation,
			Warning,
			TEXT("DeliveryBot 실패 이벤트 무시: 평가 대상 실패 유형이 아님 | Episode: %s, Type: %s, Message: %s"),
			*ActiveRuntimeContext.EpisodeId,
			*failureTypeName,
			*FailureInfo.Message);
		return;
	}

	if (ActiveNearMisses.Num() > 0)
	{
		FlushActiveNearMisses();
	}

	const FString failureMessage = FailureInfo.Message.IsEmpty()
		? TEXT("DeliveryBot simulation failed.")
		: FailureInfo.Message;
	const AActor* targetActor = FailureInfo.TargetActor.Get();
	const FString targetActorName = IsValid(targetActor) ? targetActor->GetName() : FString();
	const FString targetInstanceId = GetActorInstanceId(targetActor);

	TMap<FString, FScenarioParamValue> properties;
	const auto addMetricAndProperty = [this, &properties](const FString& key, const FScenarioParamValue& value)
	{
		CurrentResult.Metrics.Add(key, value);
		properties.Add(key, value);
	};

	addMetricAndProperty(TEXT("delivery_bot_failure_type"), MakeStringParam(failureTypeName));
	addMetricAndProperty(TEXT("delivery_bot_failure_message"), MakeStringParam(failureMessage));
	addMetricAndProperty(TEXT("delivery_bot_failure_location_cm"), MakeVectorParam(FailureInfo.LocationCm));
	addMetricAndProperty(TEXT("delivery_bot_failure_time_seconds"), MakeFloatParam(FailureInfo.TimeSeconds));
	addMetricAndProperty(TEXT("delivery_bot_failure_speed_kmh"), MakeFloatParam(FailureInfo.SpeedKmh));
	addMetricAndProperty(TEXT("delivery_bot_failure_target_actor_name"), MakeStringParam(targetActorName));
	addMetricAndProperty(TEXT("failure_type"), MakeStringParam(failureTypeName));
	addMetricAndProperty(TEXT("speed_kmh"), MakeFloatParam(FailureInfo.SpeedKmh));
	if (!targetInstanceId.IsEmpty())
	{
		properties.Add(TEXT("target_id"), MakeStringParam(targetInstanceId));
	}
	if (!targetActorName.IsEmpty())
	{
		properties.Add(TEXT("target_actor"), MakeStringParam(targetActorName));
	}
	FScenarioRuntimeCorridorSurfaceQueryResult robotSurface;
	if (TryFindCorridorSurfaceAtWorldLocation(FailureInfo.LocationCm, robotSurface))
	{
		AddCorridorSnapshotProperties(properties, robotSurface, TEXT("robot"));
	}
	if (IsValid(targetActor))
	{
		AddTargetCorridorSnapshotProperties(properties, targetActor->GetActorLocation());
	}

	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::DeliveryBotSimulationFailure,
		EEpisodeEvaluationEventSeverity::Failure,
		failureMessage,
		targetInstanceId,
		FailureInfo.LocationCm,
		0.0,
		properties);

	UE_LOG(
		LogScenarioEvaluation,
		Warning,
		TEXT("DeliveryBot 실패로 Episode 종료 | Episode: %s, Type: %s, Message: %s, SpeedKmh: %.2f"),
		*ActiveRuntimeContext.EpisodeId,
		*failureTypeName,
		*failureMessage,
		FailureInfo.SpeedKmh);

	FinishEpisode(
		false,
		EEpisodeEvaluationOutcome::Failure,
		EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed);
}

void UScenarioEvaluationSubsystem::ReportDeliveryBotPolicyEvent(
	ADeliveryBot* DeliveryBotActor,
	const FDeliveryBotPolicyEventSnapshot& Snapshot)
{
	if (!bEvaluating) return;

	if (!IsValid(DeliveryBotActor) || DeliveryBotActor != ActiveRuntimeContext.RobotActor.Get())
	{
		return;
	}

	TMap<FString, FScenarioParamValue> properties;
	properties.Add(TEXT("policy_sequence"), MakeIntegerParam(Snapshot.Sequence));
	properties.Add(TEXT("policy_run_time_seconds"), MakeFloatParam(Snapshot.RunTimeSeconds));
	properties.Add(TEXT("policy_event_code"), MakeStringParam(Snapshot.EventCode));
	properties.Add(TEXT("policy_endpoint"), MakeStringParam(Snapshot.Endpoint));
	properties.Add(TEXT("policy_selected"), MakeStringParam(Snapshot.SelectedPolicy));
	properties.Add(TEXT("policy_reason"), MakeStringParam(Snapshot.Reason));

	if (Snapshot.EventType == EEpisodeEvaluationEventType::DeliveryBotPolicyServerFailure)
	{
		properties.Add(TEXT("http_status_code"), MakeIntegerParam(Snapshot.HttpStatusCode));
		properties.Add(TEXT("error_code"), MakeStringParam(Snapshot.ErrorCode));
		properties.Add(TEXT("error_message"), MakeStringParam(Snapshot.ErrorMessage));
		properties.Add(TEXT("retryable"), MakeBoolParam(Snapshot.bRetryable));
		properties.Add(TEXT("python_process_status"), MakeStringParam(Snapshot.PythonProcessStatus));
		properties.Add(TEXT("response_body_snippet"), MakeStringParam(Snapshot.ResponseBodySnippet));
	}

	if (Snapshot.EventType == EEpisodeEvaluationEventType::DeliveryBotPolicyFailure)
	{
		properties.Add(TEXT("error_code"), MakeStringParam(Snapshot.ErrorCode));
		properties.Add(TEXT("error_message"), MakeStringParam(Snapshot.ErrorMessage));
		properties.Add(TEXT("retryable"), MakeBoolParam(Snapshot.bRetryable));
		const FString failureType = Snapshot.ErrorCode.IsEmpty()
			? Snapshot.Reason
			: Snapshot.ErrorCode;
		if (!failureType.IsEmpty())
		{
			properties.Add(TEXT("failure_type"), MakeStringParam(failureType));
		}
	}

	if (Snapshot.EventType == EEpisodeEvaluationEventType::DeliveryBotRepath)
	{
		properties.Add(TEXT("path_status"), MakeStringParam(Snapshot.PathStatus));
		properties.Add(TEXT("path_index"), MakeIntegerParam(Snapshot.PathIndex));
		properties.Add(TEXT("path_length"), MakeIntegerParam(Snapshot.PathLength));
		properties.Add(TEXT("target_path_index"), MakeIntegerParam(Snapshot.TargetPathIndex));
		properties.Add(TEXT("closest_path_distance_cm"), MakeFloatParam(Snapshot.ClosestPathDistanceCm));
		properties.Add(TEXT("max_path_error_cm"), MakeFloatParam(Snapshot.MaxPathErrorCm));
		properties.Add(TEXT("obstacle_warning_count"), MakeIntegerParam(Snapshot.ObstacleWarningCount));
		properties.Add(TEXT("last_obstacle_warning_source"), MakeStringParam(Snapshot.LastObstacleWarningSource));
		properties.Add(TEXT("blocked_corridor_cell_count"), MakeIntegerParam(Snapshot.BlockedCorridorCellCount));
		properties.Add(TEXT("dynamic_blocked_cell_count"), MakeIntegerParam(Snapshot.DynamicBlockedCellCount));

		if (Snapshot.bHasTargetWorldPoint)
		{
			properties.Add(TEXT("target_world_point_cm"), MakeVectorParam(Snapshot.TargetWorldPointCm));
		}
	}

	FString message = Snapshot.Message;
	if (message.IsEmpty())
	{
		message = Snapshot.ErrorMessage.IsEmpty()
			? Snapshot.Reason
			: Snapshot.ErrorMessage;
	}
	if (message.IsEmpty())
	{
		message = TEXT("DeliveryBot policy event.");
	}

	AddEvaluationEventWithDetails(
		Snapshot.EventType,
		Snapshot.Severity,
		message,
		FString(),
		DeliveryBotActor->GetActorLocation(),
		0.0,
		properties);

	if (Snapshot.bTerminalFailure)
	{
		FinishEpisode(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed);
	}
}

void UScenarioEvaluationSubsystem::AddEvaluationEvent(
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
		TMap<FString, FScenarioParamValue>());
}

void UScenarioEvaluationSubsystem::AddEvaluationEventWithDetails(
	EEpisodeEvaluationEventType eventType,
	EEpisodeEvaluationEventSeverity severity,
	const FString& message,
	const FString& targetInstanceId,
	const FVector& location,
	double value,
	const TMap<FString, FScenarioParamValue>& properties)
{
	FEpisodeEvaluationEvent event;
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

	PublishEvaluationEvent(event);

	UE_LOG(
		LogScenarioEvaluation,
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

void UScenarioEvaluationSubsystem::PublishEvaluationEvent(FEpisodeEvaluationEvent& event)
{
	event.EventIndex = CurrentResult.Events.Num();
	if (event.WorldTimeSeconds < 0.0)
	{
		event.WorldTimeSeconds = EvaluationStartTimeSeconds + event.ElapsedTimeSeconds;
	}

	CurrentResult.Events.Add(event);
	OnEvaluationEvent.Broadcast(event);
}

void UScenarioEvaluationSubsystem::BindEvaluationHitDelegates()
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

	for (const TObjectPtr<AActor>& corridorActor : ActiveRuntimeContext.CorridorActors)
	{
		BindActorHitDelegates(corridorActor.Get());
	}
}

void UScenarioEvaluationSubsystem::BindActorHitDelegates(AActor* actor)
{
	if (!IsValid(actor)) return;

	TArray<UPrimitiveComponent*> primitiveComponents;
	actor->GetComponents<UPrimitiveComponent>(primitiveComponents);
	for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
	{
		if (!IsValid(primitiveComponent) || IsHitComponentBound(primitiveComponent)) continue;

		primitiveComponent->OnComponentHit.RemoveDynamic(this, &UScenarioEvaluationSubsystem::HandleObservedComponentHit);
		primitiveComponent->OnComponentHit.AddDynamic(this, &UScenarioEvaluationSubsystem::HandleObservedComponentHit);
		primitiveComponent->SetNotifyRigidBodyCollision(true);
		BoundHitComponents.Add(primitiveComponent);
	}
}

void UScenarioEvaluationSubsystem::UnbindEvaluationHitDelegates()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& boundComponent : BoundHitComponents)
	{
		if (UPrimitiveComponent* primitiveComponent = boundComponent.Get())
		{
			primitiveComponent->OnComponentHit.RemoveDynamic(this, &UScenarioEvaluationSubsystem::HandleObservedComponentHit);
		}
	}

	BoundHitComponents.Reset();
}

bool UScenarioEvaluationSubsystem::IsHitComponentBound(const UPrimitiveComponent* primitiveComponent) const
{
	if (!primitiveComponent) return false;

	for (const TWeakObjectPtr<UPrimitiveComponent>& boundComponent : BoundHitComponents)
	{
		if (boundComponent.Get() == primitiveComponent) return true;
	}

	return false;
}

void UScenarioEvaluationSubsystem::HandleObservedComponentHit(
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

	if (AScenarioGroundRegion* groundRegion = Cast<AScenarioGroundRegion>(targetActor))
	{
		if (groundRegion->RegionSpec.RegionType == EScenarioGroundRegionType::Blocked)
		{
			RecordCollisionEvent(
				EEpisodeEvaluationEventType::BlockedRegionCollision,
				targetActor,
				eventLocation,
				ActiveEvaluationConfig.BlockedRegionCollisionEventValue,
				TEXT("로봇이 blocked region과 충돌함."));
		}
		return;
	}

	if (AScenarioCorridorRuntimeActor* corridorActor = Cast<AScenarioCorridorRuntimeActor>(targetActor))
	{
		FScenarioRuntimeCorridorSurfaceQueryResult surface;
		if (corridorActor->TryFindSurfaceAtWorldLocation2D(eventLocation, surface)
			&& surface.RegionType == EScenarioGroundRegionType::Blocked)
		{
			RecordBlockedCorridorSurfaceCollision(
				surface,
				corridorActor,
				eventLocation,
				TEXT("Robot collided with a blocked Corridor surface."));
		}
		return;
	}

	if (ContainsRuntimeActor(ActiveRuntimeContext.StaticObstacleActors, targetActor))
	{
		RecordCollisionEvent(
			EEpisodeEvaluationEventType::StaticObstacleCollision,
			targetActor,
			eventLocation,
			ActiveEvaluationConfig.StaticObstacleCollisionEventValue,
			TEXT("로봇이 정적 장애물과 충돌함."));
		return;
	}

	if (ContainsRuntimeActor(ActiveRuntimeContext.PedestrianActors, targetActor))
	{
		RecordCollisionEvent(
			EEpisodeEvaluationEventType::PedestrianCollision,
			targetActor,
			eventLocation,
			ActiveEvaluationConfig.PedestrianCollisionEventValue,
			TEXT("로봇이 보행자와 충돌함."));
	}
}

bool UScenarioEvaluationSubsystem::CheckGoalReached()
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
	SetFloatMetric(TEXT("distance_to_goal_m"), goalDistanceCm / 100.0);
	SetFloatMetric(TEXT("goal_threshold_m"), acceptanceRadiusCm / 100.0);

	FinishEpisode(
		true,
		HasWarningEvents() ? EEpisodeEvaluationOutcome::Warning : EEpisodeEvaluationOutcome::Success,
		EEpisodeEvaluationTerminalReason::GoalReached);
	return true;
}

bool UScenarioEvaluationSubsystem::CheckRobotTipOver()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor)) return false;

	const double tipOverAngleDegrees = FMath::Max(0.0, ActiveEvaluationConfig.TipOverAngleDegrees);
	if (tipOverAngleDegrees <= 0.0) return false;

	const FVector robotUp = ActiveRuntimeContext.RobotActor->GetActorUpVector().GetSafeNormal();
	const double upDot = FMath::Clamp(FVector::DotProduct(robotUp, FVector::UpVector), -1.0, 1.0);
	const double tiltAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(upDot));
	if (tiltAngleDegrees < tipOverAngleDegrees) return false;

	const FRotator robotRotation = ActiveRuntimeContext.RobotActor->GetActorRotation();
	const double rollDegrees = FMath::Abs(FRotator::NormalizeAxis(robotRotation.Roll));
	const double pitchDegrees = FMath::Abs(FRotator::NormalizeAxis(robotRotation.Pitch));
	++RobotTipOverCount;
	SetFloatMetric(TEXT("robot_tip_over_count"), RobotTipOverCount);
	SetFloatMetric(TEXT("robot_tip_over_angle_deg"), tiltAngleDegrees);
	SetFloatMetric(TEXT("roll_degree"), rollDegrees);
	SetFloatMetric(TEXT("pitch_degree"), pitchDegrees);
	SetFloatMetric(TEXT("threshold_degree"), tipOverAngleDegrees);

	TMap<FString, FScenarioParamValue> properties;
	properties.Add(TEXT("tilt_angle_deg"), MakeFloatParam(tiltAngleDegrees));
	properties.Add(TEXT("tip_over_angle_threshold_deg"), MakeFloatParam(tipOverAngleDegrees));
	properties.Add(TEXT("roll_degree"), MakeFloatParam(rollDegrees));
	properties.Add(TEXT("pitch_degree"), MakeFloatParam(pitchDegrees));
	properties.Add(TEXT("threshold_degree"), MakeFloatParam(tipOverAngleDegrees));
	AddRobotCorridorSnapshotProperties(properties);
	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::RobotTipOver,
		EEpisodeEvaluationEventSeverity::Failure,
		TEXT("로봇이 전복 각도 임계값을 초과함."),
		FString(),
		ActiveRuntimeContext.RobotActor->GetActorLocation(),
		tiltAngleDegrees,
		properties);

	FinishEpisode(false, EEpisodeEvaluationOutcome::Failure, EEpisodeEvaluationTerminalReason::RobotTipOver);
	return true;
}

void UScenarioEvaluationSubsystem::UpdateBlockedRegionViolations()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor)) return;

	const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> observedBlockedRegionIds;

	for (const TObjectPtr<AActor>& groundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		AScenarioGroundRegion* groundRegion = Cast<AScenarioGroundRegion>(groundRegionActor.Get());
		if (!IsValid(groundRegion)
			|| groundRegion->RegionSpec.RegionType != EScenarioGroundRegionType::Blocked)
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
			ActiveEvaluationConfig.BlockedRegionCollisionEventValue,
			TEXT("로봇이 blocked region에 진입함."));
	}

	for (const TObjectPtr<AActor>& corridorActorPtr : ActiveRuntimeContext.CorridorActors)
	{
		AScenarioCorridorRuntimeActor* corridorActor = Cast<AScenarioCorridorRuntimeActor>(corridorActorPtr.Get());
		if (!IsValid(corridorActor))
		{
			continue;
		}

		FScenarioRuntimeCorridorSurfaceQueryResult surface;
		if (!corridorActor->TryFindSurfaceAtWorldLocation2D(robotLocation, surface)
			|| surface.RegionType != EScenarioGroundRegionType::Blocked)
		{
			continue;
		}

		const FString regionId = surface.SurfaceInstanceId;
		observedBlockedRegionIds.Add(regionId);

		FBlockedRegionState& regionState = BlockedRegionStates.FindOrAdd(regionId);
		if (regionState.bInside)
		{
			continue;
		}

		regionState.bInside = true;
		RecordBlockedCorridorSurfaceCollision(
			surface,
			corridorActor,
			robotLocation,
			TEXT("Robot entered a blocked Corridor surface."));
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

void UScenarioEvaluationSubsystem::UpdatePenaltyRegionViolations()
{
	if (!IsValid(ActiveRuntimeContext.RobotActor)) return;

	const double elapsedTimeSeconds = GetElapsedTimeSeconds();
	const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
	TSet<FString> observedPenaltyRegionIds;

	for (const TObjectPtr<AActor>& groundRegionActor : ActiveRuntimeContext.GroundRegionActors)
	{
		AScenarioGroundRegion* groundRegion = Cast<AScenarioGroundRegion>(groundRegionActor.Get());
		if (!IsValid(groundRegion) || groundRegion->RegionSpec.RegionType != EScenarioGroundRegionType::Penalty) continue;

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

		const double eventValue = ActiveEvaluationConfig.PenaltyRegionViolationEventValue;
		const double durationSeconds = elapsedTimeSeconds - regionState.EnterTimeSeconds;
		TMap<FString, FScenarioParamValue> properties;
		properties.Add(TEXT("region_id"), MakeStringParam(regionId));
		properties.Add(TEXT("enter_time_s"), MakeFloatParam(regionState.EnterTimeSeconds));
		properties.Add(TEXT("start_time_s"), MakeFloatParam(regionState.EnterTimeSeconds));
		properties.Add(TEXT("end_time_s"), MakeFloatParam(elapsedTimeSeconds));
		properties.Add(TEXT("duration_s"), MakeFloatParam(durationSeconds));
		properties.Add(TEXT("violation_after_s"), MakeFloatParam(requiredDurationSeconds));
		AddRobotCorridorSnapshotProperties(properties);
		AddEvaluationEventWithDetails(
			EEpisodeEvaluationEventType::PenaltyRegionViolation,
			EEpisodeEvaluationEventSeverity::Warning,
			TEXT("로봇이 penalty region 조건을 위반함."),
			regionId,
			robotLocation,
			eventValue,
			properties);
		++PenaltyRegionViolationCount;
		SetFloatMetric(TEXT("penalty_region_violation_count"), PenaltyRegionViolationCount);
		regionState.bEventRecorded = true;
	}

	for (const TObjectPtr<AActor>& corridorActorPtr : ActiveRuntimeContext.CorridorActors)
	{
		AScenarioCorridorRuntimeActor* corridorActor = Cast<AScenarioCorridorRuntimeActor>(corridorActorPtr.Get());
		if (!IsValid(corridorActor))
		{
			continue;
		}

		FScenarioRuntimeCorridorSurfaceQueryResult surface;
		if (!corridorActor->TryFindSurfaceAtWorldLocation2D(robotLocation, surface)
			|| surface.RegionType != EScenarioGroundRegionType::Penalty)
		{
			continue;
		}

		const FString regionId = surface.SurfaceInstanceId;
		observedPenaltyRegionIds.Add(regionId);
		FPenaltyRegionState& regionState = PenaltyRegionStates.FindOrAdd(regionId);
		if (!regionState.bInside)
		{
			regionState.bInside = true;
			regionState.bEventRecorded = false;
			regionState.EnterTimeSeconds = elapsedTimeSeconds;
		}

		constexpr double RequiredDurationSeconds = 0.0;
		const double durationSeconds = elapsedTimeSeconds - regionState.EnterTimeSeconds;
		if (regionState.bEventRecorded || durationSeconds < RequiredDurationSeconds)
		{
			continue;
		}

		AddPenaltyCorridorSurfaceViolation(
			surface,
			robotLocation,
			regionState.EnterTimeSeconds,
			durationSeconds,
			RequiredDurationSeconds);
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

bool UScenarioEvaluationSubsystem::TryFindCorridorSurfaceAtWorldLocation(
	const FVector& location,
	FScenarioRuntimeCorridorSurfaceQueryResult& outSurface) const
{
	outSurface = FScenarioRuntimeCorridorSurfaceQueryResult{};
	for (const TObjectPtr<AActor>& corridorActorPtr : ActiveRuntimeContext.CorridorActors)
	{
		const AScenarioCorridorRuntimeActor* corridorActor = Cast<AScenarioCorridorRuntimeActor>(corridorActorPtr.Get());
		if (IsValid(corridorActor) && corridorActor->TryFindSurfaceAtWorldLocation2D(location, outSurface))
		{
			return true;
		}
	}

	return false;
}

void UScenarioEvaluationSubsystem::AddCorridorSnapshotProperties(
	TMap<FString, FScenarioParamValue>& properties,
	const FScenarioRuntimeCorridorSurfaceQueryResult& surface,
	const FString& prefix) const
{
	if (prefix.IsEmpty())
	{
		properties.Add(TEXT("along_m"), MakeFloatParam(surface.AlongMeters));
		properties.Add(TEXT("offset_m"), MakeFloatParam(surface.OffsetMeters));
		return;
	}

	properties.Add(FString::Printf(TEXT("%s_along_m"), *prefix), MakeFloatParam(surface.AlongMeters));
	properties.Add(FString::Printf(TEXT("%s_offset_m"), *prefix), MakeFloatParam(surface.OffsetMeters));
}

void UScenarioEvaluationSubsystem::AddRobotCorridorSnapshotProperties(
	TMap<FString, FScenarioParamValue>& properties) const
{
	if (!IsValid(ActiveRuntimeContext.RobotActor))
	{
		return;
	}

	FScenarioRuntimeCorridorSurfaceQueryResult robotSurface;
	if (TryFindCorridorSurfaceAtWorldLocation(ActiveRuntimeContext.RobotActor->GetActorLocation(), robotSurface))
	{
		AddCorridorSnapshotProperties(properties, robotSurface, TEXT("robot"));
	}
}

void UScenarioEvaluationSubsystem::AddTargetCorridorSnapshotProperties(
	TMap<FString, FScenarioParamValue>& properties,
	const FVector& targetLocation) const
{
	FScenarioRuntimeCorridorSurfaceQueryResult targetSurface;
	if (TryFindCorridorSurfaceAtWorldLocation(targetLocation, targetSurface))
	{
		AddCorridorSnapshotProperties(properties, targetSurface, TEXT("target"));
	}
}

void UScenarioEvaluationSubsystem::RecordBlockedCorridorSurfaceCollision(
	const FScenarioRuntimeCorridorSurfaceQueryResult& surface,
	AActor* targetActor,
	const FVector& location,
	const FString& message)
{
	if (!bEvaluating || surface.SurfaceInstanceId.IsEmpty())
	{
		return;
	}

	const FString collisionEventKey = FString::Printf(
		TEXT("%s:%s"),
		*ToEvaluationEnumString(EEpisodeEvaluationEventType::BlockedRegionCollision),
		*surface.SurfaceInstanceId);
	const double elapsedTimeSeconds = GetElapsedTimeSeconds();
	if (const double* lastRecordedTime = LastCollisionEventTimes.Find(collisionEventKey))
	{
		if (elapsedTimeSeconds - *lastRecordedTime < CollisionEventCooldownSeconds)
		{
			return;
		}
	}

	LastCollisionEventTimes.Add(collisionEventKey, elapsedTimeSeconds);

	TMap<FString, FScenarioParamValue> properties;
	properties.Add(TEXT("surface_instance_id"), MakeStringParam(surface.SurfaceInstanceId));
	properties.Add(TEXT("region_id"), MakeStringParam(surface.SurfaceInstanceId));
	properties.Add(TEXT("corridor_id"), MakeStringParam(surface.CorridorId));
	properties.Add(TEXT("segment_id"), MakeStringParam(surface.SegmentId));
	properties.Add(TEXT("lane_id"), MakeStringParam(surface.LaneId));
	properties.Add(TEXT("surface_id"), MakeStringParam(surface.SurfaceId));
	properties.Add(TEXT("along_m"), MakeFloatParam(surface.AlongMeters));
	properties.Add(TEXT("offset_m"), MakeFloatParam(surface.OffsetMeters));
	AddCorridorSnapshotProperties(properties, surface, TEXT("robot"));
	if (targetActor)
	{
		properties.Add(TEXT("target_actor"), MakeStringParam(targetActor->GetName()));
	}

	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::BlockedRegionCollision,
		EEpisodeEvaluationEventSeverity::Warning,
		message,
		surface.SurfaceInstanceId,
		location,
		ActiveEvaluationConfig.BlockedRegionCollisionEventValue,
		properties);
	++BlockedRegionCollisionCount;
	SetFloatMetric(TEXT("blocked_region_collision_count"), BlockedRegionCollisionCount);
}

void UScenarioEvaluationSubsystem::AddPenaltyCorridorSurfaceViolation(
	const FScenarioRuntimeCorridorSurfaceQueryResult& surface,
	const FVector& location,
	double enterTimeSeconds,
	double durationSeconds,
	double requiredDurationSeconds)
{
	if (!bEvaluating || surface.SurfaceInstanceId.IsEmpty())
	{
		return;
	}

	TMap<FString, FScenarioParamValue> properties;
	properties.Add(TEXT("surface_instance_id"), MakeStringParam(surface.SurfaceInstanceId));
	properties.Add(TEXT("region_id"), MakeStringParam(surface.SurfaceInstanceId));
	properties.Add(TEXT("corridor_id"), MakeStringParam(surface.CorridorId));
	properties.Add(TEXT("segment_id"), MakeStringParam(surface.SegmentId));
	properties.Add(TEXT("lane_id"), MakeStringParam(surface.LaneId));
	properties.Add(TEXT("surface_id"), MakeStringParam(surface.SurfaceId));
	properties.Add(TEXT("along_m"), MakeFloatParam(surface.AlongMeters));
	properties.Add(TEXT("offset_m"), MakeFloatParam(surface.OffsetMeters));
	properties.Add(TEXT("enter_time_s"), MakeFloatParam(enterTimeSeconds));
	properties.Add(TEXT("start_time_s"), MakeFloatParam(enterTimeSeconds));
	properties.Add(TEXT("end_time_s"), MakeFloatParam(enterTimeSeconds + durationSeconds));
	properties.Add(TEXT("duration_s"), MakeFloatParam(durationSeconds));
	properties.Add(TEXT("violation_after_s"), MakeFloatParam(requiredDurationSeconds));
	AddCorridorSnapshotProperties(properties, surface, TEXT("robot"));
	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::PenaltyRegionViolation,
		EEpisodeEvaluationEventSeverity::Warning,
		TEXT("Robot violated a penalty Corridor surface condition."),
		surface.SurfaceInstanceId,
		location,
		ActiveEvaluationConfig.PenaltyRegionViolationEventValue,
		properties);
	++PenaltyRegionViolationCount;
	SetFloatMetric(TEXT("penalty_region_violation_count"), PenaltyRegionViolationCount);
}

void UScenarioEvaluationSubsystem::UpdateNearMisses()
{
	const FScenarioEvaluationConfig& evaluationConfig = ActiveEvaluationConfig;
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

void UScenarioEvaluationSubsystem::FlushActiveNearMisses()
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

void UScenarioEvaluationSubsystem::CloseNearMissInterval(
	const FString& pedestrianInstanceId,
	const FNearMissIntervalState& state,
	double endTimeSeconds)
{
	if (state.MinDistanceCm == TNumericLimits<double>::Max()) return;

	const double durationSeconds = FMath::Max(0.0, endTimeSeconds - state.StartTimeSeconds);
	const double eventValue = ActiveEvaluationConfig.PedestrianNearMissEventValue;

	FEpisodeEvaluationEvent event;
	event.ElapsedTimeSeconds = endTimeSeconds;
	event.WorldTimeSeconds = EvaluationStartTimeSeconds + endTimeSeconds;
	event.EventType = EEpisodeEvaluationEventType::PedestrianNearMiss;
	event.Severity = EEpisodeEvaluationEventSeverity::Warning;
	event.SubjectInstanceId = ActiveRuntimeContext.RobotInstanceId;
	event.TargetInstanceId = pedestrianInstanceId;
	event.Location = state.ClosestRobotLocation;
	event.Value = eventValue;
	event.Message = TEXT("보행자 near-miss 구간.");
	event.Properties.Add(TEXT("start_time_s"), MakeFloatParam(state.StartTimeSeconds));
	event.Properties.Add(TEXT("end_time_s"), MakeFloatParam(endTimeSeconds));
	event.Properties.Add(TEXT("duration_s"), MakeFloatParam(durationSeconds));
	event.Properties.Add(TEXT("min_distance_m"), MakeFloatParam(state.MinDistanceCm / 100.0));
	event.Properties.Add(TEXT("pedestrian_id"), MakeStringParam(pedestrianInstanceId));
	event.Properties.Add(TEXT("target_id"), MakeStringParam(pedestrianInstanceId));
	event.Properties.Add(TEXT("threshold_m"), MakeFloatParam(ActiveEvaluationConfig.NearMissDistanceCm / 100.0));
	FScenarioRuntimeCorridorSurfaceQueryResult robotSurface;
	if (TryFindCorridorSurfaceAtWorldLocation(state.ClosestRobotLocation, robotSurface))
	{
		AddCorridorSnapshotProperties(event.Properties, robotSurface, TEXT("robot"));
	}
	AddTargetCorridorSnapshotProperties(event.Properties, state.ClosestPedestrianLocation);
	PublishEvaluationEvent(event);

	UE_LOG(
		LogScenarioEvaluation,
		Log,
		TEXT("평가 이벤트 기록 | Episode: %s, Index: %d, Type: %s, Severity: %s, Subject: %s, Target: %s, Duration: %.2fs, MinDistance: %.2fm, Value: %.2f"),
		*CurrentResult.EpisodeId,
		event.EventIndex,
		*ToEvaluationEnumString(event.EventType),
		*ToEvaluationEnumString(event.Severity),
		*event.SubjectInstanceId,
		*event.TargetInstanceId,
		durationSeconds,
		state.MinDistanceCm / 100.0,
		eventValue);

	++NearMissCount;
	NearMissTotalDurationSeconds += durationSeconds;
	NearMissMinDistanceCm = FMath::Min(NearMissMinDistanceCm, state.MinDistanceCm);

	SetFloatMetric(TEXT("near_miss_count"), NearMissCount);
	SetFloatMetric(TEXT("near_miss_total_duration_s"), NearMissTotalDurationSeconds);
	SetFloatMetric(TEXT("near_miss_min_distance_m"), NearMissMinDistanceCm / 100.0);
}

void UScenarioEvaluationSubsystem::SetFloatMetric(const FString& key, double value)
{
	CurrentResult.Metrics.Add(key, MakeFloatParam(value));
}

void UScenarioEvaluationSubsystem::FinishEpisode(
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

void UScenarioEvaluationSubsystem::RecordCollisionEvent(
	EEpisodeEvaluationEventType eventType,
	AActor* targetActor,
	const FVector& location,
	double eventValue,
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

	TMap<FString, FScenarioParamValue> properties;
	properties.Add(TEXT("target_id"), MakeStringParam(targetInstanceId));
	if (targetActor)
	{
		properties.Add(TEXT("target_actor"), MakeStringParam(targetActor->GetName()));
		AddTargetCorridorSnapshotProperties(properties, targetActor->GetActorLocation());
	}
	if (const AScenarioGroundRegion* groundRegion = Cast<AScenarioGroundRegion>(targetActor))
	{
		properties.Add(TEXT("region_id"), MakeStringParam(GetActorInstanceId(groundRegion)));
	}
	AddRobotCorridorSnapshotProperties(properties);

	AddEvaluationEventWithDetails(
		eventType,
		EEpisodeEvaluationEventSeverity::Warning,
		message,
		targetInstanceId,
		location,
		eventValue,
		properties);

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

bool UScenarioEvaluationSubsystem::HasWarningEvents() const
{
	for (const FEpisodeEvaluationEvent& event : CurrentResult.Events)
	{
		if (event.Severity == EEpisodeEvaluationEventSeverity::Warning) return true;
	}

	return false;
}

bool UScenarioEvaluationSubsystem::IsRobotActor(const AActor* actor) const
{
	return IsValid(actor) && actor == ActiveRuntimeContext.RobotActor.Get();
}

bool UScenarioEvaluationSubsystem::ContainsRuntimeActor(const TArray<TObjectPtr<AActor>>& actors, const AActor* actor) const
{
	if (!IsValid(actor)) return false;

	for (const TObjectPtr<AActor>& runtimeActor : actors)
	{
		if (runtimeActor.Get() == actor) return true;
	}

	return false;
}

FString UScenarioEvaluationSubsystem::GetActorInstanceId(const AActor* actor) const
{
	if (!actor) return FString();

	if (IsRobotActor(actor)) return ActiveRuntimeContext.RobotInstanceId;

	if (const AScenarioGroundRegion* groundRegion = Cast<AScenarioGroundRegion>(actor))
	{
		if (!groundRegion->RegionSpec.RegionId.IsEmpty()) return groundRegion->RegionSpec.RegionId;
	}

	if (const AScenarioCorridorRuntimeActor* corridorActor = Cast<AScenarioCorridorRuntimeActor>(actor))
	{
		const FScenarioRuntimeCorridorSpec corridorSpec = corridorActor->GetCorridorSpec();
		if (!corridorSpec.CorridorId.IsEmpty()) return corridorSpec.CorridorId;
	}

	if (const UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		if (!placeableComponent->InstanceId.IsEmpty()) return placeableComponent->InstanceId;
	}

	return actor->GetName();
}

double UScenarioEvaluationSubsystem::GetElapsedTimeSeconds() const
{
	const UWorld* world = GetWorld();
	if (!world) return 0.0;

	return FMath::Max(0.0, world->GetTimeSeconds() - EvaluationStartTimeSeconds);
}

void UScenarioEvaluationSubsystem::EndForTimeout()
{
	FlushActiveNearMisses();
	const double elapsedTimeSeconds = GetElapsedTimeSeconds();

	UE_LOG(
		LogScenarioEvaluation,
		Log,
		TEXT("평가 제한 시간 도달 | Episode: %s, Elapsed: %.2fs, Limit: %.2fs"),
		*ActiveRuntimeContext.EpisodeId,
		elapsedTimeSeconds,
		TimeLimitSeconds);

	TMap<FString, FScenarioParamValue> properties;
	properties.Add(TEXT("start_time_s"), MakeFloatParam(0.0));
	properties.Add(TEXT("end_time_s"), MakeFloatParam(elapsedTimeSeconds));
	properties.Add(TEXT("duration_s"), MakeFloatParam(elapsedTimeSeconds));
	properties.Add(TEXT("max_duration_s"), MakeFloatParam(TimeLimitSeconds));
	if (ActiveRuntimeContext.bHasGoalLocation && IsValid(ActiveRuntimeContext.RobotActor))
	{
		const double distanceToGoalMeters = FVector::Dist2D(
			ActiveRuntimeContext.RobotActor->GetActorLocation(),
			ActiveRuntimeContext.GoalLocation) / 100.0;
		properties.Add(TEXT("distance_to_goal_m"), MakeFloatParam(distanceToGoalMeters));
		SetFloatMetric(TEXT("distance_to_goal_m"), distanceToGoalMeters);
	}
	SetFloatMetric(TEXT("duration_s"), elapsedTimeSeconds);
	SetFloatMetric(TEXT("max_duration_s"), TimeLimitSeconds);
	AddRobotCorridorSnapshotProperties(properties);

	AddEvaluationEventWithDetails(
		EEpisodeEvaluationEventType::Timeout,
		EEpisodeEvaluationEventSeverity::Failure,
		TEXT("제한 시간을 초과함."),
		FString(),
		IsValid(ActiveRuntimeContext.RobotActor)
			? ActiveRuntimeContext.RobotActor->GetActorLocation()
			: FVector::ZeroVector,
		TimeLimitSeconds,
		properties);

	FinishEpisode(false, EEpisodeEvaluationOutcome::Failure, EEpisodeEvaluationTerminalReason::Timeout);
}
