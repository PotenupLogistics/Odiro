#include "Shared/EpisodeEvaluationReportJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr double CmToM = 0.01;

	template <typename TEnum>
	FString ToReportEnumString(TEnum Value)
	{
		if (const UEnum* EnumValue = StaticEnum<TEnum>())
		{
			return EnumValue->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}

	TArray<TSharedPtr<FJsonValue>> MakeXyArrayM(double X, double Y)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(2);
		Array.Add(MakeShared<FJsonValueNumber>(X));
		Array.Add(MakeShared<FJsonValueNumber>(Y));
		return Array;
	}

	TArray<TSharedPtr<FJsonValue>> MakeXyArrayFromCm(const FVector& LocationCm)
	{
		return MakeXyArrayM(LocationCm.X * CmToM, LocationCm.Y * CmToM);
	}

	TArray<TSharedPtr<FJsonValue>> MakeVectorArray(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(3);
		Array.Add(MakeShared<FJsonValueNumber>(Value.X));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return Array;
	}

	TSharedPtr<FJsonValue> MakeParamJsonValue(const FEpisodeParamValue& ParamValue)
	{
		switch (ParamValue.Type)
		{
		case EEpisodeParamValueType::Bool:
			return MakeShared<FJsonValueBoolean>(ParamValue.BoolValue);
		case EEpisodeParamValueType::Integer:
			return MakeShared<FJsonValueNumber>(ParamValue.IntegerValue);
		case EEpisodeParamValueType::Float:
			return MakeShared<FJsonValueNumber>(ParamValue.FloatValue);
		case EEpisodeParamValueType::String:
			return MakeShared<FJsonValueString>(ParamValue.StringValue);
		case EEpisodeParamValueType::Vector:
			return MakeShared<FJsonValueArray>(MakeVectorArray(ParamValue.VectorValue));
		case EEpisodeParamValueType::None:
		default:
			return MakeShared<FJsonValueNull>();
		}
	}

	bool TryGetStringParam(
		const TMap<FString, FEpisodeParamValue>& Params,
		const FString& Key,
		FString& OutValue)
	{
		const FEpisodeParamValue* ParamValue = Params.Find(Key);
		if (!ParamValue || ParamValue->Type != EEpisodeParamValueType::String)
		{
			return false;
		}

		OutValue = ParamValue->StringValue;
		return true;
	}

	bool TryGetNumberParam(
		const TMap<FString, FEpisodeParamValue>& Params,
		const FString& Key,
		double& OutValue)
	{
		const FEpisodeParamValue* ParamValue = Params.Find(Key);
		if (!ParamValue)
		{
			return false;
		}

		if (ParamValue->Type == EEpisodeParamValueType::Float)
		{
			OutValue = ParamValue->FloatValue;
			return true;
		}

		if (ParamValue->Type == EEpisodeParamValueType::Integer)
		{
			OutValue = ParamValue->IntegerValue;
			return true;
		}

		return false;
	}

	bool IsPolicyReportKey(const FString& Key)
	{
		return Key.StartsWith(TEXT("policy_"));
	}

	bool IsTunableDeliveryBotFailureType(const FString& FailureType)
	{
		return FailureType == TEXT("RobotTipOver")
			|| FailureType == TEXT("PathFindingFailed")
			|| FailureType == TEXT("Stuck");
	}

	FString FindDeliveryBotFailureType(const FEpisodeEvaluationResult& Result)
	{
		FString FailureType;
		if (TryGetStringParam(Result.Metrics, TEXT("delivery_bot_failure_type"), FailureType))
		{
			return FailureType;
		}

		for (const FEpisodeEvaluationEvent& Event : Result.Events)
		{
			if (Event.EventType != EEpisodeEvaluationEventType::DeliveryBotSimulationFailure)
			{
				continue;
			}

			if (TryGetStringParam(Event.Properties, TEXT("delivery_bot_failure_type"), FailureType))
			{
				return FailureType;
			}

			if (TryGetStringParam(Event.Properties, TEXT("failure_type"), FailureType))
			{
				return FailureType;
			}
		}

		return FString();
	}

	double GetDurationSeconds(const FEpisodeRunRecord& Record)
	{
		if (Record.DurationSeconds > 0.0)
		{
			return Record.DurationSeconds;
		}

		return Record.EvaluationResult.DurationSeconds;
	}

	double GetScore(const FEpisodeRunRecord& Record)
	{
		double Score = 0.0;
		TryGetNumberParam(Record.EvaluationResult.Metrics, TEXT("score"), Score);
		return Score;
	}

	bool IsScoreDeltaEventType(EEpisodeEvaluationEventType EventType)
	{
		switch (EventType)
		{
		case EEpisodeEvaluationEventType::StaticObstacleCollision:
		case EEpisodeEvaluationEventType::BlockedRegionCollision:
		case EEpisodeEvaluationEventType::PenaltyRegionViolation:
		case EEpisodeEvaluationEventType::PedestrianNearMiss:
		case EEpisodeEvaluationEventType::PedestrianCollision:
			return true;
		default:
			return false;
		}
	}

	bool IsUsableForLlmTuning(const FEpisodeRunRecord& Record)
	{
		if (!Record.bCompileSucceeded
			|| !Record.bEpisodeSetupCompileSucceeded
			|| !Record.bDeliveryBotSetupCompileSucceeded
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
			return IsTunableDeliveryBotFailureType(FindDeliveryBotFailureType(Record.EvaluationResult));
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

	void SetSortedCountFields(
		const TSharedRef<FJsonObject>& Object,
		const TMap<FString, int32>& Counts)
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

	void SetParamField(
		const TSharedRef<FJsonObject>& Object,
		const FString& Key,
		const FEpisodeParamValue& ParamValue)
	{
		Object->SetField(Key, MakeParamJsonValue(ParamValue));
	}

	TSharedRef<FJsonObject> MakeUnitsObject()
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("time"), TEXT("s"));
		Object->SetStringField(TEXT("distance"), TEXT("m"));
		Object->SetStringField(TEXT("angle"), TEXT("deg"));
		Object->SetStringField(TEXT("location"), TEXT("xy_m"));
		Object->SetStringField(TEXT("speed"), TEXT("km/h"));
		Object->SetStringField(TEXT("score"), TEXT("episode_score"));
		return Object;
	}

	TSharedRef<FJsonObject> MakeRunObject(const FEpisodeRunRecord& Record)
	{
		TSharedRef<FJsonObject> EpisodeSetupObject = MakeShared<FJsonObject>();
		EpisodeSetupObject->SetStringField(TEXT("path"), Record.EpisodeSetupJsonPath);
		EpisodeSetupObject->SetStringField(TEXT("hash"), Record.EpisodeSetupHash);

		TSharedRef<FJsonObject> DeliveryBotSetupObject = MakeShared<FJsonObject>();
		DeliveryBotSetupObject->SetStringField(TEXT("path"), Record.DeliveryBotSetupJsonPath);
		DeliveryBotSetupObject->SetStringField(TEXT("hash"), Record.DeliveryBotSetupHash);

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("run_id"), Record.RunId);
		Object->SetNumberField(TEXT("run_index"), Record.RunIndex);
		Object->SetStringField(TEXT("episode_id"), Record.EpisodeId);
		Object->SetStringField(TEXT("pair_id"), Record.PairId);
		Object->SetObjectField(TEXT("episode_setup"), EpisodeSetupObject);
		Object->SetObjectField(TEXT("delivery_bot_setup"), DeliveryBotSetupObject);
		Object->SetStringField(TEXT("pair_hash"), Record.PairHash);
		return Object;
	}

	TSharedRef<FJsonObject> MakeSummaryObject(const FEpisodeRunRecord& Record)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("completed"), Record.bEvaluationCompleted);
		Object->SetBoolField(TEXT("success"), Record.bSuccess);
		Object->SetStringField(TEXT("outcome"), ToReportEnumString(Record.Outcome));
		Object->SetStringField(TEXT("terminal_reason"), ToReportEnumString(Record.TerminalReason));
		Object->SetNumberField(TEXT("duration_s"), GetDurationSeconds(Record));
		Object->SetNumberField(TEXT("score"), GetScore(Record));
		Object->SetBoolField(TEXT("usable_for_llm_tuning"), IsUsableForLlmTuning(Record));
		return Object;
	}

	TSharedRef<FJsonObject> MakePipelineObject(const FEpisodeRunRecord& Record)
	{
		TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
		DiagnosticValues.Reserve(Record.Diagnostics.Num());
		for (const FString& Diagnostic : Record.Diagnostics)
		{
			DiagnosticValues.Add(MakeShared<FJsonValueString>(Diagnostic));
		}

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("episode_setup_compiled"), Record.bEpisodeSetupCompileSucceeded);
		Object->SetBoolField(TEXT("delivery_bot_setup_compiled"), Record.bDeliveryBotSetupCompileSucceeded);
		Object->SetBoolField(TEXT("world_setup_succeeded"), Record.bSetupSucceeded);
		Object->SetBoolField(TEXT("evaluation_completed"), Record.bEvaluationCompleted);
		Object->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
		return Object;
	}

	TSharedRef<FJsonObject> MakeMetricsObject(const FEpisodeRunRecord& Record)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();

		TArray<FString> Keys;
		Record.EvaluationResult.Metrics.GetKeys(Keys);
		Keys.Sort();

		for (const FString& Key : Keys)
		{
			const FEpisodeParamValue* ParamValue = Record.EvaluationResult.Metrics.Find(Key);
			if (!ParamValue || IsPolicyReportKey(Key))
			{
				continue;
			}

			if (Key == TEXT("delivery_bot_failure_location_cm")
				&& ParamValue->Type == EEpisodeParamValueType::Vector)
			{
				Object->SetArrayField(
					TEXT("delivery_bot_failure_xy_m"),
					MakeXyArrayFromCm(ParamValue->VectorValue));
				continue;
			}

			if (Key == TEXT("delivery_bot_failure_time_seconds"))
			{
				double TimeSeconds = 0.0;
				if (TryGetNumberParam(Record.EvaluationResult.Metrics, Key, TimeSeconds))
				{
					Object->SetNumberField(TEXT("delivery_bot_failure_time_s"), TimeSeconds);
				}
				continue;
			}

			SetParamField(Object, Key, *ParamValue);
		}

		if (!Object->HasField(TEXT("score")))
		{
			Object->SetNumberField(TEXT("score"), GetScore(Record));
		}

		if (!Object->HasField(TEXT("duration_s")))
		{
			Object->SetNumberField(TEXT("duration_s"), GetDurationSeconds(Record));
		}

		return Object;
	}

	TSharedRef<FJsonObject> MakeEventPropertiesObject(const FEpisodeEvaluationEvent& Event)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();

		if (Event.EventType == EEpisodeEvaluationEventType::DeliveryBotSimulationFailure)
		{
			FString FailureType;
			if (TryGetStringParam(Event.Properties, TEXT("delivery_bot_failure_type"), FailureType)
				|| TryGetStringParam(Event.Properties, TEXT("failure_type"), FailureType))
			{
				Object->SetStringField(TEXT("failure_type"), FailureType);
			}

			double SpeedKmh = 0.0;
			if (TryGetNumberParam(Event.Properties, TEXT("delivery_bot_failure_speed_kmh"), SpeedKmh)
				|| TryGetNumberParam(Event.Properties, TEXT("speed_kmh"), SpeedKmh))
			{
				Object->SetNumberField(TEXT("speed_kmh"), SpeedKmh);
			}

			FString TargetActorName;
			if (TryGetStringParam(Event.Properties, TEXT("delivery_bot_failure_target_actor_name"), TargetActorName)
				|| TryGetStringParam(Event.Properties, TEXT("target_actor_name"), TargetActorName))
			{
				Object->SetStringField(TEXT("target_actor_name"), TargetActorName);
			}

			return Object;
		}

		TArray<FString> Keys;
		Event.Properties.GetKeys(Keys);
		Keys.Sort();

		for (const FString& Key : Keys)
		{
			const FEpisodeParamValue* ParamValue = Event.Properties.Find(Key);
			if (!ParamValue || IsPolicyReportKey(Key))
			{
				continue;
			}

			SetParamField(Object, Key, *ParamValue);
		}

		if (IsScoreDeltaEventType(Event.EventType))
		{
			Object->SetNumberField(TEXT("score_delta"), Event.Value);
		}

		return Object;
	}

	TSharedRef<FJsonObject> MakeEventObject(const FEpisodeEvaluationEvent& Event)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("i"), Event.EventIndex);
		Object->SetNumberField(TEXT("t_s"), Event.ElapsedTimeSeconds);
		Object->SetStringField(TEXT("type"), ToReportEnumString(Event.EventType));
		Object->SetStringField(TEXT("severity"), ToReportEnumString(Event.Severity));
		Object->SetStringField(TEXT("subject"), Event.SubjectInstanceId);
		Object->SetStringField(TEXT("target"), Event.TargetInstanceId);
		Object->SetArrayField(TEXT("xy_m"), MakeXyArrayFromCm(Event.Location));
		Object->SetStringField(TEXT("message"), Event.Message);
		Object->SetObjectField(TEXT("properties"), MakeEventPropertiesObject(Event));
		return Object;
	}

	TSharedRef<FJsonObject> MakeEventSummaryObject(const FEpisodeEvaluationResult& Result)
	{
		TMap<FString, int32> TypeCounts;
		TMap<FString, int32> SeverityCounts;
		SeverityCounts.Add(TEXT("Info"), 0);
		SeverityCounts.Add(TEXT("Warning"), 0);
		SeverityCounts.Add(TEXT("Failure"), 0);

		int32 FirstFailureEventIndex = INDEX_NONE;
		for (const FEpisodeEvaluationEvent& Event : Result.Events)
		{
			const FString TypeName = ToReportEnumString(Event.EventType);
			TypeCounts.FindOrAdd(TypeName)++;

			const FString SeverityName = ToReportEnumString(Event.Severity);
			SeverityCounts.FindOrAdd(SeverityName)++;

			if (FirstFailureEventIndex == INDEX_NONE
				&& Event.Severity == EEpisodeEvaluationEventSeverity::Failure)
			{
				FirstFailureEventIndex = Event.EventIndex;
			}
		}

		TSharedRef<FJsonObject> ByTypeObject = MakeShared<FJsonObject>();
		SetSortedCountFields(ByTypeObject, TypeCounts);

		TSharedRef<FJsonObject> BySeverityObject = MakeShared<FJsonObject>();
		SetSortedCountFields(BySeverityObject, SeverityCounts);

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("total"), Result.Events.Num());
		Object->SetObjectField(TEXT("by_type"), ByTypeObject);
		Object->SetObjectField(TEXT("by_severity"), BySeverityObject);
		if (FirstFailureEventIndex == INDEX_NONE)
		{
			Object->SetField(TEXT("first_failure_event_index"), MakeShared<FJsonValueNull>());
		}
		else
		{
			Object->SetNumberField(TEXT("first_failure_event_index"), FirstFailureEventIndex);
		}
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> MakeEventArray(const FEpisodeEvaluationResult& Result)
	{
		TArray<TSharedPtr<FJsonValue>> EventValues;
		EventValues.Reserve(Result.Events.Num());
		for (const FEpisodeEvaluationEvent& Event : Result.Events)
		{
			EventValues.Add(MakeShared<FJsonValueObject>(MakeEventObject(Event)));
		}
		return EventValues;
	}
}

bool FEpisodeEvaluationReportJson::TryWriteReportJson(
	const FEpisodeRunRecord& Record,
	FString& OutJson,
	TArray<FString>& OutDiagnostics)
{
	OutJson.Reset();
	OutDiagnostics.Reset();

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(MakeReportObject(Record), Writer))
	{
		OutDiagnostics.Add(TEXT("Episode evaluation report JSON serialization failed."));
		return false;
	}

	return true;
}

TSharedRef<FJsonObject> FEpisodeEvaluationReportJson::MakeReportObject(const FEpisodeRunRecord& Record)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("schema"), TEXT("episode_evaluation_report"));
	Object->SetNumberField(TEXT("version"), 1);
	Object->SetObjectField(TEXT("units"), MakeUnitsObject());
	Object->SetObjectField(TEXT("run"), MakeRunObject(Record));
	Object->SetObjectField(TEXT("summary"), MakeSummaryObject(Record));
	Object->SetObjectField(TEXT("pipeline"), MakePipelineObject(Record));
	Object->SetObjectField(TEXT("metrics"), MakeMetricsObject(Record));
	Object->SetObjectField(TEXT("event_summary"), MakeEventSummaryObject(Record.EvaluationResult));
	Object->SetArrayField(TEXT("events"), MakeEventArray(Record.EvaluationResult));
	return Object;
}
