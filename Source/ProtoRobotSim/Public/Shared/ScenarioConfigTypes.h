#pragma once

#include "CoreMinimal.h"
#include "ScenarioCoreTypes.h"
#include "ScenarioConfigTypes.generated.h"

class AActor;

// 에피소드 실행 설정과 재현성 seed 목록을 정의하는 파일임.
// 사용자가 선택한 템플릿과 파라미터를 한 번의 실행 설정으로 묶은 타입임.
// 여러 Iteration을 중복되지 않은 Seed로 관리하기 위함.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioRunConfig
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
	TMap<FString, FScenarioParamValue> Parameters;
};

// base seed에서 파생된 세부 seed들을 기록하는 재현성 장부임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioSeedLedger
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
struct PROTOROBOTSIM_API FScenarioEvaluationConfig
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
enum class EScenarioRunnerState : uint8
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
struct PROTOROBOTSIM_API FScenarioRunInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PairId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString EpisodeSetupJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString DeliveryBotSetupJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString PolicySpecJsonPath;
};

// SimulationSubsystem이 생성한 월드 객체들을 EvaluationSubsystem이 관찰할 수 있도록 전달하는 런타임 컨텍스트.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioRuntimeContext
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

