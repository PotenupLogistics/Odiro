// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "DeliveryBot/Actor/DeliveryBot_GridBoundsActor.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
	FDeliveryBotGridCollisionRuleInfo MakeGridSubsystemFallbackCollisionRule(
		FName collisionProfileName,
		EDeliveryBotGridAreaType areaType,
		float cost,
		bool bBlocksMovement)
	{
		FDeliveryBotGridCollisionRuleInfo rule;
		rule.CollisionProfileName = collisionProfileName;
		rule.AreaType = areaType;
		rule.Cost = cost;
		rule.bBlocksMovement = bBlocksMovement;
		return rule;
	}

	const TArray<FDeliveryBotGridCollisionRuleInfo>& GetGridSubsystemFallbackCollisionRules()
	{
		static const TArray<FDeliveryBotGridCollisionRuleInfo> rules =
		{
			MakeGridSubsystemFallbackCollisionRule(FName(TEXT("Walkable")), EDeliveryBotGridAreaType::Walkable, 1.0f, false),
			MakeGridSubsystemFallbackCollisionRule(FName(TEXT("Penalty")), EDeliveryBotGridAreaType::Penalty, 3.0f, false),
			MakeGridSubsystemFallbackCollisionRule(FName(TEXT("Blocked")), EDeliveryBotGridAreaType::Blocked, BIG_NUMBER, true)
		};

		return rules;
	}

	FName ResolveGridCollisionProfileNameForScenarioRegion(EScenarioGroundRegionType regionType)
	{
		switch (regionType)
		{
		case EScenarioGroundRegionType::Penalty:
			return FName(TEXT("Penalty"));
		case EScenarioGroundRegionType::Blocked:
			return FName(TEXT("Blocked"));
		case EScenarioGroundRegionType::Walkable:
		default:
			return FName(TEXT("Walkable"));
		}
	}

	EDeliveryBotGridAreaType ResolveGridAreaTypeForScenarioRegion(EScenarioGroundRegionType regionType)
	{
		switch (regionType)
		{
		case EScenarioGroundRegionType::Penalty:
			return EDeliveryBotGridAreaType::Penalty;
		case EScenarioGroundRegionType::Blocked:
			return EDeliveryBotGridAreaType::Blocked;
		case EScenarioGroundRegionType::Walkable:
		default:
			return EDeliveryBotGridAreaType::Walkable;
		}
	}

	float ResolveFallbackGridCostForAreaType(EDeliveryBotGridAreaType areaType)
	{
		switch (areaType)
		{
		case EDeliveryBotGridAreaType::Penalty:
			return 3.0f;
		case EDeliveryBotGridAreaType::Blocked:
			return BIG_NUMBER;
		case EDeliveryBotGridAreaType::Walkable:
		default:
			return 1.0f;
		}
	}

	// Collects obstacle actors that LiDAR handles outside the static navigation grid.
	void GatherStaticObstacleActorsIgnoredByGrid(const UWorld* world, TArray<const AActor*>& outActors)
	{
		outActors.Reset();
		if (!IsValid(world))
		{
			return;
		}

		for (TActorIterator<AScenarioStaticObstacle> actorIt(world); actorIt; ++actorIt)
		{
			const AScenarioStaticObstacle* obstacleActor = *actorIt;
			if (IsValid(obstacleActor))
			{
				outActors.Add(obstacleActor);
			}
		}
	}

	// Applies the static-grid ignore list to physics queries used during grid classification.
	void AddStaticGridIgnoredActorsToQueryParams(
		const TArray<const AActor*>& staticObstacleActorsIgnoredByGrid,
		FCollisionQueryParams& queryParams)
	{
		for (const AActor* ignoredActor : staticObstacleActorsIgnoredByGrid)
		{
			if (IsValid(ignoredActor))
			{
				queryParams.AddIgnoredActor(ignoredActor);
			}
		}
	}

	// Identifies runtime/editor static obstacle collision primitives regardless of profile name.
	bool IsScenarioStaticObstacleComponent(const UPrimitiveComponent* primitiveComponent)
	{
		return IsValid(primitiveComponent)
			&& IsValid(Cast<AScenarioStaticObstacle>(primitiveComponent->GetOwner()));
	}
}

void UDeliveryBot_GridSubsystem::BuildGridFromBounds(const ADeliveryBot_GridBoundsActor* gridBoundsActor)
{
	if (!IsValid(gridBoundsActor))
		return;

	const UBoxComponent* boundsBox = gridBoundsActor->GetBoundsBox();
	if (!IsValid(boundsBox))
		return;

	if (bDrawDebug)
	{
		FlushPersistentDebugLines(GetWorld());
	}

	CellSize = FMath::Max(gridBoundsActor->GetCellSize(), 1.f);

	const FVector boxExtent = boundsBox->GetScaledBoxExtent();
	const FVector boxLocation = boundsBox->GetComponentLocation();

	GridOrigin = FVector(
		boxLocation.X - boxExtent.X,
		boxLocation.Y - boxExtent.Y,
		boxLocation.Z
	);

	GridSizeX = FMath::Max(static_cast<int32>(FMath::FloorToInt((boxExtent.X * 2.f) / CellSize)), 0);
	GridSizeY = FMath::Max(static_cast<int32>(FMath::FloorToInt((boxExtent.Y * 2.f) / CellSize)), 0);

	GridCells.Reset();
	GridCells.SetNum(GridSizeX * GridSizeY);

	const FVector robotBoxExtent = gridBoundsActor->GetRobotBoxExtent();
	const float maxWalkableSlopeDegree = gridBoundsActor->GetMaxWalkableSlopeDegree();
	const ECollisionChannel gridTraceChannel = gridBoundsActor->GetGridTraceChannel();
	const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules =
		gridBoundsActor->GetCollisionProfileRules();
	TArray<const AActor*> staticObstacleActorsIgnoredByGrid;
	GatherStaticObstacleActorsIgnoredByGrid(GetWorld(), staticObstacleActorsIgnoredByGrid);

	int32 walkableCellCount = 0;
	int32 penaltyCellCount = 0;
	int32 blockedCellCount = 0;
	
	
	for (int32 y = 0; y < GridSizeY; ++y)
	{
		for (int32 x = 0; x < GridSizeX; ++x)
		{
			const int32 index = x + y * GridSizeX;

			FDeliveryBotGridCellInfo& cellInfo = GridCells[index];
			cellInfo = FDeliveryBotGridCellInfo{};
			cellInfo.GridIndex = FIntPoint(x, y);
			cellInfo.WorldLocation = FVector(
				GridOrigin.X + (x + 0.5f) * CellSize,
				GridOrigin.Y + (y + 0.5f) * CellSize,
				GridOrigin.Z
			);

			ClassifyCellByCollisionPreset(cellInfo.WorldLocation, robotBoxExtent, maxWalkableSlopeDegree, gridTraceChannel, collisionRules, staticObstacleActorsIgnoredByGrid, cellInfo);
			switch (cellInfo.AreaType)
			{
			case EDeliveryBotGridAreaType::Walkable:
				++walkableCellCount;
				break;

			case EDeliveryBotGridAreaType::Penalty:
				++penaltyCellCount;
				break;

			case EDeliveryBotGridAreaType::Blocked:
				++blockedCellCount;
				break;

			default:
				break;
			}

			if (bDrawDebug)
			{
				DrawDebugPoint(
					GetWorld(),
					cellInfo.WorldLocation + FVector(0.f, 0.f, 20.f),
					8.f,
					GetDebugColorByAreaType(cellInfo.AreaType),
					true,
					30.f
				);
			}
		}
	}
	
	UE_LOG(
	LogTemp,
	Log,
	TEXT("DeliveryBot Grid Built | Size: %d x %d, Cells: %d, Walkable: %d, Penalty: %d, Blocked: %d"),
	GridSizeX,
	GridSizeY,
	GridCells.Num(),
	walkableCellCount,
	penaltyCellCount,
	blockedCellCount
);
	
	FString gridJson;
	if (BuildGridJson(gridJson))
	{
		UE_LOG(LogTemp, Log, TEXT("DeliveryBot Grid JSON Built | Length: %d"), gridJson.Len());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DeliveryBot Grid JSON build failed."));
	}
}

void UDeliveryBot_GridSubsystem::SetDrawDebugEnabled(bool bEnabled)
{
	bDrawDebug = bEnabled;

	if (!bDrawDebug)
	{
		if (UWorld* world = GetWorld())
		{
			FlushPersistentDebugLines(world);
		}
	}
}

void UDeliveryBot_GridSubsystem::SetDynamicBlockedByComponentBounds(const UPrimitiveComponent* obstacleComponent)
{
	if (!IsValid(obstacleComponent))
		return;

	if (GridSizeX <= 0 || GridSizeY <= 0)
		return;

	const FBoxSphereBounds componentBounds = obstacleComponent->Bounds;
	FVector boundsOrigin = componentBounds.Origin;
	FVector boundsExtent = componentBounds.BoxExtent;

	if (boundsExtent.IsNearlyZero())
	{
		SetDynamicBlockedByWorldLocation(boundsOrigin);
		return;
	}

	const float blockMargin = CellSize * 0.5f;
	boundsExtent.X += blockMargin;
	boundsExtent.Y += blockMargin;

	const FVector minLocation = FVector(
		boundsOrigin.X - boundsExtent.X,
		boundsOrigin.Y - boundsExtent.Y,
		boundsOrigin.Z
	);

	const FVector maxLocation = FVector(
		boundsOrigin.X + boundsExtent.X,
		boundsOrigin.Y + boundsExtent.Y,
		boundsOrigin.Z
	);

	const FIntPoint minGridIndex = GetGridIndexByWorldLocation(minLocation);
	const FIntPoint maxGridIndex = GetGridIndexByWorldLocation(maxLocation);

	const int32 minX = FMath::Clamp(FMath::Min(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1);
	const int32 maxX = FMath::Clamp(FMath::Max(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1);
	const int32 minY = FMath::Clamp(FMath::Min(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1);
	const int32 maxY = FMath::Clamp(FMath::Max(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1);

	for (int32 y = minY; y <= maxY; ++y)
	{
		for (int32 x = minX; x <= maxX; ++x)
		{
			const FIntPoint gridIndex = FIntPoint(x, y);
			const int32 cellArrayIndex = GetCellArrayIndexByGridIndex(gridIndex);

			if (!GridCells.IsValidIndex(cellArrayIndex))
				continue;

			FDeliveryBotGridCellInfo& cellInfo = GridCells[cellArrayIndex];
			if (cellInfo.State == EDeliveryBotGridCellState::Blocked)
				continue;

			cellInfo.State = EDeliveryBotGridCellState::DynamicBlocked;
			cellInfo.Cost = BIG_NUMBER;

			// DrawDebugPoint(
			// 	GetWorld(),
			// 	cellInfo.WorldLocation + FVector(0.f, 0.f, 50.f),
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
	const FIntPoint gridIndex = GetGridIndexByWorldLocation(worldLocation);
	if (!IsValidGridIndex(gridIndex))
		return;

	const int32 cellArrayIndex = GetCellArrayIndexByGridIndex(gridIndex);
	if (!GridCells.IsValidIndex(cellArrayIndex))
		return;

	FDeliveryBotGridCellInfo& cellInfo = GridCells[cellArrayIndex];
	if (cellInfo.State == EDeliveryBotGridCellState::Blocked)
		return;

	cellInfo.State = EDeliveryBotGridCellState::DynamicBlocked;
	cellInfo.Cost = BIG_NUMBER;

	// DrawDebugPoint(
	// 	GetWorld(),
	// 	cellInfo.WorldLocation + FVector(0.f, 0.f, 40.f),
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
		cellInfo.Cost = cellInfo.BaseCost;
	}
}

FIntPoint UDeliveryBot_GridSubsystem::GetGridIndexByWorldLocation(const FVector& worldLocation) const
{
	return FIntPoint(
		static_cast<int32>(FMath::FloorToInt((worldLocation.X - GridOrigin.X) / CellSize)),
		static_cast<int32>(FMath::FloorToInt((worldLocation.Y - GridOrigin.Y) / CellSize))
	);
}

void UDeliveryBot_GridSubsystem::SetDynamicBlockedByActorBounds(const AActor* obstacleActor)
{
	if (!IsValid(obstacleActor))
		return;

	if (GridSizeX <= 0 || GridSizeY <= 0)
		return;

	FVector boundsOrigin = FVector::ZeroVector;
	FVector boundsExtent = FVector::ZeroVector;
	obstacleActor->GetActorBounds(true, boundsOrigin, boundsExtent);

	if (boundsExtent.IsNearlyZero())
	{
		SetDynamicBlockedByWorldLocation(obstacleActor->GetActorLocation());
		return;
	}

	const float blockMargin = CellSize * DynamicObstacleBlockBound;
	boundsExtent.X += blockMargin;
	boundsExtent.Y += blockMargin;

	const FVector minLocation = FVector(
		boundsOrigin.X - boundsExtent.X,
		boundsOrigin.Y - boundsExtent.Y,
		boundsOrigin.Z
	);

	const FVector maxLocation = FVector(
		boundsOrigin.X + boundsExtent.X,
		boundsOrigin.Y + boundsExtent.Y,
		boundsOrigin.Z
	);

	const FIntPoint minGridIndex = GetGridIndexByWorldLocation(minLocation);
	const FIntPoint maxGridIndex = GetGridIndexByWorldLocation(maxLocation);

	const int32 minX = FMath::Clamp(FMath::Min(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1);
	const int32 maxX = FMath::Clamp(FMath::Max(minGridIndex.X, maxGridIndex.X), 0, GridSizeX - 1);
	const int32 minY = FMath::Clamp(FMath::Min(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1);
	const int32 maxY = FMath::Clamp(FMath::Max(minGridIndex.Y, maxGridIndex.Y), 0, GridSizeY - 1);

	for (int32 y = minY; y <= maxY; ++y)
	{
		for (int32 x = minX; x <= maxX; ++x)
		{
			const FIntPoint gridIndex = FIntPoint(x, y);
			const int32 cellArrayIndex = GetCellArrayIndexByGridIndex(gridIndex);

			if (!GridCells.IsValidIndex(cellArrayIndex))
				continue;

			FDeliveryBotGridCellInfo& cellInfo = GridCells[cellArrayIndex];
			if (cellInfo.State == EDeliveryBotGridCellState::Blocked)
				continue;

			cellInfo.State = EDeliveryBotGridCellState::DynamicBlocked;
			cellInfo.Cost = BIG_NUMBER;

			// DrawDebugPoint(
			// 	GetWorld(),
			// 	cellInfo.WorldLocation + FVector(0.f, 0.f, 50.f),
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

	const FIntPoint originGridIndex = GetGridIndexByWorldLocation(worldLocation);

	if (IsWalkableGridIndex(originGridIndex))
	{
		outWorldLocation = GetWorldLocationByGridIndex(originGridIndex);
		return true;
	}

	const int32 searchRadius = FMath::Max(maxSearchRadius, 0);
	float bestDistanceSquared = TNumericLimits<float>::Max();
	bool bFound = false;

	for (int32 radius = 1; radius <= searchRadius; ++radius)
	{
		for (int32 offsetY = -radius; offsetY <= radius; ++offsetY)
		{
			for (int32 offsetX = -radius; offsetX <= radius; ++offsetX)
			{
				const bool bIsOuterRing =
					FMath::Abs(offsetX) == radius || FMath::Abs(offsetY) == radius;

				if (!bIsOuterRing)
					continue;

				const FIntPoint candidateGridIndex = FIntPoint(
					originGridIndex.X + offsetX,
					originGridIndex.Y + offsetY
				);

				if (!IsWalkableGridIndex(candidateGridIndex))
					continue;

				const FVector candidateWorldLocation = GetWorldLocationByGridIndex(candidateGridIndex);

				const float distanceSquared =
					static_cast<float>(FVector::DistSquared2D(worldLocation, candidateWorldLocation));

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

int32 UDeliveryBot_GridSubsystem::GetGridAreaPriority(EDeliveryBotGridAreaType areaType) const
{
	switch (areaType)
	{
	case EDeliveryBotGridAreaType::Blocked:
		return 300;

	case EDeliveryBotGridAreaType::Penalty:
		return 200;

	case EDeliveryBotGridAreaType::Walkable:
		return 100;

	default:
		return 0;
	}
}



FVector UDeliveryBot_GridSubsystem::GetWorldLocationByGridIndex(const FIntPoint& gridIndex) const
{
	const FDeliveryBotGridCellInfo* cellInfo = FindCellInfoByGridIndex(gridIndex);
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
	static const TArray<FIntPoint> directions = []()
	{
		TArray<FIntPoint> result;
		result.Reserve(8);
		result.Add(FIntPoint(1, 0));
		result.Add(FIntPoint(-1, 0));
		result.Add(FIntPoint(0, 1));
		result.Add(FIntPoint(0, -1));
		result.Add(FIntPoint(1, 1));
		result.Add(FIntPoint(1, -1));
		result.Add(FIntPoint(-1, 1));
		result.Add(FIntPoint(-1, -1));
		return result;
	}();

	for (const FIntPoint& direction : directions)
	{
		const FIntPoint neighborGridIndex = FIntPoint(
			gridIndex.X + direction.X,
			gridIndex.Y + direction.Y
		);

		if (!IsWalkableGridIndex(neighborGridIndex))
			continue;
		const bool bDiagonal = direction.X != 0 && direction.Y != 0;
		if (bDiagonal)
		{
			const FIntPoint sideGridIndexX = FIntPoint(gridIndex.X + direction.X, gridIndex.Y);
			const FIntPoint sideGridIndexY = FIntPoint(gridIndex.X, gridIndex.Y + direction.Y);

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
	const FDeliveryBotGridCellInfo* cellInfo = FindCellInfoByGridIndex(gridIndex);
	if (cellInfo == nullptr)
		return false;

	return cellInfo->State == EDeliveryBotGridCellState::Free && cellInfo->AreaType != EDeliveryBotGridAreaType::Blocked;
}

const FDeliveryBotGridCellInfo* UDeliveryBot_GridSubsystem::FindCellInfoByGridIndex(const FIntPoint& gridIndex) const
{
	if (!IsValidGridIndex(gridIndex))
		return nullptr;

	const int32 cellArrayIndex = GetCellArrayIndexByGridIndex(gridIndex);
	if (!GridCells.IsValidIndex(cellArrayIndex))
		return nullptr;

	return &GridCells[cellArrayIndex];
}

const FDeliveryBotGridCollisionRuleInfo* UDeliveryBot_GridSubsystem::FindCollisionRuleByProfileName(const FName& collisionProfileName, const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules) const
{
	for (const FDeliveryBotGridCollisionRuleInfo& rule : collisionRules)
	{
		if (rule.CollisionProfileName == collisionProfileName)
		{
			return &rule;
		}
	}

	for (const FDeliveryBotGridCollisionRuleInfo& rule : GetGridSubsystemFallbackCollisionRules())
	{
		if (rule.CollisionProfileName == collisionProfileName)
		{
			return &rule;
		}
	}

	return nullptr;
}

void UDeliveryBot_GridSubsystem::ApplyWalkableCell(FDeliveryBotGridCellInfo& cellInfo, EDeliveryBotGridAreaType areaType, float cost, const FName& sourceCollisionProfileName) const
{
	cellInfo.State = EDeliveryBotGridCellState::Free;
	cellInfo.AreaType = areaType;
	cellInfo.Cost = FMath::Max(cost, 0.f);
	cellInfo.BaseCost = cellInfo.Cost;
	cellInfo.SourceCollisionProfileName = sourceCollisionProfileName;
}

void UDeliveryBot_GridSubsystem::ApplyBlockedCell(FDeliveryBotGridCellInfo& cellInfo, const FName& sourceCollisionProfileName) const
{
	cellInfo.State = EDeliveryBotGridCellState::Blocked;
	cellInfo.AreaType = EDeliveryBotGridAreaType::Blocked;
	cellInfo.Cost = BIG_NUMBER;
	cellInfo.BaseCost = BIG_NUMBER;
	cellInfo.SourceCollisionProfileName = sourceCollisionProfileName;
}

// 셀 하나를 검사해서 Walkable / Penalty / Blocked로 분류
bool UDeliveryBot_GridSubsystem::ClassifyCellByCollisionPreset(const FVector& cellCenterLocation, const FVector& robotBoxExtent, float maxWalkableSlopeDegree,
	ECollisionChannel gridTraceChannel, const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules, const TArray<const AActor*>& staticObstacleActorsIgnoredByGrid, FDeliveryBotGridCellInfo& outCellInfo) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		ApplyBlockedCell(outCellInfo, TEXT("InvalidWorld"));
		return false;
	}

	if (TryApplyScenarioCorridorSurfaceCell(
		cellCenterLocation,
		robotBoxExtent,
		gridTraceChannel,
		collisionRules,
		staticObstacleActorsIgnoredByGrid,
		outCellInfo))
	{
		return true;
	}

	const FVector traceStart(cellCenterLocation.X, cellCenterLocation.Y, cellCenterLocation.Z + 1000.f);
	const FVector traceEnd(cellCenterLocation.X, cellCenterLocation.Y, cellCenterLocation.Z - 1000.f);

	FHitResult groundHit;
	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(DeliveryBotGridTrace), false);
	AddStaticGridIgnoredActorsToQueryParams(staticObstacleActorsIgnoredByGrid, queryParams);

	const bool bHit = world->LineTraceSingleByChannel(
		groundHit,
		traceStart,
		traceEnd,
		gridTraceChannel,
		queryParams
	);

	if (!bHit || !groundHit.bBlockingHit)
	{
		ApplyBlockedCell(outCellInfo, TEXT("NoGroundHit"));
		return true;
	}

	const UPrimitiveComponent* hitComponent = groundHit.GetComponent();
	if (!IsValid(hitComponent))
	{
		ApplyBlockedCell(outCellInfo, TEXT("InvalidHitComponent"));
		return true;
	}

	const FName profileName = hitComponent->GetCollisionProfileName();
	const FDeliveryBotGridCollisionRuleInfo* rule =
		FindCollisionRuleByProfileName(profileName, collisionRules);

	outCellInfo.GroundLocation = groundHit.Location;
	outCellInfo.GroundNormal = groundHit.ImpactNormal;
	outCellInfo.WorldLocation = groundHit.Location;

	if (rule != nullptr && (rule->bBlocksMovement || rule->AreaType == EDeliveryBotGridAreaType::Blocked))
	{
		ApplyBlockedCell(outCellInfo, profileName);
		return true;
	}

	const float upDot = FMath::Clamp(
		static_cast<float>(FVector::DotProduct(outCellInfo.GroundNormal, FVector::UpVector)),
		-1.f,
		1.f
	);

	outCellInfo.SlopeDegree = FMath::RadiansToDegrees(FMath::Acos(upDot));

	if (outCellInfo.SlopeDegree > maxWalkableSlopeDegree)
	{
		ApplyBlockedCell(outCellInfo, TEXT("TooSteepSlope"));
		return true;
	}

	FName blockingProfileName = NAME_None;
	if (HasBlockingFootprintOverlap(
		outCellInfo.GroundLocation,
		robotBoxExtent,
		gridTraceChannel,
		collisionRules,
		staticObstacleActorsIgnoredByGrid,
		nullptr,
		blockingProfileName))
	{
		ApplyBlockedCell(outCellInfo, blockingProfileName);
		return true;
	}

	if (rule != nullptr)
	{
		ApplyWalkableCell(outCellInfo, rule->AreaType, rule->Cost, rule->CollisionProfileName);
	}
	else
	{
		ApplyWalkableCell(outCellInfo, EDeliveryBotGridAreaType::Walkable, 1.f, TEXT("DefaultWalkable"));
	}

	return true;
}

bool UDeliveryBot_GridSubsystem::TryApplyScenarioCorridorSurfaceCell(
	const FVector& cellCenterLocation,
	const FVector& robotBoxExtent,
	const ECollisionChannel gridTraceChannel,
	const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules,
	const TArray<const AActor*>& staticObstacleActorsIgnoredByGrid,
	FDeliveryBotGridCellInfo& outCellInfo) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		return false;
	}

	FScenarioRuntimeCorridorSurfaceQueryResult surface;
	const AScenarioCorridorRuntimeActor* corridorActor = nullptr;
	for (TActorIterator<AScenarioCorridorRuntimeActor> actorIt(world); actorIt; ++actorIt)
	{
		AScenarioCorridorRuntimeActor* candidateActor = *actorIt;
		if (IsValid(candidateActor)
			&& candidateActor->TryFindSurfaceAtWorldLocation2D(cellCenterLocation, surface))
		{
			corridorActor = candidateActor;
			break;
		}
	}

	if (!IsValid(corridorActor))
	{
		return false;
	}

	const FName profileName = ResolveGridCollisionProfileNameForScenarioRegion(surface.RegionType);
	const FDeliveryBotGridCollisionRuleInfo* rule =
		FindCollisionRuleByProfileName(profileName, collisionRules);
	const EDeliveryBotGridAreaType areaType = rule
		? rule->AreaType
		: ResolveGridAreaTypeForScenarioRegion(surface.RegionType);
	const float cost = rule
		? rule->Cost
		: ResolveFallbackGridCostForAreaType(areaType);

	outCellInfo.GroundLocation = FVector(
		cellCenterLocation.X,
		cellCenterLocation.Y,
		AScenarioCorridorRuntimeActor::GetRuntimeSurfaceTopZCm() + surface.SurfaceZOffsetCm);
	outCellInfo.GroundNormal = FVector::UpVector;
	outCellInfo.WorldLocation = outCellInfo.GroundLocation;
	outCellInfo.SlopeDegree = 0.0f;

	if ((rule != nullptr && rule->bBlocksMovement) || areaType == EDeliveryBotGridAreaType::Blocked)
	{
		ApplyBlockedCell(outCellInfo, profileName);
		return true;
	}

	FName blockingProfileName = NAME_None;
	if (HasBlockingCorridorFootprintOverlap(*corridorActor, outCellInfo.GroundLocation, robotBoxExtent, blockingProfileName))
	{
		ApplyBlockedCell(outCellInfo, blockingProfileName);
		return true;
	}

	if (HasBlockingFootprintOverlap(
		outCellInfo.GroundLocation,
		robotBoxExtent,
		gridTraceChannel,
		collisionRules,
		staticObstacleActorsIgnoredByGrid,
		corridorActor,
		blockingProfileName))
	{
		ApplyBlockedCell(outCellInfo, blockingProfileName);
		return true;
	}

	ApplyWalkableCell(outCellInfo, areaType, cost, profileName);
	return true;
}

bool UDeliveryBot_GridSubsystem::HasBlockingCorridorFootprintOverlap(
	const AScenarioCorridorRuntimeActor& corridorActor,
	const FVector& groundLocation,
	const FVector& robotBoxExtent,
	FName& outBlockingProfileName) const
{
	outBlockingProfileName = NAME_None;

	const FVector2D sampleOffsets[] =
	{
		FVector2D(-robotBoxExtent.X, -robotBoxExtent.Y),
		FVector2D(-robotBoxExtent.X, robotBoxExtent.Y),
		FVector2D(robotBoxExtent.X, -robotBoxExtent.Y),
		FVector2D(robotBoxExtent.X, robotBoxExtent.Y)
	};

	for (const FVector2D& sampleOffset : sampleOffsets)
	{
		const FVector sampleLocation(
			groundLocation.X + sampleOffset.X,
			groundLocation.Y + sampleOffset.Y,
			groundLocation.Z);
		FScenarioRuntimeCorridorSurfaceQueryResult surface;
		if (!corridorActor.TryFindSurfaceAtWorldLocation2D(sampleLocation, surface))
		{
			outBlockingProfileName = TEXT("NoCorridorSurface");
			return true;
		}

		const EDeliveryBotGridAreaType areaType = ResolveGridAreaTypeForScenarioRegion(surface.RegionType);
		if (areaType == EDeliveryBotGridAreaType::Blocked)
		{
			outBlockingProfileName = ResolveGridCollisionProfileNameForScenarioRegion(surface.RegionType);
			return true;
		}
	}

	return false;
}

// 로봇의 실제 크기만큼 박스 overlap을 해서, 해당 셀에 로봇이 들어갈 수 있는지 검사한다.
bool UDeliveryBot_GridSubsystem::HasBlockingFootprintOverlap(const FVector& groundLocation, const FVector& robotBoxExtent, ECollisionChannel gridTraceChannel,
	const TArray<FDeliveryBotGridCollisionRuleInfo>& collisionRules, const TArray<const AActor*>& staticObstacleActorsIgnoredByGrid, const AActor* ignoredActor, FName& outBlockingProfileName) const
{
	outBlockingProfileName = NAME_None;

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return true;

	// 로봇 박스 중심이 바닥 위에 오도록 Z를 올린다.
	const FVector checkLocation = groundLocation + FVector(0.f, 0.f, robotBoxExtent.Z + 2.f);

	const FCollisionShape robotShape = FCollisionShape::MakeBox(robotBoxExtent);

	TArray<FOverlapResult> overlapResults;
	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(DeliveryBotGridOverlap), false);
	AddStaticGridIgnoredActorsToQueryParams(staticObstacleActorsIgnoredByGrid, queryParams);
	if (IsValid(ignoredActor))
	{
		queryParams.AddIgnoredActor(ignoredActor);
	}

	const bool bHasOverlap = world->OverlapMultiByChannel(
		overlapResults,
		checkLocation,
		FQuat::Identity,
		gridTraceChannel,
		robotShape,
		queryParams
	);

	// 아무것도 겹치지 않으면 로봇 크기 기준으로 통과 가능하다.
	if (!bHasOverlap)
		return false;

	for (const FOverlapResult& overlapResult : overlapResults)
	{
		const UPrimitiveComponent* overlapComponent = overlapResult.GetComponent();
		if (!IsValid(overlapComponent))
			continue;

		if (ignoredActor && overlapComponent->GetOwner() == ignoredActor)
			continue;

		if (IsScenarioStaticObstacleComponent(overlapComponent))
		{
			continue;
		}

		const FName profileName = overlapComponent->GetCollisionProfileName();
		const FDeliveryBotGridCollisionRuleInfo* rule =
			FindCollisionRuleByProfileName(profileName, collisionRules);

		if (rule == nullptr)
			continue;

		// 겹친 컴포넌트가 Blocked rule이면 이 셀은 로봇 크기 기준으로 이동 불가다.
		if (rule->bBlocksMovement || rule->AreaType == EDeliveryBotGridAreaType::Blocked)
		{
			outBlockingProfileName = profileName;
			return true;
		}
	}

	return false;
}

FColor UDeliveryBot_GridSubsystem::GetDebugColorByAreaType(EDeliveryBotGridAreaType areaType) const
{
	switch (areaType)
	{
	case EDeliveryBotGridAreaType::Walkable:
		return FColor::Green;

	case EDeliveryBotGridAreaType::Penalty:
		return FColor::Yellow;

	case EDeliveryBotGridAreaType::Blocked:
		return FColor::Red;

	default:
		return FColor::White;
	}
}

FString UDeliveryBot_GridSubsystem::GetGridAreaTypeString(EDeliveryBotGridAreaType areaType) const
{
	switch (areaType)
	{
	case EDeliveryBotGridAreaType::Walkable:
		return TEXT("Walkable");

	case EDeliveryBotGridAreaType::Penalty:
		return TEXT("Penalty");

	case EDeliveryBotGridAreaType::Blocked:
		return TEXT("Blocked");

	default:
		return TEXT("Unknown");
	}
}

bool UDeliveryBot_GridSubsystem::BuildGridJson(FString& outJson) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();

	rootObject->SetNumberField(TEXT("gridSizeX"), GridSizeX);
	rootObject->SetNumberField(TEXT("gridSizeY"), GridSizeY);
	rootObject->SetNumberField(TEXT("cellSizeCm"), CellSize);
	rootObject->SetNumberField(TEXT("cellCount"), GridCells.Num());

	TSharedRef<FJsonObject> originObject = MakeShared<FJsonObject>();
	originObject->SetNumberField(TEXT("x"), GridOrigin.X);
	originObject->SetNumberField(TEXT("y"), GridOrigin.Y);
	originObject->SetNumberField(TEXT("z"), GridOrigin.Z);
	rootObject->SetObjectField(TEXT("originCm"), originObject);

	TArray<TSharedPtr<FJsonValue>> cellValues;
	cellValues.Reserve(GridCells.Num());

	for (const FDeliveryBotGridCellInfo& cellInfo : GridCells)
	{
		TSharedRef<FJsonObject> cellObject = MakeShared<FJsonObject>();

		cellObject->SetNumberField(TEXT("x"), cellInfo.GridIndex.X);
		cellObject->SetNumberField(TEXT("y"), cellInfo.GridIndex.Y);
		cellObject->SetNumberField(TEXT("worldX"), cellInfo.WorldLocation.X);
		cellObject->SetNumberField(TEXT("worldY"), cellInfo.WorldLocation.Y);
		cellObject->SetNumberField(TEXT("worldZ"), cellInfo.WorldLocation.Z);
		cellObject->SetStringField(TEXT("areaType"), GetGridAreaTypeString(cellInfo.AreaType));
		cellObject->SetNumberField(TEXT("cost"), cellInfo.Cost);
		cellObject->SetBoolField(TEXT("blocked"), !IsWalkableGridIndex(cellInfo.GridIndex));
		cellObject->SetStringField(TEXT("sourceCollisionProfile"), cellInfo.SourceCollisionProfileName.ToString());
		cellObject->SetNumberField(TEXT("slopeDegree"), cellInfo.SlopeDegree);

		cellValues.Add(MakeShared<FJsonValueObject>(cellObject));
	}

	rootObject->SetArrayField(TEXT("cells"), cellValues);

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&outJson);

	return FJsonSerializer::Serialize(rootObject, writer);
}
