#include "Episode/EpisodeRunnerSubsystem.h"

#include "Episode/EpisodeCompiler.h"
#include "Episode/EpisodeEvaluationSubsystem.h"
#include "Episode/EpisodeSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeRunner, Log, All);

namespace
{
	const TCHAR* ToRunnerCompileSeverityString(EEpisodeCompileDiagnosticSeverity severity)
	{
		switch (severity)
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
	FString ToRunnerEnumString(TEnum value)
	{
		if (const UEnum* enumValue = StaticEnum<TEnum>()) return enumValue->GetNameStringByValue(static_cast<int64>(value));

		return TEXT("Unknown");
	}

	FEpisodeSimulationSetupSpec MakeSimulationSetupSpec(const FEpisodeWorldSpec& worldSpec)
	{
		FEpisodeSimulationSetupSpec setupSpec;
		setupSpec.EpisodeId = worldSpec.RunConfig.TemplateId;
		setupSpec.SpecHash = worldSpec.SpecHash;
		setupSpec.Seeds = worldSpec.Seeds;
		setupSpec.GroundRegions = worldSpec.GroundRegions;
		setupSpec.Placeables = worldSpec.Placeables;
		setupSpec.DynamicActors = worldSpec.DynamicActors;
		setupSpec.Paths = worldSpec.Paths;
		setupSpec.Events = worldSpec.Events;
		return setupSpec;
	}
}

bool UEpisodeRunnerSubsystem::StartEpisodeFromJsonFile(const FString& jsonFilePath)
{
	if (jsonFilePath.IsEmpty()) return false;

	TArray<FString> jsonFilePaths;
	jsonFilePaths.Add(jsonFilePath);
	return StartBatchFromJsonFiles(jsonFilePaths);
}

bool UEpisodeRunnerSubsystem::StartBatchFromJsonFiles(const TArray<FString>& jsonFilePaths)
{
	PendingJsonFilePaths.Reset();
	for (const FString& jsonFilePath : jsonFilePaths)
	{
		if (!jsonFilePath.IsEmpty())
		{
			PendingJsonFilePaths.Add(jsonFilePath);
		}
	}

	if (PendingJsonFilePaths.IsEmpty()) return false;

	if (UEpisodeEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearEpisode();
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

	if (UEpisodeEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearEpisode();
	}

	UE_LOG(LogEpisodeRunner, Log, TEXT("Episode run cancelled."));
}

void UEpisodeRunnerSubsystem::HandleEpisodeEnded(FEpisodeEvaluationResult result)
{
	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode ended callback | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Events: %d"),
		*result.EpisodeId,
		result.bSuccess ? TEXT("true") : TEXT("false"),
		*ToRunnerEnumString(result.Outcome),
		*ToRunnerEnumString(result.TerminalReason),
		result.DurationSeconds,
		result.Events.Num());

	CompleteCurrentRecord(result.bSuccess, result.Outcome, result.TerminalReason, &result);

	if (UEpisodeEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearEpisode();
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

	UWorld* world = ResolveWorld();
	UEpisodeSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem();
	UEpisodeEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem();
	if (!world || !simulationSubsystem || !evaluationSubsystem)
	{
		RunnerState = EEpisodeRunnerState::Failed;
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode runner failed to resolve subsystems | World: %s, Simulation: %s, Evaluation: %s"),
			world ? TEXT("valid") : TEXT("null"),
			simulationSubsystem ? TEXT("valid") : TEXT("null"),
			evaluationSubsystem ? TEXT("valid") : TEXT("null"));
		return;
	}

	CurrentJsonFilePath = PendingJsonFilePaths[0];
	PendingJsonFilePaths.RemoveAt(0);
	++CurrentRunIndex;

	CurrentRecord = FEpisodeRunRecord{};
	CurrentRecord.RunIndex = CurrentRunIndex;
	CurrentRecord.RunId = BuildRunId();
	CurrentRecord.SourceJsonPath = CurrentJsonFilePath;
	CurrentRecord.StartTimeSeconds = world->GetTimeSeconds();

	RunnerState = EEpisodeRunnerState::Preparing;

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode preparing | RunId: %s, Index: %d, Json: %s, Remaining: %d"),
		*CurrentRecord.RunId,
		CurrentRecord.RunIndex,
		*CurrentJsonFilePath,
		PendingJsonFilePaths.Num());

	UEpisodeCompiler* compiler = NewObject<UEpisodeCompiler>(this);
	if (!compiler)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode compiler creation failed | RunId: %s"), *CurrentRecord.RunId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompilerCreateFailed);
		QueueStartNextEpisode();
		return;
	}

	const FEpisodeCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonFile(CurrentJsonFilePath);
	CurrentRecord.bCompileSucceeded = compileResult.bSuccess;
	CurrentRecord.EpisodeId = compileResult.WorldSpec.RunConfig.TemplateId;
	CurrentRecord.SpecHash = compileResult.WorldSpec.SpecHash;
	AppendCompileDiagnostics(compileResult);

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode compiled | RunId: %s, Episode: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.EpisodeId,
		compileResult.bSuccess ? TEXT("true") : TEXT("false"),
		compileResult.Diagnostics.Num(),
		*CurrentRecord.SpecHash);

	if (!compileResult.bSuccess)
	{
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompileFailed);
		QueueStartNextEpisode();
		return;
	}

	const FEpisodeSimulationSetupSpec simulationSetupSpec = MakeSimulationSetupSpec(compileResult.WorldSpec);

	simulationSubsystem->ClearEpisode();
	const bool bSetupSucceeded = simulationSubsystem->SetupEpisodeWorld(simulationSetupSpec);
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
		simulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	const FEpisodeRuntimeContext runtimeContext = simulationSubsystem->BuildRuntimeContext(simulationSetupSpec);
	const double timeLimitSeconds = GetRunTimeLimitSeconds(compileResult.WorldSpec.RunConfig);

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Evaluation handoff | RunId: %s, Episode: %s, TimeLimit: %.2fs, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
		*CurrentRecord.RunId,
		*runtimeContext.EpisodeId,
		timeLimitSeconds,
		*runtimeContext.RobotInstanceId,
		runtimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		runtimeContext.RuntimeActors.Num(),
		runtimeContext.GroundRegionActors.Num(),
		runtimeContext.StaticObstacleActors.Num(),
		runtimeContext.PedestrianActors.Num());

	evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
	evaluationSubsystem->OnEpisodeEnded.AddDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);

	if (!evaluationSubsystem->StartEvaluation(compileResult.WorldSpec.EvaluationConfig, runtimeContext, timeLimitSeconds))
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Evaluation start failed | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::EvaluationStartFailed);
		simulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	RunnerState = EEpisodeRunnerState::Running;
	UE_LOG(LogEpisodeRunner, Log, TEXT("Episode running | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
}

void UEpisodeRunnerSubsystem::QueueStartNextEpisode()
{
	if (UWorld* world = ResolveWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UEpisodeRunnerSubsystem::StartNextEpisode));
		return;
	}

	StartNextEpisode();
}

void UEpisodeRunnerSubsystem::CompleteCurrentRecord(
	bool bSuccess,
	EEpisodeEvaluationOutcome outcome,
	EEpisodeEvaluationTerminalReason terminalReason,
	const FEpisodeEvaluationResult* evaluationResult)
{
	if (UWorld* world = ResolveWorld())
	{
		CurrentRecord.EndTimeSeconds = world->GetTimeSeconds();
	}

	CurrentRecord.DurationSeconds = FMath::Max(0.0, CurrentRecord.EndTimeSeconds - CurrentRecord.StartTimeSeconds);
	CurrentRecord.bSuccess = bSuccess;
	CurrentRecord.Outcome = outcome;
	CurrentRecord.TerminalReason = terminalReason;

	if (evaluationResult)
	{
		CurrentRecord.bEvaluationCompleted = evaluationResult->bCompleted;
		CurrentRecord.EvaluationResult = *evaluationResult;
		if (CurrentRecord.DurationSeconds <= 0.0)
		{
			CurrentRecord.DurationSeconds = evaluationResult->DurationSeconds;
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

void UEpisodeRunnerSubsystem::AppendCompileDiagnostics(const FEpisodeCompileResult& compileResult)
{
	for (const FEpisodeCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("%s [%s]: %s"),
			ToRunnerCompileSeverityString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

double UEpisodeRunnerSubsystem::GetRunTimeLimitSeconds(const FEpisodeRunConfig& runConfig) const
{
	const FEpisodeParamValue* timeLimitParam = runConfig.Parameters.Find(TEXT("time_limit_s"));
	if (!timeLimitParam) return 0.0;

	double timeLimitSeconds = 0.0;
	if (timeLimitParam->Type == EEpisodeParamValueType::Float)
	{
		timeLimitSeconds = timeLimitParam->FloatValue;
	}
	else if (timeLimitParam->Type == EEpisodeParamValueType::Integer)
	{
		timeLimitSeconds = static_cast<double>(timeLimitParam->IntegerValue);
	}
	else
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Ignoring non-numeric run parameter 'time_limit_s' | Template: %s"), *runConfig.TemplateId);
		return 0.0;
	}

	if (timeLimitSeconds < 0.0)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Clamping negative run parameter 'time_limit_s' | Template: %s, Value: %.2f"), *runConfig.TemplateId, timeLimitSeconds);
	}

	return FMath::Max(0.0, timeLimitSeconds);
}

FString UEpisodeRunnerSubsystem::BuildRunId() const
{
	return FString::Printf(TEXT("episode_run_%04d"), CurrentRunIndex);
}

UWorld* UEpisodeRunnerSubsystem::ResolveWorld() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetWorld() : nullptr;
}

UEpisodeSimulationSubsystem* UEpisodeRunnerSubsystem::ResolveSimulationSubsystem() const
{
	UWorld* world = ResolveWorld();
	return world ? world->GetSubsystem<UEpisodeSimulationSubsystem>() : nullptr;
}

UEpisodeEvaluationSubsystem* UEpisodeRunnerSubsystem::ResolveEvaluationSubsystem() const
{
	UWorld* world = ResolveWorld();
	return world ? world->GetSubsystem<UEpisodeEvaluationSubsystem>() : nullptr;
}
