
#include "Episode/EpisodeCompiler.h"

#include "Dom/JsonObject.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UEpisodeCompiler::AddDiagnostic(
		FEpisodeCompileResult& result,
		EEpisodeCompileDiagnosticSeverity severity,
		const FString& code,
		const FString& message)
	{
		FEpisodeCompileDiagnostic Diagnostic;
		Diagnostic.Severity = severity;
		Diagnostic.Code = code;
		Diagnostic.Message = message;
		result.Diagnostics.Add(Diagnostic);
	}

bool UEpisodeCompiler::HasErrors(const FEpisodeCompileResult& result)
	{
		for (const FEpisodeCompileDiagnostic& Diagnostic : result.Diagnostics)
		{
			if (Diagnostic.Severity == EEpisodeCompileDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

FEpisodeParamValue UEpisodeCompiler::MakeBoolParam(bool value)
	{
		FEpisodeParamValue Param;
		Param.Type = EEpisodeParamValueType::Bool;
		Param.BoolValue = value;
		return Param;
	}

FEpisodeParamValue UEpisodeCompiler::MakeFloatParam(double value)
	{
		FEpisodeParamValue Param;
		Param.Type = EEpisodeParamValueType::Float;
		Param.FloatValue = value;
		return Param;
	}

FEpisodeParamValue UEpisodeCompiler::MakeStringParam(const FString& value)
	{
		FEpisodeParamValue Param;
		Param.Type = EEpisodeParamValueType::String;
		Param.StringValue = value;
		return Param;
	}

FEpisodeParamValue UEpisodeCompiler::MakeVectorParam(const FVector& value)
	{
		FEpisodeParamValue Param;
		Param.Type = EEpisodeParamValueType::Vector;
		Param.VectorValue = value;
		return Param;
	}

bool UEpisodeCompiler::TryGetObjectField(const FJsonObject& jsonObject, const FString& fieldName, TSharedPtr<FJsonObject>& outObject)
	{
		const TSharedPtr<FJsonValue> JsonValue = jsonObject.TryGetField(fieldName);
		if (!JsonValue.IsValid() || JsonValue->Type != EJson::Object)
		{
			return false;
		}

		outObject = JsonValue->AsObject();
		return outObject.IsValid();
	}

bool UEpisodeCompiler::TryGetArrayField(const FJsonObject& jsonObject, const FString& fieldName, TArray<TSharedPtr<FJsonValue>>& outArray)
	{
		const TSharedPtr<FJsonValue> JsonValue = jsonObject.TryGetField(fieldName);
		if (!JsonValue.IsValid() || JsonValue->Type != EJson::Array)
		{
			return false;
		}

		outArray = JsonValue->AsArray();
		return true;
	}

bool UEpisodeCompiler::TryGetStringField(const FJsonObject& jsonObject, const FString& fieldName, FString& outValue)
	{
		return jsonObject.TryGetStringField(fieldName, outValue);
	}

bool UEpisodeCompiler::TryGetStringField(const FJsonObject& jsonObject, const FString& primaryFieldName, const FString& fallbackFieldName, FString& outValue)
	{
		return TryGetStringField(jsonObject, primaryFieldName, outValue)
			|| TryGetStringField(jsonObject, fallbackFieldName, outValue);
	}

bool UEpisodeCompiler::RequireStringField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		FEpisodeCompileResult& result,
		FString& outValue)
	{
		if (TryGetStringField(jsonObject, fieldName, outValue) && !outValue.IsEmpty())
		{
			return true;
		}

		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("missing_string"), FString::Printf(TEXT("%s.%s is required."), *path, *fieldName));
		return false;
	}

bool UEpisodeCompiler::RequireStringField(
		const FJsonObject& jsonObject,
		const FString& primaryFieldName,
		const FString& fallbackFieldName,
		const FString& path,
		FEpisodeCompileResult& result,
		FString& outValue)
	{
		if (TryGetStringField(jsonObject, primaryFieldName, fallbackFieldName, outValue) && !outValue.IsEmpty())
		{
			return true;
		}

		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("missing_string"),
			FString::Printf(TEXT("%s.%s is required."), *path, *primaryFieldName));
		return false;
	}

double UEpisodeCompiler::ReadNumberOrDefault(const FJsonObject& jsonObject, const FString& fieldName, double defaultValue)
	{
		double Value = defaultValue;
		jsonObject.TryGetNumberField(fieldName, Value);
		return Value;
	}

bool UEpisodeCompiler::ReadBoolOrDefault(const FJsonObject& jsonObject, const FString& fieldName, bool defaultValue)
	{
		bool Value = defaultValue;
		jsonObject.TryGetBoolField(fieldName, Value);
		return Value;
	}

bool UEpisodeCompiler::ReadNumberArray(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		int32 expectedCount,
		const FString& path,
		FEpisodeCompileResult& result,
		TArray<double>& outValues)
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		if (!TryGetArrayField(jsonObject, fieldName, JsonArray))
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("missing_number_array"),
				FString::Printf(TEXT("%s.%s must be an array of %d numbers."), *path, *fieldName, expectedCount));
			return false;
		}

		if (JsonArray.Num() != expectedCount)
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("invalid_array_length"),
				FString::Printf(TEXT("%s.%s must contain %d numbers."), *path, *fieldName, expectedCount));
			return false;
		}

		outValues.Reset(expectedCount);
		for (int32 Index = 0; Index < JsonArray.Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& JsonValue = JsonArray[Index];
			if (!JsonValue.IsValid() || JsonValue->Type != EJson::Number)
			{
				AddDiagnostic(
					result,
					EEpisodeCompileDiagnosticSeverity::Error,
					TEXT("invalid_number_array"),
					FString::Printf(TEXT("%s.%s[%d] must be a number."), *path, *fieldName, Index));
				return false;
			}

			outValues.Add(JsonValue->AsNumber());
		}

		return true;
	}

bool UEpisodeCompiler::ReadVectorField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		double scale,
		const FString& path,
		FEpisodeCompileResult& result,
		FVector& outVector)
	{
		TArray<double> Values;
		if (!ReadNumberArray(jsonObject, fieldName, 3, path, result, Values))
		{
			return false;
		}

		outVector = FVector(Values[0] * scale, Values[1] * scale, Values[2] * scale);
		return true;
	}

bool UEpisodeCompiler::ReadVector2DField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		double scale,
		const FString& path,
		FEpisodeCompileResult& result,
		FVector2D& outVector)
	{
		TArray<double> Values;
		if (!ReadNumberArray(jsonObject, fieldName, 2, path, result, Values))
		{
			return false;
		}

		outVector = FVector2D(Values[0] * scale, Values[1] * scale);
		return true;
	}

bool UEpisodeCompiler::ReadRotatorField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		FEpisodeCompileResult& result,
		FRotator& outRotator)
	{
		const TSharedPtr<FJsonValue> JsonValue = jsonObject.TryGetField(fieldName);
		if (!JsonValue.IsValid())
		{
			outRotator = FRotator::ZeroRotator;
			return true;
		}

		if (JsonValue->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> RotationObject = JsonValue->AsObject();
			if (!RotationObject.IsValid())
			{
				return false;
			}

			outRotator = FRotator(
				ReadNumberOrDefault(*RotationObject, TEXT("pitch"), 0.0),
				ReadNumberOrDefault(*RotationObject, TEXT("yaw"), 0.0),
				ReadNumberOrDefault(*RotationObject, TEXT("roll"), 0.0));
			return true;
		}

		if (JsonValue->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> RotationArray = JsonValue->AsArray();
			if (RotationArray.Num() != 3)
			{
				AddDiagnostic(
					result,
					EEpisodeCompileDiagnosticSeverity::Error,
					TEXT("invalid_rotation"),
					FString::Printf(TEXT("%s.%s must contain 3 numbers."), *path, *fieldName));
				return false;
			}

			for (int32 Index = 0; Index < RotationArray.Num(); ++Index)
			{
				if (!RotationArray[Index].IsValid() || RotationArray[Index]->Type != EJson::Number)
				{
					AddDiagnostic(
						result,
						EEpisodeCompileDiagnosticSeverity::Error,
						TEXT("invalid_rotation"),
						FString::Printf(TEXT("%s.%s[%d] must be a number."), *path, *fieldName, Index));
					return false;
				}
			}

			const double Roll = RotationArray[0]->AsNumber();
			const double Pitch = RotationArray[1]->AsNumber();
			const double Yaw = RotationArray[2]->AsNumber();
			outRotator = FRotator(Pitch, Yaw, Roll);
			return true;
		}

		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("invalid_rotation"),
			FString::Printf(TEXT("%s.%s must be an object or [roll, pitch, yaw] array."), *path, *fieldName));
		return false;
	}

bool UEpisodeCompiler::ReadTransformField(
		const FJsonObject& jsonObject,
		const FString& fieldName,
		const FString& path,
		FEpisodeCompileResult& result,
		FTransform& outTransform)
	{
		TSharedPtr<FJsonObject> TransformObject;
		if (!TryGetObjectField(jsonObject, fieldName, TransformObject))
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Warning,
				TEXT("missing_transform"),
				FString::Printf(TEXT("%s.%s is missing. Identity transform will be used."), *path, *fieldName));
			outTransform = FTransform::Identity;
			return true;
		}

		FVector Location = FVector::ZeroVector;
		if (TransformObject->HasField(TEXT("location_m")))
		{
			ReadVectorField(*TransformObject, TEXT("location_m"), MetersToCentimeters, FString::Printf(TEXT("%s.%s"), *path, *fieldName), result, Location);
		}

		FRotator Rotation = FRotator::ZeroRotator;
		ReadRotatorField(*TransformObject, TEXT("rotation_deg"), FString::Printf(TEXT("%s.%s"), *path, *fieldName), result, Rotation);

		FVector Scale = FVector::OneVector;
		if (TransformObject->HasField(TEXT("scale")))
		{
			ReadVectorField(*TransformObject, TEXT("scale"), 1.0, FString::Printf(TEXT("%s.%s"), *path, *fieldName), result, Scale);
		}

		outTransform = FTransform(Rotation, Location, Scale);
		return true;
	}

bool UEpisodeCompiler::ParseGroundRegionType(const FString& value, EEpisodeGroundRegionType& outType)
	{
		const FString Normalized = value.ToLower();
		if (Normalized == TEXT("walkable"))
		{
			outType = EEpisodeGroundRegionType::Walkable;
			return true;
		}

		if (Normalized == TEXT("penalty"))
		{
			outType = EEpisodeGroundRegionType::Penalty;
			return true;
		}

		if (Normalized == TEXT("blocked"))
		{
			outType = EEpisodeGroundRegionType::Blocked;
			return true;
		}

		return false;
	}

bool UEpisodeCompiler::ParseGroundShapeType(const FString& value, EEpisodeGroundShapeType& outType)
	{
		const FString Normalized = value.ToLower();
		if (Normalized == TEXT("rectangle"))
		{
			outType = EEpisodeGroundShapeType::Rectangle;
			return true;
		}

		if (Normalized == TEXT("convex_polygon"))
		{
			outType = EEpisodeGroundShapeType::ConvexPolygon;
			return true;
		}

		return false;
	}

bool UEpisodeCompiler::ParsePathType(const FString& value, EEpisodePathType& outType)
	{
		const FString Normalized = value.ToLower();
		if (Normalized == TEXT("spline"))
		{
			outType = EEpisodePathType::Spline;
			return true;
		}

		if (Normalized == TEXT("waypoints"))
		{
			outType = EEpisodePathType::Waypoints;
			return true;
		}

		return false;
	}

bool UEpisodeCompiler::AddUniqueId(TSet<FString>& ids, const FString& id, const FString& path, FEpisodeCompileResult& result)
	{
		if (id.IsEmpty())
		{
			return false;
		}

		if (ids.Contains(id))
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("duplicate_id"),
				FString::Printf(TEXT("%s has duplicate id '%s'."), *path, *id));
			return false;
		}

		ids.Add(id);
		return true;
	}

void UEpisodeCompiler::AddJsonProperties(const FJsonObject& sourceObject, TMap<FString, FEpisodeParamValue>& outProperties)
	{
		TSharedPtr<FJsonObject> PropertiesObject;
		if (!TryGetObjectField(sourceObject, TEXT("properties"), PropertiesObject))
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : PropertiesObject->Values)
		{
			if (!Pair.Value.IsValid())
			{
				continue;
			}

			switch (Pair.Value->Type)
			{
			case EJson::Boolean:
				outProperties.Add(Pair.Key, MakeBoolParam(Pair.Value->AsBool()));
				break;
			case EJson::Number:
				outProperties.Add(Pair.Key, MakeFloatParam(Pair.Value->AsNumber()));
				break;
			case EJson::String:
				outProperties.Add(Pair.Key, MakeStringParam(Pair.Value->AsString()));
				break;
			case EJson::Array:
			{
				const TArray<TSharedPtr<FJsonValue>> ArrayValue = Pair.Value->AsArray();
				if (ArrayValue.Num() == 3
					&& ArrayValue[0].IsValid() && ArrayValue[0]->Type == EJson::Number
					&& ArrayValue[1].IsValid() && ArrayValue[1]->Type == EJson::Number
					&& ArrayValue[2].IsValid() && ArrayValue[2]->Type == EJson::Number)
				{
					outProperties.Add(Pair.Key, MakeVectorParam(FVector(ArrayValue[0]->AsNumber(), ArrayValue[1]->AsNumber(), ArrayValue[2]->AsNumber())));
				}
				break;
			}
			default:
				break;
			}
		}
	}

void UEpisodeCompiler::CompileRunConfig(const FJsonObject& rootObject, FEpisodeCompileResult& result)
	{
		FString ScenarioId;
		if (TryGetStringField(rootObject, TEXT("scenario_id"), ScenarioId))
		{
			result.WorldSpec.RunConfig.TemplateId = ScenarioId;
		}

		result.WorldSpec.RunConfig.TemplateVersion = FMath::RoundToInt(ReadNumberOrDefault(rootObject, TEXT("version"), 1.0));

		TSharedPtr<FJsonObject> RunObject;
		if (!TryGetObjectField(rootObject, TEXT("run"), RunObject))
		{
			return;
		}

		result.WorldSpec.RunConfig.BaseSeed = static_cast<int64>(ReadNumberOrDefault(*RunObject, TEXT("base_seed"), 0.0));
		result.WorldSpec.RunConfig.IterationIndex = FMath::RoundToInt(ReadNumberOrDefault(*RunObject, TEXT("iteration_index"), 0.0));

		if (RunObject->HasField(TEXT("time_limit_s")))
		{
			result.WorldSpec.RunConfig.Parameters.Add(TEXT("time_limit_s"), MakeFloatParam(ReadNumberOrDefault(*RunObject, TEXT("time_limit_s"), 0.0)));
		}

		const int64 BaseSeed = result.WorldSpec.RunConfig.BaseSeed;
		result.WorldSpec.Seeds.WorldSeed = BaseSeed;
		result.WorldSpec.Seeds.LayoutSeed = BaseSeed + 101;
		result.WorldSpec.Seeds.StaticObstacleSeed = BaseSeed + 202;
		result.WorldSpec.Seeds.DynamicActorSeed = BaseSeed + 303;
		result.WorldSpec.Seeds.EventSeed = BaseSeed + 404;
		result.WorldSpec.Seeds.WeatherSeed = BaseSeed + 505;
		result.WorldSpec.Seeds.PolicySeed = BaseSeed + 606;
	}

void UEpisodeCompiler::CompileGroundRegions(const FJsonObject& rootObject, FEpisodeCompileResult& result)
	{
		TSharedPtr<FJsonObject> GroundModelObject;
		if (!TryGetObjectField(rootObject, TEXT("ground_model"), GroundModelObject))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Warning, TEXT("missing_ground_model"), TEXT("ground_model is missing."));
			return;
		}

		TArray<TSharedPtr<FJsonValue>> Regions;
		if (!TryGetArrayField(*GroundModelObject, TEXT("regions"), Regions))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Warning, TEXT("missing_ground_regions"), TEXT("ground_model.regions is missing."));
			return;
		}

		TSet<FString> RegionIds;
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			const FString RegionPath = FString::Printf(TEXT("ground_model.regions[%d]"), Index);
			if (!Regions[Index].IsValid() || Regions[Index]->Type != EJson::Object)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_ground_region"), FString::Printf(TEXT("%s must be an object."), *RegionPath));
				continue;
			}

			const TSharedPtr<FJsonObject> RegionObject = Regions[Index]->AsObject();
			FEpisodeGroundRegionSpec RegionSpec;
			if (!RequireStringField(*RegionObject, TEXT("region_id"), RegionPath, result, RegionSpec.RegionId))
			{
				continue;
			}
			AddUniqueId(RegionIds, RegionSpec.RegionId, RegionPath, result);

			FString RegionTypeString;
			if (RequireStringField(*RegionObject, TEXT("region_type"), TEXT("type"), RegionPath, result, RegionTypeString)
				&& !ParseGroundRegionType(RegionTypeString, RegionSpec.RegionType))
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_ground_type"), FString::Printf(TEXT("%s.region_type '%s' is not supported."), *RegionPath, *RegionTypeString));
			}

			TSharedPtr<FJsonObject> ShapeObject;
			if (!TryGetObjectField(*RegionObject, TEXT("shape"), ShapeObject))
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("missing_shape"), FString::Printf(TEXT("%s.shape is required."), *RegionPath));
				continue;
			}

			FString ShapeTypeString;
			if (RequireStringField(*ShapeObject, TEXT("type"), TEXT("shape_type"), FString::Printf(TEXT("%s.shape"), *RegionPath), result, ShapeTypeString)
				&& !ParseGroundShapeType(ShapeTypeString, RegionSpec.ShapeType))
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_shape_type"), FString::Printf(TEXT("%s.shape.type '%s' is not supported."), *RegionPath, *ShapeTypeString));
			}

			if (RegionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("unsupported_shape"), FString::Printf(TEXT("%s only supports rectangle ground regions in MVP."), *RegionPath));
			}

			ReadVectorField(*ShapeObject, TEXT("center_m"), MetersToCentimeters, FString::Printf(TEXT("%s.shape"), *RegionPath), result, RegionSpec.Center);
			ReadVector2DField(*ShapeObject, TEXT("size_m"), MetersToCentimeters, FString::Printf(TEXT("%s.shape"), *RegionPath), result, RegionSpec.Size);
			RegionSpec.YawDegrees = ReadNumberOrDefault(*ShapeObject, TEXT("yaw_deg"), 0.0);
			RegionSpec.TraversabilityScore = ReadNumberOrDefault(*RegionObject, TEXT("traversability_score"), RegionSpec.TraversabilityScore);

			TSharedPtr<FJsonObject> PenaltyObject;
			if (TryGetObjectField(*RegionObject, TEXT("penalty"), PenaltyObject))
			{
				TryGetStringField(*PenaltyObject, TEXT("kind"), RegionSpec.PenaltyKind);
				RegionSpec.PenaltyCost = ReadNumberOrDefault(*PenaltyObject, TEXT("cost"), RegionSpec.PenaltyCost);
				RegionSpec.ViolationAfterSeconds = ReadNumberOrDefault(*PenaltyObject, TEXT("violation_after_s"), RegionSpec.ViolationAfterSeconds);
			}

			TryGetStringField(*RegionObject, TEXT("collision_tag"), RegionSpec.CollisionTag);
			result.WorldSpec.GroundRegions.Add(RegionSpec);
		}
	}

void UEpisodeCompiler::CompilePaths(const FJsonObject& rootObject, FEpisodeCompileResult& result, TSet<FString>& outPathIds)
	{
		TArray<TSharedPtr<FJsonValue>> Paths;
		if (!TryGetArrayField(rootObject, TEXT("paths"), Paths))
		{
			return;
		}

		for (int32 Index = 0; Index < Paths.Num(); ++Index)
		{
			const FString Path = FString::Printf(TEXT("paths[%d]"), Index);
			if (!Paths[Index].IsValid() || Paths[Index]->Type != EJson::Object)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path"), FString::Printf(TEXT("%s must be an object."), *Path));
				continue;
			}

			const TSharedPtr<FJsonObject> PathObject = Paths[Index]->AsObject();
			FEpisodePathSpec PathSpec;
			if (!RequireStringField(*PathObject, TEXT("path_id"), Path, result, PathSpec.PathId))
			{
				continue;
			}
			AddUniqueId(outPathIds, PathSpec.PathId, Path, result);

			FString PathTypeString;
			if (TryGetStringField(*PathObject, TEXT("type"), PathTypeString)
				&& !ParsePathType(PathTypeString, PathSpec.PathType))
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_type"), FString::Printf(TEXT("%s.type '%s' is not supported."), *Path, *PathTypeString));
			}

			TArray<TSharedPtr<FJsonValue>> PointValues;
			if (!TryGetArrayField(*PathObject, TEXT("points_m"), PointValues))
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("missing_path_points"), FString::Printf(TEXT("%s.points_m is required."), *Path));
				continue;
			}

			for (int32 PointIndex = 0; PointIndex < PointValues.Num(); ++PointIndex)
			{
				const FString PointPath = FString::Printf(TEXT("%s.points_m[%d]"), *Path, PointIndex);
				if (!PointValues[PointIndex].IsValid() || PointValues[PointIndex]->Type != EJson::Array)
				{
					AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"), FString::Printf(TEXT("%s must be a [x, y, z] array."), *PointPath));
					continue;
				}

				const TArray<TSharedPtr<FJsonValue>> PointArray = PointValues[PointIndex]->AsArray();
				if (PointArray.Num() != 3)
				{
					AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"), FString::Printf(TEXT("%s must contain 3 numbers."), *PointPath));
					continue;
				}

				bool bValidPoint = true;
				for (const TSharedPtr<FJsonValue>& PointComponent : PointArray)
				{
					bValidPoint &= PointComponent.IsValid() && PointComponent->Type == EJson::Number;
				}

				if (!bValidPoint)
				{
					AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"), FString::Printf(TEXT("%s must contain only numbers."), *PointPath));
					continue;
				}

				PathSpec.Points.Add(FVector(PointArray[0]->AsNumber() * MetersToCentimeters, PointArray[1]->AsNumber() * MetersToCentimeters, PointArray[2]->AsNumber() * MetersToCentimeters));
			}

			PathSpec.bClosedLoop = ReadBoolOrDefault(*PathObject, TEXT("closed_loop"), false);
			if (PathSpec.Points.Num() < 2)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("too_few_path_points"), FString::Printf(TEXT("%s requires at least 2 points."), *Path));
			}

			result.WorldSpec.Paths.Add(PathSpec);
		}
	}

void UEpisodeCompiler::CompileStaticObstacles(
		const FJsonObject& actorsObject,
		FEpisodeCompileResult& result,
		TSet<FString>& instanceIds)
	{
		TArray<TSharedPtr<FJsonValue>> StaticObstacles;
		if (!TryGetArrayField(actorsObject, TEXT("static_obstacles"), StaticObstacles))
		{
			return;
		}

		for (int32 Index = 0; Index < StaticObstacles.Num(); ++Index)
		{
			const FString ObstaclePath = FString::Printf(TEXT("actors.static_obstacles[%d]"), Index);
			if (!StaticObstacles[Index].IsValid() || StaticObstacles[Index]->Type != EJson::Object)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_static_obstacle"), FString::Printf(TEXT("%s must be an object."), *ObstaclePath));
				continue;
			}

			const TSharedPtr<FJsonObject> ObstacleObject = StaticObstacles[Index]->AsObject();
			FEpisodePlaceableInstanceSpec PlaceableSpec;
			if (!RequireStringField(*ObstacleObject, TEXT("instance_id"), ObstaclePath, result, PlaceableSpec.InstanceId))
			{
				continue;
			}
			AddUniqueId(instanceIds, PlaceableSpec.InstanceId, ObstaclePath, result);

			if (!RequireStringField(*ObstacleObject, TEXT("prop_id"), TEXT("asset_id"), ObstaclePath, result, PlaceableSpec.AssetId))
			{
				continue;
			}

			FEpisodeStaticObstaclePropEntry PropEntry;
			if (!AEpisodeStaticObstacle::FindDefaultPropEntryById(FName(*PlaceableSpec.AssetId), PropEntry))
			{
				AddDiagnostic(
					result,
					EEpisodeCompileDiagnosticSeverity::Error,
					TEXT("unknown_static_obstacle_prop"),
					FString::Printf(TEXT("%s.prop_id '%s' is not in the default static obstacle catalog."), *ObstaclePath, *PlaceableSpec.AssetId));
			}

			PlaceableSpec.Category = EEpisodeActorCategory::StaticObstacle;
			PlaceableSpec.MobilityMode = EEpisodeMobilityMode::Static;
			ReadTransformField(*ObstacleObject, TEXT("transform"), ObstaclePath, result, PlaceableSpec.Transform);
			AddJsonProperties(*ObstacleObject, PlaceableSpec.Properties);
			result.WorldSpec.Placeables.Add(PlaceableSpec);
		}
	}

void UEpisodeCompiler::CompilePedestrians(
		const FJsonObject& actorsObject,
		FEpisodeCompileResult& result,
		const TSet<FString>& pathIds,
		TSet<FString>& instanceIds)
	{
		TArray<TSharedPtr<FJsonValue>> Pedestrians;
		if (!TryGetArrayField(actorsObject, TEXT("pedestrians"), Pedestrians))
		{
			return;
		}

		for (int32 Index = 0; Index < Pedestrians.Num(); ++Index)
		{
			const FString PedestrianPath = FString::Printf(TEXT("actors.pedestrians[%d]"), Index);
			if (!Pedestrians[Index].IsValid() || Pedestrians[Index]->Type != EJson::Object)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_pedestrian"), FString::Printf(TEXT("%s must be an object."), *PedestrianPath));
				continue;
			}

			const TSharedPtr<FJsonObject> PedestrianObject = Pedestrians[Index]->AsObject();
			FEpisodeDynamicActorSpec DynamicActorSpec;
			if (!RequireStringField(*PedestrianObject, TEXT("instance_id"), PedestrianPath, result, DynamicActorSpec.InstanceId))
			{
				continue;
			}
			AddUniqueId(instanceIds, DynamicActorSpec.InstanceId, PedestrianPath, result);

			if (!TryGetStringField(*PedestrianObject, TEXT("archetype_id"), DynamicActorSpec.AssetId))
			{
				DynamicActorSpec.AssetId = TEXT("adult_pedestrian");
			}

			RequireStringField(*PedestrianObject, TEXT("path_id"), PedestrianPath, result, DynamicActorSpec.PathId);
			if (!DynamicActorSpec.PathId.IsEmpty() && !pathIds.Contains(DynamicActorSpec.PathId))
			{
				AddDiagnostic(
					result,
					EEpisodeCompileDiagnosticSeverity::Error,
					TEXT("unknown_path"),
					FString::Printf(TEXT("%s.path_id '%s' does not match a compiled path."), *PedestrianPath, *DynamicActorSpec.PathId));
			}

			DynamicActorSpec.Category = EEpisodeActorCategory::Pedestrian;
			DynamicActorSpec.MobilityMode = EEpisodeMobilityMode::Moving;
			DynamicActorSpec.SpawnTimeSeconds = ReadNumberOrDefault(*PedestrianObject, TEXT("spawn_time_s"), 0.0);
			ReadTransformField(*PedestrianObject, TEXT("transform"), PedestrianPath, result, DynamicActorSpec.InitialTransform);
			AddJsonProperties(*PedestrianObject, DynamicActorSpec.Properties);

			TSharedPtr<FJsonObject> MovementObject;
			if (TryGetObjectField(*PedestrianObject, TEXT("movement"), MovementObject))
			{
				FString MovementModel;
				if (TryGetStringField(*MovementObject, TEXT("model"), MovementModel))
				{
					DynamicActorSpec.Properties.Add(TEXT("movement_model"), MakeStringParam(MovementModel));
				}

				if (MovementObject->HasField(TEXT("speed_mps")))
				{
					const double SpeedMps = ReadNumberOrDefault(*MovementObject, TEXT("speed_mps"), 1.2);
					DynamicActorSpec.Properties.Add(TEXT("speed_mps"), MakeFloatParam(SpeedMps));
					DynamicActorSpec.Properties.Add(TEXT("speed_cm_per_second"), MakeFloatParam(SpeedMps * MetersToCentimeters));
				}

				if (MovementObject->HasField(TEXT("initial_distance_m")))
				{
					const double InitialDistanceM = ReadNumberOrDefault(*MovementObject, TEXT("initial_distance_m"), 0.0);
					DynamicActorSpec.Properties.Add(TEXT("initial_distance_m"), MakeFloatParam(InitialDistanceM));
					DynamicActorSpec.Properties.Add(TEXT("initial_distance_cm"), MakeFloatParam(InitialDistanceM * MetersToCentimeters));
				}

				if (MovementObject->HasField(TEXT("auto_start")))
				{
					DynamicActorSpec.Properties.Add(TEXT("auto_start"), MakeBoolParam(ReadBoolOrDefault(*MovementObject, TEXT("auto_start"), true)));
				}
			}

			result.WorldSpec.DynamicActors.Add(DynamicActorSpec);
		}
	}

void UEpisodeCompiler::CompileRobotSpawn(
		const FJsonObject& actorsObject,
		FEpisodeCompileResult& result,
		TSet<FString>& instanceIds)
	{
		TSharedPtr<FJsonObject> RobotObject;
		if (!TryGetObjectField(actorsObject, TEXT("robot"), RobotObject))
		{
			return;
		}

		FEpisodePlaceableInstanceSpec RobotSpec;
		if (!RequireStringField(*RobotObject, TEXT("instance_id"), TEXT("actor_id"), TEXT("actors.robot"), result, RobotSpec.InstanceId))
		{
			return;
		}
		AddUniqueId(instanceIds, RobotSpec.InstanceId, TEXT("actors.robot"), result);

		if (!RequireStringField(*RobotObject, TEXT("asset_id"), TEXT("type"), TEXT("actors.robot"), result, RobotSpec.AssetId))
		{
			return;
		}

		RobotSpec.Category = EEpisodeActorCategory::RoadVehicle;
		RobotSpec.MobilityMode = EEpisodeMobilityMode::Parked;
		ReadTransformField(*RobotObject, TEXT("transform"), TEXT("actors.robot"), result, RobotSpec.Transform);
		RobotSpec.Properties.Add(TEXT("spawn_only"), MakeBoolParam(ReadBoolOrDefault(*RobotObject, TEXT("spawn_only"), true)));
		AddJsonProperties(*RobotObject, RobotSpec.Properties);
		result.WorldSpec.Placeables.Add(RobotSpec);
	}

void UEpisodeCompiler::CompileActors(
		const FJsonObject& rootObject,
		FEpisodeCompileResult& result,
		const TSet<FString>& pathIds)
	{
		TSharedPtr<FJsonObject> ActorsObject;
		if (!TryGetObjectField(rootObject, TEXT("actors"), ActorsObject))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Warning, TEXT("missing_actors"), TEXT("actors is missing."));
			return;
		}

		TSet<FString> InstanceIds;
		CompileStaticObstacles(*ActorsObject, result, InstanceIds);
		CompilePedestrians(*ActorsObject, result, pathIds, InstanceIds);
		CompileRobotSpawn(*ActorsObject, result, InstanceIds);
	}

void UEpisodeCompiler::CompileRootObject(const FJsonObject& rootObject, const FString& sourceJson, FEpisodeCompileResult& result)
	{
		CompileRunConfig(rootObject, result);
		CompileGroundRegions(rootObject, result);

		TSet<FString> PathIds;
		CompilePaths(rootObject, result, PathIds);
		CompileActors(rootObject, result, PathIds);

		result.WorldSpec.SpecHash = FString::Printf(TEXT("%u"), GetTypeHash(sourceJson));
	}

FEpisodeCompileResult UEpisodeCompiler::CompileEpisodeWorldSpecFromJsonFile(const FString& JsonFilePath) const
{
	FEpisodeCompileResult Result;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		AddDiagnostic(
			Result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("file_read_failed"),
			FString::Printf(TEXT("Failed to read JSON file '%s'."), *JsonFilePath));
		Result.bSuccess = false;
		return Result;
	}

	return CompileEpisodeWorldSpecFromJsonString(JsonString);
}

FEpisodeCompileResult UEpisodeCompiler::CompileEpisodeWorldSpecFromJsonString(const FString& JsonString) const
{
	FEpisodeCompileResult Result;

	if (JsonString.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(Result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("empty_json"), TEXT("JSON input is empty."));
		Result.bSuccess = false;
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddDiagnostic(Result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_json"), TEXT("JSON input could not be parsed."));
		Result.bSuccess = false;
		return Result;
	}

	CompileRootObject(*RootObject, JsonString, Result);
	Result.bSuccess = !HasErrors(Result);
	return Result;
}
