#pragma once
#include "CoreMinimal.h"
#include "DeliveryBotPathFollowConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotPathFollowConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawDebug{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetSpeedKmh{ 3.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LookAheadDistanceM{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinLookAheadDistanceM{ 0.75f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxLookAheadDistanceM{ 2.4f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LookAheadSpeedGainMPerKmh{ 0.12f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LookAheadSteeringReductionRatio{ 0.45f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LookAheadSmoothingRatio{ 0.35f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PathPointAcceptanceDistanceM{ 0.4f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GoalAcceptanceDistanceM{ 0.8f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringSensitivity{ 1.1f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringFullScaleDegree{ 80.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxSteering{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxSteeringDelta{ 0.09f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinTurnSpeedKmh{ 1.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObstacleSlowSpeedKmh{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObstacleSoftCostRadiusM{ 1.0f }; // 장애물 주변 코스를 높여서 급격하게 피해가는 일을 줄인다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObstacleSoftCostMaxPenalty{ 8.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObstacleSoftCostPower{ 2.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PathTurnCostPenalty{ 1.5f };
};
