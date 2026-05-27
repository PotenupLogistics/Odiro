// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_GlobalPathComponent.h"
#include "DrawDebugHelpers.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Shared/Struct/DeliveryBotPathQueueInfo.h"


UDeliveryBot_GlobalPathComponent::UDeliveryBot_GlobalPathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
 
}


void UDeliveryBot_GlobalPathComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


void UDeliveryBot_GlobalPathComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

bool UDeliveryBot_GlobalPathComponent::BuildPathByAStar(const FVector& startLocation, const FVector& goalLocation)
{
	GlobalPath.Reset();

	UWorld* world{GetWorld()};
	if (!IsValid(world))
		return false;

	UDeliveryBot_GridSubsystem* gridSubsystem{world->GetSubsystem<UDeliveryBot_GridSubsystem>()};
	if (!IsValid(gridSubsystem))
		return false;

	const FIntPoint startGridIndex{ gridSubsystem->GetGridIndexByWorldLocation(startLocation) };
	const FIntPoint goalGridIndex{ gridSubsystem->GetGridIndexByWorldLocation(goalLocation) };

	if (!gridSubsystem->IsWalkableGridIndex(startGridIndex))
		return false;

	if (!gridSubsystem->IsWalkableGridIndex(goalGridIndex))
		return false;

	TMap<FIntPoint, FDeliveryBotAStarInfo> nodeMap;
	TSet<FIntPoint> closedSet;
	FDeliveryBotPathPriorityQueueInfo openQueue;

	FDeliveryBotAStarInfo startNodeInfo;
	startNodeInfo.GridIndex = startGridIndex;
	startNodeInfo.GCost = 0.f;
	startNodeInfo.HCost = GetHeuristicCost(startGridIndex, goalGridIndex);
	startNodeInfo.ParentGridIndex = FIntPoint{ INDEX_NONE, INDEX_NONE };

	nodeMap.Add(startGridIndex, startNodeInfo);

	openQueue.Push(FDeliveryBotPathQueueNodeInfo{
		startGridIndex,
		startNodeInfo.GetFCost(),
		startNodeInfo.HCost
	});

	while (!openQueue.IsEmpty())
	{
		FDeliveryBotPathQueueNodeInfo currentQueueNodeInfo;
		if (!openQueue.Pop(currentQueueNodeInfo))
		{
			break;
		}

		const FIntPoint currentGridIndex{ currentQueueNodeInfo.GridIndex };

		if (closedSet.Contains(currentGridIndex))
		{
			continue;
		}

		if (currentGridIndex == goalGridIndex)
		{
			BuildGlobalPathFromNodeMap(nodeMap, startGridIndex, goalGridIndex, gridSubsystem);
			SmoothGlobalPath();
			DrawGlobalPath();
			return GlobalPath.Num() > 0;
		}

		closedSet.Add(currentGridIndex);

		const FDeliveryBotAStarInfo* currentNodeInfo{ nodeMap.Find(currentGridIndex) };
		if (currentNodeInfo == nullptr)
		{
			continue;
		}

		const TArray<FIntPoint> neighborGridIndexes{ gridSubsystem->GetNeighborGridIndexes(currentGridIndex) };
		for (const FIntPoint& neighborGridIndex : neighborGridIndexes)
		{
			if (closedSet.Contains(neighborGridIndex))
			{
				continue;
			}

			const float newGCost{ currentNodeInfo->GCost + GetMoveCost(currentGridIndex, neighborGridIndex) };

			FDeliveryBotAStarInfo* neighborNodeInfo{ nodeMap.Find(neighborGridIndex) };
			if (neighborNodeInfo != nullptr && newGCost >= neighborNodeInfo->GCost)
			{
				continue;
			}

			FDeliveryBotAStarInfo newNodeInfo;
			newNodeInfo.GridIndex = neighborGridIndex;
			newNodeInfo.GCost = newGCost;
			newNodeInfo.HCost = GetHeuristicCost(neighborGridIndex, goalGridIndex);
			newNodeInfo.ParentGridIndex = currentGridIndex;

			nodeMap.Add(neighborGridIndex, newNodeInfo);

			openQueue.Push(FDeliveryBotPathQueueNodeInfo{
				neighborGridIndex,
				newNodeInfo.GetFCost(),
				newNodeInfo.HCost
			});
		}
	}

	return false;
}

float UDeliveryBot_GlobalPathComponent::GetHeuristicCost(const FIntPoint& fromGridIndex,
                                                         const FIntPoint& toGridIndex) const
{
	const int32 dx{ FMath::Abs(fromGridIndex.X - toGridIndex.X) };
	const int32 dy{ FMath::Abs(fromGridIndex.Y - toGridIndex.Y) };

	return static_cast<float>(FMath::Max(dx, dy) * 10);
}

float UDeliveryBot_GlobalPathComponent::GetMoveCost(const FIntPoint& fromGridIndex, const FIntPoint& toGridIndex) const
{
	const int32 dx{ FMath::Abs(fromGridIndex.X - toGridIndex.X) };
	const int32 dy{ FMath::Abs(fromGridIndex.Y - toGridIndex.Y) };

	return dx == 1 && dy == 1 ? 14.f : 10.f;
}

void UDeliveryBot_GlobalPathComponent::BuildGlobalPathFromNodeMap(const TMap<FIntPoint, FDeliveryBotAStarInfo>& nodeMap,
	const FIntPoint& startGridIndex, const FIntPoint& goalGridIndex,
	const class UDeliveryBot_GridSubsystem* gridSubsystem)
{
	GlobalPath.Reset();

	if (!IsValid(gridSubsystem))
		return;

	const FDeliveryBotAStarInfo* goalNodeInfo{ nodeMap.Find(goalGridIndex) };
	if (goalNodeInfo == nullptr)
		return;

	TArray<FIntPoint> reverseGridPath;
	FIntPoint currentGridIndex{ goalGridIndex };

	bool bFoundStart{ false };
	const int32 maxTraceCount{ nodeMap.Num() };

	for (int32 traceCount = 0; traceCount < maxTraceCount; ++traceCount)
	{
		reverseGridPath.Add(currentGridIndex);

		if (currentGridIndex == startGridIndex)
		{
			bFoundStart = true;
			break;
		}

		const FDeliveryBotAStarInfo* currentNodeInfo{ nodeMap.Find(currentGridIndex) };
		if (currentNodeInfo == nullptr)
		{
			GlobalPath.Reset();
			return;
		}

		currentGridIndex = currentNodeInfo->ParentGridIndex;

		if (currentGridIndex.X == INDEX_NONE || currentGridIndex.Y == INDEX_NONE)
		{
			GlobalPath.Reset();
			return;
		}
	}

	if (!bFoundStart)
	{
		GlobalPath.Reset();
		return;
	}

	for (int32 pathIndex = reverseGridPath.Num() - 1; pathIndex >= 0; --pathIndex)
	{
		const FVector worldLocation{ gridSubsystem->GetWorldLocationByGridIndex(reverseGridPath[pathIndex]) };
		GlobalPath.Add(worldLocation);
	}
}

void UDeliveryBot_GlobalPathComponent::DrawGlobalPath() const
{
	const UWorld* world{ GetWorld() };
	if (!IsValid(world))
		return;

	if (GlobalPath.Num() < 2)
		return;

	for (int32 pathIndex = 0; pathIndex < GlobalPath.Num() - 1; ++pathIndex)
	{
		const FVector startLocation{ GlobalPath[pathIndex] + FVector{ 0.f, 0.f, 20.f } };
		const FVector endLocation{ GlobalPath[pathIndex + 1] + FVector{ 0.f, 0.f, 20.f } };

		// A*로 생성된 전역 경로를 눈으로 확인하기 위한 Debug Line이다.
		DrawDebugLine(
			world,
			startLocation,
			endLocation,
			FColor::Blue,
			true,
			30.f,
			0,
			5.f
		);
	}

	DrawDebugSphere(
		world,
		GlobalPath[0] + FVector{ 0.f, 0.f, 20.f },
		20.f,
		12,
		FColor::Cyan,
		true,
		30.f
	);

	DrawDebugSphere(
		world,
		GlobalPath.Last() + FVector{ 0.f, 0.f, 20.f },
		20.f,
		12,
		FColor::Magenta,
		true,
		30.f
	);
}

void UDeliveryBot_GlobalPathComponent::SmoothGlobalPath()
{
	if (GlobalPath.Num() < 3)
	{
		return;
	}

	TArray<FVector> smoothPath;
	int32 currentPathIndex{ 0 };

	smoothPath.Add(GlobalPath[currentPathIndex]);

	while (currentPathIndex < GlobalPath.Num() - 1)
	{
		int32 nextPathIndex{ GlobalPath.Num() - 1 };

		for (; nextPathIndex > currentPathIndex + 1; --nextPathIndex)
		{
			if (CanConnectPathPoints(GlobalPath[currentPathIndex], GlobalPath[nextPathIndex]))
			{
				break;
			}
		}

		smoothPath.Add(GlobalPath[nextPathIndex]);
		currentPathIndex = nextPathIndex;
	}

	GlobalPath = MoveTemp(smoothPath);
}

bool UDeliveryBot_GlobalPathComponent::CanConnectPathPoints(
	const FVector& fromLocation,
	const FVector& toLocation) const
{
	const UWorld* world{ GetWorld() };
	if (!IsValid(world))
	{
		return false;
	}

	const UDeliveryBot_GridSubsystem* gridSubsystem{ world->GetSubsystem<UDeliveryBot_GridSubsystem>() };
	if (!IsValid(gridSubsystem))
	{
		return false;
	}

	const FVector deltaLocation{ toLocation - fromLocation };
	const float distance2D{static_cast<float>(FVector{ deltaLocation.X, deltaLocation.Y, 0.f }.Size()) };
	const int32 sampleCount{ FMath::Max(1, FMath::CeilToInt(distance2D / PathSmoothingSampleDistance)) };

	for (int32 sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const float alpha{ static_cast<float>(sampleIndex) / static_cast<float>(sampleCount) };
		const FVector sampleLocation{ fromLocation + deltaLocation * alpha };
		const FIntPoint sampleGridIndex{ gridSubsystem->GetGridIndexByWorldLocation(sampleLocation) };

		if (!gridSubsystem->IsWalkableGridIndex(sampleGridIndex))
		{
			return false;
		}
	}

	return true;
}
