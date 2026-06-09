// Fill out your copyright notice in the Description page of Project Settings.


#include "DeliveryBot/Actor/DeliveryBot_SimpleMesh.h"

#include "DeliveryBot/Actor/DeliveryBot_PathPoint.h"
#include "DeliveryBot/Component/DeliveryBot_GlobalPathComponent.h"
#include "DeliveryBot/Component/DeliveryBot_LocalAvoidanceComponent.h"
#include "DeliveryBot/Component/DeliveryBot_MovementComponent.h"
#include "Kismet/GameplayStatics.h"


ADeliveryBot_SimpleMesh::ADeliveryBot_SimpleMesh()
{
	PrimaryActorTick.bCanEverTick = false;
	Tags.Add(TEXT("IgnoreAboutGrid"));

	LocalAvoidanceComponent = CreateDefaultSubobject<UDeliveryBot_LocalAvoidanceComponent>(TEXT("LocalAvoidanceComponent"));
	GlobalPathComponent = CreateDefaultSubobject<UDeliveryBot_GlobalPathComponent>(TEXT("GlobalPathComponent"));
	MovementComponent = CreateDefaultSubobject<UDeliveryBot_MovementComponent>(TEXT("MovementComponent"));
}

void ADeliveryBot_SimpleMesh::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRequestPathPointsOnBeginPlay)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ADeliveryBot_SimpleMesh::RequestGlobalPathByPathPoints);
	}
}

void ADeliveryBot_SimpleMesh::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADeliveryBot_SimpleMesh::RequestGlobalPathByPathPoints()
{
	FVector startLocation = FVector::ZeroVector;
	FVector goalLocation = FVector::ZeroVector;

	if (!GetPathPointLocations(startLocation, goalLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("DeliveryBot path point start or goal is missing."));
		return;
	}

	CachedGoalLocation = goalLocation;
	bHasCachedGoalLocation = true;

	const float bodyHalfHeight = 25.f;
	const FVector botStartLocation = FVector(
		startLocation.X,
		startLocation.Y,
		startLocation.Z + bodyHalfHeight
	);

	SetActorLocation(botStartLocation);

	BuildGlobalPathAndStartMove(startLocation, CachedGoalLocation);
}

bool ADeliveryBot_SimpleMesh::GetPathPointLocations(FVector& outStartLocation, FVector& outGoalLocation) const
{
	const UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		return false;
	}

	TArray<AActor*> pathPointActors;
	UGameplayStatics::GetAllActorsOfClass(world, ADeliveryBot_PathPoint::StaticClass(), pathPointActors);

	bool bFoundStart = false;
	bool bFoundGoal = false;

	for (AActor* pathPointActor : pathPointActors)
	{
		ADeliveryBot_PathPoint* deliveryBotPathPointActor = Cast<ADeliveryBot_PathPoint>(pathPointActor);
		if (!IsValid(deliveryBotPathPointActor))
		{
			continue;
		}

		if (deliveryBotPathPointActor->GetPathPointType() == EDeliveryBotPathPointType::Start)
		{
			outStartLocation = deliveryBotPathPointActor->GetActorLocation();
			bFoundStart = true;
			continue;
		}

		if (deliveryBotPathPointActor->GetPathPointType() == EDeliveryBotPathPointType::Goal)
		{
			outGoalLocation = deliveryBotPathPointActor->GetActorLocation();
			bFoundGoal = true;
			continue;
		}
	}

	return bFoundStart && bFoundGoal;
}

bool ADeliveryBot_SimpleMesh::BuildGlobalPathAndStartMove(const FVector& startLocation, const FVector& goalLocation)
{
	if (!IsValid(GlobalPathComponent))
		return false;

	if (!IsValid(MovementComponent))
		return false;

	const bool bSuccess = GlobalPathComponent->BuildPathByAStar(startLocation, goalLocation);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeliveryBot A* path build failed."));
		return false;
	}

	MovementComponent->SetPath(GlobalPathComponent->GetGlobalPath());
	CachedGoalLocation = goalLocation;
	bHasCachedGoalLocation = true;
	return true;
}

void ADeliveryBot_SimpleMesh::SetAutoRequestPathPointsOnBeginPlay(bool bEnabled)
{
	bAutoRequestPathPointsOnBeginPlay = bEnabled;
}

void ADeliveryBot_SimpleMesh::RequestGlobalPathFromCurrentLocation()
{
	if (!bHasCachedGoalLocation)
		return;

	const FVector currentLocation = GetActorLocation();

	BuildGlobalPathAndStartMove(currentLocation, CachedGoalLocation);
}

