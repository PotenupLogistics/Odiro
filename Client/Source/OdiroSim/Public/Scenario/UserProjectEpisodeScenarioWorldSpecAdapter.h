#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"

// Converts user-project episode_scenario JSON into the runtime world spec boundary.
struct ODIROSIM_API FUserProjectEpisodeScenarioWorldSpecAdapter
{
	// Returns true when the file advertises the episode_scenario schema name.
	static bool IsEpisodeScenarioFile(const FString& jsonFilePath);

	// Parses, validates, and adapts an episode_scenario JSON file into a runtime world spec.
	static FScenarioCompileResult CompileScenarioWorldSpecFromEpisodeScenarioFile(const FString& jsonFilePath);
};
