#pragma once

#include "CoreMinimal.h"
#include "Shared/Types/DeliveryBotNavigationType.h"
#include "DeliveryBotHybridAStarConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotHybridAStarConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotHybridAStarMotionModelType MotionModelType{ EDeliveryBotHybridAStarMotionModelType::ForwardReverse };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StepDistanceCm{ 75.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinTurningRadiusCm{ 300.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 HeadingBinCount{ 72 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSearchCount{ 15000 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GoalAcceptanceDistanceCm{ 150.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GoalAcceptanceAngleDegree{ 25.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ReverseCostMultiplier{ 2.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GearSwitchCostPenalty{ 500.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxContinuousReverseDistanceCm{ 250.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ReverseStepDistanceScale{ 0.6f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TurnCostPenalty{ 15.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ReverseTurnCostPenalty{ 25.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TurnSwitchCostPenalty{ 40.f };
};
