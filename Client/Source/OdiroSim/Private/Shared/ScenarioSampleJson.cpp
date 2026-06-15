#include "Shared/ScenarioSampleJson.h"

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
	const TCHAR* ScenarioSampleSchema = TEXT("scenario_sample");

	FString ResolveSampleSchemaPath(const FString& FilePath)
	{
		if (FilePath.IsEmpty() || !FPaths::IsRelative(FilePath))
		{
			return FilePath;
		}

		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), FilePath));
	}

	void AddSampleDiagnostic(
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

	bool SampleHasErrors(const TArray<FScenarioSchemaDiagnostic>& Diagnostics)
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

	bool SampleStringIsEmpty(const FString& Value)
	{
		return Value.TrimStartAndEnd().IsEmpty();
	}

	bool SampleTryReadObjectField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TSharedPtr<FJsonObject>& OutObject,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			if (bRequired)
			{
				AddSampleDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *FieldName),
					FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
					FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			}
			return !bRequired;
		}

		if (Value->Type != EJson::Object)
		{
			AddSampleDiagnostic(
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

	bool SampleTryReadArrayField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<TSharedPtr<FJsonValue>>& OutArray,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			if (bRequired)
			{
				AddSampleDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *FieldName),
					FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
					FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			}
			return !bRequired;
		}

		if (Value->Type != EJson::Array)
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be an array."), *Path, *FieldName));
			return false;
		}

		OutArray = Value->AsArray();
		return true;
	}

	bool SampleTryReadRequiredStringField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FString& OutValue)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			return false;
		}

		if (Value->Type != EJson::String)
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *Path, *FieldName));
			return false;
		}

		OutValue = Value->AsString();
		if (SampleStringIsEmpty(OutValue))
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("empty_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must not be empty."), *Path, *FieldName));
			return false;
		}

		return true;
	}

	bool SampleTryReadOptionalStringField(
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
			AddSampleDiagnostic(
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

	bool SampleTryReadNumberField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		double& OutValue,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			if (bRequired)
			{
				AddSampleDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *FieldName),
					FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
					FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			}
			return !bRequired;
		}

		if (Value->Type != EJson::Number)
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a number."), *Path, *FieldName));
			return false;
		}

		OutValue = Value->AsNumber();
		return true;
	}

	bool SampleTryReadInt64Field(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		int64& OutValue,
		bool bRequired)
	{
		double Number = 0.0;
		if (!SampleTryReadNumberField(Object, FieldName, Path, Diagnostics, Number, bRequired))
		{
			return false;
		}
		if (!Object.TryGetField(FieldName).IsValid() && !bRequired)
		{
			return true;
		}

		const int64 RoundedNumber = static_cast<int64>(Number);
		if (!FMath::IsNearlyEqual(Number, static_cast<double>(RoundedNumber)))
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be an integer."), *Path, *FieldName));
			return false;
		}

		OutValue = RoundedNumber;
		return true;
	}

	bool SampleTryReadBoolField(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		bool& OutValue,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(FieldName);
		if (!Value.IsValid())
		{
			if (bRequired)
			{
				AddSampleDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *FieldName),
					FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
					FString::Printf(TEXT("%s.%s is required."), *Path, *FieldName));
			}
			return !bRequired;
		}

		if (Value->Type != EJson::Boolean)
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must be a boolean."), *Path, *FieldName));
			return false;
		}

		OutValue = Value->AsBool();
		return true;
	}

	bool SampleTryReadRequiredVersion(
		const FJsonObject& Object,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		int32& OutVersion)
	{
		const TSharedPtr<FJsonValue> Value = Object.TryGetField(TEXT("version"));
		if (!Value.IsValid())
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("missing_version"), TEXT("$.version"), TEXT("$.version is required."));
			return false;
		}

		if (Value->Type != EJson::Number)
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_version"), TEXT("$.version"), TEXT("$.version must be a number."));
			return false;
		}

		const double VersionNumber = Value->AsNumber();
		OutVersion = FMath::RoundToInt(VersionNumber);
		if (!FMath::IsNearlyEqual(VersionNumber, static_cast<double>(OutVersion)))
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_version"), TEXT("$.version"), TEXT("$.version must be an integer."));
			return false;
		}

		if (OutVersion != FScenarioSampleJson::SupportedVersion)
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("unsupported_schema_version"),
				TEXT("$.version"),
				FString::Printf(
					TEXT("scenario_sample version must match the current compiler and validator version: %d."),
					FScenarioSampleJson::SupportedVersion));
			return false;
		}

		return true;
	}

	bool SampleTryReadNumberArray2(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		double& OutA,
		double& OutB,
		bool bRequired)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		if (!SampleTryReadArrayField(Object, FieldName, Path, Diagnostics, Values, bRequired))
		{
			return false;
		}
		if (Values.IsEmpty() && !bRequired)
		{
			return true;
		}
		if (Values.Num() != 2
			|| !Values[0].IsValid()
			|| Values[0]->Type != EJson::Number
			|| !Values[1].IsValid()
			|| Values[1]->Type != EJson::Number)
		{
			AddSampleDiagnostic(
				Diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *FieldName),
				FString::Printf(TEXT("%s.%s"), *Path, *FieldName),
				FString::Printf(TEXT("%s.%s must contain exactly two numbers."), *Path, *FieldName));
			return false;
		}

		OutA = Values[0]->AsNumber();
		OutB = Values[1]->AsNumber();
		return true;
	}

	bool SampleTryReadAlongRange(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioAlongRangeMeters& OutRange,
		bool bRequired)
	{
		return SampleTryReadNumberArray2(
			Object,
			FieldName,
			Path,
			Diagnostics,
			OutRange.StartMeters,
			OutRange.EndMeters,
			bRequired);
	}

	bool SampleTryReadOffsetRange(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioOffsetRangeMeters& OutRange,
		bool bRequired)
	{
		return SampleTryReadNumberArray2(
			Object,
			FieldName,
			Path,
			Diagnostics,
			OutRange.MinMeters,
			OutRange.MaxMeters,
			bRequired);
	}

	bool SampleTryReadVector2D(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FVector2D& OutPoint,
		bool bRequired)
	{
		double X = 0.0;
		double Y = 0.0;
		if (!SampleTryReadNumberArray2(Object, FieldName, Path, Diagnostics, X, Y, bRequired))
		{
			return false;
		}

		OutPoint = FVector2D(X, Y);
		return true;
	}

	bool SampleTryReadVector2DArray(
		const FJsonObject& Object,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FVector2D>& OutPoints,
		bool bRequired)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		if (!SampleTryReadArrayField(Object, FieldName, Path, Diagnostics, Values, bRequired))
		{
			return false;
		}

		OutPoints.Reset();
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& PointValue = Values[Index];
			if (!PointValue.IsValid() || PointValue->Type != EJson::Array)
			{
				AddSampleDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("invalid_%s"), *FieldName),
					FString::Printf(TEXT("%s.%s[%d]"), *Path, *FieldName, Index),
					FString::Printf(TEXT("%s.%s[%d] must be a two-number array."), *Path, *FieldName, Index));
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>> Point = PointValue->AsArray();
			if (Point.Num() != 2
				|| !Point[0].IsValid()
				|| Point[0]->Type != EJson::Number
				|| !Point[1].IsValid()
				|| Point[1]->Type != EJson::Number)
			{
				AddSampleDiagnostic(
					Diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("invalid_%s"), *FieldName),
					FString::Printf(TEXT("%s.%s[%d]"), *Path, *FieldName, Index),
					FString::Printf(TEXT("%s.%s[%d] must contain exactly two numbers."), *Path, *FieldName, Index));
				return false;
			}

			OutPoints.Add(FVector2D(Point[0]->AsNumber(), Point[1]->AsNumber()));
		}

		return true;
	}

	bool SampleParseLaneType(const FString& Value, EScenarioSampleLaneType& OutType)
	{
		const FString Normalized = Value.ToLower();
		if (Normalized == TEXT("walkable")) { OutType = EScenarioSampleLaneType::Walkable; return true; }
		if (Normalized == TEXT("penalty")) { OutType = EScenarioSampleLaneType::Penalty; return true; }
		if (Normalized == TEXT("blocked")) { OutType = EScenarioSampleLaneType::Blocked; return true; }
		return false;
	}

	bool SampleParseObstacleClass(const FString& Value, EScenarioSampleObstacleClass& OutClass)
	{
		const FString Normalized = Value.ToLower();
		if (Normalized == TEXT("blocking")) { OutClass = EScenarioSampleObstacleClass::Blocking; return true; }
		if (Normalized == TEXT("traversable_cost")) { OutClass = EScenarioSampleObstacleClass::TraversableCost; return true; }
		return false;
	}

	bool SampleParsePedestrianRole(const FString& Value, EScenarioSamplePedestrianRole& OutRole)
	{
		const FString Normalized = Value.ToLower();
		if (Normalized == TEXT("encounter")) { OutRole = EScenarioSamplePedestrianRole::Encounter; return true; }
		if (Normalized == TEXT("background")) { OutRole = EScenarioSamplePedestrianRole::Background; return true; }
		return false;
	}

	bool SampleParseEncounterType(const FString& Value, EScenarioTemplateEncounterType& OutType)
	{
		const FString Normalized = Value.ToLower();
		if (Normalized == TEXT("oncoming_pass")) { OutType = EScenarioTemplateEncounterType::OncomingPass; return true; }
		if (Normalized == TEXT("overtake")) { OutType = EScenarioTemplateEncounterType::Overtake; return true; }
		if (Normalized == TEXT("cross_path")) { OutType = EScenarioTemplateEncounterType::CrossPath; return true; }
		if (Normalized == TEXT("standing_group")) { OutType = EScenarioTemplateEncounterType::StandingGroup; return true; }
		return false;
	}

	bool SampleParseAnchorType(const FString& Value, EScenarioTemplateRobotAnchorType& OutType)
	{
		const FString Normalized = Value.ToLower();
		if (Normalized == TEXT("entry")) { OutType = EScenarioTemplateRobotAnchorType::Entry; return true; }
		if (Normalized == TEXT("exit")) { OutType = EScenarioTemplateRobotAnchorType::Exit; return true; }
		if (Normalized == TEXT("corridor_pose")) { OutType = EScenarioTemplateRobotAnchorType::CorridorPose; return true; }
		return false;
	}

	bool SampleParseDiagnosticSeverity(const FString& Value, EScenarioSchemaDiagnosticSeverity& OutSeverity)
	{
		const FString Normalized = Value.ToLower();
		if (Normalized == TEXT("info")) { OutSeverity = EScenarioSchemaDiagnosticSeverity::Info; return true; }
		if (Normalized == TEXT("warning")) { OutSeverity = EScenarioSchemaDiagnosticSeverity::Warning; return true; }
		if (Normalized == TEXT("repair")) { OutSeverity = EScenarioSchemaDiagnosticSeverity::Repair; return true; }
		if (Normalized == TEXT("error")) { OutSeverity = EScenarioSchemaDiagnosticSeverity::Error; return true; }
		return false;
	}

	FString SampleLaneTypeToString(EScenarioSampleLaneType Value)
	{
		switch (Value)
		{
		case EScenarioSampleLaneType::Walkable: return TEXT("walkable");
		case EScenarioSampleLaneType::Penalty: return TEXT("penalty");
		case EScenarioSampleLaneType::Blocked: return TEXT("blocked");
		}
		return TEXT("walkable");
	}

	FString SampleObstacleClassToString(EScenarioSampleObstacleClass Value)
	{
		switch (Value)
		{
		case EScenarioSampleObstacleClass::Blocking: return TEXT("blocking");
		case EScenarioSampleObstacleClass::TraversableCost: return TEXT("traversable_cost");
		}
		return TEXT("blocking");
	}

	FString SamplePedestrianRoleToString(EScenarioSamplePedestrianRole Value)
	{
		switch (Value)
		{
		case EScenarioSamplePedestrianRole::Encounter: return TEXT("encounter");
		case EScenarioSamplePedestrianRole::Background: return TEXT("background");
		}
		return TEXT("background");
	}

	FString SampleEncounterTypeToString(EScenarioTemplateEncounterType Value)
	{
		switch (Value)
		{
		case EScenarioTemplateEncounterType::OncomingPass: return TEXT("oncoming_pass");
		case EScenarioTemplateEncounterType::Overtake: return TEXT("overtake");
		case EScenarioTemplateEncounterType::CrossPath: return TEXT("cross_path");
		case EScenarioTemplateEncounterType::StandingGroup: return TEXT("standing_group");
		}
		return TEXT("oncoming_pass");
	}

	FString SampleAnchorTypeToString(EScenarioTemplateRobotAnchorType Value)
	{
		switch (Value)
		{
		case EScenarioTemplateRobotAnchorType::Entry: return TEXT("entry");
		case EScenarioTemplateRobotAnchorType::Exit: return TEXT("exit");
		case EScenarioTemplateRobotAnchorType::CorridorPose: return TEXT("corridor_pose");
		}
		return TEXT("entry");
	}

	FString SampleDiagnosticSeverityToString(EScenarioSchemaDiagnosticSeverity Value)
	{
		switch (Value)
		{
		case EScenarioSchemaDiagnosticSeverity::Info: return TEXT("info");
		case EScenarioSchemaDiagnosticSeverity::Warning: return TEXT("warning");
		case EScenarioSchemaDiagnosticSeverity::Repair: return TEXT("repair");
		case EScenarioSchemaDiagnosticSeverity::Error: return TEXT("error");
		}
		return TEXT("info");
	}

	void SampleParseSource(
		const FJsonObject& SampleObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleSource& OutSource)
	{
		TSharedPtr<FJsonObject> SourceObject;
		if (!SampleTryReadObjectField(SampleObject, TEXT("source"), TEXT("$.sample"), Diagnostics, SourceObject, true))
		{
			return;
		}

		SampleTryReadRequiredStringField(*SourceObject, TEXT("template_ref"), TEXT("$.sample.source"), Diagnostics, OutSource.TemplateRef);
		SampleTryReadRequiredStringField(*SourceObject, TEXT("template_hash"), TEXT("$.sample.source"), Diagnostics, OutSource.TemplateHash);
		SampleTryReadRequiredStringField(*SourceObject, TEXT("profile_ref"), TEXT("$.sample.source"), Diagnostics, OutSource.ProfileRef);
		SampleTryReadRequiredStringField(*SourceObject, TEXT("profile_hash"), TEXT("$.sample.source"), Diagnostics, OutSource.ProfileHash);
		SampleTryReadRequiredStringField(*SourceObject, TEXT("setting_ref"), TEXT("$.sample.source"), Diagnostics, OutSource.SettingRef);
		SampleTryReadRequiredStringField(*SourceObject, TEXT("setting_hash"), TEXT("$.sample.source"), Diagnostics, OutSource.SettingHash);
		SampleTryReadInt64Field(*SourceObject, TEXT("seed"), TEXT("$.sample.source"), Diagnostics, OutSource.Seed, true);
		SampleTryReadRequiredStringField(*SourceObject, TEXT("generator_version"), TEXT("$.sample.source"), Diagnostics, OutSource.GeneratorVersion);
	}

	void SampleParseIdentity(
		const FJsonObject& RootObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleIdentity& OutIdentity)
	{
		TSharedPtr<FJsonObject> SampleObject;
		if (!SampleTryReadObjectField(RootObject, TEXT("sample"), TEXT("$"), Diagnostics, SampleObject, true))
		{
			return;
		}

		SampleTryReadRequiredStringField(*SampleObject, TEXT("sample_id"), TEXT("$.sample"), Diagnostics, OutIdentity.SampleId);
		SampleTryReadRequiredStringField(*SampleObject, TEXT("scenario_id"), TEXT("$.sample"), Diagnostics, OutIdentity.ScenarioId);
		SampleParseSource(*SampleObject, Diagnostics, OutIdentity.Source);
	}

	bool SampleParseParamValue(
		const TSharedPtr<FJsonValue>& Value,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleParamValue& OutParam)
	{
		OutParam = FScenarioSampleParamValue{};
		if (!Value.IsValid())
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_param"), Path, FString::Printf(TEXT("%s must not be null."), *Path));
			return false;
		}

		if (Value->Type == EJson::Boolean)
		{
			OutParam.Type = EScenarioSampleParamValueType::Boolean;
			OutParam.BoolValue = Value->AsBool();
			return true;
		}
		if (Value->Type == EJson::Number)
		{
			const double Number = Value->AsNumber();
			const int32 RoundedNumber = FMath::RoundToInt(Number);
			if (FMath::IsNearlyEqual(Number, static_cast<double>(RoundedNumber)))
			{
				OutParam.Type = EScenarioSampleParamValueType::Integer;
				OutParam.IntegerValue = RoundedNumber;
			}
			else
			{
				OutParam.Type = EScenarioSampleParamValueType::Float;
				OutParam.FloatValue = Number;
			}
			return true;
		}
		if (Value->Type == EJson::String)
		{
			OutParam.Type = EScenarioSampleParamValueType::String;
			OutParam.StringValue = Value->AsString();
			return true;
		}
		if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>> ArrayValues = Value->AsArray();
			bool bAllNumbers = true;
			bool bAllStrings = true;
			for (const TSharedPtr<FJsonValue>& ArrayValue : ArrayValues)
			{
				bAllNumbers = bAllNumbers && ArrayValue.IsValid() && ArrayValue->Type == EJson::Number;
				bAllStrings = bAllStrings && ArrayValue.IsValid() && ArrayValue->Type == EJson::String;
			}

			if (bAllNumbers)
			{
				OutParam.Type = EScenarioSampleParamValueType::FloatArray;
				for (const TSharedPtr<FJsonValue>& ArrayValue : ArrayValues)
				{
					OutParam.FloatArrayValue.Add(ArrayValue->AsNumber());
				}
				return true;
			}
			if (bAllStrings)
			{
				OutParam.Type = EScenarioSampleParamValueType::StringArray;
				for (const TSharedPtr<FJsonValue>& ArrayValue : ArrayValues)
				{
					OutParam.StringArrayValue.Add(ArrayValue->AsString());
				}
				return true;
			}
		}

		AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_param"), Path, FString::Printf(TEXT("%s must be a scalar or homogeneous scalar array."), *Path));
		return false;
	}

	void SampleParseParams(
		const FJsonObject& ScenarioObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TMap<FString, FScenarioSampleParamValue>& OutParams)
	{
		TSharedPtr<FJsonObject> ParamsObject;
		if (!SampleTryReadObjectField(ScenarioObject, TEXT("params"), TEXT("$.scenario"), Diagnostics, ParamsObject, true))
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ParamsObject->Values)
		{
			FScenarioSampleParamValue Param;
			if (SampleParseParamValue(Pair.Value, FString::Printf(TEXT("$.scenario.params.%s"), *Pair.Key), Diagnostics, Param))
			{
				OutParams.Add(Pair.Key, Param);
			}
		}
	}

	void SampleParseRobotPose(
		const FJsonObject& PoseObject,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleRobotPose& OutPose)
	{
		SampleTryReadRequiredStringField(PoseObject, TEXT("segment"), Path, Diagnostics, OutPose.SegmentId);
		SampleTryReadNumberField(PoseObject, TEXT("along_m"), Path, Diagnostics, OutPose.AlongMeters, true);
		SampleTryReadNumberField(PoseObject, TEXT("offset_m"), Path, Diagnostics, OutPose.OffsetMeters, true);
		SampleTryReadOptionalStringField(PoseObject, TEXT("lane"), Path, Diagnostics, OutPose.LaneId);
		SampleTryReadNumberField(PoseObject, TEXT("heading_deg"), Path, Diagnostics, OutPose.HeadingDegrees, false);

		FString AnchorTypeString;
		if (SampleTryReadRequiredStringField(PoseObject, TEXT("source_anchor_type"), Path, Diagnostics, AnchorTypeString)
			&& !SampleParseAnchorType(AnchorTypeString, OutPose.SourceAnchorType))
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_source_anchor_type"), FString::Printf(TEXT("%s.source_anchor_type"), *Path), FString::Printf(TEXT("%s.source_anchor_type has unsupported value '%s'."), *Path, *AnchorTypeString));
		}
	}

	void SampleParseRouteAxis(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleRouteAxis& OutRouteAxis)
	{
		TSharedPtr<FJsonObject> AxisObject;
		if (!SampleTryReadObjectField(SemanticObject, TEXT("route_axis"), TEXT("$.scenario.semantic"), Diagnostics, AxisObject, true))
		{
			return;
		}

		FString TypeString;
		if (SampleTryReadRequiredStringField(*AxisObject, TEXT("type"), TEXT("$.scenario.semantic.route_axis"), Diagnostics, TypeString)
			&& !TypeString.Equals(TEXT("polyline"), ESearchCase::IgnoreCase))
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_axis_type"), TEXT("$.scenario.semantic.route_axis.type"), TEXT("$.scenario.semantic.route_axis.type must be 'polyline'."));
		}

		SampleTryReadVector2D(*AxisObject, TEXT("origin_xy_m"), TEXT("$.scenario.semantic.route_axis"), Diagnostics, OutRouteAxis.OriginXYMeters, true);
		SampleTryReadNumberField(*AxisObject, TEXT("heading_deg"), TEXT("$.scenario.semantic.route_axis"), Diagnostics, OutRouteAxis.HeadingDegrees, true);
		SampleTryReadVector2DArray(*AxisObject, TEXT("points_m"), TEXT("$.scenario.semantic.route_axis"), Diagnostics, OutRouteAxis.PointsMeters, true);
		SampleTryReadNumberField(*AxisObject, TEXT("length_m"), TEXT("$.scenario.semantic.route_axis"), Diagnostics, OutRouteAxis.LengthMeters, true);
	}

	void SampleParseRobot(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleRobotSemantic& OutRobot)
	{
		TSharedPtr<FJsonObject> RobotObject;
		if (!SampleTryReadObjectField(SemanticObject, TEXT("robot"), TEXT("$.scenario.semantic"), Diagnostics, RobotObject, true))
		{
			return;
		}

		TSharedPtr<FJsonObject> StartObject;
		if (SampleTryReadObjectField(*RobotObject, TEXT("start"), TEXT("$.scenario.semantic.robot"), Diagnostics, StartObject, true) && StartObject.IsValid())
		{
			SampleParseRobotPose(*StartObject, TEXT("$.scenario.semantic.robot.start"), Diagnostics, OutRobot.Start);
		}

		TSharedPtr<FJsonObject> GoalObject;
		if (SampleTryReadObjectField(*RobotObject, TEXT("goal"), TEXT("$.scenario.semantic.robot"), Diagnostics, GoalObject, true) && GoalObject.IsValid())
		{
			SampleParseRobotPose(*GoalObject, TEXT("$.scenario.semantic.robot.goal"), Diagnostics, OutRobot.Goal);
		}
	}

	void SampleParseLayout(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FScenarioSampleLayoutEntry>& OutLayout)
	{
		TArray<TSharedPtr<FJsonValue>> LayoutValues;
		if (!SampleTryReadArrayField(SemanticObject, TEXT("layout"), TEXT("$.scenario.semantic"), Diagnostics, LayoutValues, true))
		{
			return;
		}

		for (int32 Index = 0; Index < LayoutValues.Num(); ++Index)
		{
			const FString EntryPath = FString::Printf(TEXT("$.scenario.semantic.layout[%d]"), Index);
			if (!LayoutValues[Index].IsValid() || LayoutValues[Index]->Type != EJson::Object)
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_layout_entry"), EntryPath, FString::Printf(TEXT("%s must be an object."), *EntryPath));
				continue;
			}

			const TSharedPtr<FJsonObject> EntryObject = LayoutValues[Index]->AsObject();
			FScenarioSampleLayoutEntry Entry;
			SampleTryReadAlongRange(*EntryObject, TEXT("along_range_m"), EntryPath, Diagnostics, Entry.AlongRangeMeters, true);
			SampleTryReadRequiredStringField(*EntryObject, TEXT("segment"), EntryPath, Diagnostics, Entry.SegmentId);

			TArray<TSharedPtr<FJsonValue>> LaneValues;
			if (SampleTryReadArrayField(*EntryObject, TEXT("lanes"), EntryPath, Diagnostics, LaneValues, true))
			{
				for (int32 LaneIndex = 0; LaneIndex < LaneValues.Num(); ++LaneIndex)
				{
					const FString LanePath = FString::Printf(TEXT("%s.lanes[%d]"), *EntryPath, LaneIndex);
					if (!LaneValues[LaneIndex].IsValid() || LaneValues[LaneIndex]->Type != EJson::Object)
					{
						AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_layout_lane"), LanePath, FString::Printf(TEXT("%s must be an object."), *LanePath));
						continue;
					}

					const TSharedPtr<FJsonObject> LaneObject = LaneValues[LaneIndex]->AsObject();
					FScenarioSampleLayoutLane Lane;
					SampleTryReadRequiredStringField(*LaneObject, TEXT("lane"), LanePath, Diagnostics, Lane.LaneId);
					SampleTryReadOffsetRange(*LaneObject, TEXT("offset_range_m"), LanePath, Diagnostics, Lane.OffsetRangeMeters, true);
					SampleTryReadRequiredStringField(*LaneObject, TEXT("surface"), LanePath, Diagnostics, Lane.SurfaceId);
					FString LaneTypeString;
					if (SampleTryReadRequiredStringField(*LaneObject, TEXT("type"), LanePath, Diagnostics, LaneTypeString)
						&& !SampleParseLaneType(LaneTypeString, Lane.Type))
					{
						AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_lane_type"), FString::Printf(TEXT("%s.type"), *LanePath), FString::Printf(TEXT("%s.type has unsupported value '%s'."), *LanePath, *LaneTypeString));
					}
					Entry.Lanes.Add(Lane);
				}
			}

			OutLayout.Add(Entry);
		}
	}

	void SampleParseStaticObstacles(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FScenarioSampleStaticObstacle>& OutObstacles)
	{
		TArray<TSharedPtr<FJsonValue>> ObstacleValues;
		if (!SampleTryReadArrayField(SemanticObject, TEXT("static_obstacles"), TEXT("$.scenario.semantic"), Diagnostics, ObstacleValues, true))
		{
			return;
		}

		for (int32 Index = 0; Index < ObstacleValues.Num(); ++Index)
		{
			const FString ObstaclePath = FString::Printf(TEXT("$.scenario.semantic.static_obstacles[%d]"), Index);
			if (!ObstacleValues[Index].IsValid() || ObstacleValues[Index]->Type != EJson::Object)
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_static_obstacle"), ObstaclePath, FString::Printf(TEXT("%s must be an object."), *ObstaclePath));
				continue;
			}

			const TSharedPtr<FJsonObject> ObstacleObject = ObstacleValues[Index]->AsObject();
			FScenarioSampleStaticObstacle Obstacle;
			SampleTryReadRequiredStringField(*ObstacleObject, TEXT("id"), ObstaclePath, Diagnostics, Obstacle.ObstacleId);
			SampleTryReadRequiredStringField(*ObstacleObject, TEXT("prop"), ObstaclePath, Diagnostics, Obstacle.PropId);
			SampleTryReadOptionalStringField(*ObstacleObject, TEXT("perception_tag"), ObstaclePath, Diagnostics, Obstacle.PerceptionTag);

			FString ClassString;
			if (SampleTryReadRequiredStringField(*ObstacleObject, TEXT("class"), ObstaclePath, Diagnostics, ClassString)
				&& !SampleParseObstacleClass(ClassString, Obstacle.ObstacleClass))
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_obstacle_class"), FString::Printf(TEXT("%s.class"), *ObstaclePath), FString::Printf(TEXT("%s.class has unsupported value '%s'."), *ObstaclePath, *ClassString));
			}

			SampleTryReadOptionalStringField(*ObstacleObject, TEXT("sensor_profile"), ObstaclePath, Diagnostics, Obstacle.SensorProfile);
			SampleTryReadNumberField(*ObstacleObject, TEXT("along_m"), ObstaclePath, Diagnostics, Obstacle.AlongMeters, true);
			SampleTryReadNumberField(*ObstacleObject, TEXT("offset_m"), ObstaclePath, Diagnostics, Obstacle.OffsetMeters, true);
			SampleTryReadNumberField(*ObstacleObject, TEXT("yaw_deg"), ObstaclePath, Diagnostics, Obstacle.YawDegrees, true);
			SampleTryReadVector2D(*ObstacleObject, TEXT("footprint_m"), ObstaclePath, Diagnostics, Obstacle.FootprintMeters, true);
			SampleTryReadRequiredStringField(*ObstacleObject, TEXT("placed_by"), ObstaclePath, Diagnostics, Obstacle.PlacedBy);
			SampleTryReadNumberField(*ObstacleObject, TEXT("clear_width_remaining_m"), ObstaclePath, Diagnostics, Obstacle.ClearWidthRemainingMeters, true);
			OutObstacles.Add(Obstacle);
		}
	}

	void SampleParseBehavior(
		const FJsonObject& PedestrianObject,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSamplePedestrianBehavior& OutBehavior)
	{
		TSharedPtr<FJsonObject> BehaviorObject;
		if (!SampleTryReadObjectField(PedestrianObject, TEXT("behavior"), Path, Diagnostics, BehaviorObject, true))
		{
			return;
		}

		SampleTryReadNumberField(*BehaviorObject, TEXT("cooperation"), FString::Printf(TEXT("%s.behavior"), *Path), Diagnostics, OutBehavior.Cooperation, true);
		SampleTryReadNumberField(*BehaviorObject, TEXT("evasiveness"), FString::Printf(TEXT("%s.behavior"), *Path), Diagnostics, OutBehavior.Evasiveness, true);
		SampleTryReadNumberField(*BehaviorObject, TEXT("personal_space_m"), FString::Printf(TEXT("%s.behavior"), *Path), Diagnostics, OutBehavior.PersonalSpaceMeters, true);
		SampleTryReadNumberField(*BehaviorObject, TEXT("awareness_horizon_s"), FString::Printf(TEXT("%s.behavior"), *Path), Diagnostics, OutBehavior.AwarenessHorizonSeconds, true);
		SampleTryReadNumberField(*BehaviorObject, TEXT("max_yield_wait_s"), FString::Printf(TEXT("%s.behavior"), *Path), Diagnostics, OutBehavior.MaxYieldWaitSeconds, true);
		SampleTryReadNumberField(*BehaviorObject, TEXT("sidestep_distance_m"), FString::Printf(TEXT("%s.behavior"), *Path), Diagnostics, OutBehavior.SidestepDistanceMeters, true);
	}

	void SampleParseBaselinePose(
		const FJsonObject& BaselineObject,
		const FString& FieldName,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FString& OutSegmentId,
		double& OutAlongMeters,
		double& OutOffsetMeters)
	{
		TSharedPtr<FJsonObject> PoseObject;
		if (!SampleTryReadObjectField(BaselineObject, FieldName, Path, Diagnostics, PoseObject, true))
		{
			return;
		}

		const FString PosePath = FString::Printf(TEXT("%s.%s"), *Path, *FieldName);
		SampleTryReadRequiredStringField(*PoseObject, TEXT("segment"), PosePath, Diagnostics, OutSegmentId);
		SampleTryReadNumberField(*PoseObject, TEXT("along_m"), PosePath, Diagnostics, OutAlongMeters, true);
		SampleTryReadNumberField(*PoseObject, TEXT("offset_m"), PosePath, Diagnostics, OutOffsetMeters, true);
	}

	void SampleParseBaseline(
		const FJsonObject& PedestrianObject,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSamplePedestrianBaseline& OutBaseline)
	{
		TSharedPtr<FJsonObject> BaselineObject;
		if (!SampleTryReadObjectField(PedestrianObject, TEXT("baseline"), Path, Diagnostics, BaselineObject, true))
		{
			return;
		}

		const FString BaselinePath = FString::Printf(TEXT("%s.baseline"), *Path);
		SampleParseBaselinePose(*BaselineObject, TEXT("start"), BaselinePath, Diagnostics, OutBaseline.StartSegmentId, OutBaseline.StartAlongMeters, OutBaseline.StartOffsetMeters);
		SampleParseBaselinePose(*BaselineObject, TEXT("goal"), BaselinePath, Diagnostics, OutBaseline.GoalSegmentId, OutBaseline.GoalAlongMeters, OutBaseline.GoalOffsetMeters);
		SampleTryReadVector2DArray(*BaselineObject, TEXT("points_m"), BaselinePath, Diagnostics, OutBaseline.PointsMeters, false);
	}

	void SampleParsePedestrians(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FScenarioSamplePedestrian>& OutPedestrians)
	{
		TArray<TSharedPtr<FJsonValue>> PedestrianValues;
		if (!SampleTryReadArrayField(SemanticObject, TEXT("pedestrians"), TEXT("$.scenario.semantic"), Diagnostics, PedestrianValues, true))
		{
			return;
		}

		for (int32 Index = 0; Index < PedestrianValues.Num(); ++Index)
		{
			const FString PedestrianPath = FString::Printf(TEXT("$.scenario.semantic.pedestrians[%d]"), Index);
			if (!PedestrianValues[Index].IsValid() || PedestrianValues[Index]->Type != EJson::Object)
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_pedestrian"), PedestrianPath, FString::Printf(TEXT("%s must be an object."), *PedestrianPath));
				continue;
			}

			const TSharedPtr<FJsonObject> PedestrianObject = PedestrianValues[Index]->AsObject();
			FScenarioSamplePedestrian Pedestrian;
			SampleTryReadRequiredStringField(*PedestrianObject, TEXT("id"), PedestrianPath, Diagnostics, Pedestrian.PedestrianId);
			FString RoleString;
			if (SampleTryReadRequiredStringField(*PedestrianObject, TEXT("role"), PedestrianPath, Diagnostics, RoleString)
				&& !SampleParsePedestrianRole(RoleString, Pedestrian.Role))
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_pedestrian_role"), FString::Printf(TEXT("%s.role"), *PedestrianPath), FString::Printf(TEXT("%s.role has unsupported value '%s'."), *PedestrianPath, *RoleString));
			}
			SampleTryReadOptionalStringField(*PedestrianObject, TEXT("placed_by"), PedestrianPath, Diagnostics, Pedestrian.PlacedBy);
			FString EncounterTypeString;
			if (SampleTryReadOptionalStringField(*PedestrianObject, TEXT("type"), PedestrianPath, Diagnostics, EncounterTypeString)
				&& !EncounterTypeString.IsEmpty()
				&& !SampleParseEncounterType(EncounterTypeString, Pedestrian.EncounterType))
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_encounter_type"), FString::Printf(TEXT("%s.type"), *PedestrianPath), FString::Printf(TEXT("%s.type has unsupported value '%s'."), *PedestrianPath, *EncounterTypeString));
			}
			SampleTryReadOptionalStringField(*PedestrianObject, TEXT("persona"), PedestrianPath, Diagnostics, Pedestrian.PersonaId);
			SampleParseBehavior(*PedestrianObject, PedestrianPath, Diagnostics, Pedestrian.Behavior);
			SampleTryReadNumberField(*PedestrianObject, TEXT("speed_mps"), PedestrianPath, Diagnostics, Pedestrian.SpeedMetersPerSecond, true);
			SampleParseBaseline(*PedestrianObject, PedestrianPath, Diagnostics, Pedestrian.Baseline);
			SampleTryReadOptionalStringField(*PedestrianObject, TEXT("pedestrian_scenario_hash"), PedestrianPath, Diagnostics, Pedestrian.PedestrianScenarioHash);
			OutPedestrians.Add(Pedestrian);
		}
	}

	void SampleParseClearWidthProfile(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FScenarioSampleClearWidthEntry>& OutProfile)
	{
		TArray<TSharedPtr<FJsonValue>> ProfileValues;
		if (!SampleTryReadArrayField(SemanticObject, TEXT("clear_width_profile"), TEXT("$.scenario.semantic"), Diagnostics, ProfileValues, true))
		{
			return;
		}

		for (int32 Index = 0; Index < ProfileValues.Num(); ++Index)
		{
			const FString EntryPath = FString::Printf(TEXT("$.scenario.semantic.clear_width_profile[%d]"), Index);
			if (!ProfileValues[Index].IsValid() || ProfileValues[Index]->Type != EJson::Object)
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_clear_width_entry"), EntryPath, FString::Printf(TEXT("%s must be an object."), *EntryPath));
				continue;
			}

			const TSharedPtr<FJsonObject> EntryObject = ProfileValues[Index]->AsObject();
			FScenarioSampleClearWidthEntry Entry;
			SampleTryReadAlongRange(*EntryObject, TEXT("along_range_m"), EntryPath, Diagnostics, Entry.AlongRangeMeters, true);
			SampleTryReadNumberField(*EntryObject, TEXT("clear_width_m"), EntryPath, Diagnostics, Entry.ClearWidthMeters, true);
			SampleTryReadOptionalStringField(*EntryObject, TEXT("limited_by"), EntryPath, Diagnostics, Entry.LimitedBy);
			OutProfile.Add(Entry);
		}
	}

	void SampleParseSummary(
		const FJsonObject& SemanticObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleSummary& OutSummary)
	{
		TSharedPtr<FJsonObject> SummaryObject;
		if (!SampleTryReadObjectField(SemanticObject, TEXT("summary"), TEXT("$.scenario.semantic"), Diagnostics, SummaryObject, true))
		{
			return;
		}

		SampleTryReadNumberField(*SummaryObject, TEXT("global_min_clear_width_m"), TEXT("$.scenario.semantic.summary"), Diagnostics, OutSummary.GlobalMinClearWidthMeters, true);
		SampleTryReadNumberField(*SummaryObject, TEXT("min_clear_at_along_m"), TEXT("$.scenario.semantic.summary"), Diagnostics, OutSummary.MinClearAtAlongMeters, true);
		SampleTryReadNumberField(*SummaryObject, TEXT("total_length_m"), TEXT("$.scenario.semantic.summary"), Diagnostics, OutSummary.TotalLengthMeters, true);
		SampleTryReadBoolField(*SummaryObject, TEXT("encounter_in_min_clear_zone"), TEXT("$.scenario.semantic.summary"), Diagnostics, OutSummary.bEncounterInMinClearZone, true);
	}

	void SampleParseSemantic(
		const FJsonObject& ScenarioObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleSemantic& OutSemantic)
	{
		TSharedPtr<FJsonObject> SemanticObject;
		if (!SampleTryReadObjectField(ScenarioObject, TEXT("semantic"), TEXT("$.scenario"), Diagnostics, SemanticObject, true))
		{
			return;
		}

		SampleParseRouteAxis(*SemanticObject, Diagnostics, OutSemantic.RouteAxis);
		SampleParseRobot(*SemanticObject, Diagnostics, OutSemantic.Robot);
		SampleParseLayout(*SemanticObject, Diagnostics, OutSemantic.Layout);
		SampleParseStaticObstacles(*SemanticObject, Diagnostics, OutSemantic.StaticObstacles);
		SampleParsePedestrians(*SemanticObject, Diagnostics, OutSemantic.Pedestrians);
		SampleParseClearWidthProfile(*SemanticObject, Diagnostics, OutSemantic.ClearWidthProfile);
		SampleParseSummary(*SemanticObject, Diagnostics, OutSemantic.Summary);
	}

	void SampleParseScenario(
		const FJsonObject& RootObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleScenario& OutScenario)
	{
		TSharedPtr<FJsonObject> ScenarioObject;
		if (!SampleTryReadObjectField(RootObject, TEXT("scenario"), TEXT("$"), Diagnostics, ScenarioObject, true))
		{
			return;
		}

		SampleParseParams(*ScenarioObject, Diagnostics, OutScenario.Params);
		SampleParseSemantic(*ScenarioObject, Diagnostics, OutScenario.Semantic);
	}

	void SampleParseDiagnosticObject(
		const FJsonObject& DiagnosticObject,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		TArray<FScenarioSchemaDiagnostic>& OutDocumentDiagnostics)
	{
		FScenarioSchemaDiagnostic DocumentDiagnostic;
		FString SeverityString;
		if (SampleTryReadRequiredStringField(DiagnosticObject, TEXT("severity"), Path, Diagnostics, SeverityString)
			&& !SampleParseDiagnosticSeverity(SeverityString, DocumentDiagnostic.Severity))
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_diagnostic_severity"), FString::Printf(TEXT("%s.severity"), *Path), FString::Printf(TEXT("%s.severity has unsupported value '%s'."), *Path, *SeverityString));
		}

		SampleTryReadRequiredStringField(DiagnosticObject, TEXT("code"), Path, Diagnostics, DocumentDiagnostic.Code);
		SampleTryReadRequiredStringField(DiagnosticObject, TEXT("path"), Path, Diagnostics, DocumentDiagnostic.Path);
		SampleTryReadRequiredStringField(DiagnosticObject, TEXT("message"), Path, Diagnostics, DocumentDiagnostic.Message);
		OutDocumentDiagnostics.Add(DocumentDiagnostic);
	}

	void SampleParseValidation(
		const FJsonObject& RootObject,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		FScenarioSampleValidation& OutValidation)
	{
		TSharedPtr<FJsonObject> ValidationObject;
		if (!SampleTryReadObjectField(RootObject, TEXT("validation"), TEXT("$"), Diagnostics, ValidationObject, true))
		{
			return;
		}

		SampleTryReadBoolField(*ValidationObject, TEXT("edited_by_user"), TEXT("$.validation"), Diagnostics, OutValidation.bEditedByUser, false);

		TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
		if (!SampleTryReadArrayField(*ValidationObject, TEXT("diagnostics"), TEXT("$.validation"), Diagnostics, DiagnosticValues, false))
		{
			return;
		}

		for (int32 Index = 0; Index < DiagnosticValues.Num(); ++Index)
		{
			const FString DiagnosticPath = FString::Printf(TEXT("$.validation.diagnostics[%d]"), Index);
			if (!DiagnosticValues[Index].IsValid() || DiagnosticValues[Index]->Type != EJson::Object)
			{
				AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_validation_diagnostic"), DiagnosticPath, FString::Printf(TEXT("%s must be an object."), *DiagnosticPath));
				continue;
			}

			SampleParseDiagnosticObject(*DiagnosticValues[Index]->AsObject(), DiagnosticPath, Diagnostics, OutValidation.Diagnostics);
		}
	}

	void SampleParseRoot(
		const FJsonObject& RootObject,
		FScenarioSampleParseResult& Result)
	{
		SampleTryReadRequiredStringField(RootObject, TEXT("schema"), TEXT("$"), Result.Diagnostics, Result.Document.Schema);
		if (!Result.Document.Schema.Equals(ScenarioSampleSchema, ESearchCase::CaseSensitive))
		{
			AddSampleDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_schema"), TEXT("$.schema"), FString::Printf(TEXT("$.schema must be '%s'."), ScenarioSampleSchema));
		}

		SampleTryReadRequiredVersion(RootObject, Result.Diagnostics, Result.Document.Version);
		SampleParseIdentity(RootObject, Result.Diagnostics, Result.Document.Sample);
		SampleParseScenario(RootObject, Result.Diagnostics, Result.Document.Scenario);
		SampleParseValidation(RootObject, Result.Diagnostics, Result.Document.Validation);
		FScenarioSampleJson::ValidateDocument(Result.Document, Result.Diagnostics);
		Result.bSuccess = !SampleHasErrors(Result.Diagnostics);
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakeNumberArray2(double A, double B)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(MakeShared<FJsonValueNumber>(A));
		Values.Add(MakeShared<FJsonValueNumber>(B));
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakeVector2DArray(const TArray<FVector2D>& Points)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Points.Num());
		for (const FVector2D& Point : Points)
		{
			Values.Add(MakeShared<FJsonValueArray>(SampleMakeNumberArray2(Point.X, Point.Y)));
		}
		return Values;
	}

	TSharedRef<FJsonObject> SampleMakeSourceObject(const FScenarioSampleSource& Source)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("template_ref"), Source.TemplateRef);
		Object->SetStringField(TEXT("template_hash"), Source.TemplateHash);
		Object->SetStringField(TEXT("profile_ref"), Source.ProfileRef);
		Object->SetStringField(TEXT("profile_hash"), Source.ProfileHash);
		Object->SetStringField(TEXT("setting_ref"), Source.SettingRef);
		Object->SetStringField(TEXT("setting_hash"), Source.SettingHash);
		Object->SetNumberField(TEXT("seed"), static_cast<double>(Source.Seed));
		Object->SetStringField(TEXT("generator_version"), Source.GeneratorVersion);
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeIdentityObject(const FScenarioSampleIdentity& Identity)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("sample_id"), Identity.SampleId);
		Object->SetStringField(TEXT("scenario_id"), Identity.ScenarioId);
		Object->SetObjectField(TEXT("source"), SampleMakeSourceObject(Identity.Source));
		return Object;
	}

	TSharedPtr<FJsonValue> SampleMakeParamValue(const FScenarioSampleParamValue& Param)
	{
		switch (Param.Type)
		{
		case EScenarioSampleParamValueType::Boolean:
			return MakeShared<FJsonValueBoolean>(Param.BoolValue);
		case EScenarioSampleParamValueType::Integer:
			return MakeShared<FJsonValueNumber>(Param.IntegerValue);
		case EScenarioSampleParamValueType::Float:
			return MakeShared<FJsonValueNumber>(Param.FloatValue);
		case EScenarioSampleParamValueType::String:
			return MakeShared<FJsonValueString>(Param.StringValue);
		case EScenarioSampleParamValueType::FloatArray:
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			Values.Reserve(Param.FloatArrayValue.Num());
			for (double Value : Param.FloatArrayValue)
			{
				Values.Add(MakeShared<FJsonValueNumber>(Value));
			}
			return MakeShared<FJsonValueArray>(Values);
		}
		case EScenarioSampleParamValueType::StringArray:
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			Values.Reserve(Param.StringArrayValue.Num());
			for (const FString& Value : Param.StringArrayValue)
			{
				Values.Add(MakeShared<FJsonValueString>(Value));
			}
			return MakeShared<FJsonValueArray>(Values);
		}
		case EScenarioSampleParamValueType::None:
		default:
			return MakeShared<FJsonValueNull>();
		}
	}

	TSharedRef<FJsonObject> SampleMakeParamsObject(const TMap<FString, FScenarioSampleParamValue>& Params)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		TArray<FString> Keys;
		Params.GenerateKeyArray(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			Object->SetField(Key, SampleMakeParamValue(Params.FindChecked(Key)));
		}
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeRouteAxisObject(const FScenarioSampleRouteAxis& RouteAxis)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("type"), TEXT("polyline"));
		Object->SetArrayField(TEXT("origin_xy_m"), SampleMakeNumberArray2(RouteAxis.OriginXYMeters.X, RouteAxis.OriginXYMeters.Y));
		Object->SetNumberField(TEXT("heading_deg"), RouteAxis.HeadingDegrees);
		Object->SetArrayField(TEXT("points_m"), SampleMakeVector2DArray(RouteAxis.PointsMeters));
		Object->SetNumberField(TEXT("length_m"), RouteAxis.LengthMeters);
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeRobotPoseObject(const FScenarioSampleRobotPose& Pose)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("segment"), Pose.SegmentId);
		Object->SetNumberField(TEXT("along_m"), Pose.AlongMeters);
		Object->SetNumberField(TEXT("offset_m"), Pose.OffsetMeters);
		if (!Pose.LaneId.IsEmpty())
		{
			Object->SetStringField(TEXT("lane"), Pose.LaneId);
		}
		Object->SetNumberField(TEXT("heading_deg"), Pose.HeadingDegrees);
		Object->SetStringField(TEXT("source_anchor_type"), SampleAnchorTypeToString(Pose.SourceAnchorType));
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeRobotObject(const FScenarioSampleRobotSemantic& Robot)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetObjectField(TEXT("start"), SampleMakeRobotPoseObject(Robot.Start));
		Object->SetObjectField(TEXT("goal"), SampleMakeRobotPoseObject(Robot.Goal));
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeLayoutLaneObject(const FScenarioSampleLayoutLane& Lane)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("lane"), Lane.LaneId);
		Object->SetArrayField(TEXT("offset_range_m"), SampleMakeNumberArray2(Lane.OffsetRangeMeters.MinMeters, Lane.OffsetRangeMeters.MaxMeters));
		Object->SetStringField(TEXT("surface"), Lane.SurfaceId);
		Object->SetStringField(TEXT("type"), SampleLaneTypeToString(Lane.Type));
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakeLayoutArray(const TArray<FScenarioSampleLayoutEntry>& Layout)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Layout.Num());
		for (const FScenarioSampleLayoutEntry& Entry : Layout)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetArrayField(TEXT("along_range_m"), SampleMakeNumberArray2(Entry.AlongRangeMeters.StartMeters, Entry.AlongRangeMeters.EndMeters));
			Object->SetStringField(TEXT("segment"), Entry.SegmentId);

			TArray<TSharedPtr<FJsonValue>> LaneValues;
			LaneValues.Reserve(Entry.Lanes.Num());
			for (const FScenarioSampleLayoutLane& Lane : Entry.Lanes)
			{
				LaneValues.Add(MakeShared<FJsonValueObject>(SampleMakeLayoutLaneObject(Lane)));
			}
			Object->SetArrayField(TEXT("lanes"), LaneValues);
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakeStaticObstacleArray(const TArray<FScenarioSampleStaticObstacle>& Obstacles)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Obstacles.Num());
		for (const FScenarioSampleStaticObstacle& Obstacle : Obstacles)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("id"), Obstacle.ObstacleId);
			Object->SetStringField(TEXT("prop"), Obstacle.PropId);
			if (!Obstacle.PerceptionTag.IsEmpty())
			{
				Object->SetStringField(TEXT("perception_tag"), Obstacle.PerceptionTag);
			}
			Object->SetStringField(TEXT("class"), SampleObstacleClassToString(Obstacle.ObstacleClass));
			if (!Obstacle.SensorProfile.IsEmpty())
			{
				Object->SetStringField(TEXT("sensor_profile"), Obstacle.SensorProfile);
			}
			Object->SetNumberField(TEXT("along_m"), Obstacle.AlongMeters);
			Object->SetNumberField(TEXT("offset_m"), Obstacle.OffsetMeters);
			Object->SetNumberField(TEXT("yaw_deg"), Obstacle.YawDegrees);
			Object->SetArrayField(TEXT("footprint_m"), SampleMakeNumberArray2(Obstacle.FootprintMeters.X, Obstacle.FootprintMeters.Y));
			Object->SetStringField(TEXT("placed_by"), Obstacle.PlacedBy);
			Object->SetNumberField(TEXT("clear_width_remaining_m"), Obstacle.ClearWidthRemainingMeters);
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TSharedRef<FJsonObject> SampleMakeBehaviorObject(const FScenarioSamplePedestrianBehavior& Behavior)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("cooperation"), Behavior.Cooperation);
		Object->SetNumberField(TEXT("evasiveness"), Behavior.Evasiveness);
		Object->SetNumberField(TEXT("personal_space_m"), Behavior.PersonalSpaceMeters);
		Object->SetNumberField(TEXT("awareness_horizon_s"), Behavior.AwarenessHorizonSeconds);
		Object->SetNumberField(TEXT("max_yield_wait_s"), Behavior.MaxYieldWaitSeconds);
		Object->SetNumberField(TEXT("sidestep_distance_m"), Behavior.SidestepDistanceMeters);
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeBaselinePoseObject(
		const FString& SegmentId,
		double AlongMeters,
		double OffsetMeters)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("segment"), SegmentId);
		Object->SetNumberField(TEXT("along_m"), AlongMeters);
		Object->SetNumberField(TEXT("offset_m"), OffsetMeters);
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeBaselineObject(const FScenarioSamplePedestrianBaseline& Baseline)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetObjectField(TEXT("start"), SampleMakeBaselinePoseObject(Baseline.StartSegmentId, Baseline.StartAlongMeters, Baseline.StartOffsetMeters));
		Object->SetObjectField(TEXT("goal"), SampleMakeBaselinePoseObject(Baseline.GoalSegmentId, Baseline.GoalAlongMeters, Baseline.GoalOffsetMeters));
		if (!Baseline.PointsMeters.IsEmpty())
		{
			Object->SetArrayField(TEXT("points_m"), SampleMakeVector2DArray(Baseline.PointsMeters));
		}
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakePedestrianArray(const TArray<FScenarioSamplePedestrian>& Pedestrians)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Pedestrians.Num());
		for (const FScenarioSamplePedestrian& Pedestrian : Pedestrians)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("id"), Pedestrian.PedestrianId);
			Object->SetStringField(TEXT("role"), SamplePedestrianRoleToString(Pedestrian.Role));
			if (!Pedestrian.PlacedBy.IsEmpty())
			{
				Object->SetStringField(TEXT("placed_by"), Pedestrian.PlacedBy);
			}
			if (Pedestrian.Role == EScenarioSamplePedestrianRole::Encounter)
			{
				Object->SetStringField(TEXT("type"), SampleEncounterTypeToString(Pedestrian.EncounterType));
			}
			if (!Pedestrian.PersonaId.IsEmpty())
			{
				Object->SetStringField(TEXT("persona"), Pedestrian.PersonaId);
			}
			Object->SetObjectField(TEXT("behavior"), SampleMakeBehaviorObject(Pedestrian.Behavior));
			Object->SetNumberField(TEXT("speed_mps"), Pedestrian.SpeedMetersPerSecond);
			Object->SetObjectField(TEXT("baseline"), SampleMakeBaselineObject(Pedestrian.Baseline));
			if (!Pedestrian.PedestrianScenarioHash.IsEmpty())
			{
				Object->SetStringField(TEXT("pedestrian_scenario_hash"), Pedestrian.PedestrianScenarioHash);
			}
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakeClearWidthProfileArray(const TArray<FScenarioSampleClearWidthEntry>& Profile)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Profile.Num());
		for (const FScenarioSampleClearWidthEntry& Entry : Profile)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetArrayField(TEXT("along_range_m"), SampleMakeNumberArray2(Entry.AlongRangeMeters.StartMeters, Entry.AlongRangeMeters.EndMeters));
			Object->SetNumberField(TEXT("clear_width_m"), Entry.ClearWidthMeters);
			if (!Entry.LimitedBy.IsEmpty())
			{
				Object->SetStringField(TEXT("limited_by"), Entry.LimitedBy);
			}
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TSharedRef<FJsonObject> SampleMakeSummaryObject(const FScenarioSampleSummary& Summary)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("global_min_clear_width_m"), Summary.GlobalMinClearWidthMeters);
		Object->SetNumberField(TEXT("min_clear_at_along_m"), Summary.MinClearAtAlongMeters);
		Object->SetNumberField(TEXT("total_length_m"), Summary.TotalLengthMeters);
		Object->SetBoolField(TEXT("encounter_in_min_clear_zone"), Summary.bEncounterInMinClearZone);
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeSemanticObject(const FScenarioSampleSemantic& Semantic)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetObjectField(TEXT("route_axis"), SampleMakeRouteAxisObject(Semantic.RouteAxis));
		Object->SetObjectField(TEXT("robot"), SampleMakeRobotObject(Semantic.Robot));
		Object->SetArrayField(TEXT("layout"), SampleMakeLayoutArray(Semantic.Layout));
		Object->SetArrayField(TEXT("static_obstacles"), SampleMakeStaticObstacleArray(Semantic.StaticObstacles));
		Object->SetArrayField(TEXT("pedestrians"), SampleMakePedestrianArray(Semantic.Pedestrians));
		Object->SetArrayField(TEXT("clear_width_profile"), SampleMakeClearWidthProfileArray(Semantic.ClearWidthProfile));
		Object->SetObjectField(TEXT("summary"), SampleMakeSummaryObject(Semantic.Summary));
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeScenarioObject(const FScenarioSampleScenario& Scenario)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetObjectField(TEXT("params"), SampleMakeParamsObject(Scenario.Params));
		Object->SetObjectField(TEXT("semantic"), SampleMakeSemanticObject(Scenario.Semantic));
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> SampleMakeDiagnosticsArray(const TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Diagnostics.Num());
		for (const FScenarioSchemaDiagnostic& Diagnostic : Diagnostics)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("severity"), SampleDiagnosticSeverityToString(Diagnostic.Severity));
			Object->SetStringField(TEXT("code"), Diagnostic.Code);
			Object->SetStringField(TEXT("path"), Diagnostic.Path);
			Object->SetStringField(TEXT("message"), Diagnostic.Message);
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TSharedRef<FJsonObject> SampleMakeValidationObject(const FScenarioSampleValidation& Validation)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("edited_by_user"), Validation.bEditedByUser);
		Object->SetArrayField(TEXT("diagnostics"), SampleMakeDiagnosticsArray(Validation.Diagnostics));
		return Object;
	}

	TSharedRef<FJsonObject> SampleMakeDocumentObject(const FScenarioSampleDocument& Document)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), Document.Schema);
		Object->SetNumberField(TEXT("version"), Document.Version);
		Object->SetObjectField(TEXT("sample"), SampleMakeIdentityObject(Document.Sample));
		Object->SetObjectField(TEXT("scenario"), SampleMakeScenarioObject(Document.Scenario));
		Object->SetObjectField(TEXT("validation"), SampleMakeValidationObject(Document.Validation));
		return Object;
	}

	void ValidateSampleAlongRange(
		const FScenarioAlongRangeMeters& Range,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		if (Range.StartMeters > Range.EndMeters)
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_along_range"), Path, FString::Printf(TEXT("%s start must be <= end."), *Path));
		}
	}

	void ValidateSampleOffsetRange(
		const FScenarioOffsetRangeMeters& Range,
		const FString& Path,
		TArray<FScenarioSchemaDiagnostic>& Diagnostics)
	{
		if (Range.MinMeters > Range.MaxMeters)
		{
			AddSampleDiagnostic(Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_offset_range"), Path, FString::Printf(TEXT("%s min must be <= max."), *Path));
		}
	}
}

FScenarioSampleParseResult FScenarioSampleJson::ParseFromFile(const FString& JsonFilePath)
{
	FScenarioSampleParseResult Result;
	if (JsonFilePath.TrimStartAndEnd().IsEmpty())
	{
		AddSampleDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_sample_path"), TEXT("$"), TEXT("Scenario sample file path must not be empty."));
		return Result;
	}

	const FString ResolvedPath = ResolveSampleSchemaPath(JsonFilePath);
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ResolvedPath))
	{
		AddSampleDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("sample_file_read_failed"), TEXT("$"), FString::Printf(TEXT("Scenario sample JSON read failed: %s"), *ResolvedPath));
		return Result;
	}

	return ParseFromString(JsonString);
}

FScenarioSampleParseResult FScenarioSampleJson::ParseFromString(const FString& JsonString)
{
	FScenarioSampleParseResult Result;
	if (JsonString.TrimStartAndEnd().IsEmpty())
	{
		AddSampleDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_sample_json"), TEXT("$"), TEXT("Scenario sample JSON must not be empty."));
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddSampleDiagnostic(Result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_sample_json"), TEXT("$"), TEXT("Scenario sample JSON parse failed."));
		return Result;
	}

	SampleParseRoot(*RootObject, Result);
	return Result;
}

bool FScenarioSampleJson::ValidateDocument(
	const FScenarioSampleDocument& Document,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	if (!Document.Schema.Equals(ScenarioSampleSchema, ESearchCase::CaseSensitive))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_schema"), TEXT("$.schema"), FString::Printf(TEXT("$.schema must be '%s'."), ScenarioSampleSchema));
	}
	if (Document.Version != SupportedVersion)
	{
		AddSampleDiagnostic(
			OutDiagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			TEXT("unsupported_schema_version"),
			TEXT("$.version"),
			FString::Printf(TEXT("scenario_sample version must match the current compiler and validator version: %d."), SupportedVersion));
	}
	if (SampleStringIsEmpty(Document.Sample.SampleId))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_sample_id"), TEXT("$.sample.sample_id"), TEXT("$.sample.sample_id must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.ScenarioId))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_scenario_id"), TEXT("$.sample.scenario_id"), TEXT("$.sample.scenario_id must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.TemplateRef))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_template_ref"), TEXT("$.sample.source.template_ref"), TEXT("$.sample.source.template_ref must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.TemplateHash))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_template_hash"), TEXT("$.sample.source.template_hash"), TEXT("$.sample.source.template_hash must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.ProfileRef))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_profile_ref"), TEXT("$.sample.source.profile_ref"), TEXT("$.sample.source.profile_ref must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.ProfileHash))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_profile_hash"), TEXT("$.sample.source.profile_hash"), TEXT("$.sample.source.profile_hash must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.SettingRef))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_setting_ref"), TEXT("$.sample.source.setting_ref"), TEXT("$.sample.source.setting_ref must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.SettingHash))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_setting_hash"), TEXT("$.sample.source.setting_hash"), TEXT("$.sample.source.setting_hash must not be empty."));
	}
	if (SampleStringIsEmpty(Document.Sample.Source.GeneratorVersion))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_generator_version"), TEXT("$.sample.source.generator_version"), TEXT("$.sample.source.generator_version must not be empty."));
	}

	const FScenarioSampleSemantic& Semantic = Document.Scenario.Semantic;
	if (Semantic.RouteAxis.PointsMeters.Num() < 2)
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("too_few_axis_points"), TEXT("$.scenario.semantic.route_axis.points_m"), TEXT("$.scenario.semantic.route_axis.points_m must contain at least two points."));
	}
	if (Semantic.RouteAxis.LengthMeters <= 0.0)
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_axis_length"), TEXT("$.scenario.semantic.route_axis.length_m"), TEXT("$.scenario.semantic.route_axis.length_m must be positive."));
	}
	if (SampleStringIsEmpty(Semantic.Robot.Start.SegmentId))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_robot_start_segment"), TEXT("$.scenario.semantic.robot.start.segment"), TEXT("$.scenario.semantic.robot.start.segment must not be empty."));
	}
	if (SampleStringIsEmpty(Semantic.Robot.Goal.SegmentId))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_robot_goal_segment"), TEXT("$.scenario.semantic.robot.goal.segment"), TEXT("$.scenario.semantic.robot.goal.segment must not be empty."));
	}

	for (int32 EntryIndex = 0; EntryIndex < Semantic.Layout.Num(); ++EntryIndex)
	{
		const FScenarioSampleLayoutEntry& Entry = Semantic.Layout[EntryIndex];
		const FString EntryPath = FString::Printf(TEXT("$.scenario.semantic.layout[%d]"), EntryIndex);
		ValidateSampleAlongRange(Entry.AlongRangeMeters, FString::Printf(TEXT("%s.along_range_m"), *EntryPath), OutDiagnostics);
		if (SampleStringIsEmpty(Entry.SegmentId))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_layout_segment"), FString::Printf(TEXT("%s.segment"), *EntryPath), FString::Printf(TEXT("%s.segment must not be empty."), *EntryPath));
		}
		for (int32 LaneIndex = 0; LaneIndex < Entry.Lanes.Num(); ++LaneIndex)
		{
			const FScenarioSampleLayoutLane& Lane = Entry.Lanes[LaneIndex];
			const FString LanePath = FString::Printf(TEXT("%s.lanes[%d]"), *EntryPath, LaneIndex);
			if (SampleStringIsEmpty(Lane.LaneId))
			{
				AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_lane"), FString::Printf(TEXT("%s.lane"), *LanePath), FString::Printf(TEXT("%s.lane must not be empty."), *LanePath));
			}
			if (SampleStringIsEmpty(Lane.SurfaceId))
			{
				AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_surface"), FString::Printf(TEXT("%s.surface"), *LanePath), FString::Printf(TEXT("%s.surface must not be empty."), *LanePath));
			}
			ValidateSampleOffsetRange(Lane.OffsetRangeMeters, FString::Printf(TEXT("%s.offset_range_m"), *LanePath), OutDiagnostics);
		}
	}

	TSet<FString> ObstacleIds;
	for (int32 Index = 0; Index < Semantic.StaticObstacles.Num(); ++Index)
	{
		const FScenarioSampleStaticObstacle& Obstacle = Semantic.StaticObstacles[Index];
		const FString ObstaclePath = FString::Printf(TEXT("$.scenario.semantic.static_obstacles[%d]"), Index);
		if (SampleStringIsEmpty(Obstacle.ObstacleId))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_obstacle_id"), FString::Printf(TEXT("%s.id"), *ObstaclePath), FString::Printf(TEXT("%s.id must not be empty."), *ObstaclePath));
		}
		else if (ObstacleIds.Contains(Obstacle.ObstacleId))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("duplicate_obstacle_id"), FString::Printf(TEXT("%s.id"), *ObstaclePath), FString::Printf(TEXT("%s.id duplicates '%s'."), *ObstaclePath, *Obstacle.ObstacleId));
		}
		else
		{
			ObstacleIds.Add(Obstacle.ObstacleId);
		}
		if (SampleStringIsEmpty(Obstacle.PropId))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_prop"), FString::Printf(TEXT("%s.prop"), *ObstaclePath), FString::Printf(TEXT("%s.prop must not be empty."), *ObstaclePath));
		}
		if (Obstacle.FootprintMeters.X < 0.0 || Obstacle.FootprintMeters.Y < 0.0)
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_footprint"), FString::Printf(TEXT("%s.footprint_m"), *ObstaclePath), FString::Printf(TEXT("%s.footprint_m values must be non-negative."), *ObstaclePath));
		}
	}

	TSet<FString> PedestrianIds;
	for (int32 Index = 0; Index < Semantic.Pedestrians.Num(); ++Index)
	{
		const FScenarioSamplePedestrian& Pedestrian = Semantic.Pedestrians[Index];
		const FString PedestrianPath = FString::Printf(TEXT("$.scenario.semantic.pedestrians[%d]"), Index);
		if (SampleStringIsEmpty(Pedestrian.PedestrianId))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_pedestrian_id"), FString::Printf(TEXT("%s.id"), *PedestrianPath), FString::Printf(TEXT("%s.id must not be empty."), *PedestrianPath));
		}
		else if (PedestrianIds.Contains(Pedestrian.PedestrianId))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("duplicate_pedestrian_id"), FString::Printf(TEXT("%s.id"), *PedestrianPath), FString::Printf(TEXT("%s.id duplicates '%s'."), *PedestrianPath, *Pedestrian.PedestrianId));
		}
		else
		{
			PedestrianIds.Add(Pedestrian.PedestrianId);
		}
		if (Pedestrian.Role == EScenarioSamplePedestrianRole::Encounter && SampleStringIsEmpty(Pedestrian.PlacedBy))
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_pedestrian_placed_by"), FString::Printf(TEXT("%s.placed_by"), *PedestrianPath), FString::Printf(TEXT("%s.placed_by must not be empty for encounter pedestrians."), *PedestrianPath));
		}
		if (Pedestrian.SpeedMetersPerSecond < 0.0)
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_pedestrian_speed"), FString::Printf(TEXT("%s.speed_mps"), *PedestrianPath), FString::Printf(TEXT("%s.speed_mps must be non-negative."), *PedestrianPath));
		}
	}

	for (int32 Index = 0; Index < Semantic.ClearWidthProfile.Num(); ++Index)
	{
		const FScenarioSampleClearWidthEntry& Entry = Semantic.ClearWidthProfile[Index];
		const FString EntryPath = FString::Printf(TEXT("$.scenario.semantic.clear_width_profile[%d]"), Index);
		ValidateSampleAlongRange(Entry.AlongRangeMeters, FString::Printf(TEXT("%s.along_range_m"), *EntryPath), OutDiagnostics);
		if (Entry.ClearWidthMeters < 0.0)
		{
			AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_clear_width"), FString::Printf(TEXT("%s.clear_width_m"), *EntryPath), FString::Printf(TEXT("%s.clear_width_m must be non-negative."), *EntryPath));
		}
	}

	for (int32 Index = 0; Index < Document.Validation.Diagnostics.Num(); ++Index)
	{
		const FScenarioSchemaDiagnostic& Diagnostic = Document.Validation.Diagnostics[Index];
		if (Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error)
		{
			AddSampleDiagnostic(
				OutDiagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("sample_contains_error_diagnostic"),
				FString::Printf(TEXT("$.validation.diagnostics[%d]"), Index),
				FString::Printf(TEXT("Sample validation diagnostics contain error code '%s'."), *Diagnostic.Code));
		}
	}

	return !SampleHasErrors(OutDiagnostics);
}

bool FScenarioSampleJson::TryWriteJson(
	const FScenarioSampleDocument& Document,
	FString& OutJson,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutJson.Reset();
	OutDiagnostics.Reset();
	if (!ValidateDocument(Document, OutDiagnostics))
	{
		return false;
	}

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(SampleMakeDocumentObject(Document), Writer))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("sample_json_serialize_failed"), TEXT("$"), TEXT("Scenario sample JSON serialization failed."));
		return false;
	}

	return true;
}

bool FScenarioSampleJson::SaveToFile(
	const FScenarioSampleDocument& Document,
	const FString& JsonFilePath,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (JsonFilePath.TrimStartAndEnd().IsEmpty())
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_sample_output_path"), TEXT("$"), TEXT("Scenario sample output path must not be empty."));
		return false;
	}

	FString Json;
	if (!TryWriteJson(Document, Json, OutDiagnostics))
	{
		return false;
	}

	const FString ResolvedPath = ResolveSampleSchemaPath(JsonFilePath);
	const FString OutputDirectory = FPaths::GetPath(ResolvedPath);
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("sample_output_directory_failed"), TEXT("$"), FString::Printf(TEXT("Scenario sample output directory create failed: %s"), *OutputDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Json, *ResolvedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddSampleDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("sample_file_write_failed"), TEXT("$"), FString::Printf(TEXT("Scenario sample file write failed: %s"), *ResolvedPath));
		return false;
	}

	return true;
}
