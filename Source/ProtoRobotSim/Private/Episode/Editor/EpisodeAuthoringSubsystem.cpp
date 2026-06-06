#include "Episode/Editor/EpisodeAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/EpisodeCompiler.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const FString SpeedMpsKey(TEXT("speed_mps"));
	const FString SpeedCmPerSecondKey(TEXT("speed_cm_per_second"));
	const FString InitialDistanceMKey(TEXT("initial_distance_m"));
	const FString InitialDistanceCmKey(TEXT("initial_distance_cm"));
	const FString AutoStartKey(TEXT("auto_start"));
	const FString MovementModelKey(TEXT("movement_model"));
	const FString PlannedStartCmKey(TEXT("planned_start_cm"));
	const FString PlannedGoalCmKey(TEXT("planned_goal_cm"));
}

UEpisodeAuthoringSubsystem::UEpisodeAuthoringSubsystem()
{
	static ConstructorHelpers::FClassFinder<AActor> startPointBlueprintClass(TEXT("/Game/Blueprints/Episode/BP_StartPoint"));
	if (startPointBlueprintClass.Succeeded())
	{
		StartPointClass = startPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> goalPointBlueprintClass(TEXT("/Game/Blueprints/Episode/BP_GoalPoint"));
	if (goalPointBlueprintClass.Succeeded())
	{
		GoalPointClass = goalPointBlueprintClass.Class;
	}
}

void UEpisodeAuthoringSubsystem::Deinitialize()
{
	ClearDraft();
	Super::Deinitialize();
}

void UEpisodeAuthoringSubsystem::ClearDraft()
{
	ClearEditorView();
	DraftWorldSpec = FEpisodeWorldSpec();
	SourceEpisodeSetupJsonPath.Reset();
	bDirty = false;
	NextStaticObstacleIndex = 1;
}

void UEpisodeAuthoringSubsystem::NewDraft()
{
	ClearDraft();
	InitializeDraftDefaults();
}

bool UEpisodeAuthoringSubsystem::LoadEpisodeSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	const FString trimmedJsonFilePath = jsonFilePath.TrimStartAndEnd();
	if (trimmedJsonFilePath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("EpisodeSetup JSON file path is empty."));
		return false;
	}

	outResolvedJsonFilePath = ResolveEpisodeSetupLoadPath(trimmedJsonFilePath);

	UEpisodeCompiler* compiler = NewObject<UEpisodeCompiler>();
	if (!compiler)
	{
		outDiagnostics.Add(TEXT("Episode compiler creation failed."));
		return false;
	}

	const FEpisodeCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonFile(outResolvedJsonFilePath);
	AppendCompileDiagnostics(compileResult, outDiagnostics);
	if (!compileResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("EpisodeSetup JSON import failed compiler validation."));
		return false;
	}

	DraftWorldSpec = compileResult.WorldSpec;
	SourceEpisodeSetupJsonPath = outResolvedJsonFilePath;
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UEpisodeAuthoringSubsystem::LoadEpisodeSetupJsonString(
	const FString& jsonString,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (jsonString.IsEmpty())
	{
		outDiagnostics.Add(TEXT("EpisodeSetup JSON string is empty."));
		return false;
	}

	UEpisodeCompiler* compiler = NewObject<UEpisodeCompiler>();
	if (!compiler)
	{
		outDiagnostics.Add(TEXT("Episode compiler creation failed."));
		return false;
	}

	const FEpisodeCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonString(jsonString);
	AppendCompileDiagnostics(compileResult, outDiagnostics);
	if (!compileResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("EpisodeSetup JSON import failed compiler validation."));
		return false;
	}

	DraftWorldSpec = compileResult.WorldSpec;
	SourceEpisodeSetupJsonPath.Reset();
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UEpisodeAuthoringSubsystem::ImportCompiledWorldSpec(
	const FEpisodeWorldSpec& worldSpec,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	DraftWorldSpec = worldSpec;
	SourceEpisodeSetupJsonPath.Reset();
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

void UEpisodeAuthoringSubsystem::GetStaticObstaclePaletteEntries(TArray<FEpisodeStaticObstaclePropEntry>& outEntries) const
{
	outEntries = AEpisodeStaticObstacle::GetDefaultPropEntries();
}

void UEpisodeAuthoringSubsystem::GetAuthoredStaticObstacleActors(TArray<AEpisodeStaticObstacle*>& outActors) const
{
	outActors.Reset();
	outActors.Reserve(StaticObstacleActors.Num());

	for (const TPair<FString, TObjectPtr<AEpisodeStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (AEpisodeStaticObstacle* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}
}

bool UEpisodeAuthoringSubsystem::CanPlaceStaticObstacle(
	FName propId,
	const FTransform& transform,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	FEpisodeStaticObstaclePropEntry candidateProp;
	if (!TryFindStaticObstacleProp(propId, candidateProp))
	{
		outFailureReason = FString::Printf(TEXT("Unknown static obstacle prop '%s'."), *propId.ToString());
		return false;
	}

	const FVector2D candidateHalfExtent = ComputePlacementHalfExtent2D(candidateProp);
	const FVector candidateLocation = transform.GetLocation();
	if (candidateLocation.Z > StaticObstacleGroundZToleranceCm)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be %.2f cm or lower. Current Z: %.2f."),
			StaticObstacleGroundZToleranceCm,
			candidateLocation.Z);
		return false;
	}

	for (const FEpisodeAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (StaticObstacleFootprintsOverlap(candidateLocation, candidateHalfExtent, record))
		{
			outFailureReason = FString::Printf(
				TEXT("Overlaps static obstacle '%s'."), *record.InstanceId);
			return false;
		}
	}

	return true;
}

bool UEpisodeAuthoringSubsystem::AddStaticObstacle(
	FName propId,
	const FTransform& transform,
	FEpisodePlaceableInstanceSpec& outSpec)
{
	AEpisodeStaticObstacle* spawnedActor = nullptr;
	return AddStaticObstacleInternal(propId, transform, outSpec, spawnedActor);
}

bool UEpisodeAuthoringSubsystem::AddStaticObstacleInternal(
	FName propId,
	const FTransform& transform,
	FEpisodePlaceableInstanceSpec& outSpec,
	AEpisodeStaticObstacle*& outActor)
{
	outActor = nullptr;
	outSpec = FEpisodePlaceableInstanceSpec();

	FString failureReason;
	if (!CanPlaceStaticObstacle(propId, transform, failureReason))
	{
		return false;
	}

	if (DraftWorldSpec.RunConfig.TemplateId.IsEmpty()
		&& DraftWorldSpec.Placeables.IsEmpty()
		&& DraftWorldSpec.DynamicActors.IsEmpty()
		&& DraftWorldSpec.GroundRegions.IsEmpty()
		&& DraftWorldSpec.Paths.IsEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString instanceId = GenerateStaticObstacleInstanceId();
	outSpec = MakeStaticObstacleSpec(instanceId, propId, transform);

	if (!SpawnEditorStaticObstacleActor(outSpec, outActor, failureReason))
	{
		return false;
	}

	FEpisodeStaticObstaclePropEntry propEntry;
	TryFindStaticObstacleProp(propId, propEntry);
	AddStaticObstacleViewRecord(outSpec, propEntry, outActor);
	DraftWorldSpec.Placeables.Add(outSpec);
	DraftWorldSpec.SpecHash.Reset();
	bDirty = true;
	return true;
}

TArray<FEpisodePlaceableInstanceSpec> UEpisodeAuthoringSubsystem::GetAuthoredStaticObstacleSpecs() const
{
	TArray<FEpisodePlaceableInstanceSpec> staticObstacleSpecs;
	for (const FEpisodePlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EEpisodeActorCategory::StaticObstacle)
		{
			staticObstacleSpecs.Add(spec);
		}
	}

	return staticObstacleSpecs;
}

bool UEpisodeAuthoringSubsystem::ExportEpisodeSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), TEXT("episode_actor_spawn_mvp"));
	rootObject->SetNumberField(TEXT("version"), DraftWorldSpec.RunConfig.TemplateVersion > 0 ? DraftWorldSpec.RunConfig.TemplateVersion : 1);
	rootObject->SetStringField(
		TEXT("scenario_id"),
		DraftWorldSpec.RunConfig.TemplateId.IsEmpty() ? ScenarioId : DraftWorldSpec.RunConfig.TemplateId);
	rootObject->SetStringField(TEXT("map_id"), MapId);

	TSharedRef<FJsonObject> runObject = MakeShared<FJsonObject>();
	runObject->SetNumberField(TEXT("base_seed"), static_cast<double>(DraftWorldSpec.RunConfig.BaseSeed));
	runObject->SetNumberField(TEXT("iteration_index"), DraftWorldSpec.RunConfig.IterationIndex);

	double timeLimitSeconds = TimeLimitSeconds;
	if (TryGetFloatProperty(DraftWorldSpec.RunConfig.Parameters, TEXT("time_limit_s"), timeLimitSeconds))
	{
		timeLimitSeconds = FMath::Max(timeLimitSeconds, 0.0);
	}
	runObject->SetNumberField(TEXT("time_limit_s"), timeLimitSeconds);
	rootObject->SetObjectField(TEXT("run"), runObject);

	TSharedRef<FJsonObject> evaluationObject = MakeShared<FJsonObject>();
	evaluationObject->SetNumberField(
		TEXT("goal_acceptance_radius_m"),
		DraftWorldSpec.EvaluationConfig.GoalAcceptanceRadiusCm * CentimetersToMeters);
	evaluationObject->SetNumberField(TEXT("tip_over_angle_deg"), DraftWorldSpec.EvaluationConfig.TipOverAngleDegrees);

	TSharedRef<FJsonObject> nearMissObject = MakeShared<FJsonObject>();
	nearMissObject->SetNumberField(TEXT("distance_m"), DraftWorldSpec.EvaluationConfig.NearMissDistanceCm * CentimetersToMeters);
	evaluationObject->SetObjectField(TEXT("near_miss"), nearMissObject);

	TSharedRef<FJsonObject> scoringObject = MakeShared<FJsonObject>();
	scoringObject->SetNumberField(TEXT("static_obstacle_collision"), DraftWorldSpec.EvaluationConfig.StaticObstacleCollisionScore);
	scoringObject->SetNumberField(TEXT("blocked_region_collision"), DraftWorldSpec.EvaluationConfig.BlockedRegionCollisionScore);
	scoringObject->SetNumberField(TEXT("penalty_region_violation"), DraftWorldSpec.EvaluationConfig.PenaltyRegionViolationScore);
	scoringObject->SetNumberField(TEXT("pedestrian_near_miss"), DraftWorldSpec.EvaluationConfig.PedestrianNearMissScore);
	scoringObject->SetNumberField(TEXT("pedestrian_collision"), DraftWorldSpec.EvaluationConfig.PedestrianCollisionScore);
	evaluationObject->SetObjectField(TEXT("scoring"), scoringObject);
	rootObject->SetObjectField(TEXT("evaluation"), evaluationObject);

	TSharedRef<FJsonObject> groundModelObject = MakeShared<FJsonObject>();
	groundModelObject->SetStringField(TEXT("default_region_type"), TEXT("walkable"));
	TArray<TSharedPtr<FJsonValue>> groundRegionValues;
	groundRegionValues.Reserve(DraftWorldSpec.GroundRegions.Num());
	for (const FEpisodeGroundRegionSpec& regionSpec : DraftWorldSpec.GroundRegions)
	{
		TSharedRef<FJsonObject> regionObject = MakeShared<FJsonObject>();
		regionObject->SetStringField(TEXT("region_id"), regionSpec.RegionId);
		regionObject->SetStringField(TEXT("region_type"), GroundRegionTypeToString(regionSpec.RegionType));

		TSharedRef<FJsonObject> shapeObject = MakeShared<FJsonObject>();
		shapeObject->SetStringField(TEXT("type"), GroundShapeTypeToString(regionSpec.ShapeType));
		shapeObject->SetArrayField(TEXT("center_xy_m"), MakeXyArrayMeters(regionSpec.Center));
		shapeObject->SetArrayField(TEXT("size_m"), MakeSizeArrayMeters(regionSpec.Size));
		shapeObject->SetNumberField(TEXT("yaw_deg"), regionSpec.YawDegrees);
		regionObject->SetObjectField(TEXT("shape"), shapeObject);

		regionObject->SetNumberField(TEXT("traversability_score"), regionSpec.TraversabilityScore);
		if (!regionSpec.PenaltyKind.IsEmpty() || !FMath::IsNearlyZero(regionSpec.PenaltyCost) || !FMath::IsNearlyZero(regionSpec.ViolationAfterSeconds))
		{
			TSharedRef<FJsonObject> penaltyObject = MakeShared<FJsonObject>();
			if (!regionSpec.PenaltyKind.IsEmpty())
			{
				penaltyObject->SetStringField(TEXT("kind"), regionSpec.PenaltyKind);
			}
			penaltyObject->SetNumberField(TEXT("cost"), regionSpec.PenaltyCost);
			penaltyObject->SetNumberField(TEXT("violation_after_s"), regionSpec.ViolationAfterSeconds);
			regionObject->SetObjectField(TEXT("penalty"), penaltyObject);
		}
		if (!regionSpec.CollisionTag.IsEmpty())
		{
			regionObject->SetStringField(TEXT("collision_tag"), regionSpec.CollisionTag);
		}

		groundRegionValues.Add(MakeShared<FJsonValueObject>(regionObject));
	}
	groundModelObject->SetArrayField(TEXT("regions"), groundRegionValues);
	rootObject->SetObjectField(TEXT("ground_model"), groundModelObject);

	TArray<TSharedPtr<FJsonValue>> pathValues;
	pathValues.Reserve(DraftWorldSpec.Paths.Num());
	for (const FEpisodePathSpec& pathSpec : DraftWorldSpec.Paths)
	{
		TSharedRef<FJsonObject> pathObject = MakeShared<FJsonObject>();
		pathObject->SetStringField(TEXT("path_id"), pathSpec.PathId);

		TArray<TSharedPtr<FJsonValue>> pointValues;
		pointValues.Reserve(pathSpec.Points.Num());
		for (const FVector& pointCm : pathSpec.Points)
		{
			pointValues.Add(MakeShared<FJsonValueArray>(MakeXyArrayMeters(pointCm)));
		}
		pathObject->SetArrayField(TEXT("points_xy_m"), pointValues);
		pathObject->SetBoolField(TEXT("closed_loop"), pathSpec.bClosedLoop);
		pathValues.Add(MakeShared<FJsonValueObject>(pathObject));
	}
	rootObject->SetArrayField(TEXT("paths"), pathValues);

	TSharedRef<FJsonObject> actorsObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> staticObstacleValues;
	staticObstacleValues.Reserve(DraftWorldSpec.Placeables.Num());
	for (const FEpisodePlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category != EEpisodeActorCategory::StaticObstacle)
		{
			continue;
		}

		TSharedRef<FJsonObject> obstacleObject = MakeShared<FJsonObject>();
		obstacleObject->SetStringField(TEXT("instance_id"), spec.InstanceId);
		obstacleObject->SetStringField(TEXT("prop_id"), spec.AssetId);
		obstacleObject->SetArrayField(TEXT("xy_m"), MakeXyArrayMeters(spec.Transform.GetLocation()));
		obstacleObject->SetNumberField(TEXT("yaw_deg"), spec.Transform.Rotator().Yaw);

		TSharedPtr<FJsonObject> propertiesObject = MakePropertiesObject(spec.Properties);
		if (propertiesObject.IsValid())
		{
			obstacleObject->SetObjectField(TEXT("properties"), propertiesObject);
		}

		staticObstacleValues.Add(MakeShared<FJsonValueObject>(obstacleObject));
	}
	actorsObject->SetArrayField(TEXT("static_obstacles"), staticObstacleValues);

	TArray<TSharedPtr<FJsonValue>> pedestrianValues;
	pedestrianValues.Reserve(DraftWorldSpec.DynamicActors.Num());
	for (const FEpisodeDynamicActorSpec& dynamicActorSpec : DraftWorldSpec.DynamicActors)
	{
		if (dynamicActorSpec.Category != EEpisodeActorCategory::Pedestrian)
		{
			continue;
		}

		TSharedRef<FJsonObject> pedestrianObject = MakeShared<FJsonObject>();
		pedestrianObject->SetStringField(TEXT("instance_id"), dynamicActorSpec.InstanceId);
		if (!dynamicActorSpec.AssetId.IsEmpty())
		{
			pedestrianObject->SetStringField(TEXT("archetype_id"), dynamicActorSpec.AssetId);
		}

		FString movementModel;
		TryGetStringProperty(dynamicActorSpec.Properties, MovementModelKey, movementModel);
		if (!movementModel.Equals(TEXT("planned_trajectory"), ESearchCase::IgnoreCase))
		{
			pedestrianObject->SetStringField(TEXT("path_id"), dynamicActorSpec.PathId);
		}
		else
		{
			FVector plannedStartCm = dynamicActorSpec.InitialTransform.GetLocation();
			FVector plannedGoalCm = FVector::ZeroVector;
			const FEpisodeParamValue* startParam = dynamicActorSpec.Properties.Find(PlannedStartCmKey);
			const FEpisodeParamValue* goalParam = dynamicActorSpec.Properties.Find(PlannedGoalCmKey);
			if (startParam && startParam->Type == EEpisodeParamValueType::Vector)
			{
				plannedStartCm = startParam->VectorValue;
			}
			if (goalParam && goalParam->Type == EEpisodeParamValueType::Vector)
			{
				plannedGoalCm = goalParam->VectorValue;
			}
			pedestrianObject->SetArrayField(TEXT("start_xy_m"), MakeXyArrayMeters(plannedStartCm));
			pedestrianObject->SetArrayField(TEXT("goal_xy_m"), MakeXyArrayMeters(plannedGoalCm));
			if (!dynamicActorSpec.PathId.IsEmpty())
			{
				pedestrianObject->SetStringField(TEXT("path_id"), dynamicActorSpec.PathId);
			}
		}

		pedestrianObject->SetArrayField(TEXT("xy_m"), MakeXyArrayMeters(dynamicActorSpec.InitialTransform.GetLocation()));
		pedestrianObject->SetNumberField(TEXT("yaw_deg"), dynamicActorSpec.InitialTransform.Rotator().Yaw);

		TSharedRef<FJsonObject> movementObject = MakeShared<FJsonObject>();
		movementObject->SetStringField(TEXT("model"), movementModel.IsEmpty() ? TEXT("spline_Relative") : movementModel);

		double speedCmPerSecond = 120.0;
		if (TryGetFloatProperty(dynamicActorSpec.Properties, SpeedCmPerSecondKey, speedCmPerSecond))
		{
			movementObject->SetNumberField(TEXT("speed_mps"), speedCmPerSecond * CentimetersToMeters);
		}
		else
		{
			double speedMps = 1.2;
			if (TryGetFloatProperty(dynamicActorSpec.Properties, SpeedMpsKey, speedMps))
			{
				movementObject->SetNumberField(TEXT("speed_mps"), speedMps);
			}
		}

		double initialDistanceCm = 0.0;
		if (TryGetFloatProperty(dynamicActorSpec.Properties, InitialDistanceCmKey, initialDistanceCm))
		{
			movementObject->SetNumberField(TEXT("initial_distance_m"), initialDistanceCm * CentimetersToMeters);
		}
		else
		{
			double initialDistanceM = 0.0;
			if (TryGetFloatProperty(dynamicActorSpec.Properties, InitialDistanceMKey, initialDistanceM))
			{
				movementObject->SetNumberField(TEXT("initial_distance_m"), initialDistanceM);
			}
		}

		bool bAutoStart = true;
		if (TryGetBoolProperty(dynamicActorSpec.Properties, AutoStartKey, bAutoStart))
		{
			movementObject->SetBoolField(TEXT("auto_start"), bAutoStart);
		}
		pedestrianObject->SetObjectField(TEXT("movement"), movementObject);

		const TSet<FString> excludedPedestrianKeys =
		{
			SpeedMpsKey,
			SpeedCmPerSecondKey,
			InitialDistanceMKey,
			InitialDistanceCmKey,
			AutoStartKey,
			MovementModelKey,
			PlannedStartCmKey,
			PlannedGoalCmKey
		};
		TSharedPtr<FJsonObject> propertiesObject = MakeFilteredPropertiesObject(dynamicActorSpec.Properties, excludedPedestrianKeys);
		if (propertiesObject.IsValid())
		{
			pedestrianObject->SetObjectField(TEXT("properties"), propertiesObject);
		}

		pedestrianValues.Add(MakeShared<FJsonValueObject>(pedestrianObject));
	}
	actorsObject->SetArrayField(TEXT("pedestrians"), pedestrianValues);

	for (const FEpisodePlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category != EEpisodeActorCategory::DeliveryBot && spec.Category != EEpisodeActorCategory::RoadVehicle)
		{
			continue;
		}

		TSharedRef<FJsonObject> robotObject = MakeShared<FJsonObject>();
		robotObject->SetStringField(TEXT("instance_id"), spec.InstanceId);
		robotObject->SetStringField(TEXT("asset_id"), spec.AssetId.IsEmpty() ? TEXT("delivery_bot") : spec.AssetId);
		robotObject->SetBoolField(TEXT("spawn_only"), spec.DeliveryBot.bSpawnOnly);
		robotObject->SetArrayField(TEXT("xy_m"), MakeXyArrayMeters(spec.Transform.GetLocation()));
		robotObject->SetNumberField(TEXT("yaw_deg"), spec.Transform.Rotator().Yaw);

		if (spec.DeliveryBot.bHasGoalLocation)
		{
			TSharedRef<FJsonObject> routeObject = MakeShared<FJsonObject>();
			routeObject->SetArrayField(
				TEXT("goal_xy_m"),
				MakeXyArrayMeters(spec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm));
			routeObject->SetBoolField(TEXT("auto_start"), spec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute);
			robotObject->SetObjectField(TEXT("route"), routeObject);
		}

		TSharedPtr<FJsonObject> propertiesObject = MakePropertiesObject(spec.Properties);
		if (propertiesObject.IsValid())
		{
			robotObject->SetObjectField(TEXT("properties"), propertiesObject);
		}

		actorsObject->SetObjectField(TEXT("robot"), robotObject);
		break;
	}

	rootObject->SetObjectField(TEXT("actors"), actorsObject);

	const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outJsonString);
	if (!FJsonSerializer::Serialize(rootObject, writer))
	{
		outDiagnostics.Add(TEXT("EpisodeSetup JSON serialization failed."));
		return false;
	}

	return true;
}

bool UEpisodeAuthoringSubsystem::ExportAndValidateEpisodeSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	if (!ExportEpisodeSetupJsonString(outJsonString, outDiagnostics))
	{
		return false;
	}

	UEpisodeCompiler* compiler = NewObject<UEpisodeCompiler>();
	if (!compiler)
	{
		outDiagnostics.Add(TEXT("Episode compiler creation failed."));
		return false;
	}

	const FEpisodeCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonString(outJsonString);
	AppendCompileDiagnostics(compileResult, outDiagnostics);

	if (!compileResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("Exported EpisodeSetup JSON failed compiler validation."));
	}

	return compileResult.bSuccess;
}

bool UEpisodeAuthoringSubsystem::SaveEpisodeSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (jsonFilePath.IsEmpty())
	{
		outResolvedJsonFilePath.Reset();
		outDiagnostics.Add(TEXT("EpisodeSetup JSON file path is empty."));
		return false;
	}

	outResolvedJsonFilePath = ResolveProjectRelativePath(jsonFilePath);

	FString jsonString;
	if (!ExportAndValidateEpisodeSetupJsonString(jsonString, outDiagnostics))
	{
		return false;
	}

	const FString directory = FPaths::GetPath(outResolvedJsonFilePath);
	if (!directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*directory, true);
	}

	if (!FFileHelper::SaveStringToFile(jsonString, *outResolvedJsonFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("Failed to save EpisodeSetup JSON to '%s'."), *outResolvedJsonFilePath));
		return false;
	}

	SourceEpisodeSetupJsonPath = outResolvedJsonFilePath;
	bDirty = false;
	return true;
}

FString UEpisodeAuthoringSubsystem::ResolveProjectRelativePath(const FString& filePath)
{
	if (FPaths::IsRelative(filePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), filePath);
	}

	return filePath;
}

FString UEpisodeAuthoringSubsystem::ResolveEpisodeSetupLoadPath(const FString& filePath) const
{
	FString normalizedPath = filePath;
	normalizedPath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedPath);

	if (FPaths::GetExtension(normalizedPath).IsEmpty())
	{
		normalizedPath = FPaths::SetExtension(normalizedPath, TEXT("json"));
	}

	if (!FPaths::IsRelative(normalizedPath))
	{
		return normalizedPath;
	}

	if (FPaths::GetPath(normalizedPath).IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), EpisodeSetupInputDirectory, normalizedPath));
	}

	return ResolveProjectRelativePath(normalizedPath);
}

FString UEpisodeAuthoringSubsystem::CompileSeverityToString(EEpisodeCompileDiagnosticSeverity severity)
{
	switch (severity)
	{
	case EEpisodeCompileDiagnosticSeverity::Info:
		return TEXT("Info");
	case EEpisodeCompileDiagnosticSeverity::Warning:
		return TEXT("Warning");
	case EEpisodeCompileDiagnosticSeverity::Error:
		return TEXT("Error");
	default:
		return TEXT("Unknown");
	}
}

void UEpisodeAuthoringSubsystem::AppendCompileDiagnostics(
	const FEpisodeCompileResult& compileResult,
	TArray<FString>& outDiagnostics)
{
	for (const FEpisodeCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("[%s] %s: %s"),
			*CompileSeverityToString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

FString UEpisodeAuthoringSubsystem::GroundRegionTypeToString(EEpisodeGroundRegionType regionType)
{
	switch (regionType)
	{
	case EEpisodeGroundRegionType::Walkable:
		return TEXT("walkable");
	case EEpisodeGroundRegionType::Penalty:
		return TEXT("penalty");
	case EEpisodeGroundRegionType::Blocked:
		return TEXT("blocked");
	default:
		return TEXT("walkable");
	}
}

FString UEpisodeAuthoringSubsystem::GroundShapeTypeToString(EEpisodeGroundShapeType shapeType)
{
	switch (shapeType)
	{
	case EEpisodeGroundShapeType::Rectangle:
		return TEXT("rectangle");
	case EEpisodeGroundShapeType::ConvexPolygon:
		return TEXT("convex_polygon");
	default:
		return TEXT("rectangle");
	}
}

TArray<TSharedPtr<FJsonValue>> UEpisodeAuthoringSubsystem::MakeXyArrayMeters(const FVector& locationCm)
{
	TArray<TSharedPtr<FJsonValue>> xyValues;
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.X * CentimetersToMeters));
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.Y * CentimetersToMeters));
	return xyValues;
}

TArray<TSharedPtr<FJsonValue>> UEpisodeAuthoringSubsystem::MakeSizeArrayMeters(const FVector2D& sizeCm)
{
	TArray<TSharedPtr<FJsonValue>> sizeValues;
	sizeValues.Add(MakeShared<FJsonValueNumber>(sizeCm.X * CentimetersToMeters));
	sizeValues.Add(MakeShared<FJsonValueNumber>(sizeCm.Y * CentimetersToMeters));
	return sizeValues;
}

TSharedPtr<FJsonObject> UEpisodeAuthoringSubsystem::MakePropertiesObject(const TMap<FString, FEpisodeParamValue>& properties)
{
	return MakeFilteredPropertiesObject(properties, TSet<FString>());
}

TSharedPtr<FJsonObject> UEpisodeAuthoringSubsystem::MakeFilteredPropertiesObject(
	const TMap<FString, FEpisodeParamValue>& properties,
	const TSet<FString>& excludedKeys)
{
	TSharedRef<FJsonObject> propertiesObject = MakeShared<FJsonObject>();
	int32 serializedCount = 0;
	for (const TPair<FString, FEpisodeParamValue>& pair : properties)
	{
		if (excludedKeys.Contains(pair.Key))
		{
			continue;
		}

		propertiesObject->SetField(pair.Key, MakeParamJsonValue(pair.Value));
		++serializedCount;
	}

	if (serializedCount <= 0)
	{
		return nullptr;
	}

	return propertiesObject;
}

TSharedPtr<FJsonValue> UEpisodeAuthoringSubsystem::MakeParamJsonValue(const FEpisodeParamValue& paramValue)
{
	switch (paramValue.Type)
	{
	case EEpisodeParamValueType::Bool:
		return MakeShared<FJsonValueBoolean>(paramValue.BoolValue);
	case EEpisodeParamValueType::Integer:
		return MakeShared<FJsonValueNumber>(paramValue.IntegerValue);
	case EEpisodeParamValueType::Float:
		return MakeShared<FJsonValueNumber>(paramValue.FloatValue);
	case EEpisodeParamValueType::String:
		return MakeShared<FJsonValueString>(paramValue.StringValue);
	case EEpisodeParamValueType::Vector:
	{
		TArray<TSharedPtr<FJsonValue>> vectorValues;
		vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.X));
		vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.Y));
		vectorValues.Add(MakeShared<FJsonValueNumber>(paramValue.VectorValue.Z));
		return MakeShared<FJsonValueArray>(vectorValues);
	}
	default:
		return MakeShared<FJsonValueNull>();
	}
}

bool UEpisodeAuthoringSubsystem::TryGetFloatProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	double& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue) return false;

	if (paramValue->Type == EEpisodeParamValueType::Float)
	{
		outValue = paramValue->FloatValue;
		return true;
	}

	if (paramValue->Type == EEpisodeParamValueType::Integer)
	{
		outValue = static_cast<double>(paramValue->IntegerValue);
		return true;
	}

	return false;
}

bool UEpisodeAuthoringSubsystem::TryGetBoolProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	bool& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::Bool) return false;

	outValue = paramValue->BoolValue;
	return true;
}

bool UEpisodeAuthoringSubsystem::TryGetStringProperty(
	const TMap<FString, FEpisodeParamValue>& properties,
	const FString& key,
	FString& outValue)
{
	const FEpisodeParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EEpisodeParamValueType::String) return false;

	outValue = paramValue->StringValue;
	return true;
}

bool UEpisodeAuthoringSubsystem::TryFindStaticObstacleProp(
	FName propId,
	FEpisodeStaticObstaclePropEntry& outPropEntry) const
{
	return AEpisodeStaticObstacle::FindDefaultPropEntryById(propId, outPropEntry);
}

double UEpisodeAuthoringSubsystem::ComputePlacementRadius2D(const FEpisodeStaticObstaclePropEntry& propEntry) const
{
	if (propEntry.SafetyRadius > 0.0)
	{
		return propEntry.SafetyRadius;
	}

	return FMath::Sqrt(FMath::Square(propEntry.FallbackBoxExtent.X) + FMath::Square(propEntry.FallbackBoxExtent.Y));
}

FVector2D UEpisodeAuthoringSubsystem::ComputePlacementHalfExtent2D(
	const FEpisodeStaticObstaclePropEntry& propEntry) const
{
	const FVector2D halfExtent(
		FMath::Max(propEntry.FallbackBoxExtent.X, 0.0),
		FMath::Max(propEntry.FallbackBoxExtent.Y, 0.0));

	if (halfExtent.X > KINDA_SMALL_NUMBER || halfExtent.Y > KINDA_SMALL_NUMBER)
	{
		return halfExtent;
	}

	const double fallbackRadius = ComputePlacementRadius2D(propEntry);
	return FVector2D(fallbackRadius, fallbackRadius);
}

bool UEpisodeAuthoringSubsystem::StaticObstacleFootprintsOverlap(
	const FVector& candidateLocation,
	const FVector2D& candidateHalfExtent,
	const FEpisodeAuthoringStaticObstacleRecord& record) const
{
	const FVector recordLocation = record.Transform.GetLocation();
	FVector2D recordHalfExtent = record.PlacementHalfExtent2D;
	if (recordHalfExtent.X <= KINDA_SMALL_NUMBER && recordHalfExtent.Y <= KINDA_SMALL_NUMBER)
	{
		recordHalfExtent = FVector2D(record.PlacementRadius2D, record.PlacementRadius2D);
	}

	const double allowedDeltaX =
		candidateHalfExtent.X + recordHalfExtent.X + StaticObstacleFootprintClearanceCm;
	const double allowedDeltaY =
		candidateHalfExtent.Y + recordHalfExtent.Y + StaticObstacleFootprintClearanceCm;
	const double deltaX = FMath::Abs(candidateLocation.X - recordLocation.X);
	const double deltaY = FMath::Abs(candidateLocation.Y - recordLocation.Y);

	return deltaX < allowedDeltaX && deltaY < allowedDeltaY;
}

FString UEpisodeAuthoringSubsystem::GenerateStaticObstacleInstanceId()
{
	FString instanceId;
	do
	{
		instanceId = FString::Printf(TEXT("obstacle_%03d"), NextStaticObstacleIndex++);
	}
	while (ContainsInstanceId(instanceId));

	return instanceId;
}

bool UEpisodeAuthoringSubsystem::ContainsInstanceId(const FString& instanceId) const
{
	for (const FEpisodePlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.InstanceId == instanceId)
		{
			return true;
		}
	}

	for (const FEpisodeDynamicActorSpec& spec : DraftWorldSpec.DynamicActors)
	{
		if (spec.InstanceId == instanceId)
		{
			return true;
		}
	}

	return false;
}

void UEpisodeAuthoringSubsystem::InitializeDraftDefaults()
{
	DraftWorldSpec = FEpisodeWorldSpec();
	DraftWorldSpec.RunConfig.TemplateId = ScenarioId;
	DraftWorldSpec.RunConfig.TemplateVersion = 1;
	DraftWorldSpec.RunConfig.BaseSeed = BaseSeed;
	DraftWorldSpec.RunConfig.IterationIndex = IterationIndex;

	FEpisodeParamValue timeLimitParam;
	timeLimitParam.Type = EEpisodeParamValueType::Float;
	timeLimitParam.FloatValue = TimeLimitSeconds;
	DraftWorldSpec.RunConfig.Parameters.Add(TEXT("time_limit_s"), timeLimitParam);

	DraftWorldSpec.Seeds.WorldSeed = BaseSeed;
	DraftWorldSpec.Seeds.LayoutSeed = BaseSeed + 101;
	DraftWorldSpec.Seeds.StaticObstacleSeed = BaseSeed + 202;
	DraftWorldSpec.Seeds.DynamicActorSeed = BaseSeed + 303;
	DraftWorldSpec.Seeds.EventSeed = BaseSeed + 404;
	DraftWorldSpec.Seeds.PolicySeed = BaseSeed + 505;
}

void UEpisodeAuthoringSubsystem::ClearEditorView()
{
	for (const TObjectPtr<AActor>& markerActor : RouteMarkerActors)
	{
		if (IsValid(markerActor))
		{
			markerActor->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AEpisodeStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	RouteMarkerActors.Reset();
	StaticObstacleRecords.Reset();
	StaticObstacleActors.Reset();
	NextStaticObstacleIndex = 1;
}

bool UEpisodeAuthoringSubsystem::RebuildEditorViewFromDraft(TArray<FString>& outDiagnostics)
{
	ClearEditorView();

	bool bSucceeded = true;
	for (const FEpisodePlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EEpisodeActorCategory::DeliveryBot || spec.Category == EEpisodeActorCategory::RoadVehicle)
		{
			SpawnRobotRouteMarkers(spec, outDiagnostics);
			continue;
		}

		if (spec.Category != EEpisodeActorCategory::StaticObstacle)
		{
			continue;
		}

		AEpisodeStaticObstacle* spawnedActor = nullptr;
		FString failureReason;
		if (!SpawnEditorStaticObstacleActor(spec, spawnedActor, failureReason))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Failed to create editor view for static obstacle '%s': %s"),
				*spec.InstanceId,
				*failureReason));
			bSucceeded = false;
			continue;
		}

		FEpisodeStaticObstaclePropEntry propEntry;
		if (TryFindStaticObstacleProp(FName(*spec.AssetId), propEntry))
		{
			AddStaticObstacleViewRecord(spec, propEntry, spawnedActor);
		}
	}

	return bSucceeded;
}

void UEpisodeAuthoringSubsystem::SpawnRobotRouteMarkers(
	const FEpisodePlaceableInstanceSpec& spec,
	TArray<FString>& outDiagnostics)
{
	if (!StartPointClass)
	{
		outDiagnostics.Add(TEXT("StartPointClass is not set; robot start marker was not spawned."));
	}
	else if (AActor* startMarker = SpawnEditorMarkerActor(StartPointClass, FTransform(spec.Transform)))
	{
		RouteMarkerActors.Add(startMarker);
	}
	else
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Failed to spawn robot start marker for '%s'."),
			*spec.InstanceId));
	}

	if (!spec.DeliveryBot.bHasGoalLocation) return;


	if (!GoalPointClass)
	{
		outDiagnostics.Add(TEXT("GoalPointClass is not set; robot goal marker was not spawned."));
		return;
	}

	const FTransform goalTransform(FRotator::ZeroRotator, spec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm);
	if (AActor* goalMarker = SpawnEditorMarkerActor(GoalPointClass, goalTransform))
	{
		RouteMarkerActors.Add(goalMarker);
	}
	else
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Failed to spawn robot goal marker for '%s'."),
			*spec.InstanceId));
	}
}

AActor* UEpisodeAuthoringSubsystem::SpawnEditorMarkerActor(
	TSubclassOf<AActor> markerClass,
	const FTransform& transform)
{
	UWorld* world = GetWorld();
	if (!world || !markerClass) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return world->SpawnActor<AActor>(markerClass, transform, spawnParams);
}

bool UEpisodeAuthoringSubsystem::SpawnEditorStaticObstacleActor(
	const FEpisodePlaceableInstanceSpec& spec,
	AEpisodeStaticObstacle*& outActor,
	FString& outFailureReason)
{
	outActor = nullptr;
	outFailureReason.Reset();

	UWorld* world = GetWorld();
	if (!world)
	{
		outFailureReason = TEXT("World is unavailable.");
		return false;
	}

	if (spec.InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("InstanceId is empty.");
		return false;
	}

	if (spec.AssetId.IsEmpty())
	{
		outFailureReason = TEXT("AssetId is empty.");
		return false;
	}

	TSubclassOf<AEpisodeStaticObstacle> spawnClass = StaticObstacleClass;
	if (!spawnClass)
	{
		spawnClass = AEpisodeStaticObstacle::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeStaticObstacle* staticObstacle = world->SpawnActor<AEpisodeStaticObstacle>(
		spawnClass,
		spec.Transform,
		spawnParams);
	if (!staticObstacle)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return false;
	}

	if (!staticObstacle->ApplyDefaultPropById(FName(*spec.AssetId)))
	{
		outFailureReason = FString::Printf(TEXT("Unknown prop '%s'."), *spec.AssetId);
		staticObstacle->Destroy();
		return false;
	}

	ConfigureAuthoredStaticObstacleActor(staticObstacle, spec);
	outActor = staticObstacle;
	return true;
}

void UEpisodeAuthoringSubsystem::AddStaticObstacleViewRecord(
	const FEpisodePlaceableInstanceSpec& spec,
	const FEpisodeStaticObstaclePropEntry& propEntry,
	AEpisodeStaticObstacle* actor)
{
	FEpisodeAuthoringStaticObstacleRecord record;
	record.InstanceId = spec.InstanceId;
	record.PropId = FName(*spec.AssetId);
	record.Transform = spec.Transform;
	record.PlacementRadius2D = ComputePlacementRadius2D(propEntry);
	record.PlacementHalfExtent2D = ComputePlacementHalfExtent2D(propEntry);

	StaticObstacleRecords.Add(record);
	StaticObstacleActors.Add(spec.InstanceId, actor);
}

FEpisodePlaceableInstanceSpec UEpisodeAuthoringSubsystem::MakeStaticObstacleSpec(
	const FString& instanceId,
	FName propId,
	const FTransform& transform) const
{
	FEpisodePlaceableInstanceSpec spec;
	spec.InstanceId = instanceId;
	spec.AssetId = propId.ToString();
	spec.Category = EEpisodeActorCategory::StaticObstacle;
	spec.Transform = transform;
	return spec;
}

void UEpisodeAuthoringSubsystem::ConfigureAuthoredStaticObstacleActor(
	AEpisodeStaticObstacle* actor,
	const FEpisodePlaceableInstanceSpec& spec) const
{
	if (!actor) return;

	if (UEpisodePlaceableComponent* placeableComponent = actor->FindComponentByClass<UEpisodePlaceableComponent>())
	{
		placeableComponent->InstanceId = spec.InstanceId;
		placeableComponent->AssetId = spec.AssetId;
		placeableComponent->Category = spec.Category;
		placeableComponent->MobilityMode = EEpisodeMobilityMode::Static;
	}
}
