#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotDriveConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotDriveConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxSpeedKmh{ 10.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxReverseSpeedKmh{ 3.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ReverseAccelerationRateKmhPerSecond{ 1.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GearSwitchStopSpeedKmh{ 0.3f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GearSwitchBrakeInput{ 0.2f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlowdownSpeedRangeKmh{ 5.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedLimitToleranceKmh{ 0.5f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedLimitBrake{ 0.06f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StopBrakeInput{ 0.18f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThrottleInputRatePerSecond{ 0.28f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BrakeInputRatePerSecond{ 0.35f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringInputRatePerSecond{ 3.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AccelerationRateKmhPerSecond{ 1.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DecelerationRateKmhPerSecond{ 0.9f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseHandbrakeWhenBrake{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxTorque{ 220.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxRPM{ 2000.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineIdleRPM{ 600.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineBrakeEffect{ 0.04f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineRevUpMOI{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EngineRevDownRate{ 600.f };
};
