
#include "Scenario/ScenarioRunnerSubsystem.h"
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "Scenario/ScenarioEvaluationSubsystem.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Scenario/ScenarioSimulationSubsystem.h"
#include "Scenario/ScenarioSimulationProfileAdapter.h"
#include "Scenario/ScenarioTemplateWorldSpecAdapter.h"
#include "Shared/EpisodeEvaluationReportJson.h"
#include "Shared/EpisodeRunResultJson.h"

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
		const FScenarioRunInput& runInput)
	{
		const FString& scenarioJsonPath = runInput.ScenarioSourceJsonPath;
		if (FScenarioSampleWorldSpecAdapter::IsScenarioSampleFile(scenarioJsonPath))
		{
			return FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleFile(scenarioJsonPath);
		}

		if (FScenarioTemplateWorldSpecAdapter::IsScenarioTemplateFile(scenarioJsonPath))
		{
			FScenarioTemplateSampleRequest request =
				FScenarioTemplateWorldSpecAdapter::MakeDefaultSampleRequest(scenarioJsonPath, runInput.PairId);
			request.ProfileRef = runInput.SimulationProfileJsonPath;
			request.ProfileHash = FScenarioSimulationProfileAdapter::MakeProfileFileHash(runInput.SimulationProfileJsonPath);
			return FScenarioTemplateWorldSpecAdapter::CompileScenarioWorldSpecFromTemplateFile(scenarioJsonPath, request).CompileResult;
		}

		FScenarioCompileResult result;
		FScenarioCompileDiagnostic diagnostic;
		diagnostic.Severity = EScenarioCompileDiagnosticSeverity::Error;
		diagnostic.Code = TEXT("unsupported_scenario_source_schema");
		diagnostic.Message = FString::Printf(
			TEXT("Scenario runner only accepts scenario_template or scenario_sample JSON sources. Source: %s"),
			*scenarioJsonPath);
		result.Diagnostics.Add(diagnostic);
		result.bSuccess = false;
		return result;
	}

	bool ApplySimulationProfileToWorldSpec(
		FScenarioWorldSpec& worldSpec,
		const FDeliveryBotSetupInfo& simulationProfileSetupInfo,
		const FString& policySpecJsonPath,
		bool bDeferPolicyAutoStartToRunner)
	{
		bool bApplied = false;
		for (FScenarioPlaceableInstanceSpec& placeableSpec : worldSpec.Placeables)
		{
			if (placeableSpec.Category != EScenarioActorCategory::DeliveryBot) continue;

			const FDeliveryBotLocationSetupInfo locationSetupInfo = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo;
			FDeliveryBotSetupInfo mergedSetupInfo = simulationProfileSetupInfo;
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

	FScenarioSimulationProfileCompileResult CompileRunnerSimulationProfile(
		const FString& simulationProfileJsonPath)
	{
		if (FScenarioSimulationProfileAdapter::IsSimulationProfileFile(simulationProfileJsonPath))
		{
			return FScenarioSimulationProfileAdapter::CompileProfileFromJsonFile(simulationProfileJsonPath);
		}

		FScenarioSimulationProfileCompileResult result;
		FScenarioCompileDiagnostic diagnostic;
		diagnostic.Severity = EScenarioCompileDiagnosticSeverity::Error;
		diagnostic.Code = TEXT("unsupported_simulation_profile_schema");
		diagnostic.Message = FString::Printf(
			TEXT("Scenario runner only accepts simulation_profile JSON for robot setup. Source: %s"),
			*simulationProfileJsonPath);
		result.Diagnostics.Add(diagnostic);
		result.bSuccess = false;
		result.ProfileHash = FScenarioSimulationProfileAdapter::MakeProfileFileHash(simulationProfileJsonPath);
		return result;
	}

	FString BuildPairHash(
		const FString& scenarioSourceHash,
		const FString& simulationProfileHash,
		const FString& policySpecJsonPath)
	{
		FString hashSource = scenarioSourceHash + TEXT(":") + simulationProfileHash;
		const FString trimmedPolicySpecJsonPath = policySpecJsonPath.TrimStartAndEnd();
		if (!trimmedPolicySpecJsonPath.IsEmpty())
		{
			hashSource += TEXT(":") + trimmedPolicySpecJsonPath;
		}

		return FString::Printf(TEXT("%u"), GetTypeHash(hashSource));
	}
}

bool UScenarioRunnerSubsystem::StartScenarioPairFromJsonFiles(
	const FString& scenarioSourceJsonPath,
	const FString& simulationProfileJsonPath)
{
	if (scenarioSourceJsonPath.IsEmpty() || simulationProfileJsonPath.IsEmpty()) return false;

	FScenarioRunInput runInput;
	runInput.ScenarioSourceJsonPath = scenarioSourceJsonPath;
	runInput.SimulationProfileJsonPath = simulationProfileJsonPath;

	TArray<FScenarioRunInput> runInputs;
	runInputs.Add(runInput);
	return StartBatchFromRunInputs(runInputs);
}

bool UScenarioRunnerSubsystem::StartBatchFromRunInputs(const TArray<FScenarioRunInput>& runInputs)
{
	return StartBatchFromRunInputsInternal(runInputs, FString(), FString());
}

bool UScenarioRunnerSubsystem::StartBatchFromRunInputsForRun(
	const TArray<FScenarioRunInput>& runInputs,
	const FString& activeRunId)
{
	return StartBatchFromRunInputsInternal(runInputs, FString(), activeRunId);
}

bool UScenarioRunnerSubsystem::StartBatchFromRunInputsInternal(
	const TArray<FScenarioRunInput>& runInputs,
	const FString& activeBatchSourceLabel,
	const FString& activeBatchRunId)
{
	if (IsBatchActive())
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Episode batch 시작 거부: 기존 batch 실행 중 | State: %s, ActiveBatchSource: %s"),
			*ToRunnerEnumString(RunnerState),
			ActiveBatchSourceLabel.IsEmpty() ? TEXT("<direct>") : *ActiveBatchSourceLabel);
		return false;
	}

	PendingRunInputs.Reset();
	for (const FScenarioRunInput& runInput : runInputs)
	{
		if (!runInput.ScenarioSourceJsonPath.IsEmpty() && !runInput.SimulationProfileJsonPath.IsEmpty())
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
				*runInput.ScenarioSourceJsonPath,
				*runInput.SimulationProfileJsonPath);
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
	ActiveBatchSourceLabel = activeBatchSourceLabel;
	ActiveBatchRunId = activeBatchRunId.TrimStartAndEnd();
	SetRunnerState(EScenarioRunnerState::Preparing);

	UE_LOG(LogScenarioRunner, Warning, TEXT("Scenario pair 배치 시작 | Count: %d"), PendingRunInputs.Num());

	StartNextScenario();
	return true;
}

void UScenarioRunnerSubsystem::CancelRun()
{
	PendingRunInputs.Reset();
	ActiveBatchSourceLabel.Reset();
	ActiveBatchRunId.Reset();
	SetRunnerState(EScenarioRunnerState::Cancelled);

	if (UScenarioEvaluationSubsystem* evaluationSubsystem = ResolveEvaluationSubsystem())
	{
		evaluationSubsystem->OnEpisodeEnded.RemoveDynamic(this, &UScenarioRunnerSubsystem::HandleEpisodeEnded);
		evaluationSubsystem->StopEvaluation();
	}

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

bool UScenarioRunnerSubsystem::BuildLatestEvaluationReportJson(FString& outJson) const
{
	if (RunRecords.IsEmpty())
	{
		outJson.Reset();
		UE_LOG(LogScenarioRunner, Warning, TEXT("Evaluation report JSON 생성 실패: 기록이 없음"));
		return false;
	}

	return BuildEvaluationReportJson(RunRecords.Num() - 1, outJson);
}

bool UScenarioRunnerSubsystem::BuildEvaluationReportJson(int32 runRecordIndex, FString& outJson) const
{
	if (!RunRecords.IsValidIndex(runRecordIndex))
	{
		outJson.Reset();
		UE_LOG(
			LogScenarioRunner,
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
		UE_LOG(LogScenarioRunner, Warning, TEXT("Evaluation report JSON 진단 | %s"), *diagnostic);
	}

	return bSucceeded;
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
		SaveRunSummaryJson();
		SetRunnerState(EScenarioRunnerState::Completed);
		ActiveBatchSourceLabel.Reset();
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
		ActiveBatchSourceLabel.Reset();
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
	CurrentRecord.SourceJsonPath = CurrentRunInput.ScenarioSourceJsonPath;
	CurrentRecord.ScenarioSourceJsonPath = CurrentRunInput.ScenarioSourceJsonPath;
	CurrentRecord.SimulationProfileJsonPath = CurrentRunInput.SimulationProfileJsonPath;
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
		*CurrentRunInput.ScenarioSourceJsonPath,
		*CurrentRunInput.SimulationProfileJsonPath,
		CurrentRunInput.PolicySpecJsonPath.IsEmpty() ? TEXT("<simulation_profile>") : *CurrentRunInput.PolicySpecJsonPath,
		PendingRunInputs.Num());

	FScenarioCompileResult compileResult = CompileRunnerScenarioWorldSpec(CurrentRunInput);
	CurrentRecord.bScenarioSourceCompileSucceeded = compileResult.bSuccess;
	CurrentRecord.EpisodeId = compileResult.WorldSpec.RunConfig.TemplateId;
	CurrentRecord.SpecHash = compileResult.WorldSpec.SpecHash;
	CurrentRecord.ScenarioSourceHash = compileResult.WorldSpec.SpecHash;
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

	FScenarioSimulationProfileCompileResult profileCompileResult =
		CompileRunnerSimulationProfile(CurrentRunInput.SimulationProfileJsonPath);

	CurrentRecord.bSimulationProfileCompileSucceeded = profileCompileResult.bSuccess;
	CurrentRecord.SimulationProfileHash = profileCompileResult.ProfileHash;
	CurrentRecord.PairHash = BuildPairHash(
		CurrentRecord.ScenarioSourceHash,
		CurrentRecord.SimulationProfileHash,
		CurrentRunInput.PolicySpecJsonPath);
	AppendSimulationProfileDiagnostics(profileCompileResult);

	UE_LOG(
		LogScenarioRunner,
		Warning,
		TEXT("DeliveryBotSetup 컴파일 완료 | RunId: %s, Pair: %s, Success: %s, Diagnostics: %d, SpecHash: %s"),
		*CurrentRecord.RunId,
		*CurrentRecord.PairId,
		profileCompileResult.bSuccess ? TEXT("true") : TEXT("false"),
		profileCompileResult.Diagnostics.Num(),
		*CurrentRecord.SimulationProfileHash);

	if (!profileCompileResult.bSuccess)
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

	const bool bSimulationProfileApplied = ApplySimulationProfileToWorldSpec(
		compileResult.WorldSpec,
		profileCompileResult.SetupInfo,
		CurrentRunInput.PolicySpecJsonPath,
		bRunnerManagedPolicyStart);
	if (!bSimulationProfileApplied)
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

	if (!evaluationSubsystem->StartEvaluation(compileResult.WorldSpec.EvaluationConfig, runtimeContext, timeLimitSeconds))
	{
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
	SaveEpisodeResultFilesForRecord(RunRecords.Last());
	if (bSaveEvaluationReportJson)
	{
		SaveEvaluationReportJsonForRecord(RunRecords.Last());
	}
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

void UScenarioRunnerSubsystem::AppendSimulationProfileDiagnostics(const FScenarioSimulationProfileCompileResult& compileResult)
{
	for (const FScenarioCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		CurrentRecord.Diagnostics.Add(FString::Printf(
			TEXT("SimulationProfile %s [%s]: %s"),
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

bool UScenarioRunnerSubsystem::SaveEpisodeResultFilesForRecord(FEpisodeRunRecord& runRecord) const
{
	const FString eventsFilePath = BuildEpisodeEventsJsonlFilePath(runRecord);
	const FString resultFilePath = BuildEpisodeResultJsonFilePath(runRecord);
	const FString outputDirectory = FPaths::GetPath(resultFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Episode result save failed: directory create failed | Path: %s"),
			*outputDirectory);
		return false;
	}

	FString eventsJsonl;
	TArray<FString> eventDiagnostics;
	if (!FEpisodeRunResultJson::TryWriteEpisodeEventsJsonl(runRecord, eventsJsonl, eventDiagnostics))
	{
		for (const FString& diagnostic : eventDiagnostics)
		{
			UE_LOG(LogScenarioRunner, Warning, TEXT("Episode events JSONL diagnostic | %s"), *diagnostic);
		}
		return false;
	}
	if (!FFileHelper::SaveStringToFile(eventsJsonl, *eventsFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("Episode events JSONL save failed | Path: %s"), *eventsFilePath);
		return false;
	}

	FString resultJson;
	TArray<FString> resultDiagnostics;
	if (!FEpisodeRunResultJson::TryWriteEpisodeResultJson(runRecord, resultJson, resultDiagnostics))
	{
		for (const FString& diagnostic : resultDiagnostics)
		{
			UE_LOG(LogScenarioRunner, Warning, TEXT("Episode result JSON diagnostic | %s"), *diagnostic);
		}
		return false;
	}
	if (!FFileHelper::SaveStringToFile(resultJson, *resultFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("Episode result JSON save failed | Path: %s"), *resultFilePath);
		return false;
	}

	runRecord.EpisodeEventsJsonlPath = MakeEpisodeRunnerProjectRelativePath(eventsFilePath);
	runRecord.EpisodeResultJsonPath = MakeEpisodeRunnerProjectRelativePath(resultFilePath);
	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Episode result files saved | RunId: %s, Episode: %s, Result: %s, Events: %s"),
		*runRecord.RunId,
		*runRecord.EpisodeId,
		*resultFilePath,
		*eventsFilePath);
	return true;
}

bool UScenarioRunnerSubsystem::SaveRunSummaryJson() const
{
	FString summaryJson;
	TArray<FString> diagnostics;
	if (!FEpisodeRunResultJson::TryWriteRunSummaryJson(RunRecords, summaryJson, diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogScenarioRunner, Warning, TEXT("Run summary JSON diagnostic | %s"), *diagnostic);
		}
		return false;
	}

	const FString outputFilePath = BuildRunSummaryJsonFilePath();
	const FString outputDirectory = FPaths::GetPath(outputFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("Run summary JSON save failed: directory create failed | Path: %s"), *outputDirectory);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(summaryJson, *outputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogScenarioRunner, Warning, TEXT("Run summary JSON save failed | Path: %s"), *outputFilePath);
		return false;
	}

	UE_LOG(LogScenarioRunner, Log, TEXT("Run summary JSON saved | Records: %d, Path: %s"), RunRecords.Num(), *outputFilePath);
	return true;
}

FString UScenarioRunnerSubsystem::BuildRunSummaryJsonFilePath() const
{
	const FString directory = EvaluationReportOutputDirectory.IsEmpty()
		? TEXT("Json/Output")
		: EvaluationReportOutputDirectory;
	const FString resolvedDirectory = FPaths::IsRelative(directory)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), directory))
		: directory;
	return FPaths::Combine(resolvedDirectory, TEXT("summary.json"));
}

FString UScenarioRunnerSubsystem::BuildEpisodeResultJsonFilePath(const FEpisodeRunRecord& runRecord) const
{
	const FString directory = EvaluationReportOutputDirectory.IsEmpty()
		? TEXT("Json/Output")
		: EvaluationReportOutputDirectory;
	const FString resolvedDirectory = FPaths::IsRelative(directory)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), directory))
		: directory;
	return FPaths::Combine(
		resolvedDirectory,
		TEXT("episodes"),
		SanitizeReportFileToken(runRecord.PairId),
		TEXT("result.json"));
}

FString UScenarioRunnerSubsystem::BuildEpisodeEventsJsonlFilePath(const FEpisodeRunRecord& runRecord) const
{
	return FPaths::Combine(FPaths::GetPath(BuildEpisodeResultJsonFilePath(runRecord)), TEXT("events.jsonl"));
}

bool UScenarioRunnerSubsystem::SaveEvaluationReportJsonForRecord(FEpisodeRunRecord& runRecord) const
{
	FString jsonString;
	TArray<FString> diagnostics;
	if (!FEpisodeEvaluationReportJson::TryWriteReportJson(runRecord, jsonString, diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogScenarioRunner, Warning, TEXT("Evaluation report JSON 저장 전 직렬화 진단 | %s"), *diagnostic);
		}
		return false;
	}

	const FString outputFilePath = BuildEvaluationReportJsonFilePath(runRecord);
	const FString outputDirectory = FPaths::GetPath(outputFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Evaluation report JSON 저장 실패: 디렉터리 생성 실패 | Path: %s"),
			*outputDirectory);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(jsonString, *outputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(
			LogScenarioRunner,
			Warning,
			TEXT("Evaluation report JSON 저장 실패: 파일 쓰기 실패 | Path: %s"),
			*outputFilePath);
		return false;
	}

	UE_LOG(
		LogScenarioRunner,
		Log,
		TEXT("Evaluation report JSON 저장 완료 | RunId: %s, Episode: %s, Path: %s"),
		*runRecord.RunId,
		*runRecord.EpisodeId,
		*outputFilePath);
	runRecord.EvaluationReportJsonPath = MakeEpisodeRunnerProjectRelativePath(outputFilePath);
	return true;
}

FString UScenarioRunnerSubsystem::BuildEvaluationReportJsonFilePath(const FEpisodeRunRecord& runRecord) const
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

FString UScenarioRunnerSubsystem::BuildPairId(const FScenarioRunInput& runInput, int32 runIndex)
{
	if (!runInput.PairId.IsEmpty()) return runInput.PairId;

	FString baseName = FPaths::GetBaseFilename(runInput.ScenarioSourceJsonPath);
	if (baseName.IsEmpty())
	{
		baseName = FString::Printf(TEXT("pair_%04d"), runIndex);
	}

	return baseName;
}

FString UScenarioRunnerSubsystem::SanitizeReportFileToken(const FString& value)
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
