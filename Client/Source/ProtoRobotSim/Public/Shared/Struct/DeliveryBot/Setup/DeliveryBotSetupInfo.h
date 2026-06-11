#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotLocationSetupInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotDriveConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotPathFollowConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Navigation/DeliveryBotNavigationConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
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
	
	// FDeliveryBotMotionControlConfigInfo 이걸로 추후 이름 변경
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotPathFollowConfigInfo PathFollowConfigInfo{}; // A* 경로 따라갈 때 쓰는 목표 속도,도착 판정 거리,장애물 감속 속도 설정.

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLidarSensorConfigInfo LidarSensorConfigInfo{}; // 라이다 센서의 탐지 거리, 레이 간격, 센서 높이 등

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotNavigationConfigInfo NavigationConfigInfo{}; // 어떤 방식으로 이동할지 A*, HybridA*, DWA 등

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StartupPolicySpecFileName{ TEXT("PolicySpec_DefaultDelivery") }; // 정책 JSON 파일 명
};
