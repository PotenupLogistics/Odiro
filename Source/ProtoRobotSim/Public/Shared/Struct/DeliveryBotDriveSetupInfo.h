#pragma once
#include "CoreMinimal.h"
#include "DeliveryBotDriveSetupInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotDriveSetupInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector StartLocation{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GoalLocation{ FVector::ZeroVector };
};

