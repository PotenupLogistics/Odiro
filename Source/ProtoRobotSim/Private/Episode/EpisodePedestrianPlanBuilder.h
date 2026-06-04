#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodePedestrianPlanTypes.h"

class FEpisodePedestrianPlanBuilder
{
public:
	static bool BuildPlans(
		const FEpisodeSimulationSetupSpec& setupSpec,
		const FEpisodePedestrianPlanBuildContext& buildContext,
		FEpisodePedestrianPlanBuildResult& outResult);

private:
	static bool IsPlannedPedestrian(const FEpisodeDynamicActorSpec& dynamicActorSpec);
	static bool TryGetFloatProperty(const TMap<FString, FEpisodeParamValue>& properties, const FString& key, double& outValue);
	static bool TryGetStringProperty(const TMap<FString, FEpisodeParamValue>& properties, const FString& key, FString& outValue);
	static bool TryGetVectorProperty(const TMap<FString, FEpisodeParamValue>& properties, const FString& key, FVector& outValue);
	static FEpisodePedestrianBehaviorParams BuildBehaviorParams(const FEpisodeDynamicActorSpec& dynamicActorSpec);

	static bool BuildPlanForPedestrian(
		const FEpisodeDynamicActorSpec& dynamicActorSpec,
		const FEpisodePedestrianPlanBuildContext& buildContext,
		const FString& resolvedFootprintHash,
		FEpisodePedestrianPlan& outPlan,
		TArray<FString>& outDiagnostics);

	static TArray<FVector> BuildPolyline(
		const FVector& startLocation,
		const FVector& goalLocation,
		const TArray<FEpisodePedestrianObstacleFootprint>& obstacleFootprints,
		double clearanceCm);

	static bool SegmentIntersectsFootprint(
		const FVector& segmentStart,
		const FVector& segmentEnd,
		const FEpisodePedestrianObstacleFootprint& footprint,
		double clearanceCm);

	static void AppendPlanPoints(
		const TArray<FVector>& polyline,
		double speedCmPerSecond,
		TArray<FEpisodePedestrianPlanPoint>& outPoints,
		double& outDurationSeconds);

	static FString BuildResolvedFootprintHash(const TArray<FEpisodePedestrianObstacleFootprint>& obstacleFootprints);
	static FString BuildPlanHash(
		const FEpisodeDynamicActorSpec& dynamicActorSpec,
		const FEpisodePedestrianPlanBuildContext& buildContext,
		const FString& resolvedFootprintHash,
		const TArray<FEpisodePedestrianPlanPoint>& planPoints);
	static FString BuildBehaviorHash(const FEpisodePedestrianBehaviorParams& behaviorParams);
	static FString BuildPedestrianScenarioHash(const FString& planHash, const FString& behaviorHash);

	static FString FormatVectorForHash(const FVector& value);
};
