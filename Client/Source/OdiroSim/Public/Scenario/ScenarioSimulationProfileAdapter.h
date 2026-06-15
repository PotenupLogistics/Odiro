#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"

/** Result of compiling a simulation_profile JSON document into DeliveryBot runtime setup. */
struct ODIROSIM_API FScenarioSimulationProfileCompileResult
{
	/** True when the profile JSON matches the latest supported schema and can be mapped to setup info. */
	bool bSuccess = false;

	/** Profile identifier read from the source document when present. */
	FString ProfileId;

	/** Stable hash of the profile source JSON used by sample lineage and run records. */
	FString ProfileHash;

	/** DeliveryBot setup values that can be injected through ADeliveryBot::InitializeSetupInfo. */
	FDeliveryBotSetupInfo SetupInfo;

	/** Validation and mapping diagnostics. */
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

/** Adapter for the latest simulation_profile JSON contract used by scenario template runs. */
struct ODIROSIM_API FScenarioSimulationProfileAdapter
{
	/** Latest simulation_profile schema version supported by this adapter. */
	static constexpr int32 SupportedVersion = 1;

	/** Returns true when the JSON file declares schema == simulation_profile. */
	static bool IsSimulationProfileFile(const FString& JsonFilePath);

	/** Returns a stable hash for a profile file, or hash:unset when the file cannot be read. */
	static FString MakeProfileFileHash(const FString& JsonFilePath);

	/** Parses and maps a simulation_profile JSON file into DeliveryBot setup info. */
	static FScenarioSimulationProfileCompileResult CompileProfileFromJsonFile(const FString& JsonFilePath);

	/** Parses and maps a simulation_profile JSON string into DeliveryBot setup info. */
	static FScenarioSimulationProfileCompileResult CompileProfileFromJsonString(const FString& JsonString);
};
