
#include "Platform/SimulatorProcessSubsystem.h"
#include "Episode/EpisodeMeasurementLogSubsystem.h"
#include "HAL/PlatformMisc.h"
#include "Scenario/ScenarioRunnerSubsystem.h"
#include "Shared/UserProjectDataTypes.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimulatorProcess, Log, All);

namespace
{
	const TCHAR* DefaultSimulationMapId = TEXT("ScenarioSimulationMap");

	FString TrimMapId(const FString& mapId)
	{
		const FString trimmedMapId = mapId.TrimStartAndEnd();
		return trimmedMapId.IsEmpty() ? FString(DefaultSimulationMapId) : trimmedMapId;
	}

	FString MakeUtcTimestamp()
	{
		return FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ"));
	}

	bool IsTerminalRunState(ESimulationRunState state)
	{
		return state == ESimulationRunState::Completed
			|| state == ESimulationRunState::Failed
			|| state == ESimulationRunState::Canceled;
	}

	bool IsTransientStartupWorld(const UWorld* world)
	{
		if (!world)
		{
			return false;
		}

		return world->GetMapName().StartsWith(TEXT("Untitled"))
			|| world->GetOutermost()->GetName().StartsWith(TEXT("/Temp/"));
	}

	bool HasFailedRunRecord(const UScenarioRunnerSubsystem* runnerSubsystem)
	{
		if (!runnerSubsystem)
		{
			return false;
		}

		for (const FEpisodeRunRecord& runRecord : runnerSubsystem->GetRunRecords())
		{
			if (!runRecord.bSuccess)
			{
				return true;
			}
		}

		return false;
	}

	FString ToProjectRelativePathIfPossible(FString filePath)
	{
		if (filePath.IsEmpty())
		{
			return filePath;
		}

		filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (FPaths::IsRelative(filePath))
		{
			return filePath;
		}

		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FString relativePath = filePath;
		if (FPaths::MakePathRelativeTo(relativePath, *projectDir))
		{
			relativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			return relativePath;
		}

		return filePath;
	}
}

void USimulatorProcessSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
	if (!commandLineResult.bSuccess)
	{
		for (const FScenarioCompileDiagnostic& diagnostic : commandLineResult.Diagnostics)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Simulator command line 진단 | Code: %s, Message: %s"),
				*diagnostic.Code,
				*diagnostic.Message);
		}
		RequestProcessExitWithError(TEXT("Simulator command line parse failed."));
		return;
	}

	if (!commandLineResult.Options.bSimulate && !commandLineResult.Options.bProjectRun)
	{
		return;
	}

	bSimulatorMode = true;
	bProjectRunMode = commandLineResult.Options.bProjectRun;
	ActiveRunId = commandLineResult.Options.RunId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)
		: commandLineResult.Options.RunId;

	if (bProjectRunMode)
	{
		ActiveProjectPath = commandLineResult.Options.ProjectPath;
		const FUserProjectRunSnapshotParseResult projectRunParseResult =
			FUserProjectRunSnapshot::Parse(ActiveProjectPath, ActiveRunId);
		ActiveProjectRunPaths = projectRunParseResult.Paths;
		LogProjectRunDiagnostics(projectRunParseResult);
		if (!projectRunParseResult.bSuccess)
		{
			RequestProcessExitWithError(TEXT("User project run snapshot validation failed."));
			return;
		}

		TArray<FUserProjectEpisodeScenarioWriteResult> episodeScenarioResults;
		TArray<FScenarioCompileDiagnostic> episodeScenarioDiagnostics;
		ActiveProjectRunInputs.Reset();
		if (!FUserProjectEpisodeScenarioJson::WriteAllEpisodeScenarios(
				projectRunParseResult.Paths,
				projectRunParseResult.Setting,
				episodeScenarioResults,
				episodeScenarioDiagnostics))
		{
			for (const FScenarioCompileDiagnostic& diagnostic : episodeScenarioDiagnostics)
			{
				UE_LOG(
					LogSimulatorProcess,
					Error,
					TEXT("EpisodeScenario 생성 진단 | Code: %s, Message: %s"),
					*diagnostic.Code,
					*diagnostic.Message);
			}
			RequestProcessExitWithError(TEXT("EpisodeScenario generation failed."));
			return;
		}
		for (const FUserProjectEpisodeScenarioWriteResult& episodeScenarioResult : episodeScenarioResults)
		{
			FScenarioRunInput runInput;
			runInput.PairId = episodeScenarioResult.EpisodeId;
			runInput.ScenarioSetupJsonPath = episodeScenarioResult.ScenarioPath;
			runInput.DeliveryBotSetupJsonPath = projectRunParseResult.Paths.ProfilePath;
			ActiveProjectRunInputs.Add(runInput);
		}
		if (ActiveProjectRunInputs.IsEmpty())
		{
			RequestProcessExitWithError(TEXT("Project run did not produce episode inputs."));
			return;
		}

		ActiveProjectPath = ActiveProjectRunPaths.ProjectPath;
		ActiveSetup = FSimulationSetup{};
		ActiveSetup.MapId = projectRunParseResult.Setting.MapId;
		ActiveSetup.FixedStep.Fps = projectRunParseResult.Setting.FixedFps;
		ActiveSetup.MeasurementLog.bEnabled = false;
		ActiveSetup.Report.bSaveEvaluationReportJson = false;
		ActiveSetup.Report.OutputDirectory = ActiveProjectRunPaths.RunPath;

		ApplyFixedStep();

		PostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
			this,
			&USimulatorProcessSubsystem::HandlePostWorldInitialization);
		PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
			this,
			&USimulatorProcessSubsystem::HandlePostLoadMapWithWorld);

		if (UGameInstance* gameInstance = GetGameInstance())
		{
			if (UWorld* world = gameInstance->GetWorld())
			{
				ProcessLoadedWorld(world);
			}
		}

		UE_LOG(
			LogSimulatorProcess,
			Log,
			TEXT("ProjectRun 활성화 | Project: %s, RunId: %s, MapId: %s, Snapshot: %s, EpisodeCount: %d"),
			*ActiveProjectPath,
			*ActiveRunId,
			*ActiveSetup.MapId,
			*ActiveProjectRunPaths.SnapshotPath,
			projectRunParseResult.Setting.EpisodeCount);
		return;
	}

	ActiveSetupPath = commandLineResult.Options.SimulationSetupFile;

	const FSimulationSetupParseResult setupParseResult = FSimulationSetupJson::ParseFromFile(ActiveSetupPath);
	ActiveSetup = setupParseResult.Setup;
	// Direct -Simulate launches and launcher-generated runtime setup files share the same run-folder contract.
	FSimulationSetupJson::ApplyRunOutputPaths(ActiveSetup, ActiveRunId);
	InitializeStatus();
	LogSetupDiagnostics(setupParseResult);
	if (!setupParseResult.bSuccess)
	{
		UE_LOG(LogSimulatorProcess, Error, TEXT("SimulatorMode 진입 실패: SimulationSetup 파싱 실패 | Path: %s"), *ActiveSetupPath);
		WriteStatus(ESimulationRunState::Failed, TEXT("SimulationSetup parse failed."));
		RequestProcessExitWithError(TEXT("SimulationSetup parse failed."));
		return;
	}

	if (UGameInstance* gameInstance = GetGameInstance())
	{
		ConfigureRunnerSubsystem(gameInstance->GetSubsystem<UScenarioRunnerSubsystem>());
	}

	ApplyFixedStep();

	PostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this,
		&USimulatorProcessSubsystem::HandlePostWorldInitialization);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&USimulatorProcessSubsystem::HandlePostLoadMapWithWorld);

	if (UGameInstance* gameInstance = GetGameInstance())
	{
		if (UWorld* world = gameInstance->GetWorld())
		{
			ProcessLoadedWorld(world);
		}
	}

	WriteStatus(ESimulationRunState::Pending);

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("SimulatorMode 활성화 | Setup: %s, RunId: %s, MapId: %s, RunQueue: %s, FixedStepFps: %d"),
		*ActiveSetupPath,
		ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
		*ActiveSetup.MapId,
		*ActiveSetup.RunQueueJsonPath,
		ActiveSetup.FixedStep.Fps);
}

void USimulatorProcessSubsystem::Deinitialize()
{
	if (bSimulatorMode && !bStatusTerminal && bStatusInitialized)
	{
		WriteStatus(ESimulationRunState::Canceled, TEXT("Simulator process deinitialized before completion."));
	}

	UnbindRunnerDelegates();

	if (PostWorldInitializationHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
		PostWorldInitializationHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();
}

FString USimulatorProcessSubsystem::NormalizeMapIdForOpenLevel(const FString& mapId)
{
	FString normalizedMapId = TrimMapId(mapId);
	int32 objectNameSeparatorIndex = INDEX_NONE;
	if (normalizedMapId.FindChar(TEXT('.'), objectNameSeparatorIndex))
	{
		normalizedMapId = normalizedMapId.Left(objectNameSeparatorIndex);
	}

	return normalizedMapId;
}

FString USimulatorProcessSubsystem::GetMapShortNameFromId(const FString& mapId)
{
	return FPackageName::GetShortName(NormalizeMapIdForOpenLevel(mapId));
}

bool USimulatorProcessSubsystem::DoesWorldMatchMapId(const UWorld* world, const FString& mapId)
{
	if (!world)
	{
		return false;
	}

	FString worldMapName = world->GetMapName();
	if (!world->StreamingLevelsPrefix.IsEmpty())
	{
		worldMapName.RemoveFromStart(world->StreamingLevelsPrefix);
	}

	return worldMapName.Equals(GetMapShortNameFromId(mapId), ESearchCase::IgnoreCase);
}

double USimulatorProcessSubsystem::CalculateFixedDeltaSeconds(int32 fps)
{
	return 1.0 / static_cast<double>(FMath::Max(1, fps));
}

ESimulationRunState USimulatorProcessSubsystem::ConvertRunnerStateToRunState(EScenarioRunnerState runnerState)
{
	switch (runnerState)
	{
	case EScenarioRunnerState::Preparing:
	case EScenarioRunnerState::Running:
	case EScenarioRunnerState::Ending:
		return ESimulationRunState::Running;
	case EScenarioRunnerState::Completed:
		return ESimulationRunState::Completed;
	case EScenarioRunnerState::Failed:
		return ESimulationRunState::Failed;
	case EScenarioRunnerState::Cancelled:
		return ESimulationRunState::Canceled;
	case EScenarioRunnerState::Idle:
	default:
		return ESimulationRunState::Pending;
	}
}

void USimulatorProcessSubsystem::HandlePostWorldInitialization(
	UWorld* world,
	const UWorld::InitializationValues initializationValues)
{
	(void)initializationValues;

	if (!bSimulatorMode || !IsValid(world))
	{
		return;
	}

	if (DoesWorldMatchMapId(world, ActiveSetup.MapId))
	{
		ApplyWorldSetup(world, false);
		return;
	}

	if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
	{
		FEpisodeMeasurementLogSettings disabledSettings = ActiveSetup.MeasurementLog;
		disabledSettings.bEnabled = false;
		measurementLogSubsystem->ApplySettings(disabledSettings, true);
	}
}

void USimulatorProcessSubsystem::HandlePostLoadMapWithWorld(UWorld* loadedWorld)
{
	ProcessLoadedWorld(loadedWorld);
}

void USimulatorProcessSubsystem::ProcessLoadedWorld(UWorld* world)
{
	if (!bSimulatorMode || bRunStarted || !IsValid(world))
	{
		return;
	}

	if (IsTransientStartupWorld(world))
	{
		return;
	}

	if (!DoesWorldMatchMapId(world, ActiveSetup.MapId))
	{
		if (bMapLoadRequested)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Simulator map load 후에도 target map이 아님 | Current: %s, Target: %s"),
				*world->GetMapName(),
				*ActiveSetup.MapId);
			const FString error = FString::Printf(
				TEXT("Loaded map '%s' did not match target '%s'."),
				*world->GetMapName(),
				*ActiveSetup.MapId);
			if (bProjectRunMode)
			{
				RequestProcessExitWithError(error);
			}
			else
			{
				WriteStatus(ESimulationRunState::Failed, error);
			}
			return;
		}

		bMapLoadRequested = true;
		const FString openLevelName = NormalizeMapIdForOpenLevel(ActiveSetup.MapId);
		UE_LOG(
			LogSimulatorProcess,
			Log,
			TEXT("Simulator target map 로드 요청 | Current: %s, Target: %s"),
			*world->GetMapName(),
			*openLevelName);
		UGameplayStatics::OpenLevel(world, FName(*openLevelName));
		return;
	}

	ApplyWorldSetup(world, true);
	QueueStartSimulationRun(world);
}

void USimulatorProcessSubsystem::QueueStartSimulationRun(UWorld* world)
{
	if (bRunStarted || bRunStartQueued || !IsValid(world))
	{
		return;
	}

	bRunStartQueued = true;
	world->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&USimulatorProcessSubsystem::StartSimulationRun,
			world));
}

void USimulatorProcessSubsystem::StartSimulationRun(UWorld* world)
{
	bRunStartQueued = false;
	if (bRunStarted || !bSimulatorMode || !IsValid(world))
	{
		return;
	}

	if (!DoesWorldMatchMapId(world, ActiveSetup.MapId))
	{
		UE_LOG(
			LogSimulatorProcess,
			Error,
			TEXT("Simulator run 시작 거부: 현재 map이 setup map과 다름 | Current: %s, Target: %s"),
			*world->GetMapName(),
			*ActiveSetup.MapId);
		WriteStatus(
			ESimulationRunState::Failed,
			FString::Printf(TEXT("Current map '%s' did not match target '%s'."), *world->GetMapName(), *ActiveSetup.MapId));
		return;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UScenarioRunnerSubsystem* runnerSubsystem = gameInstance
		? gameInstance->GetSubsystem<UScenarioRunnerSubsystem>()
		: nullptr;
	ConfigureRunnerSubsystem(runnerSubsystem);
	if (!runnerSubsystem)
	{
		UE_LOG(LogSimulatorProcess, Error, TEXT("Simulator run 시작 실패: ScenarioRunnerSubsystem 없음"));
		WriteStatus(ESimulationRunState::Failed, TEXT("ScenarioRunnerSubsystem was not available."));
		return;
	}

	if (!bProjectRunMode && runnerSubsystem->IsRunningRunQueueJsonFile(ActiveSetup.RunQueueJsonPath))
	{
		bRunStarted = true;
		WriteStatusFromRunnerState(runnerSubsystem->GetRunnerState());
		UE_LOG(
			LogSimulatorProcess,
			Log,
			TEXT("Simulator run 이미 진행 중 | Setup: %s, RunId: %s, MapId: %s, RunQueue: %s"),
			*ActiveSetupPath,
			ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
			*ActiveSetup.MapId,
			*ActiveSetup.RunQueueJsonPath);
		return;
	}

	if (runnerSubsystem->IsBatchActive())
	{
		UE_LOG(
			LogSimulatorProcess,
			Warning,
			TEXT("Simulator run 시작 전 기존 batch 취소 | ActiveRunQueue: %s, RequestedRunQueue: %s"),
			runnerSubsystem->GetActiveRunQueueJsonFilePath().IsEmpty()
				? TEXT("<direct>")
				: *runnerSubsystem->GetActiveRunQueueJsonFilePath(),
			*ActiveSetup.RunQueueJsonPath);

		bReplacingExistingRunnerBatch = true;
		runnerSubsystem->CancelRun();
		bReplacingExistingRunnerBatch = false;
	}

	bRunStarted = true;
	if (bProjectRunMode)
	{
		if (!runnerSubsystem->StartBatchFromRunInputsForRun(ActiveProjectRunInputs, ActiveRunId))
		{
			bRunStarted = false;
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Project run 시작 실패: episode input 실행 실패 | Project: %s, RunId: %s, Count: %d"),
				*ActiveProjectPath,
				*ActiveRunId,
				ActiveProjectRunInputs.Num());
			WriteStatus(
				ESimulationRunState::Failed,
				FString::Printf(TEXT("Project run inputs failed to start: %s"), *ActiveRunId));
			return;
		}

		WriteStatusFromRunnerState(runnerSubsystem->GetRunnerState());

		UE_LOG(
			LogSimulatorProcess,
			Log,
			TEXT("Project run 시작 | Project: %s, RunId: %s, MapId: %s, Episodes: %d"),
			*ActiveProjectPath,
			*ActiveRunId,
			*ActiveSetup.MapId,
			ActiveProjectRunInputs.Num());
		return;
	}

	if (!runnerSubsystem->StartBatchFromRunQueueJsonFileForRun(ActiveSetup.RunQueueJsonPath, ActiveRunId))
	{
		bRunStarted = false;
		UE_LOG(
			LogSimulatorProcess,
			Error,
			TEXT("Simulator run 시작 실패: run queue 실행 실패 | RunQueue: %s"),
			*ActiveSetup.RunQueueJsonPath);
		WriteStatus(
			ESimulationRunState::Failed,
			FString::Printf(TEXT("Run queue failed to start: %s"), *ActiveSetup.RunQueueJsonPath));
		return;
	}

	WriteStatusFromRunnerState(runnerSubsystem->GetRunnerState());

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Simulator run 시작 | Setup: %s, RunId: %s, MapId: %s, RunQueue: %s"),
		*ActiveSetupPath,
		ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
		*ActiveSetup.MapId,
		*ActiveSetup.RunQueueJsonPath);
}

void USimulatorProcessSubsystem::ConfigureRunnerSubsystem(UScenarioRunnerSubsystem* runnerSubsystem)
{
	if (!runnerSubsystem)
	{
		return;
	}

	runnerSubsystem->bSaveEvaluationReportJson = ActiveSetup.Report.bSaveEvaluationReportJson;
	runnerSubsystem->EvaluationReportOutputDirectory = ActiveSetup.Report.OutputDirectory;
	BindRunnerDelegates(runnerSubsystem);
}

void USimulatorProcessSubsystem::BindRunnerDelegates(UScenarioRunnerSubsystem* runnerSubsystem)
{
	if (!runnerSubsystem || BoundRunnerSubsystem == runnerSubsystem)
	{
		return;
	}

	UnbindRunnerDelegates();
	runnerSubsystem->OnRunnerStateChanged.AddUObject(this, &USimulatorProcessSubsystem::HandleRunnerStateChanged);
	runnerSubsystem->OnRunRecordCompleted.AddUObject(this, &USimulatorProcessSubsystem::HandleRunRecordCompleted);
	BoundRunnerSubsystem = runnerSubsystem;
}

void USimulatorProcessSubsystem::UnbindRunnerDelegates()
{
	if (!BoundRunnerSubsystem)
	{
		return;
	}

	BoundRunnerSubsystem->OnRunnerStateChanged.RemoveAll(this);
	BoundRunnerSubsystem->OnRunRecordCompleted.RemoveAll(this);
	BoundRunnerSubsystem = nullptr;
}

void USimulatorProcessSubsystem::ApplyWorldSetup(UWorld* world, bool bRestartMeasurementLog)
{
	if (!IsValid(world))
	{
		return;
	}

	if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
	{
		measurementLogSubsystem->ApplySettings(ActiveSetup.MeasurementLog, bRestartMeasurementLog);
	}
}

void USimulatorProcessSubsystem::StopMeasurementLogging(UWorld* world, const FString& closeReason)
{
	if (!IsValid(world))
	{
		return;
	}

	if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
	{
		measurementLogSubsystem->StopLogging(closeReason);
	}
}

void USimulatorProcessSubsystem::HandleRunnerStateChanged(EScenarioRunnerState runnerState)
{
	if (bReplacingExistingRunnerBatch && runnerState == EScenarioRunnerState::Cancelled)
	{
		return;
	}

	if (runnerState == EScenarioRunnerState::Completed)
	{
		if (UGameInstance* gameInstance = GetGameInstance())
		{
			StopMeasurementLogging(gameInstance->GetWorld(), TEXT("simulation_completed"));
		}
		if (bProjectRunMode && BoundRunnerSubsystem)
		{
			TArray<FString> diagnostics;
			if (!FUserProjectRunOutputJson::SaveRunSummary(
					ActiveProjectRunPaths,
					BoundRunnerSubsystem->GetRunRecords(),
					diagnostics))
			{
				for (const FString& diagnostic : diagnostics)
				{
					UE_LOG(LogSimulatorProcess, Warning, TEXT("ProjectRun summary 저장 진단 | %s"), *diagnostic);
				}
			}
		}
	}
	else if (runnerState == EScenarioRunnerState::Failed)
	{
		if (UGameInstance* gameInstance = GetGameInstance())
		{
			StopMeasurementLogging(gameInstance->GetWorld(), TEXT("simulation_failed"));
		}
		if (bProjectRunMode && BoundRunnerSubsystem)
		{
			TArray<FString> diagnostics;
			if (!FUserProjectRunOutputJson::SaveRunSummary(
					ActiveProjectRunPaths,
					BoundRunnerSubsystem->GetRunRecords(),
					diagnostics))
			{
				for (const FString& diagnostic : diagnostics)
				{
					UE_LOG(LogSimulatorProcess, Warning, TEXT("ProjectRun summary 저장 진단 | %s"), *diagnostic);
				}
			}
		}
	}
	else if (runnerState == EScenarioRunnerState::Cancelled)
	{
		if (UGameInstance* gameInstance = GetGameInstance())
		{
			StopMeasurementLogging(gameInstance->GetWorld(), TEXT("simulation_canceled"));
		}
		if (bProjectRunMode && BoundRunnerSubsystem)
		{
			TArray<FString> diagnostics;
			if (!FUserProjectRunOutputJson::SaveRunSummary(
					ActiveProjectRunPaths,
					BoundRunnerSubsystem->GetRunRecords(),
					diagnostics))
			{
				for (const FString& diagnostic : diagnostics)
				{
					UE_LOG(LogSimulatorProcess, Warning, TEXT("ProjectRun summary 저장 진단 | %s"), *diagnostic);
				}
			}
		}
	}

	WriteStatusFromRunnerState(runnerState);
	if (bProjectRunMode && runnerState == EScenarioRunnerState::Completed)
	{
		RequestProjectRunProcessExit(true, TEXT("completed"));
	}
	else if (bProjectRunMode && runnerState == EScenarioRunnerState::Failed)
	{
		RequestProjectRunProcessExit(false, TEXT("failed"));
	}
	else if (bProjectRunMode && runnerState == EScenarioRunnerState::Cancelled)
	{
		RequestProjectRunProcessExit(false, TEXT("cancelled"));
	}
}

void USimulatorProcessSubsystem::HandleRunRecordCompleted(const FEpisodeRunRecord& runRecord)
{
	if (bProjectRunMode)
	{
		TArray<FString> diagnostics;
		if (!FUserProjectRunOutputJson::SaveEpisodeArtifacts(ActiveProjectRunPaths, runRecord, diagnostics))
		{
			for (const FString& diagnostic : diagnostics)
			{
				UE_LOG(LogSimulatorProcess, Warning, TEXT("ProjectRun episode artifact 저장 진단 | %s"), *diagnostic);
			}
		}
	}
	WriteStatus(ESimulationRunState::Running);
}

void USimulatorProcessSubsystem::ApplyFixedStep() const
{
	const double fixedDeltaSeconds = CalculateFixedDeltaSeconds(ActiveSetup.FixedStep.Fps);
	FApp::SetUseFixedTimeStep(true);
	FApp::SetFixedDeltaTime(fixedDeltaSeconds);

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Simulator fixed-step 적용 | Fps: %d, DeltaSeconds: %.6f"),
		ActiveSetup.FixedStep.Fps,
		fixedDeltaSeconds);
}

void USimulatorProcessSubsystem::LogSetupDiagnostics(const FSimulationSetupParseResult& parseResult) const
{
	for (const FScenarioCompileDiagnostic& diagnostic : parseResult.Diagnostics)
	{
		if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("SimulationSetup 진단 | Code: %s, Message: %s"),
				*diagnostic.Code,
				*diagnostic.Message);
			continue;
		}

		UE_LOG(
			LogSimulatorProcess,
			Warning,
			TEXT("SimulationSetup 진단 | Code: %s, Message: %s"),
			*diagnostic.Code,
			*diagnostic.Message);
	}
}

void USimulatorProcessSubsystem::LogProjectRunDiagnostics(const FUserProjectRunSnapshotParseResult& parseResult) const
{
	for (const FScenarioCompileDiagnostic& diagnostic : parseResult.Diagnostics)
	{
		if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("ProjectRun snapshot 진단 | Code: %s, Message: %s"),
				*diagnostic.Code,
				*diagnostic.Message);
			continue;
		}

		UE_LOG(
			LogSimulatorProcess,
			Warning,
			TEXT("ProjectRun snapshot 진단 | Code: %s, Message: %s"),
			*diagnostic.Code,
			*diagnostic.Message);
	}
}

void USimulatorProcessSubsystem::RequestProcessExitWithError(const FString& error) const
{
	UE_LOG(LogSimulatorProcess, Error, TEXT("%s"), *error);
	FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("USimulatorProcessSubsystem::RequestProcessExitWithError"));
}

void USimulatorProcessSubsystem::RequestProjectRunProcessExit(bool bSuccess, const FString& reason)
{
	if (bProjectRunExitRequested)
	{
		return;
	}

	bProjectRunExitRequested = true;
	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Project run process 종료 요청 | Success: %s, Reason: %s"),
		bSuccess ? TEXT("true") : TEXT("false"),
		*reason);
	FPlatformMisc::RequestExitWithStatus(
		false,
		bSuccess ? 0 : 1,
		TEXT("USimulatorProcessSubsystem::RequestProjectRunProcessExit"));
}

void USimulatorProcessSubsystem::InitializeStatus()
{
	ActiveStatus = FSimulationRunStatus{};
	ActiveStatus.RunId = ActiveRunId;
	ActiveStatus.SetupPath = bProjectRunMode ? ActiveProjectRunPaths.SnapshotPath : ActiveSetupPath;
	ActiveStatusPath = ActiveSetup.Status.OutputPath;
	bStatusInitialized = true;
	bStatusTerminal = false;
}

void USimulatorProcessSubsystem::WriteStatus(ESimulationRunState state, const FString& error)
{
	if (bProjectRunMode)
	{
		// Project run process 생명주기 status는 Bridge가 소유한다.
		return;
	}

	if (!bStatusInitialized)
	{
		return;
	}

	ActiveStatus.State = state;
	ActiveStatus.Error = error;
	ActiveStatus.UpdatedAt = MakeUtcTimestamp();

	if (BoundRunnerSubsystem)
	{
		RefreshStatusFromRunner(BoundRunnerSubsystem);
	}

	if (UGameInstance* gameInstance = GetGameInstance())
	{
		RefreshStatusFromWorld(gameInstance->GetWorld());
	}

	TArray<FString> diagnostics;
	if (!FSimulationRunStatusJson::SaveToFile(ActiveStatus, ActiveStatusPath, diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogSimulatorProcess, Warning, TEXT("SimulationRunStatus 저장 진단 | %s"), *diagnostic);
		}
	}

	bStatusTerminal = IsTerminalRunState(state);
}

void USimulatorProcessSubsystem::WriteStatusFromRunnerState(EScenarioRunnerState runnerState, const FString& error)
{
	ESimulationRunState runState = ConvertRunnerStateToRunState(runnerState);
	FString statusError = error;
	if (runState == ESimulationRunState::Completed && HasFailedRunRecord(BoundRunnerSubsystem))
	{
		runState = ESimulationRunState::Failed;
		if (statusError.IsEmpty())
		{
			statusError = TEXT("One or more episode runs failed.");
		}
	}

	WriteStatus(runState, statusError);
}

void USimulatorProcessSubsystem::RefreshStatusFromRunner(const UScenarioRunnerSubsystem* runnerSubsystem)
{
	if (!runnerSubsystem)
	{
		return;
	}

	ActiveStatus.CompletedRuns = runnerSubsystem->GetCompletedRunCount();
	ActiveStatus.TotalRuns = runnerSubsystem->GetTotalRunCount();
	ActiveStatus.CurrentPairId = ConvertRunnerStateToRunState(runnerSubsystem->GetRunnerState()) == ESimulationRunState::Running
		? runnerSubsystem->GetCurrentPairId()
		: FString();

	ActiveStatus.ReportPaths.Reset();
	for (const FEpisodeRunRecord& runRecord : runnerSubsystem->GetRunRecords())
	{
		if (!runRecord.EvaluationReportJsonPath.IsEmpty())
		{
			ActiveStatus.ReportPaths.Add(ToProjectRelativePathIfPossible(runRecord.EvaluationReportJsonPath));
		}
	}
}

void USimulatorProcessSubsystem::RefreshStatusFromWorld(UWorld* world)
{
	if (!IsValid(world))
	{
		return;
	}

	if (const UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
	{
		const FString currentLogPath = measurementLogSubsystem->GetCurrentLogPath();
		if (!currentLogPath.IsEmpty())
		{
			ActiveStatus.LogPaths.Reset();
			ActiveStatus.LogPaths.Add(ToProjectRelativePathIfPossible(currentLogPath));
		}
	}
}
