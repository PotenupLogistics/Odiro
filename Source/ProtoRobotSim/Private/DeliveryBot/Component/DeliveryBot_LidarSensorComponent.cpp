#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"

#include "DrawDebugHelpers.h"

UDeliveryBot_LidarSensorComponent::UDeliveryBot_LidarSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeliveryBot_LidarSensorComponent::InitializeLidar(
	const FDeliveryBotLidarSensorConfigInfo& lidarSensorConfigInfo)
{
	LidarSensorConfigInfo = lidarSensorConfigInfo;

	LidarSensorConfigInfo.ScanRangeM = FMath::Max(LidarSensorConfigInfo.ScanRangeM, 0.f);
	LidarSensorConfigInfo.AngleStepDegree = FMath::Max(LidarSensorConfigInfo.AngleStepDegree, 1.f);
	LidarSensorConfigInfo.SensorHeightM = FMath::Max(LidarSensorConfigInfo.SensorHeightM, 0.f);
	LidarSensorConfigInfo.FrontHalfAngleDegree = FMath::Clamp(LidarSensorConfigInfo.FrontHalfAngleDegree, 0.f, 180.f);
	LidarSensorConfigInfo.StopDistanceM = FMath::Max(LidarSensorConfigInfo.StopDistanceM, 0.f);
	LidarSensorConfigInfo.SlowDownDistanceM = FMath::Max(LidarSensorConfigInfo.SlowDownDistanceM,LidarSensorConfigInfo.StopDistanceM + 0.1f);
}

FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::ScanLidar() const
{
	FDeliveryBotLidarScanInfo scanInfo;

	const AActor* owner{ GetOwner() };

	if (!IsValid(owner))
	{
		return scanInfo;
	}

	const FVector sensorLocationCm{
		owner->GetActorLocation() + FVector{ 0.f, 0.f, LidarSensorConfigInfo.SensorHeightM * 100.f }
	};

	scanInfo.SensorLocationCm = sensorLocationCm;

	const float angleStepDegree{ FMath::Max(LidarSensorConfigInfo.AngleStepDegree, 1.f) };
	const float scanRangeCm{ LidarSensorConfigInfo.ScanRangeM * 100.f };

	int32 rayIndex{ 0 };

	for (float yawDegree{ 0.f }; yawDegree < 360.f; yawDegree += angleStepDegree)
	{
		const FVector localDirection{ FRotator{ 0.f, yawDegree, 0.f }.Vector() };
		const FVector worldDirection{ owner->GetActorRotation().RotateVector(localDirection) };
		const FVector endLocationCm{ sensorLocationCm + worldDirection * scanRangeCm };

		FHitResult hitResult;
		const bool bHit{ TraceLidarRay(sensorLocationCm, endLocationCm, hitResult) };

		DrawDebugLidarRay(
			sensorLocationCm,
			endLocationCm,
			bHit ? &hitResult : nullptr
		);

		if (bHit || LidarSensorConfigInfo.bStoreMissedRays)
		{
			scanInfo.RayInfos.Add(
				MakeRayInfo(
					rayIndex,
					yawDegree,
					sensorLocationCm,
					endLocationCm,
					bHit ? &hitResult : nullptr
				)
			);
		}

		++rayIndex;
	}

	return scanInfo;
}

bool UDeliveryBot_LidarSensorComponent::TraceLidarRay(
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	FHitResult& outHitResult) const
{
	UWorld* world{ GetWorld() };
	const AActor* owner{ GetOwner() };

	if (world == nullptr || !IsValid(owner))
	{
		return false;
	}

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);

	TArray<FHitResult> hitResults;

	const bool bHit{
		world->LineTraceMultiByChannel(
			hitResults,
			startLocationCm,
			endLocationCm,
			LidarSensorConfigInfo.TraceChannel,
			queryParams
		)
	};

	if (!bHit)
	{
		return false;
	}

	for (const FHitResult& hitResult : hitResults)
	{
		const AActor* hitActor{ hitResult.GetActor() };

		if (ShouldIgnoreActor(hitActor))
		{
			continue;
		}

		outHitResult = hitResult;
		return true;
	}

	return false;
}

bool UDeliveryBot_LidarSensorComponent::ShouldIgnoreActor(const AActor* actor) const
{
	if (!IsValid(actor))
	{
		return true;
	}

	for (const FName& ignoreTag : LidarSensorConfigInfo.IgnoreTags)
	{
		if (actor->ActorHasTag(ignoreTag))
		{
			return true;
		}
	}

	return false;
}

FDeliveryBotLidarRayInfo UDeliveryBot_LidarSensorComponent::MakeRayInfo(
	int32 rayIndex,
	float rayYawDegree,
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	const FHitResult* hitResult) const
{
	FDeliveryBotLidarRayInfo rayInfo;

	rayInfo.RayIndex = rayIndex;
	rayInfo.RayYawDegree = rayYawDegree;
	rayInfo.StartLocationCm = startLocationCm;
	rayInfo.EndLocationCm = endLocationCm;
	rayInfo.DistanceM = FVector::Dist(startLocationCm, endLocationCm) / 100.f;

	if (hitResult == nullptr || !IsValid(hitResult->GetActor()))
	{
		rayInfo.HitLocationCm = endLocationCm;
		return rayInfo;
	}

	AActor* hitActor{ hitResult->GetActor() };

	rayInfo.bHit = true;
	rayInfo.HitActor = hitActor;
	rayInfo.ActorName = hitActor->GetName();
	rayInfo.ActorTags = hitActor->Tags;
	rayInfo.HitLocationCm = hitResult->ImpactPoint;
	rayInfo.DistanceM = FVector::Dist(startLocationCm, hitResult->ImpactPoint) / 100.f;

	return rayInfo;
}

void UDeliveryBot_LidarSensorComponent::DrawDebugLidarRay(
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	const FHitResult* hitResult) const
{
	if (!LidarSensorConfigInfo.bDrawDebug)
	{
		return;
	}

	UWorld* world{ GetWorld() };

	if (world == nullptr)
	{
		return;
	}

	const bool bHit{ hitResult != nullptr };
	const FVector drawEndLocationCm{ bHit ? hitResult->ImpactPoint : endLocationCm };
	const FColor drawColor{ bHit ? FColor::Red : FColor::Green };

	DrawDebugLine(
		world,
		startLocationCm,
		drawEndLocationCm,
		drawColor,
		false,
		0.f,
		0,
		1.f

	);
	if (bHit)
	{
		DrawDebugPoint(
			world,
			hitResult->ImpactPoint,
			8.f,
			FColor::Yellow,
			false,
			0.f
		);
	}
}

float UDeliveryBot_LidarSensorComponent::GetSignedYawDegree(float yawDegree) const
{
	float normalizedYawDegree{ FMath::Fmod(yawDegree, 360.f) };

	if (normalizedYawDegree < 0.f)
	{
		normalizedYawDegree += 360.f;
	}

	if (normalizedYawDegree > 180.f)
	{
		normalizedYawDegree -= 360.f;
	}

	return normalizedYawDegree;
}

bool UDeliveryBot_LidarSensorComponent::IsFrontYaw(float yawDegree) const
{
	const float signedYawDegree{ GetSignedYawDegree(yawDegree) };
	const float frontHalfAngleDegree{ FMath::Max(LidarSensorConfigInfo.FrontHalfAngleDegree, 0.f) };

	return FMath::Abs(signedYawDegree) <= frontHalfAngleDegree;
}

TArray<FDeliveryBotLidarDetectedObjectInfo> UDeliveryBot_LidarSensorComponent::BuildDetectedObjects(
	const FDeliveryBotLidarScanInfo& scanInfo) const
{
	TMap<TObjectPtr<AActor>, FDeliveryBotLidarDetectedObjectInfo> objectMap;

	for (const FDeliveryBotLidarRayInfo& rayInfo : scanInfo.RayInfos)
	{
		if (!rayInfo.bHit || !IsValid(rayInfo.HitActor))
		{
			continue;
		}

		const bool bFrontRay{ IsFrontYaw(rayInfo.RayYawDegree) };
		FDeliveryBotLidarDetectedObjectInfo* objectInfo{ objectMap.Find(rayInfo.HitActor) };

		if (objectInfo == nullptr)
		{
			FDeliveryBotLidarDetectedObjectInfo newObjectInfo;
			newObjectInfo.DetectedActor = rayInfo.HitActor;
			newObjectInfo.ActorName = rayInfo.ActorName;
			newObjectInfo.ActorTags = rayInfo.ActorTags;
			newObjectInfo.ClosestHitLocationCm = rayInfo.HitLocationCm;
			newObjectInfo.ClosestDistanceM = rayInfo.DistanceM;
			newObjectInfo.ClosestRayYawDegree = rayInfo.RayYawDegree;
			newObjectInfo.TotalHitRayCount = 1;
			newObjectInfo.FrontHitRayCount = bFrontRay ? 1 : 0;
			newObjectInfo.bInFront = bFrontRay;

			if (bFrontRay)
			{
				newObjectInfo.ClosestFrontHitLocationCm = rayInfo.HitLocationCm;
				newObjectInfo.ClosestFrontDistanceM = rayInfo.DistanceM;
				newObjectInfo.ClosestFrontRayYawDegree = rayInfo.RayYawDegree;
			}

			objectMap.Add(rayInfo.HitActor, newObjectInfo);
			continue;
		}

		++objectInfo->TotalHitRayCount;

		if (bFrontRay)
		{
			++objectInfo->FrontHitRayCount;
			objectInfo->bInFront = true;

			if (objectInfo->ClosestFrontDistanceM <= 0.f || rayInfo.DistanceM < objectInfo->ClosestFrontDistanceM)
			{
				objectInfo->ClosestFrontHitLocationCm = rayInfo.HitLocationCm;
				objectInfo->ClosestFrontDistanceM = rayInfo.DistanceM;
				objectInfo->ClosestFrontRayYawDegree = rayInfo.RayYawDegree;
			}
		}

		if (rayInfo.DistanceM < objectInfo->ClosestDistanceM)
		{
			objectInfo->ClosestHitLocationCm = rayInfo.HitLocationCm;
			objectInfo->ClosestDistanceM = rayInfo.DistanceM;
			objectInfo->ClosestRayYawDegree = rayInfo.RayYawDegree;
		}
	}

	TArray<FDeliveryBotLidarDetectedObjectInfo> detectedObjects;
	objectMap.GenerateValueArray(detectedObjects);

	detectedObjects.Sort(
		[](const FDeliveryBotLidarDetectedObjectInfo& leftInfo, const FDeliveryBotLidarDetectedObjectInfo& rightInfo)
		{
			return leftInfo.ClosestDistanceM < rightInfo.ClosestDistanceM;
		}
	);

	return detectedObjects;
}

bool UDeliveryBot_LidarSensorComponent::FindNearestFrontObject(
	const FDeliveryBotLidarScanInfo& scanInfo,
	FDeliveryBotLidarDetectedObjectInfo& outObjectInfo) const
{
	const TArray<FDeliveryBotLidarDetectedObjectInfo> detectedObjects{
		BuildDetectedObjects(scanInfo)
	};

	bool bFound{ false };
	float nearestFrontDistanceM{ TNumericLimits<float>::Max() };

	for (const FDeliveryBotLidarDetectedObjectInfo& objectInfo : detectedObjects)
	{
		if (!objectInfo.bInFront || objectInfo.FrontHitRayCount <= 0 || objectInfo.ClosestFrontDistanceM <= 0.f)
		{
			continue;
		}

		if (objectInfo.ClosestFrontDistanceM >= nearestFrontDistanceM)
		{
			continue;
		}

		nearestFrontDistanceM = objectInfo.ClosestFrontDistanceM;
		outObjectInfo = objectInfo;
		bFound = true;
	}

	return bFound;
}

bool UDeliveryBot_LidarSensorComponent::ShouldStopByFrontObject(
	const FDeliveryBotLidarScanInfo& scanInfo,
	FDeliveryBotLidarDetectedObjectInfo& outObjectInfo) const
{
	if (!FindNearestFrontObject(scanInfo, outObjectInfo))
	{
		return false;
	}

	return outObjectInfo.ClosestFrontDistanceM <= LidarSensorConfigInfo.StopDistanceM;
	
}
