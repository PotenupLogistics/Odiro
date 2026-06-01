#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotChaosDriveConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotChaosDriveConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxSpeedKmh{ 10.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlowdownSpeedRangeKmh{ 2.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedLimitToleranceKmh{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedLimitBrake{ 0.15f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StopBrakeInput{ 0.35f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThrottleInputRatePerSecond{ 0.7f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BrakeInputRatePerSecond{ 1.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringInputRatePerSecond{ 3.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseHandbrakeWhenBrake{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxTorque{ 220.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxRPM{ 4000.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineIdleRPM{ 600.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineBrakeEffect{ 0.04f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineRevUpMOI{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineRevDownRate{ 600.f };
};
