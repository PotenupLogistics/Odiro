#pragma once

#include "CoreMinimal.h"
#include "ScenarioCoreTypes.h"
#include "ScenarioConfigTypes.generated.h"

class AActor;

// 에피소드 실행 설정과 재현성 seed 목록을 정의하는 파일임.
// 사용자가 선택한 템플릿과 파라미터를 한 번의 실행 설정으로 묶은 타입임.
// 여러 Iteration을 중복되지 않은 Seed로 관리하기 위함.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioRunConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 TemplateVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 GeneratorVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 BaseSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int32 IterationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TMap<FString, FScenarioParamValue> Parameters;
};

// base seed에서 파생된 세부 seed들을 기록하는 재현성 장부임.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSeedLedger
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 WorldSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 LayoutSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 StaticObstacleSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 DynamicActorSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 EventSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	int64 PolicySeed = 0;
};

USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioEvaluationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation", meta = (ClampMin = "0.0"))
	double GoalAcceptanceRadiusCm = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation", meta = (ClampMin = "0.0"))
	double TipOverAngleDegrees = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation", meta = (ClampMin = "0.0"))
	double NearMissDistanceCm = 50.0;

	// Value attached to static-obstacle collision events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation")
	double StaticObstacleCollisionEventValue = -1.0;

	// Value attached to blocked-region collision events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation")
	double BlockedRegionCollisionEventValue = -1.0;

	// Value attached to penalty-region violation events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation")
	double PenaltyRegionViolationEventValue = -3.0;

	// Value attached to pedestrian near-miss events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation")
	double PedestrianNearMissEventValue = -3.0;

	// Value attached to pedestrian collision events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Evaluation")
	double PedestrianCollisionEventValue = -10.0;
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

// Runner input resolved from one user-project scenario_sample and its profile.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioRunInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PairId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString EpisodeScenarioJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString ProfileJsonPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString PolicySpecJsonPath;
};

// SimulationSubsystem이 생성한 월드 객체들을 EvaluationSubsystem이 관찰할 수 있도록 전달하는 런타임 컨텍스트.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString EpisodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString SpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FString RobotInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TObjectPtr<AActor> RobotActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	bool bHasGoalLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FVector GoalLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<TObjectPtr<AActor>> RuntimeActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<TObjectPtr<AActor>> GroundRegionActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<TObjectPtr<AActor>> StaticObstacleActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<TObjectPtr<AActor>> PedestrianActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FString> PedestrianInstanceIds;
};
