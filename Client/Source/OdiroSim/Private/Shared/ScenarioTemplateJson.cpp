#include "Shared/ScenarioTemplateJson.h"

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
	const TCHAR* ScenarioTemplateSchema = TEXT("scenario_template");

	FString ResolveScenarioSchemaPath(const FString& filePath)
	{
		if (filePath.IsEmpty() || !FPaths::IsRelative(filePath))
		{
			return filePath;
		}

		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), filePath));
	}

	void AddTemplateDiagnostic(
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		EScenarioSchemaDiagnosticSeverity severity,
		const FString& code,
		const FString& path,
		const FString& message)
	{
		FScenarioSchemaDiagnostic diagnostic;
		diagnostic.Severity = severity;
		diagnostic.Code = code;
		diagnostic.Path = path;
		diagnostic.Message = message;
		diagnostics.Add(diagnostic);
	}

	bool HasTemplateErrors(const TArray<FScenarioSchemaDiagnostic>& diagnostics)
	{
		for (const FScenarioSchemaDiagnostic& diagnostic : diagnostics)
		{
			if (diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	bool IsStringEmpty(const FString& value)
	{
		return value.TrimStartAndEnd().IsEmpty();
	}

	bool TryReadObjectField(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TSharedPtr<FJsonObject>& outObject,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			if (bRequired)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s"), *path, *fieldName),
					FString::Printf(TEXT("%s.%s is required."), *path, *fieldName));
			}
			return !bRequired;
		}

		if (value->Type != EJson::Object)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must be an object."), *path, *fieldName));
			return false;
		}

		outObject = value->AsObject();
		return outObject.IsValid();
	}

	bool TryReadArrayField(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TArray<TSharedPtr<FJsonValue>>& outArray,
		bool bRequired)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			if (bRequired)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s"), *path, *fieldName),
					FString::Printf(TEXT("%s.%s is required."), *path, *fieldName));
			}
			return !bRequired;
		}

		if (value->Type != EJson::Array)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must be an array."), *path, *fieldName));
			return false;
		}

		outArray = value->AsArray();
		return true;
	}

	bool TryReadTemplateRequiredStringField(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FString& outValue)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s is required."), *path, *fieldName));
			return false;
		}

		if (value->Type != EJson::String)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *path, *fieldName));
			return false;
		}

		outValue = value->AsString();
		if (IsStringEmpty(outValue))
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("empty_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must not be empty."), *path, *fieldName));
			return false;
		}

		return true;
	}

	bool TryReadOptionalStringField(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FString& outValue)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			return true;
		}

		if (value->Type != EJson::String)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must be a string."), *path, *fieldName));
			return false;
		}

		outValue = value->AsString();
		return true;
	}

	bool TryReadBoolField(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		bool& outValue)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			return true;
		}

		if (value->Type != EJson::Boolean)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must be a boolean."), *path, *fieldName));
			return false;
		}

		outValue = value->AsBool();
		return true;
	}

	bool TryReadRequiredVersion(
		const FJsonObject& object,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		int32& outVersion)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(TEXT("version"));
		if (!value.IsValid())
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("missing_version"),
				TEXT("$.version"),
				TEXT("$.version is required."));
			return false;
		}

		if (value->Type != EJson::Number)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("invalid_version"),
				TEXT("$.version"),
				TEXT("$.version must be a number."));
			return false;
		}

		outVersion = FMath::RoundToInt(value->AsNumber());
		if (outVersion != FScenarioTemplateJson::SupportedVersion)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				TEXT("unsupported_schema_version"),
				TEXT("$.version"),
				FString::Printf(
					TEXT("scenario_template version must match the current compiler and validator version: %d."),
					FScenarioTemplateJson::SupportedVersion));
			return false;
		}

		return true;
	}

	bool TryReadNumberValue(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateNumberValue& outValue,
		bool bRequired)
	{
		outValue = FScenarioTemplateNumberValue{};
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			if (bRequired)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s"), *path, *fieldName),
					FString::Printf(TEXT("%s.%s is required."), *path, *fieldName));
			}
			return !bRequired;
		}

		outValue.bIsSet = true;
		if (value->Type == EJson::Number)
		{
			outValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
			outValue.FixedValue = value->AsNumber();
			return true;
		}

		if (value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> rangeObject = value->AsObject();
			double minValue = 0.0;
			double maxValue = 0.0;
			if (!rangeObject.IsValid()
				|| !rangeObject->TryGetNumberField(TEXT("min"), minValue)
				|| !rangeObject->TryGetNumberField(TEXT("max"), maxValue))
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("invalid_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s"), *path, *fieldName),
					FString::Printf(TEXT("%s.%s range must contain numeric min and max."), *path, *fieldName));
				return false;
			}

			outValue.Mode = EScenarioTemplateNumberValueMode::Range;
			outValue.MinValue = minValue;
			outValue.MaxValue = maxValue;
			return true;
		}

		AddTemplateDiagnostic(
			diagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			FString::Printf(TEXT("invalid_%s"), *fieldName),
			FString::Printf(TEXT("%s.%s"), *path, *fieldName),
			FString::Printf(TEXT("%s.%s must be a number or {min,max} object."), *path, *fieldName));
		return false;
	}

	bool TryReadIntegerValue(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateIntegerValue& outValue,
		bool bRequired)
	{
		FScenarioTemplateNumberValue numberValue;
		if (!TryReadNumberValue(object, fieldName, path, diagnostics, numberValue, bRequired))
		{
			return false;
		}

		outValue = FScenarioTemplateIntegerValue{};
		outValue.bIsSet = numberValue.bIsSet;
		outValue.Mode = numberValue.Mode;
		outValue.FixedValue = FMath::RoundToInt(numberValue.FixedValue);
		outValue.MinValue = FMath::RoundToInt(numberValue.MinValue);
		outValue.MaxValue = FMath::RoundToInt(numberValue.MaxValue);
		return true;
	}

	bool TryReadStringValue(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateStringValue& outValue,
		bool bRequired)
	{
		outValue = FScenarioTemplateStringValue{};
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid())
		{
			if (bRequired)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("missing_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s"), *path, *fieldName),
					FString::Printf(TEXT("%s.%s is required."), *path, *fieldName));
			}
			return !bRequired;
		}

		outValue.bIsSet = true;
		if (value->Type == EJson::String)
		{
			outValue.Mode = EScenarioTemplateStringValueMode::Fixed;
			outValue.FixedValue = value->AsString();
			return true;
		}

		if (value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> choicesObject = value->AsObject();
			TArray<TSharedPtr<FJsonValue>> choices;
			if (!choicesObject.IsValid() || !TryReadArrayField(*choicesObject, TEXT("choices"), FString::Printf(TEXT("%s.%s"), *path, *fieldName), diagnostics, choices, true))
			{
				return false;
			}

			outValue.Mode = EScenarioTemplateStringValueMode::Choices;
			for (int32 index = 0; index < choices.Num(); ++index)
			{
				const TSharedPtr<FJsonValue>& choiceValue = choices[index];
				if (!choiceValue.IsValid() || choiceValue->Type != EJson::String)
				{
					AddTemplateDiagnostic(
						diagnostics,
						EScenarioSchemaDiagnosticSeverity::Error,
						FString::Printf(TEXT("invalid_%s_choice"), *fieldName),
						FString::Printf(TEXT("%s.%s.choices[%d]"), *path, *fieldName, index),
						FString::Printf(TEXT("%s.%s.choices[%d] must be a string."), *path, *fieldName, index));
					return false;
				}
				outValue.Choices.Add(choiceValue->AsString());
			}
			return true;
		}

		AddTemplateDiagnostic(
			diagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			FString::Printf(TEXT("invalid_%s"), *fieldName),
			FString::Printf(TEXT("%s.%s"), *path, *fieldName),
			FString::Printf(TEXT("%s.%s must be a string or {choices} object."), *path, *fieldName));
		return false;
	}

	bool TryReadAlongRange(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioAlongRangeMeters& outRange,
		bool bRequired)
	{
		TArray<TSharedPtr<FJsonValue>> values;
		if (!TryReadArrayField(object, fieldName, path, diagnostics, values, bRequired))
		{
			return false;
		}
		if (values.IsEmpty() && !bRequired)
		{
			return true;
		}
		if (values.Num() != 2)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s must contain exactly two numbers."), *path, *fieldName));
			return false;
		}
		if (!values[0].IsValid() || values[0]->Type != EJson::Number || !values[1].IsValid() || values[1]->Type != EJson::Number)
		{
			AddTemplateDiagnostic(
				diagnostics,
				EScenarioSchemaDiagnosticSeverity::Error,
				FString::Printf(TEXT("invalid_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s"), *path, *fieldName),
				FString::Printf(TEXT("%s.%s values must be numbers."), *path, *fieldName));
			return false;
		}

		outRange.StartMeters = values[0]->AsNumber();
		outRange.EndMeters = values[1]->AsNumber();
		return true;
	}

	bool TryReadVector2DArray(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TArray<FVector2D>& outPoints,
		bool bRequired)
	{
		TArray<TSharedPtr<FJsonValue>> values;
		if (!TryReadArrayField(object, fieldName, path, diagnostics, values, bRequired))
		{
			return false;
		}

		outPoints.Reset();
		for (int32 index = 0; index < values.Num(); ++index)
		{
			const TSharedPtr<FJsonValue>& pointValue = values[index];
			if (!pointValue.IsValid() || pointValue->Type != EJson::Array)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("invalid_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s[%d]"), *path, *fieldName, index),
					FString::Printf(TEXT("%s.%s[%d] must be a two-number array."), *path, *fieldName, index));
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>> point = pointValue->AsArray();
			if (point.Num() != 2 || !point[0].IsValid() || point[0]->Type != EJson::Number || !point[1].IsValid() || point[1]->Type != EJson::Number)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("invalid_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s[%d]"), *path, *fieldName, index),
					FString::Printf(TEXT("%s.%s[%d] must contain exactly two numbers."), *path, *fieldName, index));
				return false;
			}

			outPoints.Add(FVector2D(point[0]->AsNumber(), point[1]->AsNumber()));
		}

		return true;
	}

	bool TryReadStringArray(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TArray<FString>& outValues,
		bool bRequired)
	{
		TArray<TSharedPtr<FJsonValue>> values;
		if (!TryReadArrayField(object, fieldName, path, diagnostics, values, bRequired))
		{
			return false;
		}

		outValues.Reset();
		for (int32 index = 0; index < values.Num(); ++index)
		{
			const TSharedPtr<FJsonValue>& value = values[index];
			if (!value.IsValid() || value->Type != EJson::String)
			{
				AddTemplateDiagnostic(
					diagnostics,
					EScenarioSchemaDiagnosticSeverity::Error,
					FString::Printf(TEXT("invalid_%s"), *fieldName),
					FString::Printf(TEXT("%s.%s[%d]"), *path, *fieldName, index),
					FString::Printf(TEXT("%s.%s[%d] must be a string."), *path, *fieldName, index));
				return false;
			}
			outValues.Add(value->AsString());
		}

		return true;
	}

	bool ParseSegmentType(const FString& value, EScenarioTemplateSegmentType& outType)
	{
		const FString normalized = value.ToLower();
		if (normalized == TEXT("straight")) { outType = EScenarioTemplateSegmentType::Straight; return true; }
		if (normalized == TEXT("narrowing")) { outType = EScenarioTemplateSegmentType::Narrowing; return true; }
		if (normalized == TEXT("crosswalk")) { outType = EScenarioTemplateSegmentType::Crosswalk; return true; }
		if (normalized == TEXT("entrance")) { outType = EScenarioTemplateSegmentType::Entrance; return true; }
		return false;
	}

	bool ParsePlacementKind(const FString& value, EScenarioTemplateObstaclePlacementKind& outKind)
	{
		const FString normalized = value.ToLower();
		if (normalized == TEXT("fixed")) { outKind = EScenarioTemplateObstaclePlacementKind::Fixed; return true; }
		if (normalized == TEXT("pattern")) { outKind = EScenarioTemplateObstaclePlacementKind::Pattern; return true; }
		if (normalized == TEXT("scatter")) { outKind = EScenarioTemplateObstaclePlacementKind::Scatter; return true; }
		return false;
	}

	bool ParseEncounterType(const FString& value, EScenarioTemplateEncounterType& outType)
	{
		const FString normalized = value.ToLower();
		if (normalized == TEXT("oncoming_pass")) { outType = EScenarioTemplateEncounterType::OncomingPass; return true; }
		if (normalized == TEXT("overtake")) { outType = EScenarioTemplateEncounterType::Overtake; return true; }
		if (normalized == TEXT("cross_path")) { outType = EScenarioTemplateEncounterType::CrossPath; return true; }
		if (normalized == TEXT("standing_group")) { outType = EScenarioTemplateEncounterType::StandingGroup; return true; }
		return false;
	}

	bool ParseAnchorType(const FString& value, EScenarioTemplateRobotAnchorType& outType)
	{
		const FString normalized = value.ToLower();
		if (normalized == TEXT("entry")) { outType = EScenarioTemplateRobotAnchorType::Entry; return true; }
		if (normalized == TEXT("exit")) { outType = EScenarioTemplateRobotAnchorType::Exit; return true; }
		if (normalized == TEXT("corridor_pose")) { outType = EScenarioTemplateRobotAnchorType::CorridorPose; return true; }
		return false;
	}

	bool ParseHeading(const FString& value, EScenarioTemplateRobotHeading& outHeading)
	{
		const FString normalized = value.ToLower();
		if (normalized == TEXT("forward")) { outHeading = EScenarioTemplateRobotHeading::Forward; return true; }
		if (normalized == TEXT("backward")) { outHeading = EScenarioTemplateRobotHeading::Backward; return true; }
		if (normalized == TEXT("auto")) { outHeading = EScenarioTemplateRobotHeading::Auto; return true; }
		return false;
	}

	FString SegmentTypeToString(EScenarioTemplateSegmentType value)
	{
		switch (value)
		{
		case EScenarioTemplateSegmentType::Straight: return TEXT("straight");
		case EScenarioTemplateSegmentType::Narrowing: return TEXT("narrowing");
		case EScenarioTemplateSegmentType::Crosswalk: return TEXT("crosswalk");
		case EScenarioTemplateSegmentType::Entrance: return TEXT("entrance");
		}
		return TEXT("straight");
	}

	FString PlacementKindToString(EScenarioTemplateObstaclePlacementKind value)
	{
		switch (value)
		{
		case EScenarioTemplateObstaclePlacementKind::Fixed: return TEXT("fixed");
		case EScenarioTemplateObstaclePlacementKind::Pattern: return TEXT("pattern");
		case EScenarioTemplateObstaclePlacementKind::Scatter: return TEXT("scatter");
		}
		return TEXT("fixed");
	}

	FString EncounterTypeToString(EScenarioTemplateEncounterType value)
	{
		switch (value)
		{
		case EScenarioTemplateEncounterType::OncomingPass: return TEXT("oncoming_pass");
		case EScenarioTemplateEncounterType::Overtake: return TEXT("overtake");
		case EScenarioTemplateEncounterType::CrossPath: return TEXT("cross_path");
		case EScenarioTemplateEncounterType::StandingGroup: return TEXT("standing_group");
		}
		return TEXT("oncoming_pass");
	}

	FString AnchorTypeToString(EScenarioTemplateRobotAnchorType value)
	{
		switch (value)
		{
		case EScenarioTemplateRobotAnchorType::Entry: return TEXT("entry");
		case EScenarioTemplateRobotAnchorType::Exit: return TEXT("exit");
		case EScenarioTemplateRobotAnchorType::CorridorPose: return TEXT("corridor_pose");
		}
		return TEXT("entry");
	}

	FString HeadingToString(EScenarioTemplateRobotHeading value)
	{
		switch (value)
		{
		case EScenarioTemplateRobotHeading::Forward: return TEXT("forward");
		case EScenarioTemplateRobotHeading::Backward: return TEXT("backward");
		case EScenarioTemplateRobotHeading::Auto: return TEXT("auto");
		}
		return TEXT("auto");
	}

	bool TryParseCorridorPlacement(
		const FJsonObject& object,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateCorridorPlacement& outPlacement)
	{
		TryReadTemplateRequiredStringField(object, TEXT("segment"), path, diagnostics, outPlacement.SegmentId);
		TryReadNumberValue(object, TEXT("along_m"), path, diagnostics, outPlacement.AlongMeters, false);
		TryReadNumberValue(object, TEXT("offset_m"), path, diagnostics, outPlacement.OffsetMeters, false);
		TryReadOptionalStringField(object, TEXT("lane"), path, diagnostics, outPlacement.LaneId);
		return true;
	}

	void ParseLaneRules(
		const FJsonObject& corridorObject,
		const FString& fieldName,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TArray<FScenarioTemplateLaneRule>& outRules)
	{
		TArray<TSharedPtr<FJsonValue>> laneValues;
		if (!TryReadArrayField(corridorObject, fieldName, path, diagnostics, laneValues, false))
		{
			return;
		}

		for (int32 index = 0; index < laneValues.Num(); ++index)
		{
			const FString lanePath = FString::Printf(TEXT("%s.%s[%d]"), *path, *fieldName, index);
			if (!laneValues[index].IsValid() || laneValues[index]->Type != EJson::Object)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_lane"), lanePath, FString::Printf(TEXT("%s must be an object."), *lanePath));
				continue;
			}

			FScenarioTemplateLaneRule laneRule;
			const TSharedPtr<FJsonObject> laneObject = laneValues[index]->AsObject();
			TryReadTemplateRequiredStringField(*laneObject, TEXT("surface"), lanePath, diagnostics, laneRule.SurfaceId);
			TryReadNumberValue(*laneObject, TEXT("width_m"), lanePath, diagnostics, laneRule.WidthMeters, true);
			outRules.Add(laneRule);
		}
	}

	void ParseSegments(
		const FJsonObject& corridorObject,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		TArray<FScenarioTemplateSegment>& outSegments)
	{
		TArray<TSharedPtr<FJsonValue>> segmentValues;
		if (!TryReadArrayField(corridorObject, TEXT("segments"), path, diagnostics, segmentValues, true))
		{
			return;
		}

		for (int32 index = 0; index < segmentValues.Num(); ++index)
		{
			const FString segmentPath = FString::Printf(TEXT("%s.segments[%d]"), *path, index);
			if (!segmentValues[index].IsValid() || segmentValues[index]->Type != EJson::Object)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_segment"), segmentPath, FString::Printf(TEXT("%s must be an object."), *segmentPath));
				continue;
			}

			FScenarioTemplateSegment segment;
			const TSharedPtr<FJsonObject> segmentObject = segmentValues[index]->AsObject();
			TryReadTemplateRequiredStringField(*segmentObject, TEXT("id"), segmentPath, diagnostics, segment.SegmentId);
			FString typeString;
			if (TryReadTemplateRequiredStringField(*segmentObject, TEXT("type"), segmentPath, diagnostics, typeString)
				&& !ParseSegmentType(typeString, segment.Type))
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_segment_type"), FString::Printf(TEXT("%s.type"), *segmentPath), FString::Printf(TEXT("%s.type has unsupported value '%s'."), *segmentPath, *typeString));
			}
			TryReadAlongRange(*segmentObject, TEXT("along_range_m"), segmentPath, diagnostics, segment.AlongRangeMeters, true);
			TryReadStringValue(*segmentObject, TEXT("replaced_by"), segmentPath, diagnostics, segment.ReplacedBySurfaceId, false);
			outSegments.Add(segment);
		}
	}

	void ParseAxis(
		const FJsonObject& corridorObject,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateAxis& outAxis)
	{
		TSharedPtr<FJsonObject> axisObject;
		if (!TryReadObjectField(corridorObject, TEXT("axis"), path, diagnostics, axisObject, true))
		{
			return;
		}

		FString typeString;
		if (TryReadTemplateRequiredStringField(*axisObject, TEXT("type"), FString::Printf(TEXT("%s.axis"), *path), diagnostics, typeString)
			&& !typeString.Equals(TEXT("polyline"), ESearchCase::IgnoreCase))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_axis_type"), FString::Printf(TEXT("%s.axis.type"), *path), TEXT("corridor.axis.type must be 'polyline'."));
		}
		TryReadVector2DArray(*axisObject, TEXT("points_m"), FString::Printf(TEXT("%s.axis"), *path), diagnostics, outAxis.PointsMeters, true);
	}

	void ParseCorridor(
		const FJsonObject& rootObject,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateCorridor& outCorridor)
	{
		TSharedPtr<FJsonObject> corridorObject;
		if (!TryReadObjectField(rootObject, TEXT("corridor"), TEXT("$"), diagnostics, corridorObject, true))
		{
			return;
		}

		ParseAxis(*corridorObject, TEXT("$.corridor"), diagnostics, outCorridor.Axis);
		TryReadNumberValue(*corridorObject, TEXT("walkway_width_m"), TEXT("$.corridor"), diagnostics, outCorridor.WalkwayWidthMeters, true);
		ParseLaneRules(*corridorObject, TEXT("building_side"), TEXT("$.corridor"), diagnostics, outCorridor.BuildingSide);
		ParseLaneRules(*corridorObject, TEXT("curb_side"), TEXT("$.corridor"), diagnostics, outCorridor.CurbSide);
		ParseSegments(*corridorObject, TEXT("$.corridor"), diagnostics, outCorridor.Segments);
	}

	void ParseObstaclePlacement(
		const FJsonObject& placementObject,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateObstaclePlacement& outPlacement)
	{
		TryReadTemplateRequiredStringField(placementObject, TEXT("id"), path, diagnostics, outPlacement.PlacementId);
		FString kindString;
		if (TryReadTemplateRequiredStringField(placementObject, TEXT("kind"), path, diagnostics, kindString)
			&& !ParsePlacementKind(kindString, outPlacement.Kind))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_placement_kind"), FString::Printf(TEXT("%s.kind"), *path), FString::Printf(TEXT("%s.kind has unsupported value '%s'."), *path, *kindString));
		}
		TryReadOptionalStringField(placementObject, TEXT("prop"), path, diagnostics, outPlacement.PropId);
		TryReadOptionalStringField(placementObject, TEXT("pattern"), path, diagnostics, outPlacement.PatternId);

		TSharedPtr<FJsonObject> atObject;
		if (TryReadObjectField(placementObject, TEXT("at"), path, diagnostics, atObject, false) && atObject.IsValid())
		{
			TryParseCorridorPlacement(*atObject, FString::Printf(TEXT("%s.at"), *path), diagnostics, outPlacement.At);
		}

		TSharedPtr<FJsonObject> zoneObject;
		if (TryReadObjectField(placementObject, TEXT("zone"), path, diagnostics, zoneObject, false) && zoneObject.IsValid())
		{
			TryReadStringArray(*zoneObject, TEXT("segments"), FString::Printf(TEXT("%s.zone"), *path), diagnostics, outPlacement.Zone.SegmentIds, false);
			TryReadStringArray(*zoneObject, TEXT("lanes"), FString::Printf(TEXT("%s.zone"), *path), diagnostics, outPlacement.Zone.LaneIds, false);
		}

		TSharedPtr<FJsonObject> paletteObject;
		if (TryReadObjectField(placementObject, TEXT("palette"), path, diagnostics, paletteObject, false) && paletteObject.IsValid())
		{
			TryReadStringArray(*paletteObject, TEXT("categories"), FString::Printf(TEXT("%s.palette"), *path), diagnostics, outPlacement.Palette.CategoryIds, false);
			TryReadStringArray(*paletteObject, TEXT("classes"), FString::Printf(TEXT("%s.palette"), *path), diagnostics, outPlacement.Palette.ClassIds, false);
		}

		TryReadIntegerValue(placementObject, TEXT("count"), path, diagnostics, outPlacement.Count, false);
		TryReadNumberValue(placementObject, TEXT("spacing_m"), path, diagnostics, outPlacement.SpacingMeters, false);
		TryReadNumberValue(placementObject, TEXT("gap_width_m"), path, diagnostics, outPlacement.GapWidthMeters, false);
		TryReadNumberValue(placementObject, TEXT("density_per_10m"), path, diagnostics, outPlacement.DensityPer10Meters, false);
		TryReadNumberValue(placementObject, TEXT("yaw_deg"), path, diagnostics, outPlacement.YawDegrees, false);
		TryReadBoolField(placementObject, TEXT("allow_blocking"), path, diagnostics, outPlacement.bAllowBlocking);
	}

	void ParseObstacles(
		const FJsonObject& rootObject,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateObstacleRules& outObstacles)
	{
		TSharedPtr<FJsonObject> obstaclesObject;
		if (!TryReadObjectField(rootObject, TEXT("obstacles"), TEXT("$"), diagnostics, obstaclesObject, false) || !obstaclesObject.IsValid())
		{
			return;
		}

		TryReadNumberValue(*obstaclesObject, TEXT("min_clear_width_m"), TEXT("$.obstacles"), diagnostics, outObstacles.MinClearWidthMeters, false);

		TArray<TSharedPtr<FJsonValue>> placementValues;
		if (!TryReadArrayField(*obstaclesObject, TEXT("placements"), TEXT("$.obstacles"), diagnostics, placementValues, false))
		{
			return;
		}
		for (int32 index = 0; index < placementValues.Num(); ++index)
		{
			const FString placementPath = FString::Printf(TEXT("$.obstacles.placements[%d]"), index);
			if (!placementValues[index].IsValid() || placementValues[index]->Type != EJson::Object)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_obstacle_placement"), placementPath, FString::Printf(TEXT("%s must be an object."), *placementPath));
				continue;
			}

			FScenarioTemplateObstaclePlacement placement;
			ParseObstaclePlacement(*placementValues[index]->AsObject(), placementPath, diagnostics, placement);
			outObstacles.Placements.Add(placement);
		}
	}

	void ParsePedestrianOverrides(
		const FJsonObject& overridesObject,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplatePedestrianBehaviorOverrides& outOverrides)
	{
		TryReadNumberValue(overridesObject, TEXT("cooperation"), path, diagnostics, outOverrides.Cooperation, false);
		TryReadNumberValue(overridesObject, TEXT("evasiveness"), path, diagnostics, outOverrides.Evasiveness, false);
		TryReadNumberValue(overridesObject, TEXT("personal_space_m"), path, diagnostics, outOverrides.PersonalSpaceMeters, false);
		TryReadNumberValue(overridesObject, TEXT("awareness_horizon_s"), path, diagnostics, outOverrides.AwarenessHorizonSeconds, false);
		TryReadNumberValue(overridesObject, TEXT("max_yield_wait_s"), path, diagnostics, outOverrides.MaxYieldWaitSeconds, false);
		TryReadNumberValue(overridesObject, TEXT("sidestep_distance_m"), path, diagnostics, outOverrides.SidestepDistanceMeters, false);
	}

	void ParsePedestrians(
		const FJsonObject& rootObject,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplatePedestrianRules& outPedestrians)
	{
		TSharedPtr<FJsonObject> pedestriansObject;
		if (!TryReadObjectField(rootObject, TEXT("pedestrians"), TEXT("$"), diagnostics, pedestriansObject, false) || !pedestriansObject.IsValid())
		{
			return;
		}

		TSharedPtr<FJsonObject> backgroundObject;
		if (TryReadObjectField(*pedestriansObject, TEXT("background"), TEXT("$.pedestrians"), diagnostics, backgroundObject, false) && backgroundObject.IsValid())
		{
			TryReadIntegerValue(*backgroundObject, TEXT("count"), TEXT("$.pedestrians.background"), diagnostics, outPedestrians.Background.Count, false);
			TryReadNumberValue(*backgroundObject, TEXT("speed_mps"), TEXT("$.pedestrians.background"), diagnostics, outPedestrians.Background.SpeedMetersPerSecond, false);

			TSharedPtr<FJsonObject> spawnZoneObject;
			if (TryReadObjectField(*backgroundObject, TEXT("spawn_zone"), TEXT("$.pedestrians.background"), diagnostics, spawnZoneObject, false) && spawnZoneObject.IsValid())
			{
				TryReadStringArray(*spawnZoneObject, TEXT("segments"), TEXT("$.pedestrians.background.spawn_zone"), diagnostics, outPedestrians.Background.SpawnSegmentIds, false);
			}
		}

		TArray<TSharedPtr<FJsonValue>> encounterValues;
		if (!TryReadArrayField(*pedestriansObject, TEXT("encounters"), TEXT("$.pedestrians"), diagnostics, encounterValues, false))
		{
			return;
		}
		for (int32 index = 0; index < encounterValues.Num(); ++index)
		{
			const FString encounterPath = FString::Printf(TEXT("$.pedestrians.encounters[%d]"), index);
			if (!encounterValues[index].IsValid() || encounterValues[index]->Type != EJson::Object)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_encounter"), encounterPath, FString::Printf(TEXT("%s must be an object."), *encounterPath));
				continue;
			}

			FScenarioTemplatePedestrianEncounter encounter;
			const TSharedPtr<FJsonObject> encounterObject = encounterValues[index]->AsObject();
			TryReadTemplateRequiredStringField(*encounterObject, TEXT("id"), encounterPath, diagnostics, encounter.EncounterId);
			FString typeString;
			if (TryReadTemplateRequiredStringField(*encounterObject, TEXT("type"), encounterPath, diagnostics, typeString)
				&& !ParseEncounterType(typeString, encounter.Type))
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_encounter_type"), FString::Printf(TEXT("%s.type"), *encounterPath), FString::Printf(TEXT("%s.type has unsupported value '%s'."), *encounterPath, *typeString));
			}
			TryReadTemplateRequiredStringField(*encounterObject, TEXT("at"), encounterPath, diagnostics, encounter.AtSegmentId);
			TryReadTemplateRequiredStringField(*encounterObject, TEXT("persona"), encounterPath, diagnostics, encounter.PersonaId);
			TryReadNumberValue(*encounterObject, TEXT("meet_offset_m"), encounterPath, diagnostics, encounter.MeetOffsetMeters, false);

			TSharedPtr<FJsonObject> overridesObject;
			if (TryReadObjectField(*encounterObject, TEXT("overrides"), encounterPath, diagnostics, overridesObject, false) && overridesObject.IsValid())
			{
				ParsePedestrianOverrides(*overridesObject, FString::Printf(TEXT("%s.overrides"), *encounterPath), diagnostics, encounter.Overrides);
			}

			outPedestrians.Encounters.Add(encounter);
		}
	}

	void ParseRobotAnchor(
		const FJsonObject& anchorObject,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateRobotAnchor& outAnchor)
	{
		FString typeString;
		if (TryReadTemplateRequiredStringField(anchorObject, TEXT("type"), path, diagnostics, typeString)
			&& !ParseAnchorType(typeString, outAnchor.Type))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_anchor_type"), FString::Printf(TEXT("%s.type"), *path), FString::Printf(TEXT("%s.type has unsupported value '%s'."), *path, *typeString));
		}

		TryReadOptionalStringField(anchorObject, TEXT("segment"), path, diagnostics, outAnchor.SegmentId);
		TryReadNumberValue(anchorObject, TEXT("along_m"), path, diagnostics, outAnchor.AlongMeters, false);
		TryReadNumberValue(anchorObject, TEXT("offset_m"), path, diagnostics, outAnchor.OffsetMeters, false);
		TryReadOptionalStringField(anchorObject, TEXT("lane"), path, diagnostics, outAnchor.LaneId);

		FString headingString;
		if (TryReadOptionalStringField(anchorObject, TEXT("heading"), path, diagnostics, headingString) && !headingString.IsEmpty()
			&& !ParseHeading(headingString, outAnchor.Heading))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_anchor_heading"), FString::Printf(TEXT("%s.heading"), *path), FString::Printf(TEXT("%s.heading has unsupported value '%s'."), *path, *headingString));
		}
	}

	void ParseRobot(
		const FJsonObject& rootObject,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		FScenarioTemplateRobot& outRobot)
	{
		TSharedPtr<FJsonObject> robotObject;
		if (!TryReadObjectField(rootObject, TEXT("robot"), TEXT("$"), diagnostics, robotObject, true))
		{
			return;
		}

		TSharedPtr<FJsonObject> startObject;
		if (TryReadObjectField(*robotObject, TEXT("start"), TEXT("$.robot"), diagnostics, startObject, true) && startObject.IsValid())
		{
			ParseRobotAnchor(*startObject, TEXT("$.robot.start"), diagnostics, outRobot.Start);
		}

		TSharedPtr<FJsonObject> goalObject;
		if (TryReadObjectField(*robotObject, TEXT("goal"), TEXT("$.robot"), diagnostics, goalObject, true) && goalObject.IsValid())
		{
			ParseRobotAnchor(*goalObject, TEXT("$.robot.goal"), diagnostics, outRobot.Goal);
		}
	}

	void ParseTemplateRoot(
		const FJsonObject& rootObject,
		FScenarioTemplateParseResult& result)
	{
		TryReadTemplateRequiredStringField(rootObject, TEXT("schema"), TEXT("$"), result.Diagnostics, result.Document.Schema);
		if (!result.Document.Schema.Equals(ScenarioTemplateSchema, ESearchCase::CaseSensitive))
		{
			AddTemplateDiagnostic(result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_schema"), TEXT("$.schema"), FString::Printf(TEXT("$.schema must be '%s'."), ScenarioTemplateSchema));
		}

		TryReadRequiredVersion(rootObject, result.Diagnostics, result.Document.Version);
		TryReadTemplateRequiredStringField(rootObject, TEXT("template_id"), TEXT("$"), result.Diagnostics, result.Document.TemplateId);
		TryReadTemplateRequiredStringField(rootObject, TEXT("intent"), TEXT("$"), result.Diagnostics, result.Document.Intent);
		ParseCorridor(rootObject, result.Diagnostics, result.Document.Corridor);
		ParseObstacles(rootObject, result.Diagnostics, result.Document.Obstacles);
		ParsePedestrians(rootObject, result.Diagnostics, result.Document.Pedestrians);
		ParseRobot(rootObject, result.Diagnostics, result.Document.Robot);
		FScenarioTemplateJson::ValidateDocument(result.Document, result.Diagnostics);
		result.bSuccess = !HasTemplateErrors(result.Diagnostics);
	}

	void ValidateNumberValue(
		const FScenarioTemplateNumberValue& value,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		bool bRequired)
	{
		if (!value.bIsSet)
		{
			if (bRequired)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("missing_number_value"), path, FString::Printf(TEXT("%s is required."), *path));
			}
			return;
		}

		if (value.Mode == EScenarioTemplateNumberValueMode::Range && value.MinValue > value.MaxValue)
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_range"), path, FString::Printf(TEXT("%s min must be <= max."), *path));
		}
	}

	void ValidateIntegerValue(
		const FScenarioTemplateIntegerValue& value,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics,
		bool bRequired)
	{
		if (!value.bIsSet)
		{
			if (bRequired)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("missing_integer_value"), path, FString::Printf(TEXT("%s is required."), *path));
			}
			return;
		}

		if (value.Mode == EScenarioTemplateNumberValueMode::Range && value.MinValue > value.MaxValue)
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_range"), path, FString::Printf(TEXT("%s min must be <= max."), *path));
		}
	}

	void ValidateStringValue(
		const FScenarioTemplateStringValue& value,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics)
	{
		if (!value.bIsSet)
		{
			return;
		}

		if (value.Mode == EScenarioTemplateStringValueMode::Fixed && IsStringEmpty(value.FixedValue))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_string_value"), path, FString::Printf(TEXT("%s must not be empty."), *path));
		}
		if (value.Mode == EScenarioTemplateStringValueMode::Choices && value.Choices.IsEmpty())
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_choices"), path, FString::Printf(TEXT("%s choices must not be empty."), *path));
		}
	}

	void ValidateSegmentRange(
		const FScenarioAlongRangeMeters& range,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics)
	{
		if (range.StartMeters > range.EndMeters)
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_along_range"), path, FString::Printf(TEXT("%s start must be <= end."), *path));
		}
	}

	const FScenarioTemplateSegment* FindSegment(
		const FScenarioTemplateDocument& document,
		const FString& segmentId)
	{
		for (const FScenarioTemplateSegment& segment : document.Corridor.Segments)
		{
			if (segment.SegmentId == segmentId)
			{
				return &segment;
			}
		}
		return nullptr;
	}

	void ValidateSegmentReference(
		const FScenarioTemplateDocument& document,
		const FString& segmentId,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics)
	{
		if (IsStringEmpty(segmentId))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("missing_segment_ref"), path, FString::Printf(TEXT("%s must reference a segment."), *path));
			return;
		}
		if (!FindSegment(document, segmentId))
		{
			AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("unknown_segment_ref"), path, FString::Printf(TEXT("%s references unknown segment '%s'."), *path, *segmentId));
		}
	}

	void ValidateRobotAnchor(
		const FScenarioTemplateDocument& document,
		const FScenarioTemplateRobotAnchor& anchor,
		const FString& path,
		TArray<FScenarioSchemaDiagnostic>& diagnostics)
	{
		if (anchor.Type != EScenarioTemplateRobotAnchorType::CorridorPose)
		{
			return;
		}

		ValidateSegmentReference(document, anchor.SegmentId, FString::Printf(TEXT("%s.segment"), *path), diagnostics);
		ValidateNumberValue(anchor.AlongMeters, FString::Printf(TEXT("%s.along_m"), *path), diagnostics, true);
		ValidateNumberValue(anchor.OffsetMeters, FString::Printf(TEXT("%s.offset_m"), *path), diagnostics, true);

		const FScenarioTemplateSegment* segment = FindSegment(document, anchor.SegmentId);
		if (segment && anchor.AlongMeters.bIsSet && anchor.AlongMeters.Mode == EScenarioTemplateNumberValueMode::Fixed)
		{
			const double along = anchor.AlongMeters.FixedValue;
			if (along < segment->AlongRangeMeters.StartMeters || along > segment->AlongRangeMeters.EndMeters)
			{
				AddTemplateDiagnostic(diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("anchor_outside_segment"), FString::Printf(TEXT("%s.along_m"), *path), FString::Printf(TEXT("%s.along_m must be inside segment '%s' along_range_m."), *path, *anchor.SegmentId));
			}
		}
	}

	TSharedPtr<FJsonValue> MakeNumberValue(const FScenarioTemplateNumberValue& value)
	{
		if (value.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
			object->SetNumberField(TEXT("min"), value.MinValue);
			object->SetNumberField(TEXT("max"), value.MaxValue);
			return MakeShared<FJsonValueObject>(object);
		}

		return MakeShared<FJsonValueNumber>(value.FixedValue);
	}

	TSharedPtr<FJsonValue> MakeIntegerValue(const FScenarioTemplateIntegerValue& value)
	{
		if (value.Mode == EScenarioTemplateNumberValueMode::Range)
		{
			TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
			object->SetNumberField(TEXT("min"), value.MinValue);
			object->SetNumberField(TEXT("max"), value.MaxValue);
			return MakeShared<FJsonValueObject>(object);
		}

		return MakeShared<FJsonValueNumber>(value.FixedValue);
	}

	TSharedPtr<FJsonValue> MakeStringValue(const FScenarioTemplateStringValue& value)
	{
		if (value.Mode == EScenarioTemplateStringValueMode::Choices)
		{
			TArray<TSharedPtr<FJsonValue>> choices;
			choices.Reserve(value.Choices.Num());
			for (const FString& choice : value.Choices)
			{
				choices.Add(MakeShared<FJsonValueString>(choice));
			}

			TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
			object->SetArrayField(TEXT("choices"), choices);
			return MakeShared<FJsonValueObject>(object);
		}

		return MakeShared<FJsonValueString>(value.FixedValue);
	}

	void SetNumberValueField(const TSharedRef<FJsonObject>& object, const FString& fieldName, const FScenarioTemplateNumberValue& value)
	{
		if (value.bIsSet)
		{
			object->SetField(fieldName, MakeNumberValue(value));
		}
	}

	void SetIntegerValueField(const TSharedRef<FJsonObject>& object, const FString& fieldName, const FScenarioTemplateIntegerValue& value)
	{
		if (value.bIsSet)
		{
			object->SetField(fieldName, MakeIntegerValue(value));
		}
	}

	void SetStringValueField(const TSharedRef<FJsonObject>& object, const FString& fieldName, const FScenarioTemplateStringValue& value)
	{
		if (value.bIsSet)
		{
			object->SetField(fieldName, MakeStringValue(value));
		}
	}

	TArray<TSharedPtr<FJsonValue>> MakeTemplateStringArray(const TArray<FString>& values)
	{
		TArray<TSharedPtr<FJsonValue>> jsonValues;
		jsonValues.Reserve(values.Num());
		for (const FString& value : values)
		{
			jsonValues.Add(MakeShared<FJsonValueString>(value));
		}
		return jsonValues;
	}

	TArray<TSharedPtr<FJsonValue>> MakeVector2DArray(const TArray<FVector2D>& points)
	{
		TArray<TSharedPtr<FJsonValue>> pointValues;
		pointValues.Reserve(points.Num());
		for (const FVector2D& point : points)
		{
			TArray<TSharedPtr<FJsonValue>> pointArray;
			pointArray.Add(MakeShared<FJsonValueNumber>(point.X));
			pointArray.Add(MakeShared<FJsonValueNumber>(point.Y));
			pointValues.Add(MakeShared<FJsonValueArray>(pointArray));
		}
		return pointValues;
	}

	TArray<TSharedPtr<FJsonValue>> MakeAlongRangeArray(const FScenarioAlongRangeMeters& range)
	{
		TArray<TSharedPtr<FJsonValue>> values;
		values.Add(MakeShared<FJsonValueNumber>(range.StartMeters));
		values.Add(MakeShared<FJsonValueNumber>(range.EndMeters));
		return values;
	}

	TSharedRef<FJsonObject> MakeCorridorPlacementObject(const FScenarioTemplateCorridorPlacement& placement)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("segment"), placement.SegmentId);
		SetNumberValueField(object, TEXT("along_m"), placement.AlongMeters);
		SetNumberValueField(object, TEXT("offset_m"), placement.OffsetMeters);
		if (!placement.LaneId.IsEmpty())
		{
			object->SetStringField(TEXT("lane"), placement.LaneId);
		}
		return object;
	}

	TSharedRef<FJsonObject> MakeCorridorObject(const FScenarioTemplateCorridor& corridor)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> axisObject = MakeShared<FJsonObject>();
		axisObject->SetStringField(TEXT("type"), TEXT("polyline"));
		axisObject->SetArrayField(TEXT("points_m"), MakeVector2DArray(corridor.Axis.PointsMeters));
		object->SetObjectField(TEXT("axis"), axisObject);
		SetNumberValueField(object, TEXT("walkway_width_m"), corridor.WalkwayWidthMeters);

		TArray<TSharedPtr<FJsonValue>> buildingSideValues;
		for (const FScenarioTemplateLaneRule& lane : corridor.BuildingSide)
		{
			TSharedRef<FJsonObject> laneObject = MakeShared<FJsonObject>();
			laneObject->SetStringField(TEXT("surface"), lane.SurfaceId);
			SetNumberValueField(laneObject, TEXT("width_m"), lane.WidthMeters);
			buildingSideValues.Add(MakeShared<FJsonValueObject>(laneObject));
		}
		object->SetArrayField(TEXT("building_side"), buildingSideValues);

		TArray<TSharedPtr<FJsonValue>> curbSideValues;
		for (const FScenarioTemplateLaneRule& lane : corridor.CurbSide)
		{
			TSharedRef<FJsonObject> laneObject = MakeShared<FJsonObject>();
			laneObject->SetStringField(TEXT("surface"), lane.SurfaceId);
			SetNumberValueField(laneObject, TEXT("width_m"), lane.WidthMeters);
			curbSideValues.Add(MakeShared<FJsonValueObject>(laneObject));
		}
		object->SetArrayField(TEXT("curb_side"), curbSideValues);

		TArray<TSharedPtr<FJsonValue>> segmentValues;
		for (const FScenarioTemplateSegment& segment : corridor.Segments)
		{
			TSharedRef<FJsonObject> segmentObject = MakeShared<FJsonObject>();
			segmentObject->SetStringField(TEXT("id"), segment.SegmentId);
			segmentObject->SetStringField(TEXT("type"), SegmentTypeToString(segment.Type));
			segmentObject->SetArrayField(TEXT("along_range_m"), MakeAlongRangeArray(segment.AlongRangeMeters));
			SetStringValueField(segmentObject, TEXT("replaced_by"), segment.ReplacedBySurfaceId);
			segmentValues.Add(MakeShared<FJsonValueObject>(segmentObject));
		}
		object->SetArrayField(TEXT("segments"), segmentValues);
		return object;
	}

	TSharedRef<FJsonObject> MakeObstaclesObject(const FScenarioTemplateObstacleRules& obstacles)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		SetNumberValueField(object, TEXT("min_clear_width_m"), obstacles.MinClearWidthMeters);

		TArray<TSharedPtr<FJsonValue>> placementValues;
		for (const FScenarioTemplateObstaclePlacement& placement : obstacles.Placements)
		{
			TSharedRef<FJsonObject> placementObject = MakeShared<FJsonObject>();
			placementObject->SetStringField(TEXT("id"), placement.PlacementId);
			placementObject->SetStringField(TEXT("kind"), PlacementKindToString(placement.Kind));
			if (!placement.PropId.IsEmpty())
			{
				placementObject->SetStringField(TEXT("prop"), placement.PropId);
			}
			if (!placement.PatternId.IsEmpty())
			{
				placementObject->SetStringField(TEXT("pattern"), placement.PatternId);
			}
			if (!placement.At.SegmentId.IsEmpty())
			{
				placementObject->SetObjectField(TEXT("at"), MakeCorridorPlacementObject(placement.At));
			}
			if (!placement.Zone.SegmentIds.IsEmpty() || !placement.Zone.LaneIds.IsEmpty())
			{
				TSharedRef<FJsonObject> zoneObject = MakeShared<FJsonObject>();
				zoneObject->SetArrayField(TEXT("segments"), MakeTemplateStringArray(placement.Zone.SegmentIds));
				zoneObject->SetArrayField(TEXT("lanes"), MakeTemplateStringArray(placement.Zone.LaneIds));
				placementObject->SetObjectField(TEXT("zone"), zoneObject);
			}
			if (!placement.Palette.CategoryIds.IsEmpty() || !placement.Palette.ClassIds.IsEmpty())
			{
				TSharedRef<FJsonObject> paletteObject = MakeShared<FJsonObject>();
				paletteObject->SetArrayField(TEXT("categories"), MakeTemplateStringArray(placement.Palette.CategoryIds));
				paletteObject->SetArrayField(TEXT("classes"), MakeTemplateStringArray(placement.Palette.ClassIds));
				placementObject->SetObjectField(TEXT("palette"), paletteObject);
			}
			SetIntegerValueField(placementObject, TEXT("count"), placement.Count);
			SetNumberValueField(placementObject, TEXT("spacing_m"), placement.SpacingMeters);
			SetNumberValueField(placementObject, TEXT("gap_width_m"), placement.GapWidthMeters);
			SetNumberValueField(placementObject, TEXT("density_per_10m"), placement.DensityPer10Meters);
			SetNumberValueField(placementObject, TEXT("yaw_deg"), placement.YawDegrees);
			placementObject->SetBoolField(TEXT("allow_blocking"), placement.bAllowBlocking);
			placementValues.Add(MakeShared<FJsonValueObject>(placementObject));
		}
		object->SetArrayField(TEXT("placements"), placementValues);
		return object;
	}

	TSharedRef<FJsonObject> MakePedestrianOverridesObject(const FScenarioTemplatePedestrianBehaviorOverrides& overrides)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		SetNumberValueField(object, TEXT("cooperation"), overrides.Cooperation);
		SetNumberValueField(object, TEXT("evasiveness"), overrides.Evasiveness);
		SetNumberValueField(object, TEXT("personal_space_m"), overrides.PersonalSpaceMeters);
		SetNumberValueField(object, TEXT("awareness_horizon_s"), overrides.AwarenessHorizonSeconds);
		SetNumberValueField(object, TEXT("max_yield_wait_s"), overrides.MaxYieldWaitSeconds);
		SetNumberValueField(object, TEXT("sidestep_distance_m"), overrides.SidestepDistanceMeters);
		return object;
	}

	bool HasPedestrianOverrides(const FScenarioTemplatePedestrianBehaviorOverrides& overrides)
	{
		return overrides.Cooperation.bIsSet
			|| overrides.Evasiveness.bIsSet
			|| overrides.PersonalSpaceMeters.bIsSet
			|| overrides.AwarenessHorizonSeconds.bIsSet
			|| overrides.MaxYieldWaitSeconds.bIsSet
			|| overrides.SidestepDistanceMeters.bIsSet;
	}

	TSharedRef<FJsonObject> MakePedestriansObject(const FScenarioTemplatePedestrianRules& pedestrians)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> backgroundObject = MakeShared<FJsonObject>();
		SetIntegerValueField(backgroundObject, TEXT("count"), pedestrians.Background.Count);
		SetNumberValueField(backgroundObject, TEXT("speed_mps"), pedestrians.Background.SpeedMetersPerSecond);
		if (!pedestrians.Background.SpawnSegmentIds.IsEmpty())
		{
			TSharedRef<FJsonObject> spawnZoneObject = MakeShared<FJsonObject>();
			spawnZoneObject->SetArrayField(TEXT("segments"), MakeTemplateStringArray(pedestrians.Background.SpawnSegmentIds));
			backgroundObject->SetObjectField(TEXT("spawn_zone"), spawnZoneObject);
		}
		object->SetObjectField(TEXT("background"), backgroundObject);

		TArray<TSharedPtr<FJsonValue>> encounterValues;
		for (const FScenarioTemplatePedestrianEncounter& encounter : pedestrians.Encounters)
		{
			TSharedRef<FJsonObject> encounterObject = MakeShared<FJsonObject>();
			encounterObject->SetStringField(TEXT("id"), encounter.EncounterId);
			encounterObject->SetStringField(TEXT("type"), EncounterTypeToString(encounter.Type));
			encounterObject->SetStringField(TEXT("at"), encounter.AtSegmentId);
			encounterObject->SetStringField(TEXT("persona"), encounter.PersonaId);
			SetNumberValueField(encounterObject, TEXT("meet_offset_m"), encounter.MeetOffsetMeters);
			if (HasPedestrianOverrides(encounter.Overrides))
			{
				encounterObject->SetObjectField(TEXT("overrides"), MakePedestrianOverridesObject(encounter.Overrides));
			}
			encounterValues.Add(MakeShared<FJsonValueObject>(encounterObject));
		}
		object->SetArrayField(TEXT("encounters"), encounterValues);
		return object;
	}

	TSharedRef<FJsonObject> MakeRobotAnchorObject(const FScenarioTemplateRobotAnchor& anchor)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("type"), AnchorTypeToString(anchor.Type));
		if (anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
		{
			object->SetStringField(TEXT("segment"), anchor.SegmentId);
			SetNumberValueField(object, TEXT("along_m"), anchor.AlongMeters);
			SetNumberValueField(object, TEXT("offset_m"), anchor.OffsetMeters);
		}
		if (!anchor.LaneId.IsEmpty())
		{
			object->SetStringField(TEXT("lane"), anchor.LaneId);
		}
		if (anchor.Heading != EScenarioTemplateRobotHeading::Auto)
		{
			object->SetStringField(TEXT("heading"), HeadingToString(anchor.Heading));
		}
		return object;
	}

	TSharedRef<FJsonObject> MakeRobotObject(const FScenarioTemplateRobot& robot)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetObjectField(TEXT("start"), MakeRobotAnchorObject(robot.Start));
		object->SetObjectField(TEXT("goal"), MakeRobotAnchorObject(robot.Goal));
		return object;
	}

	TSharedRef<FJsonObject> MakeTemplateObject(const FScenarioTemplateDocument& document)
	{
		TSharedRef<FJsonObject> object = MakeShared<FJsonObject>();
		object->SetStringField(TEXT("schema"), document.Schema);
		object->SetNumberField(TEXT("version"), document.Version);
		object->SetStringField(TEXT("template_id"), document.TemplateId);
		object->SetStringField(TEXT("intent"), document.Intent);
		object->SetObjectField(TEXT("corridor"), MakeCorridorObject(document.Corridor));
		object->SetObjectField(TEXT("obstacles"), MakeObstaclesObject(document.Obstacles));
		object->SetObjectField(TEXT("pedestrians"), MakePedestriansObject(document.Pedestrians));
		object->SetObjectField(TEXT("robot"), MakeRobotObject(document.Robot));
		return object;
	}
}

FScenarioTemplateParseResult FScenarioTemplateJson::ParseFromFile(const FString& JsonFilePath)
{
	FScenarioTemplateParseResult result;
	if (JsonFilePath.TrimStartAndEnd().IsEmpty())
	{
		AddTemplateDiagnostic(result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_template_path"), TEXT("$"), TEXT("Scenario template file path must not be empty."));
		return result;
	}

	const FString resolvedPath = ResolveScenarioSchemaPath(JsonFilePath);
	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedPath))
	{
		AddTemplateDiagnostic(result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("template_file_read_failed"), TEXT("$"), FString::Printf(TEXT("Scenario template JSON read failed: %s"), *resolvedPath));
		return result;
	}

	return ParseFromString(jsonString);
}

FScenarioTemplateParseResult FScenarioTemplateJson::ParseFromString(const FString& JsonString)
{
	FScenarioTemplateParseResult result;
	if (JsonString.TrimStartAndEnd().IsEmpty())
	{
		AddTemplateDiagnostic(result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_template_json"), TEXT("$"), TEXT("Scenario template JSON must not be empty."));
		return result;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		AddTemplateDiagnostic(result.Diagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_template_json"), TEXT("$"), TEXT("Scenario template JSON parse failed."));
		return result;
	}

	ParseTemplateRoot(*rootObject, result);
	return result;
}

bool FScenarioTemplateJson::ValidateDocument(
	const FScenarioTemplateDocument& Document,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	if (!Document.Schema.Equals(ScenarioTemplateSchema, ESearchCase::CaseSensitive))
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("invalid_schema"), TEXT("$.schema"), FString::Printf(TEXT("$.schema must be '%s'."), ScenarioTemplateSchema));
	}
	if (Document.Version != SupportedVersion)
	{
		AddTemplateDiagnostic(
			OutDiagnostics,
			EScenarioSchemaDiagnosticSeverity::Error,
			TEXT("unsupported_schema_version"),
			TEXT("$.version"),
			FString::Printf(TEXT("scenario_template version must match the current compiler and validator version: %d."), SupportedVersion));
	}
	if (IsStringEmpty(Document.TemplateId))
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_template_id"), TEXT("$.template_id"), TEXT("$.template_id must not be empty."));
	}
	if (IsStringEmpty(Document.Intent))
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_intent"), TEXT("$.intent"), TEXT("$.intent must not be empty."));
	}
	if (Document.Corridor.Axis.PointsMeters.Num() < 2)
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("too_few_axis_points"), TEXT("$.corridor.axis.points_m"), TEXT("$.corridor.axis.points_m must contain at least two points."));
	}
	ValidateNumberValue(Document.Corridor.WalkwayWidthMeters, TEXT("$.corridor.walkway_width_m"), OutDiagnostics, true);

	TSet<FString> segmentIds;
	for (int32 index = 0; index < Document.Corridor.Segments.Num(); ++index)
	{
		const FScenarioTemplateSegment& segment = Document.Corridor.Segments[index];
		const FString segmentPath = FString::Printf(TEXT("$.corridor.segments[%d]"), index);
		if (IsStringEmpty(segment.SegmentId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_segment_id"), FString::Printf(TEXT("%s.id"), *segmentPath), FString::Printf(TEXT("%s.id must not be empty."), *segmentPath));
		}
		else if (segmentIds.Contains(segment.SegmentId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("duplicate_segment_id"), FString::Printf(TEXT("%s.id"), *segmentPath), FString::Printf(TEXT("Duplicate corridor segment id '%s'."), *segment.SegmentId));
		}
		else
		{
			segmentIds.Add(segment.SegmentId);
		}
		ValidateSegmentRange(segment.AlongRangeMeters, FString::Printf(TEXT("%s.along_range_m"), *segmentPath), OutDiagnostics);
		ValidateStringValue(segment.ReplacedBySurfaceId, FString::Printf(TEXT("%s.replaced_by"), *segmentPath), OutDiagnostics);
	}

	for (int32 index = 0; index < Document.Corridor.BuildingSide.Num(); ++index)
	{
		const FScenarioTemplateLaneRule& lane = Document.Corridor.BuildingSide[index];
		if (IsStringEmpty(lane.SurfaceId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_surface"), FString::Printf(TEXT("$.corridor.building_side[%d].surface"), index), TEXT("Building-side lane surface must not be empty."));
		}
		ValidateNumberValue(lane.WidthMeters, FString::Printf(TEXT("$.corridor.building_side[%d].width_m"), index), OutDiagnostics, true);
	}

	for (int32 index = 0; index < Document.Corridor.CurbSide.Num(); ++index)
	{
		const FScenarioTemplateLaneRule& lane = Document.Corridor.CurbSide[index];
		if (IsStringEmpty(lane.SurfaceId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_surface"), FString::Printf(TEXT("$.corridor.curb_side[%d].surface"), index), TEXT("Curb-side lane surface must not be empty."));
		}
		ValidateNumberValue(lane.WidthMeters, FString::Printf(TEXT("$.corridor.curb_side[%d].width_m"), index), OutDiagnostics, true);
	}

	ValidateNumberValue(Document.Obstacles.MinClearWidthMeters, TEXT("$.obstacles.min_clear_width_m"), OutDiagnostics, false);
	TSet<FString> placementIds;
	for (int32 index = 0; index < Document.Obstacles.Placements.Num(); ++index)
	{
		const FScenarioTemplateObstaclePlacement& placement = Document.Obstacles.Placements[index];
		const FString placementPath = FString::Printf(TEXT("$.obstacles.placements[%d]"), index);
		if (IsStringEmpty(placement.PlacementId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_placement_id"), FString::Printf(TEXT("%s.id"), *placementPath), FString::Printf(TEXT("%s.id must not be empty."), *placementPath));
		}
		else if (placementIds.Contains(placement.PlacementId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("duplicate_placement_id"), FString::Printf(TEXT("%s.id"), *placementPath), FString::Printf(TEXT("Duplicate obstacle placement id '%s'."), *placement.PlacementId));
		}
		else
		{
			placementIds.Add(placement.PlacementId);
		}

		if (placement.Kind == EScenarioTemplateObstaclePlacementKind::Fixed || placement.Kind == EScenarioTemplateObstaclePlacementKind::Pattern)
		{
			if (IsStringEmpty(placement.PropId))
			{
				AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("missing_prop"), FString::Printf(TEXT("%s.prop"), *placementPath), FString::Printf(TEXT("%s.prop is required for fixed and pattern placement."), *placementPath));
			}
			ValidateSegmentReference(Document, placement.At.SegmentId, FString::Printf(TEXT("%s.at.segment"), *placementPath), OutDiagnostics);
			ValidateNumberValue(placement.At.AlongMeters, FString::Printf(TEXT("%s.at.along_m"), *placementPath), OutDiagnostics, true);
			ValidateNumberValue(placement.At.OffsetMeters, FString::Printf(TEXT("%s.at.offset_m"), *placementPath), OutDiagnostics, true);
		}
		else
		{
			for (int32 segmentIndex = 0; segmentIndex < placement.Zone.SegmentIds.Num(); ++segmentIndex)
			{
				ValidateSegmentReference(Document, placement.Zone.SegmentIds[segmentIndex], FString::Printf(TEXT("%s.zone.segments[%d]"), *placementPath, segmentIndex), OutDiagnostics);
			}
			ValidateNumberValue(placement.DensityPer10Meters, FString::Printf(TEXT("%s.density_per_10m"), *placementPath), OutDiagnostics, true);
		}

		ValidateIntegerValue(placement.Count, FString::Printf(TEXT("%s.count"), *placementPath), OutDiagnostics, false);
		ValidateNumberValue(placement.SpacingMeters, FString::Printf(TEXT("%s.spacing_m"), *placementPath), OutDiagnostics, false);
		ValidateNumberValue(placement.GapWidthMeters, FString::Printf(TEXT("%s.gap_width_m"), *placementPath), OutDiagnostics, false);
		ValidateNumberValue(placement.YawDegrees, FString::Printf(TEXT("%s.yaw_deg"), *placementPath), OutDiagnostics, false);
	}

	ValidateIntegerValue(Document.Pedestrians.Background.Count, TEXT("$.pedestrians.background.count"), OutDiagnostics, false);
	ValidateNumberValue(Document.Pedestrians.Background.SpeedMetersPerSecond, TEXT("$.pedestrians.background.speed_mps"), OutDiagnostics, false);
	TSet<FString> encounterIds;
	for (int32 index = 0; index < Document.Pedestrians.Encounters.Num(); ++index)
	{
		const FScenarioTemplatePedestrianEncounter& encounter = Document.Pedestrians.Encounters[index];
		const FString encounterPath = FString::Printf(TEXT("$.pedestrians.encounters[%d]"), index);
		if (IsStringEmpty(encounter.EncounterId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_encounter_id"), FString::Printf(TEXT("%s.id"), *encounterPath), FString::Printf(TEXT("%s.id must not be empty."), *encounterPath));
		}
		else if (encounterIds.Contains(encounter.EncounterId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("duplicate_encounter_id"), FString::Printf(TEXT("%s.id"), *encounterPath), FString::Printf(TEXT("Duplicate pedestrian encounter id '%s'."), *encounter.EncounterId));
		}
		else
		{
			encounterIds.Add(encounter.EncounterId);
		}
		ValidateSegmentReference(Document, encounter.AtSegmentId, FString::Printf(TEXT("%s.at"), *encounterPath), OutDiagnostics);
		if (IsStringEmpty(encounter.PersonaId))
		{
			AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_persona"), FString::Printf(TEXT("%s.persona"), *encounterPath), FString::Printf(TEXT("%s.persona must not be empty."), *encounterPath));
		}
		ValidateNumberValue(encounter.MeetOffsetMeters, FString::Printf(TEXT("%s.meet_offset_m"), *encounterPath), OutDiagnostics, false);
		ValidateNumberValue(encounter.Overrides.Cooperation, FString::Printf(TEXT("%s.overrides.cooperation"), *encounterPath), OutDiagnostics, false);
		ValidateNumberValue(encounter.Overrides.Evasiveness, FString::Printf(TEXT("%s.overrides.evasiveness"), *encounterPath), OutDiagnostics, false);
		ValidateNumberValue(encounter.Overrides.PersonalSpaceMeters, FString::Printf(TEXT("%s.overrides.personal_space_m"), *encounterPath), OutDiagnostics, false);
		ValidateNumberValue(encounter.Overrides.AwarenessHorizonSeconds, FString::Printf(TEXT("%s.overrides.awareness_horizon_s"), *encounterPath), OutDiagnostics, false);
		ValidateNumberValue(encounter.Overrides.MaxYieldWaitSeconds, FString::Printf(TEXT("%s.overrides.max_yield_wait_s"), *encounterPath), OutDiagnostics, false);
		ValidateNumberValue(encounter.Overrides.SidestepDistanceMeters, FString::Printf(TEXT("%s.overrides.sidestep_distance_m"), *encounterPath), OutDiagnostics, false);
	}

	ValidateRobotAnchor(Document, Document.Robot.Start, TEXT("$.robot.start"), OutDiagnostics);
	ValidateRobotAnchor(Document, Document.Robot.Goal, TEXT("$.robot.goal"), OutDiagnostics);

	return !HasTemplateErrors(OutDiagnostics);
}

bool FScenarioTemplateJson::TryWriteJson(
	const FScenarioTemplateDocument& Document,
	FString& OutJson,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutJson.Reset();
	OutDiagnostics.Reset();
	if (!ValidateDocument(Document, OutDiagnostics))
	{
		return false;
	}

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(MakeTemplateObject(Document), writer))
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("template_json_serialize_failed"), TEXT("$"), TEXT("Scenario template JSON serialization failed."));
		return false;
	}

	return true;
}

bool FScenarioTemplateJson::SaveToFile(
	const FScenarioTemplateDocument& Document,
	const FString& JsonFilePath,
	TArray<FScenarioSchemaDiagnostic>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (JsonFilePath.TrimStartAndEnd().IsEmpty())
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("empty_template_output_path"), TEXT("$"), TEXT("Scenario template output path must not be empty."));
		return false;
	}

	FString json;
	if (!TryWriteJson(Document, json, OutDiagnostics))
	{
		return false;
	}

	const FString resolvedPath = ResolveScenarioSchemaPath(JsonFilePath);
	const FString outputDirectory = FPaths::GetPath(resolvedPath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true))
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("template_output_directory_failed"), TEXT("$"), FString::Printf(TEXT("Scenario template output directory create failed: %s"), *outputDirectory));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(json, *resolvedPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddTemplateDiagnostic(OutDiagnostics, EScenarioSchemaDiagnosticSeverity::Error, TEXT("template_file_write_failed"), TEXT("$"), FString::Printf(TEXT("Scenario template file write failed: %s"), *resolvedPath));
		return false;
	}

	return true;
}
