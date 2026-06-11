#pragma once

#include "CoreMinimal.h"
#include "EpisodeCoreTypes.h"
#include "EpisodeMeasurementLogTypes.generated.h"

class FJsonObject;

/// Severity level for measurement log diagnostics.
UENUM(BlueprintType)
enum class EEpisodeMeasurementLogSeverity : uint8
{
	Info,
	Warning,
	Error,
	Failure
};

/// Structured diagnostic emitted by the measurement logger.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogDiagnostic
{
	GENERATED_BODY()

	/// Importance of the diagnostic.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	EEpisodeMeasurementLogSeverity Severity = EEpisodeMeasurementLogSeverity::Info;

	/// Stable machine-readable diagnostic code.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Code;

	/// Human-readable diagnostic detail.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Message;

	/// World time when the diagnostic was observed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double WorldTimeSeconds = 0.0;
};

/// Settings for PIE tick-based measurement logging.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogSettings
{
	GENERATED_BODY()

	/// Enables measurement log creation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	bool bEnabled = true;

	/// Project-relative directory; writers must validate before file I/O.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString OutputDirectory = TEXT("Saved/AnalysisLogs");

	/// File name prefix used before timestamp and map name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString FilePrefix = TEXT("MeasurementLog");

	/// Measurement log contract version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog", meta = (ClampMin = "1"))
	int32 Version = 1;

	/// Number of tick records between periodic flushes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog", meta = (ClampMin = "1"))
	int32 FlushIntervalTicks = 60;

	/// Flushes the log immediately after an event record.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	bool bFlushOnEvent = true;
};

/// Unit labels used by numeric fields in the log.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogUnits
{
	GENERATED_BODY()

	/// Position unit label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Position = TEXT("cm");

	/// Velocity unit label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Velocity = TEXT("cm/s");

	/// Rotation encoding label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Rotation = TEXT("quat_xyzw");

	/// Coordinate-axis convention label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Axes = TEXT("UE_XForward_YRight_ZUp");
};

/// Actor metadata stored once in the header.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogActorInfo
{
	GENERATED_BODY()

	/// Dense actor table index.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 Index = INDEX_NONE;

	/// Stable actor identifier used by log records.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Id;

	/// Source asset or setup identifier for the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString AssetId;

	/// Semantic actor category.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	EEpisodeActorCategory ActorCategory = EEpisodeActorCategory::StaticObstacle;
};

/// Per-sample transform and velocity for an actor.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogActorState
{
	GENERATED_BODY()

	FEpisodeMeasurementLogActorState()
	{
		RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };
	}

	/// Actor table index for non-robot state samples.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 ActorIndex = INDEX_NONE;

	/// Actor world position in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FVector PositionCm = FVector::ZeroVector;

	/// Actor world rotation as quaternion x, y, z, w.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	TArray<double> RotationQuatXyzw;

	/// Actor world velocity in centimeters per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FVector VelocityCmPerSecond = FVector::ZeroVector;
};

/// Front-object lidar summary captured for the robot.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogLidarSnapshot
{
	GENERATED_BODY()

	/// Whether the front lidar query hit an object.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	bool bHasFrontObject = false;

	/// Identifier of the closest front object, when present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString FrontObjectId;

	/// Distance to the closest front object in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double FrontDistanceM = 0.0;

	/// Yaw angle to the closest front object in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double FrontYawDegree = 0.0;

	/// Number of lidar hits in the snapshot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 HitCount = 0;
};

/// Robot command state recorded with each sample.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogRobotAction
{
	GENERATED_BODY()

	/// Requested robot speed in kilometers per hour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double TargetSpeedKmh = 0.0;

	/// Steering input value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double Steering = 0.0;

	/// Brake input value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double Brake = 0.0;

	/// Whether braking was applied for this sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	bool bBrakeApplied = false;

	/// Short reason for the selected action.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Reason = TEXT("unknown");
};

/// Robot-specific measurement payload for a tick record.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogRobotTick
{
	GENERATED_BODY()

	/// Stable robot identifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Id;

	/// Ground-truth robot state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FEpisodeMeasurementLogActorState Truth;

	/// Robot perception snapshot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FEpisodeMeasurementLogLidarSnapshot Lidar;

	/// Robot action snapshot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FEpisodeMeasurementLogRobotAction Action;
};

/// First JSONL record describing the log session.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogHeaderRecord
{
	GENERATED_BODY()

	FEpisodeMeasurementLogHeaderRecord()
	{
		Categories = {
			TEXT("perception"),
			TEXT("action"),
			TEXT("truth"),
			TEXT("event"),
			TEXT("diagnostic")
		};
	}

	/// Measurement log contract version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 Version = 1;

	/// Unique identifier for this log file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString LogId;

	/// Map name where logging was captured.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString MapName;

	/// Source episode setup JSON path, when available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString SourceJsonPath;

	/// Hash of the source setup, when available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString SpecHash;

	/// Unit metadata for numeric samples.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FEpisodeMeasurementLogUnits Units;

	/// Measurement categories included in this log.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	TArray<FString> Categories;

	/// Actor table referenced by tick records.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	TArray<FEpisodeMeasurementLogActorInfo> Actors;
};

/// JSONL record for one captured measurement tick.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogTickRecord
{
	GENERATED_BODY()

	/// Sequential measurement tick index.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 TickIndex = INDEX_NONE;

	/// World time at capture in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double WorldTimeSeconds = 0.0;

	/// Frame delta at capture in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double DeltaSeconds = 0.0;

	/// Robot payload for this sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FEpisodeMeasurementLogRobotTick Robot;

	/// Moving actor states captured with this sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	TArray<FEpisodeMeasurementLogActorState> MovingActors;
};

/// JSONL record for a notable event observed during logging.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogEventRecord
{
	GENERATED_BODY()

	/// Sequential event index.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 EventIndex = INDEX_NONE;

	/// World time when the event was observed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double WorldTimeSeconds = 0.0;

	/// Stable event kind string.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString Kind;

	/// Event severity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	EEpisodeMeasurementLogSeverity Severity = EEpisodeMeasurementLogSeverity::Info;

	/// Primary actor or object related to the event.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString SubjectId;

	/// Secondary actor or object related to the event.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString TargetId;

	/// Event world location in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FVector Location = FVector::ZeroVector;

	/// Optional numeric event value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	double Value = 0.0;

	/// Additional event-specific properties.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	TMap<FString, FEpisodeParamValue> Properties;
};

/// Final JSONL record summarizing the log session.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeMeasurementLogFooterRecord
{
	GENERATED_BODY()

	/// Number of tick records written.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 Ticks = 0;

	/// Number of event records written.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	int32 Events = 0;

	/// Reason the log was closed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	FString CloseReason = TEXT("unknown");

	/// Diagnostics collected during logging.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|MeasurementLog")
	TArray<FEpisodeMeasurementLogDiagnostic> Diagnostics;
};

/// Utility functions for validating and writing measurement log JSONL records.
struct PROTOROBOTSIM_API FEpisodeMeasurementLogJson
{
	static FString MakeDefaultFileName(
		const FEpisodeMeasurementLogSettings& Settings,
		const FString& MapName,
		const FDateTime& Timestamp = FDateTime::Now());

	static FString SanitizeFileToken(const FString& Value);
	static FString ToSeverityString(EEpisodeMeasurementLogSeverity Severity);
	static FString ToActorCategoryString(EEpisodeActorCategory Category);

	static FEpisodeMeasurementLogDiagnostic MakeDiagnostic(
		EEpisodeMeasurementLogSeverity Severity,
		const FString& Code,
		const FString& Message,
		double WorldTimeSeconds = 0.0);

	static void ValidateHeaderRecord(
		const FEpisodeMeasurementLogHeaderRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static void ValidateTickRecord(
		const FEpisodeMeasurementLogTickRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static void ValidateEventRecord(
		const FEpisodeMeasurementLogEventRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static void ValidateFooterRecord(
		const FEpisodeMeasurementLogFooterRecord& Record,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static bool HasError(const TArray<FEpisodeMeasurementLogDiagnostic>& Diagnostics);

	static bool TryWriteHeaderLine(
		const FEpisodeMeasurementLogHeaderRecord& Record,
		FString& OutJsonLine,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static bool TryWriteTickLine(
		const FEpisodeMeasurementLogTickRecord& Record,
		FString& OutJsonLine,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static bool TryWriteEventLine(
		const FEpisodeMeasurementLogEventRecord& Record,
		FString& OutJsonLine,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

	static bool TryWriteFooterLine(
		const FEpisodeMeasurementLogFooterRecord& Record,
		FString& OutJsonLine,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics);

private:
	static TSharedRef<FJsonObject> MakeHeaderObject(const FEpisodeMeasurementLogHeaderRecord& Record);
	static TSharedRef<FJsonObject> MakeTickObject(const FEpisodeMeasurementLogTickRecord& Record);
	static TSharedRef<FJsonObject> MakeEventObject(const FEpisodeMeasurementLogEventRecord& Record);
	static TSharedRef<FJsonObject> MakeFooterObject(const FEpisodeMeasurementLogFooterRecord& Record);
	static bool TryWriteJsonLine(const TSharedRef<FJsonObject>& Object, FString& OutJsonLine);
};
