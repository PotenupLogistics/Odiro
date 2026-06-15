#pragma once

#include "CoreMinimal.h"
#include "Scenario/ScenarioTemplateSampler.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSampleTypes.h"
#include "Shared/ScenarioSchemaTypes.h"
#include "Shared/ScenarioTemplateTypes.h"

/** Result of compiling an authored scenario template through its deterministic sample layer. */
struct ODIROSIM_API FScenarioTemplateWorldSpecCompileResult
{
	/** True only when template parsing, sample generation, and runtime world spec compilation all succeed. */
	bool bSuccess = false;

	/** Frozen scenario sample generated from the template and request. */
	FScenarioSampleDocument SampleDocument;

	/** Runtime world spec compile result produced from the generated sample. */
	FScenarioCompileResult CompileResult;

	/** Template parse and sampling diagnostics retained before runtime world spec compilation. */
	TArray<FScenarioSchemaDiagnostic> SamplingDiagnostics;
};

/** Adapter that keeps scenario_template sampling outside the concrete runtime compiler. */
struct ODIROSIM_API FScenarioTemplateWorldSpecAdapter
{
	/** Returns true when the JSON file is a scenario_template document. */
	static bool IsScenarioTemplateFile(const FString& JsonFilePath);

	/** Builds a deterministic sample request for a template file and optional run pair id. */
	static FScenarioTemplateSampleRequest MakeDefaultSampleRequest(const FString& TemplateJsonPath, const FString& PairId = FString());

	/** Parses a template file, generates a deterministic sample, and compiles that sample to a runtime world spec. */
	static FScenarioTemplateWorldSpecCompileResult CompileScenarioWorldSpecFromTemplateFile(
		const FString& JsonFilePath,
		const FScenarioTemplateSampleRequest& Request);

	/** Generates a deterministic sample from a template document and compiles it to a runtime world spec. */
	static FScenarioTemplateWorldSpecCompileResult CompileScenarioWorldSpecFromTemplateDocument(
		const FScenarioTemplateDocument& TemplateDocument,
		const FScenarioTemplateSampleRequest& Request);
};
