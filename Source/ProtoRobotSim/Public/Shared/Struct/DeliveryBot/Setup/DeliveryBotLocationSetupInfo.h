#pragma once
#include "CoreMinimal.h"
#include "DeliveryBotLocationSetupInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotLocationSetupInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector StartLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GoalLocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoStartRoute{ true };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasGoal{ false };
};

