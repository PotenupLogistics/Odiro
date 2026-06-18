#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSampleTypes.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioSampler.generated.h"

// Source references and deterministic seed used to materialize one preview scenario sample.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSamplerRequest
{
	GENERATED_BODY()

	// Experiment-local sample id; generated from Seed when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString SampleId;

	// Scenario id used for result joins; generated from source scenario id and sample id when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString ScenarioId;

	// Concrete seed used by all deterministic sampler streams.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	int64 Seed = 0;

	// Source scenario path recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString SourceScenarioRef;

	// Canonical scenario hash recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString SourceScenarioHash;

	// Experiment profile path recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString ProfileRef;

	// Canonical profile hash recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString ProfileHash;

	// Experiment setting path recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString SettingRef;

	// Canonical setting hash recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString SettingHash;

	// Sampler implementation version recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FString GeneratorVersion;
};

// Result of deterministic scenario document sampling.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSamplerResult
{
	GENERATED_BODY()

	// True when scenario validation, generation, and sample validation all succeeded without errors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	bool bSuccess = false;

	// Generated scenario_sample document.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	FScenarioSampleDocument Document;

	// Scenario validation, generation, repair, and sample validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|ScenarioSampler")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// Deterministic scenario document to scenario sample generator for latest supported schema versions.
struct ODIROSIM_API FScenarioSampler
{
	// Current generator version string stored in scenario_sample.sample.source.generator_version.
	static const TCHAR* GeneratorVersion;

	// Generates a frozen scenario_sample document from a validated scenario document.
	static FScenarioSamplerResult GenerateSample(
		const FScenarioDocument& ScenarioDocument,
		const FScenarioSamplerRequest& Request);
};
