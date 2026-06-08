#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeMeasurementLogTypes.h"

class ADeliveryBot;
class UEpisodeLogSubjectRegistry;

/// Converts Delivery Bot runtime state into measurement log payloads.
class PROTOROBOTSIM_API FEpisodeRobotMeasurementAdapter
{
public:
	/// Builds the robot section for one tick record.
	static bool BuildRobotTick(
		ADeliveryBot* RobotActor,
		const UEpisodeLogSubjectRegistry* SubjectRegistry,
		FEpisodeMeasurementLogRobotTick& OutRobotTick,
		TArray<FEpisodeMeasurementLogDiagnostic>& OutDiagnostics,
		double WorldTimeSeconds);

private:
	static FString ResolveActorId(
		const AActor* Actor,
		const UEpisodeLogSubjectRegistry* SubjectRegistry);

	static FEpisodeMeasurementLogActorState BuildActorState(const AActor* Actor);
};
