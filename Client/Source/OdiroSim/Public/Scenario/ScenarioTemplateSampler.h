#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSampleTypes.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioTemplateSampler.generated.h"

// Source references and deterministic seed used to generate one scenario_sample from a template.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateSampleRequest
{
	GENERATED_BODY()

	// Experiment-local sample id; generated from Seed when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString SampleId;

	// Scenario id used for result joins; generated from template id and sample id when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString ScenarioId;

	// Concrete seed used by all deterministic sampler streams.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	int64 Seed = 0;

	// Source scenario_template path recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString TemplateRef;

	// Canonical template hash recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString TemplateHash;

	// Experiment profile path recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString ProfileRef;

	// Canonical profile hash recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString ProfileHash;

	// Experiment setting path recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString SettingRef;

	// Canonical setting hash recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString SettingHash;

	// Sampler implementation version recorded into sample.source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FString GeneratorVersion;
};

// Result of deterministic scenario_template sampling.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateSampleResult
{
	GENERATED_BODY()

	// True when template validation, generation, and sample validation all succeeded without errors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	bool bSuccess = false;

	// Generated scenario_sample document.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	FScenarioSampleDocument Document;

	// Template validation, generation, repair, and sample validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|TemplateSampler")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// Deterministic scenario_template to scenario_sample generator for latest supported schema versions.
struct ODIROSIM_API FScenarioTemplateSampler
{
	// Current generator version string stored in scenario_sample.sample.source.generator_version.
	static const TCHAR* GeneratorVersion;

	// Generates a frozen scenario_sample document from a validated scenario_template document.
	static FScenarioTemplateSampleResult GenerateSample(
		const FScenarioTemplateDocument& TemplateDocument,
		const FScenarioTemplateSampleRequest& Request);
};
