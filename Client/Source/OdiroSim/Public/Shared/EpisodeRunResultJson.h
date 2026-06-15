#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeResultTypes.h"

// Serializer for canonical run outputs under experiments/<Experiment>/runs/<RunId>.
struct ODIROSIM_API FEpisodeRunResultJson
{
	// Serializes one episode terminal result without embedding event lines.
	static bool TryWriteEpisodeResultJson(
		const FEpisodeRunRecord& Record,
		FString& OutJson,
		TArray<FString>& OutDiagnostics);

	// Serializes one episode's result-relevant events as JSON Lines.
	static bool TryWriteEpisodeEventsJsonl(
		const FEpisodeRunRecord& Record,
		FString& OutJsonl,
		TArray<FString>& OutDiagnostics);

	// Serializes the run-level summary table from completed episode records.
	static bool TryWriteRunSummaryJson(
		const TArray<FEpisodeRunRecord>& Records,
		FString& OutJson,
		TArray<FString>& OutDiagnostics);
};
