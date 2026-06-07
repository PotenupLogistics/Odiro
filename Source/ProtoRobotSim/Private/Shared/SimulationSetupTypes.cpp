#include "Shared/SimulationSetupTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* SimulationSetupSchema = TEXT("simulation_setup");
	const TCHAR* SimulationRunStatusSchema = TEXT("simulation_run_status");

	void AddSimulationDiagnostic(
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		EEpisodeCompileDiagnosticSeverity severity,
		const FString& code,
		const FString& message)
	{
		FEpisodeCompileDiagnostic diagnostic;
		diagnostic.Severity = severity;
		diagnostic.Code = code;
		diagnostic.Message = message;
		diagnostics.Add(diagnostic);
	}

	bool HasSimulationErrors(const TArray<FEpisodeCompileDiagnostic>& diagnostics)
	{
		for (const FEpisodeCompileDiagnostic& diagnostic : diagnostics)
		{
			if (diagnostic.Severity == EEpisodeCompileDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	bool TryGetObjectField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		TSharedPtr<FJsonObject>& outObject)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s field is required."), *path, *fieldName));
			return false;
		}

		if (jsonValue->Type != EJson::Object)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be an object."), *path, *fieldName));
			return false;
		}

		outObject = jsonValue->AsObject();
		return outObject.IsValid();
	}

	bool TryReadRequiredStringField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		FString& outValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s field is required."), *path, *fieldName));
			return false;
		}

		if (jsonValue->Type != EJson::String)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *path, *fieldName));
			return false;
		}

		outValue = jsonValue->AsString();
		if (outValue.IsEmpty())
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("empty_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must not be empty."), *path, *fieldName));
			return false;
		}

		return true;
	}

	bool TryReadOptionalStringField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		FString& targetValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			return true;
		}

		if (jsonValue->Type != EJson::String)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *path, *fieldName));
			return false;
		}

		targetValue = jsonValue->AsString();
		if (targetValue.IsEmpty())
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("empty_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must not be empty."), *path, *fieldName));
			return false;
		}

		return true;
	}

	bool TryReadOptionalBoolField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		bool& targetValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			return true;
		}

		if (jsonValue->Type != EJson::Boolean)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a boolean."), *path, *fieldName));
			return false;
		}

		targetValue = jsonValue->AsBool();
		return true;
	}

	bool TryReadOptionalIntField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		int32& targetValue,
		int32 minValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			return true;
		}

		if (jsonValue->Type != EJson::Number)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *path, *fieldName));
			return false;
		}

		const double numberValue = jsonValue->AsNumber();
		if (numberValue < minValue)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be >= %d."), *path, *fieldName, minValue));
			return false;
		}

		targetValue = FMath::RoundToInt(numberValue);
		return true;
	}

	bool TryReadRequiredPositiveIntField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		TArray<FEpisodeCompileDiagnostic>& diagnostics,
		int32& outValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s field is required."), *path, *fieldName));
			return false;
		}

		if (jsonValue->Type != EJson::Number)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *path, *fieldName));
			return false;
		}

		const double numberValue = jsonValue->AsNumber();
		if (numberValue <= 0.0)
		{
			AddSimulationDiagnostic(
				diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be > 0."), *path, *fieldName));
			return false;
		}

		outValue = FMath::RoundToInt(numberValue);
		return true;
	}

	void ParseSimulationSetupObject(
		const FJsonObject& rootObject,
		FSimulationSetupParseResult& result)
	{
		TryReadRequiredStringField(rootObject, TEXT("schema"), TEXT("$"), result.Diagnostics, result.Setup.Schema);
		if (!result.Setup.Schema.Equals(SimulationSetupSchema, ESearchCase::CaseSensitive))
		{
			AddSimulationDiagnostic(
				result.Diagnostics,
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("invalid_schema"),
				FString::Printf(TEXT("$.schema must be '%s'."), SimulationSetupSchema));
		}

		TryReadRequiredPositiveIntField(rootObject, TEXT("version"), TEXT("$"), result.Diagnostics, result.Setup.Version);

		TryReadRequiredStringField(rootObject, TEXT("map_id"), TEXT("$"), result.Diagnostics, result.Setup.MapId);
		TryReadRequiredStringField(rootObject, TEXT("run_queue"), TEXT("$"), result.Diagnostics, result.Setup.RunQueueJsonPath);

		TSharedPtr<FJsonObject> fixedStepObject;
		if (TryGetObjectField(rootObject, TEXT("fixed_step"), TEXT("$"), result.Diagnostics, fixedStepObject))
		{
			TryReadRequiredPositiveIntField(
				*fixedStepObject,
				TEXT("fps"),
				TEXT("$.fixed_step"),
				result.Diagnostics,
				result.Setup.FixedStep.Fps);
		}

		TSharedPtr<FJsonObject> loggingObject;
		if (TryGetObjectField(rootObject, TEXT("logging"), TEXT("$"), result.Diagnostics, loggingObject))
		{
			TryReadOptionalBoolField(
				*loggingObject,
				TEXT("measurement_log_enabled"),
				TEXT("$.logging"),
				result.Diagnostics,
				result.Setup.MeasurementLog.bEnabled);
			TryReadOptionalStringField(
				*loggingObject,
				TEXT("measurement_output_directory"),
				TEXT("$.logging"),
				result.Diagnostics,
				result.Setup.MeasurementLog.OutputDirectory);
			TryReadOptionalStringField(
				*loggingObject,
				TEXT("measurement_file_prefix"),
				TEXT("$.logging"),
				result.Diagnostics,
				result.Setup.MeasurementLog.FilePrefix);
			TryReadOptionalIntField(
				*loggingObject,
				TEXT("flush_interval_ticks"),
				TEXT("$.logging"),
				result.Diagnostics,
				result.Setup.MeasurementLog.FlushIntervalTicks,
				1);
			TryReadOptionalBoolField(
				*loggingObject,
				TEXT("flush_on_event"),
				TEXT("$.logging"),
				result.Diagnostics,
				result.Setup.MeasurementLog.bFlushOnEvent);
		}

		TSharedPtr<FJsonObject> reportObject;
		if (TryGetObjectField(rootObject, TEXT("report"), TEXT("$"), result.Diagnostics, reportObject))
		{
			TryReadOptionalBoolField(
				*reportObject,
				TEXT("save_evaluation_report_json"),
				TEXT("$.report"),
				result.Diagnostics,
				result.Setup.Report.bSaveEvaluationReportJson);
			TryReadOptionalStringField(
				*reportObject,
				TEXT("output_directory"),
				TEXT("$.report"),
				result.Diagnostics,
				result.Setup.Report.OutputDirectory);
		}

		TSharedPtr<FJsonObject> statusObject;
		if (TryGetObjectField(rootObject, TEXT("status"), TEXT("$"), result.Diagnostics, statusObject))
		{
			TryReadRequiredStringField(
				*statusObject,
				TEXT("output_path"),
				TEXT("$.status"),
				result.Diagnostics,
				result.Setup.Status.OutputPath);
		}
	}

	bool TryGetSwitchValue(
		const FString& commandLine,
		const FString& key,
		FString& outValue,
		bool& bOutHasBareSwitch)
	{
		bOutHasBareSwitch = false;

		TArray<FString> tokens;
		TArray<FString> switches;
		FCommandLine::Parse(*commandLine, tokens, switches);

		const FString prefix = key + TEXT("=");
		for (const FString& commandSwitch : switches)
		{
			if (commandSwitch.Equals(key, ESearchCase::IgnoreCase))
			{
				bOutHasBareSwitch = true;
				continue;
			}

			if (commandSwitch.StartsWith(prefix, ESearchCase::IgnoreCase))
			{
				outValue = commandSwitch.RightChop(prefix.Len());
				return true;
			}
		}

		return false;
	}

	template <typename TEnum>
	FString ToSimulationEnumString(TEnum value)
	{
		if (const UEnum* enumValue = StaticEnum<TEnum>())
		{
			return enumValue->GetNameStringByValue(static_cast<int64>(value));
		}

		return TEXT("Unknown");
	}

	void SetOptionalStringField(
		const TSharedRef<FJsonObject>& object,
		const FString& fieldName,
		const FString& value)
	{
		if (value.IsEmpty())
		{
			object->SetField(fieldName, MakeShared<FJsonValueNull>());
			return;
		}

		object->SetStringField(fieldName, value);
	}

	TArray<TSharedPtr<FJsonValue>> MakeStringArrayField(const TArray<FString>& values)
	{
		TArray<TSharedPtr<FJsonValue>> jsonValues;
		jsonValues.Reserve(values.Num());
		for (const FString& value : values)
		{
			jsonValues.Add(MakeShared<FJsonValueString>(value));
		}

		return jsonValues;
	}

	FString MakeSimulationStatusProjectRelativeReportPath(FString filePath)
	{
		if (filePath.IsEmpty())
		{
			return filePath;
		}

		FPaths::NormalizeFilename(filePath);
		if (FPaths::IsRelative(filePath))
		{
			filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			return filePath;
		}

		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FString projectRelativePath = filePath;
		if (FPaths::MakePathRelativeTo(projectRelativePath, *projectDir))
		{
			projectRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			return projectRelativePath;
		}

		filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return filePath;
	}

	TArray<TSharedPtr<FJsonValue>> MakeSimulationStatusReportPathArrayField(const TArray<FString>& values)
	{
		TArray<TSharedPtr<FJsonValue>> jsonValues;
		jsonValues.Reserve(values.Num());
		for (const FString& value : values)
		{
			jsonValues.Add(MakeShared<FJsonValueString>(MakeSimulationStatusProjectRelativeReportPath(value)));
		}

		return jsonValues;
	}

	TSharedRef<FJsonObject> MakeSimulationRunStatusObject(const FSimulationRunStatus& status)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), status.Schema);
		object->SetNumberField(TEXT("version"), status.Version);
		object->SetStringField(TEXT("run_id"), status.RunId);
		object->SetStringField(TEXT("state"), ToSimulationEnumString(status.State));
		object->SetStringField(TEXT("setup_path"), status.SetupPath);
		object->SetStringField(TEXT("updated_at"), status.UpdatedAt);
		SetOptionalStringField(object, TEXT("current_pair_id"), status.CurrentPairId);
		object->SetNumberField(TEXT("completed_runs"), status.CompletedRuns);
		object->SetNumberField(TEXT("total_runs"), status.TotalRuns);
		object->SetArrayField(TEXT("report_paths"), MakeSimulationStatusReportPathArrayField(status.ReportPaths));
		object->SetArrayField(TEXT("log_paths"), MakeStringArrayField(status.LogPaths));
		SetOptionalStringField(object, TEXT("error"), status.Error);
		return object;
	}

	TSharedRef<FJsonObject> MakeSimulationSetupObject(const FSimulationSetup& setup)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), setup.Schema);
		object->SetNumberField(TEXT("version"), setup.Version);
		object->SetStringField(TEXT("map_id"), setup.MapId);
		object->SetStringField(TEXT("run_queue"), setup.RunQueueJsonPath);

		TSharedRef<FJsonObject> fixedStepObject = MakeShared<FJsonObject>();
		fixedStepObject->SetNumberField(TEXT("fps"), setup.FixedStep.Fps);
		object->SetObjectField(TEXT("fixed_step"), fixedStepObject);

		TSharedRef<FJsonObject> loggingObject = MakeShared<FJsonObject>();
		loggingObject->SetBoolField(TEXT("measurement_log_enabled"), setup.MeasurementLog.bEnabled);
		loggingObject->SetStringField(TEXT("measurement_output_directory"), setup.MeasurementLog.OutputDirectory);
		loggingObject->SetStringField(TEXT("measurement_file_prefix"), setup.MeasurementLog.FilePrefix);
		loggingObject->SetNumberField(TEXT("flush_interval_ticks"), setup.MeasurementLog.FlushIntervalTicks);
		loggingObject->SetBoolField(TEXT("flush_on_event"), setup.MeasurementLog.bFlushOnEvent);
		object->SetObjectField(TEXT("logging"), loggingObject);

		TSharedRef<FJsonObject> reportObject = MakeShared<FJsonObject>();
		reportObject->SetBoolField(TEXT("save_evaluation_report_json"), setup.Report.bSaveEvaluationReportJson);
		reportObject->SetStringField(TEXT("output_directory"), setup.Report.OutputDirectory);
		object->SetObjectField(TEXT("report"), reportObject);

		TSharedRef<FJsonObject> statusObject = MakeShared<FJsonObject>();
		statusObject->SetStringField(TEXT("output_path"), setup.Status.OutputPath);
		object->SetObjectField(TEXT("status"), statusObject);

		return object;
	}

	bool ValidateSimulationSetupForWrite(
		const FSimulationSetup& setup,
		TArray<FString>& outDiagnostics)
	{
		outDiagnostics.Reset();
		if (!setup.Schema.Equals(SimulationSetupSchema, ESearchCase::CaseSensitive))
		{
			outDiagnostics.Add(FString::Printf(TEXT("SimulationSetup schema must be '%s'."), SimulationSetupSchema));
		}

		if (setup.Version <= 0)
		{
			outDiagnostics.Add(TEXT("SimulationSetup version must be > 0."));
		}

		if (setup.MapId.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(TEXT("SimulationSetup map_id must not be empty."));
		}

		if (setup.RunQueueJsonPath.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(TEXT("SimulationSetup run_queue must not be empty."));
		}

		if (setup.FixedStep.Fps <= 0)
		{
			outDiagnostics.Add(TEXT("SimulationSetup fixed_step.fps must be > 0."));
		}

		if (setup.MeasurementLog.bEnabled && setup.MeasurementLog.OutputDirectory.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(TEXT("SimulationSetup logging.measurement_output_directory must not be empty when logging is enabled."));
		}

		if (setup.MeasurementLog.FilePrefix.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(TEXT("SimulationSetup logging.measurement_file_prefix must not be empty."));
		}

		if (setup.MeasurementLog.FlushIntervalTicks <= 0)
		{
			outDiagnostics.Add(TEXT("SimulationSetup logging.flush_interval_ticks must be > 0."));
		}

		if (setup.Report.bSaveEvaluationReportJson && setup.Report.OutputDirectory.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(TEXT("SimulationSetup report.output_directory must not be empty when report saving is enabled."));
		}

		if (setup.Status.OutputPath.TrimStartAndEnd().IsEmpty())
		{
			outDiagnostics.Add(TEXT("SimulationSetup status.output_path must not be empty."));
		}

		return outDiagnostics.IsEmpty();
	}

	bool TryReadStatusStringField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		TArray<FString>& outDiagnostics,
		FString& outValue,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			if (bRequired)
			{
				outDiagnostics.Add(FString::Printf(TEXT("Simulation run status field missing: %s"), *fieldName));
			}
			return !bRequired;
		}

		if (jsonValue->Type == EJson::Null)
		{
			// Writer가 optional string을 null로 저장하므로 reader에서는 빈 FString으로 정규화한다.
			outValue.Reset();
			return true;
		}

		if (jsonValue->Type != EJson::String)
		{
			outDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must be string: %s"), *fieldName));
			return false;
		}

		outValue = jsonValue->AsString();
		return true;
	}

	bool TryReadStatusIntField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		TArray<FString>& outDiagnostics,
		int32& outValue)
	{
		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			outDiagnostics.Add(FString::Printf(TEXT("Simulation run status field missing: %s"), *fieldName));
			return false;
		}

		if (jsonValue->Type != EJson::Number)
		{
			outDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must be number: %s"), *fieldName));
			return false;
		}

		outValue = FMath::RoundToInt(jsonValue->AsNumber());
		return true;
	}

	bool TryReadStatusStringArrayField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		TArray<FString>& outDiagnostics,
		TArray<FString>& outValues)
	{
		outValues.Reset();

		const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
		if (!jsonValue.IsValid())
		{
			outDiagnostics.Add(FString::Printf(TEXT("Simulation run status field missing: %s"), *fieldName));
			return false;
		}

		if (jsonValue->Type != EJson::Array)
		{
			outDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must be array: %s"), *fieldName));
			return false;
		}

		for (const TSharedPtr<FJsonValue>& itemValue : jsonValue->AsArray())
		{
			if (!itemValue.IsValid() || itemValue->Type != EJson::String)
			{
				outDiagnostics.Add(FString::Printf(TEXT("Simulation run status array item must be string: %s"), *fieldName));
				return false;
			}

			outValues.Add(itemValue->AsString());
		}

		return true;
	}

	bool TryReadStatusState(
		const FJsonObject& jsonObject,
		TArray<FString>& outDiagnostics,
		ESimulationRunState& outState)
	{
		FString stateString;
		if (!TryReadStatusStringField(jsonObject, TEXT("state"), outDiagnostics, stateString, true))
		{
			return false;
		}

		if (const UEnum* stateEnum = StaticEnum<ESimulationRunState>())
		{
			// Status JSON은 Blueprint enum display가 아니라 C++ enum name string을 public 계약으로 사용한다.
			const int64 stateValue = stateEnum->GetValueByNameString(stateString);
			if (stateValue != INDEX_NONE)
			{
				outState = static_cast<ESimulationRunState>(stateValue);
				return true;
			}
		}

		outDiagnostics.Add(FString::Printf(TEXT("Simulation run status has unknown state: %s"), *stateString));
		return false;
	}
}

FSimulationSetupParseResult FSimulationSetupJson::ParseFromFile(const FString& jsonFilePath)
{
	FSimulationSetupParseResult result;
	if (jsonFilePath.IsEmpty())
	{
		AddSimulationDiagnostic(
			result.Diagnostics,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("empty_simulation_setup_path"),
			TEXT("Simulation setup file path must not be empty."));
		return result;
	}

	const FString resolvedPath = ResolveProjectPath(jsonFilePath);
	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedPath))
	{
		AddSimulationDiagnostic(
			result.Diagnostics,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("simulation_setup_file_missing"),
			FString::Printf(TEXT("Simulation setup JSON read failed: %s"), *resolvedPath));
		return result;
	}

	return ParseFromString(jsonString);
}

FSimulationSetupParseResult FSimulationSetupJson::ParseFromString(const FString& jsonString)
{
	FSimulationSetupParseResult result;

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		AddSimulationDiagnostic(
			result.Diagnostics,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("invalid_simulation_setup_json"),
			TEXT("Simulation setup JSON parse failed."));
		return result;
	}

	ParseSimulationSetupObject(*rootObject, result);
	result.bSuccess = !HasSimulationErrors(result.Diagnostics);
	return result;
}

bool FSimulationSetupJson::TryWriteSetupJson(
	const FSimulationSetup& setup,
	FString& outJson,
	TArray<FString>& outDiagnostics)
{
	outJson.Reset();
	if (!ValidateSimulationSetupForWrite(setup, outDiagnostics))
	{
		return false;
	}

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outJson);
	if (!FJsonSerializer::Serialize(MakeSimulationSetupObject(setup), writer))
	{
		outDiagnostics.Add(TEXT("SimulationSetup JSON serialization failed."));
		return false;
	}

	return true;
}

bool FSimulationSetupJson::SaveToFile(
	const FSimulationSetup& setup,
	const FString& setupFilePath,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (setupFilePath.TrimStartAndEnd().IsEmpty())
	{
		outDiagnostics.Add(TEXT("SimulationSetup output path must not be empty."));
		return false;
	}

	FString setupJson;
	if (!TryWriteSetupJson(setup, setupJson, outDiagnostics))
	{
		return false;
	}

	const FString resolvedSetupFilePath = ResolveProjectPath(setupFilePath);
	const FString outputDirectory = FPaths::GetPath(resolvedSetupFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("SimulationSetup directory create failed: %s"), *outputDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
			setupJson,
			*resolvedSetupFilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("SimulationSetup file write failed: %s"), *resolvedSetupFilePath));
		return false;
	}

	return true;
}

FString FSimulationSetupJson::ResolveProjectPath(const FString& filePath)
{
	if (filePath.IsEmpty() || !FPaths::IsRelative(filePath))
	{
		return filePath;
	}

	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), filePath));
}

FSimulationCommandLineParseResult FSimulationCommandLine::Parse(const FString& commandLine)
{
	FSimulationCommandLineParseResult result;
	bool bHasBareSimulate = false;
	if (TryGetSwitchValue(commandLine, TEXT("Simulate"), result.Options.SimulationSetupFile, bHasBareSimulate))
	{
		result.Options.bSimulate = true;
	}
	else if (bHasBareSimulate)
	{
		AddSimulationDiagnostic(
			result.Diagnostics,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("missing_simulate_value"),
			TEXT("-Simulate requires a simulation setup file path."));
	}

	bool bHasBareRunId = false;
	const bool bHasRunId = TryGetSwitchValue(commandLine, TEXT("RunId"), result.Options.RunId, bHasBareRunId);
	if ((bHasRunId || bHasBareRunId) && result.Options.RunId.IsEmpty())
	{
		AddSimulationDiagnostic(
			result.Diagnostics,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("missing_run_id_value"),
			TEXT("-RunId requires a non-empty value."));
	}

	if (result.Options.bSimulate && result.Options.SimulationSetupFile.IsEmpty())
	{
		AddSimulationDiagnostic(
			result.Diagnostics,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("empty_simulate_value"),
			TEXT("-Simulate value must not be empty."));
	}

	result.bSuccess = !HasSimulationErrors(result.Diagnostics);
	return result;
}

FSimulationCommandLineParseResult FSimulationCommandLine::ParseCurrent()
{
	return Parse(FCommandLine::Get());
}

bool FSimulationRunStatusJson::TryReadStatusJson(
	const FString& jsonString,
	FSimulationRunStatus& outStatus,
	TArray<FString>& outDiagnostics)
{
	outStatus = FSimulationRunStatus{};
	outDiagnostics.Reset();

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outDiagnostics.Add(TEXT("Simulation run status JSON parse failed."));
		return false;
	}

	TryReadStatusStringField(*rootObject, TEXT("schema"), outDiagnostics, outStatus.Schema, true);
	if (!outStatus.Schema.Equals(SimulationRunStatusSchema, ESearchCase::CaseSensitive))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Simulation run status schema must be '%s'."), SimulationRunStatusSchema));
	}

	TryReadStatusIntField(*rootObject, TEXT("version"), outDiagnostics, outStatus.Version);
	TryReadStatusStringField(*rootObject, TEXT("run_id"), outDiagnostics, outStatus.RunId, true);
	TryReadStatusState(*rootObject, outDiagnostics, outStatus.State);
	TryReadStatusStringField(*rootObject, TEXT("setup_path"), outDiagnostics, outStatus.SetupPath, true);
	TryReadStatusStringField(*rootObject, TEXT("updated_at"), outDiagnostics, outStatus.UpdatedAt, true);
	TryReadStatusStringField(*rootObject, TEXT("current_pair_id"), outDiagnostics, outStatus.CurrentPairId, false);
	TryReadStatusIntField(*rootObject, TEXT("completed_runs"), outDiagnostics, outStatus.CompletedRuns);
	TryReadStatusIntField(*rootObject, TEXT("total_runs"), outDiagnostics, outStatus.TotalRuns);
	TryReadStatusStringArrayField(*rootObject, TEXT("report_paths"), outDiagnostics, outStatus.ReportPaths);
	TryReadStatusStringArrayField(*rootObject, TEXT("log_paths"), outDiagnostics, outStatus.LogPaths);
	TryReadStatusStringField(*rootObject, TEXT("error"), outDiagnostics, outStatus.Error, false);

	if (outStatus.Version <= 0)
	{
		outDiagnostics.Add(TEXT("Simulation run status version must be > 0."));
	}

	if (outStatus.CompletedRuns < 0)
	{
		outDiagnostics.Add(TEXT("Simulation run status completed_runs must be >= 0."));
	}

	if (outStatus.TotalRuns < 0)
	{
		outDiagnostics.Add(TEXT("Simulation run status total_runs must be >= 0."));
	}

	return outDiagnostics.IsEmpty();
}

bool FSimulationRunStatusJson::ParseFromFile(
	const FString& statusFilePath,
	FSimulationRunStatus& outStatus,
	TArray<FString>& outDiagnostics)
{
	outStatus = FSimulationRunStatus{};
	outDiagnostics.Reset();

	if (statusFilePath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Simulation run status file path must not be empty."));
		return false;
	}

	const FString resolvedStatusFilePath = FSimulationSetupJson::ResolveProjectPath(statusFilePath);
	FString statusJson;
	if (!FFileHelper::LoadFileToString(statusJson, *resolvedStatusFilePath))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Simulation run status file read failed: %s"), *resolvedStatusFilePath));
		return false;
	}

	return TryReadStatusJson(statusJson, outStatus, outDiagnostics);
}

bool FSimulationRunStatusJson::TryWriteStatusJson(
	const FSimulationRunStatus& status,
	FString& outJson,
	TArray<FString>& outDiagnostics)
{
	outJson.Reset();
	outDiagnostics.Reset();

	if (status.RunId.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Simulation run status requires run_id."));
	}

	if (status.SetupPath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Simulation run status requires setup_path."));
	}

	if (status.UpdatedAt.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Simulation run status requires updated_at."));
	}

	if (!outDiagnostics.IsEmpty())
	{
		return false;
	}

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outJson);
	if (!FJsonSerializer::Serialize(MakeSimulationRunStatusObject(status), writer))
	{
		outDiagnostics.Add(TEXT("Simulation run status JSON serialization failed."));
		return false;
	}

	return true;
}

bool FSimulationRunStatusJson::SaveToFile(
	const FSimulationRunStatus& status,
	const FString& statusFilePath,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (statusFilePath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Simulation run status output path must not be empty."));
		return false;
	}

	FString statusJson;
	if (!TryWriteStatusJson(status, statusJson, outDiagnostics))
	{
		return false;
	}

	const FString resolvedStatusFilePath = FSimulationSetupJson::ResolveProjectPath(statusFilePath);
	const FString outputDirectory = FPaths::GetPath(resolvedStatusFilePath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Simulation run status directory create failed: %s"), *outputDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(statusJson, *resolvedStatusFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Simulation run status file write failed: %s"), *resolvedStatusFilePath));
		return false;
	}

	return true;
}
