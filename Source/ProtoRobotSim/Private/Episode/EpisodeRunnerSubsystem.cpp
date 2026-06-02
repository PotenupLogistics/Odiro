#include "Episode/EpisodeRunnerSubsystem.h"

#include "Episode/EpisodeCompiler.h"
#include "Episode/EpisodeEvaluationSubsystem.h"
#include "Episode/EpisodeSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

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
}

void UEpisodeRunnerSubsystem::HandleEpisodeEnded(FEpisodeEvaluationResult Result)
{
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
		return;
	}

	UWorld* World = ResolveWorld();
	UEpisodeSimulationSubsystem* SimulationSubsystem = ResolveSimulationSubsystem();
	UEpisodeEvaluationSubsystem* EvaluationSubsystem = ResolveEvaluationSubsystem();
	if (!World || !SimulationSubsystem || !EvaluationSubsystem)
	{
		RunnerState = EEpisodeRunnerState::Failed;
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

	UEpisodeCompiler* Compiler = NewObject<UEpisodeCompiler>(this);
	if (!Compiler)
	{
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

	if (!CompileResult.bSuccess)
	{
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompileFailed);
		QueueStartNextEpisode();
		return;
	}

	SimulationSubsystem->ClearEpisode();
	const bool bSetupSucceeded = SimulationSubsystem->SpawnEpisodeWorld(CompileResult.WorldSpec);
	CurrentRecord.bSetupSucceeded = bSetupSucceeded;
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

	const FEpisodeRuntimeContext RuntimeContext = SimulationSubsystem->BuildRuntimeContext(CompileResult.WorldSpec);
	EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
	EvaluationSubsystem->OnEpisodeEnded.AddDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);

	if (!EvaluationSubsystem->StartEvaluation(CompileResult.WorldSpec, RuntimeContext))
	{
		EvaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::EvaluationStartFailed);
		SimulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	RunnerState = EEpisodeRunnerState::Running;
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
