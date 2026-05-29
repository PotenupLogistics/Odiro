#pragma once
#include "CoreMinimal.h"
#include "DeliveryBotPathFollowConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotPathFollowConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetSpeedKmh{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LookAheadDistanceM{ 2.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PathPointAcceptanceDistanceM{ 0.8f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GoalAcceptanceDistanceM{ 1.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringSensitivity{ 1.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDrawDebug{ true };

};
