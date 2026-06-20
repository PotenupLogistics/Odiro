
#include "Scenario/ScenarioRunnerSubsystem.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Episode/EpisodeMeasurementLogSubsystem.h"
#include "Scenario/ScenarioEvaluationSubsystem.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Scenario/ScenarioSimulationSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/UserProjectDataTypes.h"
#include "TimerManager.h"

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

	EScenarioCompileDiagnosticSeverity ToRunnerCompileSeverity(EScenarioSchemaDiagnosticSeverity severity)
	{
		switch (severity)
		{
		case EScenarioSchemaDiagnosticSeverity::Info:
			return EScenarioCompileDiagnosticSeverity::Info;
		case EScenarioSchemaDiagnosticSeverity::Warning:
		case EScenarioSchemaDiagnosticSeverity::Repair:
			return EScenarioCompileDiagnosticSeverity::Warning;
		case EScenarioSchemaDiagnosticSeverity::Error:
			return EScenarioCompileDiagnosticSeverity::Error;
		default:
			return EScenarioCompileDiagnosticSeverity::Error;
		}
	}

	void AppendRunnerSchemaDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& schemaDiagnostics,
		FScenarioCompileResult& result)
	{
		for (const FScenarioSchemaDiagnostic& schemaDiagnostic : schemaDiagnostics)
		{
			FScenarioCompileDiagnostic diagnostic;
			diagnostic.Severity = ToRunnerCompileSeverity(schemaDiagnostic.Severity);
			diagnostic.Code = schemaDiagnostic.Code;
			diagnostic.Message = schemaDiagnostic.Path.IsEmpty()
				? schemaDiagnostic.Message
				: FString::Printf(TEXT("%s | Path: %s"), *schemaDiagnostic.Message, *schemaDiagnostic.Path);
			result.Diagnostics.Add(diagnostic);
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

	FString ResolveProjectOutputEpisodeId(const FEpisodeRunRecord& runRecord)
	{
		if (FUserProjectEpisodeScenarioJson::IsValidEpisodeId(runRecord.EpisodeId))
		{
			return runRecord.EpisodeId;
		}

		if (FUserProjectEpisodeScenarioJson::IsValidEpisodeId(runRecord.PairId))
		{
			return runRecord.PairId;
		}

		return FUserProjectEpisodeScenarioJson::BuildEpisodeId(FMath::Max(0, runRecord.RunIndex));
	}

	// 현재 project run의 episode 절대 경로와 run 기준 상대 경로를 함께 만든다.
	bool TryBuildProjectEpisodeOutputDirectories(
		const FString& projectOutputEpisodeId,
		FString& outAbsoluteDirectory,
		FString& outRunRelativeDirectory)
	{
		outAbsoluteDirectory.Reset();
		outRunRelativeDirectory.Reset();

		const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
		if (!commandLineResult.bSuccess || !commandLineResult.Options.bProjectRun)
		{
			return false;
		}

		if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(projectOutputEpisodeId))
		{
			return false;
		}

		const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(
			commandLineResult.Options.ProjectPath,
			commandLineResult.Options.RunId);
		outAbsoluteDirectory = FUserProjectRunOutputJson::BuildEpisodeDirectory(paths, projectOutputEpisodeId);
		outRunRelativeDirectory = outAbsoluteDirectory;

		FString runDirectory = paths.RunPath;
		FPaths::NormalizeDirectoryName(runDirectory);
		if (!runDirectory.EndsWith(TEXT("/")))
		{
			runDirectory += TEXT("/");
		}

		if (!FPaths::MakePathRelativeTo(outRunRelativeDirectory, *runDirectory))
		{
			outAbsoluteDirectory.Reset();
			outRunRelativeDirectory.Reset();
			return false;
		}

		FPaths::NormalizeDirectoryName(outAbsoluteDirectory);
		FPaths::NormalizeFilename(outRunRelativeDirectory);
		if (outRunRelativeDirectory.Equals(TEXT("..")) || outRunRelativeDirectory.StartsWith(TEXT("../")))
		{
			outAbsoluteDirectory.Reset();
			outRunRelativeDirectory.Reset();
			return false;
		}

		return true;
	}

	FString BuildProjectOutputPathForEpisode(const FString& projectOutputEpisodeId, const TCHAR* fileName)
	{
		FString absoluteDirectory;
		FString runRelativeDirectory;
		if (!TryBuildProjectEpisodeOutputDirectories(
				projectOutputEpisodeId,
				absoluteDirectory,
				runRelativeDirectory))
		{
			return FString();
		}

		return FPaths::Combine(absoluteDirectory, fileName);
	}

	bool EnsureProjectJsonlFileExists(const FString& jsonlPath)
	{
		if (jsonlPath.TrimStartAndEnd().IsEmpty())
		{
			return false;
		}

		if (FPaths::FileExists(jsonlPath))
		{
			return true;
		}

		const FString jsonlDirectory = FPaths::GetPath(jsonlPath);
		if (!IFileManager::Get().MakeDirectory(*jsonlDirectory, true))
		{
			UE_LOG(LogScenarioRunner, Warning, TEXT("Project JSONL directory create failed | Path: %s"), *jsonlDirectory);
			return false;
		}

		if (!FFileHelper::SaveStringToFile(FString(), *jsonlPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogScenarioRunner, Warning, TEXT("Project JSONL file create failed | Path: %s"), *jsonlPath);
			return false;
		}

		return true;
	}

	void StartProjectTraceLoggingForEpisode(UWorld* world, const FString& projectOutputEpisodeId)
	{
		if (!IsValid(world))
		{
			return;
		}

		const FString tracePath = BuildProjectOutputPathForEpisode(projectOutputEpisodeId, TEXT("trace.jsonl"));
		if (tracePath.IsEmpty())
		{
			return;
		}

		const FString actionsPath = BuildProjectOutputPathForEpisode(projectOutputEpisodeId, TEXT("actions.jsonl"));
		EnsureProjectJsonlFileExists(actionsPath);

		if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
		{
			if (!measurementLogSubsystem->StartProjectTraceLogging(tracePath))
			{
				UE_LOG(LogScenarioRunner, Warning, TEXT("Project trace 시작 실패 | Episode: %s, Trace: %s"), *projectOutputEpisodeId, *tracePath);
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

	bool ConfigureProjectEpisodeOutputForEpisode(
		const FScenarioRuntimeContext& runtimeContext,
		const FString& projectOutputEpisodeId)
	{
		ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(runtimeContext.RobotActor);
		if (!IsValid(deliveryBot))
		{
			return false;
		}

		FString absoluteDirectory;
		FString runRelativeDirectory;
		if (!TryBuildProjectEpisodeOutputDirectories(
				projectOutputEpisodeId,
				absoluteDirectory,
				runRelativeDirectory))
		{
			UE_LOG(
				LogScenarioRunner,
				Warning,
				TEXT("Project episode output configuration failed | Episode: %s"),
				*projectOutputEpisodeId);
			return deliveryBot->ConfigureProjectEpisodeOutput(
				projectOutputEpisodeId,
				FString(),
				FString());
		}

		return deliveryBot->ConfigureProjectEpisodeOutput(
			projectOutputEpisodeId,
			absoluteDirectory,
			runRelativeDirectory);
	}

	FScenarioSimulationSetupSpec MakeSimulationSetupSpec(const FScenarioWorldSpec& worldSpec)
	{
		FScenarioSimulationSetupSpec setupSpec;
		setupSpec.EpisodeId = worldSpec.RunConfig.TemplateId;
		setupSpec.SpecHash = worldSpec.SpecHash;
		setupSpec.Seeds = worldSpec.Seeds;
		setupSpec.Corridors = worldSpec.Corridors;
		setupSpec.GroundRegions = worldSpec.GroundRegions;
		setupSpec.Placeables = worldSpec.Placeables;
		setupSpec.DynamicActors = worldSpec.DynamicActors;
		setupSpec.Paths = worldSpec.Paths;
		setupSpec.Events = worldSpec.Events;
		return setupSpec;
	}

	FScenarioCompileResult CompileRunnerScenarioSampleWorldSpec(const FString& scenarioSampleJsonPath)
	{
		const FScenarioSampleParseResult sampleParseResult = FScenarioSampleJson::ParseFromFile(scenarioSampleJsonPath);
		if (sampleParseResult.bSuccess)
		{
			return FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(sampleParseResult.Document);
		}

		FScenarioCompileResult result;
		AppendRunnerSchemaDiagnostics(sampleParseResult.Diagnostics, result);
		FScenarioCompileDiagnostic diagnostic;
		diagnostic.Severity = EScenarioCompileDiagnosticSeverity::Error;
		diagnostic.Code = TEXT("invalid_scenario_sample_input");
		diagnostic.Message = FString::Printf(
			TEXT("Runner input must be a scenario_sample JSON file: %s"),
			*scenarioSampleJsonPath);
		result.Diagnostics.Add(diagnostic);
		return result;
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
		if (!runInput.EpisodeScenarioJsonPath.IsEmpty() && !runInput.ProfileJsonPath.IsEmpty())
		{
			PendingRunInputs.Add(runInput);
		}
		else
		{
			UE_LOG(
				LogScenarioRunner,
				Warning,
				TEXT("Episode run input ignored: scenario_sample or profile path is empty | Pair: %s, ScenarioSample: %s, Profile: %s"),
				*runInput.PairId,
				*runInput.EpisodeScenarioJsonPath,
				*runInput.ProfileJsonPath);
		}
	}

	if (PendingRunInputs.IsEmpty()) return false;

	bCancelRequested = false;
	UnbindEvaluationDelegates();
	ResetEpisodeFinalizationState();

	if (UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
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

// 실행 중인 Episode는 Python end 처리 후 취소하고 준비 단계는 즉시 정리한다.
void UScenarioRunnerSubsystem::CancelRun()
{
	PendingRunInputs.Reset();
	bCancelRequested = true;

	UScenarioEvaluationSubsystem* evaluationSubsystem =
		ResolveEvaluationSubsystem();

	if (bEpisodeFinalizationInFlight
		|| (IsValid(evaluationSubsystem)
			&& evaluationSubsystem->IsAwaitingEndFinalization()))
	{
		SetRunnerState(EScenarioRunnerState::Ending);
		return;
	}

	if (IsValid(evaluationSubsystem)
		&& evaluationSubsystem->IsEvaluating())
	{
		FEpisodeEvaluationResult cancelledResult =
			evaluationSubsystem->GetCurrentResult();

		cancelledResult.bCompleted = false;
		cancelledResult.bSuccess = false;
		cancelledResult.Outcome = EEpisodeEvaluationOutcome::Cancelled;
		cancelledResult.TerminalReason = EEpisodeEvaluationTerminalReason::Cancelled;

		evaluationSubsystem->RequestEndEpisode(cancelledResult);
		return;
	}

	StopProjectTraceLogging(ResolveWorld());
	UnbindEvaluationDelegates();

	if (IsValid(evaluationSubsystem))
	{
		evaluationSubsystem->StopEvaluation();
	}

	if (UScenarioSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearScenario();
	}

	ResetEpisodeFinalizationState();
	ActiveBatchRunId.Reset();
	bCancelRequested = false;
	SetRunnerState(EScenarioRunnerState::Cancelled);

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

// 최종 결과를 저장하고 world를 정리한 뒤 다음 Scenario를 예약한다.
void UScenarioRunnerSubsystem::HandleEpisodeEnded(FEpisodeEvaluationResult result)
{
	if (bCancelRequested)
	{
		result.bSuccess = false;
		result.Outcome = EEpisodeEvaluationOutcome::Cancelled;
		result.TerminalReason = EEpisodeEvaluationTerminalReason::Cancelled;
	}

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
	UnbindEvaluationDelegates();

	if (UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->StopEvaluation();
	}

	if (UScenarioSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearScenario();
	}

	const bool bWasCancelled = bCancelRequested;
	ResetEpisodeFinalizationState();

	if (bWasCancelled)
	{
		bCancelRequested = false;
		ActiveBatchRunId.Reset();
		SetRunnerState(EScenarioRunnerState::Cancelled);
		return;
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

	StopProjectTraceLogging(world);
	CurrentRunInput = PendingRunInputs[0];
	PendingRunInputs.RemoveAt(0);
	++CurrentRunIndex;
	CurrentRunInput.PairId = BuildPairId(CurrentRunInput, CurrentRunIndex);

	CurrentRecord = FEpisodeRunRecord{};
	CurrentRecord.RunIndex = CurrentRunIndex;
	CurrentRecord.RunId = BuildRunId();
	CurrentRecord.PairId = CurrentRunInput.PairId;
	CurrentRecord.SourceJsonPath = CurrentRunInput.EpisodeScenarioJsonPath;
	CurrentRecord.EpisodeScenarioJsonPath = CurrentRunInput.EpisodeScenarioJsonPath;
	CurrentRecord.ProfileJsonPath = CurrentRunInput.ProfileJsonPath;
	CurrentRecord.PolicySpecJsonPath = CurrentRunInput.PolicySpecJsonPath;
	CurrentRecord.StartTimeSeconds = world->GetTimeSeconds();

	SetRunnerState(EScenarioRunnerState::Preparing);

	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Episode input preparing | RunId: %s, Pair: %s, Index: %d, ScenarioSample: %s, Profile: %s, PolicySpec: %s, Remaining: %d"),
		*CurrentRecord.RunId,
		*CurrentRecord.PairId,
		CurrentRecord.RunIndex,
		*CurrentRunInput.EpisodeScenarioJsonPath,
		*CurrentRunInput.ProfileJsonPath,
		CurrentRunInput.PolicySpecJsonPath.IsEmpty() ? TEXT("<profile>") : *CurrentRunInput.PolicySpecJsonPath,
		PendingRunInputs.Num());

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

	FScenarioCompileResult compileResult = CompileRunnerScenarioSampleWorldSpec(CurrentRunInput.EpisodeScenarioJsonPath);
	CurrentRecord.bEpisodeSetupCompileSucceeded = compileResult.bSuccess;
	CurrentRecord.EpisodeId = compileResult.WorldSpec.RunConfig.TemplateId;
	CurrentRecord.SpecHash = compileResult.WorldSpec.SpecHash;
	CurrentRecord.EpisodeSetupHash = compileResult.WorldSpec.SpecHash;
	AppendCompileDiagnostics(compileResult);

	UE_LOG(
		LogScenarioRunner,
		Warning,
		TEXT("scenario_sample compile completed | RunId: %s, Pair: %s, Scenario: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
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
		deliveryBotSetupCompiler->CompileDeliveryBotSetupFromJsonFile(CurrentRunInput.ProfileJsonPath);

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

	if (CurrentRunInput.bOverrideEvaluationConfig)
	{
		compileResult.WorldSpec.EvaluationConfig = CurrentRunInput.EvaluationConfig;
	}

	const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
	const bool bProjectRun = commandLineResult.bSuccess && commandLineResult.Options.bProjectRun;
	// Project run은 episode 출력 경로를 /scenario/start보다 먼저 주입해야 하므로 runner가 policy 시작을 소유한다.
	const bool bRunnerManagedPolicyStart = bProjectRun || !CurrentRunInput.PolicySpecJsonPath.IsEmpty();

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
	const FString projectOutputEpisodeId = ResolveProjectOutputEpisodeId(CurrentRecord);
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

	if (bProjectRun && !ConfigureProjectEpisodeOutputForEpisode(runtimeContext, projectOutputEpisodeId))
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Project episode output 설정 오류를 /scenario/start로 전달 | RunId: %s, Episode: %s"),
			*CurrentRecord.RunId,
			*projectOutputEpisodeId);
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
		TEXT("평가 전달 | RunId: %s, Episode: %s, TimeLimit: %.2fs, GoalRadius: %.1fcm, TipOver: %.1fdeg, NearMiss: %.1fcm, Robot: %s, HasGoal: %s, RuntimeActors: %d, GroundRegions: %d, StaticObstacles: %d, Pedestrians: %d"),
		*CurrentRecord.RunId,
		*runtimeContext.EpisodeId,
		timeLimitSeconds,
		compileResult.WorldSpec.EvaluationConfig.GoalAcceptanceRadiusCm,
		compileResult.WorldSpec.EvaluationConfig.TipOverAngleDegrees,
		compileResult.WorldSpec.EvaluationConfig.NearMissDistanceCm,
		*runtimeContext.RobotInstanceId,
		runtimeContext.bHasGoalLocation ? TEXT("true") : TEXT("false"),
		runtimeContext.RuntimeActors.Num(),
		runtimeContext.GroundRegionActors.Num(),
		runtimeContext.StaticObstacleActors.Num(),
		runtimeContext.PedestrianActors.Num());

	// 현재 Episode의 DeliveryBot을 종료 요청 동안만 약하게 참조한다.
	CurrentDeliveryBotActor = Cast<ADeliveryBot>(runtimeContext.RobotActor);

	// Evaluation의 종료 요청과 최종 종료를 Runner에 연결한다.
	UnbindEvaluationDelegates();
	EpisodeEndRequestedHandle = evaluationSubsystem->OnEpisodeEndRequested.AddUObject(
		this,
		&UScenarioRunnerSubsystem::HandleEpisodeEndRequested);
	evaluationSubsystem->OnEpisodeEnded.AddDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);

	if (!evaluationSubsystem->StartEvaluation(compileResult.WorldSpec.EvaluationConfig, runtimeContext, timeLimitSeconds))
	{
		StopProjectTraceLogging(world);
		UnbindEvaluationDelegates();
		ResetEpisodeFinalizationState();
		UE_LOG(LogScenarioRunner, Warning, TEXT("평가 시작 실패 | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::EvaluationStartFailed);
		simulationSubsystem->ClearScenario();
		QueueStartNextScenario();
		return;
	}

	StartProjectTraceLoggingForEpisode(world, projectOutputEpisodeId);

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
			TEXT("scenario_sample %s [%s]: %s"),
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
	if (runConfig.MaxDurationSeconds < 0.0)
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("음수 RunConfig.MaxDurationSeconds 클램프 | Template: %s, Value: %.2f"), *runConfig.TemplateId, runConfig.MaxDurationSeconds);
	}

	return FMath::Max(0.0, runConfig.MaxDurationSeconds);
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

	FString baseName = FPaths::GetBaseFilename(runInput.EpisodeScenarioJsonPath);
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

// Evaluation 종료 요청을 받아 DeliveryBot end 통신과 watchdog을 시작한다.
void UScenarioRunnerSubsystem::HandleEpisodeEndRequested(
	const FEpisodeEvaluationResult& result)
{
	if (bEpisodeFinalizationInFlight)
	{
		return;
	}

	SetRunnerState(EScenarioRunnerState::Ending);
	StopProjectTraceLogging(ResolveWorld());

	bEpisodeFinalizationInFlight = true;
	const uint64 generation = ++EpisodeFinalizationGeneration;
	const FString status =
		BuildExternalEndStatus(result.TerminalReason);

	UWorld* world = ResolveWorld();
	if (!IsValid(world))
	{
		CompleteEpisodeFinalization(
			generation,
			false,
			false,
			TEXT("Runner world is invalid."));
		return;
	}

	FTimerDelegate timeoutDelegate;
	timeoutDelegate.BindUObject(
		this,
		&UScenarioRunnerSubsystem::HandleEpisodeFinalizationTimeout,
		generation);

	world->GetTimerManager().SetTimer(
		EpisodeFinalizationTimeoutHandle,
		timeoutDelegate,
		EpisodeFinalizationTimeoutSeconds,
		false);

	ADeliveryBot* deliveryBot = CurrentDeliveryBotActor.Get();
	if (!IsValid(deliveryBot))
	{
		CompleteEpisodeFinalization(
			generation,
			false,
			false,
			TEXT("Current DeliveryBot is invalid."));
		return;
	}

	TWeakObjectPtr<UScenarioRunnerSubsystem> weakThis(this);

	deliveryBot->NotifyEpisodeFinalizingByEvaluation(
		status,
		[weakThis, generation](
			bool bSucceeded,
			const FString& errorMessage)
		{
			UScenarioRunnerSubsystem* runner = weakThis.Get();
			if (!IsValid(runner))
			{
				return;
			}

			runner->CompleteEpisodeFinalization(
				generation,
				bSucceeded,
				false,
				errorMessage);
		});
}

// 먼저 도착한 callback 또는 watchdog 하나만 처리한다.
void UScenarioRunnerSubsystem::CompleteEpisodeFinalization(
	uint64 finalizationGeneration,
	bool bSucceeded,
	bool bTimedOut,
	const FString& errorMessage)
{
	if (!bEpisodeFinalizationInFlight
		|| finalizationGeneration != EpisodeFinalizationGeneration)
	{
		return;
	}

	bEpisodeFinalizationInFlight = false;

	if (UWorld* world = ResolveWorld())
	{
		world->GetTimerManager().ClearTimer(
			EpisodeFinalizationTimeoutHandle);
	}

	if (!bSucceeded)
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Python end 실패 후 Episode 종료 계속 | TimedOut: %s, Error: %s"),
			bTimedOut ? TEXT("true") : TEXT("false"),
			*errorMessage);
	}

	UScenarioEvaluationSubsystem* evaluationSubsystem =
		ResolveEvaluationSubsystem();

	if (!IsValid(evaluationSubsystem)
		|| !evaluationSubsystem->IsAwaitingEndFinalization())
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Episode finalization 실패: Evaluation 상태 불일치"));
		return;
	}

	evaluationSubsystem->CompleteEndEpisode();
}

// HTTP callback이 오지 않으면 3초 후 Episode 종료를 계속한다.
void UScenarioRunnerSubsystem::HandleEpisodeFinalizationTimeout(
	uint64 finalizationGeneration)
{
	CompleteEpisodeFinalization(
		finalizationGeneration,
		false,
		true,
		TEXT("Python /scenario/end completion callback timed out."));
}

// Episode 종료 이유를 Python status 문자열로 변환한다.
FString UScenarioRunnerSubsystem::BuildExternalEndStatus(EEpisodeEvaluationTerminalReason terminalReason)
{
	switch (terminalReason)
	{
	case EEpisodeEvaluationTerminalReason::GoalReached:
		return TEXT("goal_reached");
	case EEpisodeEvaluationTerminalReason::Timeout:
		return TEXT("timeout");
	case EEpisodeEvaluationTerminalReason::RobotTipOver:
		return TEXT("robot_tip_over");
	case EEpisodeEvaluationTerminalReason::Cancelled:
		return TEXT("cancelled");
	case EEpisodeEvaluationTerminalReason::DeliveryBotSimulationFailed:
		return TEXT("delivery_bot_simulation_failed");
	default:
		return TEXT("failed");
	}
}
// EvaluationSubsystem에 연결된 종료 delegate를 해제한다.
void UScenarioRunnerSubsystem::UnbindEvaluationDelegates()
{
	UScenarioEvaluationSubsystem* evaluationSubsystem =
		ResolveEvaluationSubsystem();

	if (!IsValid(evaluationSubsystem))
	{
		EpisodeEndRequestedHandle.Reset();
		return;
	}

	if (EpisodeEndRequestedHandle.IsValid())
	{
		evaluationSubsystem->OnEpisodeEndRequested.Remove(
			EpisodeEndRequestedHandle);
		EpisodeEndRequestedHandle.Reset();
	}

	evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(
		this,
		&UScenarioRunnerSubsystem::HandleEpisodeEnded);
}


// timer를 해제하고 이전 Episode callback을 무효화한다.
void UScenarioRunnerSubsystem::ResetEpisodeFinalizationState()
{
	if (UWorld* world = ResolveWorld())
	{
		world->GetTimerManager().ClearTimer(
			EpisodeFinalizationTimeoutHandle);
	}

	bEpisodeFinalizationInFlight = false;
	++EpisodeFinalizationGeneration;
	CurrentDeliveryBotActor.Reset();
}

// Subsystem 종료 전에 delegate와 timer를 정리한다.
void UScenarioRunnerSubsystem::Deinitialize()
{
	UnbindEvaluationDelegates();
	ResetEpisodeFinalizationState();
	Super::Deinitialize();
}
