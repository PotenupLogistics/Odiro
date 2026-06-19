#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioDocumentJson.generated.h"

// Parse and validation result for a scenario authoring JSON document.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioDocumentParseResult
{
	GENERATED_BODY()

	// True when no error diagnostics were produced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	bool bSuccess = false;

	// Parsed authoring document for the project scenario.json contract.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	FScenarioDocument Document;

	// Parse and validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Document")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// JSON reader, writer, and latest-version validator for scenario authoring documents.
struct ODIROSIM_API FScenarioDocumentJson
{
	// Latest scenario schema version supported by this reader, writer, and validator.
	static constexpr int32 SupportedVersion = 1;

	// Parses and validates a scenario JSON file.
	static FScenarioDocumentParseResult ParseFromFile(const FString& JsonFilePath);

	// Parses and validates a scenario JSON string.
	static FScenarioDocumentParseResult ParseFromString(const FString& JsonString);

	// Parses and validates a user project scenario JSON file for editor authoring.
	static FScenarioDocumentParseResult ParseProjectScenarioFromFile(const FString& JsonFilePath);

	// Parses and validates a user project scenario JSON string for editor authoring.
	static FScenarioDocumentParseResult ParseProjectScenarioFromString(const FString& JsonString);

	// Validates an already-populated scenario document against the latest schema version.
	static bool ValidateDocument(
		const FScenarioDocument& Document,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes a valid scenario document to formatted JSON.
	static bool TryWriteJson(
		const FScenarioDocument& Document,
		FString& OutJson,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes a valid editor draft as the user project scenario.json contract.
	static bool TryWriteProjectScenarioJson(
		const FScenarioDocument& Document,
		FString& OutJson,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes and saves a valid scenario document to disk.
	static bool SaveToFile(
		const FScenarioDocument& Document,
		const FString& JsonFilePath,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);
};
