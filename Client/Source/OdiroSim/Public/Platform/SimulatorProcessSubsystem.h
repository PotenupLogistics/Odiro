#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Shared/EpisodeResultTypes.h"
#include "Shared/EpisodeMeasurementLogTypes.h"
#include "Shared/ExperimentSettingTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/SimulationRunStatusTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimulatorProcessSubsystem.generated.h"

class UScenarioRunnerSubsystem;

// `-Experiment=<ExperimentRef>` process를 감지하고 simulator bootstrap을 수행하는 subsystem
UCLASS(BlueprintType)
class ODIROSIM_API USimulatorProcessSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Whether this process was launched as an automated simulator run.
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	bool IsSimulatorMode() const { return bSimulatorMode; }

	// Active experiment folder used by this simulator process.
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	FString GetActiveExperimentRef() const { return ActiveRequest.ExperimentRef; }

	// Active experiment run request parsed from process command-line switches.
	FExperimentRunRequest GetActiveRequest() const { return ActiveRequest; }

	// Active experiment_setting document loaded for this simulator process.
	FExperimentSettingDocument GetActiveSetting() const { return ActiveSetting; }

	// 현재 process에 전달된 optional run id
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	FString GetActiveRunId() const { return ActiveRunId; }

	// `map_id`를 `OpenLevel`에 전달할 map name 또는 package path로 정규화
	static FString NormalizeMapIdForOpenLevel(const FString& mapId);

	// `map_id` 비교용 short map name
	static FString GetMapShortNameFromId(const FString& mapId);

	// 현재 world가 setup의 target map인지 확인
	static bool DoesWorldMatchMapId(const UWorld* world, const FString& mapId);

	// fixed-step FPS를 fixed delta seconds로 변환
	static double CalculateFixedDeltaSeconds(int32 fps);

	// EpisodeRunner lifecycle 상태를 launcher polling 상태로 변환
	static ESimulationRunState ConvertRunnerStateToRunState(EScenarioRunnerState runnerState);

private:
	void HandlePostWorldInitialization(UWorld* world, const UWorld::InitializationValues initializationValues);
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);
	void ProcessLoadedWorld(UWorld* world);
	void QueueStartExperimentRun(UWorld* world);
	void StartExperimentRun(UWorld* world);
	void ConfigureRunnerSubsystem(UScenarioRunnerSubsystem* runnerSubsystem);
	void BindRunnerDelegates(UScenarioRunnerSubsystem* runnerSubsystem);
	void UnbindRunnerDelegates();
	void ApplyWorldSetup(UWorld* world, bool bRestartMeasurementLog);
	void StopMeasurementLogging(UWorld* world, const FString& closeReason);
	void HandleRunnerStateChanged(EScenarioRunnerState runnerState);
	void HandleRunRecordCompleted(const FEpisodeRunRecord& runRecord);
	void ApplyFixedStep() const;
	void LogExperimentDiagnostics(const FExperimentSettingParseResult& parseResult) const;
	void InitializeStatus();
	void WriteStatus(ESimulationRunState state, const FString& error = FString());
	void WriteStatusFromRunnerState(EScenarioRunnerState runnerState, const FString& error = FString());
	void RefreshStatusFromRunner(const UScenarioRunnerSubsystem* runnerSubsystem);
	void RefreshStatusFromWorld(UWorld* world);

	UPROPERTY(Transient)
	bool bSimulatorMode = false;

	UPROPERTY(Transient)
	bool bMapLoadRequested = false;

	UPROPERTY(Transient)
	bool bRunStartQueued = false;

	UPROPERTY(Transient)
	bool bRunStarted = false;

	UPROPERTY(Transient)
	bool bReplacingExistingRunnerBatch = false;

	// Experiment launch request parsed from this process command line.
	UPROPERTY(Transient)
	FExperimentRunRequest ActiveRequest;

	// Loaded experiment setting used to materialize scenario/profile run inputs.
	UPROPERTY(Transient)
	FExperimentSettingDocument ActiveSetting;

	// Measurement log options derived from the active experiment setting.
	UPROPERTY(Transient)
	FEpisodeMeasurementLogSettings ActiveMeasurementLogSettings;

	// Canonical output directory for this active run.
	UPROPERTY(Transient)
	FString ActiveRunOutputDirectory;

	UPROPERTY(Transient)
	FString ActiveRunId;

	UPROPERTY(Transient)
	FSimulationRunStatus ActiveStatus;

	UPROPERTY(Transient)
	FString ActiveStatusPath;

	UPROPERTY(Transient)
	TObjectPtr<UScenarioRunnerSubsystem> BoundRunnerSubsystem;

	UPROPERTY(Transient)
	bool bStatusInitialized = false;

	UPROPERTY(Transient)
	bool bStatusTerminal = false;

	FDelegateHandle PostWorldInitializationHandle;
	FDelegateHandle PostLoadMapHandle;
};
