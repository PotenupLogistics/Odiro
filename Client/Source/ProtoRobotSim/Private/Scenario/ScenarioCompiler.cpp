
#include "Scenario/ScenarioCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ResolveScenarioJsonFilePath(const FString& jsonFilePath)
	{
		if (jsonFilePath.IsEmpty()) return jsonFilePath;

		if (FPaths::IsRelative(jsonFilePath))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), jsonFilePath));
		}

		return jsonFilePath;
	}
}

UScenarioCompiler::UScenarioCompiler()
{
	StaticObstaclePropCatalog = UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference();
}

void UScenarioCompiler::AddDiagnostic(
	FScenarioCompileResult& result,
	EScenarioCompileDiagnosticSeverity severity,
	const FString& code,
	const FString& message)
{
	FScenarioCompileDiagnostic diagnostic;
	diagnostic.Severity = severity;
	diagnostic.Code = code;
	diagnostic.Message = message;
	result.Diagnostics.Add(diagnostic);
}

bool UScenarioCompiler::HasErrors(const FScenarioCompileResult& result)
{
	for (const FScenarioCompileDiagnostic& diagnostic : result.Diagnostics)
	{
		if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error) return true;
	}

	return false;
}

FScenarioParamValue UScenarioCompiler::MakeBoolParam(bool value)
{
	FScenarioParamValue param;
	param.Type = EScenarioParamValueType::Bool;
	param.BoolValue = value;
	return param;
}

FScenarioParamValue UScenarioCompiler::MakeFloatParam(double value)
{
	FScenarioParamValue param;
	param.Type = EScenarioParamValueType::Float;
	param.FloatValue = value;
	return param;
}

FScenarioParamValue UScenarioCompiler::MakeStringParam(const FString& value)
{
	FScenarioParamValue param;
	param.Type = EScenarioParamValueType::String;
	param.StringValue = value;
	return param;
}

FScenarioParamValue UScenarioCompiler::MakeVectorParam(const FVector& value)
{
	FScenarioParamValue param;
	param.Type = EScenarioParamValueType::Vector;
	param.VectorValue = value;
	return param;
}

bool UScenarioCompiler::TryGetObjectField(const FJsonObject& jsonObject, const FString& fieldName,
                                         TSharedPtr<FJsonObject>& outObject)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid() || jsonValue->Type != EJson::Object) return false;

	outObject = jsonValue->AsObject();
	return outObject.IsValid();
}

bool UScenarioCompiler::TryGetArrayField(const FJsonObject& jsonObject, const FString& fieldName,
                                        TArray<TSharedPtr<FJsonValue>>& outArray)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid() || jsonValue->Type != EJson::Array) return false;

	outArray = jsonValue->AsArray();
	return true;
}

bool UScenarioCompiler::TryGetStringField(const FJsonObject& jsonObject, const FString& fieldName, FString& outValue)
{
	return jsonObject.TryGetStringField(fieldName, outValue);
}

bool UScenarioCompiler::TryGetStringField(const FJsonObject& jsonObject, const FString& primaryFieldName,
                                         const FString& fallbackFieldName, FString& outValue)
{
	return TryGetStringField(jsonObject, primaryFieldName, outValue)
		|| TryGetStringField(jsonObject, fallbackFieldName, outValue);
}

bool UScenarioCompiler::RequireStringField(
	const FJsonObject& jsonObject,
	const FString& fieldName,
	const FString& path,
	FScenarioCompileResult& result,
	FString& outValue)
{
	if (TryGetStringField(jsonObject, fieldName, outValue) && !outValue.IsEmpty()) return true;

	AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("missing_string"),
	              FString::Printf(TEXT("%s.%s 필드는 필수."), *path, *fieldName));
	return false;
}

bool UScenarioCompiler::RequireStringField(
	const FJsonObject& jsonObject,
	const FString& primaryFieldName,
	const FString& fallbackFieldName,
	const FString& path,
	FScenarioCompileResult& result,
	FString& outValue)
{
	if (TryGetStringField(jsonObject, primaryFieldName, fallbackFieldName, outValue) && !outValue.IsEmpty()) return true;

	AddDiagnostic(
		result,
		EScenarioCompileDiagnosticSeverity::Error,
		TEXT("missing_string"),
		FString::Printf(TEXT("%s.%s 필드는 필수."), *path, *primaryFieldName));
	return false;
}

double UScenarioCompiler::ReadNumberOrDefault(const FJsonObject& jsonObject, const FString& fieldName,
                                             double defaultValue)
{
	double value = defaultValue;
	jsonObject.TryGetNumberField(fieldName, value);
	return value;
}

bool UScenarioCompiler::ReadBoolOrDefault(const FJsonObject& jsonObject, const FString& fieldName, bool defaultValue)
{
	bool value = defaultValue;
	jsonObject.TryGetBoolField(fieldName, value);
	return value;
}

bool UScenarioCompiler::ReadNumberArray(
	const FJsonObject& jsonObject,
	const FString& fieldName,
	int32 expectedCount,
	const FString& path,
	FScenarioCompileResult& result,
	TArray<double>& outValues)
{
	TArray<TSharedPtr<FJsonValue>> jsonArray;
	if (!TryGetArrayField(jsonObject, fieldName, jsonArray))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("missing_number_array"),
			FString::Printf(TEXT("%s.%s 필드는 숫자 %d개로 이루어진 배열이어야 함."), *path, *fieldName, expectedCount));
		return false;
	}

	if (jsonArray.Num() != expectedCount)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_array_length"),
			FString::Printf(TEXT("%s.%s 필드는 숫자 %d개를 포함해야 함."), *path, *fieldName, expectedCount));
		return false;
	}

	outValues.Reset(expectedCount);
	for (int32 index = 0; index < jsonArray.Num(); ++index)
	{
		const TSharedPtr<FJsonValue>& jsonValue = jsonArray[index];
		if (!jsonValue.IsValid() || jsonValue->Type != EJson::Number)
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("invalid_number_array"),
				FString::Printf(TEXT("%s.%s[%d] 값은 숫자여야 함."), *path, *fieldName, index));
			return false;
		}

		outValues.Add(jsonValue->AsNumber());
	}

	return true;
}

bool UScenarioCompiler::ReadVector2DField(
	const FJsonObject& jsonObject,
	const FString& fieldName,
	double scale,
	const FString& path,
	FScenarioCompileResult& result,
	FVector2D& outVector)
{
	TArray<double> values;
	if (!ReadNumberArray(jsonObject, fieldName, 2, path, result, values)) return false;

	outVector = FVector2D(values[0] * scale, values[1] * scale);
	return true;
}

bool UScenarioCompiler::ReadVector2DAsVectorField(
	const FJsonObject& jsonObject,
	const FString& fieldName,
	double scale,
	const FString& path,
	FScenarioCompileResult& result,
	FVector& outVector)
{
	FVector2D vector2D;
	if (!ReadVector2DField(jsonObject, fieldName, scale, path, result, vector2D)) return false;

	outVector = FVector(vector2D.X, vector2D.Y, 0.0);
	return true;
}

bool UScenarioCompiler::ReadActorPlacementTransform(
	const FJsonObject& jsonObject,
	const FString& path,
	FScenarioCompileResult& result,
	FTransform& outTransform)
{
	outTransform = FTransform::Identity;

	if (jsonObject.HasField(TEXT("transform")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("legacy_transform_unsupported"),
			FString::Printf(TEXT("%s.transform은 더 이상 지원하지 않음. xy_m/yaw_deg를 사용해야 함."), *path));
		return false;
	}

	FVector location = FVector::ZeroVector;
	if (jsonObject.HasField(TEXT("xy_m")))
	{
		if (!ReadVector2DAsVectorField(jsonObject, TEXT("xy_m"), MetersToCentimeters, path, result, location))
		{
			return false;
		}
	}

	double yawDegrees = 0.0;
	if (jsonObject.HasField(TEXT("yaw_deg")) && !jsonObject.TryGetNumberField(TEXT("yaw_deg"), yawDegrees))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_number"),
			FString::Printf(TEXT("%s.yaw_deg 필드는 숫자여야 함."), *path));
		return false;
	}

	outTransform = FTransform(FRotator(0.0, yawDegrees, 0.0), location, FVector::OneVector);
	return true;
}

bool UScenarioCompiler::ParseGroundRegionType(const FString& value, EScenarioGroundRegionType& outType)
{
	const FString normalized = value.ToLower();
	if (normalized == TEXT("walkable"))
	{
		outType = EScenarioGroundRegionType::Walkable;
		return true;
	}

	if (normalized == TEXT("penalty"))
	{
		outType = EScenarioGroundRegionType::Penalty;
		return true;
	}

	if (normalized == TEXT("blocked"))
	{
		outType = EScenarioGroundRegionType::Blocked;
		return true;
	}

	return false;
}

bool UScenarioCompiler::ParseGroundShapeType(const FString& value, EScenarioGroundShapeType& outType)
{
	const FString normalized = value.ToLower();
	if (normalized == TEXT("rectangle"))
	{
		outType = EScenarioGroundShapeType::Rectangle;
		return true;
	}

	if (normalized == TEXT("convex_polygon"))
	{
		outType = EScenarioGroundShapeType::ConvexPolygon;
		return true;
	}

	return false;
}

bool UScenarioCompiler::AddUniqueId(TSet<FString>& ids, const FString& id, const FString& path,
                                   FScenarioCompileResult& result)
{
	if (id.IsEmpty()) return false;

	if (ids.Contains(id))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("duplicate_id"),
			FString::Printf(TEXT("%s에 중복 id '%s'가 있음."), *path, *id));
		return false;
	}

	ids.Add(id);
	return true;
}

void UScenarioCompiler::AddJsonProperties(const FJsonObject& sourceObject,
                                         TMap<FString, FScenarioParamValue>& outProperties)
{
	TSharedPtr<FJsonObject> propertiesObject;
	if (!TryGetObjectField(sourceObject, TEXT("properties"), propertiesObject)) return;

	for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : propertiesObject->Values)
	{
		if (!pair.Value.IsValid()) continue;

		switch (pair.Value->Type)
		{
		case EJson::Boolean:
			outProperties.Add(pair.Key, MakeBoolParam(pair.Value->AsBool()));
			break;
		case EJson::Number:
			outProperties.Add(pair.Key, MakeFloatParam(pair.Value->AsNumber()));
			break;
		case EJson::String:
			outProperties.Add(pair.Key, MakeStringParam(pair.Value->AsString()));
			break;
		case EJson::Array:
			{
				const TArray<TSharedPtr<FJsonValue>> arrayValue = pair.Value->AsArray();
				if (arrayValue.Num() == 3
					&& arrayValue[0].IsValid() && arrayValue[0]->Type == EJson::Number
					&& arrayValue[1].IsValid() && arrayValue[1]->Type == EJson::Number
					&& arrayValue[2].IsValid() && arrayValue[2]->Type == EJson::Number)
				{
					outProperties.Add(pair.Key, MakeVectorParam(
						                  FVector(arrayValue[0]->AsNumber(), arrayValue[1]->AsNumber(),
						                          arrayValue[2]->AsNumber())));
				}
				break;
			}
		default:
			break;
		}
	}
}

void UScenarioCompiler::CompileRunConfig(const FJsonObject& rootObject, FScenarioCompileResult& result)
{
	FString scenarioId;
	if (TryGetStringField(rootObject, TEXT("scenario_id"), scenarioId))
	{
		result.WorldSpec.RunConfig.TemplateId = scenarioId;
	}

	result.WorldSpec.RunConfig.TemplateVersion = FMath::RoundToInt(
		ReadNumberOrDefault(rootObject, TEXT("version"), 1.0));

	TSharedPtr<FJsonObject> runObject;
	if (!TryGetObjectField(rootObject, TEXT("run"), runObject)) return;

	result.WorldSpec.RunConfig.BaseSeed = static_cast<int64>(ReadNumberOrDefault(*runObject, TEXT("base_seed"), 0.0));
	result.WorldSpec.RunConfig.IterationIndex = FMath::RoundToInt(
		ReadNumberOrDefault(*runObject, TEXT("iteration_index"), 0.0));

	if (runObject->HasField(TEXT("time_limit_s")))
	{
		result.WorldSpec.RunConfig.Parameters.Add(
			TEXT("time_limit_s"), MakeFloatParam(ReadNumberOrDefault(*runObject, TEXT("time_limit_s"), 0.0)));
	}

	const int64 baseSeed = result.WorldSpec.RunConfig.BaseSeed;
	result.WorldSpec.Seeds.WorldSeed = baseSeed;
	result.WorldSpec.Seeds.LayoutSeed = baseSeed + 101;
	result.WorldSpec.Seeds.StaticObstacleSeed = baseSeed + 202;
	result.WorldSpec.Seeds.DynamicActorSeed = baseSeed + 303;
	result.WorldSpec.Seeds.EventSeed = baseSeed + 404;
	result.WorldSpec.Seeds.PolicySeed = baseSeed + 505;
}

void UScenarioCompiler::CompileEvaluationConfig(const FJsonObject& rootObject, FScenarioCompileResult& result)
{
	FScenarioEvaluationConfig& evaluationConfig = result.WorldSpec.EvaluationConfig;

	TSharedPtr<FJsonObject> evaluationObject;
	if (!TryGetObjectField(rootObject, TEXT("evaluation"), evaluationObject)) return;

	evaluationConfig.GoalAcceptanceRadiusCm = FMath::Max(
		0.0,
		ReadNumberOrDefault(
			*evaluationObject,
			TEXT("goal_acceptance_radius_m"),
			evaluationConfig.GoalAcceptanceRadiusCm / MetersToCentimeters) * MetersToCentimeters);
	evaluationConfig.TipOverAngleDegrees = FMath::Max(
		0.0,
		ReadNumberOrDefault(*evaluationObject, TEXT("tip_over_angle_deg"), evaluationConfig.TipOverAngleDegrees));

	TSharedPtr<FJsonObject> nearMissObject;
	if (TryGetObjectField(*evaluationObject, TEXT("near_miss"), nearMissObject))
	{
		evaluationConfig.NearMissDistanceCm = FMath::Max(
			0.0,
			ReadNumberOrDefault(
				*nearMissObject,
				TEXT("distance_m"),
				evaluationConfig.NearMissDistanceCm / MetersToCentimeters) * MetersToCentimeters);
	}

	TSharedPtr<FJsonObject> scoringObject;
	if (TryGetObjectField(*evaluationObject, TEXT("scoring"), scoringObject))
	{
		evaluationConfig.StaticObstacleCollisionScore = ReadNumberOrDefault(
			*scoringObject,
			TEXT("static_obstacle_collision"),
			evaluationConfig.StaticObstacleCollisionScore);
		evaluationConfig.BlockedRegionCollisionScore = ReadNumberOrDefault(
			*scoringObject,
			TEXT("blocked_region_collision"),
			evaluationConfig.BlockedRegionCollisionScore);
		evaluationConfig.PenaltyRegionViolationScore = ReadNumberOrDefault(
			*scoringObject,
			TEXT("penalty_region_violation"),
			evaluationConfig.PenaltyRegionViolationScore);
		evaluationConfig.PedestrianNearMissScore = ReadNumberOrDefault(
			*scoringObject,
			TEXT("pedestrian_near_miss"),
			evaluationConfig.PedestrianNearMissScore);
		evaluationConfig.PedestrianCollisionScore = ReadNumberOrDefault(
			*scoringObject,
			TEXT("pedestrian_collision"),
			evaluationConfig.PedestrianCollisionScore);
	}
}

void UScenarioCompiler::CompileGroundRegions(const FJsonObject& rootObject, FScenarioCompileResult& result)
{
	TSharedPtr<FJsonObject> groundModelObject;
	if (!TryGetObjectField(rootObject, TEXT("ground_model"), groundModelObject))
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Warning, TEXT("missing_ground_model"),
		              TEXT("ground_model 필드가 없음."));
		return;
	}

	TArray<TSharedPtr<FJsonValue>> regions;
	if (!TryGetArrayField(*groundModelObject, TEXT("regions"), regions))
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Warning, TEXT("missing_ground_regions"),
		              TEXT("ground_model.regions 필드가 없음."));
		return;
	}

	TSet<FString> regionIds;
	for (int32 index = 0; index < regions.Num(); ++index)
	{
		const FString regionPath = FString::Printf(TEXT("ground_model.regions[%d]"), index);
		if (!regions[index].IsValid() || regions[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_ground_region"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *regionPath));
			continue;
		}

		const TSharedPtr<FJsonObject> regionObject = regions[index]->AsObject();
		FScenarioGroundRegionSpec regionSpec;
		if (!RequireStringField(*regionObject, TEXT("region_id"), regionPath, result, regionSpec.RegionId)) continue;
		AddUniqueId(regionIds, regionSpec.RegionId, regionPath, result);

		FString regionTypeString;
		if (RequireStringField(*regionObject, TEXT("region_type"), TEXT("type"), regionPath, result, regionTypeString)
			&& !ParseGroundRegionType(regionTypeString, regionSpec.RegionType))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_ground_type"),
			              FString::Printf(TEXT("%s.region_type '%s' 값은 지원하지 않음."), *regionPath, *regionTypeString));
		}

		TSharedPtr<FJsonObject> shapeObject;
		if (!TryGetObjectField(*regionObject, TEXT("shape"), shapeObject))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("missing_shape"),
			              FString::Printf(TEXT("%s.shape 필드는 필수."), *regionPath));
			continue;
		}

		FString shapeTypeString;
		if (RequireStringField(*shapeObject, TEXT("type"), TEXT("shape_type"),
		                       FString::Printf(TEXT("%s.shape"), *regionPath), result, shapeTypeString)
			&& !ParseGroundShapeType(shapeTypeString, regionSpec.ShapeType))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_shape_type"),
			              FString::Printf(TEXT("%s.shape.type '%s' 값은 지원하지 않음."), *regionPath, *shapeTypeString));
		}

		if (regionSpec.ShapeType != EScenarioGroundShapeType::Rectangle)
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("unsupported_shape"),
			              FString::Printf(TEXT("%s는 MVP에서 rectangle 지면 영역만 지원함."), *regionPath));
		}

		if (shapeObject->HasField(TEXT("center_m")))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("legacy_center_unsupported"),
			              FString::Printf(TEXT("%s.shape.center_m은 더 이상 지원하지 않음. center_xy_m을 사용해야 함."), *regionPath));
		}

		ReadVector2DAsVectorField(*shapeObject, TEXT("center_xy_m"), MetersToCentimeters,
		                          FString::Printf(TEXT("%s.shape"), *regionPath), result, regionSpec.Center);
		ReadVector2DField(*shapeObject, TEXT("size_m"), MetersToCentimeters,
		                  FString::Printf(TEXT("%s.shape"), *regionPath), result, regionSpec.Size);
		regionSpec.YawDegrees = ReadNumberOrDefault(*shapeObject, TEXT("yaw_deg"), 0.0);
		regionSpec.TraversabilityScore = ReadNumberOrDefault(*regionObject, TEXT("traversability_score"),
		                                                     regionSpec.TraversabilityScore);

		TSharedPtr<FJsonObject> penaltyObject;
		if (TryGetObjectField(*regionObject, TEXT("penalty"), penaltyObject))
		{
			TryGetStringField(*penaltyObject, TEXT("kind"), regionSpec.PenaltyKind);
			regionSpec.PenaltyCost = ReadNumberOrDefault(*penaltyObject, TEXT("cost"), regionSpec.PenaltyCost);
			regionSpec.ViolationAfterSeconds = ReadNumberOrDefault(*penaltyObject, TEXT("violation_after_s"),
			                                                       regionSpec.ViolationAfterSeconds);
		}

		TryGetStringField(*regionObject, TEXT("collision_tag"), regionSpec.CollisionTag);
		result.WorldSpec.GroundRegions.Add(regionSpec);
	}
}

void UScenarioCompiler::CompilePaths(const FJsonObject& rootObject, FScenarioCompileResult& result,
                                    TSet<FString>& outPathIds)
{
	TArray<TSharedPtr<FJsonValue>> paths;
	if (!TryGetArrayField(rootObject, TEXT("paths"), paths)) return;

	for (int32 index = 0; index < paths.Num(); ++index)
	{
		const FString path = FString::Printf(TEXT("paths[%d]"), index);
		if (!paths[index].IsValid() || paths[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_path"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *path));
			continue;
		}

		const TSharedPtr<FJsonObject> pathObject = paths[index]->AsObject();
		FScenarioPathSpec pathSpec;
		if (!RequireStringField(*pathObject, TEXT("path_id"), path, result, pathSpec.PathId)) continue;
		AddUniqueId(outPathIds, pathSpec.PathId, path, result);

		if (pathObject->HasField(TEXT("type")))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("legacy_path_type_unsupported"),
			              FString::Printf(TEXT("%s.type은 더 이상 지원하지 않음. paths는 spline으로 고정됨."), *path));
		}

		if (pathObject->HasField(TEXT("points_m")))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("legacy_path_points_unsupported"),
			              FString::Printf(TEXT("%s.points_m은 더 이상 지원하지 않음. points_xy_m을 사용해야 함."), *path));
		}

		TArray<TSharedPtr<FJsonValue>> pointValues;
		if (!TryGetArrayField(*pathObject, TEXT("points_xy_m"), pointValues))
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("missing_path_points"),
			              FString::Printf(TEXT("%s.points_xy_m 필드는 필수."), *path));
			continue;
		}

		for (int32 pointIndex = 0; pointIndex < pointValues.Num(); ++pointIndex)
		{
			const FString pointPath = FString::Printf(TEXT("%s.points_xy_m[%d]"), *path, pointIndex);
			if (!pointValues[pointIndex].IsValid() || pointValues[pointIndex]->Type != EJson::Array)
			{
				AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"),
				              FString::Printf(TEXT("%s 항목은 숫자 배열이어야 함."), *pointPath));
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>> pointArray = pointValues[pointIndex]->AsArray();
			if (pointArray.Num() != 2)
			{
				AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"),
				              FString::Printf(TEXT("%s 항목은 숫자 2개를 포함해야 함."), *pointPath));
				continue;
			}

			bool bValidPoint = true;
			for (const TSharedPtr<FJsonValue>& pointComponent : pointArray)
			{
				bValidPoint &= pointComponent.IsValid() && pointComponent->Type == EJson::Number;
			}

			if (!bValidPoint)
			{
				AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"),
				              FString::Printf(TEXT("%s 항목은 숫자만 포함해야 함."), *pointPath));
				continue;
			}

			pathSpec.Points.Add(FVector(
				pointArray[0]->AsNumber() * MetersToCentimeters,
				pointArray[1]->AsNumber() * MetersToCentimeters,
				0.0));
		}

		pathSpec.bClosedLoop = ReadBoolOrDefault(*pathObject, TEXT("closed_loop"), false);
		if (pathSpec.Points.Num() < 2)
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("too_few_path_points"),
			              FString::Printf(TEXT("%s에는 최소 2개의 point가 필요함."), *path));
		}

		result.WorldSpec.Paths.Add(pathSpec);
	}
}

void UScenarioCompiler::CompileStaticObstacles(
	const FJsonObject& actorsObject,
	FScenarioCompileResult& result,
	TSet<FString>& instanceIds) const
{
	TArray<TSharedPtr<FJsonValue>> staticObstacles;
	if (!TryGetArrayField(actorsObject, TEXT("static_obstacles"), staticObstacles)) return;

	for (int32 index = 0; index < staticObstacles.Num(); ++index)
	{
		const FString obstaclePath = FString::Printf(TEXT("actors.static_obstacles[%d]"), index);
		if (!staticObstacles[index].IsValid() || staticObstacles[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_static_obstacle"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *obstaclePath));
			continue;
		}

		const TSharedPtr<FJsonObject> obstacleObject = staticObstacles[index]->AsObject();
		FScenarioPlaceableInstanceSpec placeableSpec;
		if (!RequireStringField(*obstacleObject, TEXT("instance_id"), obstaclePath, result, placeableSpec.InstanceId)) continue;
		AddUniqueId(instanceIds, placeableSpec.InstanceId, obstaclePath, result);

		if (!RequireStringField(*obstacleObject, TEXT("prop_id"), TEXT("asset_id"), obstaclePath, result,
		                        placeableSpec.AssetId))
		{
			continue;
		}

		FScenarioStaticObstaclePropEntry propEntry;
		const UScenarioStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
		if (!IsValid(propCatalog))
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("static_obstacle_catalog_unavailable"),
				FString::Printf(
					TEXT("%s.prop_id '%s' validation failed because the static obstacle catalog could not be loaded: %s"),
					*obstaclePath,
					*placeableSpec.AssetId,
					*StaticObstaclePropCatalog.ToSoftObjectPath().ToString()));
		}
		else if (!propCatalog->FindPropEntryById(FName(*placeableSpec.AssetId), propEntry))
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("unknown_static_obstacle_prop"),
				FString::Printf(
					TEXT("%s.prop_id '%s' 값이 기본 정적 장애물 catalog에 없음."), *obstaclePath, *placeableSpec.AssetId));
		}

		placeableSpec.Category = EScenarioActorCategory::StaticObstacle;
		ReadActorPlacementTransform(*obstacleObject, obstaclePath, result, placeableSpec.Transform);
		AddJsonProperties(*obstacleObject, placeableSpec.Properties);
		result.WorldSpec.Placeables.Add(placeableSpec);
	}
}

void UScenarioCompiler::CompilePedestrianBehavior(
	const FJsonObject& pedestrianObject,
	const FString& path,
	FScenarioCompileResult& result,
	FScenarioDynamicActorSpec& dynamicActorSpec)
{
	TSharedPtr<FJsonObject> behaviorObject;
	if (!TryGetObjectField(pedestrianObject, TEXT("behavior"), behaviorObject))
	{
		return;
	}

	const auto readOptionalNumber = [&result, &behaviorObject, &path](
		const TCHAR* fieldName,
		const TCHAR* propertyName,
		double scale,
		TMap<FString, FScenarioParamValue>& outProperties)
	{
		const FString fieldNameString(fieldName);
		if (!behaviorObject->HasField(fieldNameString)) return;

		double value = 0.0;
		if (!behaviorObject->TryGetNumberField(fieldNameString, value))
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("invalid_number"),
				FString::Printf(TEXT("%s.behavior.%s 필드는 숫자여야 함."), *path, fieldName));
			return;
		}

		outProperties.Add(propertyName, MakeFloatParam(value * scale));
	};

	readOptionalNumber(TEXT("cooperation"), TEXT("behavior_cooperation"), 1.0, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("evasiveness"), TEXT("behavior_evasiveness"), 1.0, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("personal_space_m"), TEXT("behavior_personal_space_cm"), MetersToCentimeters, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("personal_space_cm"), TEXT("behavior_personal_space_cm"), 1.0, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("awareness_horizon_s"), TEXT("behavior_awareness_horizon_s"), 1.0, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("max_yield_wait_s"), TEXT("behavior_max_yield_wait_s"), 1.0, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("sidestep_distance_m"), TEXT("behavior_sidestep_distance_cm"), MetersToCentimeters, dynamicActorSpec.Properties);
	readOptionalNumber(TEXT("sidestep_distance_cm"), TEXT("behavior_sidestep_distance_cm"), 1.0, dynamicActorSpec.Properties);
}

void UScenarioCompiler::CompilePedestrians(
	const FJsonObject& actorsObject,
	FScenarioCompileResult& result,
	const TSet<FString>& pathIds,
	TSet<FString>& instanceIds)
{
	TArray<TSharedPtr<FJsonValue>> pedestrians;
	if (!TryGetArrayField(actorsObject, TEXT("pedestrians"), pedestrians)) return;

	for (int32 index = 0; index < pedestrians.Num(); ++index)
	{
		const FString pedestrianPath = FString::Printf(TEXT("actors.pedestrians[%d]"), index);
		if (!pedestrians[index].IsValid() || pedestrians[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_pedestrian"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *pedestrianPath));
			continue;
		}

		const TSharedPtr<FJsonObject> pedestrianObject = pedestrians[index]->AsObject();
		FScenarioDynamicActorSpec dynamicActorSpec;
		if (!RequireStringField(*pedestrianObject, TEXT("instance_id"), pedestrianPath, result,
		                        dynamicActorSpec.InstanceId))
		{
			continue;
		}
		AddUniqueId(instanceIds, dynamicActorSpec.InstanceId, pedestrianPath, result);

		if (!TryGetStringField(*pedestrianObject, TEXT("archetype_id"), dynamicActorSpec.AssetId))
		{
			dynamicActorSpec.AssetId = TEXT("adult_pedestrian");
		}

		FString movementModel = TEXT("spline_Relative");
		TSharedPtr<FJsonObject> movementObject;
		if (TryGetObjectField(*pedestrianObject, TEXT("movement"), movementObject))
		{
			TryGetStringField(*movementObject, TEXT("model"), movementModel);
		}
		dynamicActorSpec.Properties.Add(TEXT("movement_model"), MakeStringParam(movementModel));
		const bool bPlannedTrajectory = movementModel.Equals(TEXT("planned_trajectory"), ESearchCase::IgnoreCase);
		const bool bStaticPlacement = movementModel.Equals(TEXT("static_placement"), ESearchCase::IgnoreCase);

		if (!bPlannedTrajectory && !bStaticPlacement)
		{
			RequireStringField(*pedestrianObject, TEXT("path_id"), pedestrianPath, result, dynamicActorSpec.PathId);
			if (!dynamicActorSpec.PathId.IsEmpty() && !pathIds.Contains(dynamicActorSpec.PathId))
			{
				AddDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("unknown_path"),
					FString::Printf(
						TEXT("%s.path_id '%s' 값과 일치하는 컴파일된 path가 없음."), *pedestrianPath, *dynamicActorSpec.PathId));
			}
		}
		else
		{
			TryGetStringField(*pedestrianObject, TEXT("path_id"), dynamicActorSpec.PathId);
		}

		dynamicActorSpec.Category = EScenarioActorCategory::Pedestrian;
		ReadActorPlacementTransform(*pedestrianObject, pedestrianPath, result, dynamicActorSpec.InitialTransform);
		AddJsonProperties(*pedestrianObject, dynamicActorSpec.Properties);

		if (bPlannedTrajectory)
		{
			FVector plannedStartCm = FVector::ZeroVector;
			if (pedestrianObject->HasField(TEXT("start_xy_m")))
			{
				if (ReadVector2DAsVectorField(*pedestrianObject, TEXT("start_xy_m"), MetersToCentimeters, pedestrianPath, result, plannedStartCm))
				{
					dynamicActorSpec.InitialTransform.SetLocation(plannedStartCm);
					dynamicActorSpec.Properties.Add(TEXT("planned_start_cm"), MakeVectorParam(plannedStartCm));
				}
			}
			else
			{
				AddDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("missing_pedestrian_start"),
					FString::Printf(TEXT("%s.start_xy_m 필드는 planned_trajectory 보행자에 필수."), *pedestrianPath));
			}

			FVector plannedGoalCm = FVector::ZeroVector;
			if (pedestrianObject->HasField(TEXT("goal_xy_m")))
			{
				if (ReadVector2DAsVectorField(*pedestrianObject, TEXT("goal_xy_m"), MetersToCentimeters, pedestrianPath, result, plannedGoalCm))
				{
					dynamicActorSpec.Properties.Add(TEXT("planned_goal_cm"), MakeVectorParam(plannedGoalCm));
				}
			}
			else
			{
				AddDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("missing_pedestrian_goal"),
					FString::Printf(TEXT("%s.goal_xy_m 필드는 planned_trajectory 보행자에 필수."), *pedestrianPath));
			}
		}

		if (movementObject.IsValid())
		{
			if (movementObject->HasField(TEXT("speed_mps")))
			{
				const double speedMps = ReadNumberOrDefault(*movementObject, TEXT("speed_mps"), 1.2);
				dynamicActorSpec.Properties.Add(TEXT("speed_mps"), MakeFloatParam(speedMps));
				dynamicActorSpec.Properties.Add(
					TEXT("speed_cm_per_second"), MakeFloatParam(speedMps * MetersToCentimeters));
			}

			if (movementObject->HasField(TEXT("initial_distance_m")))
			{
				const double initialDistanceM = ReadNumberOrDefault(*movementObject, TEXT("initial_distance_m"), 0.0);
				dynamicActorSpec.Properties.Add(TEXT("initial_distance_m"), MakeFloatParam(initialDistanceM));
				dynamicActorSpec.Properties.Add(
					TEXT("initial_distance_cm"), MakeFloatParam(initialDistanceM * MetersToCentimeters));
			}

			if (movementObject->HasField(TEXT("curve_offset_m")))
			{
				const double curveOffsetM = ReadNumberOrDefault(*movementObject, TEXT("curve_offset_m"), 0.0);
				dynamicActorSpec.Properties.Add(TEXT("path_curve_offset_cm"), MakeFloatParam(curveOffsetM * MetersToCentimeters));
			}

			if (movementObject->HasField(TEXT("curve_offset_cm")))
			{
				dynamicActorSpec.Properties.Add(
					TEXT("path_curve_offset_cm"),
					MakeFloatParam(ReadNumberOrDefault(*movementObject, TEXT("curve_offset_cm"), 0.0)));
			}

			if (movementObject->HasField(TEXT("curve_sample_spacing_m")))
			{
				const double curveSampleSpacingM = ReadNumberOrDefault(*movementObject, TEXT("curve_sample_spacing_m"), 0.5);
				dynamicActorSpec.Properties.Add(
					TEXT("path_curve_sample_spacing_cm"),
					MakeFloatParam(curveSampleSpacingM * MetersToCentimeters));
			}

			if (movementObject->HasField(TEXT("curve_sample_spacing_cm")))
			{
				dynamicActorSpec.Properties.Add(
					TEXT("path_curve_sample_spacing_cm"),
					MakeFloatParam(ReadNumberOrDefault(*movementObject, TEXT("curve_sample_spacing_cm"), 50.0)));
			}

			if (movementObject->HasField(TEXT("auto_start")))
			{
				dynamicActorSpec.Properties.Add(
					TEXT("auto_start"), MakeBoolParam(ReadBoolOrDefault(*movementObject, TEXT("auto_start"), true)));
			}
		}

		CompilePedestrianBehavior(*pedestrianObject, pedestrianPath, result, dynamicActorSpec);

		result.WorldSpec.DynamicActors.Add(dynamicActorSpec);
	}
}

void UScenarioCompiler::CompileRobotSpawn(const FJsonObject& actorsObject, FScenarioCompileResult& result, TSet<FString>& instanceIds)
{
	TSharedPtr<FJsonObject> robotObject;
	if (!TryGetObjectField(actorsObject, TEXT("robot"), robotObject))
		return;

	FScenarioPlaceableInstanceSpec robotSpec;
	robotSpec.Category = EScenarioActorCategory::DeliveryBot;

	if (!RequireStringField(*robotObject,TEXT("instance_id"),TEXT("actor_id"),TEXT("actors.robot"),result,robotSpec.InstanceId))
		return;

	AddUniqueId(instanceIds, robotSpec.InstanceId, TEXT("actors.robot"), result);

	if (!RequireStringField(*robotObject,TEXT("asset_id"),TEXT("type"),TEXT("actors.robot"),result,robotSpec.AssetId))
		return;

	const bool bSpawnOnly{ ReadBoolOrDefault(*robotObject, TEXT("spawn_only"), true)};
	bool bHasGoal{ false };

	if (!ReadActorPlacementTransform(
		*robotObject,
		TEXT("actors.robot"),
		result,
		robotSpec.Transform
	))
	{
		robotSpec.Transform = FTransform::Identity;
	}

	FDeliveryBotSetupInfo& robotSetupInfo = robotSpec.DeliveryBot.SetupInfo;
	robotSpec.DeliveryBot.bSpawnOnly = bSpawnOnly;
	robotSpec.DeliveryBot.bHasStartLocation = true;
	robotSetupInfo.LocationSetupInfo.StartLocationCm = robotSpec.Transform.GetLocation();
	robotSetupInfo.LocationSetupInfo.GoalLocationCm = robotSetupInfo.LocationSetupInfo.StartLocationCm;

	const auto readOptionalBoolField = [&result](
		const FJsonObject& sourceObject,
		const TCHAR* fieldName,
		const FString& path,
		bool& targetValue)
	{
		const FString fieldNameString{ fieldName };
		if (!sourceObject.HasField(fieldNameString)) return false;

		if (!sourceObject.TryGetBoolField(fieldNameString, targetValue))
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("invalid_bool"),
				FString::Printf(TEXT("%s.%s 필드는 bool 값이어야 함."), *path, *fieldNameString));
			return false;
		}

		return true;
	};

	if (robotObject->HasField(TEXT("location")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Warning,
			TEXT("robot_location_ignored"),
			TEXT("actors.robot.location은 더 이상 ScenarioCompiler에서 읽지 않음. 로봇 배치는 xy_m/yaw_deg, 목적지는 route.goal_xy_m을 사용해야 함."));
	}

	if (robotObject->HasField(TEXT("goal_xy_m")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("legacy_robot_goal_unsupported"),
			TEXT("actors.robot.goal_xy_m은 더 이상 지원하지 않음. actors.robot.route.goal_xy_m을 사용해야 함."));
	}

	if (robotObject->HasField(TEXT("goal_m")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("legacy_robot_goal_unsupported"),
			TEXT("actors.robot.goal_m은 더 이상 지원하지 않음. actors.robot.route.goal_xy_m을 사용해야 함."));
	}

	if (robotObject->HasField(TEXT("drive"))
		|| robotObject->HasField(TEXT("path_follow"))
		|| robotObject->HasField(TEXT("lidar")))
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("delivery_bot_setup_fields_in_scenario_setup"),
			TEXT("actors.robot.drive/path_follow/lidar는 더 이상 ScenarioSetup에서 지원하지 않음. DeliveryBotSetup JSON으로 분리해야 함."));
	}

	TSharedPtr<FJsonObject> routeObject;
	if (TryGetObjectField(*robotObject, TEXT("route"), routeObject))
	{
		FVector goalLocationCm{ FVector::ZeroVector };

		if (routeObject->HasField(TEXT("goal_m")))
		{
			AddDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("legacy_robot_goal_unsupported"),
				TEXT("actors.robot.route.goal_m은 더 이상 지원하지 않음. goal_xy_m을 사용해야 함."));
		}

		if (!bHasGoal &&
			routeObject->HasField(TEXT("goal_xy_m")) &&
			ReadVector2DAsVectorField(
				*routeObject,
				TEXT("goal_xy_m"),
				MetersToCentimeters,
				TEXT("actors.robot.route"),
				result,
				goalLocationCm))
		{
			robotSetupInfo.LocationSetupInfo.GoalLocationCm = goalLocationCm;
			bHasGoal = true;
		}

		if (routeObject->HasField(TEXT("auto_start")))
		{
			readOptionalBoolField(
				*routeObject,
				TEXT("auto_start"),
				TEXT("actors.robot.route"),
				robotSetupInfo.LocationSetupInfo.bAutoStartRoute);
		}
	}

	// 목표 존재 여부를 SetupInfo에도 저장한다.
	robotSetupInfo.LocationSetupInfo.bHasGoal = bHasGoal;
	robotSetupInfo.LocationSetupInfo.bAutoStartRoute = !bSpawnOnly && robotSetupInfo.LocationSetupInfo.bAutoStartRoute && bHasGoal;
	robotSpec.DeliveryBot.bHasGoalLocation = bHasGoal;

	AddJsonProperties(*robotObject, robotSpec.Properties);

	if (!bSpawnOnly && !bHasGoal)
	{
		AddDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Warning,
			TEXT("missing_robot_goal"),
			TEXT("actors.robot.spawn_only가 false이지만 route.goal_xy_m이 없어 로봇 경로 주입을 건너뜀."));
	}

	result.WorldSpec.Placeables.Add(robotSpec);
}

void UScenarioCompiler::CompileActors(
	const FJsonObject& rootObject,
	FScenarioCompileResult& result,
	const TSet<FString>& pathIds) const
{
	TSharedPtr<FJsonObject> actorsObject;
	if (!TryGetObjectField(rootObject, TEXT("actors"), actorsObject))
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Warning, TEXT("missing_actors"),
		              TEXT("actors 필드가 없음."));
		return;
	}

	TSet<FString> instanceIds;
	CompileStaticObstacles(*actorsObject, result, instanceIds);
	CompilePedestrians(*actorsObject, result, pathIds, instanceIds);
	CompileRobotSpawn(*actorsObject, result, instanceIds);
}

void UScenarioCompiler::CompileRootObject(const FJsonObject& rootObject, const FString& sourceJson,
                                         FScenarioCompileResult& result) const
{
	CompileRunConfig(rootObject, result);
	CompileEvaluationConfig(rootObject, result);
	CompileGroundRegions(rootObject, result);

	TSet<FString> pathIds;
	CompilePaths(rootObject, result, pathIds);
	CompileActors(rootObject, result, pathIds);

	result.WorldSpec.SpecHash = FString::Printf(TEXT("%u"), GetTypeHash(sourceJson));
}

FScenarioCompileResult UScenarioCompiler::CompileScenarioWorldSpecFromJsonFile(const FString& jsonFilePath) const
{
	FScenarioCompileResult result;
	const FString resolvedJsonFilePath = ResolveScenarioJsonFilePath(jsonFilePath);

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedJsonFilePath))
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("file_read_failed"),
		              FString::Printf(TEXT("JSON 파일 '%s' 읽기 실패. ResolvedPath: '%s'"), *jsonFilePath, *resolvedJsonFilePath));
		result.bSuccess = false;
		return result;
	}

	return CompileScenarioWorldSpecFromJsonString(jsonString);
}

FScenarioCompileResult UScenarioCompiler::CompileScenarioWorldSpecFromJsonString(const FString& jsonString) const
{
	FScenarioCompileResult result;

	if (jsonString.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("empty_json"), TEXT("JSON 입력이 비어 있음."));
		result.bSuccess = false;
		return result;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		AddDiagnostic(result, EScenarioCompileDiagnosticSeverity::Error, TEXT("invalid_json"),
		              TEXT("JSON 입력을 파싱할 수 없음."));
		result.bSuccess = false;
		return result;
	}

	CompileRootObject(*rootObject, jsonString, result);
	result.bSuccess = !HasErrors(result);
	return result;
}
