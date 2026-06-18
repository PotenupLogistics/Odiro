#include "Episode/EpisodeMeasurementLogSubsystem.h"

#include "DeliveryBot/Actor/DeliveryBot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Scenario/ScenarioEvaluationSubsystem.h"
#include "Episode/EpisodeRobotMeasurementAdapter.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Shared/EpisodeJsonlMeasurementWriter.h"
#include "Shared/EpisodeLogSubjectRegistry.h"
#include "Shared/UserProjectDataTypes.h"

namespace
{
	bool IsKnownMeasurementEventKind(const FString& Kind)
	{
		return Kind == TEXT("pedestrian_near_miss")
			|| Kind == TEXT("pedestrian_collision")
			|| Kind == TEXT("static_obstacle_collision")
			|| Kind == TEXT("blocked_region_collision")
			|| Kind == TEXT("penalty_region_violation")
			|| Kind == TEXT("robot_tip_over")
			|| Kind == TEXT("delivery_bot_simulation_failure")
			|| Kind == TEXT("collision")
			|| Kind == TEXT("emergency_stop")
			|| Kind == TEXT("timeout");
	}

	EEpisodeMeasurementLogSeverity ConvertEvaluationSeverity(EEpisodeEvaluationEventSeverity Severity)
	{
		switch (Severity)
		{
		case EEpisodeEvaluationEventSeverity::Info:
			return EEpisodeMeasurementLogSeverity::Info;
		case EEpisodeEvaluationEventSeverity::Warning:
			return EEpisodeMeasurementLogSeverity::Warning;
		case EEpisodeEvaluationEventSeverity::Failure:
			return EEpisodeMeasurementLogSeverity::Failure;
		default:
			return EEpisodeMeasurementLogSeverity::Warning;
		}
	}

	FString ConvertEvaluationEventType(EEpisodeEvaluationEventType EventType)
	{
		switch (EventType)
		{
		case EEpisodeEvaluationEventType::Timeout:
			return TEXT("timeout");
		case EEpisodeEvaluationEventType::RobotTipOver:
			return TEXT("robot_tip_over");
		case EEpisodeEvaluationEventType::StaticObstacleCollision:
			return TEXT("static_obstacle_collision");
		case EEpisodeEvaluationEventType::BlockedRegionCollision:
			return TEXT("blocked_region_collision");
		case EEpisodeEvaluationEventType::PenaltyRegionViolation:
			return TEXT("penalty_region_violation");
		case EEpisodeEvaluationEventType::PedestrianNearMiss:
			return TEXT("pedestrian_near_miss");
		case EEpisodeEvaluationEventType::PedestrianCollision:
			return TEXT("pedestrian_collision");
		case EEpisodeEvaluationEventType::DeliveryBotSimulationFailure:
			return TEXT("delivery_bot_simulation_failure");
		case EEpisodeEvaluationEventType::None:
		default:
			return TEXT("unknown");
		}
	}

	FScenarioParamValue MakeMeasurementStringParamValue(const FString& Value)
	{
		FScenarioParamValue ParamValue;
		ParamValue.Type = EScenarioParamValueType::String;
		ParamValue.StringValue = Value;
		return ParamValue;
	}
}

void UEpisodeMeasurementLogSubsystem::ApplySettings(
	const FEpisodeMeasurementLogSettings& settings,
	bool bRestartIfLogging)
{
	const bool bWasLogging = IsLogging();
	if (bWasLogging && bRestartIfLogging)
	{
		StopLogging(TEXT("settings_reconfigured"));
	}

	Settings = settings;
	if ((bWasLogging || bStartAttempted) && !IsLogging() && Settings.bEnabled)
	{
		StartLogging();
	}
}

bool UEpisodeMeasurementLogSubsystem::StartLogging()
{
	if (IsLogging())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_world"),
			TEXT("Measurement logging requires a valid world."));
		return false;
	}

	const double WorldTimeSeconds = World->GetTimeSeconds();
	bStartAttempted = true;
	bStopping = false;
	Diagnostics.Reset();
	ReportedInvalidMovingActorIndexes.Reset();
	ReportedUnknownEventKinds.Reset();
	NextTickIndex = 0;
	NextEventIndex = 0;
	AppendedRegistryDiagnosticCount = 0;
	bReportedMissingRobot = false;
	bHeaderWritten = false;

	if (!Settings.bEnabled)
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Info,
			TEXT("measurement_log_disabled"),
			TEXT("Measurement logging is disabled."),
			WorldTimeSeconds);
		return false;
	}

	SubjectRegistry = NewObject<UEpisodeLogSubjectRegistry>(this);
	if (!IsValid(SubjectRegistry))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("subject_registry_create_failed"),
			TEXT("Failed to create measurement log subject registry."),
			WorldTimeSeconds);
		return false;
	}

	if (!OpenWriter(WorldTimeSeconds))
	{
		return false;
	}

	BindEvaluationSubsystem();
	return true;
}

void UEpisodeMeasurementLogSubsystem::StopLogging(const FString& CloseReason)
{
	if (!Writer || bStopping)
	{
		return;
	}

	bStopping = true;
	UnbindEvaluationSubsystem();
	const UWorld* World = GetWorld();
	const double WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	if (!bHeaderWritten)
	{
		EnsureHeaderWritten(WorldTimeSeconds);
	}
	if (bHeaderWritten)
	{
		WriteFooter(CloseReason.IsEmpty() ? TEXT("unknown") : CloseReason);
	}
	Writer->Close();
	Writer.Reset();
	SubjectRegistry = nullptr;
	bHeaderWritten = false;
	bStopping = false;
}

bool UEpisodeMeasurementLogSubsystem::WriteEventRecord(const FEpisodeMeasurementLogEventRecord& EventRecord)
{
	if (!IsLogging())
	{
		return false;
	}

	FEpisodeMeasurementLogEventRecord Record = EventRecord;
	if (!EnsureHeaderWritten(Record.WorldTimeSeconds))
	{
		return false;
	}

	Record.EventIndex = NextEventIndex;
	if (!Record.Kind.IsEmpty() && !IsKnownMeasurementEventKind(Record.Kind) && !ReportedUnknownEventKinds.Contains(Record.Kind))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("unknown_event_kind"),
			FString::Printf(TEXT("Unknown measurement event kind '%s' was written."), *Record.Kind),
			Record.WorldTimeSeconds);
		ReportedUnknownEventKinds.Add(Record.Kind);
	}

	const bool bWrote = Writer->WriteEvent(Record, Diagnostics);
	if (bWrote)
	{
		++NextEventIndex;
		if (Settings.bFlushOnEvent)
		{
			Writer->Flush();
		}
	}

	return bWrote;
}

bool UEpisodeMeasurementLogSubsystem::RecordCollisionEvent(
	const FString& SubjectId,
	const FString& TargetId,
	FVector Location,
	double Value)
{
	const UWorld* World = GetWorld();

	FEpisodeMeasurementLogEventRecord EventRecord;
	EventRecord.WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	EventRecord.Kind = TEXT("collision");
	EventRecord.Severity = EEpisodeMeasurementLogSeverity::Failure;
	EventRecord.SubjectId = SubjectId;
	EventRecord.TargetId = TargetId;
	EventRecord.Location = Location;
	EventRecord.Value = Value;
	return WriteEventRecord(EventRecord);
}

bool UEpisodeMeasurementLogSubsystem::RecordEmergencyStopEvent(
	const FString& SubjectId,
	const FString& TargetId,
	FVector Location,
	double Value,
	const FString& Reason)
{
	const UWorld* World = GetWorld();

	FEpisodeMeasurementLogEventRecord EventRecord;
	EventRecord.WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	EventRecord.Kind = TEXT("emergency_stop");
	EventRecord.Severity = EEpisodeMeasurementLogSeverity::Warning;
	EventRecord.SubjectId = SubjectId;
	EventRecord.TargetId = TargetId;
	EventRecord.Location = Location;
	EventRecord.Value = Value;
	if (!Reason.IsEmpty())
	{
		EventRecord.Properties.Add(TEXT("reason"), MakeMeasurementStringParamValue(Reason));
	}

	return WriteEventRecord(EventRecord);
}

bool UEpisodeMeasurementLogSubsystem::IsLogging() const
{
	return Writer && Writer->IsOpen();
}

FString UEpisodeMeasurementLogSubsystem::GetCurrentLogPath() const
{
	return CurrentLogPath;
}

const TArray<FEpisodeMeasurementLogDiagnostic>& UEpisodeMeasurementLogSubsystem::GetDiagnostics() const
{
	return Diagnostics;
}

void UEpisodeMeasurementLogSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (Settings.bEnabled)
	{
		StartLogging();
	}
}

void UEpisodeMeasurementLogSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	StopLogging(TEXT("world_end_play"));
	StopProjectTraceLogging();
	Super::OnWorldEndPlay(InWorld);
}

void UEpisodeMeasurementLogSubsystem::Deinitialize()
{
	StopLogging(TEXT("deinitialize"));
	StopProjectTraceLogging();
	Super::Deinitialize();
}

bool UEpisodeMeasurementLogSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UEpisodeMeasurementLogSubsystem::Tick(float DeltaTime)
{
	if (IsProjectTraceLogging())
	{
		WriteProjectTraceTick(DeltaTime);
		return;
	}

	if (!IsLogging())
	{
		return;
	}

	WriteTick(DeltaTime);
}

bool UEpisodeMeasurementLogSubsystem::IsTickable() const
{
	return IsLogging() || IsProjectTraceLogging();
}

TStatId UEpisodeMeasurementLogSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEpisodeMeasurementLogSubsystem, STATGROUP_Tickables);
}

void UEpisodeMeasurementLogSubsystem::HandleEvaluationEvent(FEpisodeEvaluationEvent Event)
{
	FEpisodeMeasurementLogEventRecord EventRecord;
	EventRecord.WorldTimeSeconds = Event.WorldTimeSeconds >= 0.0
		? Event.WorldTimeSeconds
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	EventRecord.Kind = ConvertEvaluationEventType(Event.EventType);
	EventRecord.Severity = ConvertEvaluationSeverity(Event.Severity);
	EventRecord.SubjectId = Event.SubjectInstanceId;
	EventRecord.TargetId = Event.TargetInstanceId;
	EventRecord.Location = Event.Location;
	EventRecord.Value = Event.Value;
	EventRecord.Properties = Event.Properties;
	if (!Event.Message.IsEmpty())
	{
		EventRecord.Properties.Add(TEXT("message"), MakeMeasurementStringParamValue(Event.Message));
	}

	WriteEventRecord(EventRecord);
}

bool UEpisodeMeasurementLogSubsystem::OpenWriter(double WorldTimeSeconds)
{
	Writer = MakeUnique<FEpisodeJsonlMeasurementWriter>();
	CurrentLogPath = BuildOutputPath();
	return Writer->Open(CurrentLogPath, Diagnostics, WorldTimeSeconds);
}

bool UEpisodeMeasurementLogSubsystem::StartProjectTraceLogging(const FString& TraceJsonlPath)
{
	if (TraceJsonlPath.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("empty_project_trace_path"),
			TEXT("Project trace path must not be empty."));
		return false;
	}

	if (IsLogging())
	{
		StopLogging(TEXT("project_trace_started"));
	}

	StopProjectTraceLogging();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_world"),
			TEXT("Project trace logging requires a valid world."));
		return false;
	}

	const FString TraceDirectory = FPaths::GetPath(TraceJsonlPath);
	if (!IFileManager::Get().MakeDirectory(*TraceDirectory, true))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("project_trace_directory_create_failed"),
			FString::Printf(TEXT("Project trace directory create failed: %s"), *TraceDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(FString(), *TraceJsonlPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("project_trace_file_create_failed"),
			FString::Printf(TEXT("Project trace file create failed: %s"), *TraceJsonlPath));
		return false;
	}

	Diagnostics.Reset();
	ReportedInvalidMovingActorIndexes.Reset();
	ReportedUnknownEventKinds.Reset();
	AppendedRegistryDiagnosticCount = 0;
	bReportedMissingRobot = false;
	NextProjectTraceSampleIndex = 0;
	CurrentProjectTracePath = TraceJsonlPath;
	SubjectRegistry = NewObject<UEpisodeLogSubjectRegistry>(this);
	if (!IsValid(SubjectRegistry))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("subject_registry_create_failed"),
			TEXT("Failed to create project trace subject registry."));
		CurrentProjectTracePath.Reset();
		return false;
	}

	SubjectRegistry->BuildFromWorld(World, World->GetTimeSeconds());
	AppendRegistryDiagnostics();
	bProjectTraceLogging = true;
	return true;
}

void UEpisodeMeasurementLogSubsystem::StopProjectTraceLogging()
{
	if (!bProjectTraceLogging)
	{
		return;
	}

	bProjectTraceLogging = false;
	CurrentProjectTracePath.Reset();
	NextProjectTraceSampleIndex = 0;
	SubjectRegistry = nullptr;
}

bool UEpisodeMeasurementLogSubsystem::EnsureHeaderWritten(double WorldTimeSeconds)
{
	if (bHeaderWritten)
	{
		return true;
	}

	if (!Writer || !SubjectRegistry)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Error,
			TEXT("missing_world"),
			TEXT("Measurement log header requires a valid world."),
			WorldTimeSeconds);
		return false;
	}

	SubjectRegistry->BuildFromWorld(World, WorldTimeSeconds);
	AppendRegistryDiagnostics();
	bHeaderWritten = WriteHeader(WorldTimeSeconds);
	return bHeaderWritten;
}

bool UEpisodeMeasurementLogSubsystem::WriteHeader(double WorldTimeSeconds)
{
	if (!Writer)
	{
		return false;
	}

	return Writer->WriteHeader(BuildHeaderRecord(WorldTimeSeconds), Diagnostics);
}

bool UEpisodeMeasurementLogSubsystem::WriteTick(float DeltaTime)
{
	if (!Writer)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	if (!EnsureHeaderWritten(WorldTimeSeconds))
	{
		return false;
	}

	const FEpisodeMeasurementLogTickRecord TickRecord = BuildTickRecord(DeltaTime);
	const bool bWrote = Writer->WriteTick(TickRecord, Diagnostics);
	if (bWrote)
	{
		++NextTickIndex;
		if (Settings.FlushIntervalTicks > 0 && NextTickIndex % Settings.FlushIntervalTicks == 0)
		{
			Writer->Flush();
		}
	}

	return bWrote;
}

bool UEpisodeMeasurementLogSubsystem::WriteProjectTraceTick(float DeltaTime)
{
	if (!bProjectTraceLogging || CurrentProjectTracePath.IsEmpty())
	{
		return false;
	}

	const FEpisodeMeasurementLogTickRecord TickRecord = BuildTickRecord(DeltaTime);
	TArray<FString> TraceDiagnostics;
	const bool bWrote = FUserProjectRunOutputJson::AppendEpisodeTraceRecordToFile(
		CurrentProjectTracePath,
		TickRecord,
		NextProjectTraceSampleIndex,
		TraceDiagnostics);

	for (const FString& Diagnostic : TraceDiagnostics)
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("project_trace_write_warning"),
			Diagnostic,
			TickRecord.WorldTimeSeconds);
	}

	if (bWrote)
	{
		++NextProjectTraceSampleIndex;
	}

	return bWrote;
}

bool UEpisodeMeasurementLogSubsystem::WriteFooter(const FString& CloseReason)
{
	if (!Writer)
	{
		return false;
	}

	FEpisodeMeasurementLogFooterRecord Footer;
	Footer.Ticks = NextTickIndex;
	Footer.Events = NextEventIndex;
	Footer.CloseReason = CloseReason;
	Footer.Diagnostics = Diagnostics;
	const bool bWrote = Writer->WriteFooter(Footer, Diagnostics);
	Writer->Flush();
	return bWrote;
}

FEpisodeMeasurementLogTickRecord UEpisodeMeasurementLogSubsystem::BuildTickRecord(float DeltaTime)
{
	const UWorld* World = GetWorld();

	FEpisodeMeasurementLogTickRecord TickRecord;
	TickRecord.TickIndex = NextTickIndex;
	TickRecord.WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	TickRecord.DeltaSeconds = FMath::Max(0.0f, DeltaTime);
	TickRecord.Robot.Truth.RotationQuatXyzw = { 0.0, 0.0, 0.0, 1.0 };

	ADeliveryBot* RobotActor = FindRobotActor();
	if (RobotActor)
	{
		FEpisodeRobotMeasurementAdapter::BuildRobotTick(
			RobotActor,
			SubjectRegistry,
			TickRecord.Robot,
			Diagnostics,
			TickRecord.WorldTimeSeconds);
	}
	else if (!bReportedMissingRobot)
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Warning,
			TEXT("missing_robot_actor"),
			TEXT("No Delivery Bot actor was found for measurement logging."),
			TickRecord.WorldTimeSeconds);
		bReportedMissingRobot = true;
	}

	CaptureMovingActors(TickRecord, RobotActor);
	return TickRecord;
}

FEpisodeMeasurementLogHeaderRecord UEpisodeMeasurementLogSubsystem::BuildHeaderRecord(double WorldTimeSeconds) const
{
	FEpisodeMeasurementLogHeaderRecord Header;
	Header.Version = Settings.Version;
	Header.LogId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	Header.MapName = GetCleanMapName();

	if (SubjectRegistry)
	{
		Header.Actors = SubjectRegistry->GetActorTable();
	}

	return Header;
}

ADeliveryBot* UEpisodeMeasurementLogSubsystem::FindRobotActor() const
{
	if (SubjectRegistry)
	{
		for (const FEpisodeMeasurementLogActorInfo& ActorInfo : SubjectRegistry->GetActorTable())
		{
			if (ActorInfo.ActorCategory != EScenarioActorCategory::DeliveryBot)
			{
				continue;
			}

			if (ADeliveryBot* RobotActor = Cast<ADeliveryBot>(SubjectRegistry->GetActorByIndex(ActorInfo.Index)))
			{
				return RobotActor;
			}
		}
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<ADeliveryBot> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		if (ADeliveryBot* RobotActor = *ActorIterator)
		{
			if (IsValid(RobotActor))
			{
				return RobotActor;
			}
		}
	}

	return nullptr;
}

void UEpisodeMeasurementLogSubsystem::CaptureMovingActors(
	FEpisodeMeasurementLogTickRecord& TickRecord,
	AActor* RobotActor)
{
	if (!SubjectRegistry)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		SubjectRegistry->DetectNewSubjectsInWorld(World, TickRecord.WorldTimeSeconds);
		AppendRegistryDiagnostics();
	}

	const TArray<FEpisodeMeasurementLogActorInfo>& ActorTable = SubjectRegistry->GetActorTable();
	for (const FEpisodeMeasurementLogActorInfo& ActorInfo : ActorTable)
	{
		AActor* Actor = SubjectRegistry->GetActorByIndex(ActorInfo.Index);
		if (!IsValid(Actor))
		{
			if (!ReportedInvalidMovingActorIndexes.Contains(ActorInfo.Index))
			{
				AddDiagnostic(
					EEpisodeMeasurementLogSeverity::Warning,
					TEXT("invalid_moving_actor"),
					FString::Printf(
						TEXT("Moving actor '%s' at index %d is invalid and was skipped."),
						*ActorInfo.Id,
						ActorInfo.Index),
					TickRecord.WorldTimeSeconds);
				ReportedInvalidMovingActorIndexes.Add(ActorInfo.Index);
			}
			continue;
		}

		if (Actor == RobotActor || ActorInfo.ActorCategory == EScenarioActorCategory::DeliveryBot)
		{
			continue;
		}

		const FQuat Rotation = Actor->GetActorQuat();
		FEpisodeMeasurementLogActorState ActorState;
		ActorState.ActorIndex = ActorInfo.Index;
		ActorState.PositionCm = Actor->GetActorLocation();
		ActorState.RotationQuatXyzw = { Rotation.X, Rotation.Y, Rotation.Z, Rotation.W };
		ActorState.VelocityCmPerSecond = Actor->GetVelocity();
		TickRecord.MovingActors.Add(ActorState);
	}
}

FString UEpisodeMeasurementLogSubsystem::BuildOutputPath() const
{
	const FString Directory = Settings.OutputDirectory.IsEmpty()
		? FString(TEXT("Saved/AnalysisLogs"))
		: Settings.OutputDirectory;
	const FString AbsoluteDirectory = FPaths::IsRelative(Directory)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Directory))
		: Directory;

	const FString FileName = FEpisodeMeasurementLogJson::MakeDefaultFileName(Settings, GetCleanMapName());
	return FPaths::Combine(AbsoluteDirectory, FileName);
}

FString UEpisodeMeasurementLogSubsystem::GetCleanMapName() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return TEXT("UnknownMap");
	}

	FString MapName = World->GetMapName();
	if (!World->StreamingLevelsPrefix.IsEmpty())
	{
		MapName.RemoveFromStart(World->StreamingLevelsPrefix);
	}

	return MapName.IsEmpty() ? FString(TEXT("UnknownMap")) : MapName;
}

void UEpisodeMeasurementLogSubsystem::AddDiagnostic(
	EEpisodeMeasurementLogSeverity Severity,
	const FString& Code,
	const FString& Message,
	double WorldTimeSeconds)
{
	Diagnostics.Add(FEpisodeMeasurementLogJson::MakeDiagnostic(
		Severity,
		Code,
		Message,
		WorldTimeSeconds));
}

void UEpisodeMeasurementLogSubsystem::AppendRegistryDiagnostics()
{
	if (!SubjectRegistry)
	{
		return;
	}

	const TArray<FEpisodeMeasurementLogDiagnostic>& RegistryDiagnostics = SubjectRegistry->GetDiagnostics();
	for (int32 Index = AppendedRegistryDiagnosticCount; Index < RegistryDiagnostics.Num(); ++Index)
	{
		Diagnostics.Add(RegistryDiagnostics[Index]);
	}

	AppendedRegistryDiagnosticCount = RegistryDiagnostics.Num();
}

void UEpisodeMeasurementLogSubsystem::BindEvaluationSubsystem()
{
	UWorld* World = GetWorld();
	UScenarioEvaluationSubsystem* EvaluationSubsystem = World
		? World->GetSubsystem<UScenarioEvaluationSubsystem>()
		: nullptr;
	if (!EvaluationSubsystem)
	{
		AddDiagnostic(
			EEpisodeMeasurementLogSeverity::Info,
			TEXT("evaluation_subsystem_not_found"),
			TEXT("No evaluation subsystem was available for measurement event bridging."),
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
		return;
	}

	EvaluationSubsystem->OnEvaluationEvent.RemoveDynamic(this, &UEpisodeMeasurementLogSubsystem::HandleEvaluationEvent);
	EvaluationSubsystem->OnEvaluationEvent.AddDynamic(this, &UEpisodeMeasurementLogSubsystem::HandleEvaluationEvent);
	BoundEvaluationSubsystem = EvaluationSubsystem;
}

void UEpisodeMeasurementLogSubsystem::UnbindEvaluationSubsystem()
{
	if (BoundEvaluationSubsystem)
	{
		BoundEvaluationSubsystem->OnEvaluationEvent.RemoveDynamic(this, &UEpisodeMeasurementLogSubsystem::HandleEvaluationEvent);
		BoundEvaluationSubsystem = nullptr;
	}
}
