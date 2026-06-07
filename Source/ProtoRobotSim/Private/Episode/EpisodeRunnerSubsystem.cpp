
#include "Episode/EpisodeRunnerSubsystem.h"
#include "DeliveryBot/DeliveryBotSetupCompiler.h"
#include "Episode/EpisodeCompiler.h"
#include "Episode/EpisodeEvaluationSubsystem.h"
#include "Episode/EpisodeSimulationSubsystem.h"
#include "Shared/EpisodeEvaluationReportJson.h"

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

	void LogRunnerCompileDiagnostic(const TCHAR* sourceName, const FEpisodeCompileDiagnostic& diagnostic)
	{
		if (diagnostic.Severity == EEpisodeCompileDiagnosticSeverity::Info)
		{
			UE_LOG(
				LogEpisodeRunner,
				Log,
				TEXT("%s 컴파일 진단 | Severity: %s, Code: %s, Message: %s"),
				sourceName,
				ToRunnerCompileSeverityString(diagnostic.Severity),
				*diagnostic.Code,
				*diagnostic.Message);
			return;
		}

		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("%s 컴파일 진단 | Severity: %s, Code: %s, Message: %s"),
			sourceName,
			ToRunnerCompileSeverityString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message);
	}

	FString ResolveRunnerJsonFilePath(const FString& jsonFilePath)
	{
		if (jsonFilePath.IsEmpty()) return jsonFilePath;

		if (FPaths::IsRelative(jsonFilePath))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), jsonFilePath));
		}

		return jsonFilePath;
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

	FString NormalizeRunnerJsonFilePathForCompare(const FString& jsonFilePath)
	{
		FString normalizedJsonFilePath = ResolveRunnerJsonFilePath(jsonFilePath);
		FPaths::NormalizeFilename(normalizedJsonFilePath);
		FPaths::CollapseRelativeDirectories(normalizedJsonFilePath);
		return normalizedJsonFilePath;
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

	bool ApplyDeliveryBotSetupToWorldSpec(
		FEpisodeWorldSpec& worldSpec,
		const FDeliveryBotSetupInfo& deliveryBotSetupInfo)
	{
		bool bApplied = false;
		for (FEpisodePlaceableInstanceSpec& placeableSpec : worldSpec.Placeables)
		{
			if (placeableSpec.Category != EEpisodeActorCategory::DeliveryBot) continue;

			const FDeliveryBotLocationSetupInfo locationSetupInfo = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo;
			FDeliveryBotSetupInfo mergedSetupInfo = deliveryBotSetupInfo;
			mergedSetupInfo.LocationSetupInfo = locationSetupInfo;
			placeableSpec.DeliveryBot.SetupInfo = mergedSetupInfo;
			bApplied = true;
		}

		return bApplied;
	}

	FString BuildPairHash(const FString& episodeSetupHash, const FString& deliveryBotSetupHash)
	{
		return FString::Printf(TEXT("%u"), GetTypeHash(episodeSetupHash + TEXT(":") + deliveryBotSetupHash));
	}
}

bool UEpisodeRunnerSubsystem::StartEpisodePairFromJsonFiles(
	const FString& episodeSetupJsonPath,
	const FString& deliveryBotSetupJsonPath)
{
	if (episodeSetupJsonPath.IsEmpty() || deliveryBotSetupJsonPath.IsEmpty()) return false;

	FEpisodeRunInput runInput;
	runInput.EpisodeSetupJsonPath = episodeSetupJsonPath;
	runInput.DeliveryBotSetupJsonPath = deliveryBotSetupJsonPath;

	TArray<FEpisodeRunInput> runInputs;
	runInputs.Add(runInput);
	return StartBatchFromRunInputs(runInputs);
}

bool UEpisodeRunnerSubsystem::StartBatchFromRunInputs(const TArray<FEpisodeRunInput>& runInputs)
{
	return StartBatchFromRunInputsInternal(runInputs, FString(), FString());
}

bool UEpisodeRunnerSubsystem::StartBatchFromRunInputsInternal(
	const TArray<FEpisodeRunInput>& runInputs,
	const FString& activeRunQueueJsonFilePath,
	const FString& activeBatchRunId)
{
	if (IsBatchActive())
	{
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode batch 시작 거부: 기존 batch 실행 중 | State: %s, ActiveRunQueue: %s"),
			*ToRunnerEnumString(RunnerState),
			ActiveRunQueueJsonFilePath.IsEmpty() ? TEXT("<direct>") : *ActiveRunQueueJsonFilePath);
		return false;
	}

	PendingRunInputs.Reset();
	for (const FEpisodeRunInput& runInput : runInputs)
	{
		if (!runInput.EpisodeSetupJsonPath.IsEmpty() && !runInput.DeliveryBotSetupJsonPath.IsEmpty())
		{
			PendingRunInputs.Add(runInput);
		}
		else
		{
			UE_LOG(
				LogEpisodeRunner,
				Warning,
				TEXT("Episode pair 입력 무시: EpisodeSetup 또는 DeliveryBotSetup 경로가 비어 있음 | Pair: %s, EpisodeSetup: %s, DeliveryBotSetup: %s"),
				*runInput.PairId,
				*runInput.EpisodeSetupJsonPath,
				*runInput.DeliveryBotSetupJsonPath);
		}
	}

	if (PendingRunInputs.IsEmpty()) return false;

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
	TotalRunCount = PendingRunInputs.Num();
	ActiveRunQueueJsonFilePath = activeRunQueueJsonFilePath;
	ActiveBatchRunId = activeBatchRunId.TrimStartAndEnd();
	SetRunnerState(EEpisodeRunnerState::Preparing);

	UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode pair 배치 시작 | Count: %d"), PendingRunInputs.Num());

	StartNextEpisode();
	return true;
}

bool UEpisodeRunnerSubsystem::StartBatchFromRunQueueJsonFile(const FString& runQueueJsonFilePath)
{
	return StartBatchFromRunQueueJsonFileForRun(runQueueJsonFilePath, FString());
}

bool UEpisodeRunnerSubsystem::StartBatchFromRunQueueJsonFileForRun(
	const FString& runQueueJsonFilePath,
	const FString& activeRunId)
{
	if (runQueueJsonFilePath.IsEmpty()) return false;
	const FString resolvedRunQueueJsonFilePath = ResolveRunnerJsonFilePath(runQueueJsonFilePath);
	const FString normalizedRunQueueJsonFilePath = NormalizeRunnerJsonFilePathForCompare(runQueueJsonFilePath);

	if (IsBatchActive())
	{
		if (IsRunningRunQueueJsonFile(runQueueJsonFilePath))
		{
			UE_LOG(
				LogEpisodeRunner,
				Log,
				TEXT("Episode run queue 이미 실행 중 | Path: %s, State: %s"),
				*runQueueJsonFilePath,
				*ToRunnerEnumString(RunnerState));
			return true;
		}

		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode run queue 시작 거부: 다른 batch 실행 중 | ActiveRunQueue: %s, RequestedRunQueue: %s"),
			ActiveRunQueueJsonFilePath.IsEmpty() ? TEXT("<direct>") : *ActiveRunQueueJsonFilePath,
			*normalizedRunQueueJsonFilePath);
		return false;
	}

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedRunQueueJsonFilePath))
	{
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode run queue JSON 읽기 실패 | Path: %s, ResolvedPath: %s"),
			*runQueueJsonFilePath,
			*resolvedRunQueueJsonFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode run queue JSON 파싱 실패 | Path: %s, ResolvedPath: %s"),
			*runQueueJsonFilePath,
			*resolvedRunQueueJsonFilePath);
		return false;
	}

	const TSharedPtr<FJsonValue> runsValue = rootObject->TryGetField(TEXT("runs"));
	if (!runsValue.IsValid() || runsValue->Type != EJson::Array)
	{
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode run queue에 runs 배열이 없음 | Path: %s, ResolvedPath: %s"),
			*runQueueJsonFilePath,
			*resolvedRunQueueJsonFilePath);
		return false;
	}

	TArray<FEpisodeRunInput> runInputs;
	const TArray<TSharedPtr<FJsonValue>> runValues = runsValue->AsArray();
	for (int32 index = 0; index < runValues.Num(); ++index)
	{
		const TSharedPtr<FJsonValue>& runValue = runValues[index];
		if (!runValue.IsValid() || runValue->Type != EJson::Object)
		{
			UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode run queue 항목 무시: object가 아님 | Path: %s, Index: %d"), *runQueueJsonFilePath, index);
			continue;
		}

		const TSharedPtr<FJsonObject> runObject = runValue->AsObject();
		if (!runObject.IsValid())
		{
			UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode run queue 항목 무시: object 변환 실패 | Path: %s, Index: %d"), *runQueueJsonFilePath, index);
			continue;
		}

		FEpisodeRunInput runInput;
		runObject->TryGetStringField(TEXT("pair_id"), runInput.PairId);
		if (!runObject->TryGetStringField(TEXT("episode_setup"), runInput.EpisodeSetupJsonPath))
		{
			runObject->TryGetStringField(TEXT("episode_setup_json_path"), runInput.EpisodeSetupJsonPath);
		}
		if (!runObject->TryGetStringField(TEXT("delivery_bot_setup"), runInput.DeliveryBotSetupJsonPath))
		{
			runObject->TryGetStringField(TEXT("delivery_bot_setup_json_path"), runInput.DeliveryBotSetupJsonPath);
		}

		if (runInput.EpisodeSetupJsonPath.IsEmpty())
		{
			UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode run queue 항목 무시: episode_setup이 비어 있음 | Path: %s, Index: %d"), *runQueueJsonFilePath, index);
			continue;
		}

		if (runInput.DeliveryBotSetupJsonPath.IsEmpty())
		{
			UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode run queue 항목 무시: delivery_bot_setup이 비어 있음 | Path: %s, Index: %d"), *runQueueJsonFilePath, index);
			continue;
		}

		runInputs.Add(runInput);
	}

	if (runInputs.IsEmpty())
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode run queue에서 실행할 pair를 찾지 못함 | Path: %s"), *runQueueJsonFilePath);
		return false;
	}

	UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode run queue 로드 완료 | Path: %s, Count: %d"), *runQueueJsonFilePath, runInputs.Num());
	return StartBatchFromRunInputsInternal(runInputs, normalizedRunQueueJsonFilePath, activeRunId);
}

void UEpisodeRunnerSubsystem::CancelRun()
{
	PendingRunInputs.Reset();
	ActiveRunQueueJsonFilePath.Reset();
	ActiveBatchRunId.Reset();
	SetRunnerState(EEpisodeRunnerState::Cancelled);

	if (UEpisodeEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

	if (UEpisodeSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem())
	{
		simulationSubsystem->ClearEpisode();
	}

	UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode 실행 취소됨."));
}

bool UEpisodeRunnerSubsystem::IsBatchActive() const
{
	return RunnerState == EEpisodeRunnerState::Preparing
		|| RunnerState == EEpisodeRunnerState::Running
		|| RunnerState == EEpisodeRunnerState::Ending;
}

void UEpisodeRunnerSubsystem::SetRunnerState(EEpisodeRunnerState runnerState)
{
	if (RunnerState == runnerState)
	{
		return;
	}

	RunnerState = runnerState;
	OnRunnerStateChanged.Broadcast(RunnerState);
}

bool UEpisodeRunnerSubsystem::IsRunningRunQueueJsonFile(const FString& runQueueJsonFilePath) const
{
	if (!IsBatchActive() || ActiveRunQueueJsonFilePath.IsEmpty())
	{
		return false;
	}

	return ActiveRunQueueJsonFilePath.Equals(
		NormalizeRunnerJsonFilePathForCompare(runQueueJsonFilePath),
		ESearchCase::IgnoreCase);
}

bool UEpisodeRunnerSubsystem::BuildLatestEvaluationReportJson(FString& outJson) const
{
	if (RunRecords.IsEmpty())
	{
		outJson.Reset();
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Evaluation report JSON 생성 실패: 기록이 없음"));
		return false;
	}

	return BuildEvaluationReportJson(RunRecords.Num() - 1, outJson);
}

bool UEpisodeRunnerSubsystem::BuildEvaluationReportJson(int32 runRecordIndex, FString& outJson) const
{
	if (!RunRecords.IsValidIndex(runRecordIndex))
	{
		outJson.Reset();
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Evaluation report JSON 생성 실패: 기록 index가 유효하지 않음 | Index: %d, Records: %d"),
			runRecordIndex,
			RunRecords.Num());
		return false;
	}

	TArray<FString> diagnostics;
	const bool bSucceeded = FEpisodeEvaluationReportJson::TryWriteReportJson(
		RunRecords[runRecordIndex],
		outJson,
		diagnostics);
	for (const FString& diagnostic : diagnostics)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Evaluation report JSON 진단 | %s"), *diagnostic);
	}

	return bSucceeded;
}

void UEpisodeRunnerSubsystem::HandleEpisodeEnded(FEpisodeEvaluationResult result)
{
	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode 종료 콜백 수신 | Episode: %s, Success: %s, Outcome: %s, TerminalReason: %s, Duration: %.2fs, Events: %d"),
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

	SetRunnerState(EEpisodeRunnerState::Ending);
	QueueStartNextEpisode();
}

void UEpisodeRunnerSubsystem::StartNextEpisode()
{
	if (PendingRunInputs.IsEmpty())
	{
		SetRunnerState(EEpisodeRunnerState::Completed);
		ActiveRunQueueJsonFilePath.Reset();
		ActiveBatchRunId.Reset();
		UE_LOG(LogEpisodeRunner, Log, TEXT("Episode 배치 완료 | Records: %d"), RunRecords.Num());
		return;
	}

	UWorld* world = ResolveWorld();
	UEpisodeSimulationSubsystem* simulationSubsystem = ResolveSimulationSubsystem();
	UEpisodeEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem();
	if (!world || !simulationSubsystem || !evaluationSubsystem)
	{
		SetRunnerState(EEpisodeRunnerState::Failed);
		ActiveRunQueueJsonFilePath.Reset();
		ActiveBatchRunId.Reset();
		UE_LOG(
			LogEpisodeRunner,
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
	CurrentRecord.SourceJsonPath = CurrentRunInput.EpisodeSetupJsonPath;
	CurrentRecord.EpisodeSetupJsonPath = CurrentRunInput.EpisodeSetupJsonPath;
	CurrentRecord.DeliveryBotSetupJsonPath = CurrentRunInput.DeliveryBotSetupJsonPath;
	CurrentRecord.StartTimeSeconds = world->GetTimeSeconds();

	SetRunnerState(EEpisodeRunnerState::Preparing);

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode pair 준비 중 | RunId: %s, Pair: %s, Index: %d, EpisodeSetup: %s, DeliveryBotSetup: %s, Remaining: %d"),
		*CurrentRecord.RunId,
		*CurrentRecord.PairId,
		CurrentRecord.RunIndex,
		*CurrentRunInput.EpisodeSetupJsonPath,
		*CurrentRunInput.DeliveryBotSetupJsonPath,
		PendingRunInputs.Num());

	UEpisodeCompiler* compiler = NewObject<UEpisodeCompiler>(this);
	if (!compiler)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("Episode 컴파일러 생성 실패 | RunId: %s"), *CurrentRecord.RunId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompilerCreateFailed);
		QueueStartNextEpisode();
		return;
	}

	UDeliveryBotSetupCompiler* deliveryBotSetupCompiler = NewObject<UDeliveryBotSetupCompiler>(this);
	if (!deliveryBotSetupCompiler)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("DeliveryBotSetup 컴파일러 생성 실패 | RunId: %s"), *CurrentRecord.RunId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::CompilerCreateFailed);
		QueueStartNextEpisode();
		return;
	}

	FEpisodeCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonFile(CurrentRunInput.EpisodeSetupJsonPath);
	CurrentRecord.bEpisodeSetupCompileSucceeded = compileResult.bSuccess;
	CurrentRecord.EpisodeId = compileResult.WorldSpec.RunConfig.TemplateId;
	CurrentRecord.SpecHash = compileResult.WorldSpec.SpecHash;
	CurrentRecord.EpisodeSetupHash = compileResult.WorldSpec.SpecHash;
	AppendCompileDiagnostics(compileResult);

	UE_LOG(
		LogEpisodeRunner,
		Warning,
		TEXT("EpisodeSetup 컴파일 완료 | RunId: %s, Pair: %s, Episode: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
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
		QueueStartNextEpisode();
		return;
	}

	FDeliveryBotSetupCompileResult deliveryBotCompileResult =
		deliveryBotSetupCompiler->CompileDeliveryBotSetupFromJsonFile(CurrentRunInput.DeliveryBotSetupJsonPath);

	CurrentRecord.bDeliveryBotSetupCompileSucceeded = deliveryBotCompileResult.bSuccess;
	CurrentRecord.DeliveryBotSetupHash = deliveryBotCompileResult.SpecHash;
	CurrentRecord.PairHash = BuildPairHash(CurrentRecord.EpisodeSetupHash, CurrentRecord.DeliveryBotSetupHash);
	AppendDeliveryBotSetupDiagnostics(deliveryBotCompileResult);

	UE_LOG(
		LogEpisodeRunner,
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
		QueueStartNextEpisode();
		return;
	}

	const bool bDeliveryBotSetupApplied = ApplyDeliveryBotSetupToWorldSpec(
		compileResult.WorldSpec,
		deliveryBotCompileResult.SetupInfo);
	if (!bDeliveryBotSetupApplied)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("DeliveryBotSetup 적용 대상 로봇이 없음 | RunId: %s, Pair: %s"), *CurrentRecord.RunId, *CurrentRecord.PairId);
	}

	CurrentRecord.bCompileSucceeded = true;

	const FEpisodeSimulationSetupSpec simulationSetupSpec = MakeSimulationSetupSpec(compileResult.WorldSpec);

	simulationSubsystem->ClearEpisode();
	const bool bSetupSucceeded = simulationSubsystem->SetupEpisodeWorld(simulationSetupSpec);
	CurrentRecord.bSetupSucceeded = bSetupSucceeded;

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Episode 설정 완료 | RunId: %s, Episode: %s, Success: %s"),
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
	if (!IsValid(runtimeContext.RobotActor))
	{
		CurrentRecord.bSetupSucceeded = false;
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Episode 설정 실패: 런타임 컨텍스트에 유효한 로봇 액터가 없음 | RunId: %s, Episode: %s"),
			*CurrentRecord.RunId,
			*runtimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::SetupFailed);
		simulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	const double timeLimitSeconds = GetRunTimeLimitSeconds(compileResult.WorldSpec.RunConfig);

	UE_LOG(
		LogEpisodeRunner,
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

	evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
	evaluationSubsystem->OnEpisodeEnded.AddDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);

	if (!evaluationSubsystem->StartEvaluation(compileResult.WorldSpec.EvaluationConfig, runtimeContext, timeLimitSeconds))
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UEpisodeRunnerSubsystem::HandleEpisodeEnded);
		UE_LOG(LogEpisodeRunner, Warning, TEXT("평가 시작 실패 | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
		CompleteCurrentRecord(
			false,
			EEpisodeEvaluationOutcome::Failure,
			EEpisodeEvaluationTerminalReason::EvaluationStartFailed);
		simulationSubsystem->ClearEpisode();
		QueueStartNextEpisode();
		return;
	}

	SetRunnerState(EEpisodeRunnerState::Running);
	UE_LOG(LogEpisodeRunner, Log, TEXT("Episode 실행 중 | RunId: %s, Episode: %s"), *CurrentRecord.RunId, *runtimeContext.EpisodeId);
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
	if (bSaveEvaluationReportJson)
	{
		SaveEvaluationReportJsonForRecord(RunRecords.Last());
	}
	OnRunRecordCompleted.Broadcast(RunRecords.Last());

	UE_LOG(
		LogEpisodeRunner,
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

void UEpisodeRunnerSubsystem::AppendCompileDiagnostics(const FEpisodeCompileResult& compileResult)
{
	for (const FEpisodeCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("EpisodeSetup %s [%s]: %s"),
			ToRunnerCompileSeverityString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

void UEpisodeRunnerSubsystem::AppendDeliveryBotSetupDiagnostics(const FDeliveryBotSetupCompileResult& compileResult)
{
	for (const FEpisodeCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("DeliveryBotSetup %s [%s]: %s"),
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
		UE_LOG(LogEpisodeRunner, Warning, TEXT("숫자가 아닌 run parameter 'time_limit_s' 무시 | Template: %s"), *runConfig.TemplateId);
		return 0.0;
	}

	if (timeLimitSeconds < 0.0)
	{
		UE_LOG(LogEpisodeRunner, Warning, TEXT("음수 run parameter 'time_limit_s' 클램프 | Template: %s, Value: %.2f"), *runConfig.TemplateId, timeLimitSeconds);
	}

	return FMath::Max(0.0, timeLimitSeconds);
}

FString UEpisodeRunnerSubsystem::BuildRunId() const
{
	if (!ActiveBatchRunId.IsEmpty())
	{
		return ActiveBatchRunId;
	}

	return FString::Printf(TEXT("episode_run_%04d"), CurrentRunIndex);
}

bool UEpisodeRunnerSubsystem::SaveEvaluationReportJsonForRecord(FEpisodeRunRecord& runRecord) const
{
	FString jsonString;
	TArray<FString> diagnostics;
	if (!FEpisodeEvaluationReportJson::TryWriteReportJson(runRecord, jsonString, diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogEpisodeRunner, Warning, TEXT("Evaluation report JSON 저장 전 직렬화 진단 | %s"), *diagnostic);
		}
		return false;
	}

	const FString outputFilePath = BuildEvaluationReportJsonFilePath(runRecord);
	const FString outputDirectory = FPaths::GetPath(outputFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Evaluation report JSON 저장 실패: 디렉터리 생성 실패 | Path: %s"),
			*outputDirectory);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(jsonString, *outputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(
			LogEpisodeRunner,
			Warning,
			TEXT("Evaluation report JSON 저장 실패: 파일 쓰기 실패 | Path: %s"),
			*outputFilePath);
		return false;
	}

	UE_LOG(
		LogEpisodeRunner,
		Log,
		TEXT("Evaluation report JSON 저장 완료 | RunId: %s, Episode: %s, Path: %s"),
		*runRecord.RunId,
		*runRecord.EpisodeId,
		*outputFilePath);
	runRecord.EvaluationReportJsonPath = MakeEpisodeRunnerProjectRelativePath(outputFilePath);
	return true;
}

FString UEpisodeRunnerSubsystem::BuildEvaluationReportJsonFilePath(const FEpisodeRunRecord& runRecord) const
{
	const FString directory = EvaluationReportOutputDirectory.IsEmpty()
		? TEXT("Json/Output")
		: EvaluationReportOutputDirectory;
	const FString resolvedDirectory = FPaths::IsRelative(directory)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), directory))
		: directory;

	const FString fileName = FString::Printf(
		TEXT("%s_%s_%s_evaluation_report.json"),
		*SanitizeReportFileToken(runRecord.RunId),
		*SanitizeReportFileToken(runRecord.PairId),
		*SanitizeReportFileToken(runRecord.EpisodeId));
	return FPaths::Combine(resolvedDirectory, fileName);
}

FString UEpisodeRunnerSubsystem::BuildPairId(const FEpisodeRunInput& runInput, int32 runIndex)
{
	if (!runInput.PairId.IsEmpty()) return runInput.PairId;

	FString baseName = FPaths::GetBaseFilename(runInput.EpisodeSetupJsonPath);
	if (baseName.IsEmpty())
	{
		baseName = FString::Printf(TEXT("pair_%04d"), runIndex);
	}

	return baseName;
}

FString UEpisodeRunnerSubsystem::SanitizeReportFileToken(const FString& value)
{
	FString safeValue = FPaths::MakeValidFileName(value);
	if (safeValue.IsEmpty())
	{
		return TEXT("Unknown");
	}

	safeValue.ReplaceInline(TEXT(".."), TEXT("_"));
	safeValue.ReplaceInline(TEXT("/"), TEXT("_"));
	safeValue.ReplaceInline(TEXT("\\"), TEXT("_"));
	return safeValue;
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
