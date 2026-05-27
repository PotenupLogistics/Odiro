#pragma once
#include "CoreMinimal.h"

struct FDeliveryBotPathQueueNodeInfo
{
public:
	FIntPoint GridIndex{ FIntPoint::ZeroValue };

	float FCost{ 0.f };
	float HCost{ 0.f };
};
	

struct FDeliveryBotPathPriorityQueueInfo
{
public:
	bool IsEmpty() const
	{
		return Items.IsEmpty();
	}	
	
	void Push(const FDeliveryBotPathQueueNodeInfo& itemInfo)
	{
		Items.Add(itemInfo);
		MoveUp(Items.Num() - 1);
	}
	
	bool Pop(FDeliveryBotPathQueueNodeInfo& outItemInfo)
	{
		if (Items.IsEmpty())
			return false;

		outItemInfo = Items[0];

		if (Items.Num() == 1)
		{
			Items.Reset();
			return true;
		}

		Items[0] = Items.Last();
		Items.RemoveAt(Items.Num() - 1);
		MoveDown(0);
		return true;
	}
	
private:
	bool HasHigherPriority(const FDeliveryBotPathQueueNodeInfo& leftInfo, const FDeliveryBotPathQueueNodeInfo& rightInfo) const	
	{
		if (!FMath::IsNearlyEqual(leftInfo.FCost, rightInfo.FCost))
			return leftInfo.FCost < rightInfo.FCost;
		return leftInfo.HCost < rightInfo.HCost;
	}
	
	void MoveUp(int32 itemIndex)
	{
		while (itemIndex > 0)
		{
			const int32 parentIndex{(itemIndex - 1) / 2};
			if (!HasHigherPriority(Items[itemIndex], Items[parentIndex]))
				break;
			
			Swap(Items[itemIndex], Items[parentIndex]);
			itemIndex = parentIndex;
		}
	}
	
	void MoveDown(int32 itemIndex)
	{
		while (true)
		{
			const int32 leftIndex{ itemIndex * 2 + 1 };
			const int32 rightIndex{ itemIndex * 2 + 2 };
			int32 bestIndex{ itemIndex };

			if (Items.IsValidIndex(leftIndex) 
				&& HasHigherPriority(Items[leftIndex], Items[bestIndex]))
				bestIndex = leftIndex;

			if (Items.IsValidIndex(rightIndex) 
				&& HasHigherPriority(Items[rightIndex], Items[bestIndex]))
				bestIndex = rightIndex;

			if (bestIndex == itemIndex)
				break;

			Swap(Items[itemIndex], Items[bestIndex]);
			itemIndex = bestIndex;
		}
	}
	
	
private:
	TArray<FDeliveryBotPathQueueNodeInfo> Items;
	
};

