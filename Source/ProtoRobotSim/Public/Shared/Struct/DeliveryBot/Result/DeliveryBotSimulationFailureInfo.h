#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotSimulationFailureInfo.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EDeliveryBotSimulationFailureType : uint8
{
	None,
	PolicyRequestFailed,
	RobotTipOver,
	Collision,
	PathFindingFailed,
	Stuck,
	Timeout
};

USTRUCT(BlueprintType)
struct FDeliveryBotSimulationFailureInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotSimulationFailureType FailureType{ EDeliveryBotSimulationFailureType::None };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Message{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeSeconds{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> TargetActor{ nullptr };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedKmh{ 0.f };
};
