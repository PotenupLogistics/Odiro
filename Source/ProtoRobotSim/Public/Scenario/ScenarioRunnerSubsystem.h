#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/EpisodeConfigTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioRunnerSubsystem.generated.h"

struct FDeliveryBotSetupCompileResult;
class UScenarioEvaluationSubsystem;
class UScenarioSimulationSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioRunnerStateChangedNative, EScenarioRunnerState);
DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioRunRecordCompletedNative, const FEpisodeRunRecord&);

// Episode compile, setup, evaluation을 순차 실행하고 최종 FEpisodeRunRecord를 수집하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioRunnerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FScenarioRunnerStateChangedNative OnRunnerStateChanged;
	FScenarioRunRecordCompletedNative OnRunRecordCompleted;

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartEpisodePairFromJsonFiles(const FString& episodeSetupJsonPath, const FString& deliveryBotSetupJsonPath);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartBatchFromRunInputs(const TArray<FScenarioRunInput>& runInputs);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartBatchFromRunQueueJsonFile(const FString& runQueueJsonFilePath);

	bool StartBatchFromRunQueueJsonFileForRun(const FString& runQueueJsonFilePath, const FString& activeRunId);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	void CancelRun();

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	EScenarioRunnerState GetRunnerState() const { return RunnerState; }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	bool IsBatchActive() const;

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	FString GetActiveRunQueueJsonFilePath() const { return ActiveRunQueueJsonFilePath; }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	bool IsRunningRunQueueJsonFile(const FString& runQueueJsonFilePath) const;

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	TArray<FEpisodeRunRecord> GetRunRecords() const { return RunRecords; }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	int32 GetCompletedRunCount() const { return RunRecords.Num(); }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	int32 GetTotalRunCount() const { return TotalRunCount; }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	FString GetCurrentPairId() const { return CurrentRecord.PairId; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Runner|Report")
	bool bSaveEvaluationReportJson = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Runner|Report")
	FString EvaluationReportOutputDirectory = TEXT("Json/Output");

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool BuildLatestEvaluationReportJson(FString& outJson) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool BuildEvaluationReportJson(int32 runRecordIndex, FString& outJson) const;

private:
	UFUNCTION()
	void HandleEpisodeEnded(FEpisodeEvaluationResult result);

	bool StartBatchFromRunInputsInternal(
		const TArray<FScenarioRunInput>& runInputs,
		const FString& activeRunQueueJsonFilePath,
		const FString& activeBatchRunId);
	void SetRunnerState(EScenarioRunnerState runnerState);
	void StartNextEpisode();
	void QueueStartNextEpisode();
	void CompleteCurrentRecord(
		bool bSuccess,
		EEpisodeEvaluationOutcome outcome,
		EEpisodeEvaluationTerminalReason terminalReason,
		const FEpisodeEvaluationResult* evaluationResult = nullptr);

	void AppendCompileDiagnostics(const FScenarioCompileResult& compileResult);
	void AppendDeliveryBotSetupDiagnostics(const FDeliveryBotSetupCompileResult& compileResult);
	double GetRunTimeLimitSeconds(const FScenarioRunConfig& runConfig) const;
	FString BuildRunId() const;
	bool SaveEvaluationReportJsonForRecord(FEpisodeRunRecord& runRecord) const;
	FString BuildEvaluationReportJsonFilePath(const FEpisodeRunRecord& runRecord) const;
	static FString BuildPairId(const FScenarioRunInput& runInput, int32 runIndex);
	static FString SanitizeReportFileToken(const FString& value);

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
	FString ActiveRunQueueJsonFilePath;

	UPROPERTY(Transient)
	FString ActiveBatchRunId;

	UPROPERTY(Transient)
	FEpisodeRunRecord CurrentRecord;

	UPROPERTY(Transient)
	FScenarioRunInput CurrentRunInput;

	int32 CurrentRunIndex = INDEX_NONE;
};
