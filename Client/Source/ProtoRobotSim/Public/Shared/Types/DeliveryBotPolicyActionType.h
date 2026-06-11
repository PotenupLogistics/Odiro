#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPolicyActionType.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotPolicyActionType : uint8
{
	None,
	SlowDown,
	Stop,
	Repath,
	Avoidance,
	RequestControl,
	End
};