#pragma once

#include "CoreMinimal.h"
#include "ScenarioSchemaTypes.generated.h"

// Scenario schema validation or repair result severity shared by template and sample documents.
UENUM(BlueprintType)
enum class EScenarioSchemaDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Repair,
	Error
};

// Corridor axis representation supported by scenario_template and scenario_sample v1.
UENUM(BlueprintType)
enum class EScenarioCorridorAxisType : uint8
{
	Polyline
};

// Numeric template field representation in authored JSON: either a fixed value or a seeded range.
UENUM(BlueprintType)
enum class EScenarioTemplateNumberValueMode : uint8
{
	Fixed,
	Range
};

// String template field representation in authored JSON: either a fixed value or one seeded choice.
UENUM(BlueprintType)
enum class EScenarioTemplateStringValueMode : uint8
{
	Fixed,
	Choices
};

// Concrete scalar type used by scenario_sample params after template sampling.
UENUM(BlueprintType)
enum class EScenarioSampleParamValueType : uint8
{
	None,
	Boolean,
	Integer,
	Float,
	String,
	FloatArray,
	StringArray
};

// Validation, warning, or repair note emitted while reading or generating a schema document.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSchemaDiagnostic
{
	GENERATED_BODY()

	// Severity controls whether the document can continue through generation or runtime preview.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	EScenarioSchemaDiagnosticSeverity Severity = EScenarioSchemaDiagnosticSeverity::Info;

	// Stable machine-readable diagnostic code.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	FString Code;

	// JSON path or semantic field path where the diagnostic applies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	FString Path;

	// Human-readable diagnostic text for editor status and logs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	FString Message;
};

// Authored number field that may be absent, fixed, or sampled from a min/max range.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateNumberValue
{
	GENERATED_BODY()

	// True when the source JSON explicitly provided this field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	bool bIsSet = false;

	// Storage mode selected by the source JSON shape.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateNumberValueMode Mode = EScenarioTemplateNumberValueMode::Fixed;

	// Fixed numeric value when Mode is Fixed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	double FixedValue = 0.0;

	// Inclusive lower bound when Mode is Range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	double MinValue = 0.0;

	// Inclusive upper bound when Mode is Range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	double MaxValue = 0.0;
};

// Authored integer field that may be absent, fixed, or sampled from a min/max range.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateIntegerValue
{
	GENERATED_BODY()

	// True when the source JSON explicitly provided this field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	bool bIsSet = false;

	// Storage mode selected by the source JSON shape.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateNumberValueMode Mode = EScenarioTemplateNumberValueMode::Fixed;

	// Fixed integer value when Mode is Fixed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	int32 FixedValue = 0;

	// Inclusive lower bound when Mode is Range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	int32 MinValue = 0;

	// Inclusive upper bound when Mode is Range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	int32 MaxValue = 0;
};

// Authored string field that may be absent, fixed, or sampled from a choice list.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioTemplateStringValue
{
	GENERATED_BODY()

	// True when the source JSON explicitly provided this field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	bool bIsSet = false;

	// Storage mode selected by the source JSON shape.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	EScenarioTemplateStringValueMode Mode = EScenarioTemplateStringValueMode::Fixed;

	// Fixed string value when Mode is Fixed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	FString FixedValue;

	// Candidate values when Mode is Choices.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Template")
	TArray<FString> Choices;
};

// Corridor distance interval measured in meters along the route axis.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioAlongRangeMeters
{
	GENERATED_BODY()

	// Start distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	double StartMeters = 0.0;

	// End distance along the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	double EndMeters = 0.0;
};

// Corridor lateral interval measured in meters from the route axis.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioOffsetRangeMeters
{
	GENERATED_BODY()

	// Minimum lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	double MinMeters = 0.0;

	// Maximum lateral offset from the corridor axis in meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Schema")
	double MaxMeters = 0.0;
};

// Concrete value stored in scenario_sample.scenario.params after seed resolution.
USTRUCT(BlueprintType)
struct ODIROSIM_API FScenarioSampleParamValue
{
	GENERATED_BODY()

	// Active value type for this sampled parameter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	EScenarioSampleParamValueType Type = EScenarioSampleParamValueType::None;

	// Boolean payload when Type is Boolean.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	bool BoolValue = false;

	// Integer payload when Type is Integer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	int32 IntegerValue = 0;

	// Floating point payload when Type is Float.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	double FloatValue = 0.0;

	// String payload when Type is String.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	FString StringValue;

	// Numeric array payload when Type is FloatArray.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<double> FloatArrayValue;

	// String array payload when Type is StringArray.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Sample")
	TArray<FString> StringArrayValue;
};
