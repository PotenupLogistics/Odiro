#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioTemplateJson.generated.h"

// Parse and validation result for a scenario_template JSON document.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateParseResult
{
	GENERATED_BODY()

	// True when no error diagnostics were produced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	bool bSuccess = false;

	// Parsed scenario_template document.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FScenarioTemplateDocument Document;

	// Parse and validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// JSON reader, writer, and latest-version validator for scenario_template documents.
struct ODIROSIM_API FScenarioTemplateJson
{
	// Latest scenario_template schema version supported by this compiler and validator.
	static constexpr int32 SupportedVersion = 1;

	// Parses and validates a scenario_template JSON file.
	static FScenarioTemplateParseResult ParseFromFile(const FString& JsonFilePath);

	// Parses and validates a scenario_template JSON string.
	static FScenarioTemplateParseResult ParseFromString(const FString& JsonString);

	// Validates an already-populated scenario_template document against the latest schema version.
	static bool ValidateDocument(
		const FScenarioTemplateDocument& Document,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes a valid scenario_template document to formatted JSON.
	static bool TryWriteJson(
		const FScenarioTemplateDocument& Document,
		FString& OutJson,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes and saves a valid scenario_template document to disk.
	static bool SaveToFile(
		const FScenarioTemplateDocument& Document,
		const FString& JsonFilePath,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);
};
