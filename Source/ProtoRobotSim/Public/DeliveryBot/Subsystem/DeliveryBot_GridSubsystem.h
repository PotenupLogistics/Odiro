// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Shared/Struct/DeliveryBot/Navigation/DeliveryBotGridInfo.h"
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
	
	// Cell Info
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntPoint GridIndex{ FIntPoint::ZeroValue };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EDeliveryBotGridAreaType AreaType{ EDeliveryBotGridAreaType::Walkable };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BaseCost{ 1.f };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName SourceCollisionProfileName{ NAME_None };
};

class UPrimitiveComponent;
class AActor;
class ADeliveryBot_GridBoundsActor;
UCLASS()
class PROTOROBOTSIM_API UDeliveryBot_GridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void BuildGridFromBounds(const ADeliveryBot_GridBoundsActor* gridBoundsActor);
	void SetDrawDebugEnabled(bool bEnabled);

public:
	void SetDynamicBlockedByComponentBounds(const UPrimitiveComponent* obstacleComponent);
	void SetDynamicBlockedByWorldLocation(const FVector& worldLocation);
	void ClearDynamicBlockedCells();
	void SetDynamicBlockedByActorBounds(const AActor* obstacleActor);
	bool GetNearestWalkableWorldLocation(const FVector& worldLocation, int32 maxSearchRadius, FVector& outWorldLocation) const;
	int32 GetGridAreaPriority(EDeliveryBotGridAreaType areaType) const;
	bool BuildGridJson(FString& outJson) const;
	
	
public:
	// Grid가 정상적으로 생성되어 Python 서버에 보낼 수 있는 상태인지 확인한다.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Grid")
	bool HasBuiltGrid() const
	{
		return GridSizeX > 0 && GridSizeY > 0 && GridCells.Num() > 0;
	}

	// 현재 생성된 Grid cell 개수를 반환한다. 로그와 검증용으로 사용한다.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Grid")
	int32 GetGridCellCount() const
	{
		return GridCells.Num();
	}
	
private:
	const FDeliveryBotGridCollisionRuleInfo* FindCollisionRuleByProfileName(const FName& collisionProfileName, const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules) const;

	void ApplyWalkableCell(	FDeliveryBotGridCellInfo& cellInfo,	EDeliveryBotGridAreaType areaType,	float cost,	const FName& sourceCollisionProfileName) const;

	void ApplyBlockedCell(FDeliveryBotGridCellInfo& cellInfo, const FName& sourceCollisionProfileName) const;

	bool ClassifyCellByCollisionPreset(const FVector& cellCenterLocation, const FVector& robotBoxExtent,	float maxWalkableSlopeDegree, ECollisionChannel gridTraceChannel,
		const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules, FDeliveryBotGridCellInfo& outCellInfo) const;

	bool HasBlockingFootprintOverlap(const FVector& groundLocation,	const FVector& robotBoxExtent, ECollisionChannel gridTraceChannel, 
		const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules, FName& outBlockingProfileName) const;
	
	FColor GetDebugColorByAreaType(EDeliveryBotGridAreaType areaType) const;
	
	FString GetGridAreaTypeString(EDeliveryBotGridAreaType areaType) const;
	
	
public:
	FIntPoint GetGridIndexByWorldLocation(const FVector& worldLocation) const;
	FVector GetWorldLocationByGridIndex(const FIntPoint& gridIndex) const;
	int32 GetCellArrayIndexByGridIndex(const FIntPoint& gridIndex) const;
	TArray<FIntPoint> GetNeighborGridIndexes(const FIntPoint& gridIndex) const;
	
public:
	bool IsValidGridIndex(const FIntPoint& gridIndex) const;
	bool IsWalkableGridIndex(const FIntPoint& gridIndex) const;

	const FDeliveryBotGridCellInfo* FindCellInfoByGridIndex(const FIntPoint& gridIndex) const;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Grid")
	float DynamicObstacleBlockBound{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Grid")
	bool bDrawDebug{ true };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Grid")
	float CellSize{50.f};  // Grid 사이즈 
	
private:
	UPROPERTY()
	TArray<FDeliveryBotGridCellInfo> GridCells;

	
private:
	FVector GridOrigin{ FVector::ZeroVector };
	
	int32 GridSizeX{0};
	int32 GridSizeY{0};
	

	
	
};
