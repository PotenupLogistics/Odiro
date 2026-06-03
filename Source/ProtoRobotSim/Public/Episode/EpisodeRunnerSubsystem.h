#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCompileTypes.h"
#include "Shared/EpisodeConfigTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EpisodeRunnerSubsystem.generated.h"

struct FDeliveryBotSetupCompileResult;
class UEpisodeEvaluationSubsystem;
class UEpisodeSimulationSubsystem;

// Episode compile, setup, evaluation을 순차 실행하고 최종 FEpisodeRunRecord를 수집하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeRunnerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartEpisodePairFromJsonFiles(const FString& episodeSetupJsonPath, const FString& deliveryBotSetupJsonPath);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartBatchFromRunInputs(const TArray<FEpisodeRunInput>& runInputs);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartBatchFromRunQueueJsonFile(const FString& runQueueJsonFilePath);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	void CancelRun();

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	EEpisodeRunnerState GetRunnerState() const { return RunnerState; }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	TArray<FEpisodeRunRecord> GetRunRecords() const { return RunRecords; }

private:
	UFUNCTION()
	void HandleEpisodeEnded(FEpisodeEvaluationResult result);

	void StartNextEpisode();
	void QueueStartNextEpisode();
	void CompleteCurrentRecord(
		bool bSuccess,
		EEpisodeEvaluationOutcome outcome,
		EEpisodeEvaluationTerminalReason terminalReason,
		const FEpisodeEvaluationResult* evaluationResult = nullptr);

	void AppendCompileDiagnostics(const FEpisodeCompileResult& compileResult);
	void AppendDeliveryBotSetupDiagnostics(const FDeliveryBotSetupCompileResult& compileResult);
	double GetRunTimeLimitSeconds(const FEpisodeRunConfig& runConfig) const;
	FString BuildRunId() const;
	static FString BuildPairId(const FEpisodeRunInput& runInput, int32 runIndex);

	UWorld* ResolveWorld() const;
	UEpisodeSimulationSubsystem* ResolveSimulationSubsystem() const;
	UEpisodeEvaluationSubsystem* ResolveEvaluationSubsystem() const;

	UPROPERTY(Transient)
	EEpisodeRunnerState RunnerState = EEpisodeRunnerState::Idle;

	UPROPERTY(Transient)
	TArray<FEpisodeRunInput> PendingRunInputs;

	UPROPERTY(Transient)
	TArray<FEpisodeRunRecord> RunRecords;

	UPROPERTY(Transient)
	FEpisodeRunRecord CurrentRecord;

	UPROPERTY(Transient)
	FEpisodeRunInput CurrentRunInput;

	int32 CurrentRunIndex = INDEX_NONE;
};
