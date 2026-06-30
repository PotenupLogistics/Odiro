#include "DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h"

#include "DrawDebugHelpers.h"
#include "Scenario/Actors/ScenarioCorridorRuntimeActor.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotLidarSensor, Log, All);

namespace
{
	// Returns the scenario-authored semantic id when the actor participates in scenario logging.
	FString ResolveLidarScenarioTargetId(const AActor* actor)
	{
		if (!IsValid(actor))
		{
			return FString();
		}

		const UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>();
		return placeableComponent ? placeableComponent->InstanceId : FString();
	}

	// Filters floor-like scenario surfaces so LiDAR reports vertical obstacles instead of road/walkway planes.
	bool ShouldIgnoreScenarioSurfaceLidarHit(const FHitResult& hitResult)
	{
		const AActor* hitActor = hitResult.GetActor();
		if (IsValid(Cast<AScenarioGroundRegion>(hitActor))
			|| IsValid(Cast<AScenarioCorridorRuntimeActor>(hitActor)))
		{
			return true;
		}

		const UPrimitiveComponent* hitComponent = hitResult.GetComponent();
		if (!IsValid(hitComponent))
		{
			return true;
		}

		const FName profileName = hitComponent->GetCollisionProfileName();
		return profileName == FName(TEXT("Walkable"))
			|| profileName == FName(TEXT("Penalty"));
	}
}

UDeliveryBot_LidarSensorComponent::UDeliveryBot_LidarSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector UDeliveryBot_LidarSensorComponent::GetSensorWorldLocationCm(const AActor& owner) const
{
	const FVector localOffsetCm(
		LidarSensorConfigInfo.SensorForwardOffsetM * 100.f,
		LidarSensorConfigInfo.SensorRightOffsetM * 100.f,
		LidarSensorConfigInfo.SensorHeightM * 100.f);
	return owner.GetActorLocation() + owner.GetActorRotation().RotateVector(localOffsetCm);
}

// LiDAR 설정을 런타임에서 안전하게 사용할 수 있도록 보정한다.
void UDeliveryBot_LidarSensorComponent::InitializeLidar(const FDeliveryBotLidarSensorConfigInfo& lidarSensorConfigInfo)
{
	LidarSensorConfigInfo = lidarSensorConfigInfo;

	LidarSensorConfigInfo.ScanRangeM = FMath::Max(LidarSensorConfigInfo.ScanRangeM, 0.f);
	LidarSensorConfigInfo.AngleStepDegree = FMath::Max(LidarSensorConfigInfo.AngleStepDegree, 1.f);
	LidarSensorConfigInfo.SensorHeightM = FMath::Max(LidarSensorConfigInfo.SensorHeightM, 0.f);
	LidarSensorConfigInfo.SensorForwardOffsetM =
		FMath::Clamp(LidarSensorConfigInfo.SensorForwardOffsetM, -10.f, 10.f);
	LidarSensorConfigInfo.SensorRightOffsetM =
		FMath::Clamp(LidarSensorConfigInfo.SensorRightOffsetM, -10.f, 10.f);
	LidarSensorConfigInfo.ScanRateHz = FMath::Max(LidarSensorConfigInfo.ScanRateHz, 0.1f);
	LidarSensorConfigInfo.bStoreMissedRays = true;
	LidarSensorConfigInfo.FrontHalfAngleDegree = FMath::Clamp(LidarSensorConfigInfo.FrontHalfAngleDegree, 0.f, 180.f);
	LidarSensorConfigInfo.VerticalMinDegree = FMath::Clamp(LidarSensorConfigInfo.VerticalMinDegree, -89.f, 89.f);
	LidarSensorConfigInfo.VerticalMaxDegree = FMath::Clamp(LidarSensorConfigInfo.VerticalMaxDegree, -89.f, 89.f);
	LidarSensorConfigInfo.VerticalStepDegree = FMath::Max(LidarSensorConfigInfo.VerticalStepDegree, 1.f);

	if (LidarSensorConfigInfo.VerticalMinDegree > LidarSensorConfigInfo.VerticalMaxDegree)
	{
		const float verticalMinDegree = LidarSensorConfigInfo.VerticalMinDegree;
		LidarSensorConfigInfo.VerticalMinDegree = LidarSensorConfigInfo.VerticalMaxDegree;
		LidarSensorConfigInfo.VerticalMaxDegree = verticalMinDegree;
	}

	LidarSensorConfigInfo.StopDistanceM = FMath::Max(LidarSensorConfigInfo.StopDistanceM, 0.f);

	LidarSensorConfigInfo.ObstacleWarningDistanceM = FMath::Max(
		LidarSensorConfigInfo.ObstacleWarningDistanceM,
		LidarSensorConfigInfo.StopDistanceM + 0.1f);

	LidarSensorConfigInfo.SlowDownDistanceM = FMath::Max(
		LidarSensorConfigInfo.SlowDownDistanceM,
		LidarSensorConfigInfo.ObstacleWarningDistanceM + 0.1f);

	LidarSensorConfigInfo.CollisionStopHalfAngleDegree = FMath::Clamp(
		LidarSensorConfigInfo.CollisionStopHalfAngleDegree,
		0.f,
		LidarSensorConfigInfo.FrontHalfAngleDegree);

	LidarSensorConfigInfo.CollisionStopDistanceM = FMath::Clamp(
		LidarSensorConfigInfo.CollisionStopDistanceM,
		0.f,
		LidarSensorConfigInfo.SlowDownDistanceM);
}

// 설정된 LiDAR 모드에 따라 스캔 방식을 선택한다.
FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::ScanLidar() const
{
	switch (LidarSensorConfigInfo.LidarModeType)
	{
	case EDeliveryBotLidarModeType::OneD:
		return ScanLidar1D();

	case EDeliveryBotLidarModeType::TwoD:
		return ScanLidar2D();

	case EDeliveryBotLidarModeType::ThreeD:
		return ScanLidar3D();

	case EDeliveryBotLidarModeType::OneDAndTwoD:
		return MergeLidarScans(ScanLidar1D(), ScanLidar2D());

	case EDeliveryBotLidarModeType::TwoDAndThreeD:
		return MergeLidarScans(ScanLidar2D(), ScanLidar3D());

	case EDeliveryBotLidarModeType::All:
		return MergeLidarScans(ScanLidar1D(), MergeLidarScans(ScanLidar2D(), ScanLidar3D()));

	default:
		return ScanLidar2D();
	}
}

// Python에 보낼 nearest raw hit을 ActorTag 필터 없이 찾는다.
bool UDeliveryBot_LidarSensorComponent::TraceLidarRay(
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	FHitResult& outHitResult,
	TArray<FHitResult>& outRawHitResults) const
{
	outHitResult = FHitResult();
	outRawHitResults.Reset();

	UWorld* world = GetWorld();
	const AActor* owner = GetOwner();
	if (world == nullptr || !IsValid(owner))
	{
		return false;
	}

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(owner);

	TArray<FHitResult> hitResults;
	const bool bTraceHit = world->LineTraceMultiByChannel(
		hitResults,
		startLocationCm,
		endLocationCm,
		LidarSensorConfigInfo.TraceChannel,
		queryParams);

	if (!bTraceHit)
	{
		return false;
	}

	bool bFoundActorHit = false;

	for (const FHitResult& hitResult : hitResults)
	{
		const AActor* hitActor = hitResult.GetActor();
		if (!IsValid(hitActor))
		{
			continue;
		}

		if (ShouldIgnoreScenarioSurfaceLidarHit(hitResult))
		{
			continue;
		}

		outRawHitResults.Add(hitResult);

		if (!bFoundActorHit)
		{
			outHitResult = hitResult;
			bFoundActorHit = true;
		}
	}

	return bFoundActorHit;
}


// Python 정책 입력은 수평 레이어를 우선 사용해 3D raw scan이 길찾기 기준을 바꾸지 않도록 한다.
FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::BuildPolicy2DScan(const FDeliveryBotLidarScanInfo& rawScanInfo) const
{
	FDeliveryBotLidarScanInfo policyScanInfo;
	policyScanInfo.SensorLocationCm = rawScanInfo.SensorLocationCm;
	policyScanInfo.SimulationTimeSeconds = rawScanInfo.SimulationTimeSeconds;

	TMap<int32, FDeliveryBotLidarRayInfo> policyRayByYaw;

	for (const FDeliveryBotLidarRayInfo& rayInfo : rawScanInfo.RayInfos)
	{
		const int32 yawKey = FMath::RoundToInt(rayInfo.RayYawDegree * 100.f);
		FDeliveryBotLidarRayInfo* existingRayInfo = policyRayByYaw.Find(yawKey);

		const float pitchAbsDegree = FMath::Abs(rayInfo.RayPitchDegree);
		const float existingPitchAbsDegree = existingRayInfo != nullptr
			? FMath::Abs(existingRayInfo->RayPitchDegree)
			: TNumericLimits<float>::Max();

		const bool bUseRay =
			existingRayInfo == nullptr
			|| pitchAbsDegree < existingPitchAbsDegree
			|| (FMath::IsNearlyEqual(pitchAbsDegree, existingPitchAbsDegree)
				&& rayInfo.DistanceM < existingRayInfo->DistanceM);

		if (bUseRay)
		{
			FDeliveryBotLidarRayInfo projectedRayInfo = rayInfo;
			projectedRayInfo.RayPitchDegree = 0.f;
			policyRayByYaw.Add(yawKey, projectedRayInfo);
		}
	}

	policyRayByYaw.GenerateValueArray(policyScanInfo.RayInfos);

	policyScanInfo.RayInfos.Sort(
		[](const FDeliveryBotLidarRayInfo& leftInfo, const FDeliveryBotLidarRayInfo& rightInfo)
		{
			return leftInfo.RayYawDegree < rightInfo.RayYawDegree;
		});

	for (int32 rayIndex = 0; rayIndex < policyScanInfo.RayInfos.Num(); ++rayIndex)
	{
		policyScanInfo.RayInfos[rayIndex].RayIndex = rayIndex;
	}

	return policyScanInfo;
}

// Actor tag에 따라 debug ray line만 숨길지 판단한다.
bool UDeliveryBot_LidarSensorComponent::ShouldSuppressDebugRayLine(const AActor* actor) const
{
	if (!IsValid(actor)) 
		return true;

	for (const FName& ignoreTag : LidarSensorConfigInfo.IgnoreTags)
	{
		if (actor->ActorHasTag(ignoreTag))
			return true;
	}
	return false;
}

// 두 LiDAR scan 결과를 하나로 합친다.
FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::MergeLidarScans(
	const FDeliveryBotLidarScanInfo& firstScanInfo,
	const FDeliveryBotLidarScanInfo& secondScanInfo) const
{
	FDeliveryBotLidarScanInfo mergedScanInfo = firstScanInfo;

	if (mergedScanInfo.SensorLocationCm.IsNearlyZero())
	{
		mergedScanInfo.SensorLocationCm = secondScanInfo.SensorLocationCm;
	}

	mergedScanInfo.RayInfos.Reserve(firstScanInfo.RayInfos.Num() + secondScanInfo.RayInfos.Num());
	mergedScanInfo.RayInfos.Append(secondScanInfo.RayInfos);
	mergedScanInfo.SimulationTimeSeconds = FMath::Max(firstScanInfo.SimulationTimeSeconds, secondScanInfo.SimulationTimeSeconds);

	return mergedScanInfo;
}

// 낮은 scan rate에서도 LiDAR debug draw가 다음 scan까지 유지되도록 시간을 계산한다.
float UDeliveryBot_LidarSensorComponent::GetDebugDrawLifeTimeSeconds() const
{
	const float scanRateHz = FMath::Max(LidarSensorConfigInfo.ScanRateHz, 0.1f);
	return FMath::Max(1.f / scanRateHz, 0.05f);
}

// 3D LiDAR scan 결과를 raw hit와 전방 장애물 후보 기준으로 요약한다.
void UDeliveryBot_LidarSensorComponent::LogLidarTraceSummary(const FDeliveryBotLidarScanInfo& scanInfo) const
{
	if (!bLogTraceSummary)
		return;

	int32 rayCount = 0;
	int32 hitCount = 0;
	int32 frontHitCount = 0;
	int32 frontObstacleCount = 0;
	const FDeliveryBotLidarRayInfo* nearestObstacleRayInfo = nullptr;

	for (const FDeliveryBotLidarRayInfo& rayInfo : scanInfo.RayInfos)
	{
		if (rayInfo.RayDimensionType != EDeliveryBotLidarRayDimensionType::ThreeD)
			continue;

		++rayCount;

		if (!rayInfo.bHit)
			continue;

		++hitCount;

		if (!IsFrontYaw(rayInfo.RayYawDegree))
			continue;

		++frontHitCount;

		if (rayInfo.ActorTags.Num() <= 0)
			continue;

		++frontObstacleCount;

		if (nearestObstacleRayInfo == nullptr || rayInfo.DistanceM < nearestObstacleRayInfo->DistanceM)
		{
			nearestObstacleRayInfo = &rayInfo;
		}
	}

	FString nearestObstacleActor = TEXT("none");
	FString nearestObstacleTags = TEXT("none");
	float nearestObstacleDistanceM = -1.f;
	float nearestObstacleYawDegree = 0.f;
	float nearestObstaclePitchDegree = 0.f;

	if (nearestObstacleRayInfo != nullptr)
	{
		nearestObstacleActor = nearestObstacleRayInfo->ActorName;
		nearestObstacleDistanceM = nearestObstacleRayInfo->DistanceM;
		nearestObstacleYawDegree = nearestObstacleRayInfo->RayYawDegree;
		nearestObstaclePitchDegree = nearestObstacleRayInfo->RayPitchDegree;

		TArray<FString> actorTagStrings;
		for (const FName& actorTag : nearestObstacleRayInfo->ActorTags)
		{
			actorTagStrings.Add(actorTag.ToString());
		}

		if (!actorTagStrings.IsEmpty())
		{
			nearestObstacleTags = FString::Join(actorTagStrings, TEXT(","));
		}
	}

	UE_LOG(
		LogDeliveryBotLidarSensor,
		Log,
		TEXT("LiDAR 3D Trace Summary | rayCount=%d hitCount=%d frontHitCount=%d frontObstacleCount=%d frontHalfAngle=%.2f nearestObstacleActor=%s nearestObstacleDistanceM=%.3f nearestObstacleYaw=%.2f nearestObstaclePitch=%.2f nearestObstacleTags=%s"),
		rayCount,
		hitCount,
		frontHitCount,
		frontObstacleCount,
		LidarSensorConfigInfo.FrontHalfAngleDegree,
		*nearestObstacleActor,
		nearestObstacleDistanceM,
		nearestObstacleYawDegree,
		nearestObstaclePitchDegree,
		*nearestObstacleTags);
}

// 모든 raw hit point를 찍고, 숨김 tag가 없는 hit에만 ray line을 그린다.
void UDeliveryBot_LidarSensorComponent::DrawDebugLidarRay(
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	const FHitResult* hitResult,
	const TArray<FHitResult>& rawHitResults) const
{
	if (!LidarSensorConfigInfo.bDrawDebug) 
		return;

	UWorld* world = GetWorld();
	if (world == nullptr) 
		return;

	const float lifeTimeSeconds = GetDebugDrawLifeTimeSeconds();

	for (const FHitResult& rawHitResult : rawHitResults)
	{
		DrawDebugPoint(
			world,
			rawHitResult.ImpactPoint,
			8.f,
			FColor::Yellow,
			false,
			lifeTimeSeconds);
	}

	const bool bHasHit = hitResult != nullptr && IsValid(hitResult->GetActor());
	if (!bHasHit) return;

	if (ShouldSuppressDebugRayLine(hitResult->GetActor()))
		return;

	DrawDebugLine(
		world,
		startLocationCm,
		hitResult->ImpactPoint,
		FColor::Red,
		false,
		lifeTimeSeconds,
		0,
		0.75f);
}

// Python과 point cloud로 전달되는 LiDAR hit world location을 누적 점으로 표시한다.
void UDeliveryBot_LidarSensorComponent::DrawAccumulatedHitLocationDebug(
	const FVector& hitLocationCm,
	bool bObstacleHit) const
{
	if (!bDrawAccumulatedHitLocationDebug)
		return;

	UWorld* world = GetWorld();
	if (world == nullptr)
		return;

	const float lifeTimeSeconds = FMath::Max(AccumulatedHitLocationDebugLifeTimeSeconds, 0.1f);
	const float pointSize = FMath::Max(AccumulatedHitLocationDebugPointSize, 1.f);
	const FColor pointColor = bObstacleHit ? FColor::Red : FColor(160, 160, 160);

	DrawDebugPoint(
		world,
		hitLocationCm,
		pointSize,
		pointColor,
		false,
		lifeTimeSeconds);
}

void UDeliveryBot_LidarSensorComponent::DrawDebugObstacleWarningRange(const FVector& sensorLocationCm) const
{
	if (!LidarSensorConfigInfo.bDrawDebug || !LidarSensorConfigInfo.bDrawObstacleWarningDebug)
		return;

	UWorld* world = GetWorld();
	const AActor* owner = GetOwner();

	if (world == nullptr || !IsValid(owner))
		return;

	const float frontHalfAngleDegree = FMath::Max(LidarSensorConfigInfo.FrontHalfAngleDegree, 0.f);
	const float collisionHalfAngleDegree = FMath::Clamp(
		LidarSensorConfigInfo.CollisionStopHalfAngleDegree,
		0.f,
		frontHalfAngleDegree);
	const float stopDistanceCm = FMath::Max(LidarSensorConfigInfo.StopDistanceM * 100.f, 0.f);
	const float obstacleWarningDistanceCm = FMath::Max(
		LidarSensorConfigInfo.ObstacleWarningDistanceM * 100.f,
		stopDistanceCm + 1.f);
	const float slowDownDistanceCm = FMath::Max(LidarSensorConfigInfo.SlowDownDistanceM * 100.f, 0.f);
	const float collisionStopDistanceCm = FMath::Clamp(
		LidarSensorConfigInfo.CollisionStopDistanceM * 100.f,
		0.f,
		slowDownDistanceCm);

	if (frontHalfAngleDegree <= KINDA_SMALL_NUMBER || slowDownDistanceCm <= KINDA_SMALL_NUMBER)
		return;

	const FColor obstacleWarningColor = FColor::Cyan;
	const FColor slowDownColor(255, 128, 0);
	const FColor collisionColor = FColor::Red;
	const float lifeTimeSeconds = GetDebugDrawLifeTimeSeconds();
	const uint8 depthPriority = 0;

	const auto getPointAtYaw = [owner, sensorLocationCm](float yawDegree, float distanceCm)
	{
		const FVector localDirection = FRotator(0.f, yawDegree, 0.f).Vector();
		const FVector worldDirection = owner->GetActorRotation().RotateVector(localDirection);
		return sensorLocationCm + worldDirection * distanceCm;
	};

	const auto drawYawLine = [&](float yawDegree, float distanceCm, const FColor& color, float thickness)
	{
		DrawDebugLine(
			world,
			sensorLocationCm,
			getPointAtYaw(yawDegree, distanceCm),
			color,
			false,
			lifeTimeSeconds,
			depthPriority,
			thickness);
	};

	const auto drawArc = [&](float startYawDegree, float endYawDegree, float radiusCm, const FColor& color, float thickness)
	{
		if (radiusCm <= KINDA_SMALL_NUMBER || FMath::IsNearlyEqual(startYawDegree, endYawDegree))
			return;

		const int32 segmentCount = FMath::Max(4, FMath::CeilToInt(FMath::Abs(endYawDegree - startYawDegree) / 5.f));
		FVector previousPoint = getPointAtYaw(startYawDegree, radiusCm);

		for (int32 segmentIndex = 1; segmentIndex <= segmentCount; ++segmentIndex)
		{
			const float alpha = static_cast<float>(segmentIndex) / static_cast<float>(segmentCount);
			const float yawDegree = FMath::Lerp(startYawDegree, endYawDegree, alpha);
			const FVector nextPoint = getPointAtYaw(yawDegree, radiusCm);

			DrawDebugLine(
				world,
				previousPoint,
				nextPoint,
				color,
				false,
				lifeTimeSeconds,
				depthPriority,
				thickness);

			previousPoint = nextPoint;
		}
	};

	drawArc(-180.f, 180.f, obstacleWarningDistanceCm, obstacleWarningColor, 3.f);
	drawArc(-frontHalfAngleDegree, frontHalfAngleDegree, slowDownDistanceCm, slowDownColor, 2.f);
	drawYawLine(-frontHalfAngleDegree, slowDownDistanceCm, slowDownColor, 2.f);
	drawYawLine(frontHalfAngleDegree, slowDownDistanceCm, slowDownColor, 2.f);

	if (collisionHalfAngleDegree > KINDA_SMALL_NUMBER)
	{
		drawYawLine(-collisionHalfAngleDegree, slowDownDistanceCm, slowDownColor, 2.f);
		drawYawLine(collisionHalfAngleDegree, slowDownDistanceCm, slowDownColor, 2.f);
	}

	if (stopDistanceCm > KINDA_SMALL_NUMBER)
	{
		drawArc(-frontHalfAngleDegree, frontHalfAngleDegree, stopDistanceCm, collisionColor, 3.f);
	}

	if (collisionStopDistanceCm > KINDA_SMALL_NUMBER)
	{
		drawArc(-180.f, 180.f, collisionStopDistanceCm, collisionColor, 1.5f);
	}

	const FVector labelLocationCm = getPointAtYaw(0.f, slowDownDistanceCm + 35.f) + FVector(0.f, 0.f, 20.f);
	DrawDebugString(
		world,
		labelLocationCm,
		FString::Printf(
			TEXT("Stop %.2fm | ObstacleWarning %.2fm | Slow %.2fm"),
			LidarSensorConfigInfo.StopDistanceM,
			LidarSensorConfigInfo.ObstacleWarningDistanceM,
			LidarSensorConfigInfo.SlowDownDistanceM),
		nullptr,
		obstacleWarningColor,
		lifeTimeSeconds,
		true);
}

float UDeliveryBot_LidarSensorComponent::GetSignedYawDegree(float yawDegree) const
{
	float normalizedYawDegree = FMath::Fmod(yawDegree, 360.f);

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
	const float signedYawDegree = GetSignedYawDegree(yawDegree);
	const float frontHalfAngleDegree = FMath::Max(LidarSensorConfigInfo.FrontHalfAngleDegree, 0.f);

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

		const bool bFrontRay = IsFrontYaw(rayInfo.RayYawDegree);
		FDeliveryBotLidarDetectedObjectInfo* objectInfo = objectMap.Find(rayInfo.HitActor);

		if (objectInfo == nullptr)
		{
			FDeliveryBotLidarDetectedObjectInfo newObjectInfo;
			newObjectInfo.DetectedActor = rayInfo.HitActor;
			newObjectInfo.ActorName = rayInfo.ActorName;
			newObjectInfo.ActorTags = rayInfo.ActorTags;
			newObjectInfo.TargetId = rayInfo.TargetId;
			newObjectInfo.TargetTags = rayInfo.TargetTags;
			rayInfo.HitActor->GetActorBounds(true, newObjectInfo.BoundsOriginCm, newObjectInfo.BoundsExtentCm);
			newObjectInfo.bHasBounds = !newObjectInfo.BoundsExtentCm.IsNearlyZero();
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
	const TArray<FDeliveryBotLidarDetectedObjectInfo> detectedObjects = BuildDetectedObjects(scanInfo);

	bool bFound = false;
	float nearestFrontDistanceM = TNumericLimits<float>::Max();

	for (const FDeliveryBotLidarDetectedObjectInfo& objectInfo : detectedObjects)
	{
		if (!objectInfo.bInFront || objectInfo.FrontHitRayCount <= 0 || objectInfo.ClosestFrontDistanceM <= 0.f)
			continue;

		if (objectInfo.ClosestFrontDistanceM >= nearestFrontDistanceM)
			continue;

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

// 정면 1개 ray만 사용하는 1D LiDAR scan을 생성한다.
FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::ScanLidar1D() const
{
	FDeliveryBotLidarScanInfo scanInfo;

	const AActor* owner = GetOwner();
	if (!IsValid(owner))
		return scanInfo;

	const FVector sensorLocationCm = GetSensorWorldLocationCm(*owner);

	scanInfo.SensorLocationCm = sensorLocationCm;

	const float scanRangeCm = LidarSensorConfigInfo.ScanRangeM * 100.f;
	const FVector endLocationCm = sensorLocationCm + owner->GetActorForwardVector() * scanRangeCm;

	DrawDebugObstacleWarningRange(sensorLocationCm);

	FHitResult hitResult;
	TArray<FHitResult> rawHitResults;
	const bool bHit = TraceLidarRay(sensorLocationCm, endLocationCm, hitResult, rawHitResults);

	DrawDebugLidarRay(sensorLocationCm, endLocationCm, bHit ? &hitResult : nullptr, rawHitResults);

	if (bHit || LidarSensorConfigInfo.bStoreMissedRays)
	{
		scanInfo.RayInfos.Add(MakeRayInfo(
			0,
			0.f,
			sensorLocationCm,
			endLocationCm,
			bHit ? &hitResult : nullptr,
			EDeliveryBotLidarRayDimensionType::OneD));
	}

	return scanInfo;
}

// 수평 360도 방향의 2D LiDAR scan을 생성한다.
FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::ScanLidar2D() const
{
	FDeliveryBotLidarScanInfo scanInfo;

	const AActor* owner = GetOwner();

	if (!IsValid(owner))
		return scanInfo;

	const FVector sensorLocationCm = GetSensorWorldLocationCm(*owner);

	scanInfo.SensorLocationCm = sensorLocationCm;

	const float angleStepDegree = FMath::Max(LidarSensorConfigInfo.AngleStepDegree, 1.f);
	const float scanRangeCm = LidarSensorConfigInfo.ScanRangeM * 100.f;

	DrawDebugObstacleWarningRange(sensorLocationCm);

	int32 rayIndex = 0;

	for (float yawDegree = 0.f; yawDegree < 360.f; yawDegree += angleStepDegree)
	{
		const FVector localDirection = FRotator(0.f, yawDegree, 0.f).Vector();
		const FVector worldDirection = owner->GetActorRotation().RotateVector(localDirection);
		const FVector endLocationCm = sensorLocationCm + worldDirection * scanRangeCm;

		FHitResult hitResult;
		TArray<FHitResult> rawHitResults;
		const bool bHit = TraceLidarRay(sensorLocationCm, endLocationCm, hitResult, rawHitResults);

		DrawDebugLidarRay(sensorLocationCm, endLocationCm, bHit ? &hitResult : nullptr, rawHitResults);
		
		if (bHit || LidarSensorConfigInfo.bStoreMissedRays)
		{
			scanInfo.RayInfos.Add(MakeRayInfo(
				rayIndex,
				yawDegree,
				sensorLocationCm,
				endLocationCm,
				bHit ? &hitResult : nullptr,
				EDeliveryBotLidarRayDimensionType::TwoD));
		}

		++rayIndex;
	}

	return scanInfo;
}

// 수평 yaw와 수직 pitch를 조합한 3D LiDAR scan을 생성한다.
FDeliveryBotLidarScanInfo UDeliveryBot_LidarSensorComponent::ScanLidar3D() const
{
	FDeliveryBotLidarScanInfo scanInfo;

	const AActor* owner = GetOwner();
	if (!IsValid(owner))
		return scanInfo;

	const FVector sensorLocationCm = GetSensorWorldLocationCm(*owner);

	scanInfo.SensorLocationCm = sensorLocationCm;

	const float angleStepDegree = FMath::Max(LidarSensorConfigInfo.AngleStepDegree, 1.f);
	const float verticalStepDegree = FMath::Max(LidarSensorConfigInfo.VerticalStepDegree, 1.f);
	const float scanRangeCm = LidarSensorConfigInfo.ScanRangeM * 100.f;

	DrawDebugObstacleWarningRange(sensorLocationCm);

	int32 rayIndex = 0;

	for (float pitchDegree = LidarSensorConfigInfo.VerticalMinDegree;
		pitchDegree <= LidarSensorConfigInfo.VerticalMaxDegree;
		pitchDegree += verticalStepDegree)
	{
		for (float yawDegree = 0.f; yawDegree < 360.f; yawDegree += angleStepDegree)
		{
			AppendLidarRay(
				scanInfo,
				rayIndex,
				yawDegree,
				pitchDegree,
				sensorLocationCm,
				owner->GetActorRotation(),
				scanRangeCm,
				EDeliveryBotLidarRayDimensionType::ThreeD);

			++rayIndex;
		}
	}

	LogLidarTraceSummary(scanInfo);

	return scanInfo;
}


// 1D/2D LiDAR ray 결과를 pitch 0 기준 결과로 변환한다.
FDeliveryBotLidarRayInfo UDeliveryBot_LidarSensorComponent::MakeRayInfo(
	int32 rayIndex,
	float rayYawDegree,
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	const FHitResult* hitResult,
	EDeliveryBotLidarRayDimensionType rayDimensionType) const
{
	return MakeRayInfo(
		rayIndex,
		rayYawDegree,
		0.f,
		startLocationCm,
		endLocationCm,
		hitResult,
		rayDimensionType);
}

// trace 결과를 LiDAR ray 결과 구조체로 변환한다.
FDeliveryBotLidarRayInfo UDeliveryBot_LidarSensorComponent::MakeRayInfo(
	int32 rayIndex,
	float rayYawDegree,
	float rayPitchDegree,
	const FVector& startLocationCm,
	const FVector& endLocationCm,
	const FHitResult* hitResult,
	EDeliveryBotLidarRayDimensionType rayDimensionType) const
{
	FDeliveryBotLidarRayInfo rayInfo;

	rayInfo.RayIndex = rayIndex;
	rayInfo.RayYawDegree = GetSignedYawDegree(rayYawDegree);
	rayInfo.RayPitchDegree = rayPitchDegree;
	rayInfo.RayDimensionType = rayDimensionType;
	rayInfo.StartLocationCm = startLocationCm;
	rayInfo.EndLocationCm = endLocationCm;
	rayInfo.DistanceM = FVector::Dist(startLocationCm, endLocationCm) / 100.f;

	if (hitResult == nullptr || !IsValid(hitResult->GetActor()))
	{
		rayInfo.HitLocationCm = endLocationCm;
		return rayInfo;
	}

	AActor* hitActor = hitResult->GetActor();

	rayInfo.bHit = true;
	rayInfo.HitActor = hitActor;
	rayInfo.ActorName = hitActor->GetName();
	rayInfo.ActorTags = hitActor->Tags;
	rayInfo.TargetId = ResolveLidarScenarioTargetId(hitActor);
	rayInfo.TargetTags = hitActor->Tags;
	rayInfo.HitLocationCm = hitResult->ImpactPoint;
	rayInfo.DistanceM = FVector::Dist(startLocationCm, hitResult->ImpactPoint) / 100.f;

	DrawAccumulatedHitLocationDebug(rayInfo.HitLocationCm, rayInfo.ActorTags.Num() > 0);

	return rayInfo;
}

// yaw/pitch 각도 하나에 대한 LiDAR ray를 쏘고 scan 결과에 추가한다.
void UDeliveryBot_LidarSensorComponent::AppendLidarRay(
	FDeliveryBotLidarScanInfo& scanInfo,
	int32 rayIndex,
	float yawDegree,
	float pitchDegree,
	const FVector& sensorLocationCm,
	const FRotator& ownerRotation,
	float scanRangeCm,
	EDeliveryBotLidarRayDimensionType rayDimensionType) const
{
	const FVector localDirection = FRotator(pitchDegree, yawDegree, 0.f).Vector();
	const FVector worldDirection = ownerRotation.RotateVector(localDirection);
	const FVector endLocationCm = sensorLocationCm + worldDirection * scanRangeCm;

	FHitResult hitResult;
	TArray<FHitResult> rawHitResults;
	const bool bHit = TraceLidarRay(sensorLocationCm, endLocationCm, hitResult, rawHitResults);

	DrawDebugLidarRay(sensorLocationCm, endLocationCm, bHit ? &hitResult : nullptr, rawHitResults);
	
	if (bHit || LidarSensorConfigInfo.bStoreMissedRays)
	{
		scanInfo.RayInfos.Add(MakeRayInfo(
			rayIndex,
			yawDegree,
			pitchDegree,
			sensorLocationCm,
			endLocationCm,
			bHit ? &hitResult : nullptr,
			rayDimensionType));
	}
}
