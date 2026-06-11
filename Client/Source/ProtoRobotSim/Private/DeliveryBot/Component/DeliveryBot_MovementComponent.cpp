// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Component/DeliveryBot_MovementComponent.h"

#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Component/DeliveryBot_LocalAvoidanceComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"


UDeliveryBot_MovementComponent::UDeliveryBot_MovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UDeliveryBot_MovementComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* owner = GetOwner();
	if (!IsValid(owner))
		return;

	LocalAvoidanceComponent = owner->FindComponentByClass<UDeliveryBot_LocalAvoidanceComponent>();
}


void UDeliveryBot_MovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bMoving)
		return;

	MoveAlongPath(DeltaTime);
}

void UDeliveryBot_MovementComponent::SetPath(const TArray<FVector>& pathPoints)
{
	PathPoints = pathPoints;
	CurrentPathIndex = PathPoints.Num() > 1 ? 1 : 0;
	bMoving = PathPoints.IsValidIndex(CurrentPathIndex);

	UE_LOG(LogTemp, Warning, TEXT("SetPath PathCount: %d, CurrentPathIndex: %d, bMoving: %s"),
		PathPoints.Num(),
		CurrentPathIndex,
		bMoving ? TEXT("true") : TEXT("false")
	);
}

void UDeliveryBot_MovementComponent::StopMove()
{
	bMoving = false;
	CurrentPathIndex = 0;
	PathPoints.Reset();
}

void UDeliveryBot_MovementComponent::MoveAlongPath(float deltaTime)
{
	AActor* owner = GetOwner();
	if (!IsValid(owner))
	{
		StopMove();
		return;
	}

	if (!PathPoints.IsValidIndex(CurrentPathIndex))
	{
		StopMove();
		return;
	}

	if (IsArrivedAtCurrentPathPoint())
	{
		++CurrentPathIndex;

		if (!PathPoints.IsValidIndex(CurrentPathIndex))
		{
			StopMove();
			return;
		}
	}

	const FVector ownerLocation = owner->GetActorLocation();
	const FVector targetLocation = PathPoints[CurrentPathIndex];
	FVector direction = targetLocation - ownerLocation;
	direction.Z = 0.f;

	if (direction.IsNearlyZero())
		return;

	direction.Normalize();

	FHitResult obstacleHitResult;
	if (IsValid(LocalAvoidanceComponent) && LocalAvoidanceComponent->HasObstacleAhead(direction, obstacleHitResult))
	{
		MarkObstacleAsDynamicBlocked(obstacleHitResult);

		if (CanRequestReroute())
		{
			RequestOwnerReroute();
		}

		return;
	}

	FVector nextLocation = ownerLocation + direction * MoveSpeed * deltaTime;

	FVector groundLocation = FVector::ZeroVector;
	if (GetGroundLocationByWorldLocation(nextLocation, groundLocation))
	{
		nextLocation.Z = groundLocation.Z + BodyHalfHeight;
	}

	const FRotator nextRotation = direction.Rotation();
	owner->SetActorLocationAndRotation(nextLocation, nextRotation);
}

void UDeliveryBot_MovementComponent::MarkObstacleAsDynamicBlocked(const FHitResult& obstacleHitResult) const
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
		return;

	gridSubsystem->ClearDynamicBlockedCells();

	const UPrimitiveComponent* obstacleComponent = obstacleHitResult.GetComponent();
	if (IsValid(obstacleComponent))
	{
		gridSubsystem->SetDynamicBlockedByComponentBounds(obstacleComponent);
		return;
	}

	FVector obstacleLocation = obstacleHitResult.ImpactPoint;

	if (obstacleLocation.IsNearlyZero())
	{
		obstacleLocation = obstacleHitResult.Location;
	}

	gridSubsystem->SetDynamicBlockedByWorldLocation(obstacleLocation);
}

bool UDeliveryBot_MovementComponent::IsArrivedAtCurrentPathPoint() const
{
	const AActor* owner = GetOwner();
	if (!IsValid(owner))
		return false;

	if (!PathPoints.IsValidIndex(CurrentPathIndex))
		return false;

	const FVector ownerLocation = owner->GetActorLocation();
	const FVector targetLocation = PathPoints[CurrentPathIndex];
	return FVector::DistSquared2D(ownerLocation, targetLocation) <= FMath::Square(AcceptanceRadius);

}

bool UDeliveryBot_MovementComponent::GetGroundLocationByWorldLocation(const FVector& worldLocation,
	FVector& outGroundLocation) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const FVector traceStart = FVector(worldLocation.X, worldLocation.Y, worldLocation.Z + 300.f);
	const FVector traceEnd = FVector(worldLocation.X, worldLocation.Y, worldLocation.Z - 500.f);

	FHitResult hitResult;

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(GetOwner());
	const bool bHit
	{ world->LineTraceSingleByChannel(
		hitResult,
		traceStart,
		traceEnd,
		ECC_WorldStatic,
		queryParams
	)};

	if (!bHit)
		return false;

	outGroundLocation = hitResult.Location;
	return true;
}

bool UDeliveryBot_MovementComponent::CanRequestReroute() const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const float currentTime = static_cast<float>(world->GetTimeSeconds());
	return currentTime - LastRerouteRequestTime >= RerouteCooldownTime;
}

void UDeliveryBot_MovementComponent::RequestOwnerReroute()
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	ADeliveryBot* deliveryBot = Cast<ADeliveryBot>(GetOwner());
	if (!IsValid(deliveryBot))
		return;

	LastRerouteRequestTime = world->GetTimeSeconds();
	// deliveryBot->RequestGlobalPathFromCurrentLocation();
}
