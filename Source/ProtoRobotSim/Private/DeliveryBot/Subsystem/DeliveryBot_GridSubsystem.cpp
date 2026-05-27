// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"
#include "Engine/OverlapResult.h"

void UDeliveryBot_GridSubsystem::BuildGridFromBounds(const ADeliveryBot_GridBoundsActor* gridBoundsActor)
{
	if (!IsValid(gridBoundsActor))
	{
		return;
	}

	const UBoxComponent* boundsBox = gridBoundsActor->GetBoundsBox();
	if (!IsValid(boundsBox))
	{
		return;
	}

	CellSize = gridBoundsActor->GetCellSize();

	const FVector boxExtent = boundsBox->GetScaledBoxExtent();
	const FVector boxLocation = boundsBox->GetComponentLocation();

	GridOrigin = FVector(
		boxLocation.X - boxExtent.X,
		boxLocation.Y - boxExtent.Y,
		boxLocation.Z
	);

	GridSizeX = FMath::FloorToInt((boxExtent.X * 2.f) / CellSize);
	GridSizeY = FMath::FloorToInt((boxExtent.Y * 2.f) / CellSize);

	GridCells.Reset();
	GridCells.SetNum(GridSizeX * GridSizeY);

	const FVector robotBoxExtent = gridBoundsActor->GetRobotBoxExtent();
	
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

			DrawDebugPoint(
				GetWorld(),
				cellInfo.WorldLocation,
				8.f,
				debugColor,
				true,
				30.f
			);
					
		}
	}
}

bool UDeliveryBot_GridSubsystem::IsCellBlocked(const FVector& worldLocation, const FVector& robotBoxExtent) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		return true;
	}

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
	{
		return false;
	}

	static const FName NoCollisionTag{ TEXT("IgnoreAboutGrid") };

	for (const FOverlapResult& overlapResult : overlapResults)
	{
		AActor* overlapActor = overlapResult.GetActor();
		if (!IsValid(overlapActor))
		{
			continue;
		}

		if (overlapActor->ActorHasTag(NoCollisionTag))
		{
			continue;
		}
		// NoCollision 태그가 없는 액터가 하나라도 있으면 이동 불가로 처리한다.
		return true;
	}
	return false;
}

