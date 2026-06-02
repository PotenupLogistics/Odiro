// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"
#include "Engine/OverlapResult.h"

void UDeliveryBot_GridSubsystem::BuildGridFromBounds(const ADeliveryBot_GridBoundsActor* gridBoundsActor)
{
	if (!IsValid(gridBoundsActor))
		return;

	const UBoxComponent* boundsBox = gridBoundsActor->GetBoundsBox();
	if (!IsValid(boundsBox))
		return;

	CellSize = gridBoundsActor->GetCellSize();

	const FVector boxExtent = boundsBox->GetScaledBoxExtent();
	const FVector boxLocation = boundsBox->GetComponentLocation();

	GridOrigin = FVector(
		boxLocation.X - boxExtent.X,
		boxLocation.Y - boxExtent.Y,
		boxLocation.Z
	);

	GridSizeX = static_cast<int32>(FMath::FloorToInt((boxExtent.X * 2.f) / CellSize));
	GridSizeY = static_cast<int32>(FMath::FloorToInt((boxExtent.Y * 2.f) / CellSize));

	GridCells.Reset();
	GridCells.SetNum(GridSizeX * GridSizeY);

	const FVector robotBoxExtent = gridBoundsActor->GetRobotBoxExtent();
	const float maxWalkableSlopeDegree{ gridBoundsActor->GetMaxWalkableSlopeDegree() };
	for (int32 y = 0; y < GridSizeY; ++y)
	{
		for (int32 x = 0; x < GridSizeX; ++x)
		{
			const int32 index = x + y * GridSizeX;

			FDeliveryBotGridCellInfo& cellInfo = GridCells[index];
			cellInfo.State = EDeliveryBotGridCellState::Free;
			cellInfo.Cost = 1.f;
			cellInfo.WorldLocation = FVector(
				GridOrigin.X + (x + 0.5f) * CellSize,
				GridOrigin.Y + (y + 0.5f) * CellSize,
				GridOrigin.Z
			);
			FVector groundLocation{ cellInfo.WorldLocation };
			FVector groundNormal{ FVector::UpVector };

			if (!GetGroundInfoByWorldLocation(cellInfo.WorldLocation, groundLocation, groundNormal))
			{
				cellInfo.State = EDeliveryBotGridCellState::Blocked;
				cellInfo.Cost = BIG_NUMBER;
				
				DrawDebugPoint(
				GetWorld(),
				cellInfo.WorldLocation,
				8.f,
				FColor::Red,
				true,
				30.f
			);

				continue;
			}

			cellInfo.GroundLocation = groundLocation;
			cellInfo.GroundNormal = groundNormal;

			const float upDot{ FMath::Clamp(static_cast<float>(FVector::DotProduct(groundNormal, FVector::UpVector)), -1.f, 1.f) };
			cellInfo.SlopeDegree = FMath::RadiansToDegrees(FMath::Acos(upDot));
			cellInfo.WorldLocation = groundLocation;

			if (cellInfo.SlopeDegree > maxWalkableSlopeDegree)
			{
				cellInfo.State = EDeliveryBotGridCellState::Blocked;
				cellInfo.Cost = BIG_NUMBER;

				// 이동 가능한 최대 경사각을 넘는 셀은 이동 불가로 표시한다.
				DrawDebugPoint(
					GetWorld(),
					cellInfo.WorldLocation,
					8.f,
					FColor::Orange,
					true,
					30.f
				);

				continue;
			}

			const bool bBlocked = IsCellBlocked(cellInfo.WorldLocation, robotBoxExtent);
			cellInfo.State = bBlocked
				? EDeliveryBotGridCellState::Blocked
				: EDeliveryBotGridCellState::Free;

			cellInfo.Cost = bBlocked ? BIG_NUMBER : 1.f;
			
			
			const UWorld* world = GetWorld();
			if (!IsValid(world))
				return;
			
			// Debug Draw
			const FColor debugColor = bBlocked ? FColor::Red : FColor::Green;

			// DrawDebugPoint(
			// 	GetWorld(),
			// 	cellInfo.WorldLocation,
			// 	8.f,
			// 	debugColor,
			// 	true,
			// 	30.f
			// );
			// 		
		}
	}
}

void UDeliveryBot_GridSubsystem::SetDynamicBlockedByComponentBounds(const UPrimitiveComponent* obstacleComponent)
{
	if (!IsValid(obstacleComponent))
		return;

	if (GridSizeX <= 0 || GridSizeY <= 0)
		return;

	const FBoxSphereBounds componentBounds{ obstacleComponent->Bounds };
	FVector boundsOrigin{ componentBounds.Origin };
	FVector boundsExtent{ componentBounds.BoxExtent };

	if (boundsExtent.IsNearlyZero())
	{
		SetDynamicBlockedByWorldLocation(boundsOrigin);
		return;
	}

	const float blockMargin{ CellSize * 0.5f };
	boundsExtent.X += blockMargin;
	boundsExtent.Y += blockMargin;

	const FVector minLocation{
		boundsOrigin.X - boundsExtent.X,
		boundsOrigin.Y - boundsExtent.Y,
		boundsOrigin.Z
	};

	const FVector maxLocation{
		boundsOrigin.X + boundsExtent.X,
		boundsOrigin.Y + boundsExtent.Y,
		boundsOrigin.Z
	};

	const FIntPoint minGridIndex{ GetGridIndexByWorldLocation(minLocation) };
	const FIntPoint maxGridIndex{ GetGridIndexByWorldLocation(maxLocation) };

	const int32 minX{ FMath::Clamp(FMath::Min(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1) };
	const int32 maxX{ FMath::Clamp(FMath::Max(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1) };
	const int32 minY{ FMath::Clamp(FMath::Min(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1) };
	const int32 maxY{ FMath::Clamp(FMath::Max(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1) };

	for (int32 y = minY; y <= maxY; ++y)
	{
		for (int32 x = minX; x <= maxX; ++x)
		{
			const FIntPoint gridIndex{ x, y };
			const int32 cellArrayIndex{ GetCellArrayIndexByGridIndex(gridIndex) };

			if (!GridCells.IsValidIndex(cellArrayIndex))
				continue;

			FDeliveryBotGridCellInfo& cellInfo{ GridCells[cellArrayIndex] };
			if (cellInfo.State == EDeliveryBotGridCellState::Blocked)
				continue;

			cellInfo.State = EDeliveryBotGridCellState::DynamicBlocked;
			cellInfo.Cost = BIG_NUMBER;

			// DrawDebugPoint(
			// 	GetWorld(),
			// 	cellInfo.WorldLocation + FVector{ 0.f, 0.f, 50.f },
			// 	16.f,
			// 	FColor::Purple,
			// 	false,
			// 	0.5f
			// );
		}
	}
}

void UDeliveryBot_GridSubsystem::SetDynamicBlockedByWorldLocation(const FVector& worldLocation)
{
	const FIntPoint gridIndex{ GetGridIndexByWorldLocation(worldLocation) };
	if (!IsValidGridIndex(gridIndex))
		return;

	const int32 cellArrayIndex{ GetCellArrayIndexByGridIndex(gridIndex) };
	if (!GridCells.IsValidIndex(cellArrayIndex))
		return;

	FDeliveryBotGridCellInfo& cellInfo{ GridCells[cellArrayIndex] };
	if (cellInfo.State == EDeliveryBotGridCellState::Blocked)
		return;

	cellInfo.State = EDeliveryBotGridCellState::DynamicBlocked;
	cellInfo.Cost = BIG_NUMBER;

	// DrawDebugPoint(
	// 	GetWorld(),
	// 	cellInfo.WorldLocation + FVector{ 0.f, 0.f, 40.f },
	// 	14.f,
	// 	FColor::Purple,
	// 	false,
	// 	0.5f
	// );
}

void UDeliveryBot_GridSubsystem::ClearDynamicBlockedCells()
{
	for (FDeliveryBotGridCellInfo& cellInfo : GridCells)
	{
		if (cellInfo.State != EDeliveryBotGridCellState::DynamicBlocked)
			continue;

		cellInfo.State = EDeliveryBotGridCellState::Free;
		cellInfo.Cost = 1.f;
	}
}

bool UDeliveryBot_GridSubsystem::IsCellBlocked(const FVector& worldLocation, const FVector& robotBoxExtent) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return true;

	const FCollisionShape robotShape = FCollisionShape::MakeBox(robotBoxExtent);

	FVector checkLocation = worldLocation;
	checkLocation.Z += robotBoxExtent.Z + 2.f;

	TArray<FOverlapResult> overlapResults;

	const bool bHasOverlap = world->OverlapMultiByChannel(
		overlapResults,
		checkLocation,
		FQuat::Identity,
		ECC_WorldStatic,
		robotShape
	);

	if (!bHasOverlap)
		return false;

	static const FName ignoreAboutGridTag{ TEXT("IgnoreAboutGrid") };
	static const FName localOnlyObstacleTag{ TEXT("LocalOnlyObstacle") };

	for (const FOverlapResult& overlapResult : overlapResults)
	{
		AActor* overlapActor = overlapResult.GetActor();

		if (!IsValid(overlapActor))
			continue;

		if (overlapActor->ActorHasTag(ignoreAboutGridTag))
			continue;
		if (overlapActor->ActorHasTag(localOnlyObstacleTag))
			continue;

		// NoCollision 태그가 없는 액터가 하나라도 있으면 이동 불가로 처리한다.
		return true;
	}
	return false;
}

bool UDeliveryBot_GridSubsystem::GetGroundInfoByWorldLocation(const FVector& worldLocation, FVector& outGroundLocation,
	FVector& outGroundNormal) const
{
	const UWorld* world{ GetWorld() };
	if (!IsValid(world))
	{
		return false;
	}
	const FVector traceStart{ worldLocation.X, worldLocation.Y, worldLocation.Z + 1000.f };
	const FVector traceEnd{ worldLocation.X, worldLocation.Y, worldLocation.Z - 1000.f };

	FHitResult hitResult;
	const bool bHit{ world->LineTraceSingleByChannel(
		hitResult,
		traceStart,
		traceEnd,
		ECC_WorldStatic
	) };

	if (!bHit)
	{
		return false;
	}

	outGroundLocation = hitResult.Location;
	outGroundNormal = hitResult.ImpactNormal;
	return true;
}

FIntPoint UDeliveryBot_GridSubsystem::GetGridIndexByWorldLocation(const FVector& worldLocation) const
{
	return FIntPoint
	{
		static_cast<int32>(FMath::FloorToInt((worldLocation.X - GridOrigin.X) / CellSize)),
		static_cast<int32>(FMath::FloorToInt((worldLocation.Y - GridOrigin.Y) / CellSize))
	};
}

void UDeliveryBot_GridSubsystem::SetDynamicBlockedByActorBounds(const AActor* obstacleActor)
{
	if (!IsValid(obstacleActor))
		return;

	if (GridSizeX <= 0 || GridSizeY <= 0)
		return;

	FVector boundsOrigin{ FVector::ZeroVector };
	FVector boundsExtent{ FVector::ZeroVector };
	obstacleActor->GetActorBounds(true, boundsOrigin, boundsExtent);

	if (boundsExtent.IsNearlyZero())
	{
		SetDynamicBlockedByWorldLocation(obstacleActor->GetActorLocation());
		return;
	}

	const float blockMargin{ CellSize * DynamicObstacleBlockBound };
	boundsExtent.X += blockMargin;
	boundsExtent.Y += blockMargin;

	const FVector minLocation{
		boundsOrigin.X - boundsExtent.X,
		boundsOrigin.Y - boundsExtent.Y,
		boundsOrigin.Z
	};

	const FVector maxLocation{
		boundsOrigin.X + boundsExtent.X,
		boundsOrigin.Y + boundsExtent.Y,
		boundsOrigin.Z
	};

	const FIntPoint minGridIndex{ GetGridIndexByWorldLocation(minLocation) };
	const FIntPoint maxGridIndex{ GetGridIndexByWorldLocation(maxLocation) };

	const int32 minX{ FMath::Clamp(FMath::Min(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1) };
	const int32 maxX{ FMath::Clamp(FMath::Max(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1) };
	const int32 minY{ FMath::Clamp(FMath::Min(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1) };
	const int32 maxY{ FMath::Clamp(FMath::Max(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1) };

	for (int32 y = minY; y <= maxY; ++y)
	{
		for (int32 x = minX; x <= maxX; ++x)
		{
			const FIntPoint gridIndex{ x, y };
			const int32 cellArrayIndex{ GetCellArrayIndexByGridIndex(gridIndex) };

			if (!GridCells.IsValidIndex(cellArrayIndex))
				continue;

			FDeliveryBotGridCellInfo& cellInfo{ GridCells[cellArrayIndex] };
			if (cellInfo.State == EDeliveryBotGridCellState::Blocked)
				continue;

			cellInfo.State = EDeliveryBotGridCellState::DynamicBlocked;
			cellInfo.Cost = BIG_NUMBER;

			// DrawDebugPoint(
			// 	GetWorld(),
			// 	cellInfo.WorldLocation + FVector{ 0.f, 0.f, 50.f },
			// 	16.f,
			// 	FColor::Purple,
			// 	false,
			// 	0.5f
			// );
		}
	}
}

bool UDeliveryBot_GridSubsystem::GetNearestWalkableWorldLocation(
	const FVector& worldLocation,
	int32 maxSearchRadius,
	FVector& outWorldLocation) const
{
	outWorldLocation = FVector::ZeroVector;

	if (GridSizeX <= 0 || GridSizeY <= 0)
		return false;

	const FIntPoint originGridIndex{ GetGridIndexByWorldLocation(worldLocation) };

	if (IsWalkableGridIndex(originGridIndex))
	{
		outWorldLocation = GetWorldLocationByGridIndex(originGridIndex);
		return true;
	}

	const int32 searchRadius{ FMath::Max(maxSearchRadius, 0) };
	float bestDistanceSquared{ TNumericLimits<float>::Max() };
	bool bFound{ false };

	for (int32 radius{ 1 }; radius <= searchRadius; ++radius)
	{
		for (int32 offsetY{ -radius }; offsetY <= radius; ++offsetY)
		{
			for (int32 offsetX{ -radius }; offsetX <= radius; ++offsetX)
			{
				const bool bIsOuterRing{
					FMath::Abs(offsetX) == radius || FMath::Abs(offsetY) == radius
				};

				if (!bIsOuterRing)
					continue;

				const FIntPoint candidateGridIndex{
					originGridIndex.X + offsetX,
					originGridIndex.Y + offsetY
				};

				if (!IsWalkableGridIndex(candidateGridIndex))
					continue;

				const FVector candidateWorldLocation{
					GetWorldLocationByGridIndex(candidateGridIndex)
				};

				const float distanceSquared{
					static_cast<float>(FVector::DistSquared2D(worldLocation, candidateWorldLocation))
				};

				if (distanceSquared >= bestDistanceSquared)
					continue;

				bestDistanceSquared = distanceSquared;
				outWorldLocation = candidateWorldLocation;
				bFound = true;
			}
		}

		if (bFound)
			return true;
	}

	return false;
}

FVector UDeliveryBot_GridSubsystem::GetWorldLocationByGridIndex(const FIntPoint& gridIndex) const
{
	const FDeliveryBotGridCellInfo* cellInfo{ FindCellInfoByGridIndex(gridIndex) };
	if (cellInfo == nullptr)
	{
		return FVector::ZeroVector;
	}

	return cellInfo->WorldLocation;
}

int32 UDeliveryBot_GridSubsystem::GetCellArrayIndexByGridIndex(const FIntPoint& gridIndex) const
{
	return gridIndex.X + gridIndex.Y * GridSizeX;
}

TArray<FIntPoint> UDeliveryBot_GridSubsystem::GetNeighborGridIndexes(const FIntPoint& gridIndex) const
{
	TArray<FIntPoint> neighborGridIndexes;
	static const TArray<FIntPoint> directions
	{
		FIntPoint{ 1, 0 },
		FIntPoint{ -1, 0 },
		FIntPoint{ 0, 1 },
		FIntPoint{ 0, -1 },
		FIntPoint{ 1, 1 },
		FIntPoint{ 1, -1 },
		FIntPoint{ -1, 1 },
		FIntPoint{ -1, -1 }
	};

	for (const FIntPoint& direction : directions)
	{
		const FIntPoint neighborGridIndex
		{
			gridIndex.X + direction.X,
			gridIndex.Y + direction.Y
		};

		if (!IsWalkableGridIndex(neighborGridIndex))
			continue;
		const bool bDiagonal{ direction.X != 0 && direction.Y != 0 };
		if (bDiagonal)
		{
			const FIntPoint sideGridIndexX{gridIndex.X + direction.X, gridIndex.Y};
			const FIntPoint sideGridIndexY{	gridIndex.X,gridIndex.Y + direction.Y};

			// 대각선 이동 시 양쪽 직선 셀이 막혀 있으면 모서리를 뚫고 지나가는 경로가 생기므로 제외
			if (!IsWalkableGridIndex(sideGridIndexX) || !IsWalkableGridIndex(sideGridIndexY))
				continue;
		}
		
		neighborGridIndexes.Add(neighborGridIndex);
	}
	return neighborGridIndexes;
}

bool UDeliveryBot_GridSubsystem::IsValidGridIndex(const FIntPoint& gridIndex) const
{
	return gridIndex.X >= 0
		&& gridIndex.Y >= 0
		&& gridIndex.X < GridSizeX
		&& gridIndex.Y < GridSizeY;
}

bool UDeliveryBot_GridSubsystem::IsWalkableGridIndex(const FIntPoint& gridIndex) const
{
	const FDeliveryBotGridCellInfo* cellInfo{ FindCellInfoByGridIndex(gridIndex) };
	if (cellInfo == nullptr)
		return false;

	return cellInfo->State == EDeliveryBotGridCellState::Free;
}

const FDeliveryBotGridCellInfo* UDeliveryBot_GridSubsystem::FindCellInfoByGridIndex(const FIntPoint& gridIndex) const
{
	if (!IsValidGridIndex(gridIndex))
		return nullptr;

	const int32 cellArrayIndex{ GetCellArrayIndexByGridIndex(gridIndex) };
	if (!GridCells.IsValidIndex(cellArrayIndex))
		return nullptr;

	return &GridCells[cellArrayIndex];
}

