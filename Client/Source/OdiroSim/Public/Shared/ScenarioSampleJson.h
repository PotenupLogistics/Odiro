#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSampleTypes.h"
#include "ScenarioSampleJson.generated.h"

// Parse and validation result for a scenario_sample JSON document.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleParseResult
{
	GENERATED_BODY()

	// True when no error diagnostics were produced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	bool bSuccess = false;

	// Parsed scenario_sample document.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FScenarioSampleDocument Document;

	// Parse and validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// JSON reader, writer, and latest-version validator for scenario_sample documents.
struct ODIROSIM_API FScenarioSampleJson
{
	// Latest scenario_sample schema version supported by this compiler and validator.
	static constexpr int32 SupportedVersion = 1;

	// Parses and validates a scenario_sample JSON file.
	static FScenarioSampleParseResult ParseFromFile(const FString& JsonFilePath);

	// Parses and validates a scenario_sample JSON string.
	static FScenarioSampleParseResult ParseFromString(const FString& JsonString);

	// Validates an already-populated scenario_sample document against the latest schema version.
	static bool ValidateDocument(
		const FScenarioSampleDocument& Document,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes a valid scenario_sample document to formatted JSON.
	static bool TryWriteJson(
		const FScenarioSampleDocument& Document,
		FString& OutJson,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes and saves a valid scenario_sample document to disk.
	static bool SaveToFile(
		const FScenarioSampleDocument& Document,
		const FString& JsonFilePath,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);
};
