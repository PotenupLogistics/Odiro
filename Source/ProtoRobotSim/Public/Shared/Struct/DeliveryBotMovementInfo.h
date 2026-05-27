#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotMovementInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotPosInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location{FVector::ZeroVector};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawDegree{0.f};
};


USTRUCT(BlueprintType)
struct FDeliveryBotMoveCommandInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetSpeed{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Steering{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBrake{false};
};

