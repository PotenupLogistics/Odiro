#include "Shared/SimulationRunStatusTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* SimulationRunStatusSchema = TEXT("simulation_run_status");

	FString ResolveProjectPath(const FString& FilePath)
	{
		if (FilePath.IsEmpty() || !FPaths::IsRelative(FilePath))
		{
			return FilePath;
		}

		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), FilePath));
	}

	FString ToSimulationEnumString(const ESimulationRunState State)
	{
		if (const UEnum* StateEnum = StaticEnum<ESimulationRunState>())
		{
			return StateEnum->GetNameStringByValue(static_cast<int64>(State));
		}

		return TEXT("Unknown");
	}

	void SetOptionalStringField(
		const TSharedRef<FJsonObject>& Object,
		const FString& FieldName,
		const FString& Value)
	{
		if (Value.IsEmpty())
		{
			Object->SetField(FieldName, MakeShared<FJsonValueNull>());
			return;
		}

		Object->SetStringField(FieldName, Value);
	}

	bool TryReadStatusStringField(
		const FJsonObject& Object,
		const FString& FieldName,
		TArray<FString>& OutDiagnostics,
		FString& OutValue,
		const bool bRequired)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid() || Value->IsNull())
		{
			OutValue.Reset();
			if (bRequired)
			{
				OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status missing required field: %s"), *FieldName));
			}
			return !bRequired;
		}

		if (Value->Type != EJson::String)
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must be a string: %s"), *FieldName));
			return false;
		}

		OutValue = Value->AsString();
		if (bRequired && OutValue.IsEmpty())
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must not be empty: %s"), *FieldName));
			return false;
		}

		return true;
	}

	bool TryReadStatusIntField(
		const FJsonObject& Object,
		const FString& FieldName,
		TArray<FString>& OutDiagnostics,
		int32& OutValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid() || Value->Type != EJson::Number)
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must be a number: %s"), *FieldName));
			return false;
		}

		OutValue = static_cast<int32>(Value->AsNumber());
		return true;
	}

	bool TryReadStatusStringArrayField(
		const FJsonObject& Object,
		const FString& FieldName,
		TArray<FString>& OutDiagnostics,
		TArray<FString>& OutValues)
	{
		OutValues.Reset();

		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid() || Value->Type != EJson::Array)
		{
			OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status field must be an array: %s"), *FieldName));
			return false;
		}

		for (const TSharedPtr<FJsonValue>& ItemValue : Value->AsArray())
		{
			if (!ItemValue.IsValid() || ItemValue->Type != EJson::String)
			{
				OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status array contains a non-string value: %s"), *FieldName));
				continue;
			}

			OutValues.Add(ItemValue->AsString());
		}

		return true;
	}

	bool TryReadStatusState(
		const FJsonObject& Object,
		TArray<FString>& OutDiagnostics,
		ESimulationRunState& OutState)
	{
		FString StateString;
		if (!TryReadStatusStringField(Object, TEXT("state"), OutDiagnostics, StateString, true))
		{
			return false;
		}

		if (const UEnum* StateEnum = StaticEnum<ESimulationRunState>())
		{
			// Status JSON stores the C++ enum token, not a localized display label.
			const int64 StateValue = StateEnum->GetValueByNameString(StateString);
			if (StateValue != INDEX_NONE)
			{
				OutState = static_cast<ESimulationRunState>(StateValue);
				return true;
			}
		}

		OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status has unknown state: %s"), *StateString));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> MakeStringArrayField(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}

		return JsonValues;
	}

	FString MakeProjectRelativePathIfPossible(FString FilePath)
	{
		if (FilePath.IsEmpty())
		{
			return FilePath;
		}

		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FString ProjectRelativePath = FilePath;
		if (FPaths::MakePathRelativeTo(ProjectRelativePath, *ProjectDir))
		{
			ProjectRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			return ProjectRelativePath;
		}

		FilePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return FilePath;
	}

	TArray<TSharedPtr<FJsonValue>> MakeStatusReportPathArrayField(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(MakeProjectRelativePathIfPossible(Value)));
		}

		return JsonValues;
	}

	TSharedRef<FJsonObject> MakeSimulationRunStatusObject(const FSimulationRunStatus& Status)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), Status.Schema);
		Object->SetNumberField(TEXT("version"), Status.Version);
		Object->SetStringField(TEXT("run_id"), Status.RunId);
		Object->SetStringField(TEXT("state"), ToSimulationEnumString(Status.State));
		Object->SetStringField(TEXT("setup_path"), Status.SetupPath);
		Object->SetStringField(TEXT("updated_at"), Status.UpdatedAt);
		SetOptionalStringField(Object, TEXT("current_pair_id"), Status.CurrentPairId);
		Object->SetNumberField(TEXT("completed_runs"), Status.CompletedRuns);
		Object->SetNumberField(TEXT("total_runs"), Status.TotalRuns);
		Object->SetArrayField(TEXT("report_paths"), MakeStatusReportPathArrayField(Status.ReportPaths));
		Object->SetArrayField(TEXT("log_paths"), MakeStringArrayField(Status.LogPaths));
		SetOptionalStringField(Object, TEXT("error"), Status.Error);
		return Object;
	}
}

bool FSimulationRunStatusJson::TryReadStatusJson(
	const FString& JsonString,
	FSimulationRunStatus& OutStatus,
	TArray<FString>& OutDiagnostics)
{
	OutStatus = FSimulationRunStatus{};
	OutDiagnostics.Reset();

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutDiagnostics.Add(TEXT("Simulation run status JSON parse failed."));
		return false;
	}

	TryReadStatusStringField(*RootObject, TEXT("schema"), OutDiagnostics, OutStatus.Schema, true);
	if (!OutStatus.Schema.Equals(SimulationRunStatusSchema, ESearchCase::CaseSensitive))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status schema must be '%s'."), SimulationRunStatusSchema));
	}

	TryReadStatusIntField(*RootObject, TEXT("version"), OutDiagnostics, OutStatus.Version);
	TryReadStatusStringField(*RootObject, TEXT("run_id"), OutDiagnostics, OutStatus.RunId, true);
	TryReadStatusState(*RootObject, OutDiagnostics, OutStatus.State);
	TryReadStatusStringField(*RootObject, TEXT("setup_path"), OutDiagnostics, OutStatus.SetupPath, true);
	TryReadStatusStringField(*RootObject, TEXT("updated_at"), OutDiagnostics, OutStatus.UpdatedAt, true);
	TryReadStatusStringField(*RootObject, TEXT("current_pair_id"), OutDiagnostics, OutStatus.CurrentPairId, false);
	TryReadStatusIntField(*RootObject, TEXT("completed_runs"), OutDiagnostics, OutStatus.CompletedRuns);
	TryReadStatusIntField(*RootObject, TEXT("total_runs"), OutDiagnostics, OutStatus.TotalRuns);
	TryReadStatusStringArrayField(*RootObject, TEXT("report_paths"), OutDiagnostics, OutStatus.ReportPaths);
	TryReadStatusStringArrayField(*RootObject, TEXT("log_paths"), OutDiagnostics, OutStatus.LogPaths);
	TryReadStatusStringField(*RootObject, TEXT("error"), OutDiagnostics, OutStatus.Error, false);

	if (OutStatus.Version <= 0)
	{
		OutDiagnostics.Add(TEXT("Simulation run status version must be > 0."));
	}
	if (OutStatus.CompletedRuns < 0)
	{
		OutDiagnostics.Add(TEXT("Simulation run status completed_runs must be >= 0."));
	}
	if (OutStatus.TotalRuns < 0)
	{
		OutDiagnostics.Add(TEXT("Simulation run status total_runs must be >= 0."));
	}

	return OutDiagnostics.IsEmpty();
}

bool FSimulationRunStatusJson::ParseFromFile(
	const FString& StatusFilePath,
	FSimulationRunStatus& OutStatus,
	TArray<FString>& OutDiagnostics)
{
	OutStatus = FSimulationRunStatus{};
	OutDiagnostics.Reset();

	if (StatusFilePath.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Simulation run status file path must not be empty."));
		return false;
	}

	const FString ResolvedStatusFilePath = ResolveProjectPath(StatusFilePath);
	FString StatusJson;
	if (!FFileHelper::LoadFileToString(StatusJson, *ResolvedStatusFilePath))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status file read failed: %s"), *ResolvedStatusFilePath));
		return false;
	}

	return TryReadStatusJson(StatusJson, OutStatus, OutDiagnostics);
}

bool FSimulationRunStatusJson::TryWriteStatusJson(
	const FSimulationRunStatus& Status,
	FString& OutJson,
	TArray<FString>& OutDiagnostics)
{
	OutJson.Reset();
	OutDiagnostics.Reset();

	if (Status.RunId.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Simulation run status requires run_id."));
	}
	if (Status.SetupPath.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Simulation run status requires setup_path."));
	}
	if (Status.UpdatedAt.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Simulation run status requires updated_at."));
	}
	if (!OutDiagnostics.IsEmpty())
	{
		return false;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(MakeSimulationRunStatusObject(Status), Writer))
	{
		OutDiagnostics.Add(TEXT("Simulation run status JSON serialization failed."));
		return false;
	}

	return true;
}

bool FSimulationRunStatusJson::SaveToFile(
	const FSimulationRunStatus& Status,
	const FString& StatusFilePath,
	TArray<FString>& OutDiagnostics)
{
	OutDiagnostics.Reset();

	if (StatusFilePath.IsEmpty())
	{
		OutDiagnostics.Add(TEXT("Simulation run status output path must not be empty."));
		return false;
	}

	FString StatusJson;
	if (!TryWriteStatusJson(Status, StatusJson, OutDiagnostics))
	{
		return false;
	}

	const FString ResolvedStatusFilePath = ResolveProjectPath(StatusFilePath);
	const FString OutputDirectory = FPaths::GetPath(ResolvedStatusFilePath);
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status directory create failed: %s"), *OutputDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(StatusJson, *ResolvedStatusFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutDiagnostics.Add(FString::Printf(TEXT("Simulation run status file write failed: %s"), *ResolvedStatusFilePath));
		return false;
	}

	return true;
}
