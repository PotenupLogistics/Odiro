#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioPedestrianPlanTypes.h"

class FScenarioPedestrianPlanBuilder
{
public:
	static bool BuildPlans(
		const FScenarioSimulationSetupSpec& setupSpec,
		const FScenarioPedestrianPlanBuildContext& buildContext,
		FScenarioPedestrianPlanBuildResult& outResult);

private:
	static bool IsPlannedPedestrian(const FScenarioDynamicActorSpec& dynamicActorSpec);
	static bool TryGetFloatProperty(const TMap<FString, FScenarioParamValue>& properties, const FString& key, double& outValue);
	static bool TryGetStringProperty(const TMap<FString, FScenarioParamValue>& properties, const FString& key, FString& outValue);
	static bool TryGetVectorProperty(const TMap<FString, FScenarioParamValue>& properties, const FString& key, FVector& outValue);
	static FScenarioPedestrianBehaviorParams BuildBehaviorParams(const FScenarioDynamicActorSpec& dynamicActorSpec);
	static FScenarioPedestrianPathShapeParams BuildPathShapeParams(const FScenarioDynamicActorSpec& dynamicActorSpec);

	static bool BuildPlanForPedestrian(
		const FScenarioDynamicActorSpec& dynamicActorSpec,
		const FScenarioPedestrianPlanBuildContext& buildContext,
		const FString& resolvedFootprintHash,
		FScenarioPedestrianPlan& outPlan,
		TArray<FString>& outDiagnostics);

	static TArray<FVector> BuildPolyline(
		const FVector& startLocation,
		const FVector& goalLocation,
		const TArray<FScenarioPedestrianObstacleFootprint>& obstacleFootprints,
		double clearanceCm);

	static TArray<FVector> BuildCurvedPolyline(
		const TArray<FVector>& polyline,
		const FString& instanceId,
		const FScenarioPedestrianPathShapeParams& pathShapeParams,
		const TArray<FScenarioPedestrianObstacleFootprint>& obstacleFootprints,
		double clearanceCm);

	static TArray<FVector> BuildTwoPointBezierPolyline(
		const TArray<FVector>& polyline,
		const FString& instanceId,
		const FScenarioPedestrianPathShapeParams& pathShapeParams);

	static TArray<FVector> BuildCornerRoundedPolyline(
		const TArray<FVector>& polyline,
		const FScenarioPedestrianPathShapeParams& pathShapeParams);

	static bool PathIntersectsFootprints(
		const TArray<FVector>& polyline,
		const TArray<FScenarioPedestrianObstacleFootprint>& obstacleFootprints,
		double clearanceCm);

	static void AppendPointIfSeparated(TArray<FVector>& points, const FVector& point);

	static FVector EvaluateCubicBezier(
		const FVector& p0,
		const FVector& p1,
		const FVector& p2,
		const FVector& p3,
		double alpha);

	static FVector EvaluateQuadraticBezier(
		const FVector& p0,
		const FVector& p1,
		const FVector& p2,
		double alpha);

	static bool SegmentIntersectsFootprint(
		const FVector& segmentStart,
		const FVector& segmentEnd,
		const FScenarioPedestrianObstacleFootprint& footprint,
		double clearanceCm);

	static void AppendPlanPoints(
		const TArray<FVector>& polyline,
		double speedCmPerSecond,
		TArray<FScenarioPedestrianPlanPoint>& outPoints,
		double& outDurationSeconds);

	static FString BuildResolvedFootprintHash(const TArray<FScenarioPedestrianObstacleFootprint>& obstacleFootprints);
	static FString BuildPlanHash(
		const FScenarioDynamicActorSpec& dynamicActorSpec,
		const FScenarioPedestrianPlanBuildContext& buildContext,
		const FString& resolvedFootprintHash,
		const TArray<FScenarioPedestrianPlanPoint>& planPoints);
	static FString BuildBehaviorHash(const FScenarioPedestrianBehaviorParams& behaviorParams);
	static FString BuildPedestrianScenarioHash(const FString& planHash, const FString& behaviorHash);

	static FString FormatVectorForHash(const FVector& value);
};
