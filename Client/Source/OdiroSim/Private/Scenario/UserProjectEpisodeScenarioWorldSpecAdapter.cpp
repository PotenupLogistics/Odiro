#include "Scenario/UserProjectEpisodeScenarioWorldSpecAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* EpisodeScenarioSchemaName = TEXT("episode_scenario");
	const double MetersToCentimeters = 100.0;

	struct FProjectScenarioSegment
	{
		FString SegmentId;
		double StartMeters = 0.0;
		double EndMeters = 0.0;
		double WalkwayWidthMeters = 3.0;
	};

	FString ResolveEpisodeScenarioPath(const FString& jsonFilePath)
	{
		if (jsonFilePath.IsEmpty() || !FPaths::IsRelative(jsonFilePath))
		{
			return jsonFilePath;
		}

		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), jsonFilePath));
	}

	void AddAdapterDiagnostic(
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

	bool HasErrorDiagnostics(const TArray<FScenarioCompileDiagnostic>& diagnostics)
	{
		for (const FScenarioCompileDiagnostic& diagnostic : diagnostics)
		{
			if (diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	bool TryLoadJsonObject(const FString& jsonFilePath, TSharedPtr<FJsonObject>& outRootObject, FString& outJsonString)
	{
		outRootObject.Reset();
		outJsonString.Reset();

		if (!FFileHelper::LoadFileToString(outJsonString, *ResolveEpisodeScenarioPath(jsonFilePath)))
		{
			return false;
		}

		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(outJsonString);
		return FJsonSerializer::Deserialize(reader, outRootObject) && outRootObject.IsValid();
	}

	bool TryReadEpisodeScenarioSchema(const FString& jsonFilePath, FString& outSchema)
	{
		TSharedPtr<FJsonObject> rootObject;
		FString jsonString;
		return TryLoadJsonObject(jsonFilePath, rootObject, jsonString)
			&& rootObject.IsValid()
			&& rootObject->TryGetStringField(TEXT("schema"), outSchema);
	}

	bool TryGetObjectField(
		const FJsonObject& object,
		const FString& fieldName,
		const FString& path,
		FScenarioCompileResult& result,
		TSharedPtr<FJsonObject>& outObject)
	{
		outObject.Reset();
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid() || value->Type != EJson::Object)
		{
			AddAdapterDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				FString::Printf(TEXT("missing_%s"), *fieldName),
				FString::Printf(TEXT("%s.%s must be an object."), *path, *fieldName));
			return false;
		}

		outObject = value->AsObject();
		return outObject.IsValid();
	}

	const TArray<TSharedPtr<FJsonValue>>* TryGetArrayField(const FJsonObject& object, const FString& fieldName)
	{
		const TSharedPtr<FJsonValue> value = object.TryGetField(fieldName);
		if (!value.IsValid() || value->Type != EJson::Array)
		{
			return nullptr;
		}

		return &value->AsArray();
	}

	FString ReadStringOrDefault(const FJsonObject& object, const FString& fieldName, const FString& defaultValue = FString())
	{
		FString value;
		return object.TryGetStringField(fieldName, value) ? value.TrimStartAndEnd() : defaultValue;
	}

	double ReadNumberOrDefault(const FJsonObject& object, const FString& fieldName, double defaultValue)
	{
		double value = defaultValue;
		return object.TryGetNumberField(fieldName, value) ? value : defaultValue;
	}

	int32 ReadIntegerOrDefault(const FJsonObject& object, const FString& fieldName, int32 defaultValue)
	{
		return FMath::RoundToInt(ReadNumberOrDefault(object, fieldName, static_cast<double>(defaultValue)));
	}

	FScenarioParamValue MakeBoolParam(bool value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Bool;
		paramValue.BoolValue = value;
		return paramValue;
	}

	FScenarioParamValue MakeIntegerParam(int32 value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Integer;
		paramValue.IntegerValue = value;
		return paramValue;
	}

	FScenarioParamValue MakeFloatParam(double value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Float;
		paramValue.FloatValue = value;
		return paramValue;
	}

	FScenarioParamValue MakeStringParam(const FString& value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::String;
		paramValue.StringValue = value;
		return paramValue;
	}

	EScenarioGroundRegionType ToGroundRegionType(const FString& surfaceId)
	{
		const FString normalized = surfaceId.ToLower();
		if (normalized == TEXT("wall") || normalized == TEXT("building"))
		{
			return EScenarioGroundRegionType::Blocked;
		}
		if (normalized == TEXT("grass") || normalized == TEXT("road") || normalized == TEXT("driveway"))
		{
			return EScenarioGroundRegionType::Penalty;
		}
		return EScenarioGroundRegionType::Walkable;
	}

	double ToTraversabilityScore(EScenarioGroundRegionType regionType)
	{
		switch (regionType)
		{
		case EScenarioGroundRegionType::Blocked:
			return 0.0;
		case EScenarioGroundRegionType::Penalty:
			return 0.5;
		default:
			return 1.0;
		}
	}

	void AddGroundRegion(
		FScenarioWorldSpec& worldSpec,
		const FString& regionId,
		const FString& surfaceId,
		double startMeters,
		double endMeters,
		double centerOffsetMeters,
		double widthMeters)
	{
		if (endMeters <= startMeters || widthMeters <= 0.0)
		{
			return;
		}

		FScenarioGroundRegionSpec region;
		region.RegionId = regionId;
		region.RegionType = ToGroundRegionType(surfaceId);
		region.ShapeType = EScenarioGroundShapeType::Rectangle;
		region.Center = FVector((startMeters + endMeters) * 0.5 * MetersToCentimeters, centerOffsetMeters * MetersToCentimeters, 0.0);
		region.Size = FVector2D((endMeters - startMeters) * MetersToCentimeters, widthMeters * MetersToCentimeters);
		region.YawDegrees = 0.0;
		region.TraversabilityScore = ToTraversabilityScore(region.RegionType);
		if (region.RegionType == EScenarioGroundRegionType::Penalty)
		{
			region.PenaltyKind = surfaceId;
			region.PenaltyCost = 1.0;
		}
		else if (region.RegionType == EScenarioGroundRegionType::Blocked)
		{
			region.CollisionTag = surfaceId;
		}
		worldSpec.GroundRegions.Add(region);
	}

	bool TryParseSegments(
		const FJsonObject& semanticObject,
		FScenarioCompileResult& result,
		TArray<FProjectScenarioSegment>& outSegments)
	{
		outSegments.Reset();

		TSharedPtr<FJsonObject> corridorObject;
		if (!TryGetObjectField(semanticObject, TEXT("corridor"), TEXT("$.semantic"), result, corridorObject))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* segmentValues = TryGetArrayField(*corridorObject, TEXT("segments"));
		if (!segmentValues || segmentValues->IsEmpty())
		{
			AddAdapterDiagnostic(
				result,
				EScenarioCompileDiagnosticSeverity::Error,
				TEXT("missing_segments"),
				TEXT("$.semantic.corridor.segments must be a non-empty array."));
			return false;
		}

		double nextStartMeters = 0.0;
		for (int32 index = 0; index < segmentValues->Num(); ++index)
		{
			const TSharedPtr<FJsonValue>& segmentValue = (*segmentValues)[index];
			if (!segmentValue.IsValid() || segmentValue->Type != EJson::Object)
			{
				AddAdapterDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("invalid_segment"),
					FString::Printf(TEXT("$.semantic.corridor.segments[%d] must be an object."), index));
				continue;
			}

			const TSharedPtr<FJsonObject> segmentObject = segmentValue->AsObject();
			FProjectScenarioSegment segment;
			segment.SegmentId = ReadStringOrDefault(*segmentObject, TEXT("id"), FString::Printf(TEXT("segment_%03d"), index));
			segment.WalkwayWidthMeters = ReadNumberOrDefault(*segmentObject, TEXT("walkway_width_m"), 3.0);

			bool bHasAlongRange = false;
			const TArray<TSharedPtr<FJsonValue>>* alongRangeValues = TryGetArrayField(*segmentObject, TEXT("along_range_m"));
			if (alongRangeValues && alongRangeValues->Num() >= 2
				&& (*alongRangeValues)[0].IsValid()
				&& (*alongRangeValues)[1].IsValid()
				&& (*alongRangeValues)[0]->Type == EJson::Number
				&& (*alongRangeValues)[1]->Type == EJson::Number)
			{
				segment.StartMeters = (*alongRangeValues)[0]->AsNumber();
				segment.EndMeters = (*alongRangeValues)[1]->AsNumber();
				bHasAlongRange = true;
			}

			if (!bHasAlongRange)
			{
				const double lengthMeters = ReadNumberOrDefault(*segmentObject, TEXT("length_m"), 0.0);
				segment.StartMeters = nextStartMeters;
				segment.EndMeters = segment.StartMeters + FMath::Max(lengthMeters, 0.0);
			}

			if (segment.EndMeters <= segment.StartMeters)
			{
				AddAdapterDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("invalid_segment_range"),
					FString::Printf(TEXT("Segment '%s' must have positive length."), *segment.SegmentId));
				continue;
			}

			outSegments.Add(segment);
			nextStartMeters = segment.EndMeters;
		}

		return !outSegments.IsEmpty();
	}

	const FProjectScenarioSegment* FindSegmentById(
		const TArray<FProjectScenarioSegment>& segments,
		const FString& segmentId)
	{
		return segments.FindByPredicate(
			[&segmentId](const FProjectScenarioSegment& segment)
			{
				return segment.SegmentId.Equals(segmentId, ESearchCase::IgnoreCase);
			});
	}

	FVector MakeCorridorLocation(double alongMeters, double offsetMeters)
	{
		return FVector(alongMeters * MetersToCentimeters, offsetMeters * MetersToCentimeters, 0.0);
	}

	void AddCorridorGroundRegions(
		const FJsonObject& semanticObject,
		const TArray<FProjectScenarioSegment>& segments,
		FScenarioCompileResult& result,
		FScenarioWorldSpec& worldSpec)
	{
		TSharedPtr<FJsonObject> corridorObject;
		if (!TryGetObjectField(semanticObject, TEXT("corridor"), TEXT("$.semantic"), result, corridorObject))
		{
			return;
		}

		const auto addSideLanes =
			[&worldSpec](
				const TArray<TSharedPtr<FJsonValue>>* laneValues,
				const FString& sideLabel,
				double sideSign,
				const FProjectScenarioSegment& segment)
			{
				if (!laneValues)
				{
					return;
				}

				double outerEdgeMeters = sideSign * (segment.WalkwayWidthMeters * 0.5);
				for (int32 laneIndex = 0; laneIndex < laneValues->Num(); ++laneIndex)
				{
					const TSharedPtr<FJsonValue>& laneValue = (*laneValues)[laneIndex];
					if (!laneValue.IsValid() || laneValue->Type != EJson::Object)
					{
						continue;
					}

					const TSharedPtr<FJsonObject> laneObject = laneValue->AsObject();
					const FString surfaceId = ReadStringOrDefault(*laneObject, TEXT("surface"), TEXT("sidewalk"));
					const double widthMeters = ReadNumberOrDefault(*laneObject, TEXT("width_m"), 0.0);
					const double centerOffsetMeters = outerEdgeMeters + (sideSign * widthMeters * 0.5);
					AddGroundRegion(
						worldSpec,
						FString::Printf(TEXT("%s_%s_%03d"), *segment.SegmentId, *sideLabel, laneIndex),
						surfaceId,
						segment.StartMeters,
						segment.EndMeters,
						centerOffsetMeters,
						widthMeters);
					outerEdgeMeters += sideSign * widthMeters;
				}
			};

		const TArray<TSharedPtr<FJsonValue>>* buildingSideValues = TryGetArrayField(*corridorObject, TEXT("building_side"));
		const TArray<TSharedPtr<FJsonValue>>* curbSideValues = TryGetArrayField(*corridorObject, TEXT("curb_side"));

		for (const FProjectScenarioSegment& segment : segments)
		{
			AddGroundRegion(
				worldSpec,
				FString::Printf(TEXT("%s_walkway"), *segment.SegmentId),
				TEXT("sidewalk"),
				segment.StartMeters,
				segment.EndMeters,
				0.0,
				segment.WalkwayWidthMeters);
			addSideLanes(buildingSideValues, TEXT("building"), -1.0, segment);
			addSideLanes(curbSideValues, TEXT("curb"), 1.0, segment);
		}
	}

	bool TryReadCorridorPose(
		const FJsonObject& poseObject,
		const TArray<FProjectScenarioSegment>& segments,
		bool bGoalPose,
		FVector& outLocation,
		double& outYawDegrees)
	{
		const FString segmentId = ReadStringOrDefault(poseObject, TEXT("segment"));
		const FProjectScenarioSegment* segment = FindSegmentById(segments, segmentId);
		const double defaultAlongMeters = segment
			? (bGoalPose ? segment->EndMeters : segment->StartMeters)
			: 0.0;
		const double alongMeters = ReadNumberOrDefault(poseObject, TEXT("along_m"), defaultAlongMeters);
		const double offsetMeters = ReadNumberOrDefault(poseObject, TEXT("offset_m"), 0.0);
		outYawDegrees = ReadNumberOrDefault(poseObject, TEXT("yaw_deg"), 0.0);
		outLocation = MakeCorridorLocation(alongMeters, offsetMeters);
		return true;
	}

	void AddRobot(
		const FJsonObject& semanticObject,
		const TArray<FProjectScenarioSegment>& segments,
		FScenarioCompileResult& result,
		FScenarioWorldSpec& worldSpec)
	{
		TSharedPtr<FJsonObject> robotObject;
		TSharedPtr<FJsonObject> startObject;
		TSharedPtr<FJsonObject> goalObject;
		if (!TryGetObjectField(semanticObject, TEXT("robot"), TEXT("$.semantic"), result, robotObject)
			|| !TryGetObjectField(*robotObject, TEXT("start"), TEXT("$.semantic.robot"), result, startObject)
			|| !TryGetObjectField(*robotObject, TEXT("goal"), TEXT("$.semantic.robot"), result, goalObject))
		{
			return;
		}

		FVector startLocationCm;
		FVector goalLocationCm;
		double startYawDegrees = 0.0;
		double goalYawDegrees = 0.0;
		TryReadCorridorPose(*startObject, segments, false, startLocationCm, startYawDegrees);
		TryReadCorridorPose(*goalObject, segments, true, goalLocationCm, goalYawDegrees);

		FScenarioPlaceableInstanceSpec robotSpec;
		robotSpec.InstanceId = TEXT("robot_01");
		robotSpec.AssetId = TEXT("delivery_bot");
		robotSpec.Category = EScenarioActorCategory::DeliveryBot;
		robotSpec.Transform = FTransform(FRotator(0.0, startYawDegrees, 0.0), startLocationCm);
		robotSpec.DeliveryBot.bSpawnOnly = false;
		robotSpec.DeliveryBot.bHasStartLocation = true;
		robotSpec.DeliveryBot.bHasGoalLocation = true;
		robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = startLocationCm;
		robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = goalLocationCm;
		robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bHasGoal = true;
		robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
		worldSpec.Placeables.Add(robotSpec);
	}

	void AddFixedObstacles(
		const FJsonObject& semanticObject,
		FScenarioCompileResult& result,
		FScenarioWorldSpec& worldSpec)
	{
		TSharedPtr<FJsonObject> obstaclesObject;
		if (!TryGetObjectField(semanticObject, TEXT("obstacles"), TEXT("$.semantic"), result, obstaclesObject))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* placementValues = TryGetArrayField(*obstaclesObject, TEXT("placements"));
		if (!placementValues)
		{
			return;
		}

		for (int32 index = 0; index < placementValues->Num(); ++index)
		{
			const TSharedPtr<FJsonValue>& placementValue = (*placementValues)[index];
			if (!placementValue.IsValid() || placementValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> placementObject = placementValue->AsObject();
			const FString kind = ReadStringOrDefault(*placementObject, TEXT("kind"), TEXT("fixed")).ToLower();
			if (kind != TEXT("fixed"))
			{
				AddAdapterDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Warning,
					TEXT("unsupported_project_obstacle_rule"),
					FString::Printf(TEXT("Obstacle placement '%s' kind '%s' is not yet converted to runtime actors."), *ReadStringOrDefault(*placementObject, TEXT("id")), *kind));
				continue;
			}

			TSharedPtr<FJsonObject> atObject;
			if (!TryGetObjectField(*placementObject, TEXT("at"), FString::Printf(TEXT("$.semantic.obstacles.placements[%d]"), index), result, atObject))
			{
				continue;
			}

			const FString propId = ReadStringOrDefault(*placementObject, TEXT("prop"));
			if (propId.IsEmpty())
			{
				AddAdapterDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Error,
					TEXT("missing_obstacle_prop"),
					FString::Printf(TEXT("$.semantic.obstacles.placements[%d].prop is required."), index));
				continue;
			}

			const double alongMeters = ReadNumberOrDefault(*atObject, TEXT("along_m"), 0.0);
			const double offsetMeters = ReadNumberOrDefault(*atObject, TEXT("offset_m"), 0.0);
			const double yawDegrees = ReadNumberOrDefault(*placementObject, TEXT("yaw_deg"), 0.0);

			FScenarioPlaceableInstanceSpec obstacleSpec;
			obstacleSpec.InstanceId = ReadStringOrDefault(*placementObject, TEXT("id"), FString::Printf(TEXT("obstacle_%03d"), index));
			obstacleSpec.AssetId = propId;
			obstacleSpec.Category = EScenarioActorCategory::StaticObstacle;
			obstacleSpec.Transform = FTransform(FRotator(0.0, yawDegrees, 0.0), MakeCorridorLocation(alongMeters, offsetMeters));
			obstacleSpec.Properties.Add(TEXT("placed_by"), MakeStringParam(TEXT("project_scenario")));
			worldSpec.Placeables.Add(obstacleSpec);
		}
	}

	void AddPedestrians(
		const FJsonObject& semanticObject,
		const TArray<FProjectScenarioSegment>& segments,
		FScenarioCompileResult& result,
		FScenarioWorldSpec& worldSpec)
	{
		TSharedPtr<FJsonObject> pedestriansObject;
		if (!TryGetObjectField(semanticObject, TEXT("pedestrians"), TEXT("$.semantic"), result, pedestriansObject))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* encounterValues = TryGetArrayField(*pedestriansObject, TEXT("encounters"));
		if (!encounterValues)
		{
			return;
		}

		for (int32 index = 0; index < encounterValues->Num(); ++index)
		{
			const TSharedPtr<FJsonValue>& encounterValue = (*encounterValues)[index];
			if (!encounterValue.IsValid() || encounterValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> encounterObject = encounterValue->AsObject();
			const FString encounterId = ReadStringOrDefault(*encounterObject, TEXT("id"), FString::Printf(TEXT("pedestrian_%03d"), index));
			const FString atSegmentId = ReadStringOrDefault(*encounterObject, TEXT("at"));
			const FProjectScenarioSegment* segment = FindSegmentById(segments, atSegmentId);
			if (!segment)
			{
				AddAdapterDiagnostic(
					result,
					EScenarioCompileDiagnosticSeverity::Warning,
					TEXT("unknown_encounter_segment"),
					FString::Printf(TEXT("Pedestrian encounter '%s' references unknown segment '%s'."), *encounterId, *atSegmentId));
				continue;
			}

			const double offsetMeters = ReadNumberOrDefault(*encounterObject, TEXT("meet_offset_m"), 0.0);
			const double speedMetersPerSecond = ReadNumberOrDefault(*encounterObject, TEXT("speed_mps"), 1.2);
			const FVector startLocationCm = MakeCorridorLocation(segment->EndMeters - 0.5, offsetMeters);
			const FVector goalLocationCm = MakeCorridorLocation(segment->StartMeters + 0.5, offsetMeters);

			FScenarioDynamicActorSpec actorSpec;
			actorSpec.InstanceId = encounterId;
			actorSpec.AssetId = TEXT("adult_pedestrian");
			actorSpec.Category = EScenarioActorCategory::Pedestrian;
			actorSpec.InitialTransform = FTransform(FRotator(0.0, 180.0, 0.0), startLocationCm);
			actorSpec.PathId = FString::Printf(TEXT("%s_path"), *encounterId);
			actorSpec.Properties.Add(TEXT("movement_model"), MakeStringParam(TEXT("spline_Relative")));
			actorSpec.Properties.Add(TEXT("speed_mps"), MakeFloatParam(speedMetersPerSecond));
			actorSpec.Properties.Add(TEXT("speed_cm_per_second"), MakeFloatParam(speedMetersPerSecond * MetersToCentimeters));
			actorSpec.Properties.Add(TEXT("auto_start"), MakeBoolParam(true));
			actorSpec.Properties.Add(TEXT("role"), MakeStringParam(TEXT("encounter")));
			actorSpec.Properties.Add(TEXT("persona"), MakeStringParam(ReadStringOrDefault(*encounterObject, TEXT("persona"), TEXT("normal"))));
			worldSpec.DynamicActors.Add(actorSpec);

			FScenarioPathSpec pathSpec;
			pathSpec.PathId = actorSpec.PathId;
			pathSpec.PathType = EScenarioPathType::Spline;
			pathSpec.Points.Add(startLocationCm);
			pathSpec.Points.Add(goalLocationCm);
			worldSpec.Paths.Add(pathSpec);
		}
	}

	void PopulateRunConfig(
		const FJsonObject& rootObject,
		const FJsonObject& semanticObject,
		const FString& jsonString,
		FScenarioWorldSpec& worldSpec)
	{
		TSharedPtr<FJsonObject> episodeObject;
		if (const TSharedPtr<FJsonValue> episodeValue = rootObject.TryGetField(TEXT("episode"));
			episodeValue.IsValid() && episodeValue->Type == EJson::Object)
		{
			episodeObject = episodeValue->AsObject();
		}

		const int64 seed = episodeObject.IsValid()
			? static_cast<int64>(ReadNumberOrDefault(*episodeObject, TEXT("seed"), 0.0))
			: 0;
		const FString episodeId = episodeObject.IsValid()
			? ReadStringOrDefault(*episodeObject, TEXT("episode_id"), ReadStringOrDefault(semanticObject, TEXT("scenario_id"), TEXT("episode")))
			: ReadStringOrDefault(semanticObject, TEXT("scenario_id"), TEXT("episode"));

		const FString scenarioId = ReadStringOrDefault(semanticObject, TEXT("scenario_id"), episodeId);
		worldSpec.RunConfig.TemplateId = episodeId;
		worldSpec.RunConfig.TemplateVersion = ReadIntegerOrDefault(semanticObject, TEXT("version"), 1);
		worldSpec.RunConfig.GeneratorVersion = 1;
		worldSpec.RunConfig.BaseSeed = seed;
		worldSpec.RunConfig.IterationIndex = episodeObject.IsValid()
			? ReadIntegerOrDefault(*episodeObject, TEXT("episode_index"), 0)
			: 0;
		worldSpec.RunConfig.Parameters.Add(TEXT("episode_id"), MakeStringParam(episodeId));
		worldSpec.RunConfig.Parameters.Add(TEXT("scenario_id"), MakeStringParam(scenarioId));
		worldSpec.RunConfig.Parameters.Add(TEXT("scenario_schema"), MakeStringParam(ReadStringOrDefault(semanticObject, TEXT("schema"), TEXT("scenario"))));

		worldSpec.Seeds.WorldSeed = seed;
		worldSpec.Seeds.LayoutSeed = seed + 101;
		worldSpec.Seeds.StaticObstacleSeed = seed + 202;
		worldSpec.Seeds.DynamicActorSeed = seed + 303;
		worldSpec.Seeds.EventSeed = seed + 404;
		worldSpec.Seeds.PolicySeed = seed + 505;
		worldSpec.SpecHash = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*jsonString));
	}
}

bool FUserProjectEpisodeScenarioWorldSpecAdapter::IsEpisodeScenarioFile(const FString& jsonFilePath)
{
	FString schema;
	return TryReadEpisodeScenarioSchema(jsonFilePath, schema)
		&& schema.Equals(EpisodeScenarioSchemaName, ESearchCase::CaseSensitive);
}

FScenarioCompileResult FUserProjectEpisodeScenarioWorldSpecAdapter::CompileScenarioWorldSpecFromEpisodeScenarioFile(
	const FString& jsonFilePath)
{
	FScenarioCompileResult result;

	TSharedPtr<FJsonObject> rootObject;
	FString jsonString;
	if (!TryLoadJsonObject(jsonFilePath, rootObject, jsonString) || !rootObject.IsValid())
	{
		AddAdapterDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("episode_scenario_read_failed"),
			FString::Printf(TEXT("episode_scenario JSON read or parse failed: %s"), *jsonFilePath));
		result.bSuccess = false;
		return result;
	}

	const FString schema = ReadStringOrDefault(*rootObject, TEXT("schema"));
	if (!schema.Equals(EpisodeScenarioSchemaName, ESearchCase::CaseSensitive))
	{
		AddAdapterDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("invalid_episode_scenario_schema"),
			FString::Printf(TEXT("$.schema must be '%s'."), EpisodeScenarioSchemaName));
		result.bSuccess = false;
		return result;
	}

	TSharedPtr<FJsonObject> semanticObject;
	if (!TryGetObjectField(*rootObject, TEXT("semantic"), TEXT("$"), result, semanticObject))
	{
		result.bSuccess = false;
		return result;
	}

	TArray<FProjectScenarioSegment> segments;
	if (!TryParseSegments(*semanticObject, result, segments))
	{
		result.bSuccess = false;
		return result;
	}

	FScenarioWorldSpec worldSpec;
	PopulateRunConfig(*rootObject, *semanticObject, jsonString, worldSpec);
	AddCorridorGroundRegions(*semanticObject, segments, result, worldSpec);
	AddRobot(*semanticObject, segments, result, worldSpec);
	AddFixedObstacles(*semanticObject, result, worldSpec);
	AddPedestrians(*semanticObject, segments, result, worldSpec);

	if (worldSpec.Placeables.IsEmpty())
	{
		AddAdapterDiagnostic(
			result,
			EScenarioCompileDiagnosticSeverity::Error,
			TEXT("missing_runtime_placeables"),
			TEXT("episode_scenario did not produce any runtime placeable actors."));
	}

	result.WorldSpec = worldSpec;
	result.bSuccess = !HasErrorDiagnostics(result.Diagnostics);
	return result;
}
