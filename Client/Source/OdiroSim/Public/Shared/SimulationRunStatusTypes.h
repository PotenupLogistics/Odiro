#pragma once

#include "CoreMinimal.h"
#include "SimulationRunStatusTypes.generated.h"

// Launcher UI lifecycle state exposed by a child simulator process.
UENUM(BlueprintType)
enum class ESimulationRunState : uint8
{
	Pending,
	Running,
	Completed,
	Failed,
	Canceled
};

// Status payload written by a child simulator process for launcher polling.
USTRUCT(BlueprintType)
struct ODIROSIM_API FSimulationRunStatus
{
	GENERATED_BODY()

	// JSON schema name; v1 stores simulation_run_status.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString Schema = TEXT("simulation_run_status");

	// JSON schema version supported by the current status reader/writer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status", meta = (ClampMin = "1"))
	int32 Version = 1;

	// Stable simulator run id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString RunId;

	// Current run lifecycle state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	ESimulationRunState State = ESimulationRunState::Pending;

	// Experiment folder that started this run; legacy JSON key remains setup_path for status compatibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString SetupPath;

	// UTC timestamp for the last status update.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString UpdatedAt;

	// Currently running scenario sample or pair id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString CurrentPairId;

	// Number of completed scenario samples.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status", meta = (ClampMin = "0"))
	int32 CompletedRuns = 0;

	// Total scenario sample count in this run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status", meta = (ClampMin = "0"))
	int32 TotalRuns = 0;

	// Completed result JSON paths exposed to the launcher UI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	TArray<FString> ReportPaths;

	// Generated measurement or event log paths exposed to the launcher UI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	TArray<FString> LogPaths;

	// Human-readable failure message; empty values are serialized as null.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Status")
	FString Error;
};

// Reader and writer for simulation_run_status JSON files used by launcher polling.
struct ODIROSIM_API FSimulationRunStatusJson
{
	// Parses a status JSON string.
	static bool TryReadStatusJson(
		const FString& JsonString,
		FSimulationRunStatus& OutStatus,
		TArray<FString>& OutDiagnostics);

	// Parses a project-relative or absolute status JSON file.
	static bool ParseFromFile(
		const FString& StatusFilePath,
		FSimulationRunStatus& OutStatus,
		TArray<FString>& OutDiagnostics);

	// Serializes a status payload to compact JSON.
	static bool TryWriteStatusJson(
		const FSimulationRunStatus& Status,
		FString& OutJson,
		TArray<FString>& OutDiagnostics);

	// Writes a status payload to a project-relative or absolute JSON file.
	static bool SaveToFile(
		const FSimulationRunStatus& Status,
		const FString& StatusFilePath,
		TArray<FString>& OutDiagnostics);
};
