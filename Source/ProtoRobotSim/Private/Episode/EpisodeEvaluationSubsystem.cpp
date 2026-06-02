
#include "Episode/EpisodeEvaluationSubsystem.h"

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
	SetFloatMetric(TEXT("score"), CurrentScore);
	SetFloatMetric(TEXT("near_miss_count"), 0.0);
	SetFloatMetric(TEXT("near_miss_total_duration_s"), 0.0);
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
	ActiveEvaluationConfig = FEpisodeEvaluationConfig{};
	ActiveRuntimeContext = FEpisodeRuntimeContext{};
	TimeLimitSeconds = 0.0;
	EvaluationStartTimeSeconds = 0.0;
	CurrentScore = 0.0;
	NearMissCount = 0;
	NearMissTotalDurationSeconds = 0.0;
	NearMissMinDistanceCm = TNumericLimits<double>::Max();
	ActiveNearMisses.Reset();
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
	FEpisodeEvaluationEvent Event;
	Event.EventIndex = CurrentResult.Events.Num();
	Event.ElapsedTimeSeconds = GetElapsedTimeSeconds();
	Event.EventType = EventType;
	Event.Severity = Severity;
	Event.Message = Message;
	if (IsValid(ActiveRuntimeContext.RobotActor))
	{
		Event.SubjectInstanceId = ActiveRuntimeContext.RobotInstanceId;
		Event.Location = ActiveRuntimeContext.RobotActor->GetActorLocation();
	}

	CurrentResult.Events.Add(Event);

	UE_LOG(
		LogEpisodeEvaluation,
		Log,
		TEXT("Evaluation event | Episode: %s, Index: %d, Type: %s, Severity: %s, Subject: %s, Time: %.2fs, Message: %s"),
		*CurrentResult.EpisodeId,
		Event.EventIndex,
		*ToEvaluationEnumString(Event.EventType),
		*ToEvaluationEnumString(Event.Severity),
		*Event.SubjectInstanceId,
		Event.ElapsedTimeSeconds,
		*Event.Message);
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
		TEXT("시간 초과."));

	FEpisodeEvaluationResult Result = CurrentResult;
	Result.EpisodeId = ActiveRuntimeContext.EpisodeId;
	Result.bSuccess = false;
	Result.Outcome = EEpisodeEvaluationOutcome::Failure;
	Result.TerminalReason = EEpisodeEvaluationTerminalReason::Timeout;
	Result.DurationSeconds = GetElapsedTimeSeconds();
	RequestEndEpisode(Result);
}
