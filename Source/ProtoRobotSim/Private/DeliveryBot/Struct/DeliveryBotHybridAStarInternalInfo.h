#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotPathInfo.h"

struct FDeliveryBotHybridAStarNodeKey
{
	FIntPoint GridIndex{ FIntPoint::ZeroValue };
	int32 HeadingIndex{ 0 };
	EDeliveryBotMoveDirectionType MoveDirectionType{ EDeliveryBotMoveDirectionType::Forward };
	int32 ContinuousReverseStepCount{ 0 };

	// Hybrid A* 노드 키가 같은 탐색 상태를 가리키는지 비교한다.
	bool operator==(const FDeliveryBotHybridAStarNodeKey& other) const
	{
		return GridIndex == other.GridIndex
			&& HeadingIndex == other.HeadingIndex
			&& MoveDirectionType == other.MoveDirectionType
			&& ContinuousReverseStepCount == other.ContinuousReverseStepCount;
	}
};

// Hybrid A* 노드 키를 TMap/TSet에서 사용할 수 있도록 해시값을 만든다.
FORCEINLINE uint32 GetTypeHash(const FDeliveryBotHybridAStarNodeKey& key)
{
	uint32 hash = HashCombine(
		static_cast<uint32>(key.GridIndex.X),
		static_cast<uint32>(key.GridIndex.Y));

	hash = HashCombine(hash, static_cast<uint32>(key.HeadingIndex));
	hash = HashCombine(hash, static_cast<uint32>(key.MoveDirectionType));
	hash = HashCombine(hash, static_cast<uint32>(key.ContinuousReverseStepCount));

	return hash;
}


struct FDeliveryBotHybridAStarNodeInfo
{
	FDeliveryBotHybridAStarNodeKey Key{};
	FDeliveryBotHybridAStarNodeKey ParentKey{};

	FVector LocationCm{ FVector::ZeroVector };
	float HeadingRadian{ 0.f };
	float ContinuousReverseDistanceCm{ 0.f };
	float TurnDirectionSign{ 0.f };

	float GCost{ 0.f };
	float HCost{ 0.f };

	bool bHasParent{ false };

	// 현재 노드의 총 비용인 FCost를 반환한다.
	float GetFCost() const
	{
		return GCost + HCost;
	}
};

struct FDeliveryBotHybridAStarQueueNodeInfo
{
	FDeliveryBotHybridAStarNodeKey Key{};
	float FCost{ 0.f };
	float HCost{ 0.f };
};

struct FDeliveryBotHybridAStarPriorityQueueInfo
{
	// FCost가 낮은 노드를 먼저 꺼내기 위해 우선순위 큐에 후보를 추가한다.
	void Push(const FDeliveryBotHybridAStarQueueNodeInfo& itemInfo)
	{
		Items.Add(itemInfo);

		int32 childIndex = Items.Num() - 1;
		while (childIndex > 0)
		{
			const int32 parentIndex = (childIndex - 1) / 2;

			if (!HasHigherPriority(Items[childIndex], Items[parentIndex]))
				break;

			Items.Swap(childIndex, parentIndex);
			childIndex = parentIndex;
		}
	}

	// 우선순위가 가장 높은 Hybrid A* 후보 노드를 꺼낸다.
	bool Pop(FDeliveryBotHybridAStarQueueNodeInfo& outItemInfo)
	{
		if (Items.Num() == 0)
			return false;

		outItemInfo = Items[0];

		Items[0] = Items.Last();
		Items.Pop();

		int32 parentIndex = 0;

		while (true)
		{
			const int32 leftChildIndex = parentIndex * 2 + 1;
			const int32 rightChildIndex = parentIndex * 2 + 2;
			int32 bestIndex = parentIndex;

			if (Items.IsValidIndex(leftChildIndex)
				&& HasHigherPriority(Items[leftChildIndex], Items[bestIndex]))
			{
				bestIndex = leftChildIndex;
			}

			if (Items.IsValidIndex(rightChildIndex)
				&& HasHigherPriority(Items[rightChildIndex], Items[bestIndex]))
			{
				bestIndex = rightChildIndex;
			}

			if (bestIndex == parentIndex)
				break;

			Items.Swap(parentIndex, bestIndex);
			parentIndex = bestIndex;
		}

		return true;
	}

	// 우선순위 큐에 더 이상 탐색 후보가 없는지 확인한다.
	bool IsEmpty() const
	{
		return Items.Num() == 0;
	}

private:
	// 두 큐 노드 중 더 먼저 탐색해야 하는 노드를 판단한다.
	bool HasHigherPriority(
		const FDeliveryBotHybridAStarQueueNodeInfo& leftInfo,
		const FDeliveryBotHybridAStarQueueNodeInfo& rightInfo) const
	{
		if (!FMath::IsNearlyEqual(leftInfo.FCost, rightInfo.FCost))
			return leftInfo.FCost < rightInfo.FCost;

		return leftInfo.HCost < rightInfo.HCost;
	}

private:
	TArray<FDeliveryBotHybridAStarQueueNodeInfo> Items;
};

// 그리드, 헤딩, 이동 방향, 연속 후진 상태를 묶어 Hybrid A* 노드 키를 만든다.
FORCEINLINE FDeliveryBotHybridAStarNodeKey MakeDeliveryBotHybridAStarNodeKey(
	const FIntPoint& gridIndex,
	int32 headingIndex,
	EDeliveryBotMoveDirectionType moveDirectionType,
	int32 continuousReverseStepCount = 0)
{
	FDeliveryBotHybridAStarNodeKey nodeKey;
	nodeKey.GridIndex = gridIndex;
	nodeKey.HeadingIndex = headingIndex;
	nodeKey.MoveDirectionType = moveDirectionType;
	nodeKey.ContinuousReverseStepCount = continuousReverseStepCount;

	return nodeKey;
}

// 부모가 없는 Hybrid A* 노드 정보를 만든다.
FORCEINLINE FDeliveryBotHybridAStarNodeInfo MakeDeliveryBotHybridAStarNodeInfo(
	const FDeliveryBotHybridAStarNodeKey& nodeKey,
	const FVector& locationCm,
	float headingRadian,
	float gCost,
	float hCost,
	float continuousReverseDistanceCm = 0.f,
	float turnDirectionSign = 0.f)
{
	FDeliveryBotHybridAStarNodeInfo nodeInfo;
	nodeInfo.Key = nodeKey;
	nodeInfo.LocationCm = locationCm;
	nodeInfo.HeadingRadian = headingRadian;
	nodeInfo.ContinuousReverseDistanceCm = continuousReverseDistanceCm;
	nodeInfo.TurnDirectionSign = turnDirectionSign;
	nodeInfo.GCost = gCost;
	nodeInfo.HCost = hCost;
	nodeInfo.bHasParent = false;

	return nodeInfo;
}

// 노드 정보를 우선순위 큐에 넣기 위한 가벼운 큐 노드로 변환한다.
FORCEINLINE FDeliveryBotHybridAStarQueueNodeInfo MakeDeliveryBotHybridAStarQueueNodeInfo(
	const FDeliveryBotHybridAStarNodeInfo& nodeInfo)
{
	FDeliveryBotHybridAStarQueueNodeInfo queueNodeInfo;
	queueNodeInfo.Key = nodeInfo.Key;
	queueNodeInfo.FCost = nodeInfo.GetFCost();
	queueNodeInfo.HCost = nodeInfo.HCost;

	return queueNodeInfo;
}

// 부모 키를 포함한 Hybrid A* 자식 노드 정보를 만든다.
FORCEINLINE FDeliveryBotHybridAStarNodeInfo MakeDeliveryBotHybridAStarChildNodeInfo(
	const FDeliveryBotHybridAStarNodeKey& nodeKey,
	const FDeliveryBotHybridAStarNodeKey& parentKey,
	const FVector& locationCm,
	float headingRadian,
	float gCost,
	float hCost,
	float continuousReverseDistanceCm = 0.f,
	float turnDirectionSign = 0.f)
{
	FDeliveryBotHybridAStarNodeInfo nodeInfo =
		MakeDeliveryBotHybridAStarNodeInfo(
			nodeKey,
			locationCm,
			headingRadian,
			gCost,
			hCost,
			continuousReverseDistanceCm,
			turnDirectionSign);

	nodeInfo.ParentKey = parentKey;
	nodeInfo.bHasParent = true;

	return nodeInfo;
}
