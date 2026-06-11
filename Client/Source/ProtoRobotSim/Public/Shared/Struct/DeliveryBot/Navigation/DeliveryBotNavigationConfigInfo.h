#pragma once

#include "CoreMinimal.h"
#include "Shared/Types/DeliveryBotNavigationType.h"
#include "Shared/Struct/DeliveryBot/Navigation/DeliveryBotHybridAStarConfigInfo.h"
#include "DeliveryBotNavigationConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotNavigationConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotPathFinderType PathFinderType{ EDeliveryBotPathFinderType::GridAStar };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotDriveControllerType DriveControllerType{ EDeliveryBotDriveControllerType::PathFollow };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotHybridAStarConfigInfo HybridAStarConfigInfo{};
	
	
};
