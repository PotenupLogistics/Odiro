#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/EpisodeResultTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioRunnerSubsystem.generated.h"

struct FDeliveryBotSetupCompileResult;
struct FScenarioSimulationSetupSpec;
class UScenarioEvaluationSubsystem;
class UScenarioSimulationSubsystem;
class ADeliveryBot;
DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioRunnerStateChangedNative, EScenarioRunnerState);
DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioRunRecordCompletedNative, const FEpisodeRunRecord&);

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioRunnerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Subsystem 종료 전에 delegate와 timer를 정리한다.
	virtual void Deinitialize() override;

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

	// Evaluation 종료 요청을 받아 DeliveryBot end 통신을 시작한다.
	void HandleEpisodeEndRequested(const FEpisodeEvaluationResult& result);

	// callback 또는 watchdog 결과로 Episode 종료를 계속한다.
	void CompleteEpisodeFinalization(
		uint64 finalizationGeneration,
		bool bSucceeded,
		bool bTimedOut,
		const FString& errorMessage);

	// Runner watchdog 제한 시간을 처리한다.
	void HandleEpisodeFinalizationTimeout(uint64 finalizationGeneration);

	// 종료 이유를 Python status 문자열로 변환한다.
	static FString BuildExternalEndStatus(EEpisodeEvaluationTerminalReason terminalReason);

	// Evaluation 종료 delegate를 해제한다.
	void UnbindEvaluationDelegates();

	// timer와 DeliveryBot 참조를 초기화한다.
	void ResetEpisodeFinalizationState();

	// Schedules a non-blocking top-view PNG for the active project episode.
	void ScheduleEpisodePreviewCapture(
		const FScenarioSimulationSetupSpec& setupSpec,
		const FScenarioRuntimeContext& runtimeContext,
		const FString& projectOutputEpisodeId);

	// Captures the active episode if the delayed request still belongs to it.
	void CaptureEpisodePreview(
		uint64 captureGeneration,
		FScenarioSimulationSetupSpec setupSpec,
		FScenarioRuntimeContext runtimeContext,
		FString outputPath);

	// Cancels any delayed episode preview capture from an older episode.
	void ResetEpisodePreviewCapture();

private:

	TWeakObjectPtr<ADeliveryBot> CurrentDeliveryBotActor;// 현재 Episode의 DeliveryBot 약한 참조다.
	FDelegateHandle EpisodeEndRequestedHandle;// Evaluation 종료 요청 delegate 연결이다.
	FTimerHandle EpisodeFinalizationTimeoutHandle;	// Runner의 Python end watchdog timer다.
	FTimerHandle EpisodePreviewCaptureTimerHandle;	// Delayed project episode preview timer.

	bool bEpisodeFinalizationInFlight = false;	// callback 또는 watchdog을 기다리는 상태다.
	bool bCancelRequested = false;	// 취소 후 다음 Scenario 시작을 막는다.

	// 이전 Episode의 늦은 callback을 차단한다.
	uint64 EpisodeFinalizationGeneration = 0;

	// Blocks stale delayed preview callbacks from previous episodes.
	uint64 EpisodePreviewCaptureGeneration = 0;

	// Episode 종료 흐름의 최대 대기 시간이다.
	static constexpr double EpisodeFinalizationTimeoutSeconds = 3.0;

	// Delay after the episode starts before writing preview.png.
	static constexpr double EpisodePreviewCaptureDelaySeconds = 2.0;

	// SceneCapture exposure history warmup before the final preview frame is saved.
	static constexpr double EpisodePreviewCaptureWarmupSeconds = 0.5;
};
