// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_PathFollowComponent.h"
#include "DrawDebugHelpers.h"

UDeliveryBot_PathFollowComponent::UDeliveryBot_PathFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;


}

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
}

void UDeliveryBot_PathFollowComponent::SetPath(const TArray<FVector>& pathPoints)
{
	PathPoints = pathPoints;
	CurrentPathIndex = PathPoints.Num() > 0 ? 0 : INDEX_NONE;
	bArrived = false;
}

void UDeliveryBot_PathFollowComponent::ClearPath()
{
	PathPoints.Reset();
	CurrentPathIndex = INDEX_NONE;
	bArrived = false;
}

bool UDeliveryBot_PathFollowComponent::HasPath() const
{
	return PathPoints.Num() > 0 && PathPoints.IsValidIndex(CurrentPathIndex);
}

bool UDeliveryBot_PathFollowComponent::HasArrived() const
{
	return bArrived;
}

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

	const FVector lookAheadLocation{ GetLookAheadLocation() };
	const float steering{ GetSteeringToLocation(lookAheadLocation) };
	const float steeringAlpha{ FMath::Clamp(FMath::Abs(steering), 0.f, 1.f) };

	moveCommandInfo.TargetSpeedKmh = FMath::Lerp(
		PathFollowConfigInfo.TargetSpeedKmh,
		PathFollowConfigInfo.MinTurnSpeedKmh,
		steeringAlpha
	);

	moveCommandInfo.Steering = steering;
	moveCommandInfo.Brake = 0.f;
	moveCommandInfo.bBrake = false;

	DrawDebugPathFollow(lookAheadLocation, steering);

	return moveCommandInfo;
}


void UDeliveryBot_PathFollowComponent::UpdateCurrentPathIndex()
{
	const AActor* owner{ GetOwner() };

	if (!IsValid(owner) || PathPoints.Num() == 0)
	{
		return;
	}

	const FVector ownerLocation{ owner->GetActorLocation() };
	const FVector goalLocation{ PathPoints.Last() };

	const float goalAcceptanceDistanceCm{
		PathFollowConfigInfo.GoalAcceptanceDistanceM * 100.f
	};

	if (GetDistance2D(ownerLocation, goalLocation) <= goalAcceptanceDistanceCm)
	{
		bArrived = true;
		CurrentPathIndex = PathPoints.Num() - 1;
		return;
	}

	const float pathPointAcceptanceDistanceCm{
		PathFollowConfigInfo.PathPointAcceptanceDistanceM * 100.f
	};

	while (PathPoints.IsValidIndex(CurrentPathIndex))
	{
		const float distanceToPathPointCm{
			GetDistance2D(ownerLocation, PathPoints[CurrentPathIndex])
		};

		if (distanceToPathPointCm > pathPointAcceptanceDistanceCm)
		{
			break;
		}

		++CurrentPathIndex;
	}

	CurrentPathIndex = FMath::Clamp(CurrentPathIndex, 0, PathPoints.Num() - 1);
}

FVector UDeliveryBot_PathFollowComponent::GetLookAheadLocation() const
{
	const AActor* owner{ GetOwner() };

	if (!IsValid(owner) || !HasPath())
	{
		return FVector::ZeroVector;
	}

	if (PathPoints.Num() == 1)
	{
		return PathPoints[0];
	}

	const FVector ownerLocation{ owner->GetActorLocation() };
	const float lookAheadDistanceCm{ PathFollowConfigInfo.LookAheadDistanceM * 100.f };

	int32 closestSegmentIndex{ FMath::Clamp(CurrentPathIndex - 1, 0, PathPoints.Num() - 2) };
	double closestAlpha{ 0.0 };
	double closestDistanceSquared{ TNumericLimits<double>::Max() };

	for (int32 pathIndex{ closestSegmentIndex }; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		const FVector segmentStart{ PathPoints[pathIndex] };
		const FVector segmentEnd{ PathPoints[pathIndex + 1] };

		FVector segment{ segmentEnd - segmentStart };
		segment.Z = 0.f;

		FVector ownerToStart{ ownerLocation - segmentStart };
		ownerToStart.Z = 0.f;

		const double segmentLengthSquared{ segment.SizeSquared2D() };

		if (segmentLengthSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const double alpha{
			FMath::Clamp(
				FVector::DotProduct(ownerToStart, segment) / segmentLengthSquared,
				0.0,
				1.0
			)
		};

		const FVector closestLocation{ FMath::Lerp(segmentStart, segmentEnd, alpha) };
		const double distanceSquared{ FVector::DistSquared2D(ownerLocation, closestLocation) };

		if (distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			closestSegmentIndex = pathIndex;
			closestAlpha = alpha;
		}
	}

	const FVector currentLocationOnPath{
		FMath::Lerp(PathPoints[closestSegmentIndex], PathPoints[closestSegmentIndex + 1], closestAlpha)
	};

	float remainDistanceCm{ lookAheadDistanceCm };

	for (int32 pathIndex{ closestSegmentIndex }; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		const FVector segmentStart{
			pathIndex == closestSegmentIndex ? currentLocationOnPath : PathPoints[pathIndex]
		};

		const FVector segmentEnd{ PathPoints[pathIndex + 1] };
		const float segmentDistanceCm{ static_cast<float>(FVector::Dist2D(segmentStart, segmentEnd)) };

		if (segmentDistanceCm <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (remainDistanceCm <= segmentDistanceCm)
		{
			const float alpha{ remainDistanceCm / segmentDistanceCm };
			return FMath::Lerp(segmentStart, segmentEnd, alpha);
		}

		remainDistanceCm -= segmentDistanceCm;
	}

	return PathPoints.Last();
}

float UDeliveryBot_PathFollowComponent::GetSteeringToLocation(const FVector& targetLocation) const
{
	const AActor* owner{ GetOwner() };

	if (!IsValid(owner))
	{
		return 0.f;
	}

	FVector forward{ owner->GetActorForwardVector() };
	forward.Z = 0.f;

	if (!forward.Normalize())
	{
		return 0.f;
	}

	FVector targetDirection{ targetLocation - owner->GetActorLocation() };
	targetDirection.Z = 0.f;

	if (!targetDirection.Normalize())
	{
		return 0.f;
	}

	const double crossZ{ FVector::CrossProduct(forward, targetDirection).Z };
	const double dot{ FVector::DotProduct(forward, targetDirection) };
	const double angleRad{ FMath::Atan2(crossZ, dot) };

	const double steering{
		(angleRad / FMath::DegreesToRadians(45.0)) * PathFollowConfigInfo.SteeringSensitivity
	};

	return static_cast<float>(FMath::Clamp(steering, -1.0, 1.0));
}

float UDeliveryBot_PathFollowComponent::GetDistance2D(const FVector& fromLocation, const FVector& toLocation) const
{
	return FVector::Dist2D(fromLocation, toLocation);
}

void UDeliveryBot_PathFollowComponent::DrawDebugPathFollow(const FVector& lookAheadLocation, float steering) const
{
	if (!PathFollowConfigInfo.bDrawDebug)
	{
		return;
	}

	const UWorld* world{ GetWorld() };
	const AActor* owner{ GetOwner() };

	if (world == nullptr || !IsValid(owner))
	{
		return;
	}

	for (int32 pathIndex{ 0 }; pathIndex < PathPoints.Num() - 1; ++pathIndex)
	{
		DrawDebugLine(
			world,
			PathPoints[pathIndex] + FVector{ 0.f, 0.f, 20.f },
			PathPoints[pathIndex + 1] + FVector{ 0.f, 0.f, 20.f },
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
			PathPoints[CurrentPathIndex] + FVector{ 0.f, 0.f, 35.f },
			18.f,
			12,
			FColor::Orange,
			false,
			0.f
		);
	}

	DrawDebugSphere(
		world,
		lookAheadLocation + FVector{ 0.f, 0.f, 45.f },
		22.f,
		12,
		FColor::Yellow,
		false,
		0.f
	);

	DrawDebugLine(
		world,
		owner->GetActorLocation() + FVector{ 0.f, 0.f, 35.f },
		lookAheadLocation + FVector{ 0.f, 0.f, 45.f },
		FColor::Cyan,
		false,
		0.f,
		0,
		3.f
	);
}
