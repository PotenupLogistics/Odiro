
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

	void ConfigureExperimentMeasurementLogSettings(
		const FString& runOutputDirectory,
		FEpisodeMeasurementLogSettings& settings)
	{
		settings = FEpisodeMeasurementLogSettings{};
		settings.OutputDirectory = runOutputDirectory;
	}
}

void USimulatorProcessSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FExperimentRunCommandLineParseResult commandLineResult = FExperimentRunCommandLine::ParseCurrent();
	if (!commandLineResult.bSuccess)
	{
		for (const FScenarioSchemaDiagnostic& diagnostic : commandLineResult.Diagnostics)
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

	if (!commandLineResult.Options.bExperimentRun)
	{
		return;
	}

	bSimulatorMode = true;
	ActiveRequest = commandLineResult.Options.Request;
	ActiveRunId = ActiveRequest.RunId;

	if (ActiveRunId.IsEmpty())
	{
		ActiveRunId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	}
	ActiveRunOutputDirectory = FExperimentSettingJson::BuildExperimentRunDirectory(ActiveRequest.ExperimentRef, ActiveRunId);
	ConfigureExperimentMeasurementLogSettings(ActiveRunOutputDirectory, ActiveMeasurementLogSettings);
	InitializeStatus();
	const FExperimentSettingParseResult settingParseResult =
		FExperimentSettingJson::ParseFromFile(FExperimentSettingJson::BuildExperimentSettingPath(ActiveRequest.ExperimentRef));
	ActiveSetting = settingParseResult.Document;
	LogExperimentDiagnostics(settingParseResult);
	if (!settingParseResult.bSuccess)
	{
		UE_LOG(LogSimulatorProcess, Error, TEXT("SimulatorMode entry failed: experiment_setting parse failed | Experiment: %s"), *ActiveRequest.ExperimentRef);
		const FString diagnosticMessage = JoinExperimentSchemaDiagnostics(settingParseResult.Diagnostics);
		(void)ActiveRequest;
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

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("SimulatorMode active | Experiment: %s, RunId: %s, MapId: %s, FixedStepFps: %d"),
		*ActiveRequest.ExperimentRef,
		ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
		*ActiveSetting.Runtime.MapId,
		ActiveSetting.Runtime.FixedFps);
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

	if (DoesWorldMatchMapId(world, ActiveSetting.Runtime.MapId))
	{
		ApplyWorldSetup(world, false);
		return;
	}

	if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
	{
		FEpisodeMeasurementLogSettings disabledSettings = ActiveMeasurementLogSettings;
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

	if (!DoesWorldMatchMapId(world, ActiveSetting.Runtime.MapId))
	{
		if (bMapLoadRequested)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Simulator map load 후에도 target map이 아님 | Current: %s, Target: %s"),
				*world->GetMapName(),
				*ActiveSetting.Runtime.MapId);
			WriteStatus(
				ESimulationRunState::Failed,
				FString::Printf(TEXT("Loaded map '%s' did not match target '%s'."), *world->GetMapName(), *ActiveSetting.Runtime.MapId));
			return;
		}

		bMapLoadRequested = true;
		const FString openLevelName = NormalizeMapIdForOpenLevel(ActiveSetting.Runtime.MapId);
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
	QueueStartExperimentRun(world);
}

void USimulatorProcessSubsystem::QueueStartExperimentRun(UWorld* world)
{
	if (bRunStarted || bRunStartQueued || !IsValid(world))
	{
		return;
	}

	bRunStartQueued = true;
	world->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&USimulatorProcessSubsystem::StartExperimentRun,
			world));
}

void USimulatorProcessSubsystem::StartExperimentRun(UWorld* world)
{
	bRunStartQueued = false;
	if (bRunStarted || !bSimulatorMode || !IsValid(world))
	{
		return;
	}

	if (!DoesWorldMatchMapId(world, ActiveSetting.Runtime.MapId))
	{
		UE_LOG(
			LogSimulatorProcess,
			Error,
			TEXT("Simulator run 시작 거부: 현재 map이 setup map과 다름 | Current: %s, Target: %s"),
			*world->GetMapName(),
			*ActiveSetting.Runtime.MapId);
		WriteStatus(
			ESimulationRunState::Failed,
			FString::Printf(TEXT("Current map '%s' did not match target '%s'."), *world->GetMapName(), *ActiveSetting.Runtime.MapId));
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

	const FString RequestedSourceLabel = FString::Printf(TEXT("Experiment: %s"), *ActiveRequest.ExperimentRef);

	if (runnerSubsystem->IsBatchActive())
	{
		const FString CurrentSourceLabel = TEXT("<active batch>");
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
	const FExperimentRunInputBuildResult buildResult =
		FExperimentSettingJson::BuildRunInputsFromExperiment(ActiveRequest.ExperimentRef, ActiveRequest.SampleSelection);
	if (!buildResult.bSuccess)
	{
		bRunStarted = false;
		const FString diagnosticMessage = JoinExperimentSchemaDiagnostics(buildResult.Diagnostics);
		UE_LOG(
			LogSimulatorProcess,
			Error,
			TEXT("Simulator run 시작 실패: experiment 준비 실패 | Experiment: %s, Diagnostics: %s"),
			*ActiveRequest.ExperimentRef,
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
			*ActiveRequest.ExperimentRef);
		WriteStatus(
			ESimulationRunState::Failed,
			FString::Printf(TEXT("Experiment run inputs failed to start: %s"), *ActiveRequest.ExperimentRef));
		return;
	}
	WriteStatusFromRunnerState(runnerSubsystem->GetRunnerState());

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Simulator run started | Experiment: %s, RunId: %s, MapId: %s, Source: %s"),
		*ActiveRequest.ExperimentRef,
		ActiveRunId.IsEmpty() ? TEXT("<auto>") : *ActiveRunId,
		*ActiveSetting.Runtime.MapId,
		*RequestedSourceLabel);
}

void USimulatorProcessSubsystem::ConfigureRunnerSubsystem(UScenarioRunnerSubsystem* runnerSubsystem)
{
	if (!runnerSubsystem)
	{
		return;
	}

	runnerSubsystem->RunOutputDirectory = ActiveRunOutputDirectory;
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
		measurementLogSubsystem->ApplySettings(ActiveMeasurementLogSettings, bRestartMeasurementLog);
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
	const double fixedDeltaSeconds = CalculateFixedDeltaSeconds(ActiveSetting.Runtime.FixedFps);
	FApp::SetUseFixedTimeStep(true);
	FApp::SetFixedDeltaTime(fixedDeltaSeconds);

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Simulator fixed-step 적용 | Fps: %d, DeltaSeconds: %.6f"),
		ActiveSetting.Runtime.FixedFps,
		fixedDeltaSeconds);
}

void USimulatorProcessSubsystem::LogExperimentDiagnostics(const FExperimentSettingParseResult& parseResult) const
{
	for (const FScenarioSchemaDiagnostic& diagnostic : parseResult.Diagnostics)
	{
		if (diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("experiment_setting diagnostic | Code: %s, Message: %s"),
				*diagnostic.Code,
				*diagnostic.Message);
			continue;
		}

		UE_LOG(
			LogSimulatorProcess,
			Warning,
			TEXT("experiment_setting diagnostic | Code: %s, Message: %s"),
			*diagnostic.Code,
			*diagnostic.Message);
	}
}

void USimulatorProcessSubsystem::InitializeStatus()
{
	ActiveStatus = FSimulationRunStatus{};
	ActiveStatus.RunId = ActiveRunId;
	ActiveStatus.SetupPath = ActiveRequest.ExperimentRef;
	ActiveStatusPath = FExperimentSettingJson::BuildExperimentRunStatusPath(ActiveRequest.ExperimentRef, ActiveRunId);
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

	ActiveStatus.ResultPaths.Reset();
	for (const FEpisodeRunRecord& runRecord : runnerSubsystem->GetRunRecords())
	{
		if (!runRecord.EpisodeResultJsonPath.IsEmpty())
		{
			ActiveStatus.ResultPaths.Add(ToProjectRelativePathIfPossible(runRecord.EpisodeResultJsonPath));
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
