#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotNavigationType.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotPathFinderType : uint8
{
	GridAStar,
	HybridAStar
};

UENUM(BlueprintType)
enum class EDeliveryBotDriveControllerType : uint8
{
	PathFollow,
	DWA,
	None
};

// Hybrid A*가 전진만 탐색할지, 전진/후진을 모두 탐색할지
UENUM(BlueprintType)
enum class EDeliveryBotHybridAStarMotionModelType : uint8
{
	ForwardOnly,
	ForwardReverse
};