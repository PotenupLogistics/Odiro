
#include "Scenario/ScenarioRunnerSubsystem.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Episode/EpisodeMeasurementLogSubsystem.h"
#include "Scenario/ScenarioCompiler.h"
#include "Scenario/ScenarioEvaluationSubsystem.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Scenario/ScenarioSimulationSubsystem.h"
#include "Scenario/UserProjectEpisodeScenarioWorldSpecAdapter.h"
#include "Misc/Paths.h"
#include "Shared/UserProjectDataTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioRunner, Log, All);

namespace
{
	const TCHAR* ToRunnerCompileSeverityString(EScenarioCompileDiagnosticSeverity severity)
	{
		switch (severity)
		{
		case EScenarioCompileDiagnosticSeverity::Info:
			return TEXT("정보");
		case EScenarioCompileDiagnosticSeverity::Warning:
			return TEXT("경고");
		case EScenarioCompileDiagnosticSeverity::Error:
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

	void LogRunnerCompileDiagnostic(const TCHAR* sourceName, const FScenarioCompileDiagnostic& diagnostic)
	{
		if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Info)
		{
			UE_LOG(
				LogScenarioRunner,
				Log,
				TEXT("%s 컴파일 진단 | Severity: %s, Code: %s, Message: %s"),
				sourceName,
				ToRunnerCompileSeverityString(diagnostic.Severity),
				*diagnostic.Code,
				*diagnostic.Message);
			return;
		}

		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("%s 컴파일 진단 | Severity: %s, Code: %s, Message: %s"),
			sourceName,
			ToRunnerCompileSeverityString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message);
	}

	FString BuildProjectTracePathForEpisode(const FString& episodeId)
	{
		const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
		if (!commandLineResult.bSuccess || !commandLineResult.Options.bProjectRun)
		{
			return FString();
		}

		if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(episodeId))
		{
			return FString();
		}

		const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(
			commandLineResult.Options.ProjectPath,
			commandLineResult.Options.RunId);
		return FPaths::Combine(
			FUserProjectRunOutputJson::BuildEpisodeDirectory(paths, episodeId),
			TEXT("trace.jsonl"));
	}

	void StartProjectTraceLoggingForEpisode(UWorld* world, const FString& episodeId)
	{
		if (!IsValid(world))
		{
			return;
		}

		const FString tracePath = BuildProjectTracePathForEpisode(episodeId);
		if (tracePath.IsEmpty())
		{
			return;
		}

		if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
		{
			if (!measurementLogSubsystem->StartProjectTraceLogging(tracePath))
			{
				UE_LOG(LogScenarioRunner, Warning, TEXT("Project trace 시작 실패 | Episode: %s, Trace: %s"), *episodeId, *tracePath);
			}
		}
	}

	void StopProjectTraceLogging(UWorld* world)
	{
		if (!IsValid(world))
		{
			return;
		}

		if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
		{
			measurementLogSubsystem->StopProjectTraceLogging();
		}
	}

	FString MakeEpisodeRunnerProjectRelativePath(FString filePath)
	{
		if (filePath.IsEmpty())
		{
			return filePath;
		}

		FPaths::NormalizeFilename(filePath);

		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FString projectRelativePath = filePath;
		if (!FPaths::IsRelative(projectRelativePath) && FPaths::MakePathRelativeTo(projectRelativePath, *projectDir))
		{
			projectRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			return projectRelativePath;
		}

		filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return filePath;
	}

	FScenarioSimulationSetupSpec MakeSimulationSetupSpec(const FScenarioWorldSpec& worldSpec)
	{
		FScenarioSimulationSetupSpec setupSpec;
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

	FScenarioCompileResult CompileRunnerScenarioWorldSpec(
		const FString& scenarioJsonPath,
		const UScenarioCompiler* runtimeCompiler)
	{
		if (FUserProjectEpisodeScenarioWorldSpecAdapter::IsEpisodeScenarioFile(scenarioJsonPath))
		{
			return FUserProjectEpisodeScenarioWorldSpecAdapter::CompileScenarioWorldSpecFromEpisodeScenarioFile(scenarioJsonPath);
		}

		if (FScenarioSampleWorldSpecAdapter::IsScenarioSampleFile(scenarioJsonPath))
		{
			return FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleFile(scenarioJsonPath);
		}

		return runtimeCompiler
			? runtimeCompiler->CompileScenarioWorldSpecFromJsonFile(scenarioJsonPath)
			: FScenarioCompileResult();
	}

	bool ApplyDeliveryBotSetupToWorldSpec(
		FScenarioWorldSpec& worldSpec,
		const FDeliveryBotSetupInfo& deliveryBotSetupInfo,
		const FString& policySpecJsonPath,
		bool bDeferPolicyAutoStartToRunner)
	{
		bool bApplied = false;
		for (FScenarioPlaceableInstanceSpec& placeableSpec : worldSpec.Placeables)
		{
			if (placeableSpec.Category != EScenarioActorCategory::DeliveryBot) continue;

			const FDeliveryBotLocationSetupInfo locationSetupInfo = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo;
			FDeliveryBotSetupInfo mergedSetupInfo = deliveryBotSetupInfo;
			if (!policySpecJsonPath.TrimStartAndEnd().IsEmpty())
			{
				mergedSetupInfo.StartupPolicySpecFileName = policySpecJsonPath.TrimStartAndEnd();
			}
			mergedSetupInfo.LocationSetupInfo = locationSetupInfo;
			if (bDeferPolicyAutoStartToRunner)
			{
				mergedSetupInfo.LocationSetupInfo.bAutoStartRoute = false;
			}
			placeableSpec.DeliveryBot.SetupInfo = mergedSetupInfo;
			bApplied = true;
		}

		return bApplied;
	}

	FString BuildPairHash(
		const FString& episodeSetupHash,
		const FString& deliveryBotSetupHash,
		const FString& policySpecJsonPath)
	{
		FString hashSource = episodeSetupHash + TEXT(":") + deliveryBotSetupHash;
		const FString trimmedPolicySpecJsonPath = policySpecJsonPath.TrimStartAndEnd();
		if (!trimmedPolicySpecJsonPath.IsEmpty())
		{
			hashSource += TEXT(":") + trimmedPolicySpecJsonPath;
		}

		return FString::Printf(TEXT("%u"), GetTypeHash(hashSource));
	}
}

bool UScenarioRunnerSubsystem::StartScenarioPairFromJsonFiles(
	const FString& scenarioSetupJsonPath,
	const FString& deliveryBotSetupJsonPath)
{
	if (scenarioSetupJsonPath.IsEmpty() || deliveryBotSetupJsonPath.IsEmpty()) return false;

	FScenarioRunInput runInput;
	runInput.ScenarioSetupJsonPath = scenarioSetupJsonPath;
	runInput.DeliveryBotSetupJsonPath = deliveryBotSetupJsonPath;

	TArray<FScenarioRunInput> runInputs;
	runInputs.Add(runInput);
	return StartBatchFromRunInputs(runInputs);
}

bool UScenarioRunnerSubsystem::StartBatchFromRunInputs(const TArray<FScenarioRunInput>& runInputs)
{
	return StartBatchFromRunInputsInternal(runInputs, FString());
}

bool UScenarioRunnerSubsystem::StartBatchFromRunInputsForRun(
	const TArray<FScenarioRunInput>& runInputs,
	const FString& activeRunId)
{
	return StartBatchFromRunInputsInternal(runInputs, activeRunId);
}

bool UScenarioRunnerSubsystem::StartBatchFromRunInputsInternal(
	const TArray<FScenarioRunInput>& runInputs,
	const FString& activeBatchRunId)
{
	if (IsBatchActive())
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Episode batch 시작 거부: 기존 batch 실행 중 | State: %s"),
			*ToRunnerEnumString(RunnerState));
		return false;
	}

	PendingRunInputs.Reset();
	for (const FScenarioRunInput& runInput : runInputs)
	{
		if (!runInput.ScenarioSetupJsonPath.IsEmpty() && !runInput.DeliveryBotSetupJsonPath.IsEmpty())
		{
			PendingRunInputs.Add(runInput);
		}
		else
		{
			UE_LOG(
				LogScenarioRunner,
				Warning,
				TEXT("Scenario pair 입력 무시: ScenarioSetup 또는 DeliveryBotSetup 경로가 비어 있음 | Pair: %s, ScenarioSetup: %s, DeliveryBotSetup: %s"),
				*runInput.PairId,
				*runInput.ScenarioSetupJsonPath,
				*runInput.DeliveryBotSetupJsonPath);
		}
	}

	if (PendingRunInputs.IsEmpty()) return false;

	if (UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	if (UScenarioSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearScenario();
	}

	RunRecords.Reset();
	CurrentRunIndex = INDEX_NONE;
	TotalRunCount = PendingRunInputs.Num();
	ActiveBatchRunId = activeBatchRunId.TrimStartAndEnd();
	SetRunnerState(EScenarioRunnerState::Preparing);

	UE_LOG(LogScenarioRunner, Warning, TEXT("Scenario pair 배치 시작 | Count: %d"), PendingRunInputs.Num());

	StartNextScenario();
	return true;
}

void UScenarioRunnerSubsystem::CancelRun()
{
	StopProjectTraceLogging(ResolveWorld());
	PendingRunInputs.Reset();
	ActiveBatchRunId.Reset();
	SetRunnerState(EScenarioRunnerState::Cancelled);

	if (UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	StopProjectTraceLogging(ResolveWorld());

	if (UScenarioSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearScenario();
	}

	UE_LOG(LogScenarioRunner, Warning, TEXT("Episode 실행 취소됨."));
}

bool UScenarioRunnerSubsystem::IsBatchActive() const
{
	return RunnerState == EScenarioRunnerState::Preparing
		|| RunnerState == EScenarioRunnerState::Running
		|| RunnerState == EScenarioRunnerState::Ending;
}

void UScenarioRunnerSubsystem::SetRunnerState(EScenarioRunnerState runnerState)
{
	if (RunnerState == runnerState)
	{
		return;
	}

	RunnerState = runnerState;
	OnRunnerStateChanged.Broadcast(RunnerState);
}

void UScenarioRunnerSubsystem::HandleEpisodeEnded(FEpisodeEvaluationResult result)
{
	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Episode 종료 콜백 수신 | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Events: %d"),
		*result.EpisodeId,
		result.bSuccess ? TEXT("true") : TEXT("false"),
		*ToRunnerEnumString(result.Outcome),
		*ToRunnerEnumString(result.TerminalReason),
		result.DurationSeconds,
		result.Events.Num());

	CompleteCurrentRecord(result.bSuccess, result.Outcome, result.TerminalReason, &result);

	if (UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	if (UScenarioSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearScenario();
	}

	SetRunnerState(EScenarioRunnerState::Ending);
	QueueStartNextScenario();
}

void UScenarioRunnerSubsystem::StartNextScenario()
{
	if (PendingRunInputs.IsEmpty())
	{
		SetRunnerState(EScenarioRunnerState::Completed);
		ActiveBatchRunId.Reset();
		UE_LOG(LogScenarioRunner, Log, TEXT("Episode 배치 완료 | Records: %d"), RunRecords.Num());
		return;
	}

	UWorld* world = ResolveWorld();
	UScenarioSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem();
	UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem();
	if (!world || !simulationSubsystem || !evaluationSubsystem)
	{
		SetRunnerState(EScenarioRunnerState::Failed);
		ActiveBatchRunId.Reset();
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Episode runner가 서브시스템을 찾지 못함 | World: %s, Simulation: %s, Evaluation: %s"),
			world ? TEXT("valid") : TEXT("null"),
			simulationSubsystem ? TEXT("valid") : TEXT("null"),
			evaluationSubsystem ? TEXT("valid") : TEXT("null"));
		return;
	}

	CurrentRunInput = PendingRunInputs[0];
	PendingRunInputs.RemoveAt(0);
	++CurrentRunIndex;
	CurrentRunInput.PairId = BuildPairId(CurrentRunInput, CurrentRunIndex);

	CurrentRecord = FEpisodeRunRecord{};
	CurrentRecord.RunIndex = CurrentRunIndex;
	CurrentRecord.RunId = BuildRunId();
	CurrentRecord.PairId = CurrentRunInput.PairId;
	CurrentRecord.SourceJsonPath = CurrentRunInput.ScenarioSetupJsonPath;
	CurrentRecord.EpisodeSetupJsonPath = CurrentRunInput.ScenarioSetupJsonPath;
	CurrentRecord.DeliveryBotSetupJsonPath = CurrentRunInput.DeliveryBotSetupJsonPath;
	CurrentRecord.PolicySpecJsonPath = CurrentRunInput.PolicySpecJsonPath;
	CurrentRecord.StartTimeSeconds = world->GetTimeSeconds();

	SetRunnerState(EScenarioRunnerState::Preparing);

	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Scenario pair 준비 중 | RunId: %s, Pair: %s, Index: %d, ScenarioSetup: %s, DeliveryBotSetup: %s, PolicySpec: %s, Remaining: %d"),
		*CurrentRecord.RunId,
		*CurrentRecord.PairId,
		CurrentRecord.RunIndex,
		*CurrentRunInput.ScenarioSetupJsonPath,
		*CurrentRunInput.DeliveryBotSetupJsonPath,
		CurrentRunInput.PolicySpecJsonPath.IsEmpty() ? TEXT("<delivery_bot_setup>") : *CurrentRunInput.PolicySpecJsonPath,
		PendingRunInputs.Num());

	UScenarioCompiler* compiler = NewObject<UScenarioCompiler>(this);
	if (!compiler)
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("Episode 컴파일러 생성 실패 | RunId: %s"), *CurrentRecord.RunId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompilerCreateFailed);
		QueueStartNextScenario();
		return;
	}

	UDeliveryBotSetupCompiler* deliveryBotSetupCompiler = NewObject<UDeliveryBotSetupCompiler>(this);
	if (!deliveryBotSetupCompiler)
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("DeliveryBotSetup 컴파일러 생성 실패 | RunId: %s"), *CurrentRecord.RunId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompilerCreateFailed);
		QueueStartNextScenario();
		return;
	}

	FScenarioCompileResult compileResult = CompileRunnerScenarioWorldSpec(CurrentRunInput.ScenarioSetupJsonPath, compiler);
	CurrentRecord.bEpisodeSetupCompileSucceeded = compileResult.bSuccess;
	CurrentRecord.EpisodeId = compileResult.WorldSpec.RunConfig.TemplateId;
	CurrentRecord.SpecHash = compileResult.WorldSpec.SpecHash;
	CurrentRecord.EpisodeSetupHash = compileResult.WorldSpec.SpecHash;
	AppendCompileDiagnostics(compileResult);

	UE_LOG(
		LogScenarioRunner,
		Warning,
		TEXT("ScenarioSetup 컴파일 완료 | RunId: %s, Pair: %s, Scenario: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.PairId,
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
		QueueStartNextScenario();
		return;
	}

	FDeliveryBotSetupCompileResult deliveryBotCompileResult =
		deliveryBotSetupCompiler->CompileDeliveryBotSetupFromJsonFile(CurrentRunInput.DeliveryBotSetupJsonPath);

	CurrentRecord.bDeliveryBotSetupCompileSucceeded = deliveryBotCompileResult.bSuccess;
	CurrentRecord.DeliveryBotSetupHash = deliveryBotCompileResult.SpecHash;
	CurrentRecord.PairHash = BuildPairHash(
		CurrentRecord.EpisodeSetupHash,
		CurrentRecord.DeliveryBotSetupHash,
		CurrentRunInput.PolicySpecJsonPath);
	AppendDeliveryBotSetupDiagnostics(deliveryBotCompileResult);

	UE_LOG(
		LogScenarioRunner,
		Warning,
		TEXT("DeliveryBotSetup 컴파일 완료 | RunId: %s, Pair: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.PairId,
		deliveryBotCompileResult.bSuccess ? TEXT("true") : TEXT("false"),
		deliveryBotCompileResult.Diagnostics.Num(),
		*CurrentRecord.DeliveryBotSetupHash);

	if (!deliveryBotCompileResult.bSuccess)
	{
		CurrentRecord.bCompileSucceeded = false;
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompileFailed);
		QueueStartNextScenario();
		return;
	}

	const bool bRunnerManagedPolicyStart = !CurrentRunInput.PolicySpecJsonPath.IsEmpty();

	const bool bDeliveryBotSetupApplied = ApplyDeliveryBotSetupToWorldSpec(
		compileResult.WorldSpec,
		deliveryBotCompileResult.SetupInfo,
		CurrentRunInput.PolicySpecJsonPath,
		bRunnerManagedPolicyStart);
	if (!bDeliveryBotSetupApplied)
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("DeliveryBotSetup 적용 대상 로봇이 없음 | RunId: %s, Pair: %s"), *CurrentRecord.RunId, *CurrentRecord.PairId);
	}

	CurrentRecord.bCompileSucceeded = true;

	const FScenarioSimulationSetupSpec simulationSetupSpec = MakeSimulationSetupSpec(compileResult.WorldSpec);

	simulationSubsystem->ClearScenario();
	const bool bSetupSucceeded = simulationSubsystem->SetupScenarioWorld(simulationSetupSpec);
	CurrentRecord.bSetupSucceeded = bSetupSucceeded;

	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Scenario 설정 완료 | RunId: %s, Scenario: %s, Success: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.EpisodeId,
		bSetupSucceeded ? TEXT("true") : TEXT("false"));

	if (!bSetupSucceeded)
	{
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::SetupFailed);
		simulationSubsystem->ClearScenario();
		QueueStartNextScenario();
		return;
	}

	const FScenarioRuntimeContext runtimeContext = simulationSubsystem->BuildRuntimeContext(simulationSetupSpec);
	if (!IsValid(runtimeContext.RobotActor))
	{
		CurrentRecord.bSetupSucceeded = false;
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Scenario 설정 실패: 런타임 컨텍스트에 유효한 로봇 액터가 없음 | RunId: %s, Scenario: %s"),
			*CurrentRecord.RunId,
			*runtimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::SetupFailed);
		simulationSubsystem->ClearScenario();
		QueueStartNextScenario();
		return;
	}

	if (bRunnerManagedPolicyStart)
	{
		ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(runtimeContext.RobotActor);
		if (!IsValid(deliveryBot))
		{
			CurrentRecord.bSetupSucceeded = false;
			UE_LOG(
				LogScenarioRunner,
				Warning,
				TEXT("PolicySpec 적용 실패: runtime robot actor가 DeliveryBot이 아님 | RunId: %s, Pair: %s, PolicySpec: %s"),
				*CurrentRecord.RunId,
				*CurrentRecord.PairId,
				*CurrentRunInput.PolicySpecJsonPath);
			CompleteCurrentRecord(
				false,
				EEpisodeEvaluationOutcome::Failure,
				EEpisodeEvaluationTerminalReason::SetupFailed);
			simulationSubsystem->ClearScenario();
			QueueStartNextScenario();
			return;
		}

		if (!deliveryBot->StartPolicyRunWithPolicySpecFileName(CurrentRunInput.PolicySpecJsonPath))
		{
			CurrentRecord.bSetupSucceeded = false;
			UE_LOG(
				LogScenarioRunner,
				Warning,
				TEXT("PolicySpec 적용 요청 시작 실패 | RunId: %s, Pair: %s, PolicySpec: %s"),
				*CurrentRecord.RunId,
				*CurrentRecord.PairId,
				*CurrentRunInput.PolicySpecJsonPath);
			CompleteCurrentRecord(
				false,
				EEpisodeEvaluationOutcome::Failure,
				EEpisodeEvaluationTerminalReason::SetupFailed);
			simulationSubsystem->ClearScenario();
			QueueStartNextScenario();
			return;
		}

		UE_LOG(
			LogScenarioRunner,
			Log,
			TEXT("PolicySpec 적용 요청 시작 | RunId: %s, Pair: %s, PolicySpec: %s"),
			*CurrentRecord.RunId,
			*CurrentRecord.PairId,
			*CurrentRunInput.PolicySpecJsonPath);
	}

	const double timeLimitSeconds = GetRunTimeLimitSeconds(compileResult.WorldSpec.RunConfig);

	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("평가 전달 | RunId: %s, Episode: %s, TimeLimit: %.2fs, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
		*CurrentRecord.RunId,
		*runtimeContext.EpisodeId,
		timeLimitSeconds,
		*runtimeContext.RobotInstanceId,
		runtimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		runtimeContext.RuntimeActors.Num(),
		runtimeContext.GroundRegionActors.Num(),
		runtimeContext.StaticObstacleActors.Num(),
		runtimeContext.PedestrianActors.Num());

	evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);
	evaluationSubsystem->OnEpisodeEnded.AddDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);

	StartProjectTraceLoggingForEpisode(world, runtimeContext.EpisodeId);

	if (!evaluationSubsystem->StartEvaluation(compileResult.WorldSpec.EvaluationConfig, runtimeContext, timeLimitSeconds))
	{
		StopProjectTraceLogging(world);
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);
		UE_LOG(LogScenarioRunner, Warning, TEXT("평가 시작 실패 | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::EvaluationStartFailed);
		simulationSubsystem->ClearScenario();
		QueueStartNextScenario();
		return;
	}

	SetRunnerState(EScenarioRunnerState::Running);
	UE_LOG(LogScenarioRunner, Log, TEXT("Scenario 실행 중 | RunId: %s, Scenario: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
}

void UScenarioRunnerSubsystem::QueueStartNextScenario()
{
	if (UWorld* world = ResolveWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UScenarioRunnerSubsystem::StartNextScenario));
		return;
	}

	StartNextScenario();
}

void UScenarioRunnerSubsystem::CompleteCurrentRecord(
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
	OnRunRecordCompleted.Broadcast(RunRecords.Last());

	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Episode 기록 완료 | RunId: %s, Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, EvaluationCompleted: %s, TotalRecords: %d"),
		*CurrentRecord.RunId,
		*CurrentRecord.EpisodeId,
		CurrentRecord.bSuccess ? TEXT("true") : TEXT("false"),
		*ToRunnerEnumString(CurrentRecord.Outcome),
		*ToRunnerEnumString(CurrentRecord.TerminalReason),
		CurrentRecord.DurationSeconds,
		CurrentRecord.bEvaluationCompleted ? TEXT("true") : TEXT("false"),
		RunRecords.Num());
}

void UScenarioRunnerSubsystem::AppendCompileDiagnostics(const FScenarioCompileResult& compileResult)
{
	for (const FScenarioCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("ScenarioSetup %s [%s]: %s"),
			ToRunnerCompileSeverityString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

void UScenarioRunnerSubsystem::AppendDeliveryBotSetupDiagnostics(const FDeliveryBotSetupCompileResult& compileResult)
{
	for (const FScenarioCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("DeliveryBotSetup %s [%s]: %s"),
			ToRunnerCompileSeverityString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

double UScenarioRunnerSubsystem::GetRunTimeLimitSeconds(const FScenarioRunConfig& runConfig) const
{
	const FScenarioParamValue* timeLimitParam = runConfig.Parameters.Find(TEXT("time_limit_s"));
	if (!timeLimitParam) return 0.0;

	double timeLimitSeconds = 0.0;
	if (timeLimitParam->Type == EScenarioParamValueType::Float)
	{
		timeLimitSeconds = timeLimitParam->FloatValue;
	}
	else if (timeLimitParam->Type == EScenarioParamValueType::Integer)
	{
		timeLimitSeconds = static_cast<double>(timeLimitParam->IntegerValue);
	}
	else
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("숫자가 아닌 run parameter 'time_limit_s' 무시 | Template: %s"), *runConfig.TemplateId);
		return 0.0;
	}

	if (timeLimitSeconds < 0.0)
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("음수 run parameter 'time_limit_s' 클램프 | Template: %s, Value: %.2f"), *runConfig.TemplateId, timeLimitSeconds);
	}

	return FMath::Max(0.0, timeLimitSeconds);
}

FString UScenarioRunnerSubsystem::BuildRunId() const
{
	if (!ActiveBatchRunId.IsEmpty())
	{
		return ActiveBatchRunId;
	}

	return FString::Printf(TEXT("episode_run_%04d"), CurrentRunIndex);
}

FString UScenarioRunnerSubsystem::BuildPairId(const FScenarioRunInput& runInput, int32 runIndex)
{
	if (!runInput.PairId.IsEmpty()) return runInput.PairId;

	FString baseName = FPaths::GetBaseFilename(runInput.ScenarioSetupJsonPath);
	if (baseName.IsEmpty())
	{
		baseName = FString::Printf(TEXT("pair_%04d"), runIndex);
	}

	return baseName;
}

UWorld* UScenarioRunnerSubsystem::ResolveWorld() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetWorld() : nullptr;
}

UScenarioSimulationSubsystem* UScenarioRunnerSubsystem::ResolveSimulationSubsystem() const
{
	UWorld* world = ResolveWorld();
	return world ? world->GetSubsystem<UScenarioSimulationSubsystem>() : nullptr;
}

UScenarioEvaluationSubsystem* UScenarioRunnerSubsystem::ResolveEvaluationSubsystem() const
{
	UWorld* world = ResolveWorld();
	return world ? world->GetSubsystem<UScenarioEvaluationSubsystem>() : nullptr;
}
