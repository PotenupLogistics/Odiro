#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotMovementTypes.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotMoveDirectionType : uint8
{
	Forward,
	Reverse
};
