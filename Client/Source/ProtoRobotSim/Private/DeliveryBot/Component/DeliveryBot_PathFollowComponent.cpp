// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_PathFollowComponent.h"
#include "DrawDebugHelpers.h"

// 경로 추종 컴포넌트의 기본 Tick 설정을 초기화한다.
UDeliveryBot_PathFollowComponent::UDeliveryBot_PathFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;


}

// 경로 추종에 사용할 속도, 거리, 조향 민감도 설정값을 보정해 저장한다.
void UDeliveryBot_PathFollowComponent::InitializePathFollow(
	const FDeliveryBotPathFollowConfigInfo& pathFollowConfigInfo)
{
	PathFollowConfigInfo = pathFollowConfigInfo;

	PathFollowConfigInfo.TargetSpeedKmh = FMath::Max(PathFollowConfigInfo.TargetSpeedKmh, 0.f);
	PathFollowConfigInfo.LookAheadDistanceM = FMath::Max(PathFollowConfigInfo.LookAheadDistanceM, 0.1f);
	PathFollowConfigInfo.PathPointAcceptanceDistanceM = FMath::Max(PathFollowConfigInfo.PathPointAcceptanceDistanceM, 0.1f);
	PathFollowConfigInfo.GoalAcceptanceDistanceM = FMath::Max(PathFollowConfigInfo.GoalAcceptanceDistanceM, 0.1f);
	PathFollowConfigInfo.SteeringSensitivity = FMath::Max(PathFollowConfigInfo.SteeringSensitivity, 0.f);
	PathFollowConfigInfo.MinTurnSpeedKmh = FMath::Max(PathFollowConfigInfo.MinTurnSpeedKmh, 0.f);
	PathFollowConfigInfo.ObstacleSlowSpeedKmh = FMath::Max(PathFollowConfigInfo.ObstacleSlowSpeedKmh, 0.f);
}

// FVector 배열 경로를 설정하고 추종 상태를 처음부터 다시 시작한다.
void UDeliveryBot_PathFollowComponent::SetPath(const TArray<FVector>& pathPoints)
{
	PathPoints = pathPoints;
	PathPointInfos.Reset();

	CurrentPathIndex = PathPoints.Num() > 0 ? 0 : INDEX_NONE;
	bArrived = false;
}

// 현재 경로와 도착 상태를 모두 초기화한다.
void UDeliveryBot_PathFollowComponent::ClearPath()
{
	PathPoints.Reset();
	PathPointInfos.Reset();
	CurrentPathIndex = INDEX_NONE;
	bArrived = false;
}

// 현재 추종 가능한 경로와 유효한 경로 인덱스가 있는지 확인한다.
bool UDeliveryBot_PathFollowComponent::HasPath() const
{
	return PathPoints.Num() > 0 && PathPoints.IsValidIndex(CurrentPathIndex);
}

// 목표 지점 도착 여부를 반환한다.
bool UDeliveryBot_PathFollowComponent::HasArrived() const
{
	return bArrived;
}

// 현재 경로 상태를 기반으로 목표 속도, 조향, 브레이크 명령을 만든다.
FDeliveryBotMoveCommandInfo UDeliveryBot_PathFollowComponent::BuildMoveCommand(float deltaTime)
{
	FDeliveryBotMoveCommandInfo moveCommandInfo;

	if (!HasPath() || bArrived)
	{
		moveCommandInfo.TargetSpeedKmh = 0.f;
		moveCommandInfo.Steering = 0.f;
		moveCommandInfo.Brake = 1.f;
		moveCommandInfo.bBrake = true;
		return moveCommandInfo;
	}

	UpdateCurrentPathIndex();

	if (bArrived)
	{
		moveCommandInfo.TargetSpeedKmh = 0.f;
		moveCommandInfo.Steering = 0.f;
		moveCommandInfo.Brake = 1.f;
		moveCommandInfo.bBrake = true;
		return moveCommandInfo;
	}

	const FVector lookAheadLocation = GetLookAheadLocation();
	const EDeliveryBotMoveDirectionType moveDirectionType = GetCurrentMoveDirectionType();
	const float steering = GetSteeringToLocation(lookAheadLocation, moveDirectionType);
	const float steeringAlpha = FMath::Clamp(FMath::Abs(steering), 0.f, 1.f);

	moveCommandInfo.TargetSpeedKmh = FMath::Lerp(
		PathFollowConfigInfo.TargetSpeedKmh,
		PathFollowConfigInfo.MinTurnSpeedKmh,
		steeringAlpha
	);

	moveCommandInfo.Steering = steering;
	moveCommandInfo.Brake = 0.f;
	moveCommandInfo.bBrake = false;
	moveCommandInfo.MoveDirectionType = moveDirectionType;

	DrawDebugPathFollow(lookAheadLocation, steering);

	return moveCommandInfo;
}

// 선택된 주행 컨트롤러 타입에 맞는 주행 명령을 만든다.
FDeliveryBotMoveCommandInfo UDeliveryBot_PathFollowComponent::BuildDriveCommand(float deltaTime, const FDeliveryBotNavigationConfigInfo& navigationConfigInfo)
{
	switch (navigationConfigInfo.DriveControllerType)
	{
		case EDeliveryBotDriveControllerType::PathFollow:
		{
			return BuildMoveCommand(deltaTime);
		}
		case EDeliveryBotDriveControllerType::DWA:
		{
			UE_LOG(LogTemp, Warning, TEXT("DeliveryBot DWA is not implemented yet. Fallback to PathFollow."));
			return BuildMoveCommand(deltaTime);
		}
		default:
			return BuildMoveCommand(deltaTime);
	}
}

// 방향 정보가 포함된 경로 포인트를 설정하고 추종용 위치 배열을 갱신한다.
void UDeliveryBot_PathFollowComponent::SetPathPointInfos(const TArray<FDeliveryBotPathPointInfo>& pathPointInfos)
{
	PathPointInfos = pathPointInfos;

	PathPoints.Reset();
	for (const FDeliveryBotPathPointInfo& pathPointInfo : PathPointInfos)
		PathPoints.Add(pathPointInfo.LocationCm);

	CurrentPathIndex = PathPointInfos.Num() > 0 ? 0 : INDEX_NONE;
	bArrived = false;
}

// 방향 정보 경로가 있으면 그 위치를, 없으면 기본 FVector 경로 위치를 반환한다.
FVector UDeliveryBot_PathFollowComponent::GetPathPointLocation(int32 pathIndex) const
{
	if (PathPointInfos.IsValidIndex(pathIndex))
		return PathPointInfos[pathIndex].LocationCm;

	if (PathPoints.IsValidIndex(pathIndex))
		return PathPoints[pathIndex];

	return FVector::ZeroVector;
}

// 현재 차량과 가장 가까운 경로 구간을 기준으로 전진/후진 방향을 반환한다.
EDeliveryBotMoveDirectionType UDeliveryBot_PathFollowComponent::GetCurrentMoveDirectionType() const
{
	if (PathPointInfos.Num() == 0)
		return EDeliveryBotMoveDirectionType::Forward;

	const int32 closestSegmentIndex = GetClosestPathSegmentIndex();

	if (closestSegmentIndex == INDEX_NONE)
	{
		if (PathPointInfos.IsValidIndex(CurrentPathIndex))
			return PathPointInfos[CurrentPathIndex].MoveDirectionType;

		return EDeliveryBotMoveDirectionType::Forward;
	}

	const int32 directionPathIndex = FMath::Clamp(
		closestSegmentIndex + 1,
		0,
		PathPointInfos.Num() - 1);

	return PathPointInfos[directionPathIndex].MoveDirectionType;
}

// 차량 위치에서 가장 가까운 경로 선분의 인덱스를 찾는다.
int32 UDeliveryBot_PathFollowComponent::GetClosestPathSegmentIndex() const
{
	const AActor* owner = GetOwner();

	if (!IsValid(owner) || PathPoints.Num() < 2)
		return INDEX_NONE;

	const FVector ownerLocation = owner->GetActorLocation();
	const int32 startSegmentIndex = FMath::Clamp(CurrentPathIndex - 1, 0, PathPoints.Num() - 2);

	int32 closestSegmentIndex = startSegmentIndex;
	double closestDistanceSquared = TNumericLimits<double>::Max();

	for (int32 pathIndex = startSegmentIndex; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		const FVector segmentStart = GetPathPointLocation(pathIndex);
		const FVector segmentEnd = GetPathPointLocation(pathIndex + 1);

		FVector segment = segmentEnd - segmentStart;
		segment.Z = 0.f;

		FVector ownerToStart = ownerLocation - segmentStart;
		ownerToStart.Z = 0.f;

		const double segmentLengthSquared = segment.SizeSquared2D();

		if (segmentLengthSquared <= KINDA_SMALL_NUMBER)
			continue;

		const double alpha = FMath::Clamp(
			FVector::DotProduct(ownerToStart, segment) / segmentLengthSquared,
			0.0,
			1.0);

		const FVector closestLocation = FMath::Lerp(segmentStart, segmentEnd, alpha);
		const double distanceSquared = FVector::DistSquared2D(ownerLocation, closestLocation);

		if (distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			closestSegmentIndex = pathIndex;
		}
	}

	return closestSegmentIndex;
}

// 차량 위치와 도착 허용 거리를 기준으로 현재 경로 인덱스를 앞으로 진행시킨다.
void UDeliveryBot_PathFollowComponent::UpdateCurrentPathIndex()
{
	const AActor* owner = GetOwner();

	if (!IsValid(owner) || PathPoints.Num() == 0)
		return;

	const FVector ownerLocation = owner->GetActorLocation();
	const int32 goalPathIndex = PathPoints.Num() - 1;
	const FVector goalLocation = GetPathPointLocation(goalPathIndex);

	const float goalAcceptanceDistanceCm = PathFollowConfigInfo.GoalAcceptanceDistanceM * 100.f;

	if (GetDistance2D(ownerLocation, goalLocation) <= goalAcceptanceDistanceCm)
	{
		bArrived = true;
		CurrentPathIndex = goalPathIndex;
		return;
	}

	const float pathPointAcceptanceDistanceCm = PathFollowConfigInfo.PathPointAcceptanceDistanceM * 100.f;

	const int32 closestSegmentIndex = GetClosestPathSegmentIndex();

	if (closestSegmentIndex != INDEX_NONE)
	{
		CurrentPathIndex = FMath::Max(CurrentPathIndex, closestSegmentIndex + 1);
	}

	while (PathPoints.IsValidIndex(CurrentPathIndex))
	{
		const FVector currentPathLocation = GetPathPointLocation(CurrentPathIndex);
		const float distanceToPathPointCm = GetDistance2D(ownerLocation, currentPathLocation);

		if (distanceToPathPointCm > pathPointAcceptanceDistanceCm)
			break;

		++CurrentPathIndex;
	}

	CurrentPathIndex = FMath::Clamp(CurrentPathIndex, 0, goalPathIndex);
}

// 경로 위에서 차량 앞쪽의 LookAhead 목표 위치를 계산한다.
FVector UDeliveryBot_PathFollowComponent::GetLookAheadLocation() const
{
	const AActor* owner = GetOwner();

	if (!IsValid(owner) || !HasPath())
		return FVector::ZeroVector;

	if (PathPoints.Num() == 1)
		return GetPathPointLocation(0);

	const FVector ownerLocation = owner->GetActorLocation();
	const float lookAheadDistanceCm = PathFollowConfigInfo.LookAheadDistanceM * 100.f;

	int32 closestSegmentIndex = FMath::Clamp(CurrentPathIndex - 1, 0, PathPoints.Num() - 2);
	double closestAlpha = 0.0;
	double closestDistanceSquared = TNumericLimits<double>::Max();

	for (int32 pathIndex = closestSegmentIndex; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		const FVector segmentStart = GetPathPointLocation(pathIndex);
		const FVector segmentEnd = GetPathPointLocation(pathIndex + 1);
		
		FVector segment = segmentEnd - segmentStart;
		segment.Z = 0.f;

		FVector ownerToStart = ownerLocation - segmentStart;
		ownerToStart.Z = 0.f;

		const double segmentLengthSquared = segment.SizeSquared2D();

		if (segmentLengthSquared <= KINDA_SMALL_NUMBER)
			continue;

		const double alpha = FMath::Clamp(FVector::DotProduct(ownerToStart, segment) / segmentLengthSquared,0.0,1.0);

		
		const FVector closestLocation = FMath::Lerp(segmentStart, segmentEnd, alpha);
		const double distanceSquared = FVector::DistSquared2D(ownerLocation, closestLocation);

		if (distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			closestSegmentIndex = pathIndex;
			closestAlpha = alpha;
		}
	}

	const FVector currentLocationOnPath
		= FMath::Lerp(GetPathPointLocation(closestSegmentIndex),	GetPathPointLocation(closestSegmentIndex + 1), closestAlpha);

	float remainDistanceCm = lookAheadDistanceCm;

	for (int32 pathIndex = closestSegmentIndex; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		const FVector segmentStart = pathIndex == closestSegmentIndex ? currentLocationOnPath : GetPathPointLocation(pathIndex);
		const FVector segmentEnd = GetPathPointLocation(pathIndex + 1);

		const float segmentDistanceCm = static_cast<float>(FVector::Dist2D(segmentStart, segmentEnd));

		if (segmentDistanceCm <= KINDA_SMALL_NUMBER)
			continue;

		if (remainDistanceCm <= segmentDistanceCm)
		{
			const float alpha = remainDistanceCm / segmentDistanceCm;
			return FMath::Lerp(segmentStart, segmentEnd, alpha);
		}

		remainDistanceCm -= segmentDistanceCm;
	}

	return GetPathPointLocation(PathPoints.Num() - 1);
}

// 전진/후진 방향을 고려해 목표 위치로 향하는 조향 입력값을 계산한다.
float UDeliveryBot_PathFollowComponent::GetSteeringToLocation(
	const FVector& targetLocation,
	EDeliveryBotMoveDirectionType moveDirectionType) const
{
	const AActor* owner = GetOwner();

	if (!IsValid(owner))
		return 0.f;

	FVector moveBaseDirection = owner->GetActorForwardVector();
	float steeringDirectionSign = 1.f;

	if (moveDirectionType == EDeliveryBotMoveDirectionType::Reverse)
	{
		moveBaseDirection *= -1.f;
		steeringDirectionSign = -1.f;
	}

	moveBaseDirection.Z = 0.f;

	if (!moveBaseDirection.Normalize())
		return 0.f;

	FVector targetDirection = targetLocation - owner->GetActorLocation();
	targetDirection.Z = 0.f;

	if (!targetDirection.Normalize())
		return 0.f;

	const double crossZ = FVector::CrossProduct(moveBaseDirection, targetDirection).Z;
	const double dot = FVector::DotProduct(moveBaseDirection, targetDirection);
	const double angleRad = FMath::Atan2(crossZ, dot);

	const double steering =
		(angleRad / FMath::DegreesToRadians(45.0)) *
		PathFollowConfigInfo.SteeringSensitivity *
		steeringDirectionSign;

	return static_cast<float>(FMath::Clamp(steering, -1.0, 1.0));
}

// 전진 기준으로 목표 위치를 향하는 조향 입력값을 계산한다.
float UDeliveryBot_PathFollowComponent::GetSteeringToLocation(const FVector& targetLocation) const
{
	const AActor* owner = GetOwner();

	if (!IsValid(owner))
		return 0.f;

	FVector forward = owner->GetActorForwardVector();
	forward.Z = 0.f;

	if (!forward.Normalize())
		return 0.f;

	FVector targetDirection = targetLocation - owner->GetActorLocation();
	targetDirection.Z = 0.f;

	if (!targetDirection.Normalize())
		return 0.f;

	const double crossZ = FVector::CrossProduct(forward, targetDirection).Z;
	const double dot = FVector::DotProduct(forward, targetDirection);
	const double angleRad = FMath::Atan2(crossZ, dot);

	const double steering = (angleRad / FMath::DegreesToRadians(45.0)) * PathFollowConfigInfo.SteeringSensitivity;

	return static_cast<float>(FMath::Clamp(steering, -1.0, 1.0));
}

// 두 위치 사이의 2D 거리(cm)를 계산한다.
float UDeliveryBot_PathFollowComponent::GetDistance2D(const FVector& fromLocation, const FVector& toLocation) const
{
	return FVector::Dist2D(fromLocation, toLocation);
}

// 경로, 현재 인덱스, LookAhead 위치를 월드에 디버그로 표시한다.
void UDeliveryBot_PathFollowComponent::DrawDebugPathFollow(	const FVector& lookAheadLocation,	float steering) const
{
	if (!PathFollowConfigInfo.bDrawDebug)
		return;

	const UWorld* world = GetWorld();
	const AActor* owner = GetOwner();

	if (world == nullptr || !IsValid(owner))
		return;

	for (int32 pathIndex = 0; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		DrawDebugLine(
			world,
			GetPathPointLocation(pathIndex) + FVector(0.f, 0.f, 20.f),
			GetPathPointLocation(pathIndex + 1) + FVector(0.f, 0.f, 20.f),
			FColor::Blue,
			false,
			0.f,
			0,
			6.f
		);
	}

	if (PathPoints.IsValidIndex(CurrentPathIndex))
	{
		DrawDebugSphere(
			world,
			GetPathPointLocation(CurrentPathIndex) + FVector(0.f, 0.f, 35.f),
			18.f,
			12,
			FColor::Orange,
			false,
			0.f
		);
	}

	DrawDebugSphere(
		world,
		lookAheadLocation + FVector(0.f, 0.f, 45.f),
		22.f,
		12,
		FColor::Yellow,
		false,
		0.f
	);

	DrawDebugLine(
		world,
		owner->GetActorLocation() + FVector(0.f, 0.f, 35.f),
		lookAheadLocation + FVector(0.f, 0.f, 45.f),
		FColor::Cyan,
		false,
		0.f,
		0,
		3.f
	);
}
