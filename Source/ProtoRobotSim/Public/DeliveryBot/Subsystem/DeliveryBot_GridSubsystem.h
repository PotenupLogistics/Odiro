// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeliveryBot_GridSubsystem.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotGridCellState : uint8
{
	Free,
	Blocked,
	DynamicBlocked
};

USTRUCT(BlueprintType)
struct FDeliveryBotGridCellInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EDeliveryBotGridCellState State{ EDeliveryBotGridCellState::Free };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Cost{ 1.f };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector WorldLocation{ FVector::ZeroVector };
	
	// Height
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector GroundLocation{ FVector::ZeroVector };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector GroundNormal{ FVector::UpVector };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float SlopeDegree{ 0.f };
};

class AActor;
class ADeliveryBot_GridBoundsActor;
UCLASS()
class PROTOROBOTSIM_API UDeliveryBot_GridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void BuildGridFromBounds(const ADeliveryBot_GridBoundsActor* gridBoundsActor);

public:
	void SetDynamicBlockedByWorldLocation(const FVector& worldLocation);
	void ClearDynamicBlockedCells();
	void SetDynamicBlockedByActorBounds(const AActor* obstacleActor);
	
	
private:
	bool IsCellBlocked(const FVector& worldLocation, const FVector& robotBoxExtent) const;
	bool GetGroundInfoByWorldLocation(
	const FVector& worldLocation,
	FVector& outGroundLocation,
	FVector& outGroundNormal) const;
	
	
public:
	FIntPoint GetGridIndexByWorldLocation(const FVector& worldLocation) const;
	FVector GetWorldLocationByGridIndex(const FIntPoint& gridIndex) const;
	int32 GetCellArrayIndexByGridIndex(const FIntPoint& gridIndex) const;
	TArray<FIntPoint> GetNeighborGridIndexes(const FIntPoint& gridIndex) const;
	
public:
	bool IsValidGridIndex(const FIntPoint& gridIndex) const;
	bool IsWalkableGridIndex(const FIntPoint& gridIndex) const;

	const FDeliveryBotGridCellInfo* FindCellInfoByGridIndex(const FIntPoint& gridIndex) const;
	
	
private:
	UPROPERTY()
	TArray<FDeliveryBotGridCellInfo> GridCells;

	
private:
	FVector GridOrigin{ FVector::ZeroVector };
	
	int32 GridSizeX{0};
	int32 GridSizeY{0};
	
	float CellSize{100.f};
	
	
};
