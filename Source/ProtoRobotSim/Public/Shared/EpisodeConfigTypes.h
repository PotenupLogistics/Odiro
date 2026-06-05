#pragma once

#include "CoreMinimal.h"
#include "EpisodeCoreTypes.h"
#include "EpisodeConfigTypes.generated.h"

class AActor;

// 에피소드 실행 설정과 재현성 seed 목록을 정의하는 파일임.
// 사용자가 선택한 템플릿과 파라미터를 한 번의 실행 설정으로 묶은 타입임.
// 여러 Iteration을 중복되지 않은 Seed로 관리하기 위함.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeRunConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 TemplateVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 GeneratorVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 BaseSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 IterationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TMap<FString, FEpisodeParamValue> Parameters;
};

// base seed에서 파생된 세부 seed들을 기록하는 재현성 장부임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeSeedLedger
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 WorldSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 LayoutSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 StaticObstacleSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 DynamicActorSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 EventSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int64 PolicySeed = 0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeEvaluationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation", meta = (ClampMin = "0.0"))
	double GoalAcceptanceRadiusCm = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation", meta = (ClampMin = "0.0"))
	double TipOverAngleDegrees = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation", meta = (ClampMin = "0.0"))
	double NearMissDistanceCm = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation")
	double StaticObstacleCollisionScore = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation")
	double BlockedRegionCollisionScore = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation")
	double PenaltyRegionViolationScore = -3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation")
	double PedestrianNearMissScore = -3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Evaluation")
	double PedestrianCollisionScore = -10.0;
};

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

UENUM(BlueprintType)
enum class EEpisodeRunnerState : uint8
{
	Idle,
	Preparing,
	Running,
	Ending,
	Completed,
	Cancelled,
	Failed
};

// Runner가 한 번의 실행으로 묶어 처리할 EpisodeSetup/DeliveryBotSetup 파일 pair.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeRunInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PairId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeSetupJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString DeliveryBotSetupJsonPath;
};

// SimulationSubsystem이 생성한 월드 객체들을 EvaluationSubsystem이 관찰할 수 있도록 전달하는 런타임 컨텍스트.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString SpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString RobotInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TObjectPtr<AActor> RobotActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool bHasGoalLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector GoalLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<TObjectPtr<AActor>> RuntimeActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<TObjectPtr<AActor>> GroundRegionActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<TObjectPtr<AActor>> StaticObstacleActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<TObjectPtr<AActor>> PedestrianActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	TArray<FString> PedestrianInstanceIds;
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
	TMap<FString, FEpisodeParamValue> Properties;
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
	TMap<FString, FEpisodeParamValue> Metrics;

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
