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
	float PathPointAcceptanceDistanceM{ 0.4f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GoalAcceptanceDistanceM{ 0.8f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringSensitivity{ 0.8f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinTurnSpeedKmh{ 1.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObstacleSlowSpeedKmh{ 0.5f };
};
