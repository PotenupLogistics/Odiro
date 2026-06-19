
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

	bool IsTransientStartupWorld(const UWorld* world)
	{
		if (!world)
		{
			return false;
		}

		return world->GetMapName().StartsWith(TEXT("Untitled"))
			|| world->GetOutermost()->GetName().StartsWith(TEXT("/Temp/"));
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

	if (!commandLineResult.Options.bProjectRun)
	{
		return;
	}

	bSimulatorMode = true;
	bProjectRunMode = true;
	ActiveRunId = commandLineResult.Options.RunId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)
		: commandLineResult.Options.RunId;

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
		runInput.EpisodeScenarioJsonPath = episodeScenarioResult.ScenarioPath;
		runInput.ProfileJsonPath = projectRunParseResult.Paths.ProfilePath;
		runInput.bOverrideEvaluationConfig = true;
		runInput.EvaluationConfig = projectRunParseResult.Setting.EvaluationConfig;
		ActiveProjectRunInputs.Add(runInput);
	}
	if (ActiveProjectRunInputs.IsEmpty())
	{
		RequestProcessExitWithError(TEXT("Project run did not produce episode inputs."));
		return;
	}

	ActiveProjectPath = ActiveProjectRunPaths.ProjectPath;
	ActiveMapId = projectRunParseResult.Setting.MapId;
	ActiveFixedStepFps = projectRunParseResult.Setting.FixedFps;
	ActiveTimeScale = projectRunParseResult.Setting.TimeScale;
	ActiveMeasurementLogSettings = FEpisodeMeasurementLogSettings{};
	ActiveMeasurementLogSettings.bEnabled = false;

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
		*ActiveMapId,
		*ActiveProjectRunPaths.SnapshotPath,
		projectRunParseResult.Setting.EpisodeCount);
}

void USimulatorProcessSubsystem::Deinitialize()
{
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

	if (bSimulatorMode)
	{
		if (UGameInstance* gameInstance = GetGameInstance())
		{
			if (UWorld* world = gameInstance->GetWorld())
			{
				if (IsValid(world))
				{
					UGameplayStatics::SetGlobalTimeDilation(world, 1.0f);
				}
			}
		}
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

void USimulatorProcessSubsystem::HandlePostWorldInitialization(
	UWorld* world,
	const UWorld::InitializationValues initializationValues)
{
	(void)initializationValues;

	if (!bSimulatorMode || !IsValid(world))
	{
		return;
	}

	if (DoesWorldMatchMapId(world, ActiveMapId))
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

	if (!DoesWorldMatchMapId(world, ActiveMapId))
	{
		if (bMapLoadRequested)
		{
			UE_LOG(
				LogSimulatorProcess,
				Error,
				TEXT("Simulator map load 후에도 target map이 아님 | Current: %s, Target: %s"),
				*world->GetMapName(),
				*ActiveMapId);
			const FString error = FString::Printf(
				TEXT("Loaded map '%s' did not match target '%s'."),
				*world->GetMapName(),
				*ActiveMapId);
			RequestProcessExitWithError(error);
			return;
		}

		bMapLoadRequested = true;
		const FString openLevelName = NormalizeMapIdForOpenLevel(ActiveMapId);
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

	if (!DoesWorldMatchMapId(world, ActiveMapId))
	{
		UE_LOG(
			LogSimulatorProcess,
			Error,
			TEXT("Simulator run 시작 거부: 현재 map이 setup map과 다름 | Current: %s, Target: %s"),
			*world->GetMapName(),
			*ActiveMapId);
		RequestProcessExitWithError(
			FString::Printf(TEXT("Current map '%s' did not match target '%s'."), *world->GetMapName(), *ActiveMapId));
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
		RequestProcessExitWithError(TEXT("ScenarioRunnerSubsystem was not available."));
		return;
	}

	if (runnerSubsystem->IsBatchActive())
	{
		UE_LOG(
			LogSimulatorProcess,
			Warning,
			TEXT("Project run 시작 전 기존 batch 취소 | Project: %s, RunId: %s"),
			*ActiveProjectPath,
			*ActiveRunId);

		bReplacingExistingRunnerBatch = true;
		runnerSubsystem->CancelRun();
		bReplacingExistingRunnerBatch = false;
	}

	bRunStarted = true;
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
		RequestProcessExitWithError(FString::Printf(TEXT("Project run inputs failed to start: %s"), *ActiveRunId));
		return;
	}

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Project run 시작 | Project: %s, RunId: %s, MapId: %s, Episodes: %d"),
		*ActiveProjectPath,
		*ActiveRunId,
		*ActiveMapId,
		ActiveProjectRunInputs.Num());
}

void USimulatorProcessSubsystem::ConfigureRunnerSubsystem(UScenarioRunnerSubsystem* runnerSubsystem)
{
	if (!runnerSubsystem)
	{
		return;
	}

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

	ApplyTimeScale(world);

	if (UEpisodeMeasurementLogSubsystem* measurementLogSubsystem = world->GetSubsystem<UEpisodeMeasurementLogSubsystem>())
	{
		measurementLogSubsystem->ApplySettings(ActiveMeasurementLogSettings, bRestartMeasurementLog);
	}
}

void USimulatorProcessSubsystem::ApplyTimeScale(UWorld* world) const
{
	if (!IsValid(world))
	{
		return;
	}

	const float resolvedTimeScale = static_cast<float>(FMath::Max(ActiveTimeScale, 0.0001));
	UGameplayStatics::SetGlobalTimeDilation(world, resolvedTimeScale);
	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Simulator time_scale 적용 | TimeScale: %.4f"),
		resolvedTimeScale);
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
}

void USimulatorProcessSubsystem::ApplyFixedStep() const
{
	const double fixedDeltaSeconds = CalculateFixedDeltaSeconds(ActiveFixedStepFps);
	FApp::SetUseFixedTimeStep(true);
	FApp::SetFixedDeltaTime(fixedDeltaSeconds);

	UE_LOG(
		LogSimulatorProcess,
		Log,
		TEXT("Simulator fixed-step 적용 | Fps: %d, DeltaSeconds: %.6f"),
		ActiveFixedStepFps,
		fixedDeltaSeconds);
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
