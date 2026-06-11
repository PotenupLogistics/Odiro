#pragma once

#include "CoreMinimal.h"
#include "ScenarioConfigTypes.h"
#include "EpisodeConfigTypes.generated.h"

UENUM(BlueprintType)
enum class EEpisodeEvaluationOutcome : uint8
{
	Running,
	Success,
	Warning,
	Failure,
	Cancelled
};

UENUM(BlueprintType)
enum class EEpisodeEvaluationTerminalReason : uint8
{
	None = 0,
	GoalReached = 1,
	Timeout = 2,
	RobotTipOver = 3,
	CompilerCreateFailed = 8,
	CompileFailed = 9,
	SetupFailed = 10,
	EvaluationStartFailed = 11,
	Cancelled = 12,
	DeliveryBotSimulationFailed = 13
};

UENUM(BlueprintType)
enum class EEpisodeEvaluationEventType : uint8
{
	None = 0,
	Timeout = 2,
	RobotTipOver = 3,
	StaticObstacleCollision = 4,
	BlockedRegionCollision = 5,
	PenaltyRegionViolation = 6,
	PedestrianNearMiss = 7,
	PedestrianCollision = 8,
	DeliveryBotSimulationFailure = 9
};

UENUM(BlueprintType)
enum class EEpisodeEvaluationEventSeverity : uint8
{
	Info,
	Warning,
	Failure
};

// EvaluationSubsystem이 평가 중 발견한 사건 한 건에 대한 Snapshot.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeEvaluationEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 EventIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double ElapsedTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double WorldTimeSeconds = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeEvaluationEventType EventType = EEpisodeEvaluationEventType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeEvaluationEventSeverity Severity = EEpisodeEvaluationEventSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString SubjectInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString TargetInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double Value = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FScenarioParamValue> Properties;
};

// EvaluationSubsystem이 한 에피소드 평가 종료 시 산출하는 요약.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bCompleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeEvaluationOutcome Outcome = EEpisodeEvaluationOutcome::Running;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeEvaluationTerminalReason TerminalReason = EEpisodeEvaluationTerminalReason::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double DurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FScenarioParamValue> Metrics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FEpisodeEvaluationEvent> Events;
};

// EpisodeRunner가 compile, setup, evaluation 결과를 합쳐 보존하는 최종 기록.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeRunRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString RunId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 RunIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PairId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString SourceJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeSetupJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString DeliveryBotSetupJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PolicySpecJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString SpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeSetupHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString DeliveryBotSetupHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PairHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bCompileSucceeded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bEpisodeSetupCompileSucceeded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bDeliveryBotSetupCompileSucceeded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bSetupSucceeded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bEvaluationCompleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeEvaluationOutcome Outcome = EEpisodeEvaluationOutcome::Running;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeEvaluationTerminalReason TerminalReason = EEpisodeEvaluationTerminalReason::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double EndTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double DurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FEpisodeEvaluationResult EvaluationResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FString> Diagnostics;

	// 저장된 evaluation report JSON path. 저장이 비활성화되었거나 실패하면 비어 있음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EvaluationReportJsonPath;
};
