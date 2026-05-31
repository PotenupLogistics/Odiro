// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_PathFollowComponent.h"


UDeliveryBot_PathFollowComponent::UDeliveryBot_PathFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;


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

	moveCommandInfo.TargetSpeedKmh = PathFollowConfigInfo.TargetSpeedKmh;
	moveCommandInfo.Steering = GetSteeringToLocation(lookAheadLocation);
	moveCommandInfo.Brake = 0.f;
	moveCommandInfo.bBrake = false;

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

	const FVector ownerLocation{ owner->GetActorLocation() };

	const float lookAheadDistanceCm{
		PathFollowConfigInfo.LookAheadDistanceM * 100.f
	};

	for (int32 pathIndex{ CurrentPathIndex }; pathIndex < PathPoints.Num(); ++pathIndex)
	{
		const float distanceCm{
			GetDistance2D(ownerLocation, PathPoints[pathIndex])
		};

		if (distanceCm >= lookAheadDistanceCm)
		{
			return PathPoints[pathIndex];
		}
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

	double crossZ{ FVector::CrossProduct(forward, targetDirection).Z };
	double dot{ FVector::DotProduct(forward, targetDirection) };
	double angleRad{ FMath::Atan2(crossZ, dot) };

	double steering{
		(angleRad / FMath::DegreesToRadians(45.f)) * PathFollowConfigInfo.SteeringSensitivity
	};

	return FMath::Clamp(steering, -1.f, 1.f);
}

float UDeliveryBot_PathFollowComponent::GetDistance2D(const FVector& fromLocation, const FVector& toLocation) const
{
	return FVector::Dist2D(fromLocation, toLocation);
}


