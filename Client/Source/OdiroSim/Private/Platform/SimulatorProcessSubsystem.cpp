
#include "Platform/SimulatorProcessSubsystem.h"
#include "Episode/EpisodeMeasurementLogSubsystem.h"
#include "Scenario/ScenarioRunnerSubsystem.h"
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

	FString JoinExperimentSchemaDiagnostics(const TArray<FScenarioSchemaDiagnostic>& diagnostics)
	{
		TArray<FString> Lines;
		Lines.Reserve(diagnostics.Num());
		for (const FScenarioSchemaDiagnostic& diagnostic : diagnostics)
		{
			Lines.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}

		return FString::Join(Lines, TEXT(" | "));
	}

	bool ApplyExperimentRuntimeSettings(
		FSimulationSetup& setup,
		TArray<FScenarioSchemaDiagnostic>& outDiagnostics)
	{
		outDiagnostics.Reset();
		const FString experimentRef = setup.ExperimentRef.TrimStartAndEnd();
		if (experimentRef.IsEmpty())
		{
			return true;
		}

		const FString settingPath = FExperimentSettingJson::BuildExperimentSettingPath(experimentRef);
		const FExperimentSettingParseResult settingResult = FExperimentSettingJson::ParseFromFile(settingPath);
		outDiagnostics = settingResult.Diagnostics;
		if (!settingResult.bSuccess)
		{
			return false;
		}

		// Experiment setting owns runtime values; SimulationSetup is only the process launch envelope.
		setup.MapId = settingResult.Document.Runtime.MapId;
		setup.FixedStep.Fps = settingResult.Document.Runtime.FixedFps;
		return true;
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
		return;
	}

	if (!commandLineResult.Options.bSimulate)
	{
		return;
	}

	bSimulatorMode = true;
	ActiveSetupPath = commandLineResult.Options.SimulationSetupFile;
	ActiveRunId = commandLineResult.Options.RunId;

	const FSimulationSetupParseResult setupParseResult = FSimulationSetupJson::ParseFromFile(ActiveSetupPath);
	ActiveSetup = setupParseResult.Setup;
	if (ActiveRunId.IsEmpty())
	{
		ActiveRunId = ActiveSetup.RunId.IsEmpty()
			? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)
			: ActiveSetup.RunId;
	}
	TArray<FScenarioSchemaDiagnostic> experimentRuntimeDiagnostics;
	const bool bExperimentRuntimeSettingsLoaded = setupParseResult.bSuccess
		? ApplyExperimentRuntimeSettings(ActiveSetup, experimentRuntimeDiagnostics)
		: true;
	// Direct -Simulate launches and launcher-generated runtime setup files share the same run-folder contract.
	FSimulationSetupJson::ApplyRunOutputPaths(ActiveSetup, ActiveRunId);
	InitializeStatus();
	LogSetupDiagnostics(setupParseResult);
	if (!setupParseResult.bSuccess)
	{
		UE_LOG(LogSimulatorProcess, Error, TEXT("SimulatorMode 진입 실패: SimulationSetup 파싱 실패 | Path: %s"), *ActiveSetupPath);
		WriteStatus(ESimulationRunState::Failed, TEXT("SimulationSetup parse failed."));
		return;
	}
	if (!bExperimentRuntimeSettingsLoaded)
	{
		const FString diagnosticMessage = JoinExperimentSchemaDiagnostics(experimentRuntimeDiagnostics);
		UE_LOG(
			LogSimulatorProcess,
			Error,
			TEXT("SimulatorMode entry failed: experiment_setting parse failed | Experiment: %s, Diagnostics: %s"),
			*ActiveSetup.ExperimentRef,
			*diagnosticMessage);
		WriteStatus(
			ESimulationRunState::Failed,
			diagnosticMessage.IsEmpty() ? TEXT("experiment_setting parse failed.") : diagnosticMessage);
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

	const FString ActiveSourceLabel = !ActiveSetup.ExperimentRef.TrimStartAndEnd().IsEmpty()
		? FString::Printf(TEXT("Experiment: %s"), *ActiveSetup.ExperimentRef)
		: FString::Printf(TEXT("RunQueue: %s"), *ActiveSetup.RunQueueJsonPath);
	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("SimulatorMode active | Setup: %s, RunId: %s, MapId: %s, Source: %s, FixedStepFps: %d"),
		*ActiveSetupPath,
		ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
		*ActiveSetup.MapId,
		*ActiveSourceLabel,
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
			WriteStatus(
				ESimulationRunState::Failed,
				FString::Printf(TEXT("Loaded map '%s' did not match target '%s'."), *world->GetMapName(), *ActiveSetup.MapId));
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

	const bool bUseExperiment = !ActiveSetup.ExperimentRef.TrimStartAndEnd().IsEmpty();
	const FString RequestedSourceLabel = bUseExperiment
		? FString::Printf(TEXT("Experiment: %s"), *ActiveSetup.ExperimentRef)
		: FString::Printf(TEXT("RunQueue: %s"), *ActiveSetup.RunQueueJsonPath);
	if (!bUseExperiment && runnerSubsystem->IsRunningRunQueueJsonFile(ActiveSetup.RunQueueJsonPath))
	{
		bRunStarted = true;
		WriteStatusFromRunnerState(runnerSubsystem->GetRunnerState());
		UE_LOG(
			LogSimulatorProcess,
			Log,
			TEXT("Simulator run already active | Setup: %s, RunId: %s, MapId: %s, Source: %s"),
			*ActiveSetupPath,
			ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
			*ActiveSetup.MapId,
			*RequestedSourceLabel);
		return;
	}

	if (runnerSubsystem->IsBatchActive())
	{
		const FString CurrentSourceLabel = runnerSubsystem->GetActiveRunQueueJsonFilePath().IsEmpty()
			? FString(TEXT("<direct>"))
			: FString::Printf(TEXT("RunQueue: %s"), *runnerSubsystem->GetActiveRunQueueJsonFilePath());
		UE_LOG(
			LogSimulatorProcess,
			Warning,
			TEXT("Simulator run replacing active batch | ActiveSource: %s, RequestedSource: %s"),
			*CurrentSourceLabel,
			*RequestedSourceLabel);

		bReplacingExistingRunnerBatch = true;
		runnerSubsystem->CancelRun();
		bReplacingExistingRunnerBatch = false;
	}

	bRunStarted = true;
	if (bUseExperiment)
	{
		const FExperimentRunInputBuildResult buildResult =
			FExperimentSettingJson::BuildRunInputsFromExperiment(ActiveSetup.ExperimentRef, ActiveSetup.SampleSelection);
		if (!buildResult.bSuccess)
		{
			bRunStarted = false;
			const FString diagnosticMessage = JoinExperimentSchemaDiagnostics(buildResult.Diagnostics);
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Simulator run 시작 실패: experiment 준비 실패 | Experiment: %s, Diagnostics: %s"),
				*ActiveSetup.ExperimentRef,
				*diagnosticMessage);
			WriteStatus(
				ESimulationRunState::Failed,
				diagnosticMessage.IsEmpty() ? TEXT("Experiment preparation failed.") : diagnosticMessage);
			return;
		}

		if (!runnerSubsystem->StartBatchFromRunInputsForRun(buildResult.RunInputs, ActiveRunId))
		{
			bRunStarted = false;
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Simulator run 시작 실패: experiment run input 실행 실패 | Experiment: %s"),
				*ActiveSetup.ExperimentRef);
			WriteStatus(
				ESimulationRunState::Failed,
				FString::Printf(TEXT("Experiment run inputs failed to start: %s"), *ActiveSetup.ExperimentRef));
			return;
		}
	}
	else if (!runnerSubsystem->StartBatchFromRunQueueJsonFileForRun(ActiveSetup.RunQueueJsonPath, ActiveRunId))
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
		TEXT("Simulator run started | Setup: %s, RunId: %s, MapId: %s, Source: %s"),
		*ActiveSetupPath,
		ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
		*ActiveSetup.MapId,
		*RequestedSourceLabel);
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
	}
	else if (runnerState == EScenarioRunnerState::Failed)
	{
		if (UGameInstance* gameInstance = GetGameInstance())
		{
			StopMeasurementLogging(gameInstance->GetWorld(), TEXT("simulation_failed"));
		}
	}
	else if (runnerState == EScenarioRunnerState::Cancelled)
	{
		if (UGameInstance* gameInstance = GetGameInstance())
		{
			StopMeasurementLogging(gameInstance->GetWorld(), TEXT("simulation_canceled"));
		}
	}

	WriteStatusFromRunnerState(runnerState);
}

void USimulatorProcessSubsystem::HandleRunRecordCompleted(const FEpisodeRunRecord& runRecord)
{
	(void)runRecord;
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

void USimulatorProcessSubsystem::InitializeStatus()
{
	ActiveStatus = FSimulationRunStatus{};
	ActiveStatus.RunId = ActiveRunId;
	ActiveStatus.SetupPath = ActiveSetupPath;
	ActiveStatusPath = ActiveSetup.Status.OutputPath;
	bStatusInitialized = true;
	bStatusTerminal = false;
}

void USimulatorProcessSubsystem::WriteStatus(ESimulationRunState state, const FString& error)
{
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
		if (!runRecord.EpisodeResultJsonPath.IsEmpty())
		{
			ActiveStatus.ReportPaths.Add(ToProjectRelativePathIfPossible(runRecord.EpisodeResultJsonPath));
		}
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
