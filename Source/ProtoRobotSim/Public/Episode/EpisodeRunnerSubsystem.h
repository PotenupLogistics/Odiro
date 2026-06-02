#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeCompileTypes.h"
#include "Shared/EpisodeConfigTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EpisodeRunnerSubsystem.generated.h"

class UEpisodeEvaluationSubsystem;
class UEpisodeSimulationSubsystem;

// Episode compile, setup, evaluation을 순차 실행하고 최종 FEpisodeRunRecord를 수집하는 subsystem.
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeRunnerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartEpisodeFromJsonFile(const FString& JsonFilePath);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	bool StartBatchFromJsonFiles(const TArray<FString>& JsonFilePaths);

	UFUNCTION(BlueprintCallable, Category = "Episode|Runner")
	void CancelRun();

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	EEpisodeRunnerState GetRunnerState() const { return RunnerState; }

	UFUNCTION(BlueprintPure, Category = "Episode|Runner")
	TArray<FEpisodeRunRecord> GetRunRecords() const { return RunRecords; }

private:
	UFUNCTION()
	void HandleEpisodeEnded(FEpisodeEvaluationResult Result);

	void StartNextEpisode();
	void QueueStartNextEpisode();
	void CompleteCurrentRecord(
		bool bSuccess,
		EEpisodeEvaluationOutcome Outcome,
		EEpisodeEvaluationTerminalReason TerminalReason,
		const FEpisodeEvaluationResult* EvaluationResult = nullptr);

	void AppendCompileDiagnostics(const FEpisodeCompileResult& CompileResult);
	FString BuildRunId() const;

	UWorld* ResolveWorld() const;
	UEpisodeSimulationSubsystem* ResolveSimulationSubsystem() const;
	UEpisodeEvaluationSubsystem* ResolveEvaluationSubsystem() const;

	UPROPERTY(Transient)
	EEpisodeRunnerState RunnerState = EEpisodeRunnerState::Idle;

	UPROPERTY(Transient)
	TArray<FString> PendingJsonFilePaths;

	UPROPERTY(Transient)
	TArray<FEpisodeRunRecord> RunRecords;

	UPROPERTY(Transient)
	FEpisodeRunRecord CurrentRecord;

	UPROPERTY(Transient)
	FString CurrentJsonFilePath;

	int32 CurrentRunIndex = INDEX_NONE;
};
