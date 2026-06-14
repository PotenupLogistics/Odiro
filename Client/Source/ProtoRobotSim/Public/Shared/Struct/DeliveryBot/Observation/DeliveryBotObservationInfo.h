#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "DeliveryBotObservationInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotRobotStateInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawDegree{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector VelocityCmPerSecond{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedKmh{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bColliding{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CollisionActorName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> CollisionActorTags{};
};

// Python이 로봇의 물리/센서 한계를 알기 위해 사용하는 정적 스펙 정보.
USTRUCT(BlueprintType)
struct FDeliveryBotVehicleSpecInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxSpeedKmh{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxReverseSpeedKmh{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector RobotBoxExtentCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WheelBaseCm{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinTurningRadiusCm{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotLidarModeType LidarModeType{ EDeliveryBotLidarModeType::OneD };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LidarScanRangeM{ 0.f };
};

USTRUCT(BlueprintType)
struct FDeliveryBotObservationInfo
{
	GENERATED_BODY()
	
	// Observation 생성 순번, Python 응답 action이 어떤 observation에 대한 것인지 맞출 때 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Sequence{ 0 };

	// 센서 snapshot 갱신 순번. Tick에서 라이다가 갱신될 때마다 증가한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SensorSequence{ 0 };
	
	// Unreal World 시간(초). timeout, 로그 정렬, action freshness 판단에 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WorldTimeSeconds{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotRobotStateInfo RobotState{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLidarScanInfo LidarScanInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDeliveryBotLidarObservedObjectInfo> ObservedObjects{};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotVehicleSpecInfo VehicleSpec{};
};
