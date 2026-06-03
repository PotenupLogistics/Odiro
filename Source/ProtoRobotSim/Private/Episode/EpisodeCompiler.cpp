#include "Episode/EpisodeCompiler.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString ResolveEpisodeJsonFilePath(const FString& jsonFilePath)
	{
		if (jsonFilePath.IsEmpty()) return jsonFilePath;

		if (FPaths::IsRelative(jsonFilePath))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), jsonFilePath));
		}

		return jsonFilePath;
	}
}

void UEpisodeCompiler::AddDiagnostic(
	FEpisodeCompileResult& result,
	EEpisodeCompileDiagnosticSeverity severity,
	const FString& code,
	const FString& message)
{
	FEpisodeCompileDiagnostic diagnostic;
	diagnostic.Severity = severity;
	diagnostic.Code = code;
	diagnostic.Message = message;
	result.Diagnostics.Add(diagnostic);
}

bool UEpisodeCompiler::HasErrors(const FEpisodeCompileResult& result)
{
	for (const FEpisodeCompileDiagnostic& diagnostic : result.Diagnostics)
	{
		if (diagnostic.Severity == EEpisodeCompileDiagnosticSeverity::Error) return true;
	}

	return false;
}

FEpisodeParamValue UEpisodeCompiler::MakeBoolParam(bool value)
{
	FEpisodeParamValue param;
	param.Type = EEpisodeParamValueType::Bool;
	param.BoolValue = value;
	return param;
}

FEpisodeParamValue UEpisodeCompiler::MakeFloatParam(double value)
{
	FEpisodeParamValue param;
	param.Type = EEpisodeParamValueType::Float;
	param.FloatValue = value;
	return param;
}

FEpisodeParamValue UEpisodeCompiler::MakeStringParam(const FString& value)
{
	FEpisodeParamValue param;
	param.Type = EEpisodeParamValueType::String;
	param.StringValue = value;
	return param;
}

FEpisodeParamValue UEpisodeCompiler::MakeVectorParam(const FVector& value)
{
	FEpisodeParamValue param;
	param.Type = EEpisodeParamValueType::Vector;
	param.VectorValue = value;
	return param;
}

bool UEpisodeCompiler::TryGetObjectField(const FJsonObject& jsonObject, const FString& fieldName,
                                         TSharedPtr<FJsonObject>& outObject)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid() || jsonValue->Type != EJson::Object) return false;

	outObject = jsonValue->AsObject();
	return outObject.IsValid();
}

bool UEpisodeCompiler::TryGetArrayField(const FJsonObject& jsonObject, const FString& fieldName,
                                        TArray<TSharedPtr<FJsonValue>>& outArray)
{
	const TSharedPtr<FJsonValue> jsonValue = jsonObject.TryGetField(fieldName);
	if (!jsonValue.IsValid() || jsonValue->Type != EJson::Array) return false;

	outArray = jsonValue->AsArray();
	return true;
}

bool UEpisodeCompiler::TryGetStringField(const FJsonObject& jsonObject, const FString& fieldName, FString& outValue)
{
	return jsonObject.TryGetStringField(fieldName, outValue);
}

bool UEpisodeCompiler::TryGetStringField(const FJsonObject& jsonObject, const FString& primaryFieldName,
                                         const FString& fallbackFieldName, FString& outValue)
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
	if (TryGetStringField(jsonObject, fieldName, outValue) && !outValue.IsEmpty()) return true;

	AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("missing_string"),
	              FString::Printf(TEXT("%s.%s 필드는 필수."), *path, *fieldName));
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
	if (TryGetStringField(jsonObject, primaryFieldName, fallbackFieldName, outValue) && !outValue.IsEmpty()) return true;

	AddDiagnostic(
		result,
		EEpisodeCompileDiagnosticSeverity::Error,
		TEXT("missing_string"),
		FString::Printf(TEXT("%s.%s 필드는 필수."), *path, *primaryFieldName));
	return false;
}

double UEpisodeCompiler::ReadNumberOrDefault(const FJsonObject& jsonObject, const FString& fieldName,
                                             double defaultValue)
{
	double value = defaultValue;
	jsonObject.TryGetNumberField(fieldName, value);
	return value;
}

bool UEpisodeCompiler::ReadBoolOrDefault(const FJsonObject& jsonObject, const FString& fieldName, bool defaultValue)
{
	bool value = defaultValue;
	jsonObject.TryGetBoolField(fieldName, value);
	return value;
}

bool UEpisodeCompiler::ReadNumberArray(
	const FJsonObject& jsonObject,
	const FString& fieldName,
	int32 expectedCount,
	const FString& path,
	FEpisodeCompileResult& result,
	TArray<double>& outValues)
{
	TArray<TSharedPtr<FJsonValue>> jsonArray;
	if (!TryGetArrayField(jsonObject, fieldName, jsonArray))
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("missing_number_array"),
			FString::Printf(TEXT("%s.%s 필드는 숫자 %d개로 이루어진 배열이어야 함."), *path, *fieldName, expectedCount));
		return false;
	}

	if (jsonArray.Num() != expectedCount)
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
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
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("invalid_number_array"),
				FString::Printf(TEXT("%s.%s[%d] 값은 숫자여야 함."), *path, *fieldName, index));
			return false;
		}

		outValues.Add(jsonValue->AsNumber());
	}

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
	TArray<double> values;
	if (!ReadNumberArray(jsonObject, fieldName, 2, path, result, values)) return false;

	outVector = FVector2D(values[0] * scale, values[1] * scale);
	return true;
}

bool UEpisodeCompiler::ReadVector2DAsVectorField(
	const FJsonObject& jsonObject,
	const FString& fieldName,
	double scale,
	const FString& path,
	FEpisodeCompileResult& result,
	FVector& outVector)
{
	FVector2D vector2D;
	if (!ReadVector2DField(jsonObject, fieldName, scale, path, result, vector2D)) return false;

	outVector = FVector(vector2D.X, vector2D.Y, 0.0);
	return true;
}

bool UEpisodeCompiler::ReadActorPlacementTransform(
	const FJsonObject& jsonObject,
	const FString& path,
	FEpisodeCompileResult& result,
	FTransform& outTransform)
{
	outTransform = FTransform::Identity;

	if (jsonObject.HasField(TEXT("transform")))
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
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
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("invalid_number"),
			FString::Printf(TEXT("%s.yaw_deg 필드는 숫자여야 함."), *path));
		return false;
	}

	outTransform = FTransform(FRotator(0.0, yawDegrees, 0.0), location, FVector::OneVector);
	return true;
}

bool UEpisodeCompiler::ParseGroundRegionType(const FString& value, EEpisodeGroundRegionType& outType)
{
	const FString normalized = value.ToLower();
	if (normalized == TEXT("walkable"))
	{
		outType = EEpisodeGroundRegionType::Walkable;
		return true;
	}

	if (normalized == TEXT("penalty"))
	{
		outType = EEpisodeGroundRegionType::Penalty;
		return true;
	}

	if (normalized == TEXT("blocked"))
	{
		outType = EEpisodeGroundRegionType::Blocked;
		return true;
	}

	return false;
}

bool UEpisodeCompiler::ParseGroundShapeType(const FString& value, EEpisodeGroundShapeType& outType)
{
	const FString normalized = value.ToLower();
	if (normalized == TEXT("rectangle"))
	{
		outType = EEpisodeGroundShapeType::Rectangle;
		return true;
	}

	if (normalized == TEXT("convex_polygon"))
	{
		outType = EEpisodeGroundShapeType::ConvexPolygon;
		return true;
	}

	return false;
}

bool UEpisodeCompiler::AddUniqueId(TSet<FString>& ids, const FString& id, const FString& path,
                                   FEpisodeCompileResult& result)
{
	if (id.IsEmpty()) return false;

	if (ids.Contains(id))
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("duplicate_id"),
			FString::Printf(TEXT("%s에 중복 id '%s'가 있음."), *path, *id));
		return false;
	}

	ids.Add(id);
	return true;
}

void UEpisodeCompiler::AddJsonProperties(const FJsonObject& sourceObject,
                                         TMap<FString, FEpisodeParamValue>& outProperties)
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

void UEpisodeCompiler::CompileRunConfig(const FJsonObject& rootObject, FEpisodeCompileResult& result)
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

void UEpisodeCompiler::CompileEvaluationConfig(const FJsonObject& rootObject, FEpisodeCompileResult& result)
{
	FEpisodeEvaluationConfig& evaluationConfig = result.WorldSpec.EvaluationConfig;

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

void UEpisodeCompiler::CompileGroundRegions(const FJsonObject& rootObject, FEpisodeCompileResult& result)
{
	TSharedPtr<FJsonObject> groundModelObject;
	if (!TryGetObjectField(rootObject, TEXT("ground_model"), groundModelObject))
	{
		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Warning, TEXT("missing_ground_model"),
		              TEXT("ground_model 필드가 없음."));
		return;
	}

	TArray<TSharedPtr<FJsonValue>> regions;
	if (!TryGetArrayField(*groundModelObject, TEXT("regions"), regions))
	{
		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Warning, TEXT("missing_ground_regions"),
		              TEXT("ground_model.regions 필드가 없음."));
		return;
	}

	TSet<FString> regionIds;
	for (int32 index = 0; index < regions.Num(); ++index)
	{
		const FString regionPath = FString::Printf(TEXT("ground_model.regions[%d]"), index);
		if (!regions[index].IsValid() || regions[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_ground_region"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *regionPath));
			continue;
		}

		const TSharedPtr<FJsonObject> regionObject = regions[index]->AsObject();
		FEpisodeGroundRegionSpec regionSpec;
		if (!RequireStringField(*regionObject, TEXT("region_id"), regionPath, result, regionSpec.RegionId)) continue;
		AddUniqueId(regionIds, regionSpec.RegionId, regionPath, result);

		FString regionTypeString;
		if (RequireStringField(*regionObject, TEXT("region_type"), TEXT("type"), regionPath, result, regionTypeString)
			&& !ParseGroundRegionType(regionTypeString, regionSpec.RegionType))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_ground_type"),
			              FString::Printf(TEXT("%s.region_type '%s' 값은 지원하지 않음."), *regionPath, *regionTypeString));
		}

		TSharedPtr<FJsonObject> shapeObject;
		if (!TryGetObjectField(*regionObject, TEXT("shape"), shapeObject))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("missing_shape"),
			              FString::Printf(TEXT("%s.shape 필드는 필수."), *regionPath));
			continue;
		}

		FString shapeTypeString;
		if (RequireStringField(*shapeObject, TEXT("type"), TEXT("shape_type"),
		                       FString::Printf(TEXT("%s.shape"), *regionPath), result, shapeTypeString)
			&& !ParseGroundShapeType(shapeTypeString, regionSpec.ShapeType))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_shape_type"),
			              FString::Printf(TEXT("%s.shape.type '%s' 값은 지원하지 않음."), *regionPath, *shapeTypeString));
		}

		if (regionSpec.ShapeType != EEpisodeGroundShapeType::Rectangle)
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("unsupported_shape"),
			              FString::Printf(TEXT("%s는 MVP에서 rectangle 지면 영역만 지원함."), *regionPath));
		}

		if (shapeObject->HasField(TEXT("center_m")))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("legacy_center_unsupported"),
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

void UEpisodeCompiler::CompilePaths(const FJsonObject& rootObject, FEpisodeCompileResult& result,
                                    TSet<FString>& outPathIds)
{
	TArray<TSharedPtr<FJsonValue>> paths;
	if (!TryGetArrayField(rootObject, TEXT("paths"), paths)) return;

	for (int32 index = 0; index < paths.Num(); ++index)
	{
		const FString path = FString::Printf(TEXT("paths[%d]"), index);
		if (!paths[index].IsValid() || paths[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *path));
			continue;
		}

		const TSharedPtr<FJsonObject> pathObject = paths[index]->AsObject();
		FEpisodePathSpec pathSpec;
		if (!RequireStringField(*pathObject, TEXT("path_id"), path, result, pathSpec.PathId)) continue;
		AddUniqueId(outPathIds, pathSpec.PathId, path, result);

		if (pathObject->HasField(TEXT("type")))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("legacy_path_type_unsupported"),
			              FString::Printf(TEXT("%s.type은 더 이상 지원하지 않음. paths는 spline으로 고정됨."), *path));
		}

		if (pathObject->HasField(TEXT("points_m")))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("legacy_path_points_unsupported"),
			              FString::Printf(TEXT("%s.points_m은 더 이상 지원하지 않음. points_xy_m을 사용해야 함."), *path));
		}

		TArray<TSharedPtr<FJsonValue>> pointValues;
		if (!TryGetArrayField(*pathObject, TEXT("points_xy_m"), pointValues))
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("missing_path_points"),
			              FString::Printf(TEXT("%s.points_xy_m 필드는 필수."), *path));
			continue;
		}

		for (int32 pointIndex = 0; pointIndex < pointValues.Num(); ++pointIndex)
		{
			const FString pointPath = FString::Printf(TEXT("%s.points_xy_m[%d]"), *path, pointIndex);
			if (!pointValues[pointIndex].IsValid() || pointValues[pointIndex]->Type != EJson::Array)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"),
				              FString::Printf(TEXT("%s 항목은 숫자 배열이어야 함."), *pointPath));
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>> pointArray = pointValues[pointIndex]->AsArray();
			if (pointArray.Num() != 2)
			{
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"),
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
				AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_path_point"),
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
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("too_few_path_points"),
			              FString::Printf(TEXT("%s에는 최소 2개의 point가 필요함."), *path));
		}

		result.WorldSpec.Paths.Add(pathSpec);
	}
}

void UEpisodeCompiler::CompileStaticObstacles(
	const FJsonObject& actorsObject,
	FEpisodeCompileResult& result,
	TSet<FString>& instanceIds)
{
	TArray<TSharedPtr<FJsonValue>> staticObstacles;
	if (!TryGetArrayField(actorsObject, TEXT("static_obstacles"), staticObstacles)) return;

	for (int32 index = 0; index < staticObstacles.Num(); ++index)
	{
		const FString obstaclePath = FString::Printf(TEXT("actors.static_obstacles[%d]"), index);
		if (!staticObstacles[index].IsValid() || staticObstacles[index]->Type != EJson::Object)
		{
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_static_obstacle"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *obstaclePath));
			continue;
		}

		const TSharedPtr<FJsonObject> obstacleObject = staticObstacles[index]->AsObject();
		FEpisodePlaceableInstanceSpec placeableSpec;
		if (!RequireStringField(*obstacleObject, TEXT("instance_id"), obstaclePath, result, placeableSpec.InstanceId)) continue;
		AddUniqueId(instanceIds, placeableSpec.InstanceId, obstaclePath, result);

		if (!RequireStringField(*obstacleObject, TEXT("prop_id"), TEXT("asset_id"), obstaclePath, result,
		                        placeableSpec.AssetId))
		{
			continue;
		}

		FEpisodeStaticObstaclePropEntry propEntry;
		if (!AEpisodeStaticObstacle::FindDefaultPropEntryById(FName(*placeableSpec.AssetId), propEntry))
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("unknown_static_obstacle_prop"),
				FString::Printf(
					TEXT("%s.prop_id '%s' 값이 기본 정적 장애물 catalog에 없음."), *obstaclePath, *placeableSpec.AssetId));
		}

		placeableSpec.Category = EEpisodeActorCategory::StaticObstacle;
		ReadActorPlacementTransform(*obstacleObject, obstaclePath, result, placeableSpec.Transform);
		AddJsonProperties(*obstacleObject, placeableSpec.Properties);
		result.WorldSpec.Placeables.Add(placeableSpec);
	}
}

void UEpisodeCompiler::CompilePedestrians(
	const FJsonObject& actorsObject,
	FEpisodeCompileResult& result,
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
			AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_pedestrian"),
			              FString::Printf(TEXT("%s 항목은 object이어야 함."), *pedestrianPath));
			continue;
		}

		const TSharedPtr<FJsonObject> pedestrianObject = pedestrians[index]->AsObject();
		FEpisodeDynamicActorSpec dynamicActorSpec;
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

		RequireStringField(*pedestrianObject, TEXT("path_id"), pedestrianPath, result, dynamicActorSpec.PathId);
		if (!dynamicActorSpec.PathId.IsEmpty() && !pathIds.Contains(dynamicActorSpec.PathId))
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Error,
				TEXT("unknown_path"),
				FString::Printf(
					TEXT("%s.path_id '%s' 값과 일치하는 컴파일된 path가 없음."), *pedestrianPath, *dynamicActorSpec.PathId));
		}

		dynamicActorSpec.Category = EEpisodeActorCategory::Pedestrian;
		dynamicActorSpec.SpawnTimeSeconds = ReadNumberOrDefault(*pedestrianObject, TEXT("spawn_time_s"), 0.0);
		ReadActorPlacementTransform(*pedestrianObject, pedestrianPath, result, dynamicActorSpec.InitialTransform);
		AddJsonProperties(*pedestrianObject, dynamicActorSpec.Properties);

		TSharedPtr<FJsonObject> movementObject;
		if (TryGetObjectField(*pedestrianObject, TEXT("movement"), movementObject))
		{
			FString movementModel;
			if (TryGetStringField(*movementObject, TEXT("model"), movementModel))
			{
				dynamicActorSpec.Properties.Add(TEXT("movement_model"), MakeStringParam(movementModel));
			}

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

			if (movementObject->HasField(TEXT("auto_start")))
			{
				dynamicActorSpec.Properties.Add(
					TEXT("auto_start"), MakeBoolParam(ReadBoolOrDefault(*movementObject, TEXT("auto_start"), true)));
			}
		}

		result.WorldSpec.DynamicActors.Add(dynamicActorSpec);
	}
}

void UEpisodeCompiler::CompileRobotSpawn(const FJsonObject& actorsObject, FEpisodeCompileResult& result, TSet<FString>& instanceIds)
{
	TSharedPtr<FJsonObject> robotObject;
	if (!TryGetObjectField(actorsObject, TEXT("robot"), robotObject))
		return;

	FEpisodePlaceableInstanceSpec robotSpec;
	robotSpec.Category = EEpisodeActorCategory::DeliveryBot;

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
				EEpisodeCompileDiagnosticSeverity::Error,
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
			EEpisodeCompileDiagnosticSeverity::Warning,
			TEXT("robot_location_ignored"),
			TEXT("actors.robot.location은 더 이상 EpisodeCompiler에서 읽지 않음. 로봇 배치는 xy_m/yaw_deg, 목적지는 route.goal_xy_m을 사용해야 함."));
	}

	if (robotObject->HasField(TEXT("goal_xy_m")))
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("legacy_robot_goal_unsupported"),
			TEXT("actors.robot.goal_xy_m은 더 이상 지원하지 않음. actors.robot.route.goal_xy_m을 사용해야 함."));
	}

	if (robotObject->HasField(TEXT("goal_m")))
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("legacy_robot_goal_unsupported"),
			TEXT("actors.robot.goal_m은 더 이상 지원하지 않음. actors.robot.route.goal_xy_m을 사용해야 함."));
	}

	if (robotObject->HasField(TEXT("drive"))
		|| robotObject->HasField(TEXT("path_follow"))
		|| robotObject->HasField(TEXT("lidar")))
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Error,
			TEXT("delivery_bot_setup_fields_in_episode_setup"),
			TEXT("actors.robot.drive/path_follow/lidar는 더 이상 EpisodeSetup에서 지원하지 않음. DeliveryBotSetup JSON으로 분리해야 함."));
	}

	TSharedPtr<FJsonObject> routeObject;
	if (TryGetObjectField(*robotObject, TEXT("route"), routeObject))
	{
		FVector goalLocationCm{ FVector::ZeroVector };

		if (routeObject->HasField(TEXT("goal_m")))
		{
			AddDiagnostic(
				result,
				EEpisodeCompileDiagnosticSeverity::Error,
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

	robotSetupInfo.LocationSetupInfo.bAutoStartRoute =
		!bSpawnOnly && robotSetupInfo.LocationSetupInfo.bAutoStartRoute && bHasGoal;
	robotSpec.DeliveryBot.bHasGoalLocation = bHasGoal;

	AddJsonProperties(*robotObject, robotSpec.Properties);

	if (!bSpawnOnly && !bHasGoal)
	{
		AddDiagnostic(
			result,
			EEpisodeCompileDiagnosticSeverity::Warning,
			TEXT("missing_robot_goal"),
			TEXT("actors.robot.spawn_only가 false이지만 route.goal_xy_m이 없어 로봇 경로 주입을 건너뜀."));
	}

	result.WorldSpec.Placeables.Add(robotSpec);
}

void UEpisodeCompiler::CompileActors(
	const FJsonObject& rootObject,
	FEpisodeCompileResult& result,
	const TSet<FString>& pathIds)
{
	TSharedPtr<FJsonObject> actorsObject;
	if (!TryGetObjectField(rootObject, TEXT("actors"), actorsObject))
	{
		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Warning, TEXT("missing_actors"),
		              TEXT("actors 필드가 없음."));
		return;
	}

	TSet<FString> instanceIds;
	CompileStaticObstacles(*actorsObject, result, instanceIds);
	CompilePedestrians(*actorsObject, result, pathIds, instanceIds);
	CompileRobotSpawn(*actorsObject, result, instanceIds);
}

void UEpisodeCompiler::CompileRootObject(const FJsonObject& rootObject, const FString& sourceJson,
                                         FEpisodeCompileResult& result)
{
	CompileRunConfig(rootObject, result);
	CompileEvaluationConfig(rootObject, result);
	CompileGroundRegions(rootObject, result);

	TSet<FString> pathIds;
	CompilePaths(rootObject, result, pathIds);
	CompileActors(rootObject, result, pathIds);

	result.WorldSpec.SpecHash = FString::Printf(TEXT("%u"), GetTypeHash(sourceJson));
}

FEpisodeCompileResult UEpisodeCompiler::CompileEpisodeWorldSpecFromJsonFile(const FString& jsonFilePath) const
{
	FEpisodeCompileResult result;
	const FString resolvedJsonFilePath = ResolveEpisodeJsonFilePath(jsonFilePath);

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *resolvedJsonFilePath))
	{
		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("file_read_failed"),
		              FString::Printf(TEXT("JSON 파일 '%s' 읽기 실패. ResolvedPath: '%s'"), *jsonFilePath, *resolvedJsonFilePath));
		result.bSuccess = false;
		return result;
	}

	return CompileEpisodeWorldSpecFromJsonString(jsonString);
}

FEpisodeCompileResult UEpisodeCompiler::CompileEpisodeWorldSpecFromJsonString(const FString& jsonString) const
{
	FEpisodeCompileResult result;

	if (jsonString.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("empty_json"), TEXT("JSON 입력이 비어 있음."));
		result.bSuccess = false;
		return result;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		AddDiagnostic(result, EEpisodeCompileDiagnosticSeverity::Error, TEXT("invalid_json"),
		              TEXT("JSON 입력을 파싱할 수 없음."));
		result.bSuccess = false;
		return result;
	}

	CompileRootObject(*rootObject, jsonString, result);
	result.bSuccess = !HasErrors(result);
	return result;
}
