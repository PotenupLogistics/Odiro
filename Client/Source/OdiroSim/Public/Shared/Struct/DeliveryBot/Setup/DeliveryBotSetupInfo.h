#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotLocationSetupInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotDriveConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotPointCloudCaptureConfigInfo.h"
#include "DeliveryBotSetupInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotSetupInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLocationSetupInfo LocationSetupInfo{}; // 출발 도착 위치

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotDriveConfigInfo ChaosDriveConfigInfo{}; // Chaos Vehicle의 속도 제한, 가속/감속 같은 실제 주행 값

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLidarSensorConfigInfo LidarSensorConfigInfo{}; // 라이다 센서의 탐지 거리, 레이 간격, 센서 높이 등

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotPointCloudCaptureConfigInfo PointCloudCaptureConfigInfo{}; // Python Point Cloud capture profile과 저장 옵션

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StartupPolicySpecFileName{ TEXT("PolicySpec_DefaultDelivery") }; // 정책 JSON 파일 명
};
