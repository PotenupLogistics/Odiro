#pragma once
#include "CoreMinimal.h"


struct FDeliveryBotAStarInfo
{
	FIntPoint GridIndex{ FIntPoint::ZeroValue };

	float GCost{ 0.f };
	float HCost{ 0.f };

	FIntPoint ParentGridIndex{ FIntPoint{ INDEX_NONE, INDEX_NONE } };

	float GetFCost() const
	{
		return GCost + HCost;
	}
};
