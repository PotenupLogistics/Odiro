#include "Episode/EpisodePedestrianPlanBuilder.h"

namespace
{
	constexpr double DefaultPedestrianSpeedCmPerSecond = 120.0;
	constexpr double MinPlanSpeedCmPerSecond = 1.0;
	constexpr double MinPlanSegmentLengthCm = 1.0;

	FString HashToString(uint32 hash)
	{
		return FString::Printf(TEXT("%u"), hash);
	}

	void HashString(uint32& hash, const FString& value)
	{
		hash = HashCombine(hash, GetTypeHash(value));
	}

	FVector2D ToVector2D(const FVector& value)
	{
		return FVector2D(value.X, value.Y);
	}
}

bool FEpisodePedestrianPlanBuilder::BuildPlans(
	const FEpisodeSimulationSetupSpec& setupSpec,
	const FEpisodePedestrianPlanBuildContext& buildContext,
	FEpisodePedestrianPlanBuildResult& outResult)
{
	outResult = FEpisodePedestrianPlanBuildResult{};
	outResult.ResolvedFootprintHash = BuildResolvedFootprintHash(buildContext.StaticObstacleFootprints);

	TArray<FEpisodeDynamicActorSpec> plannedPedestrians;
	for (const FEpisodeDynamicActorSpec& dynamicActorSpec : setupSpec.DynamicActors)
	{
		if (IsPlannedPedestrian(dynamicActorSpec))
		{
			plannedPedestrians.Add(dynamicActorSpec);
		}
	}

	// 추린 보행자를 InstanceId 기준으로 정렬: 입력 배열 순서가 바뀌어도 결과 순서와 hash가 안정적이게 하기 위한 처리
	plannedPedestrians.Sort(
		[](const FEpisodeDynamicActorSpec& left, const FEpisodeDynamicActorSpec& right)
		{
			return left.InstanceId < right.InstanceId;
		});

	bool bAllSucceeded = true;
	for (const FEpisodeDynamicActorSpec& dynamicActorSpec : plannedPedestrians)
	{
		FEpisodePedestrianPlan plan;
		if (!BuildPlanForPedestrian(
			dynamicActorSpec,
			buildContext,
			outResult.ResolvedFootprintHash,
			plan,
			outResult.Diagnostics))
		{
			bAllSucceeded = false;
			continue;
		}

		outResult.Plans.Add(MoveTemp(plan));
	}

	outResult.bSuccess = bAllSucceeded;
	return bAllSucceeded;
}

bool FEpisodePedestrianPlanBuilder::IsPlannedPedestrian(const FEpisodeDynamicActorSpec& dynamicActorSpec)
{
	if (dynamicActorSpec.Category != EEpisodeActorCategory::Pedestrian) return false;

	FString movementModel;
	if (!TryGetStringProperty(dynamicActorSpec.Properties, TEXT("movement_model"), movementModel))
	{
		return false;
	}

	return movementModel.Equals(TEXT("planned_trajectory"), ESearchCase::IgnoreCase);
}

bool FEpisodePedestrianPlanBuilder::TryGetFloatProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	double& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue) return false;

	if (paramValue->Type == EEpisodeParamValueType::Float)
	{
		outValue = paramValue->FloatValue;
		return true;
	}

	if (paramValue->Type == EEpisodeParamValueType::Integer)
	{
		outValue = static_cast<double>(paramValue->IntegerValue);
		return true;
	}

	return false;
}

bool FEpisodePedestrianPlanBuilder::TryGetStringProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	FString& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::String) return false;

	outValue = paramValue->StringValue;
	return true;
}

bool FEpisodePedestrianPlanBuilder::TryGetVectorProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	FVector& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::Vector) return false;

	outValue = paramValue->VectorValue;
	return true;
}

FEpisodePedestrianBehaviorParams FEpisodePedestrianPlanBuilder::BuildBehaviorParams(
	const FEpisodeDynamicActorSpec& dynamicActorSpec)
{
	FEpisodePedestrianBehaviorParams behaviorParams;

	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("behavior_cooperation"), behaviorParams.Cooperation);
	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("behavior_evasiveness"), behaviorParams.Evasiveness);
	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("behavior_personal_space_cm"), behaviorParams.PersonalSpaceCm);
	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("behavior_awareness_horizon_s"), behaviorParams.AwarenessHorizonSeconds);
	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("behavior_max_yield_wait_s"), behaviorParams.MaxYieldWaitSeconds);
	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("behavior_sidestep_distance_cm"), behaviorParams.SidestepDistanceCm);

	behaviorParams.Cooperation = FMath::Clamp(behaviorParams.Cooperation, 0.0, 1.0);
	behaviorParams.Evasiveness = FMath::Clamp(behaviorParams.Evasiveness, 0.0, 1.0);
	behaviorParams.PersonalSpaceCm = FMath::Max(behaviorParams.PersonalSpaceCm, 0.0);
	behaviorParams.AwarenessHorizonSeconds = FMath::Max(behaviorParams.AwarenessHorizonSeconds, 0.0);
	behaviorParams.MaxYieldWaitSeconds = FMath::Max(behaviorParams.MaxYieldWaitSeconds, 0.0);
	behaviorParams.SidestepDistanceCm = FMath::Max(behaviorParams.SidestepDistanceCm, 0.0);
	return behaviorParams;
}

bool FEpisodePedestrianPlanBuilder::BuildPlanForPedestrian(
	const FEpisodeDynamicActorSpec& dynamicActorSpec,
	const FEpisodePedestrianPlanBuildContext& buildContext,
	const FString& resolvedFootprintHash,
	FEpisodePedestrianPlan& outPlan,
	TArray<FString>& outDiagnostics)
{
	FVector startLocation = dynamicActorSpec.InitialTransform.GetLocation();
	FVector goalLocation = FVector::ZeroVector;

	if (!TryGetVectorProperty(dynamicActorSpec.Properties, TEXT("planned_start_cm"), startLocation))
	{
		outDiagnostics.Add(FString::Printf(TEXT("planned pedestrian '%s' has no planned_start_cm property."), *dynamicActorSpec.InstanceId));
		return false;
	}

	if (!TryGetVectorProperty(dynamicActorSpec.Properties, TEXT("planned_goal_cm"), goalLocation))
	{
		outDiagnostics.Add(FString::Printf(TEXT("planned pedestrian '%s' has no planned_goal_cm property."), *dynamicActorSpec.InstanceId));
		return false;
	}

	double speedCmPerSecond = DefaultPedestrianSpeedCmPerSecond;
	TryGetFloatProperty(dynamicActorSpec.Properties, TEXT("speed_cm_per_second"), speedCmPerSecond);
	speedCmPerSecond = FMath::Max(speedCmPerSecond, MinPlanSpeedCmPerSecond);

	TArray<FVector> polyline = BuildPolyline(
		startLocation,
		goalLocation,
		buildContext.StaticObstacleFootprints,
		buildContext.StaticObstacleClearanceCm);

	outPlan = FEpisodePedestrianPlan{};
	outPlan.InstanceId = dynamicActorSpec.InstanceId;
	outPlan.PlanId = FString::Printf(TEXT("%s_plan"), *dynamicActorSpec.InstanceId);
	outPlan.SourceSpecHash = buildContext.SourceSpecHash;
	outPlan.ResolvedFootprintHash = resolvedFootprintHash;
	outPlan.SemanticNavigationHash = buildContext.SemanticNavigationHash;
	outPlan.SpawnTimeSeconds = dynamicActorSpec.SpawnTimeSeconds;
	outPlan.BehaviorParams = BuildBehaviorParams(dynamicActorSpec);

	AppendPlanPoints(polyline, speedCmPerSecond, outPlan.Points, outPlan.NominalDurationSeconds);
	if (outPlan.Points.Num() < 2)
	{
		outDiagnostics.Add(FString::Printf(TEXT("planned pedestrian '%s' produced less than two plan points."), *dynamicActorSpec.InstanceId));
		return false;
	}

	outPlan.PlanHash = BuildPlanHash(dynamicActorSpec, buildContext, resolvedFootprintHash, outPlan.Points);
	outPlan.BehaviorHash = BuildBehaviorHash(outPlan.BehaviorParams);
	outPlan.PedestrianScenarioHash = BuildPedestrianScenarioHash(outPlan.PlanHash, outPlan.BehaviorHash);
	return true;
}

TArray<FVector> FEpisodePedestrianPlanBuilder::BuildPolyline(
	const FVector& startLocation,
	const FVector& goalLocation,
	const TArray<FEpisodePedestrianObstacleFootprint>& obstacleFootprints,
	double clearanceCm)
{
	TArray<FVector> polyline;
	polyline.Add(startLocation);

	FVector direction3D = goalLocation - startLocation;
	direction3D.Z = 0.0;
	const double pathLength = direction3D.Size2D();
	if (pathLength <= MinPlanSegmentLengthCm)
	{
		polyline.Add(goalLocation);
		return polyline;
	}

	const FVector direction = direction3D / pathLength;
	const FVector right(direction.Y, -direction.X, 0.0);

	struct FBlockingObstacle
	{
		const FEpisodePedestrianObstacleFootprint* Footprint = nullptr;
		double ProjectionCm = 0.0;
	};

	TArray<FBlockingObstacle> blockingObstacles;
	for (const FEpisodePedestrianObstacleFootprint& footprint : obstacleFootprints)
	{
		if (!SegmentIntersectsFootprint(startLocation, goalLocation, footprint, clearanceCm))
		{
			continue;
		}

		const FVector startToCenter = footprint.Center - startLocation;
		const double projectionCm = FMath::Clamp(FVector::DotProduct(startToCenter, direction), 0.0, pathLength);
		blockingObstacles.Add(FBlockingObstacle{ &footprint, projectionCm });
	}

	blockingObstacles.Sort(
		[](const FBlockingObstacle& left, const FBlockingObstacle& rightObstacle)
		{
			if (!FMath::IsNearlyEqual(left.ProjectionCm, rightObstacle.ProjectionCm))
			{
				return left.ProjectionCm < rightObstacle.ProjectionCm;
			}

			const FString leftId = left.Footprint ? left.Footprint->InstanceId : FString();
			const FString rightId = rightObstacle.Footprint ? rightObstacle.Footprint->InstanceId : FString();
			return leftId < rightId;
		});

	for (const FBlockingObstacle& blockingObstacle : blockingObstacles)
	{
		if (!blockingObstacle.Footprint) continue;

		const FEpisodePedestrianObstacleFootprint& footprint = *blockingObstacle.Footprint;
		const FVector projectionPoint = startLocation + direction * blockingObstacle.ProjectionCm;
		const double forwardHalfExtent =
			FMath::Abs(direction.X) * footprint.Extent.X
			+ FMath::Abs(direction.Y) * footprint.Extent.Y;
		const double lateralHalfExtent =
			FMath::Abs(right.X) * footprint.Extent.X
			+ FMath::Abs(right.Y) * footprint.Extent.Y;

		const double sideDot = FVector::DotProduct(footprint.Center - projectionPoint, right);
		int32 side = sideDot >= 0.0 ? -1 : 1;
		if (FMath::IsNearlyZero(sideDot))
		{
			side = (GetTypeHash(footprint.InstanceId) % 2) == 0 ? -1 : 1;
		}

		const FVector lateralOffset = right * static_cast<double>(side) * (lateralHalfExtent + clearanceCm);
		FVector entryPoint = projectionPoint - direction * (forwardHalfExtent + clearanceCm) + lateralOffset;
		FVector exitPoint = projectionPoint + direction * (forwardHalfExtent + clearanceCm) + lateralOffset;
		entryPoint.Z = startLocation.Z;
		exitPoint.Z = startLocation.Z;

		if (FVector::DistSquared2D(polyline.Last(), entryPoint) > FMath::Square(MinPlanSegmentLengthCm))
		{
			polyline.Add(entryPoint);
		}
		if (FVector::DistSquared2D(polyline.Last(), exitPoint) > FMath::Square(MinPlanSegmentLengthCm))
		{
			polyline.Add(exitPoint);
		}
	}

	if (FVector::DistSquared2D(polyline.Last(), goalLocation) > FMath::Square(MinPlanSegmentLengthCm))
	{
		polyline.Add(goalLocation);
	}

	return polyline;
}

bool FEpisodePedestrianPlanBuilder::SegmentIntersectsFootprint(
	const FVector& segmentStart,
	const FVector& segmentEnd,
	const FEpisodePedestrianObstacleFootprint& footprint,
	double clearanceCm)
{
	const FVector2D start = ToVector2D(segmentStart);
	const FVector2D end = ToVector2D(segmentEnd);
	const FVector2D direction = end - start;
	const FVector2D minPoint(
		footprint.Center.X - footprint.Extent.X - clearanceCm,
		footprint.Center.Y - footprint.Extent.Y - clearanceCm);
	const FVector2D maxPoint(
		footprint.Center.X + footprint.Extent.X + clearanceCm,
		footprint.Center.Y + footprint.Extent.Y + clearanceCm);

	double tMin = 0.0;
	double tMax = 1.0;

	const auto updateSlab = [&tMin, &tMax](double startValue, double directionValue, double minValue, double maxValue)
	{
		if (FMath::IsNearlyZero(directionValue))
		{
			return startValue >= minValue && startValue <= maxValue;
		}

		double t1 = (minValue - startValue) / directionValue;
		double t2 = (maxValue - startValue) / directionValue;
		if (t1 > t2)
		{
			Swap(t1, t2);
		}

		tMin = FMath::Max(tMin, t1);
		tMax = FMath::Min(tMax, t2);
		return tMin <= tMax;
	};

	return updateSlab(start.X, direction.X, minPoint.X, maxPoint.X)
		&& updateSlab(start.Y, direction.Y, minPoint.Y, maxPoint.Y);
}

void FEpisodePedestrianPlanBuilder::AppendPlanPoints(
	const TArray<FVector>& polyline,
	double speedCmPerSecond,
	TArray<FEpisodePedestrianPlanPoint>& outPoints,
	double& outDurationSeconds)
{
	outPoints.Reset();
	outDurationSeconds = 0.0;

	if (polyline.IsEmpty()) return;

	double cumulativeDistanceCm = 0.0;
	for (int32 pointIndex = 0; pointIndex < polyline.Num(); ++pointIndex)
	{
		if (pointIndex > 0)
		{
			cumulativeDistanceCm += FVector::Dist2D(polyline[pointIndex - 1], polyline[pointIndex]);
		}

		FVector direction = FVector::ForwardVector;
		if (polyline.IsValidIndex(pointIndex + 1))
		{
			direction = (polyline[pointIndex + 1] - polyline[pointIndex]).GetSafeNormal2D();
		}
		else if (pointIndex > 0)
		{
			direction = (polyline[pointIndex] - polyline[pointIndex - 1]).GetSafeNormal2D();
		}

		FEpisodePedestrianPlanPoint planPoint;
		planPoint.Location = polyline[pointIndex];
		planPoint.Direction = direction.IsNearlyZero() ? FVector::ForwardVector : direction;
		planPoint.DistanceCm = cumulativeDistanceCm;
		planPoint.TimeSeconds = cumulativeDistanceCm / speedCmPerSecond;
		planPoint.SpeedCmPerSecond = speedCmPerSecond;
		outPoints.Add(planPoint);
	}

	outDurationSeconds = outPoints.Last().TimeSeconds;
}

FString FEpisodePedestrianPlanBuilder::BuildResolvedFootprintHash(const TArray<FEpisodePedestrianObstacleFootprint>& obstacleFootprints)
{
	TArray<FEpisodePedestrianObstacleFootprint> sortedFootprints = obstacleFootprints;
	sortedFootprints.Sort(
		[](const FEpisodePedestrianObstacleFootprint& left, const FEpisodePedestrianObstacleFootprint& right)
		{
			return left.InstanceId < right.InstanceId;
		});

	uint32 hash = 0;
	for (const FEpisodePedestrianObstacleFootprint& footprint : sortedFootprints)
	{
		HashString(hash, footprint.InstanceId);
		HashString(hash, footprint.AssetId);
		HashString(hash, FormatVectorForHash(footprint.Center));
		HashString(hash, FormatVectorForHash(footprint.Extent));
	}

	return HashToString(hash);
}

FString FEpisodePedestrianPlanBuilder::BuildPlanHash(
	const FEpisodeDynamicActorSpec& dynamicActorSpec,
	const FEpisodePedestrianPlanBuildContext& buildContext,
	const FString& resolvedFootprintHash,
	const TArray<FEpisodePedestrianPlanPoint>& planPoints)
{
	uint32 hash = 0;
	HashString(hash, buildContext.SourceSpecHash);
	HashString(hash, buildContext.SemanticNavigationHash);
	HashString(hash, resolvedFootprintHash);
	HashString(hash, dynamicActorSpec.InstanceId);
	HashString(hash, dynamicActorSpec.AssetId);

	for (const FEpisodePedestrianPlanPoint& planPoint : planPoints)
	{
		HashString(hash, FormatVectorForHash(planPoint.Location));
		HashString(hash, FString::Printf(TEXT("%.6f"), planPoint.DistanceCm));
		HashString(hash, FString::Printf(TEXT("%.6f"), planPoint.TimeSeconds));
	}

	return HashToString(hash);
}

FString FEpisodePedestrianPlanBuilder::BuildBehaviorHash(const FEpisodePedestrianBehaviorParams& behaviorParams)
{
	uint32 hash = 0;
	HashString(hash, FString::Printf(TEXT("%.6f"), behaviorParams.Cooperation));
	HashString(hash, FString::Printf(TEXT("%.6f"), behaviorParams.Evasiveness));
	HashString(hash, FString::Printf(TEXT("%.6f"), behaviorParams.PersonalSpaceCm));
	HashString(hash, FString::Printf(TEXT("%.6f"), behaviorParams.AwarenessHorizonSeconds));
	HashString(hash, FString::Printf(TEXT("%.6f"), behaviorParams.MaxYieldWaitSeconds));
	HashString(hash, FString::Printf(TEXT("%.6f"), behaviorParams.SidestepDistanceCm));
	return HashToString(hash);
}

FString FEpisodePedestrianPlanBuilder::BuildPedestrianScenarioHash(const FString& planHash, const FString& behaviorHash)
{
	uint32 hash = 0;
	HashString(hash, planHash);
	HashString(hash, behaviorHash);
	return HashToString(hash);
}

FString FEpisodePedestrianPlanBuilder::FormatVectorForHash(const FVector& value)
{
	return FString::Printf(TEXT("%.3f,%.3f,%.3f"), value.X, value.Y, value.Z);
}
