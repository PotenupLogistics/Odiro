#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPolicyDecisionTypes.generated.h"

// DeliveryBot policy decision 처리 상태를 나타낸다.
UENUM(BlueprintType)
enum class EDeliveryBotPolicyDecisionStatusTypes : uint8
{
	Unknown,
	Ok,
	Error
};
