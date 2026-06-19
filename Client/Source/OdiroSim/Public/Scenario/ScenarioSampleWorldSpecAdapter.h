#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSampleTypes.h"

// Converts latest-version scenario_sample documents into the existing runtime world spec boundary.
struct ODIROSIM_API FScenarioSampleWorldSpecAdapter
{
	// Validates and adapts an already-populated scenario_sample document into a runtime world spec.
	static FScenarioCompileResult CompileScenarioWorldSpecFromSampleDocument(const FScenarioSampleDocument& Document);
};
