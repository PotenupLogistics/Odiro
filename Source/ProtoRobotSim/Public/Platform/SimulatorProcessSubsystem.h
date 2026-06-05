#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Shared/EpisodeConfigTypes.h"
#include "Shared/SimulationSetupTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimulatorProcessSubsystem.generated.h"

class UEpisodeRunnerSubsystem;

// `-Simulate=<SimulationSetupFile>` process를 감지하고 simulator bootstrap을 수행하는 subsystem
UCLASS(BlueprintType)
class PROTOROBOTSIM_API USimulatorProcessSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 현재 process가 `-Simulate` 계약으로 실행 중이면 true
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	bool IsSimulatorMode() const { return bSimulatorMode; }

	// SimulatorMode에서 읽은 setup 값
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	FSimulationSetup GetActiveSetup() const { return ActiveSetup; }

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
	static ESimulationRunState ConvertRunnerStateToRunState(EEpisodeRunnerState runnerState);

private:
	void HandlePostWorldInitialization(UWorld* world, const UWorld::InitializationValues initializationValues);
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);
	void ProcessLoadedWorld(UWorld* world);
	void QueueStartSimulationRun(UWorld* world);
	void StartSimulationRun(UWorld* world);
	void ConfigureRunnerSubsystem(UEpisodeRunnerSubsystem* runnerSubsystem);
	void BindRunnerDelegates(UEpisodeRunnerSubsystem* runnerSubsystem);
	void UnbindRunnerDelegates();
	void ApplyWorldSetup(UWorld* world, bool bRestartMeasurementLog);
	void StopMeasurementLogging(UWorld* world, const FString& closeReason);
	void HandleRunnerStateChanged(EEpisodeRunnerState runnerState);
	void HandleRunRecordCompleted(const FEpisodeRunRecord& runRecord);
	void ApplyFixedStep() const;
	void LogSetupDiagnostics(const FSimulationSetupParseResult& parseResult) const;
	void InitializeStatus();
	void WriteStatus(ESimulationRunState state, const FString& error = FString());
	void WriteStatusFromRunnerState(EEpisodeRunnerState runnerState, const FString& error = FString());
	void RefreshStatusFromRunner(const UEpisodeRunnerSubsystem* runnerSubsystem);
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

	UPROPERTY(Transient)
	FSimulationSetup ActiveSetup;

	UPROPERTY(Transient)
	FString ActiveSetupPath;

	UPROPERTY(Transient)
	FString ActiveRunId;

	UPROPERTY(Transient)
	FSimulationRunStatus ActiveStatus;

	UPROPERTY(Transient)
	FString ActiveStatusPath;

	UPROPERTY(Transient)
	TObjectPtr<UEpisodeRunnerSubsystem> BoundRunnerSubsystem;

	UPROPERTY(Transient)
	bool bStatusInitialized = false;

	UPROPERTY(Transient)
	bool bStatusTerminal = false;

	FDelegateHandle PostWorldInitializationHandle;
	FDelegateHandle PostLoadMapHandle;
};
