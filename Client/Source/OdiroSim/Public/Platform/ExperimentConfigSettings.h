#pragma once

#include "CoreMinimal.h"
#include "ExperimentConfigSettings.generated.h"

// user project setting.json에서 Platform UI가 편집하는 experiment 설정 subset.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentConfigSettings
{
	GENERATED_BODY()

	// runtime.map_id 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	FString MapId = TEXT("ScenarioSimulationMap");

	// runtime.fixed_fps 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	int32 FixedFps = 60;

	// runtime.time_scale 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	float TimeScale = 1.0f;

	// runtime.max_duration_s 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	float MaxDurationSeconds = 60.0f;

	// sampling.episode_count 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	int32 EpisodeCount = 1;

	// sampling.base_seed 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	int64 BaseSeed = 0;

	// evaluation.tip_over_angle_deg 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	float TipOverAngleDegrees = 60.0f;

	// evaluation.near_miss_distance_m 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	float NearMissDistanceMeters = 0.5f;

	// evaluation.goal_acceptance_radius_m 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	float GoalAcceptanceRadiusMeters = 1.0f;
};
