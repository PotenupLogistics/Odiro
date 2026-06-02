#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBotLocationSetupInfo.h"
#include "Shared/Struct/DeliveryBotDriveConfigInfo.h"
#include "Shared/Struct/DeliveryBotPathFollowConfigInfo.h"
#include "Shared/Struct/DeliveryBotLidarSensorInfo.h"
#include "DeliveryBotSetupInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotSetupInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLocationSetupInfo LocationSetupInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotDriveConfigInfo ChaosDriveConfigInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotPathFollowConfigInfo PathFollowConfigInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLidarSensorConfigInfo LidarSensorConfigInfo{};
};
