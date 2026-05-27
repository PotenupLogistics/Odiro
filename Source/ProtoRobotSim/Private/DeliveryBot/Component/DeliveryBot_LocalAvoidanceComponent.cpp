
#include "DeliveryBot/Component/DeliveryBot_LocalAvoidanceComponent.h"

#include "DrawDebugHelpers.h"


UDeliveryBot_LocalAvoidanceComponent::UDeliveryBot_LocalAvoidanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}


void UDeliveryBot_LocalAvoidanceComponent::BeginPlay()
{
	Super::BeginPlay();
}


bool UDeliveryBot_LocalAvoidanceComponent::HasObstacleAhead(
	const FVector& moveDirection,
	FHitResult& outHitResult) const
{
	outHitResult = FHitResult{};

	const UWorld* world{ GetWorld() };
	if (!IsValid(world))
		return false;

	const AActor* owner{ GetOwner() };
	if (!IsValid(owner))
		return false;

	FVector forwardVector{ moveDirection };
	forwardVector.Z = 0.f;

	if (forwardVector.IsNearlyZero())
		return false;

	forwardVector.Normalize();

	const FVector ownerLocation{ owner->GetActorLocation() };
	const FVector traceStart{ ownerLocation + FVector::UpVector * TraceHeightOffset };
	const FVector traceEnd{ traceStart + forwardVector * ObstacleTraceDistance };

	const FCollisionShape obstacleShape{ FCollisionShape::MakeBox(ObstacleBoxHalfExtent) };

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);

	FCollisionObjectQueryParams objectQueryParams;
	objectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	objectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	objectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FRotator traceRotation{ forwardVector.Rotation() };
	traceRotation.Pitch = 0.f;
	traceRotation.Roll = 0.f;

	TArray<FHitResult> hitResults;

	const bool bHit{ world->SweepMultiByObjectType(
		hitResults,
		traceStart,
		traceEnd,
		traceRotation.Quaternion(),
		objectQueryParams,
		obstacleShape,
		queryParams
	) };

	bool bFoundObstacle{ false };

	if (bHit)
	{
		for (const FHitResult& hitResult : hitResults)
		{
			const AActor* hitActor{ hitResult.GetActor() };
			if (!IsValid(hitActor))
				continue;

			if (IsIgnoredActor(hitActor))
				continue;

			outHitResult = hitResult;
			bFoundObstacle = true;
			break;
		}
	}

	if (bDrawDebugTrace)
	{
		const FColor debugColor{ bFoundObstacle ? FColor::Red : FColor::Green };

		DrawDebugLine(
			world,
			traceStart,
			traceEnd,
			debugColor,
			false,
			0.f,
			0,
			2.f
		);

		DrawDebugBox(
			world,
			traceEnd,
			ObstacleBoxHalfExtent,
			traceRotation.Quaternion(),
			debugColor,
			false,
			0.f,
			0,
			2.f
		);
	}

	return bFoundObstacle;
}


bool UDeliveryBot_LocalAvoidanceComponent::IsIgnoredActor(const AActor* actor) const
{
	if (!IsValid(actor))
		return true;

	static const FName ignoreAboutGridTag{ TEXT("IgnoreAboutGrid") };
	static const FName staticMapObjectTag{ TEXT("StaticMapObject") };


	return actor->ActorHasTag(ignoreAboutGridTag)
		|| actor->ActorHasTag(staticMapObjectTag);
}

