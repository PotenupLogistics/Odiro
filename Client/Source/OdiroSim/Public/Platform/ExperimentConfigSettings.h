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

	// sampling.episode_count 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	int32 EpisodeCount = 1;

	// sampling.base_seed 값.
	UPROPERTY(BlueprintReadWrite, Category = "Platform|ExperimentConfig")
	int64 BaseSeed = 0;
};
