#include "Shared/ExperimentSettingTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scenario/ScenarioTemplateSampler.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioTemplateJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* ExperimentSettingSchema = TEXT("experiment_setting");
	const TCHAR* ExperimentProfileFileName = TEXT("profile.json");
	const TCHAR* ExperimentSettingFileName = TEXT("setting.json");
	const TCHAR* ExperimentScenariosDirectoryName = TEXT("scenarios");
	const TCHAR* ExperimentRunsDirectoryName = TEXT("runs");
	const TCHAR* ExperimentUnsetHash = TEXT("hash:unset");

	void AddExperimentDiagnostic(
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		EScenarioSchemaDiagnosticSeverity Severity,
		const FString& Code,
		const FString& Path,
		const FString& Message)
	{
		FScenarioSchemaDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Path = Path;
		Diagnostic.Message = Message;
		Diagnostics.Add(Diagnostic);
	}

	bool ExperimentHasErrors(const TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		for (const FScenarioSchemaDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	FString NormalizeExperimentPath(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Path;
	}

	FString MakeProjectRelativeIfPossible(FString Path)
	{
		if (Path.IsEmpty())
		{
			return Path;
		}

		Path = NormalizeExperimentPath(Path);
		if (FPaths::IsRelative(Path))
		{
			return Path;
		}

		const FString ProjectDir = NormalizeExperimentPath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		FString RelativePath = Path;
		if (FPaths::MakePathRelativeTo(RelativePath, *ProjectDir))
		{
			return NormalizeExperimentPath(RelativePath);
		}

		return Path;
	}

	bool IsEmptyExperimentString(const FString& Value)
	{
		return Value.TrimStartAndEnd().IsEmpty();
	}

	bool TryReadExperimentObjectField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			return false;
		}

		if (Value->Type != EJson::Object)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be an object."), *Path, *FieldName));
			return false;
		}

		OutObject = Value->AsObject();
		return OutObject.IsValid();
	}

	bool TryReadRequiredExperimentStringField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FString& OutValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			return false;
		}

		if (Value->Type != EJson::String)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *Path, *FieldName));
			return false;
		}

		OutValue = Value->AsString();
		if (IsEmptyExperimentString(OutValue))
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("empty_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must not be empty."), *Path, *FieldName));
			return false;
		}

		return true;
	}

	bool TryReadOptionalExperimentStringField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FString& OutValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			return true;
		}

		if (Value->Type != EJson::String)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *Path, *FieldName));
			return false;
		}

		OutValue = Value->AsString();
		return true;
	}

	bool TryReadExperimentIntField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		int32& OutValue,
		int32 MinValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			return false;
		}

		if (Value->Type != EJson::Number)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *Path, *FieldName));
			return false;
		}

		const int32 IntValue = FMath::RoundToInt(Value->AsNumber());
		if (IntValue < MinValue)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be >= %d."), *Path, *FieldName, MinValue));
			return false;
		}

		OutValue = IntValue;
		return true;
	}

	bool TryReadExperimentInt64Field(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		int64& OutValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			return false;
		}

		if (Value->Type != EJson::Number)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *Path, *FieldName));
			return false;
		}

		OutValue = static_cast<int64>(Value->AsNumber());
		return true;
	}

	bool TryReadExperimentNumberField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		double& OutValue,
		double MinValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			return false;
		}

		if (Value->Type != EJson::Number)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *Path, *FieldName));
			return false;
		}

		OutValue = Value->AsNumber();
		if (OutValue < MinValue)
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be >= %.3f."), *Path, *FieldName, MinValue));
			return false;
		}

		return true;
	}

	void ParseExperimentSampling(
		const FJsonObject& RootObject,
		FExperimentSettingParseResult& Result)
	{
		TSharedPtr<FJsonObject> SamplingObject;
		if (!TryReadExperimentObjectField(RootObject, TEXT("sampling"), TEXT("$"), Result.Diagnostics, SamplingObject))
		{
			return;
		}

		TryReadRequiredExperimentStringField(
			*SamplingObject,
			TEXT("scenario_template_ref"),
			TEXT("$.sampling"),
			Result.Diagnostics,
			Result.Document.Sampling.ScenarioTemplateRef);
		TryReadRequiredExperimentStringField(
			*SamplingObject,
			TEXT("profile_template_ref"),
			TEXT("$.sampling"),
			Result.Diagnostics,
			Result.Document.Sampling.ProfileTemplateRef);
		TryReadExperimentInt64Field(
			*SamplingObject,
			TEXT("base_seed"),
			TEXT("$.sampling"),
			Result.Diagnostics,
			Result.Document.Sampling.BaseSeed);
		TryReadExperimentIntField(
			*SamplingObject,
			TEXT("sample_count"),
			TEXT("$.sampling"),
			Result.Diagnostics,
			Result.Document.Sampling.SampleCount,
			1);
		if (TryReadRequiredExperimentStringField(
			*SamplingObject,
			TEXT("generator_version"),
			TEXT("$.sampling"),
			Result.Diagnostics,
			Result.Document.Sampling.GeneratorVersion)
			&& !Result.Document.Sampling.GeneratorVersion.Equals(FScenarioTemplateSampler::GeneratorVersion, ESearchCase::CaseSensitive))
		{
			AddExperimentDiagnostic(
				Result.Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("unsupported_generator_version"),
				TEXT("$.sampling.generator_version"),
				FString::Printf(
					TEXT("experiment_setting generator_version must match the current sampler version: %s."),
					FScenarioTemplateSampler::GeneratorVersion));
		}
	}

	void ParseExperimentRuntime(
		const FJsonObject& RootObject,
		FExperimentSettingParseResult& Result)
	{
		TSharedPtr<FJsonObject> RuntimeObject;
		if (!TryReadExperimentObjectField(RootObject, TEXT("runtime"), TEXT("$"), Result.Diagnostics, RuntimeObject))
		{
			return;
		}

		TryReadRequiredExperimentStringField(
			*RuntimeObject,
			TEXT("map_id"),
			TEXT("$.runtime"),
			Result.Diagnostics,
			Result.Document.Runtime.MapId);
		TryReadExperimentIntField(
			*RuntimeObject,
			TEXT("fixed_fps"),
			TEXT("$.runtime"),
			Result.Diagnostics,
			Result.Document.Runtime.FixedFps,
			1);
		TryReadExperimentNumberField(
			*RuntimeObject,
			TEXT("time_scale"),
			TEXT("$.runtime"),
			Result.Diagnostics,
			Result.Document.Runtime.TimeScale,
			0.0);

		const TSharedPtr<FJsonValue> MaxDurationValue = RuntimeObject->TryGetField(TEXT("max_duration_s"));
		if (MaxDurationValue.IsValid())
		{
			TryReadExperimentNumberField(
				*RuntimeObject,
				TEXT("max_duration_s"),
				TEXT("$.runtime"),
				Result.Diagnostics,
				Result.Document.Runtime.MaxDurationSeconds,
				0.0);
		}
	}

	void ParseExperimentEvaluation(
		const FJsonObject& RootObject,
		FExperimentSettingParseResult& Result)
	{
		TSharedPtr<FJsonObject> EvaluationObject;
		if (!TryReadExperimentObjectField(RootObject, TEXT("evaluation"), TEXT("$"), Result.Diagnostics, EvaluationObject))
		{
			return;
		}

		TryReadExperimentNumberField(
			*EvaluationObject,
			TEXT("goal_acceptance_radius_m"),
			TEXT("$.evaluation"),
			Result.Diagnostics,
			Result.Document.Evaluation.GoalAcceptanceRadiusMeters,
			0.0);
		TryReadExperimentNumberField(
			*EvaluationObject,
			TEXT("tip_over_angle_deg"),
			TEXT("$.evaluation"),
			Result.Diagnostics,
			Result.Document.Evaluation.TipOverAngleDegrees,
			0.0);
		TryReadExperimentNumberField(
			*EvaluationObject,
			TEXT("near_miss_distance_m"),
			TEXT("$.evaluation"),
			Result.Diagnostics,
			Result.Document.Evaluation.NearMissDistanceMeters,
			0.0);
	}

	void ParseExperimentSettingObject(
		const FJsonObject& RootObject,
		FExperimentSettingParseResult& Result)
	{
		TryReadRequiredExperimentStringField(RootObject, TEXT("schema"), TEXT("$"), Result.Diagnostics, Result.Document.Schema);
		if (!Result.Document.Schema.Equals(ExperimentSettingSchema, ESearchCase::CaseSensitive))
		{
			AddExperimentDiagnostic(
				Result.Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("invalid_schema"),
				TEXT("$.schema"),
				FString::Printf(TEXT("$.schema must be '%s'."), ExperimentSettingSchema));
		}

		TryReadExperimentIntField(RootObject, TEXT("version"), TEXT("$"), Result.Diagnostics, Result.Document.Version, 1);
		if (Result.Document.Version != FExperimentSettingJson::SupportedVersion)
		{
			AddExperimentDiagnostic(
				Result.Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("unsupported_version"),
				TEXT("$.version"),
				FString::Printf(
					TEXT("experiment_setting version must match the current validator version: %d."),
					FExperimentSettingJson::SupportedVersion));
		}

		TryReadRequiredExperimentStringField(RootObject, TEXT("experiment_id"), TEXT("$"), Result.Diagnostics, Result.Document.ExperimentId);
		TryReadOptionalExperimentStringField(RootObject, TEXT("display_name"), TEXT("$"), Result.Diagnostics, Result.Document.DisplayName);
		ParseExperimentSampling(RootObject, Result);
		ParseExperimentRuntime(RootObject, Result);
		ParseExperimentEvaluation(RootObject, Result);
	}

	TSharedRef<FJsonObject> MakeExperimentSamplingObject(const FExperimentSamplingSettings& Sampling)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("scenario_template_ref"), Sampling.ScenarioTemplateRef);
		Object->SetStringField(TEXT("profile_template_ref"), Sampling.ProfileTemplateRef);
		Object->SetNumberField(TEXT("base_seed"), static_cast<double>(Sampling.BaseSeed));
		Object->SetNumberField(TEXT("sample_count"), Sampling.SampleCount);
		Object->SetStringField(TEXT("generator_version"), Sampling.GeneratorVersion);
		return Object;
	}

	TSharedRef<FJsonObject> MakeExperimentRuntimeObject(const FExperimentRuntimeSettings& Runtime)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("map_id"), Runtime.MapId);
		Object->SetNumberField(TEXT("fixed_fps"), Runtime.FixedFps);
		Object->SetNumberField(TEXT("time_scale"), Runtime.TimeScale);
		if (Runtime.MaxDurationSeconds > 0.0)
		{
			Object->SetNumberField(TEXT("max_duration_s"), Runtime.MaxDurationSeconds);
		}
		return Object;
	}

	TSharedRef<FJsonObject> MakeExperimentEvaluationObject(const FExperimentEvaluationSettings& Evaluation)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("goal_acceptance_radius_m"), Evaluation.GoalAcceptanceRadiusMeters);
		Object->SetNumberField(TEXT("tip_over_angle_deg"), Evaluation.TipOverAngleDegrees);
		Object->SetNumberField(TEXT("near_miss_distance_m"), Evaluation.NearMissDistanceMeters);
		return Object;
	}

	TSharedRef<FJsonObject> MakeExperimentSettingObject(const FExperimentSettingDocument& Document)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), Document.Schema);
		Object->SetNumberField(TEXT("version"), Document.Version);
		Object->SetStringField(TEXT("experiment_id"), Document.ExperimentId);
		if (!Document.DisplayName.IsEmpty())
		{
			Object->SetStringField(TEXT("display_name"), Document.DisplayName);
		}
		Object->SetObjectField(TEXT("sampling"), MakeExperimentSamplingObject(Document.Sampling));
		Object->SetObjectField(TEXT("runtime"), MakeExperimentRuntimeObject(Document.Runtime));
		Object->SetObjectField(TEXT("evaluation"), MakeExperimentEvaluationObject(Document.Evaluation));
		return Object;
	}

	FString BuildExperimentFilePath(const FString& ExperimentRef, const FString& FileName)
	{
		return NormalizeExperimentPath(FPaths::Combine(ExperimentRef, FileName));
	}

	FString BuildExperimentDirectoryPath(const FString& ExperimentRef, const FString& DirectoryName)
	{
		return NormalizeExperimentPath(FPaths::Combine(ExperimentRef, DirectoryName));
	}

	void AppendExperimentDiagnostics(
		const TArray<FScenarioSchemaDiagnostic>& Source,
		TArray<FScenarioSchemaDiagnostic>& Target)
	{
		Target.Append(Source);
	}

	bool FileExistsForExperiment(const FString& Path)
	{
		return FPaths::FileExists(FExperimentSettingJson::ResolveProjectPath(Path));
	}

	bool EnsureExperimentProfile(
		const FString& ExperimentRef,
		const FExperimentSettingDocument& Setting,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		const FString ProfilePath = BuildExperimentFilePath(ExperimentRef, ExperimentProfileFileName);
		if (FileExistsForExperiment(ProfilePath))
		{
			return true;
		}

		const FString ProfileTemplatePath = Setting.Sampling.ProfileTemplateRef.TrimStartAndEnd();
		if (!FileExistsForExperiment(ProfileTemplatePath))
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("profile_template_missing"),
				TEXT("$.sampling.profile_template_ref"),
				FString::Printf(TEXT("Profile template is missing: %s"), *ProfileTemplatePath));
			return false;
		}

		FString ProfileJsonText;
		const FString ResolvedTemplatePath = FExperimentSettingJson::ResolveProjectPath(ProfileTemplatePath);
		if (!FFileHelper::LoadFileToString(ProfileJsonText, *ResolvedTemplatePath))
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("profile_template_read_failed"),
				TEXT("$.sampling.profile_template_ref"),
				FString::Printf(TEXT("Profile template read failed: %s"), *ResolvedTemplatePath));
			return false;
		}

		const FString ResolvedProfilePath = FExperimentSettingJson::ResolveProjectPath(ProfilePath);
		const FString ProfileDirectory = FPaths::GetPath(ResolvedProfilePath);
		if (!IFileManager::Get().MakeDirectory(*ProfileDirectory, true))
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("profile_directory_create_failed"),
				TEXT("profile.json"),
				FString::Printf(TEXT("Experiment profile directory create failed: %s"), *ProfileDirectory));
			return false;
		}

		if (!FFileHelper::SaveStringToFile(ProfileJsonText, *ResolvedProfilePath))
		{
			AddExperimentDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("profile_copy_failed"),
				TEXT("profile.json"),
				FString::Printf(TEXT("Experiment profile copy failed: %s"), *ResolvedProfilePath));
			return false;
		}

		return true;
	}
}

FExperimentSettingParseResult FExperimentSettingJson::ParseFromFile(const FString& JsonFilePath)
{
	FExperimentSettingParseResult Result;
	if (JsonFilePath.TrimStartAndEnd().IsEmpty())
	{
		AddExperimentDiagnostic(
			Result.Diagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			TEXT("empty_experiment_setting_path"),
			TEXT("$"),
			TEXT("experiment_setting file path must not be empty."));
		return Result;
	}

	const FString ResolvedPath = ResolveProjectPath(JsonFilePath);
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ResolvedPath))
	{
		AddExperimentDiagnostic(
			Result.Diagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			TEXT("experiment_setting_file_missing"),
			TEXT("$"),
			FString::Printf(TEXT("experiment_setting JSON read failed: %s"), *ResolvedPath));
		return Result;
	}

	return ParseFromString(JsonString);
}

FExperimentSettingParseResult FExperimentSettingJson::ParseFromString(const FString& JsonString)
{
	FExperimentSettingParseResult Result;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddExperimentDiagnostic(
			Result.Diagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			TEXT("invalid_experiment_setting_json"),
			TEXT("$"),
			TEXT("experiment_setting JSON parse failed."));
		return Result;
	}

	ParseExperimentSettingObject(*RootObject, Result);
	Result.bSuccess = !ExperimentHasErrors(Result.Diagnostics);
	return Result;
}

bool FExperimentSettingJson::ValidateDocument(
	const FExperimentSettingDocument& Document,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!Document.Schema.Equals(ExperimentSettingSchema, ESearchCase::CaseSensitive))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_schema"), TEXT("$.schema"), FString::Printf(TEXT("$.schema must be '%s'."), ExperimentSettingSchema));
	}
	if (Document.Version != SupportedVersion)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("unsupported_version"), TEXT("$.version"), FString::Printf(TEXT("experiment_setting version must match the current validator version: %d."), SupportedVersion));
	}
	if (IsEmptyExperimentString(Document.ExperimentId))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_experiment_id"), TEXT("$.experiment_id"), TEXT("$.experiment_id must not be empty."));
	}
	if (IsEmptyExperimentString(Document.Sampling.ScenarioTemplateRef))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_scenario_template_ref"), TEXT("$.sampling.scenario_template_ref"), TEXT("$.sampling.scenario_template_ref must not be empty."));
	}
	if (IsEmptyExperimentString(Document.Sampling.ProfileTemplateRef))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_profile_template_ref"), TEXT("$.sampling.profile_template_ref"), TEXT("$.sampling.profile_template_ref must not be empty."));
	}
	if (Document.Sampling.SampleCount <= 0)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_sample_count"), TEXT("$.sampling.sample_count"), TEXT("$.sampling.sample_count must be > 0."));
	}
	if (IsEmptyExperimentString(Document.Sampling.GeneratorVersion))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_generator_version"), TEXT("$.sampling.generator_version"), TEXT("$.sampling.generator_version must not be empty."));
	}
	else if (!Document.Sampling.GeneratorVersion.Equals(FScenarioTemplateSampler::GeneratorVersion, ESearchCase::CaseSensitive))
	{
		AddExperimentDiagnostic(
			OutDiagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			TEXT("unsupported_generator_version"),
			TEXT("$.sampling.generator_version"),
			FString::Printf(
				TEXT("experiment_setting generator_version must match the current sampler version: %s."),
				FScenarioTemplateSampler::GeneratorVersion));
	}
	if (IsEmptyExperimentString(Document.Runtime.MapId))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_map_id"), TEXT("$.runtime.map_id"), TEXT("$.runtime.map_id must not be empty."));
	}
	if (Document.Runtime.FixedFps <= 0)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_fixed_fps"), TEXT("$.runtime.fixed_fps"), TEXT("$.runtime.fixed_fps must be > 0."));
	}
	if (Document.Runtime.TimeScale < 0.0)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_time_scale"), TEXT("$.runtime.time_scale"), TEXT("$.runtime.time_scale must be >= 0."));
	}
	if (Document.Evaluation.GoalAcceptanceRadiusMeters < 0.0)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_goal_acceptance_radius_m"), TEXT("$.evaluation.goal_acceptance_radius_m"), TEXT("$.evaluation.goal_acceptance_radius_m must be >= 0."));
	}
	if (Document.Evaluation.TipOverAngleDegrees < 0.0)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_tip_over_angle_deg"), TEXT("$.evaluation.tip_over_angle_deg"), TEXT("$.evaluation.tip_over_angle_deg must be >= 0."));
	}
	if (Document.Evaluation.NearMissDistanceMeters < 0.0)
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_near_miss_distance_m"), TEXT("$.evaluation.near_miss_distance_m"), TEXT("$.evaluation.near_miss_distance_m must be >= 0."));
	}

	return !ExperimentHasErrors(OutDiagnostics);
}

bool FExperimentSettingJson::TryWriteJson(
	const FExperimentSettingDocument& Document,
	FString& OutJson,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutJson.Reset();
	if (!ValidateDocument(Document, OutDiagnostics))
	{
		return false;
	}

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(MakeExperimentSettingObject(Document), Writer))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("serialization_failed"), TEXT("$"), TEXT("experiment_setting JSON serialization failed."));
		return false;
	}

	return true;
}

bool FExperimentSettingJson::SaveToFile(
	const FExperimentSettingDocument& Document,
	const FString& JsonFilePath,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (JsonFilePath.TrimStartAndEnd().IsEmpty())
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_output_path"), TEXT("$"), TEXT("experiment_setting output path must not be empty."));
		return false;
	}

	FString Json;
	if (!TryWriteJson(Document, Json, OutDiagnostics))
	{
		return false;
	}

	const FString ResolvedPath = ResolveProjectPath(JsonFilePath);
	const FString Directory = FPaths::GetPath(ResolvedPath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("directory_create_failed"), TEXT("$"), FString::Printf(TEXT("experiment_setting directory create failed: %s"), *Directory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Json, *ResolvedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddExperimentDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("file_write_failed"), TEXT("$"), FString::Printf(TEXT("experiment_setting file write failed: %s"), *ResolvedPath));
		return false;
	}

	return true;
}

FString FExperimentSettingJson::ResolveProjectPath(const FString& FilePath)
{
	if (FilePath.IsEmpty() || !FPaths::IsRelative(FilePath))
	{
		return FilePath;
	}

	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), FilePath));
}

FString FExperimentSettingJson::MakeFileHash(const FString& JsonFilePath)
{
	const FString ResolvedPath = ResolveProjectPath(JsonFilePath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
	{
		return ExperimentUnsetHash;
	}

	return FString::Printf(TEXT("hash:%u"), GetTypeHash(JsonText));
}

FString FExperimentSettingJson::BuildExperimentSettingPath(const FString& ExperimentRef)
{
	return BuildExperimentFilePath(ExperimentRef, ExperimentSettingFileName);
}

FString FExperimentSettingJson::BuildExperimentProfilePath(const FString& ExperimentRef)
{
	return BuildExperimentFilePath(ExperimentRef, ExperimentProfileFileName);
}

FString FExperimentSettingJson::BuildExperimentScenariosDirectory(const FString& ExperimentRef)
{
	return BuildExperimentDirectoryPath(ExperimentRef, ExperimentScenariosDirectoryName);
}

FString FExperimentSettingJson::BuildExperimentRunsDirectory(const FString& ExperimentRef)
{
	return BuildExperimentDirectoryPath(ExperimentRef, ExperimentRunsDirectoryName);
}

FString FExperimentSettingJson::MakeSampleId(const int32 SampleIndex)
{
	return FString::Printf(TEXT("%06d"), FMath::Max(0, SampleIndex) + 1);
}

FExperimentRunInputBuildResult FExperimentSettingJson::EnsureScenarioSamples(
	const FString& ExperimentRef,
	const FExperimentSettingDocument& Setting)
{
	FExperimentRunInputBuildResult Result;
	const FString NormalizedExperimentRef = NormalizeExperimentPath(ExperimentRef.TrimStartAndEnd());
	if (NormalizedExperimentRef.IsEmpty())
	{
		AddExperimentDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_experiment_ref"), TEXT("$"), TEXT("experiment_ref must not be empty."));
		return Result;
	}

	if (!EnsureExperimentProfile(NormalizedExperimentRef, Setting, Result.Diagnostics))
	{
		return Result;
	}

	const FString ProfilePath = BuildExperimentProfilePath(NormalizedExperimentRef);

	FScenarioTemplateParseResult TemplateParseResult = FScenarioTemplateJson::ParseFromFile(Setting.Sampling.ScenarioTemplateRef);
	AppendExperimentDiagnostics(TemplateParseResult.Diagnostics, Result.Diagnostics);
	if (!TemplateParseResult.bSuccess)
	{
		Result.bSuccess = false;
		return Result;
	}

	const FString ScenariosDirectory = BuildExperimentScenariosDirectory(NormalizedExperimentRef);
	const FString ResolvedScenariosDirectory = ResolveProjectPath(ScenariosDirectory);
	if (!IFileManager::Get().MakeDirectory(*ResolvedScenariosDirectory, true))
	{
		AddExperimentDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("scenarios_directory_create_failed"), TEXT("scenarios"), FString::Printf(TEXT("Scenario sample directory create failed: %s"), *ResolvedScenariosDirectory));
		return Result;
	}

	const FString SettingPath = BuildExperimentSettingPath(NormalizedExperimentRef);
	const FString TemplateHash = MakeFileHash(Setting.Sampling.ScenarioTemplateRef);
	const FString ProfileHash = MakeFileHash(ProfilePath);
	const FString SettingHash = MakeFileHash(SettingPath);
	const FString GeneratorVersion = Setting.Sampling.GeneratorVersion.IsEmpty()
		? FString(FScenarioTemplateSampler::GeneratorVersion)
		: Setting.Sampling.GeneratorVersion;

	for (int32 SampleIndex = 0; SampleIndex < Setting.Sampling.SampleCount; ++SampleIndex)
	{
		const FString SampleId = MakeSampleId(SampleIndex);
		const FString SamplePath = NormalizeExperimentPath(FPaths::Combine(ScenariosDirectory, FString::Printf(TEXT("%s.json"), *SampleId)));
		Result.ScenarioSampleJsonPaths.Add(SamplePath);

		if (FileExistsForExperiment(SamplePath))
		{
			FScenarioSampleParseResult SampleParseResult = FScenarioSampleJson::ParseFromFile(SamplePath);
			AppendExperimentDiagnostics(SampleParseResult.Diagnostics, Result.Diagnostics);
			continue;
		}

		FScenarioTemplateSampleRequest Request;
		Request.SampleId = SampleId;
		Request.ScenarioId = FString::Printf(TEXT("%s_%s"), *TemplateParseResult.Document.TemplateId, *SampleId);
		Request.Seed = Setting.Sampling.BaseSeed + SampleIndex;
		Request.TemplateRef = MakeProjectRelativeIfPossible(Setting.Sampling.ScenarioTemplateRef);
		Request.TemplateHash = TemplateHash;
		Request.ProfileRef = MakeProjectRelativeIfPossible(ProfilePath);
		Request.ProfileHash = ProfileHash;
		Request.SettingRef = MakeProjectRelativeIfPossible(SettingPath);
		Request.SettingHash = SettingHash;
		Request.GeneratorVersion = GeneratorVersion;

		FScenarioTemplateSampleResult SampleResult =
			FScenarioTemplateSampler::GenerateSample(TemplateParseResult.Document, Request);
		AppendExperimentDiagnostics(SampleResult.Diagnostics, Result.Diagnostics);
		if (!SampleResult.bSuccess)
		{
			continue;
		}

		TArray<FScenarioSchemaDiagnostic> SaveDiagnostics;
		if (!FScenarioSampleJson::SaveToFile(SampleResult.Document, SamplePath, SaveDiagnostics))
		{
			AppendExperimentDiagnostics(SaveDiagnostics, Result.Diagnostics);
		}
	}

	Result.bSuccess = !ExperimentHasErrors(Result.Diagnostics);
	return Result;
}

FExperimentRunInputBuildResult FExperimentSettingJson::BuildRunInputsFromExperiment(
	const FString& ExperimentRef,
	const FExperimentSampleSelection& Selection)
{
	FExperimentRunInputBuildResult Result;
	const FString NormalizedExperimentRef = NormalizeExperimentPath(ExperimentRef.TrimStartAndEnd());
	if (NormalizedExperimentRef.IsEmpty())
	{
		AddExperimentDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_experiment_ref"), TEXT("$"), TEXT("experiment_ref must not be empty."));
		return Result;
	}

	const FString SettingPath = BuildExperimentSettingPath(NormalizedExperimentRef);
	const FExperimentSettingParseResult SettingParseResult = ParseFromFile(SettingPath);
	AppendExperimentDiagnostics(SettingParseResult.Diagnostics, Result.Diagnostics);
	if (!SettingParseResult.bSuccess)
	{
		Result.bSuccess = false;
		return Result;
	}

	FExperimentRunInputBuildResult SampleResult = EnsureScenarioSamples(NormalizedExperimentRef, SettingParseResult.Document);
	AppendExperimentDiagnostics(SampleResult.Diagnostics, Result.Diagnostics);
	if (!SampleResult.bSuccess)
	{
		Result.ScenarioSampleJsonPaths = SampleResult.ScenarioSampleJsonPaths;
		Result.bSuccess = false;
		return Result;
	}

	TArray<FString> SelectedSampleIds;
	if (Selection.Kind == EExperimentSampleSelectionKind::ExplicitIds)
	{
		SelectedSampleIds = Selection.SampleIds;
	}
	else
	{
		for (int32 SampleIndex = 0; SampleIndex < SettingParseResult.Document.Sampling.SampleCount; ++SampleIndex)
		{
			SelectedSampleIds.Add(MakeSampleId(SampleIndex));
		}
	}

	const FString ProfilePath = BuildExperimentProfilePath(NormalizedExperimentRef);
	const FString ScenariosDirectory = BuildExperimentScenariosDirectory(NormalizedExperimentRef);
	for (const FString& SampleIdValue : SelectedSampleIds)
	{
		const FString SampleId = SampleIdValue.TrimStartAndEnd();
		if (SampleId.IsEmpty())
		{
			AddExperimentDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_sample_id"), TEXT("sample_selection.sample_ids"), TEXT("sample_selection.sample_ids must not contain empty values."));
			continue;
		}

		const FString SamplePath = NormalizeExperimentPath(FPaths::Combine(ScenariosDirectory, FString::Printf(TEXT("%s.json"), *SampleId)));
		if (!FileExistsForExperiment(SamplePath))
		{
			AddExperimentDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("scenario_sample_missing"), TEXT("scenarios"), FString::Printf(TEXT("Scenario sample is missing: %s"), *SamplePath));
			continue;
		}

		FScenarioRunInput RunInput;
		RunInput.PairId = SampleId;
		RunInput.ScenarioSourceJsonPath = MakeProjectRelativeIfPossible(SamplePath);
		RunInput.SimulationProfileJsonPath = MakeProjectRelativeIfPossible(ProfilePath);
		Result.RunInputs.Add(RunInput);
		Result.ScenarioSampleJsonPaths.Add(RunInput.ScenarioSourceJsonPath);
	}

	if (Result.RunInputs.IsEmpty())
	{
		AddExperimentDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("no_selected_samples"), TEXT("sample_selection"), TEXT("Experiment run requires at least one selected scenario sample."));
	}

	Result.bSuccess = !ExperimentHasErrors(Result.Diagnostics);
	return Result;
}
