#include "Shared/EpisodeMeasurementLogTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* HeaderRecordType = TEXT("header");
	const TCHAR* TickRecordType = TEXT("tick");
	const TCHAR* EventRecordType = TEXT("event");
	const TCHAR* FooterRecordType = TEXT("footer");

	TArray<TSharedPtr<FJsonValue>> MakeVectorArray(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(3);
		Array.Add(MakeShared<FJsonValueNumber>(Value.X));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return Array;
	}

	TArray<TSharedPtr<FJsonValue>> MakeQuatArray(const TArray<double>& RotationQuatXyzw)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(4);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const double Value = RotationQuatXyzw.IsValidIndex(Index)
				? RotationQuatXyzw[Index]
				: (Index == 3 ? 1.0 : 0.0);
			Array.Add(MakeShared<FJsonValueNumber>(Value));
		}
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

	TSharedRef<FJsonObject> MakeDiagnosticObject(const FEpisodeMeasurementLogDiagnostic& Diagnostic)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("severity"), FEpisodeMeasurementLogJson::ToSeverityString(Diagnostic.Severity));
		Object->SetStringField(TEXT("code"), Diagnostic.Code);
		Object->SetStringField(TEXT("message"), Diagnostic.Message);
		Object->SetNumberField(TEXT("worldTimeSeconds"), Diagnostic.WorldTimeSeconds);
		return Object;
	}

	TSharedRef<FJsonObject> MakeActorStateObject(const FEpisodeMeasurementLogActorState& State, bool bIncludeActorIndex)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		if (bIncludeActorIndex)
		{
			Object->SetNumberField(TEXT("i"), State.ActorIndex);
		}
		Object->SetArrayField(TEXT("p"), MakeVectorArray(State.PositionCm));
		Object->SetArrayField(TEXT("q"), MakeQuatArray(State.RotationQuatXyzw));
		Object->SetArrayField(TEXT("v"), MakeVectorArray(State.VelocityCmPerSecond));
		return Object;
	}

	TSharedRef<FJsonObject> MakeLidarObject(const FEpisodeMeasurementLogLidarSnapshot& Lidar)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("hasFrontObject"), Lidar.bHasFrontObject);
		if (!Lidar.bHasFrontObject)
		{
			Object->SetField(TEXT("frontObjectId"), MakeShared<FJsonValueNull>());
			Object->SetNumberField(TEXT("frontDistanceM"), 0.0);
			Object->SetNumberField(TEXT("frontYawDegree"), 0.0);
			Object->SetNumberField(TEXT("hitCount"), Lidar.HitCount);
			return Object;
		}

		Object->SetStringField(TEXT("frontObjectId"), Lidar.FrontObjectId);
		Object->SetNumberField(TEXT("frontDistanceM"), Lidar.FrontDistanceM);
		Object->SetNumberField(TEXT("frontYawDegree"), Lidar.FrontYawDegree);
		Object->SetNumberField(TEXT("hitCount"), Lidar.HitCount);
		return Object;
	}

	TSharedRef<FJsonObject> MakeActionObject(const FEpisodeMeasurementLogRobotAction& Action)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("targetSpeedKmh"), Action.TargetSpeedKmh);
		Object->SetNumberField(TEXT("steering"), Action.Steering);
		Object->SetNumberField(TEXT("brake"), Action.Brake);
		Object->SetBoolField(TEXT("brakeApplied"), Action.bBrakeApplied);
		Object->SetStringField(TEXT("reason"), Action.Reason);
		return Object;
	}

	TSharedRef<FJsonObject> MakeRobotTickObject(const FEpisodeMeasurementLogRobotTick& Robot)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> PerceptionObject = MakeShared<FJsonObject>();
		PerceptionObject->SetObjectField(TEXT("lidar"), MakeLidarObject(Robot.Lidar));

		Object->SetStringField(TEXT("id"), Robot.Id);
		Object->SetObjectField(TEXT("truth"), MakeActorStateObject(Robot.Truth, false));
		Object->SetObjectField(TEXT("perception"), PerceptionObject);
		Object->SetObjectField(TEXT("action"), MakeActionObject(Robot.Action));
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			Array.Add(MakeShared<FJsonValueString>(Value));
		}
		return Array;
	}

	bool IsBlockingSeverity(EEpisodeMeasurementLogSeverity Severity)
	{
		return Severity == EEpisodeMeasurementLogSeverity::Error
			|| Severity == EEpisodeMeasurementLogSeverity::Failure;
	}

	bool HasErrorSince(
		const TArray<FEpisodeMeasurementLogDiagnostic>& Diagnostics,
		int32 StartIndex)
	{
		for (int32 Index = StartIndex; Index < Diagnostics.Num(); ++Index)
		{
			if (IsBlockingSeverity(Diagnostics[Index].Severity))
			{
				return true;
			}
		}

		return false;
	}

}

FString FEpisodeMeasurementLogJson::MakeDefaultFileName(
	const FEpisodeMeasurementLogSettings& Settings,
	const FString& MapName,
	const FDateTime& Timestamp)
{
	const FString SafePrefix = SanitizeFileToken(Settings.FilePrefix.IsEmpty()
		? TEXT("MeasurementLog")
		: Settings.FilePrefix);
	const FString SafeMapName = SanitizeFileToken(MapName.IsEmpty()
		? TEXT("UnknownMap")
		: MapName);
	return FString::Printf(
		TEXT("%s_%s_%s.jsonl"),
		*SafePrefix,
		*Timestamp.ToString(TEXT("%Y%m%d_%H%M%S")),
		*SafeMapName);
}

FString FEpisodeMeasurementLogJson::SanitizeFileToken(const FString& Value)
{
	FString SafeValue = FPaths::MakeValidFileName(Value);
	if (SafeValue.IsEmpty())
	{
		return TEXT("Unknown");
	}

	SafeValue.ReplaceInline(TEXT(".."), TEXT("_"));
	SafeValue.ReplaceInline(TEXT("/"), TEXT("_"));
	SafeValue.ReplaceInline(TEXT("\\"), TEXT("_"));
	return SafeValue;
}

FString FEpisodeMeasurementLogJson::ToSeverityString(EEpisodeMeasurementLogSeverity Severity)
{
	switch (Severity)
	{
	case EEpisodeMeasurementLogSeverity::Info:
		return TEXT("info");
	case EEpisodeMeasurementLogSeverity::Warning:
		return TEXT("warning");
	case EEpisodeMeasurementLogSeverity::Error:
		return TEXT("error");
	case EEpisodeMeasurementLogSeverity::Failure:
		return TEXT("failure");
	default:
		return TEXT("unknown");
	}
}

FString FEpisodeMeasurementLogJson::ToActorCategoryString(EEpisodeActorCategory Category)
{
	switch (Category)
	{
	case EEpisodeActorCategory::DeliveryBot:
		return TEXT("DeliveryBot");
	case EEpisodeActorCategory::StaticObstacle:
		return TEXT("StaticObstacle");
	case EEpisodeActorCategory::Pedestrian:
		return TEXT("Pedestrian");
	default:
		return TEXT("Unknown");
	}
}

FEpisodeMeasurementLogDiagnostic FEpisodeMeasurementLogJson::MakeDiagnostic(
	EEpisodeMeasurementLogSeverity Severity,
	const FString& Code,
	const FString& Message,
	double WorldTimeSeconds)
{
	FEpisodeMeasurementLogDiagnostic Diagnostic;
	Diagnostic.Severity = Severity;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.WorldTimeSeconds = WorldTimeSeconds;
	return Diagnostic;
}

void FEpisodeMeasurementLogJson::ValidateHeaderRecord(
	const FEpisodeMeasurementLogHeaderRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	if (Record.Version <= 0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_version"),
			TEXT("header.version must be 1 or greater.")));
	}

	if (Record.LogId.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_log_id"),
			TEXT("header.logId is required.")));
	}

	if (Record.MapName.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_map_name"),
			TEXT("header.mapName is empty.")));
	}

	for (int32 Index = 0; Index < Record.Actors.Num(); ++Index)
	{
		const FEpisodeMeasurementLogActorInfo& Actor = Record.Actors[Index];
		if (Actor.Index != Index)
		{
			OutDiagnostics.Add(MakeDiagnostic(
				EEpisodeMeasurementLogSeverity::Error,
				TEXT("non_contiguous_actor_index"),
				FString::Printf(TEXT("header.actors[%d].index must be %d."), Index, Index)));
		}

		if (Actor.Id.IsEmpty())
		{
			OutDiagnostics.Add(MakeDiagnostic(
				EEpisodeMeasurementLogSeverity::Error,
				TEXT("missing_actor_id"),
				FString::Printf(TEXT("header.actors[%d].id is required."), Index)));
		}
	}
}

void FEpisodeMeasurementLogJson::ValidateTickRecord(
	const FEpisodeMeasurementLogTickRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	if (Record.TickIndex < 0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_tick_index"),
			TEXT("tick.tickIndex must be 0 or greater."),
			Record.WorldTimeSeconds));
	}

	if (Record.WorldTimeSeconds < 0.0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_world_time"),
			TEXT("tick.worldTimeSeconds must be 0 or greater."),
			Record.WorldTimeSeconds));
	}

	if (Record.DeltaSeconds < 0.0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_delta_seconds"),
			TEXT("tick.deltaSeconds must be 0 or greater."),
			Record.WorldTimeSeconds));
	}

	if (Record.Robot.Id.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_robot_id"),
			TEXT("tick.robot.id is empty."),
			Record.WorldTimeSeconds));
	}

	if (Record.Robot.Truth.RotationQuatXyzw.Num() != 4)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_robot_rotation"),
			TEXT("tick.robot.truth.q must contain quaternion [x,y,z,w] values."),
			Record.WorldTimeSeconds));
	}

	for (int32 Index = 0; Index < Record.MovingActors.Num(); ++Index)
	{
		const FEpisodeMeasurementLogActorState& State = Record.MovingActors[Index];
		if (State.ActorIndex < 0)
		{
			OutDiagnostics.Add(MakeDiagnostic(
				EEpisodeMeasurementLogSeverity::Error,
				TEXT("invalid_moving_actor_index"),
				FString::Printf(TEXT("tick.movingActors[%d].i must be 0 or greater."), Index),
				Record.WorldTimeSeconds));
		}

		if (State.RotationQuatXyzw.Num() != 4)
		{
			OutDiagnostics.Add(MakeDiagnostic(
				EEpisodeMeasurementLogSeverity::Error,
				TEXT("invalid_moving_actor_rotation"),
				FString::Printf(TEXT("tick.movingActors[%d].q must contain quaternion [x,y,z,w] values."), Index),
				Record.WorldTimeSeconds));
		}
	}
}

void FEpisodeMeasurementLogJson::ValidateEventRecord(
	const FEpisodeMeasurementLogEventRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	if (Record.EventIndex < 0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_event_index"),
			TEXT("event.eventIndex must be 0 or greater."),
			Record.WorldTimeSeconds));
	}

	if (Record.WorldTimeSeconds < 0.0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_world_time"),
			TEXT("event.worldTimeSeconds must be 0 or greater."),
			Record.WorldTimeSeconds));
	}

	if (Record.Kind.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_event_kind"),
			TEXT("event.kind is required."),
			Record.WorldTimeSeconds));
	}
}

void FEpisodeMeasurementLogJson::ValidateFooterRecord(
	const FEpisodeMeasurementLogFooterRecord& Record,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	if (Record.Ticks < 0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_tick_count"),
			TEXT("footer.ticks must be 0 or greater.")));
	}

	if (Record.Events < 0)
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("invalid_event_count"),
			TEXT("footer.events must be 0 or greater.")));
	}

	if (Record.CloseReason.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_close_reason"),
			TEXT("footer.closeReason is empty.")));
	}
}

bool FEpisodeMeasurementLogJson::HasError(const TArray<FEpisodeMeasurementLogDiagnostic>& Diagnostics)
{
	for (const FEpisodeMeasurementLogDiagnostic& Diagnostic : Diagnostics)
	{
		if (IsBlockingSeverity(Diagnostic.Severity))
		{
			return true;
		}
	}

	return false;
}

bool FEpisodeMeasurementLogJson::TryWriteHeaderLine(
	const FEpisodeMeasurementLogHeaderRecord& Record,
	FString& OutJsonLine,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	const int32 FirstNewDiagnosticIndex = OutDiagnostics.Num();
	ValidateHeaderRecord(Record, OutDiagnostics);
	if (HasErrorSince(OutDiagnostics, FirstNewDiagnosticIndex))
	{
		return false;
	}

	if (!TryWriteJsonLine(MakeHeaderObject(Record), OutJsonLine))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("json_serialize_failed"),
			TEXT("header record JSON serialization failed.")));
		return false;
	}

	return true;
}

bool FEpisodeMeasurementLogJson::TryWriteTickLine(
	const FEpisodeMeasurementLogTickRecord& Record,
	FString& OutJsonLine,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	const int32 FirstNewDiagnosticIndex = OutDiagnostics.Num();
	ValidateTickRecord(Record, OutDiagnostics);
	if (HasErrorSince(OutDiagnostics, FirstNewDiagnosticIndex))
	{
		return false;
	}

	if (!TryWriteJsonLine(MakeTickObject(Record), OutJsonLine))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("json_serialize_failed"),
			TEXT("tick record JSON serialization failed."),
			Record.WorldTimeSeconds));
		return false;
	}

	return true;
}

bool FEpisodeMeasurementLogJson::TryWriteEventLine(
	const FEpisodeMeasurementLogEventRecord& Record,
	FString& OutJsonLine,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	const int32 FirstNewDiagnosticIndex = OutDiagnostics.Num();
	ValidateEventRecord(Record, OutDiagnostics);
	if (HasErrorSince(OutDiagnostics, FirstNewDiagnosticIndex))
	{
		return false;
	}

	if (!TryWriteJsonLine(MakeEventObject(Record), OutJsonLine))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("json_serialize_failed"),
			TEXT("event record JSON serialization failed."),
			Record.WorldTimeSeconds));
		return false;
	}

	return true;
}

bool FEpisodeMeasurementLogJson::TryWriteFooterLine(
	const FEpisodeMeasurementLogFooterRecord& Record,
	FString& OutJsonLine,
	TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics)
{
	const int32 FirstNewDiagnosticIndex = OutDiagnostics.Num();
	ValidateFooterRecord(Record, OutDiagnostics);
	if (HasErrorSince(OutDiagnostics, FirstNewDiagnosticIndex))
	{
		return false;
	}

	if (!TryWriteJsonLine(MakeFooterObject(Record), OutJsonLine))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("json_serialize_failed"),
			TEXT("footer record JSON serialization failed.")));
		return false;
	}

	return true;
}

TSharedRef<FJsonObject> FEpisodeMeasurementLogJson::MakeHeaderObject(
	const FEpisodeMeasurementLogHeaderRecord& Record)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> UnitsObject = MakeShared<FJsonObject>();
	UnitsObject->SetStringField(TEXT("position"), Record.Units.Position);
	UnitsObject->SetStringField(TEXT("velocity"), Record.Units.Velocity);
	UnitsObject->SetStringField(TEXT("rotation"), Record.Units.Rotation);
	UnitsObject->SetStringField(TEXT("axes"), Record.Units.Axes);

	TArray<TSharedPtr<FJsonValue>> ActorValues;
	ActorValues.Reserve(Record.Actors.Num());
	for (const FEpisodeMeasurementLogActorInfo& Actor : Record.Actors)
	{
		TSharedRef<FJsonObject> ActorObject = MakeShared<FJsonObject>();
		ActorObject->SetNumberField(TEXT("index"), Actor.Index);
		ActorObject->SetStringField(TEXT("id"), Actor.Id);
		ActorObject->SetStringField(TEXT("assetId"), Actor.AssetId);
		ActorObject->SetStringField(TEXT("actorCategory"), ToActorCategoryString(Actor.ActorCategory));
		ActorValues.Add(MakeShared<FJsonValueObject>(ActorObject));
	}

	Object->SetStringField(TEXT("type"), HeaderRecordType);
	Object->SetNumberField(TEXT("version"), Record.Version);
	Object->SetStringField(TEXT("logId"), Record.LogId);
	Object->SetStringField(TEXT("mapName"), Record.MapName);
	Object->SetStringField(TEXT("sourceJsonPath"), Record.SourceJsonPath);
	Object->SetStringField(TEXT("specHash"), Record.SpecHash);
	Object->SetObjectField(TEXT("units"), UnitsObject);
	Object->SetArrayField(TEXT("categories"), MakeStringArray(Record.Categories));
	Object->SetArrayField(TEXT("actors"), ActorValues);
	return Object;
}

TSharedRef<FJsonObject> FEpisodeMeasurementLogJson::MakeTickObject(
	const FEpisodeMeasurementLogTickRecord& Record)
{
	TArray<TSharedPtr<FJsonValue>> MovingActorValues;
	MovingActorValues.Reserve(Record.MovingActors.Num());
	for (const FEpisodeMeasurementLogActorState& MovingActor : Record.MovingActors)
	{
		MovingActorValues.Add(MakeShared<FJsonValueObject>(MakeActorStateObject(MovingActor, true)));
	}

	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("type"), TickRecordType);
	Object->SetNumberField(TEXT("tickIndex"), Record.TickIndex);
	Object->SetNumberField(TEXT("worldTimeSeconds"), Record.WorldTimeSeconds);
	Object->SetNumberField(TEXT("deltaSeconds"), Record.DeltaSeconds);
	Object->SetObjectField(TEXT("robot"), MakeRobotTickObject(Record.Robot));
	Object->SetArrayField(TEXT("movingActors"), MovingActorValues);
	return Object;
}

TSharedRef<FJsonObject> FEpisodeMeasurementLogJson::MakeEventObject(
	const FEpisodeMeasurementLogEventRecord& Record)
{
	TSharedRef<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FEpisodeParamValue>& Pair : Record.Properties)
	{
		PropertiesObject->SetField(Pair.Key, MakeParamJsonValue(Pair.Value));
	}

	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("type"), EventRecordType);
	Object->SetNumberField(TEXT("eventIndex"), Record.EventIndex);
	Object->SetNumberField(TEXT("worldTimeSeconds"), Record.WorldTimeSeconds);
	Object->SetStringField(TEXT("kind"), Record.Kind);
	Object->SetStringField(TEXT("severity"), ToSeverityString(Record.Severity));
	Object->SetStringField(TEXT("subjectId"), Record.SubjectId);
	Object->SetStringField(TEXT("targetId"), Record.TargetId);
	Object->SetArrayField(TEXT("location"), MakeVectorArray(Record.Location));
	Object->SetNumberField(TEXT("value"), Record.Value);
	Object->SetObjectField(TEXT("properties"), PropertiesObject);
	return Object;
}

TSharedRef<FJsonObject> FEpisodeMeasurementLogJson::MakeFooterObject(
	const FEpisodeMeasurementLogFooterRecord& Record)
{
	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	DiagnosticValues.Reserve(Record.Diagnostics.Num());
	for (const FEpisodeMeasurementLogDiagnostic& Diagnostic : Record.Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(MakeDiagnosticObject(Diagnostic)));
	}

	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("type"), FooterRecordType);
	Object->SetNumberField(TEXT("ticks"), Record.Ticks);
	Object->SetNumberField(TEXT("events"), Record.Events);
	Object->SetStringField(TEXT("closeReason"), Record.CloseReason);
	Object->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
	return Object;
}

bool FEpisodeMeasurementLogJson::TryWriteJsonLine(
	const TSharedRef<FJsonObject>& Object,
	FString& OutJsonLine)
{
	OutJsonLine.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonLine);
	return FJsonSerializer::Serialize(Object, Writer);
}
