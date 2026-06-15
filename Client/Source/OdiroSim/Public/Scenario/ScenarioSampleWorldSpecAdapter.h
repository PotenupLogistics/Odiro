#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSampleTypes.h"

// Converts latest-version scenario_sample documents into the existing runtime world spec boundary.
struct ODIROSIM_API FScenarioSampleWorldSpecAdapter
{
	// Returns true when the file advertises the scenario_sample schema name.
	static bool IsScenarioSampleFile(const FString& JsonFilePath);

	// Parses, validates, and adapts a scenario_sample JSON file into a runtime world spec.
	static FScenarioCompileResult CompileScenarioWorldSpecFromSampleFile(const FString& JsonFilePath);

	// Parses, validates, and adapts a scenario_sample JSON string into a runtime world spec.
	static FScenarioCompileResult CompileScenarioWorldSpecFromSampleString(const FString& JsonString);

	// Validates and adapts an already-populated scenario_sample document into a runtime world spec.
	static FScenarioCompileResult CompileScenarioWorldSpecFromSampleDocument(const FScenarioSampleDocument& Document);
};
