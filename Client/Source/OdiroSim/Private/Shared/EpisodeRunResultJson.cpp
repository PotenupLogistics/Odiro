#include "Shared/EpisodeRunResultJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Shared/ScenarioSampleJson.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr double RunResultCmToM = 0.01;
	const TCHAR* RunResultUnsetHash = TEXT("hash:unset");

	struct FRunResultSampleInfo
	{
		// Experiment-local scenario sample id.
		FString SampleId;

		// Scenario display id from the sample or runtime record.
		FString ScenarioId;

		// Source scenario template id inferred from sample lineage.
		FString TemplateId;

		// Source scenario template hash.
		FString TemplateHash;

		// Experiment profile hash.
		FString ProfileHash;

		// Experiment setting hash.
		FString SettingHash;

		// Concrete sample seed.
		int64 Seed = 0;

		// Scenario params copied from the frozen sample.
		TMap<FString, FScenarioSampleParamValue> ScenarioParams;

		// Compact semantic summary copied from the frozen sample.
		FScenarioSampleSummary SemanticSummary;
	};

	struct FRunResultEventLine
	{
		// Episode-local event index.
		int32 EventIndex = 0;

		// Episode runtime timestamp in seconds.
		double RunTimeSeconds = 0.0;

		// System that produced this event.
		FString Source = TEXT("EvaluationSubsystem");

		// Canonical event type string.
		FString EventType;

		// Stable machine-readable reason code.
		FString Reason;

		// Short human-readable event message.
		FString Message;

		// actions.jsonl sequence for this event; unset until policy logging is connected.
		TOptional<int32> ActionSequence;

		// Event-specific properties.
		TSharedPtr<FJsonObject> Properties;
	};

	template <typename TEnum>
	FString RunResultEnumString(TEnum Value)
	{
		if (const UEnum* EnumValue = StaticEnum<TEnum>())
		{
			return EnumValue->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}

	FString RunResultNormalizePath(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Path;
	}

	FString RunResultMakeTemplateId(const FString& TemplateRef, const FString& ScenarioId, const FString& SampleId)
	{
		if (!TemplateRef.TrimStartAndEnd().IsEmpty())
		{
			FString BaseName = FPaths::GetBaseFilename(TemplateRef);
			if (BaseName.EndsWith(TEXT(".template")))
			{
				BaseName.LeftChopInline(9);
			}
			return BaseName;
		}

		const FString Suffix = FString::Printf(TEXT("_%s"), *SampleId);
		if (!SampleId.IsEmpty() && ScenarioId.EndsWith(Suffix))
		{
			return ScenarioId.LeftChop(Suffix.Len());
		}

		return ScenarioId;
	}

	FString RunResultMakeExperimentId(const FEpisodeRunRecord& Record)
	{
		const FString NormalizedPath = RunResultNormalizePath(Record.ScenarioSourceJsonPath);
		const FString Marker = TEXT("Json/Experiments/");
		int32 MarkerIndex = INDEX_NONE;
		if (!NormalizedPath.FindChar(TEXT('/'), MarkerIndex))
		{
			return FString();
		}

		const int32 ExperimentRootIndex = NormalizedPath.Find(Marker, ESearchCase::IgnoreCase);
		if (ExperimentRootIndex == INDEX_NONE)
		{
			return FString();
		}

		const int32 ExperimentNameStart = ExperimentRootIndex + Marker.Len();
		const int32 ScenariosIndex = NormalizedPath.Find(TEXT("/scenarios/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ExperimentNameStart);
		if (ScenariosIndex == INDEX_NONE || ScenariosIndex <= ExperimentNameStart)
		{
			return FString();
		}

		return NormalizedPath.Mid(ExperimentNameStart, ScenariosIndex - ExperimentNameStart);
	}

	TArray<TSharedPtr<FJsonValue>> RunResultMakeXyArrayM(double X, double Y)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(2);
		Array.Add(MakeShared<FJsonValueNumber>(X));
		Array.Add(MakeShared<FJsonValueNumber>(Y));
		return Array;
	}

	TArray<TSharedPtr<FJsonValue>> RunResultMakeXyArrayFromCm(const FVector& LocationCm)
	{
		return RunResultMakeXyArrayM(LocationCm.X * RunResultCmToM, LocationCm.Y * RunResultCmToM);
	}

	TArray<TSharedPtr<FJsonValue>> RunResultMakeVectorArray(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(3);
		Array.Add(MakeShared<FJsonValueNumber>(Value.X));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return Array;
	}

	TSharedPtr<FJsonValue> RunResultMakeParamJsonValue(const FScenarioParamValue& ParamValue)
	{
		switch (ParamValue.Type)
		{
		case EScenarioParamValueType::Bool:
			return MakeShared<FJsonValueBoolean>(ParamValue.BoolValue);
		case EScenarioParamValueType::Integer:
			return MakeShared<FJsonValueNumber>(ParamValue.IntegerValue);
		case EScenarioParamValueType::Float:
			return MakeShared<FJsonValueNumber>(ParamValue.FloatValue);
		case EScenarioParamValueType::String:
			return MakeShared<FJsonValueString>(ParamValue.StringValue);
		case EScenarioParamValueType::Vector:
			return MakeShared<FJsonValueArray>(RunResultMakeVectorArray(ParamValue.VectorValue));
		case EScenarioParamValueType::None:
		default:
			return MakeShared<FJsonValueNull>();
		}
	}

	TSharedPtr<FJsonValue> RunResultMakeSampleParamJsonValue(const FScenarioSampleParamValue& ParamValue)
	{
		switch (ParamValue.Type)
		{
		case EScenarioSampleParamValueType::Boolean:
			return MakeShared<FJsonValueBoolean>(ParamValue.BoolValue);
		case EScenarioSampleParamValueType::Integer:
			return MakeShared<FJsonValueNumber>(ParamValue.IntegerValue);
		case EScenarioSampleParamValueType::Float:
			return MakeShared<FJsonValueNumber>(ParamValue.FloatValue);
		case EScenarioSampleParamValueType::String:
			return MakeShared<FJsonValueString>(ParamValue.StringValue);
		case EScenarioSampleParamValueType::FloatArray:
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			Values.Reserve(ParamValue.FloatArrayValue.Num());
			for (const double Value : ParamValue.FloatArrayValue)
			{
				Values.Add(MakeShared<FJsonValueNumber>(Value));
			}
			return MakeShared<FJsonValueArray>(Values);
		}
		case EScenarioSampleParamValueType::StringArray:
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			Values.Reserve(ParamValue.StringArrayValue.Num());
			for (const FString& Value : ParamValue.StringArrayValue)
			{
				Values.Add(MakeShared<FJsonValueString>(Value));
			}
			return MakeShared<FJsonValueArray>(Values);
		}
		case EScenarioSampleParamValueType::None:
		default:
			return MakeShared<FJsonValueNull>();
		}
	}

	bool RunResultTryGetStringParam(
		const TMap<FString, FScenarioParamValue>& Params,
		const FString& Key,
		FString& OutValue)
	{
		const FScenarioParamValue* ParamValue = Params.Find(Key);
		if (!ParamValue || ParamValue->Type != EScenarioParamValueType::String)
		{
			return false;
		}

		OutValue = ParamValue->StringValue;
		return true;
	}

	bool RunResultTryGetNumberParam(
		const TMap<FString, FScenarioParamValue>& Params,
		const FString& Key,
		double& OutValue)
	{
		const FScenarioParamValue* ParamValue = Params.Find(Key);
		if (!ParamValue)
		{
			return false;
		}

		if (ParamValue->Type == EScenarioParamValueType::Float)
		{
			OutValue = ParamValue->FloatValue;
			return true;
		}

		if (ParamValue->Type == EScenarioParamValueType::Integer)
		{
			OutValue = ParamValue->IntegerValue;
			return true;
		}

		return false;
	}

	bool RunResultIsPolicyKey(const FString& Key)
	{
		return Key.StartsWith(TEXT("policy_"));
	}

	bool RunResultIsTunableDeliveryBotFailureType(const FString& FailureType)
	{
		return FailureType == TEXT("RobotTipOver")
			|| FailureType == TEXT("PathFindingFailed")
			|| FailureType == TEXT("Stuck");
	}

	FString RunResultFindDeliveryBotFailureType(const FEpisodeEvaluationResult& Result)
	{
		FString FailureType;
		if (RunResultTryGetStringParam(Result.Metrics, TEXT("delivery_bot_failure_type"), FailureType))
		{
			return FailureType;
		}

		for (const FEpisodeEvaluationEvent& Event : Result.Events)
		{
			if (Event.EventType != EEpisodeEvaluationEventType::DeliveryBotSimulationFailure)
			{
				continue;
			}

			if (RunResultTryGetStringParam(Event.Properties, TEXT("delivery_bot_failure_type"), FailureType)
				|| RunResultTryGetStringParam(Event.Properties, TEXT("failure_type"), FailureType))
			{
				return FailureType;
			}
		}

		return FString();
	}

	bool RunResultIsUsableForLlmTuning(const FEpisodeRunRecord& Record)
	{
		if (!Record.bCompileSucceeded
			|| !Record.bScenarioSourceCompileSucceeded
			|| !Record.bSimulationProfileCompileSucceeded
			|| !Record.bSetupSucceeded
			|| !Record.bEvaluationCompleted)
		{
			return false;
		}

		switch (Record.TerminalReason)
		{
		case EEpisodeEvaluationTerminalReason::GoalReached:
		case EEpisodeEvaluationTerminalReason::Timeout:
		case EEpisodeEvaluationTerminalReason::RobotTipOver:
			return true;
		case EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed:
			return RunResultIsTunableDeliveryBotFailureType(RunResultFindDeliveryBotFailureType(Record.EvaluationResult));
		case EEpisodeEvaluationTerminalReason::CompilerCreateFailed:
		case EEpisodeEvaluationTerminalReason::CompileFailed:
		case EEpisodeEvaluationTerminalReason::SetupFailed:
		case EEpisodeEvaluationTerminalReason::EvaluationStartFailed:
		case EEpisodeEvaluationTerminalReason::Cancelled:
			return false;
		case EEpisodeEvaluationTerminalReason::None:
		default:
			break;
		}

		return Record.Outcome == EEpisodeEvaluationOutcome::Success
			|| Record.Outcome == EEpisodeEvaluationOutcome::Warning;
	}

	double RunResultGetDurationSeconds(const FEpisodeRunRecord& Record)
	{
		if (Record.DurationSeconds > 0.0)
		{
			return Record.DurationSeconds;
		}

		return Record.EvaluationResult.DurationSeconds;
	}

	FRunResultSampleInfo RunResultLoadSampleInfo(const FEpisodeRunRecord& Record)
	{
		FRunResultSampleInfo Info;
		Info.SampleId = Record.PairId;
		Info.ScenarioId = Record.EpisodeId;
		Info.TemplateId = Record.PairId;
		Info.TemplateHash = Record.ScenarioSourceHash.IsEmpty() ? FString(RunResultUnsetHash) : Record.ScenarioSourceHash;
		Info.ProfileHash = Record.SimulationProfileHash.IsEmpty() ? FString(RunResultUnsetHash) : Record.SimulationProfileHash;
		Info.SettingHash = RunResultUnsetHash;

		const FScenarioSampleParseResult SampleResult = FScenarioSampleJson::ParseFromFile(Record.ScenarioSourceJsonPath);
		if (!SampleResult.bSuccess)
		{
			return Info;
		}

		const FScenarioSampleIdentity& Sample = SampleResult.Document.Sample;
		Info.SampleId = Sample.SampleId;
		Info.ScenarioId = Sample.ScenarioId;
		Info.TemplateId = RunResultMakeTemplateId(Sample.Source.TemplateRef, Sample.ScenarioId, Sample.SampleId);
		Info.TemplateHash = Sample.Source.TemplateHash;
		Info.ProfileHash = Sample.Source.ProfileHash;
		Info.SettingHash = Sample.Source.SettingHash;
		Info.Seed = Sample.Source.Seed;
		Info.ScenarioParams = SampleResult.Document.Scenario.Params;
		Info.SemanticSummary = SampleResult.Document.Scenario.Semantic.Summary;
		return Info;
	}

	EEpisodeEvaluationEventType RunResultTerminalEventType(EEpisodeEvaluationTerminalReason TerminalReason)
	{
		switch (TerminalReason)
		{
		case EEpisodeEvaluationTerminalReason::Timeout:
			return EEpisodeEvaluationEventType::Timeout;
		case EEpisodeEvaluationTerminalReason::RobotTipOver:
			return EEpisodeEvaluationEventType::RobotTipOver;
		case EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed:
			return EEpisodeEvaluationEventType::DeliveryBotSimulationFailure;
		default:
			return EEpisodeEvaluationEventType::None;
		}
	}

	FString RunResultTerminalEventTypeString(EEpisodeEvaluationTerminalReason TerminalReason)
	{
		if (TerminalReason == EEpisodeEvaluationTerminalReason::GoalReached)
		{
			return TEXT("GoalReached");
		}

		const EEpisodeEvaluationEventType EventType = RunResultTerminalEventType(TerminalReason);
		return EventType == EEpisodeEvaluationEventType::None
			? FString()
			: RunResultEnumString(EventType);
	}

	bool RunResultHasTerminalEvent(const FEpisodeRunRecord& Record, int32& OutEventIndex)
	{
		OutEventIndex = INDEX_NONE;
		const EEpisodeEvaluationEventType TerminalType = RunResultTerminalEventType(Record.TerminalReason);
		if (TerminalType == EEpisodeEvaluationEventType::None)
		{
			return false;
		}

		for (const FEpisodeEvaluationEvent& Event : Record.EvaluationResult.Events)
		{
			if (Event.EventType == TerminalType)
			{
				OutEventIndex = Event.EventIndex;
				return true;
			}
		}

		return false;
	}

	FString RunResultReasonForEventType(const FString& EventType)
	{
		if (EventType == TEXT("Timeout"))
		{
			return TEXT("time_limit_reached");
		}
		if (EventType == TEXT("RobotTipOver"))
		{
			return TEXT("tip_over_threshold_exceeded");
		}
		if (EventType == TEXT("StaticObstacleCollision"))
		{
			return TEXT("static_obstacle_collision");
		}
		if (EventType == TEXT("BlockedRegionCollision"))
		{
			return TEXT("blocked_region_collision");
		}
		if (EventType == TEXT("PenaltyRegionViolation"))
		{
			return TEXT("penalty_region_violation");
		}
		if (EventType == TEXT("PedestrianNearMiss"))
		{
			return TEXT("distance_below_threshold");
		}
		if (EventType == TEXT("PedestrianCollision"))
		{
			return TEXT("pedestrian_collision");
		}
		if (EventType == TEXT("DeliveryBotSimulationFailure"))
		{
			return TEXT("delivery_bot_simulation_failure");
		}
		if (EventType == TEXT("GoalReached"))
		{
			return TEXT("goal_reached");
		}

		return TEXT("evaluation_event");
	}

	TSharedRef<FJsonObject> RunResultMakeEventPropertiesObject(const FEpisodeEvaluationEvent& Event)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		TArray<FString> Keys;
		Event.Properties.GetKeys(Keys);
		Keys.Sort();

		for (const FString& Key : Keys)
		{
			const FScenarioParamValue* ParamValue = Event.Properties.Find(Key);
			if (!ParamValue || RunResultIsPolicyKey(Key))
			{
				continue;
			}

			if (Key == TEXT("delivery_bot_failure_location_cm")
				&& ParamValue->Type == EScenarioParamValueType::Vector)
			{
				Object->SetArrayField(TEXT("delivery_bot_failure_xy_m"), RunResultMakeXyArrayFromCm(ParamValue->VectorValue));
				continue;
			}
			if (Key == TEXT("delivery_bot_failure_time_seconds"))
			{
				double TimeSeconds = 0.0;
				if (RunResultTryGetNumberParam(Event.Properties, Key, TimeSeconds))
				{
					Object->SetNumberField(TEXT("delivery_bot_failure_time_s"), TimeSeconds);
				}
				continue;
			}
			if (Key == TEXT("delivery_bot_failure_target_actor_name"))
			{
				FString TargetActorName;
				if (RunResultTryGetStringParam(Event.Properties, Key, TargetActorName))
				{
					Object->SetStringField(TEXT("target_actor"), TargetActorName);
				}
				continue;
			}
			if (Key == TEXT("pedestrian_id"))
			{
				FString PedestrianId;
				if (RunResultTryGetStringParam(Event.Properties, Key, PedestrianId))
				{
					Object->SetStringField(TEXT("target_id"), PedestrianId);
				}
				continue;
			}
			if (Key == TEXT("tilt_angle_deg"))
			{
				double AngleDegrees = 0.0;
				if (RunResultTryGetNumberParam(Event.Properties, Key, AngleDegrees))
				{
					Object->SetNumberField(TEXT("roll_degree"), AngleDegrees);
				}
				continue;
			}
			if (Key == TEXT("tip_over_angle_threshold_deg"))
			{
				double ThresholdDegrees = 0.0;
				if (RunResultTryGetNumberParam(Event.Properties, Key, ThresholdDegrees))
				{
					Object->SetNumberField(TEXT("threshold_degree"), ThresholdDegrees);
				}
				continue;
			}

			Object->SetField(Key, RunResultMakeParamJsonValue(*ParamValue));
		}

		if (!Event.Location.IsNearlyZero())
		{
			Object->SetArrayField(TEXT("event_xy_m"), RunResultMakeXyArrayFromCm(Event.Location));
		}
		if (!Event.TargetInstanceId.IsEmpty() && !Object->HasField(TEXT("target_id")))
		{
			Object->SetStringField(TEXT("target_id"), Event.TargetInstanceId);
		}

		return Object;
	}

	FRunResultEventLine RunResultMakeEventLine(const FEpisodeEvaluationEvent& Event)
	{
		FRunResultEventLine Line;
		Line.EventIndex = Event.EventIndex;
		Line.RunTimeSeconds = Event.ElapsedTimeSeconds;
		Line.EventType = RunResultEnumString(Event.EventType);
		Line.Reason = RunResultReasonForEventType(Line.EventType);
		Line.Message = Event.Message;
		Line.Properties = RunResultMakeEventPropertiesObject(Event);
		return Line;
	}

	FRunResultEventLine RunResultMakeSyntheticTerminalEvent(const FEpisodeRunRecord& Record, int32 EventIndex)
	{
		FRunResultEventLine Line;
		Line.EventIndex = EventIndex;
		Line.RunTimeSeconds = RunResultGetDurationSeconds(Record);
		Line.EventType = RunResultTerminalEventTypeString(Record.TerminalReason);
		Line.Reason = RunResultReasonForEventType(Line.EventType);
		Line.Message = FString::Printf(TEXT("Episode ended with terminal reason %s."), *RunResultEnumString(Record.TerminalReason));
		Line.Properties = MakeShared<FJsonObject>();
		return Line;
	}

	TArray<FRunResultEventLine> RunResultBuildEventLines(const FEpisodeRunRecord& Record, int32& OutTerminalEventIndex)
	{
		TArray<FRunResultEventLine> Lines;
		Lines.Reserve(Record.EvaluationResult.Events.Num() + 1);
		for (const FEpisodeEvaluationEvent& Event : Record.EvaluationResult.Events)
		{
			Lines.Add(RunResultMakeEventLine(Event));
		}

		if (RunResultHasTerminalEvent(Record, OutTerminalEventIndex))
		{
			return Lines;
		}

		if (!RunResultTerminalEventTypeString(Record.TerminalReason).IsEmpty())
		{
			const int32 EventIndex = Lines.Num();
			Lines.Add(RunResultMakeSyntheticTerminalEvent(Record, EventIndex));
			OutTerminalEventIndex = EventIndex;
			return Lines;
		}

		OutTerminalEventIndex = INDEX_NONE;
		return Lines;
	}

	TSharedRef<FJsonObject> RunResultMakeSampleObject(const FRunResultSampleInfo& Info)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("sample_id"), Info.SampleId);
		Object->SetStringField(TEXT("scenario_id"), Info.ScenarioId);
		Object->SetStringField(TEXT("template_id"), Info.TemplateId);
		Object->SetStringField(TEXT("template_hash"), Info.TemplateHash);
		Object->SetStringField(TEXT("profile_hash"), Info.ProfileHash);
		Object->SetStringField(TEXT("setting_hash"), Info.SettingHash);
		Object->SetNumberField(TEXT("seed"), static_cast<double>(Info.Seed));
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeRunObject(const FEpisodeRunRecord& Record)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("run_id"), Record.RunId);
		Object->SetStringField(TEXT("episode_id"), Record.EpisodeId);
		Object->SetStringField(TEXT("policy_snapshot_hash"), RunResultUnsetHash);
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeSummaryObject(const FEpisodeRunRecord& Record, int32 TerminalEventIndex)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("completed"), Record.bEvaluationCompleted);
		Object->SetBoolField(TEXT("success"), Record.bSuccess);
		Object->SetStringField(TEXT("outcome"), RunResultEnumString(Record.Outcome));
		Object->SetStringField(TEXT("terminal_reason"), RunResultEnumString(Record.TerminalReason));
		if (TerminalEventIndex == INDEX_NONE)
		{
			Object->SetField(TEXT("terminal_event_index"), MakeShared<FJsonValueNull>());
		}
		else
		{
			Object->SetNumberField(TEXT("terminal_event_index"), TerminalEventIndex);
		}
		Object->SetNumberField(TEXT("duration_s"), RunResultGetDurationSeconds(Record));
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeMetricsObject(const FEpisodeRunRecord& Record)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		TArray<FString> Keys;
		Record.EvaluationResult.Metrics.GetKeys(Keys);
		Keys.Sort();

		for (const FString& Key : Keys)
		{
			const FScenarioParamValue* ParamValue = Record.EvaluationResult.Metrics.Find(Key);
			if (!ParamValue || RunResultIsPolicyKey(Key) || Key == TEXT("score"))
			{
				continue;
			}

			if (Key == TEXT("delivery_bot_failure_location_cm")
				&& ParamValue->Type == EScenarioParamValueType::Vector)
			{
				Object->SetArrayField(TEXT("delivery_bot_failure_xy_m"), RunResultMakeXyArrayFromCm(ParamValue->VectorValue));
				continue;
			}
			if (Key == TEXT("delivery_bot_failure_time_seconds"))
			{
				double TimeSeconds = 0.0;
				if (RunResultTryGetNumberParam(Record.EvaluationResult.Metrics, Key, TimeSeconds))
				{
					Object->SetNumberField(TEXT("delivery_bot_failure_time_s"), TimeSeconds);
				}
				continue;
			}

			Object->SetField(Key, RunResultMakeParamJsonValue(*ParamValue));
		}

		if (!Object->HasField(TEXT("duration_s")))
		{
			Object->SetNumberField(TEXT("duration_s"), RunResultGetDurationSeconds(Record));
		}

		return Object;
	}

	void RunResultSetSortedCountFields(const TSharedRef<FJsonObject>& Object, const TMap<FString, int32>& Counts)
	{
		TArray<FString> Keys;
		Counts.GetKeys(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			if (const int32* Count = Counts.Find(Key))
			{
				Object->SetNumberField(Key, *Count);
			}
		}
	}

	TSharedRef<FJsonObject> RunResultMakeEventSummaryObject(
		const TArray<FRunResultEventLine>& Lines,
		int32 TerminalEventIndex)
	{
		TMap<FString, int32> TypeCounts;
		TMap<FString, int32> SourceCounts;
		for (const FRunResultEventLine& Line : Lines)
		{
			TypeCounts.FindOrAdd(Line.EventType)++;
			SourceCounts.FindOrAdd(Line.Source)++;
		}

		TSharedRef<FJsonObject> ByTypeObject = MakeShared<FJsonObject>();
		RunResultSetSortedCountFields(ByTypeObject, TypeCounts);

		TSharedRef<FJsonObject> BySourceObject = MakeShared<FJsonObject>();
		RunResultSetSortedCountFields(BySourceObject, SourceCounts);

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("total"), Lines.Num());
		Object->SetObjectField(TEXT("by_type"), ByTypeObject);
		Object->SetObjectField(TEXT("by_source"), BySourceObject);
		if (TerminalEventIndex == INDEX_NONE)
		{
			Object->SetField(TEXT("terminal_event_index"), MakeShared<FJsonValueNull>());
		}
		else
		{
			Object->SetNumberField(TEXT("terminal_event_index"), TerminalEventIndex);
		}
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeScenarioParamsObject(const FRunResultSampleInfo& Info)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		TArray<FString> Keys;
		Info.ScenarioParams.GetKeys(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			if (const FScenarioSampleParamValue* ParamValue = Info.ScenarioParams.Find(Key))
			{
				Object->SetField(Key, RunResultMakeSampleParamJsonValue(*ParamValue));
			}
		}
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeScenarioSemanticObject(const FRunResultSampleInfo& Info)
	{
		TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
		SummaryObject->SetNumberField(TEXT("global_min_clear_width_m"), Info.SemanticSummary.GlobalMinClearWidthMeters);
		SummaryObject->SetNumberField(TEXT("min_clear_at_along_m"), Info.SemanticSummary.MinClearAtAlongMeters);
		SummaryObject->SetNumberField(TEXT("total_length_m"), Info.SemanticSummary.TotalLengthMeters);
		SummaryObject->SetBoolField(TEXT("encounter_in_min_clear_zone"), Info.SemanticSummary.bEncounterInMinClearZone);

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetObjectField(TEXT("summary"), SummaryObject);
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeEpisodeResultObject(
		const FEpisodeRunRecord& Record,
		const FRunResultSampleInfo& Info,
		const TArray<FRunResultEventLine>& Lines,
		int32 TerminalEventIndex)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), TEXT("episode_result"));
		Object->SetNumberField(TEXT("version"), 1);
		Object->SetObjectField(TEXT("sample"), RunResultMakeSampleObject(Info));
		Object->SetObjectField(TEXT("run"), RunResultMakeRunObject(Record));
		Object->SetObjectField(TEXT("summary"), RunResultMakeSummaryObject(Record, TerminalEventIndex));
		Object->SetObjectField(TEXT("metrics"), RunResultMakeMetricsObject(Record));
		Object->SetObjectField(TEXT("event_summary"), RunResultMakeEventSummaryObject(Lines, TerminalEventIndex));
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeEventLineObject(const FRunResultEventLine& Line)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), TEXT("episode_event"));
		Object->SetNumberField(TEXT("version"), 1);
		Object->SetNumberField(TEXT("event_index"), Line.EventIndex);
		Object->SetNumberField(TEXT("run_time_seconds"), Line.RunTimeSeconds);
		Object->SetStringField(TEXT("source"), Line.Source);
		Object->SetStringField(TEXT("event_type"), Line.EventType);
		Object->SetStringField(TEXT("reason"), Line.Reason);
		Object->SetStringField(TEXT("message"), Line.Message);
		if (Line.ActionSequence.IsSet())
		{
			Object->SetNumberField(TEXT("action_sequence"), Line.ActionSequence.GetValue());
		}
		else
		{
			Object->SetField(TEXT("action_sequence"), MakeShared<FJsonValueNull>());
		}
		Object->SetObjectField(TEXT("properties"), Line.Properties.IsValid() ? Line.Properties.ToSharedRef() : MakeShared<FJsonObject>());
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeRunSummaryRunObject(const TArray<FEpisodeRunRecord>& Records)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("run_id"), Records.IsEmpty() ? FString() : Records[0].RunId);
		Object->SetStringField(TEXT("experiment_id"), Records.IsEmpty() ? FString() : RunResultMakeExperimentId(Records[0]));
		Object->SetField(TEXT("started_at"), MakeShared<FJsonValueNull>());
		Object->SetField(TEXT("ended_at"), MakeShared<FJsonValueNull>());
		Object->SetStringField(TEXT("policy_snapshot_hash"), RunResultUnsetHash);
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeRunSummaryRow(const FEpisodeRunRecord& Record)
	{
		const FRunResultSampleInfo Info = RunResultLoadSampleInfo(Record);
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("episode_id"), Record.EpisodeId);
		Object->SetStringField(TEXT("sample_id"), Info.SampleId);
		Object->SetStringField(TEXT("scenario_id"), Info.ScenarioId);
		Object->SetStringField(TEXT("template_id"), Info.TemplateId);
		Object->SetStringField(TEXT("template_hash"), Info.TemplateHash);
		Object->SetStringField(TEXT("profile_hash"), Info.ProfileHash);
		Object->SetStringField(TEXT("setting_hash"), Info.SettingHash);
		Object->SetNumberField(TEXT("seed"), static_cast<double>(Info.Seed));
		Object->SetStringField(TEXT("outcome"), RunResultEnumString(Record.Outcome));
		Object->SetStringField(TEXT("terminal_reason"), RunResultEnumString(Record.TerminalReason));
		Object->SetNumberField(TEXT("duration_s"), RunResultGetDurationSeconds(Record));
		Object->SetBoolField(TEXT("usable_for_llm_tuning"), RunResultIsUsableForLlmTuning(Record));
		Object->SetObjectField(TEXT("metrics"), RunResultMakeMetricsObject(Record));
		Object->SetObjectField(TEXT("scenario_params"), RunResultMakeScenarioParamsObject(Info));
		Object->SetObjectField(TEXT("scenario_semantic"), RunResultMakeScenarioSemanticObject(Info));
		return Object;
	}

	TSharedRef<FJsonObject> RunResultMakeRunSummaryObject(const TArray<FEpisodeRunRecord>& Records)
	{
		TArray<TSharedPtr<FJsonValue>> Rows;
		Rows.Reserve(Records.Num());
		for (const FEpisodeRunRecord& Record : Records)
		{
			Rows.Add(MakeShared<FJsonValueObject>(RunResultMakeRunSummaryRow(Record)));
		}

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), TEXT("run_summary"));
		Object->SetNumberField(TEXT("version"), 1);
		Object->SetObjectField(TEXT("run"), RunResultMakeRunSummaryRunObject(Records));
		Object->SetArrayField(TEXT("rows"), Rows);
		return Object;
	}

	bool RunResultTryWriteObjectJson(
		const TSharedRef<FJsonObject>& Object,
		FString& OutJson,
		TArray<FString>& OutDiagnostics)
	{
		OutJson.Reset();
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(Object, Writer))
		{
			OutDiagnostics.Add(TEXT("Run result JSON serialization failed."));
			return false;
		}

		return true;
	}

	bool RunResultTryWriteObjectLine(
		const TSharedRef<FJsonObject>& Object,
		FString& OutLine,
		TArray<FString>& OutDiagnostics)
	{
		OutLine.Reset();
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutLine);
		if (!FJsonSerializer::Serialize(Object, Writer))
		{
			OutDiagnostics.Add(TEXT("Episode event JSON line serialization failed."));
			return false;
		}

		return true;
	}
}

bool FEpisodeRunResultJson::TryWriteEpisodeResultJson(
	const FEpisodeRunRecord& Record,
	FString& OutJson,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	int32 TerminalEventIndex = INDEX_NONE;
	const TArray<FRunResultEventLine> Lines = RunResultBuildEventLines(Record, TerminalEventIndex);
	const FRunResultSampleInfo Info = RunResultLoadSampleInfo(Record);
	return RunResultTryWriteObjectJson(
		RunResultMakeEpisodeResultObject(Record, Info, Lines, TerminalEventIndex),
		OutJson,
		OutDiagnostics);
}

bool FEpisodeRunResultJson::TryWriteEpisodeEventsJsonl(
	const FEpisodeRunRecord& Record,
	FString& OutJsonl,
	TArray<FString>& OutDiagnostics)
{
	OutJsonl.Reset();
	OutDiagnostics.Reset();

	int32 TerminalEventIndex = INDEX_NONE;
	const TArray<FRunResultEventLine> Lines = RunResultBuildEventLines(Record, TerminalEventIndex);
	TArray<FString> JsonLines;
	JsonLines.Reserve(Lines.Num());
	for (const FRunResultEventLine& Line : Lines)
	{
		FString JsonLine;
		if (!RunResultTryWriteObjectLine(RunResultMakeEventLineObject(Line), JsonLine, OutDiagnostics))
		{
			return false;
		}
		JsonLines.Add(JsonLine);
	}

	OutJsonl = FString::Join(JsonLines, TEXT("\n"));
	if (!OutJsonl.IsEmpty())
	{
		OutJsonl.AppendChar(TEXT('\n'));
	}
	return true;
}

bool FEpisodeRunResultJson::TryWriteRunSummaryJson(
	const TArray<FEpisodeRunRecord>& Records,
	FString& OutJson,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	return RunResultTryWriteObjectJson(RunResultMakeRunSummaryObject(Records), OutJson, OutDiagnostics);
}
