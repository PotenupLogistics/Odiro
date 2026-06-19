#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/EpisodeResultTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioRunnerSubsystem.generated.h"

struct FDeliveryBotSetupCompileResult;
class UScenarioEvaluationSubsystem;
class UScenarioSimulationSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioRunnerStateChangedNative, EScenarioRunnerState);
DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioRunRecordCompletedNative, const FEpisodeRunRecord&);

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioRunnerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FScenarioRunnerStateChangedNative OnRunnerStateChanged;
	FScenarioRunRecordCompletedNative OnRunRecordCompleted;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Runner")
	bool StartBatchFromRunInputs(const TArray<FScenarioRunInput>& runInputs);

	// Starts direct run inputs under one externally-owned run id.
	bool StartBatchFromRunInputsForRun(const TArray<FScenarioRunInput>& runInputs, const FString& activeRunId);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Runner")
	void CancelRun();

	UFUNCTION(BlueprintPure, Category = "Scenario|Runner")
	EScenarioRunnerState GetRunnerState() const { return RunnerState; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Runner")
	bool IsBatchActive() const;

	UFUNCTION(BlueprintPure, Category = "Scenario|Runner")
	TArray<FEpisodeRunRecord> GetRunRecords() const { return RunRecords; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Runner")
	int32 GetCompletedRunCount() const { return RunRecords.Num(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|Runner")
	int32 GetTotalRunCount() const { return TotalRunCount; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Runner")
	FString GetCurrentPairId() const { return CurrentRecord.PairId; }

private:
	UFUNCTION()
	void HandleEpisodeEnded(FEpisodeEvaluationResult result);

	bool StartBatchFromRunInputsInternal(
		const TArray<FScenarioRunInput>& runInputs,
		const FString& activeBatchRunId);
	void SetRunnerState(EScenarioRunnerState runnerState);
	void StartNextScenario();
	void QueueStartNextScenario();
	void CompleteCurrentRecord(
		bool bSuccess,
		EEpisodeEvaluationOutcome outcome,
		EEpisodeEvaluationTerminalReason terminalReason,
		const FEpisodeEvaluationResult* evaluationResult = nullptr);

	void AppendCompileDiagnostics(const FScenarioCompileResult& compileResult);
	void AppendDeliveryBotSetupDiagnostics(const FDeliveryBotSetupCompileResult& compileResult);
	double GetRunTimeLimitSeconds(const FScenarioRunConfig& runConfig) const;
	FString BuildRunId() const;
	static FString BuildPairId(const FScenarioRunInput& runInput, int32 runIndex);

	UWorld* ResolveWorld() const;
	UScenarioSimulationSubsystem* ResolveSimulationSubsystem() const;
	UScenarioEvaluationSubsystem* ResolveEvaluationSubsystem() const;

	UPROPERTY(Transient)
	EScenarioRunnerState RunnerState = EScenarioRunnerState::Idle;

	UPROPERTY(Transient)
	TArray<FScenarioRunInput> PendingRunInputs;

	UPROPERTY(Transient)
	TArray<FEpisodeRunRecord> RunRecords;

	UPROPERTY(Transient)
	int32 TotalRunCount = 0;

	UPROPERTY(Transient)
	FString ActiveBatchRunId;

	UPROPERTY(Transient)
	FEpisodeRunRecord CurrentRecord;

	UPROPERTY(Transient)
	FScenarioRunInput CurrentRunInput;

	int32 CurrentRunIndex = INDEX_NONE;
};
