// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_GlobalPathComponent.h"
#include "DrawDebugHelpers.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotPathQueueInfo.h"



// 전역 경로 컴포넌트의 기본 Tick 설정을 초기화한다.
UDeliveryBot_GlobalPathComponent::UDeliveryBot_GlobalPathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// 컴포넌트 시작 시 필요한 초기 처리를 수행한다.
void UDeliveryBot_GlobalPathComponent::BeginPlay()
{
	Super::BeginPlay();

}


// 현재 전역 경로 컴포넌트는 Tick 동작 없이 부모 Tick만 호출한다.
void UDeliveryBot_GlobalPathComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

// 그리드 A*를 사용해 시작 위치에서 목표 위치까지 전역 경로를 생성한다.
bool UDeliveryBot_GlobalPathComponent::BuildPathByAStar(const FVector& startLocation, const FVector& goalLocation)
{
	GlobalPath.Reset();
	GlobalPathPointInfos.Reset();
	
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();

	if (!IsValid(gridSubsystem))
		return false;

	FVector fixedStartLocation = startLocation;
	constexpr int32 maxStartSearchRadius = 5;

	FIntPoint startGridIndex = gridSubsystem->GetGridIndexByWorldLocation(fixedStartLocation);

	if (!gridSubsystem->IsWalkableGridIndex(startGridIndex))
	{
		if (!gridSubsystem->GetNearestWalkableWorldLocation(
			startLocation,
			maxStartSearchRadius,
			fixedStartLocation))
		{
			return false;
		}

		startGridIndex = gridSubsystem->GetGridIndexByWorldLocation(fixedStartLocation);
	}

	const FIntPoint goalGridIndex = gridSubsystem->GetGridIndexByWorldLocation(goalLocation);

	if (!gridSubsystem-> IsWalkableGridIndex(goalGridIndex))
		return false;

	TMap<FIntPoint, FDeliveryBotAStarInfo> nodeMap;  // 각 그리드의 GCost, HCost, Parent 정보를 저장
	TSet<FIntPoint> closedSet; //  이미 탐색 완료한 그리드 저장
	FDeliveryBotPathPriorityQueueInfo openQueue;  // 앞으로 탐색할 후보 노드 우선순위 큐

	// 시작 노드
	FDeliveryBotAStarInfo startNodeInfo;
	startNodeInfo.GridIndex = startGridIndex;
	startNodeInfo.GCost = 0.f;
	startNodeInfo.HCost = GetHeuristicCost(startGridIndex, goalGridIndex);
	startNodeInfo.ParentGridIndex = FIntPoint(INDEX_NONE, INDEX_NONE);
	nodeMap.Add(startGridIndex, startNodeInfo);
	
	FDeliveryBotPathQueueNodeInfo startQueueNodeInfo;
	startQueueNodeInfo.GridIndex = startGridIndex;
	startQueueNodeInfo.FCost = startNodeInfo.GetFCost();
	startQueueNodeInfo.HCost = startNodeInfo.HCost;
	openQueue.Push(startQueueNodeInfo);

	while (!openQueue.IsEmpty())
	{
		FDeliveryBotPathQueueNodeInfo currentQueueNodeInfo;
		if (!openQueue.Pop(currentQueueNodeInfo))
			break;

		const FIntPoint currentGridIndex = currentQueueNodeInfo.GridIndex;

		if (closedSet.Contains(currentGridIndex))
			continue;

		// 도착 검사
		if (currentGridIndex == goalGridIndex)
		{
			BuildGlobalPathFromNodeMap(nodeMap, startGridIndex, goalGridIndex, gridSubsystem);

			SmoothGlobalPath(); // 경로 보정
			BuildGlobalPathPointInfosFromGlobalPath(EDeliveryBotMoveDirectionType::Forward);
			DrawGlobalPath();

			return GlobalPath.Num() > 0 && GlobalPathPointInfos.Num() > 0;
		}

		closedSet.Add(currentGridIndex);

		const FDeliveryBotAStarInfo* currentNodeInfo = nodeMap.Find(currentGridIndex);

		if (currentNodeInfo == nullptr)
			continue;

		const TArray<FIntPoint> neighborGridIndexes = gridSubsystem->GetNeighborGridIndexes(currentGridIndex);

		for (const FIntPoint& neighborGridIndex : neighborGridIndexes)
		{
			if (closedSet.Contains(neighborGridIndex))
				continue;

			const float newGCost = currentNodeInfo->GCost + GetMoveCost(currentGridIndex, neighborGridIndex);

			FDeliveryBotAStarInfo* neighborNodeInfo = nodeMap.Find(neighborGridIndex);

			if (neighborNodeInfo != nullptr && newGCost >= neighborNodeInfo->GCost)
				continue;

			FDeliveryBotAStarInfo newNodeInfo;
			newNodeInfo.GridIndex = neighborGridIndex;
			newNodeInfo.GCost = newGCost;
			newNodeInfo.HCost = GetHeuristicCost(neighborGridIndex, goalGridIndex);
			newNodeInfo.ParentGridIndex = currentGridIndex;

			nodeMap.Add(neighborGridIndex, newNodeInfo);

			FDeliveryBotPathQueueNodeInfo neighborQueueNodeInfo;
			neighborQueueNodeInfo.GridIndex = neighborGridIndex;
			neighborQueueNodeInfo.FCost = newNodeInfo.GetFCost();
			neighborQueueNodeInfo.HCost = newNodeInfo.HCost;
			openQueue.Push(neighborQueueNodeInfo);
		}
	}

	return false;
}

// Navigation 설정에 따라 Grid A* 또는 Hybrid A* 경로 생성을 선택한다.
bool UDeliveryBot_GlobalPathComponent::BuildPath(const FVector& startLocation, const FVector& goalLocation, const FDeliveryBotNavigationConfigInfo& navigationConfigInfo)
{
	switch (navigationConfigInfo.PathFinderType)
	{
	case EDeliveryBotPathFinderType::GridAStar:
		return BuildPathByAStar(startLocation, goalLocation);

	default:
		return BuildPathByAStar(startLocation, goalLocation);
	}
}

// 그리드 A*에서 두 그리드 사이의 휴리스틱 비용을 계산한다.
float UDeliveryBot_GlobalPathComponent::GetHeuristicCost(const FIntPoint& fromGridIndex,
                                                         const FIntPoint& toGridIndex) const
{
	const int32 dx = FMath::Abs(fromGridIndex.X - toGridIndex.X);
	const int32 dy = FMath::Abs(fromGridIndex.Y - toGridIndex.Y);

	return static_cast<float>(FMath::Max(dx, dy) * 10);
}

// 그리드 A*에서 직선/대각선 이동 비용을 계산한다.
float UDeliveryBot_GlobalPathComponent::GetMoveCost(const FIntPoint& fromGridIndex, const FIntPoint& toGridIndex) const
{
	const int32 dx = FMath::Abs(fromGridIndex.X - toGridIndex.X);
	const int32 dy = FMath::Abs(fromGridIndex.Y - toGridIndex.Y);

	return dx == 1 && dy == 1 ? 14.f : 10.f;
}

// A* 노드 맵의 부모 정보를 역추적해 GlobalPath 배열을 만든다.
void UDeliveryBot_GlobalPathComponent::BuildGlobalPathFromNodeMap(const TMap<FIntPoint, FDeliveryBotAStarInfo>& nodeMap,
	const FIntPoint& startGridIndex, const FIntPoint& goalGridIndex, const class UDeliveryBot_GridSubsystem* gridSubsystem)
{
	GlobalPath.Reset();

	if (!IsValid(gridSubsystem))
		return;

	const FDeliveryBotAStarInfo* goalNodeInfo = nodeMap.Find(goalGridIndex);
	if (goalNodeInfo == nullptr)
		return;

	TArray<FIntPoint> reverseGridPath;
	FIntPoint currentGridIndex = goalGridIndex;

	bool bFoundStart = false;
	const int32 maxTraceCount = nodeMap.Num();

	for (int32 traceCount = 0; traceCount < maxTraceCount; ++traceCount)
	{
		reverseGridPath.Add(currentGridIndex);

		if (currentGridIndex == startGridIndex)
		{
			bFoundStart = true;
			break;
		}

		const FDeliveryBotAStarInfo* currentNodeInfo = nodeMap.Find(currentGridIndex);
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
		const FVector worldLocation = gridSubsystem->GetWorldLocationByGridIndex(reverseGridPath[pathIndex]);
		GlobalPath.Add(worldLocation);
	}
}

// 생성된 전역 경로와 시작/도착 지점을 디버그로 그린다.
void UDeliveryBot_GlobalPathComponent::DrawGlobalPath() const
{
	if (!bDrawDebug)
		return;

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	if (GlobalPath.Num() < 2)
		return;

	for (int32 pathIndex = 0; pathIndex < GlobalPath.Num() - 1; ++pathIndex)
	{
		const FVector startLocation = GlobalPath[pathIndex] + FVector(0.f, 0.f, 20.f);
		const FVector endLocation = GlobalPath[pathIndex + 1] + FVector(0.f, 0.f, 20.f);
		
		// A*로 생성된 전역 경로를 눈으로 확인하기 위한 Debug Line
		DrawDebugLine(
			world,
			startLocation,
			endLocation,
			FColor::Blue,
			true,
			1.f,
			0,
			5.f
		);
	}

	// 시작 점
	DrawDebugSphere(
		world,
		GlobalPath[0] + FVector(0.f, 0.f, 20.f),
		20.f,
		12,
		FColor::Cyan,
		true,
		30.f
	);

	// 도착 점
	DrawDebugSphere(
		world,
		GlobalPath.Last() + FVector(0.f, 0.f, 20.f),
		20.f,
		12,
		FColor::Magenta,
		true,
		30.f
	);
}

// 전역 경로 디버그 출력 여부를 설정하고 꺼질 때 persistent debug line을 지운다.
void UDeliveryBot_GlobalPathComponent::SetDrawDebugEnabled(bool bEnabled)
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

// 직선 연결 가능한 중간 경로점을 제거해 그리드 A* 경로를 단순화한다.
void UDeliveryBot_GlobalPathComponent::SmoothGlobalPath()
{
	if (GlobalPath.Num() < 3)
	{
		return;
	}

	TArray<FVector> smoothPath;
	int32 currentPathIndex = 0;

	smoothPath.Add(GlobalPath[currentPathIndex]);

	while (currentPathIndex < GlobalPath.Num() - 1)
	{
		int32 nextPathIndex = GlobalPath.Num() - 1;

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

// 두 경로점 사이를 샘플링해 장애물 없이 직선 연결 가능한지 확인한다.
bool UDeliveryBot_GlobalPathComponent::CanConnectPathPoints(const FVector& fromLocation, const FVector& toLocation) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
		return false;

	const FVector deltaLocation = toLocation - fromLocation;
	const float distance2D = static_cast<float>(FVector(deltaLocation.X, deltaLocation.Y, 0.f).Size());
	const int32 sampleCount = FMath::Max(1, FMath::CeilToInt(distance2D / PathSmoothingSampleDistance));

	for (int32 sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const float alpha = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
		const FVector sampleLocation = fromLocation + deltaLocation * alpha;
		const FIntPoint sampleGridIndex = gridSubsystem->GetGridIndexByWorldLocation(sampleLocation);

		if (!gridSubsystem->IsWalkableGridIndex(sampleGridIndex))
			return false;
	}
	return true;
}



// 위치 배열만 있는 경로를 방향 정보가 포함된 PathPointInfo 배열로 변환한다.
void UDeliveryBot_GlobalPathComponent::BuildGlobalPathPointInfosFromGlobalPath(EDeliveryBotMoveDirectionType moveDirectionType)
{
	GlobalPathPointInfos.Reset();

	for (int32 pathIndex = 0; pathIndex < GlobalPath.Num(); ++pathIndex)
	{
		FVector direction = FVector::ForwardVector;

		if (GlobalPath.IsValidIndex(pathIndex + 1))
		{
			direction = GlobalPath[pathIndex + 1] - GlobalPath[pathIndex];
		}
		else if (GlobalPath.IsValidIndex(pathIndex - 1))
		{
			direction = GlobalPath[pathIndex] - GlobalPath[pathIndex - 1];
		}

		direction.Z = 0.f;
		if (!direction.Normalize())
		{
			direction = FVector::ForwardVector;
		}

		FDeliveryBotPathPointInfo pathPointInfo;
		pathPointInfo.LocationCm = GlobalPath[pathIndex];
		pathPointInfo.HeadingRadian = FMath::DegreesToRadians(direction.Rotation().Yaw);
		pathPointInfo.MoveDirectionType = moveDirectionType;

		GlobalPathPointInfos.Add(pathPointInfo);
	}
}

// Hybrid A* 설정값이 탐색 가능한 범위 안에 있도록 보정한다.
FDeliveryBotHybridAStarConfigInfo UDeliveryBot_GlobalPathComponent::NormalizeHybridAStarConfigInfo(	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const
{
	FDeliveryBotHybridAStarConfigInfo normalizedInfo = hybridAStarConfigInfo;

	normalizedInfo.StepDistanceCm = FMath::Max(normalizedInfo.StepDistanceCm, 1.f);
	normalizedInfo.MinTurningRadiusCm = FMath::Max(normalizedInfo.MinTurningRadiusCm, normalizedInfo.StepDistanceCm);
	normalizedInfo.HeadingBinCount = FMath::Max(normalizedInfo.HeadingBinCount, 8);
	normalizedInfo.MaxSearchCount = FMath::Max(normalizedInfo.MaxSearchCount, 15000);
	normalizedInfo.GoalAcceptanceDistanceCm = FMath::Max(normalizedInfo.GoalAcceptanceDistanceCm, normalizedInfo.StepDistanceCm);
	normalizedInfo.GoalAcceptanceAngleDegree = FMath::Clamp(FMath::Max(normalizedInfo.GoalAcceptanceAngleDegree, 25.f), 1.f, 180.f);
	normalizedInfo.ReverseCostMultiplier = FMath::Max(normalizedInfo.ReverseCostMultiplier, 2.2f);
	normalizedInfo.GearSwitchCostPenalty = FMath::Max(normalizedInfo.GearSwitchCostPenalty, 500.f);
	normalizedInfo.MaxContinuousReverseDistanceCm = normalizedInfo.MaxContinuousReverseDistanceCm <= 0.f
		? FMath::Max(250.f, normalizedInfo.StepDistanceCm)
		: FMath::Max(normalizedInfo.MaxContinuousReverseDistanceCm, normalizedInfo.StepDistanceCm);
	normalizedInfo.ReverseStepDistanceScale = FMath::Clamp(normalizedInfo.ReverseStepDistanceScale, 0.2f, 1.f);
	normalizedInfo.TurnCostPenalty = FMath::Max(normalizedInfo.TurnCostPenalty, 0.f);
	normalizedInfo.ReverseTurnCostPenalty = FMath::Max(normalizedInfo.ReverseTurnCostPenalty, 0.f);
	normalizedInfo.TurnSwitchCostPenalty = FMath::Max(normalizedInfo.TurnSwitchCostPenalty, 0.f);

	return normalizedInfo;
}

// Hybrid A* 모션 모델에 따라 사용할 이동 방향 목록을 만든다.
TArray<EDeliveryBotMoveDirectionType> UDeliveryBot_GlobalPathComponent::GetHybridAStarMoveDirectionTypes(
	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const
{
	TArray<EDeliveryBotMoveDirectionType> moveDirectionTypes;

	moveDirectionTypes.Add(EDeliveryBotMoveDirectionType::Forward);

	if (hybridAStarConfigInfo.MotionModelType == EDeliveryBotHybridAStarMotionModelType::ForwardReverse)
	{
		moveDirectionTypes.Add(EDeliveryBotMoveDirectionType::Reverse);
	}

	return moveDirectionTypes;
}

// 이동 방향, 기어 전환, 조향 변화에 따른 Hybrid A* 이동 비용을 계산한다.
float UDeliveryBot_GlobalPathComponent::GetHybridAStarDirectionCost(
	float baseMoveCost,
	EDeliveryBotMoveDirectionType previousMoveDirectionType,
	EDeliveryBotMoveDirectionType currentMoveDirectionType,
	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo,
	float turnDirectionSign,
	float previousTurnDirectionSign) const
{
	float directionCost = FMath::Max(baseMoveCost, 0.f);

	if (currentMoveDirectionType == EDeliveryBotMoveDirectionType::Reverse)
	{
		directionCost *= hybridAStarConfigInfo.ReverseCostMultiplier;
	}

	if (previousMoveDirectionType != currentMoveDirectionType)
	{
		directionCost += hybridAStarConfigInfo.GearSwitchCostPenalty;
	}

	if (FMath::Abs(turnDirectionSign) > KINDA_SMALL_NUMBER)
	{
		directionCost += hybridAStarConfigInfo.TurnCostPenalty;

		if (currentMoveDirectionType == EDeliveryBotMoveDirectionType::Reverse)
		{
			directionCost += hybridAStarConfigInfo.ReverseTurnCostPenalty;
		}
	}

	if (FMath::Abs(previousTurnDirectionSign) > KINDA_SMALL_NUMBER
		&& FMath::Abs(turnDirectionSign) > KINDA_SMALL_NUMBER
		&& FMath::Sign(previousTurnDirectionSign) != FMath::Sign(turnDirectionSign))
	{
		directionCost += hybridAStarConfigInfo.TurnSwitchCostPenalty;
	}

	return directionCost;
}

// Hybrid A* 모션 모델 enum 값을 로그 출력용 문자열로 변환한다.
const TCHAR* UDeliveryBot_GlobalPathComponent::GetHybridAStarMotionModelName(EDeliveryBotHybridAStarMotionModelType motionModelType) const
{
	switch (motionModelType)
	{
	case EDeliveryBotHybridAStarMotionModelType::ForwardOnly:
		return TEXT("ForwardOnly");

	case EDeliveryBotHybridAStarMotionModelType::ForwardReverse:
		return TEXT("ForwardReverse");

	default:
		return TEXT("Unknown");
	}
}

// 라디안 각도를 -PI~PI 범위로 정규화한다.
float UDeliveryBot_GlobalPathComponent::NormalizeRadian(float angleRadian) const
{
	return FMath::UnwindRadians(angleRadian);
}

// 연속 헤딩 라디안을 Hybrid A*에서 사용할 이산 헤딩 인덱스로 변환한다.
int32 UDeliveryBot_GlobalPathComponent::GetHybridAStarHeadingIndex(
	float headingRadian,
	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const
{
	const int32 headingBinCount = FMath::Max(hybridAStarConfigInfo.HeadingBinCount, 1);
	const float normalizedHeadingRadian = NormalizeRadian(headingRadian);
	const float positiveHeadingRadian = normalizedHeadingRadian < 0.f
		? normalizedHeadingRadian + 2.f * PI
		: normalizedHeadingRadian;

	const float headingUnitRadian = 2.f * PI / static_cast<float>(headingBinCount);
	const int32 headingIndex = FMath::FloorToInt(positiveHeadingRadian / headingUnitRadian);

	return FMath::Clamp(headingIndex, 0, headingBinCount - 1);
}

// 이산 헤딩 인덱스를 대표 라디안 각도로 변환한다.
float UDeliveryBot_GlobalPathComponent::GetHybridAStarHeadingRadianByIndex(int32 headingIndex,	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const
{
	const int32 headingBinCount = FMath::Max(hybridAStarConfigInfo.HeadingBinCount, 1);
	const int32 safeHeadingIndex = FMath::Clamp(headingIndex, 0, headingBinCount - 1);
	const float headingUnitRadian = 2.f * PI / static_cast<float>(headingBinCount);

	return NormalizeRadian(static_cast<float>(safeHeadingIndex) * headingUnitRadian);
}


// Hybrid A*에서 목표까지 남은 2D 거리 기반 휴리스틱 비용을 계산한다.
float UDeliveryBot_GlobalPathComponent::GetHybridAStarHeuristicCost(const FVector& fromLocationCm, const FVector& goalLocationCm) const
{
	return static_cast<float>(FVector::Dist2D(fromLocationCm, goalLocationCm));
}

// 현재 위치와 헤딩이 목표 허용 거리/각도 안에 들어왔는지 확인한다.
bool UDeliveryBot_GlobalPathComponent::IsHybridAStarGoalReached(
	const FVector& currentLocationCm,
	float currentHeadingRadian,
	const FVector& goalLocationCm,
	float goalHeadingRadian,
	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const
{
	const float distanceCm = static_cast<float>(FVector::Dist2D(currentLocationCm, goalLocationCm));

	if (distanceCm > hybridAStarConfigInfo.GoalAcceptanceDistanceCm)
		return false;

	const float angleDifferenceRadian = FMath::Abs(
		NormalizeRadian(goalHeadingRadian - currentHeadingRadian));

	const float goalAcceptanceAngleRadian =
		FMath::DegreesToRadians(hybridAStarConfigInfo.GoalAcceptanceAngleDegree);

	return angleDifferenceRadian <= goalAcceptanceAngleRadian;
}

// Hybrid A* 시작 헤딩을 차량 현재 회전값 또는 목표 방향으로 결정한다.
float UDeliveryBot_GlobalPathComponent::GetHybridAStarStartHeadingRadian(
	const FVector& startLocationCm,
	const FVector& goalLocationCm) const
{
	const AActor* owner = GetOwner();

	if (IsValid(owner))
	{
		return FMath::DegreesToRadians(owner->GetActorRotation().Yaw);
	}

	FVector direction = goalLocationCm - startLocationCm;
	direction.Z = 0.f;

	if (!direction.Normalize())
		return 0.f;

	return FMath::DegreesToRadians(direction.Rotation().Yaw);
}


// Hybrid A* 목표 헤딩을 시작점에서 목표점으로 향하는 방향으로 계산한다.
float UDeliveryBot_GlobalPathComponent::GetHybridAStarGoalHeadingRadian(
	const FVector& startLocationCm,
	const FVector& goalLocationCm) const
{
	FVector direction = goalLocationCm - startLocationCm;
	direction.Z = 0.f;

	if (!direction.Normalize())
	{
		return GetHybridAStarStartHeadingRadian(startLocationCm, goalLocationCm);
	}

	return FMath::DegreesToRadians(direction.Rotation().Yaw);
}

// 이동 방향 enum을 전진은 1, 후진은 -1 부호로 변환한다.
float UDeliveryBot_GlobalPathComponent::GetHybridAStarMoveDirectionSign(EDeliveryBotMoveDirectionType moveDirectionType) const
{
	return moveDirectionType == EDeliveryBotMoveDirectionType::Reverse ? -1.f : 1.f;
}

// Hybrid A*에서 좌회전, 직진, 우회전 후보 조향 부호를 반환한다.
TArray<float> UDeliveryBot_GlobalPathComponent::GetHybridAStarTurnDirectionSigns() const
{
	TArray<float> turnDirectionSigns;
	turnDirectionSigns.Reserve(3);

	turnDirectionSigns.Add(-1.f);
	turnDirectionSigns.Add(0.f);
	turnDirectionSigns.Add(1.f);

	return turnDirectionSigns;
}

// 차량의 최소 회전 반경과 이동 방향을 반영해 다음 Hybrid A* 후보 위치를 계산한다.
FVector UDeliveryBot_GlobalPathComponent::CalculateHybridAStarNextLocation(
	const FVector& currentLocationCm,
	float currentHeadingRadian,
	EDeliveryBotMoveDirectionType moveDirectionType,
	float turnDirectionSign,
	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo,
	float& outNextHeadingRadian) const
{
	const float stepDistanceCm = moveDirectionType == EDeliveryBotMoveDirectionType::Reverse
		? hybridAStarConfigInfo.StepDistanceCm * hybridAStarConfigInfo.ReverseStepDistanceScale
		: hybridAStarConfigInfo.StepDistanceCm;

	const float signedStepDistanceCm =
		stepDistanceCm * GetHybridAStarMoveDirectionSign(moveDirectionType);

	FVector nextLocationCm = currentLocationCm;

	if (FMath::Abs(turnDirectionSign) <= KINDA_SMALL_NUMBER)
	{
		nextLocationCm.X += FMath::Cos(currentHeadingRadian) * signedStepDistanceCm;
		nextLocationCm.Y += FMath::Sin(currentHeadingRadian) * signedStepDistanceCm;

		outNextHeadingRadian = NormalizeRadian(currentHeadingRadian);
		return nextLocationCm;
	}

	const float minTurningRadiusCm = FMath::Max(hybridAStarConfigInfo.MinTurningRadiusCm, 1.f);
	const float safeTurnDirectionSign = FMath::Clamp(turnDirectionSign, -1.f, 1.f);

	const float headingDeltaRadian =
		(signedStepDistanceCm / minTurningRadiusCm) * safeTurnDirectionSign;

	outNextHeadingRadian = NormalizeRadian(currentHeadingRadian + headingDeltaRadian);

	const float signedRadiusCm = minTurningRadiusCm / safeTurnDirectionSign;

	nextLocationCm.X += signedRadiusCm *
		(FMath::Sin(outNextHeadingRadian) - FMath::Sin(currentHeadingRadian));

	nextLocationCm.Y += -signedRadiusCm *
		(FMath::Cos(outNextHeadingRadian) - FMath::Cos(currentHeadingRadian));

	return nextLocationCm;
}

// Hybrid A* 후보 이동 구간이 그리드 장애물을 통과하지 않는지 샘플링으로 확인한다.
bool UDeliveryBot_GlobalPathComponent::CanUseHybridAStarSegment(
	const FVector& fromLocationCm,
	const FVector& toLocationCm,
	const UDeliveryBot_GridSubsystem* gridSubsystem) const
{
	if (!IsValid(gridSubsystem))
		return false;

	const float distanceCm = static_cast<float>(FVector::Dist2D(fromLocationCm, toLocationCm));
	const float sampleDistanceCm = FMath::Max(PathSmoothingSampleDistance, 1.f);
	const int32 sampleCount = FMath::Max(1, FMath::CeilToInt(distanceCm / sampleDistanceCm));

	for (int32 sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const float alpha = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
		const FVector sampleLocationCm = FMath::Lerp(fromLocationCm, toLocationCm, alpha);
		const FIntPoint sampleGridIndex = gridSubsystem->GetGridIndexByWorldLocation(sampleLocationCm);

		if (!gridSubsystem->IsWalkableGridIndex(sampleGridIndex))
			return false;
	}

	return true;
}
