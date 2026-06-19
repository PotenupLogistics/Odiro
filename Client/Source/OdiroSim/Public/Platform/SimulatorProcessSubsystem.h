#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Shared/EpisodeMeasurementLogTypes.h"
#include "Shared/EpisodeResultTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/UserProjectDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimulatorProcessSubsystem.generated.h"

class UScenarioRunnerSubsystem;

// Boots simulator runtime from the public user project command-line contract.
UCLASS(BlueprintType)
class ODIROSIM_API USimulatorProcessSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Parses process command-line state and attaches project-run world delegates.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Releases world and runner delegates owned by this process subsystem.
	virtual void Deinitialize() override;

	// True when this process is running under the simulator launch contract.
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	bool IsSimulatorMode() const { return bSimulatorMode; }

	// True when this process is executing a user project run snapshot.
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	bool IsProjectRunMode() const { return bProjectRunMode; }

	// User project root resolved from -OdiroProject.
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	FString GetActiveProjectPath() const { return ActiveProjectPath; }

	// Run id resolved from -RunId or generated for this process.
	UFUNCTION(BlueprintPure, Category = "Simulation|Process")
	FString GetActiveRunId() const { return ActiveRunId; }

	// Normalizes a map id into the package path or short map name OpenLevel accepts.
	static FString NormalizeMapIdForOpenLevel(const FString& mapId);

	// Returns the short asset name portion of a map id.
	static FString GetMapShortNameFromId(const FString& mapId);

	// Checks whether the current world is the requested simulation map.
	static bool DoesWorldMatchMapId(const UWorld* world, const FString& mapId);

	// Converts a fixed-step FPS value to the engine fixed delta seconds value.
	static double CalculateFixedDeltaSeconds(int32 fps);

private:
	// Applies setup as soon as a new world is initialized.
	void HandlePostWorldInitialization(UWorld* world, const UWorld::InitializationValues initializationValues);

	// Re-runs world setup after OpenLevel finishes loading the target map.
	void HandlePostLoadMapWithWorld(UWorld* loadedWorld);

	// Validates map identity and queues the project run when the world is ready.
	void ProcessLoadedWorld(UWorld* world);

	// Defers run start to the next tick so world subsystems are initialized.
	void QueueStartSimulationRun(UWorld* world);

	// Starts the scenario runner with materialized project episode inputs.
	void StartSimulationRun(UWorld* world);

	// Applies runner delegates and future runner-owned runtime configuration.
	void ConfigureRunnerSubsystem(UScenarioRunnerSubsystem* runnerSubsystem);

	// Binds lifecycle callbacks for the active runner subsystem.
	void BindRunnerDelegates(UScenarioRunnerSubsystem* runnerSubsystem);

	// Removes lifecycle callbacks from the previously bound runner subsystem.
	void UnbindRunnerDelegates();

	// Applies per-world runtime settings owned by the project-run snapshot.
	void ApplyWorldSetup(UWorld* world, bool bRestartMeasurementLog);

	// Closes measurement logging before process exit or cancellation.
	void StopMeasurementLogging(UWorld* world, const FString& closeReason);

	// Persists project-run artifacts and requests process exit on terminal runner states.
	void HandleRunnerStateChanged(EScenarioRunnerState runnerState);

	// Writes canonical episode artifacts when the runner finishes an episode.
	void HandleRunRecordCompleted(const FEpisodeRunRecord& runRecord);

	// Applies fixed-step timing from the project runtime setting.
	void ApplyFixedStep() const;

	// Applies project runtime time_scale to the active simulation world.
	void ApplyTimeScale(UWorld* world) const;

	// Logs project-run snapshot validation diagnostics.
	void LogProjectRunDiagnostics(const FUserProjectRunSnapshotParseResult& parseResult) const;

	// Requests an abnormal process exit after a trust-boundary failure.
	void RequestProcessExitWithError(const FString& error) const;

	// Requests the process exit code used by the launcher to classify the run.
	void RequestProjectRunProcessExit(bool bSuccess, const FString& reason);

	UPROPERTY(Transient)
	// True after a valid simulator command-line contract activates this process.
	bool bSimulatorMode = false;

	UPROPERTY(Transient)
	// True when the process is executing a user project run snapshot.
	bool bProjectRunMode = false;

	UPROPERTY(Transient)
	// True after OpenLevel has been requested for the target simulation map.
	bool bMapLoadRequested = false;

	UPROPERTY(Transient)
	// True while the run start has been queued for the next world tick.
	bool bRunStartQueued = false;

	UPROPERTY(Transient)
	// True after the scenario runner accepts the project episode batch.
	bool bRunStarted = false;

	UPROPERTY(Transient)
	// Guards against treating intentional batch replacement cancellation as terminal.
	bool bReplacingExistingRunnerBatch = false;

	UPROPERTY(Transient)
	// Prevents duplicate process-exit requests from repeated terminal callbacks.
	bool bProjectRunExitRequested = false;

	UPROPERTY(Transient)
	// Target simulation map id resolved from project setting.json.
	FString ActiveMapId;

	UPROPERTY(Transient)
	// Fixed-step FPS resolved from project setting.json.
	int32 ActiveFixedStepFps = 60;

	UPROPERTY(Transient)
	// World time dilation resolved from project setting.json.
	double ActiveTimeScale = 1.0;

	UPROPERTY(Transient)
	// Measurement logging settings applied to each simulation world.
	FEpisodeMeasurementLogSettings ActiveMeasurementLogSettings;

	UPROPERTY(Transient)
	// User project root for the current project run.
	FString ActiveProjectPath;

	UPROPERTY(Transient)
	// Canonical run snapshot paths under <UserProject>/runs/<RunId>.
	FUserProjectRunSnapshotPaths ActiveProjectRunPaths;

	UPROPERTY(Transient)
	// Episode inputs materialized from the project scenario snapshot.
	TArray<FScenarioRunInput> ActiveProjectRunInputs;

	UPROPERTY(Transient)
	// Six-digit run id for the current project run.
	FString ActiveRunId;

	UPROPERTY(Transient)
	// Runner subsystem currently bound to this process subsystem.
	TObjectPtr<UScenarioRunnerSubsystem> BoundRunnerSubsystem;

	// Delegate handle for world initialization callbacks owned by this subsystem.
	FDelegateHandle PostWorldInitializationHandle;

	// Delegate handle for post-map-load callbacks owned by this subsystem.
	FDelegateHandle PostLoadMapHandle;
};
