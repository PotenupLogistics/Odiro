#include "Episode/EpisodeRunnerSubsystem.h"

#include "Episode/EpisodeCompiler.h"
#include "Episode/EpisodeEvaluationSubsystem.h"
#include "Episode/EpisodeSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeRunner, Log, All);

namespace
{
	const TCHAR* ToRunnerCompileSeverityString(EEpisodeCompileDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EEpisodeCompileDiagnosticSeverity::Info:
			return TEXT("정보");
		case EEpisodeCompileDiagnosticSeverity::Warning:
			return TEXT("경고");
		case EEpisodeCompileDiagnosticSeverity::Error:
			return TEXT("오류");
		default:
			return TEXT("알 수 없음");
		}
	}

	template <typename TEnum>
	FString ToRunnerEnumString(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}

	FEpisodeSimulationSetupSpec MakeSimulationSetupSpec(const FEpisodeWorldSpec& WorldSpec)
	{
		FEpisodeSimulationSetupSpec SetupSpec;
		SetupSpec.EpisodeId = WorldSpec.RunConfig.TemplateId;
		SetupSpec.SpecHash = WorldSpec.SpecHash;
		SetupSpec.Seeds = WorldSpec.Seeds;
		SetupSpec.GroundRegions = WorldSpec.GroundRegions;
		SetupSpec.Placeables = WorldSpec.Placeables;
		SetupSpec.DynamicActors = WorldSpec.DynamicActors;
		SetupSpec.Paths = WorldSpec.Paths;
		SetupSpec.Events = WorldSpec.Events;
		return SetupSpec;
	}
}

bool UEpisodeRunnerSubsystem::StartEpisodeFromJsonFile(const FString& JsonFilePath)
{
	if (JsonFilePath.IsEmpty())
	{
		return false;
	}

	TArray<FString> JsonFilePaths;
	JsonFilePaths.Add(JsonFilePath);
	return StartBatchFromJsonFiles(JsonFilePaths);
}

bool UEpisodeRunnerSubsystem::StartBatchFromJsonFiles(const TArray<FString>& JsonFilePaths)
{
	PendingJsonFilePaths.Reset();
	for (const FString& JsonFilePath : JsonFilePaths)
	{
		if (!JsonFilePath.IsEmpty())
		{
			PendingJsonFilePaths.Add(JsonFilePath);
		}
	}

	if (PendingJsonFilePaths.IsEmpty())
	{
		return false;
	}

	if (UEpisodeEvaluationSubsystem* EvaluationSubsystem = ResolveEvaluationSubsystem())
	{
		EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		EvaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* SimulationSubsystem = ResolveSimulationSubsystem())
	{
		SimulationSubsystem->ClearEpisode();
	}

	RunRecords.Reset();
	CurrentRunIndex = INDEX_NONE;
	RunnerState = EEpisodeRunnerState::Preparing;

	UE_LOG(LogEpisodeRunner, Log, TEXT("Episode batch started | Count: %d"), PendingJsonFilePaths.Num());

	StartNextEpisode();
	return true;
}

void UEpisodeRunnerSubsystem::CancelRun()
{
	PendingJsonFilePaths.Reset();
	RunnerState = EEpisodeRunnerState::Cancelled;

	if (UEpisodeEvaluationSubsystem* EvaluationSubsystem = ResolveEvaluationSubsystem())
	{
		EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		EvaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* SimulationSubsystem = ResolveSimulationSubsystem())
	{
		SimulationSubsystem->ClearEpisode();
	}

	UE_LOG(LogEpisodeRunner, Log, TEXT("Episode run cancelled."));
}

void UEpisodeRunnerSubsystem::HandleEpisodeEnded(FEpisodeEvaluationResult Result)
{
	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode ended callback | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Events: %d"),
		*Result.EpisodeId,
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		*ToRunnerEnumString(Result.Outcome),
		*ToRunnerEnumString(Result.TerminalReason),
		Result.DurationSeconds,
		Result.Events.Num());

	CompleteCurrentRecord(Result.bSuccess, Result.Outcome, Result.TerminalReason, &Result);

	if (UEpisodeEvaluationSubsystem* EvaluationSubsystem = ResolveEvaluationSubsystem())
	{
		EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		EvaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* SimulationSubsystem = ResolveSimulationSubsystem())
	{
		SimulationSubsystem->ClearEpisode();
	}

	RunnerState = EEpisodeRunnerState::Ending;
	QueueStartNextEpisode();
}

void UEpisodeRunnerSubsystem::StartNextEpisode()
{
	if (PendingJsonFilePaths.IsEmpty())
	{
		RunnerState = EEpisodeRunnerState::Completed;
		UE_LOG(LogEpisodeRunner, Log, TEXT("Episode batch completed | Records: %d"), RunRecords.Num());
		return;
	}

	UWorld* World = ResolveWorld();
	UEpisodeSimulationSubsystem* SimulationSubsystem = ResolveSimulationSubsystem();
	UEpisodeEvaluationSubsystem* EvaluationSubsystem = ResolveEvaluationSubsystem();
	if (!World || !SimulationSubsystem || !EvaluationSubsystem)
	{
		RunnerState = EEpisodeRunnerState::Failed;
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode runner failed to resolve subsystems | World: %s, Simulation: %s, Evaluation: %s"),
			World ? TEXT("valid") : TEXT("null"),
			SimulationSubsystem ? TEXT("valid") : TEXT("null"),
			EvaluationSubsystem ? TEXT("valid") : TEXT("null"));
		return;
	}

	CurrentJsonFilePath = PendingJsonFilePaths[0];
	PendingJsonFilePaths.RemoveAt(0);
	++CurrentRunIndex;

	CurrentRecord = FEpisodeRunRecord{};
	CurrentRecord.RunIndex = CurrentRunIndex;
	CurrentRecord.RunId = BuildRunId();
	CurrentRecord.SourceJsonPath = CurrentJsonFilePath;
	CurrentRecord.StartTimeSeconds = World->GetTimeSeconds();

	RunnerState = EEpisodeRunnerState::Preparing;

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode preparing | RunId: %s, Index: %d, Json: %s, Remaining: %d"),
		*CurrentRecord.RunId,
		CurrentRecord.RunIndex,
		*CurrentJsonFilePath,
		PendingJsonFilePaths.Num());

	UEpisodeCompiler* Compiler = NewObject<UEpisodeCompiler>(this);
	if (!Compiler)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode compiler creation failed | RunId: %s"), *CurrentRecord.RunId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompilerCreateFailed);
		QueueStartNextEpisode();
		return;
	}

	const FEpisodeCompileResult CompileResult = Compiler->CompileEpisodeWorldSpecFromJsonFile(CurrentJsonFilePath);
	CurrentRecord.bCompileSucceeded = CompileResult.bSuccess;
	CurrentRecord.EpisodeId = CompileResult.WorldSpec.RunConfig.TemplateId;
	CurrentRecord.SpecHash = CompileResult.WorldSpec.SpecHash;
	AppendCompileDiagnostics(CompileResult);

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode compiled | RunId: %s, Episode: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.EpisodeId,
		CompileResult.bSuccess ? TEXT("true") : TEXT("false"),
		CompileResult.Diagnostics.Num(),
		*CurrentRecord.SpecHash);

	if (!CompileResult.bSuccess)
	{
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompileFailed);
		QueueStartNextEpisode();
		return;
	}

	const FEpisodeSimulationSetupSpec SimulationSetupSpec = MakeSimulationSetupSpec(CompileResult.WorldSpec);

	SimulationSubsystem->ClearEpisode();
	const bool bSetupSucceeded = SimulationSubsystem->SetupEpisodeWorld(SimulationSetupSpec);
	CurrentRecord.bSetupSucceeded = bSetupSucceeded;

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode setup completed | RunId: %s, Episode: %s, Success: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.EpisodeId,
		bSetupSucceeded ? TEXT("true") : TEXT("false"));

	if (!bSetupSucceeded)
	{
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::SetupFailed);
		SimulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	const FEpisodeRuntimeContext RuntimeContext = SimulationSubsystem->BuildRuntimeContext(SimulationSetupSpec);
	const double TimeLimitSeconds = GetRunTimeLimitSeconds(CompileResult.WorldSpec.RunConfig);

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Evaluation handoff | RunId: %s, Episode: %s, TimeLimit: %.2fs, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
		*CurrentRecord.RunId,
		*RuntimeContext.EpisodeId,
		TimeLimitSeconds,
		*RuntimeContext.RobotInstanceId,
		RuntimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		RuntimeContext.RuntimeActors.Num(),
		RuntimeContext.GroundRegionActors.Num(),
		RuntimeContext.StaticObstacleActors.Num(),
		RuntimeContext.PedestrianActors.Num());

	EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
	EvaluationSubsystem->OnEpisodeEnded.AddDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);

	if (!EvaluationSubsystem->StartEvaluation(CompileResult.WorldSpec.EvaluationConfig, RuntimeContext, TimeLimitSeconds))
	{
		EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Evaluation start failed | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *RuntimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::EvaluationStartFailed);
		SimulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	RunnerState = EEpisodeRunnerState::Running;
	UE_LOG(LogEpisodeRunner, Log, TEXT("Episode running | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *RuntimeContext.EpisodeId);
}

void UEpisodeRunnerSubsystem::QueueStartNextEpisode()
{
	if (UWorld* World = ResolveWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UEpisodeRunnerSubsystem::StartNextEpisode));
		return;
	}

	StartNextEpisode();
}

void UEpisodeRunnerSubsystem::CompleteCurrentRecord(
	bool bSuccess,
	EEpisodeEvaluationOutcome Outcome,
	EEpisodeEvaluationTerminalReason TerminalReason,
	const FEpisodeEvaluationResult* EvaluationResult)
{
	if (UWorld* World = ResolveWorld())
	{
		CurrentRecord.EndTimeSeconds = World->GetTimeSeconds();
	}

	CurrentRecord.DurationSeconds = FMath::Max(0.0, CurrentRecord.EndTimeSeconds - CurrentRecord.StartTimeSeconds);
	CurrentRecord.bSuccess = bSuccess;
	CurrentRecord.Outcome = Outcome;
	CurrentRecord.TerminalReason = TerminalReason;

	if (EvaluationResult)
	{
		CurrentRecord.bEvaluationCompleted = EvaluationResult->bCompleted;
		CurrentRecord.EvaluationResult = *EvaluationResult;
		if (CurrentRecord.DurationSeconds <= 0.0)
		{
			CurrentRecord.DurationSeconds = EvaluationResult->DurationSeconds;
		}
	}

	RunRecords.Add(CurrentRecord);

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode record completed | RunId: %s, Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, EvaluationCompleted: %s, TotalRecords: %d"),
		*CurrentRecord.RunId,
		*CurrentRecord.EpisodeId,
		CurrentRecord.bSuccess ? TEXT("true") : TEXT("false"),
		*ToRunnerEnumString(CurrentRecord.Outcome),
		*ToRunnerEnumString(CurrentRecord.TerminalReason),
		CurrentRecord.DurationSeconds,
		CurrentRecord.bEvaluationCompleted ? TEXT("true") : TEXT("false"),
		RunRecords.Num());
}

void UEpisodeRunnerSubsystem::AppendCompileDiagnostics(const FEpisodeCompileResult& CompileResult)
{
	for (const FEpisodeCompileDiagnostic& Diagnostic : CompileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("%s [%s]: %s"),
			ToRunnerCompileSeverityString(Diagnostic.Severity),
			*Diagnostic.Code,
			*Diagnostic.Message));
	}
}

double UEpisodeRunnerSubsystem::GetRunTimeLimitSeconds(const FEpisodeRunConfig& RunConfig) const
{
	const FEpisodeParamValue* TimeLimitParam = RunConfig.Parameters.Find(TEXT("time_limit_s"));
	if (!TimeLimitParam)
	{
		return 0.0;
	}

	double TimeLimitSeconds = 0.0;
	if (TimeLimitParam->Type == EEpisodeParamValueType::Float)
	{
		TimeLimitSeconds = TimeLimitParam->FloatValue;
	}
	else if (TimeLimitParam->Type == EEpisodeParamValueType::Integer)
	{
		TimeLimitSeconds = static_cast<double>(TimeLimitParam->IntegerValue);
	}
	else
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Ignoring non-numeric run parameter 'time_limit_s' | Template: %s"), *RunConfig.TemplateId);
		return 0.0;
	}

	if (TimeLimitSeconds < 0.0)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Clamping negative run parameter 'time_limit_s' | Template: %s, Value: %.2f"), *RunConfig.TemplateId, TimeLimitSeconds);
	}

	return FMath::Max(0.0, TimeLimitSeconds);
}

FString UEpisodeRunnerSubsystem::BuildRunId() const
{
	return FString::Printf(TEXT("episode_run_%04d"), CurrentRunIndex);
}

UWorld* UEpisodeRunnerSubsystem::ResolveWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

UEpisodeSimulationSubsystem* UEpisodeRunnerSubsystem::ResolveSimulationSubsystem() const
{
	UWorld* World = ResolveWorld();
	return World ? World->GetSubsystem<UEpisodeSimulationSubsystem>() : nullptr;
}

UEpisodeEvaluationSubsystem* UEpisodeRunnerSubsystem::ResolveEvaluationSubsystem() const
{
	UWorld* World = ResolveWorld();
	return World ? World->GetSubsystem<UEpisodeEvaluationSubsystem>() : nullptr;
}
