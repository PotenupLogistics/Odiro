#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioConfigTypes.h"
#include "Shared/ScenarioSchemaTypes.h"
#include "ExperimentSettingTypes.generated.h"

// Scenario sample selection mode used by simulation_setup and experiment execution.
UENUM(BlueprintType)
enum class EExperimentSampleSelectionKind : uint8
{
	All,
	ExplicitIds
};

// Scenario sample subset requested for one simulation run.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentSampleSelection
{
	GENERATED_BODY()

	// Selection mode; All runs every materialized sample in the experiment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	EExperimentSampleSelectionKind Kind = EExperimentSampleSelectionKind::All;

	// Experiment-local sample ids used when Kind is ExplicitIds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	TArray<FString> SampleIds;
};

// Sampling settings stored in experiments/<Experiment>/setting.json.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentSamplingSettings
{
	GENERATED_BODY()

	// Source scenario_template path selected from the reusable template library.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString ScenarioTemplateRef;

	// Source simulation_profile template path selected from the reusable profile library.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString ProfileTemplateRef;

	// Base seed used to derive deterministic sample seeds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	int64 BaseSeed = 0;

	// Number of scenario_sample files expected under experiments/<Experiment>/scenarios.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "1"))
	int32 SampleCount = 1;

	// Scenario template sampler implementation version expected by this setting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString GeneratorVersion = TEXT("scenario_template_sampler_v1");
};

// Runtime settings stored in experiments/<Experiment>/setting.json.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentRuntimeSettings
{
	GENERATED_BODY()

	// UE level identifier opened by simulator mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString MapId = TEXT("ScenarioSimulationMap");

	// Fixed simulation step rate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "1"))
	int32 FixedFps = 60;

	// Runtime time scale requested by the experiment; currently stored for the process boundary.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "0.0"))
	double TimeScale = 1.0;

	// Optional maximum episode duration in seconds; zero means the runtime default applies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "0.0"))
	double MaxDurationSeconds = 0.0;
};

// Evaluation thresholds stored in experiments/<Experiment>/setting.json.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentEvaluationSettings
{
	GENERATED_BODY()

	// Goal reach threshold in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "0.0"))
	double GoalAcceptanceRadiusMeters = 0.5;

	// Robot tip-over threshold in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "0.0"))
	double TipOverAngleDegrees = 60.0;

	// Pedestrian near-miss threshold in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "0.0"))
	double NearMissDistanceMeters = 0.5;
};

// Canonical experiment setting document stored at experiments/<Experiment>/setting.json.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentSettingDocument
{
	GENERATED_BODY()

	// JSON schema name; v1 stores experiment_setting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString Schema = TEXT("experiment_setting");

	// JSON schema version supported by the current validator.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting", meta = (ClampMin = "1"))
	int32 Version = 1;

	// Experiment folder and result join identifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString ExperimentId;

	// Optional UI display label.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FString DisplayName;

	// Deterministic scenario_sample generation settings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FExperimentSamplingSettings Sampling;

	// Simulator runtime settings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FExperimentRuntimeSettings Runtime;

	// Episode evaluation thresholds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FExperimentEvaluationSettings Evaluation;
};

// Parse and validation result for an experiment_setting JSON document.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentSettingParseResult
{
	GENERATED_BODY()

	// True when no error diagnostics were produced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	bool bSuccess = false;

	// Parsed experiment_setting document.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	FExperimentSettingDocument Document;

	// Parse and validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Setting")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// Result of preparing an experiment folder for one simulator run.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentRunInputBuildResult
{
	GENERATED_BODY()

	// True when samples exist or were generated and run inputs were built.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	bool bSuccess = false;

	// Scenario runner inputs derived from experiment samples and profile.json.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	TArray<FScenarioRunInput> RunInputs;

	// Scenario sample paths selected for the run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	TArray<FString> ScenarioSampleJsonPaths;

	// Preparation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// Request envelope for one simulator run against an experiment folder.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentRunRequest
{
	GENERATED_BODY()

	// Experiment folder that owns setting.json, profile.json, scenarios, and runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	FString ExperimentRef;

	// Stable run id used as the runs/<RunId> folder name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	FString RunId;

	// Scenario sample subset requested for this run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Run")
	FExperimentSampleSelection SampleSelection;
};

// Parsed public simulator command-line options for experiment runs.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentRunCommandLineOptions
{
	GENERATED_BODY()

	// True when the process was launched with -Experiment=<ExperimentRef>.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|CommandLine")
	bool bExperimentRun = false;

	// Experiment run request parsed from command-line switches.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|CommandLine")
	FExperimentRunRequest Request;
};

// Parse result for experiment-run command-line switches.
USTRUCT(BlueprintType)
struct ODIROSIM_API FExperimentRunCommandLineParseResult
{
	GENERATED_BODY()

	// True when command-line parsing produced no errors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|CommandLine")
	bool bSuccess = false;

	// Parsed command-line options.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|CommandLine")
	FExperimentRunCommandLineOptions Options;

	// Command-line validation diagnostics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|CommandLine")
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
};

// JSON reader, writer, and experiment folder helper for experiment_setting documents.
struct ODIROSIM_API FExperimentSettingJson
{
	// Latest experiment_setting schema version supported by this validator.
	static constexpr int32 SupportedVersion = 1;

	// Parses and validates an experiment_setting JSON file.
	static FExperimentSettingParseResult ParseFromFile(const FString& JsonFilePath);

	// Parses and validates an experiment_setting JSON string.
	static FExperimentSettingParseResult ParseFromString(const FString& JsonString);

	// Validates an already-populated experiment_setting document.
	static bool ValidateDocument(
		const FExperimentSettingDocument& Document,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes a valid experiment_setting document to formatted JSON.
	static bool TryWriteJson(
		const FExperimentSettingDocument& Document,
		FString& OutJson,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Serializes and saves a valid experiment_setting document to disk.
	static bool SaveToFile(
		const FExperimentSettingDocument& Document,
		const FString& JsonFilePath,
		TArray<FScenarioSchemaDiagnostic>& OutDiagnostics);

	// Resolves project-relative experiment paths for the current UE project.
	static FString ResolveProjectPath(const FString& FilePath);

	// Returns a lightweight hash of a JSON file's current text.
	static FString MakeFileHash(const FString& JsonFilePath);

	// Returns experiments/<Experiment>/setting.json for an experiment folder path.
	static FString BuildExperimentSettingPath(const FString& ExperimentRef);

	// Returns experiments/<Experiment>/profile.json for an experiment folder path.
	static FString BuildExperimentProfilePath(const FString& ExperimentRef);

	// Returns experiments/<Experiment>/scenarios for an experiment folder path.
	static FString BuildExperimentScenariosDirectory(const FString& ExperimentRef);

	// Returns experiments/<Experiment>/runs for an experiment folder path.
	static FString BuildExperimentRunsDirectory(const FString& ExperimentRef);

	// Returns experiments/<Experiment>/runs/<RunId> for one simulator run.
	static FString BuildExperimentRunDirectory(const FString& ExperimentRef, const FString& RunId);

	// Returns experiments/<Experiment>/runs/<RunId>/status.json for launcher polling.
	static FString BuildExperimentRunStatusPath(const FString& ExperimentRef, const FString& RunId);

	// Returns a stable scenario sample id for a zero-based sample index.
	static FString MakeSampleId(int32 SampleIndex);

	// Creates missing scenario_sample files under the experiment's scenarios folder.
	static FExperimentRunInputBuildResult EnsureScenarioSamples(
		const FString& ExperimentRef,
		const FExperimentSettingDocument& Setting);

	// Builds runner inputs from an experiment folder and sample selection.
	static FExperimentRunInputBuildResult BuildRunInputsFromExperiment(
		const FString& ExperimentRef,
		const FExperimentSampleSelection& Selection);
};

// Parser for the public simulator experiment-run command-line contract.
struct ODIROSIM_API FExperimentRunCommandLine
{
	// Parses an explicit command-line string for -Experiment, -RunId, and -SampleIds switches.
	static FExperimentRunCommandLineParseResult Parse(const FString& CommandLine);

	// Parses the current process command line.
	static FExperimentRunCommandLineParseResult ParseCurrent();
};
