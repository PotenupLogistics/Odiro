#pragma once
#include "CoreMinimal.h"

struct FDeliveryBotPathRequestInfo
{
public:
	FVector StartLocation{ FVector::ZeroVector };
	FVector GoalLocation{ FVector::ZeroVector };
};

struct FDeliveryBotPathResultInfo
{
public:
	bool bSuccess{ false };
	TArray<FVector> PathPoints;
};
