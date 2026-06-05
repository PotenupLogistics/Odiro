#include "Episode/Editor/EpisodeAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/EpisodeCompiler.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UEpisodeAuthoringSubsystem::Deinitialize()
{
	ClearDraft();
	Super::Deinitialize();
}

void UEpisodeAuthoringSubsystem::ClearDraft()
{
	for (const TPair<FString, TObjectPtr<AEpisodeStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	StaticObstacleSpecs.Reset();
	StaticObstacleRecords.Reset();
	StaticObstacleActors.Reset();
	NextStaticObstacleIndex = 1;
}

void UEpisodeAuthoringSubsystem::GetStaticObstaclePaletteEntries(TArray<FEpisodeStaticObstaclePropEntry>& outEntries) const
{
	outEntries = AEpisodeStaticObstacle::GetDefaultPropEntries();
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

	const double candidateRadius = ComputePlacementRadius2D(candidateProp);
	const FVector candidateLocation = transform.GetLocation();

	for (const FEpisodeAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		const double minDistance = candidateRadius + record.PlacementRadius2D;
		const double distance2D = FVector::Dist2D(candidateLocation, record.Transform.GetLocation());
		if (distance2D < minDistance)
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

	UWorld* world = GetWorld();
	if (!world) return false;

	TSubclassOf<AEpisodeStaticObstacle> spawnClass = StaticObstacleClass;
	if (!spawnClass)
	{
		spawnClass = AEpisodeStaticObstacle::StaticClass();
	}

	const FString instanceId = GenerateStaticObstacleInstanceId();
	outSpec = MakeStaticObstacleSpec(instanceId, propId, transform);

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEpisodeStaticObstacle* staticObstacle = world->SpawnActor<AEpisodeStaticObstacle>(
		spawnClass,
		transform,
		spawnParams);
	if (!staticObstacle)
	{
		return false;
	}

	if (!staticObstacle->ApplyDefaultPropById(propId))
	{
		staticObstacle->Destroy();
		return false;
	}

	ConfigureAuthoredStaticObstacleActor(staticObstacle, outSpec);

	FEpisodeStaticObstaclePropEntry propEntry;
	TryFindStaticObstacleProp(propId, propEntry);

	FEpisodeAuthoringStaticObstacleRecord record;
	record.InstanceId = instanceId;
	record.PropId = propId;
	record.Transform = transform;
	record.PlacementRadius2D = ComputePlacementRadius2D(propEntry);

	StaticObstacleSpecs.Add(outSpec);
	StaticObstacleRecords.Add(record);
	StaticObstacleActors.Add(instanceId, staticObstacle);
	outActor = staticObstacle;
	return true;
}

bool UEpisodeAuthoringSubsystem::ExportEpisodeSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema"), TEXT("episode_actor_spawn_mvp"));
	rootObject->SetNumberField(TEXT("version"), 1);
	rootObject->SetStringField(TEXT("scenario_id"), ScenarioId);
	rootObject->SetStringField(TEXT("map_id"), MapId);

	TSharedRef<FJsonObject> runObject = MakeShared<FJsonObject>();
	runObject->SetNumberField(TEXT("base_seed"), static_cast<double>(BaseSeed));
	runObject->SetNumberField(TEXT("iteration_index"), IterationIndex);
	runObject->SetNumberField(TEXT("time_limit_s"), TimeLimitSeconds);
	rootObject->SetObjectField(TEXT("run"), runObject);

	TSharedRef<FJsonObject> groundModelObject = MakeShared<FJsonObject>();
	groundModelObject->SetStringField(TEXT("default_region_type"), TEXT("walkable"));
	groundModelObject->SetArrayField(TEXT("regions"), TArray<TSharedPtr<FJsonValue>>());
	rootObject->SetObjectField(TEXT("ground_model"), groundModelObject);

	rootObject->SetArrayField(TEXT("paths"), TArray<TSharedPtr<FJsonValue>>());

	TSharedRef<FJsonObject> actorsObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> staticObstacleValues;
	staticObstacleValues.Reserve(StaticObstacleSpecs.Num());
	for (const FEpisodePlaceableInstanceSpec& spec : StaticObstacleSpecs)
	{
		TSharedRef<FJsonObject> obstacleObject = MakeShared<FJsonObject>();
		obstacleObject->SetStringField(TEXT("instance_id"), spec.InstanceId);
		obstacleObject->SetStringField(TEXT("prop_id"), spec.AssetId);
		obstacleObject->SetArrayField(TEXT("xy_m"), MakeXyArrayMeters(spec.Transform.GetLocation()));
		obstacleObject->SetNumberField(TEXT("yaw_deg"), spec.Transform.Rotator().Yaw);

		if (!spec.Properties.IsEmpty())
		{
			obstacleObject->SetObjectField(TEXT("properties"), MakePropertiesObject(spec.Properties));
		}

		staticObstacleValues.Add(MakeShared<FJsonValueObject>(obstacleObject));
	}

	actorsObject->SetArrayField(TEXT("static_obstacles"), staticObstacleValues);
	actorsObject->SetArrayField(TEXT("pedestrians"), TArray<TSharedPtr<FJsonValue>>());
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
	for (const FEpisodeCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("[%s] %s: %s"),
			*CompileSeverityToString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}

	if (!compileResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("Exported EpisodeSetup JSON failed compiler validation."));
	}

	return compileResult.bSuccess;
}

bool UEpisodeAuthoringSubsystem::SaveEpisodeSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics) const
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

TArray<TSharedPtr<FJsonValue>> UEpisodeAuthoringSubsystem::MakeXyArrayMeters(const FVector& locationCm)
{
	TArray<TSharedPtr<FJsonValue>> xyValues;
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.X * CentimetersToMeters));
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.Y * CentimetersToMeters));
	return xyValues;
}

TSharedPtr<FJsonObject> UEpisodeAuthoringSubsystem::MakePropertiesObject(const TMap<FString, FEpisodeParamValue>& properties)
{
	TSharedRef<FJsonObject> propertiesObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FEpisodeParamValue>& pair : properties)
	{
		propertiesObject->SetField(pair.Key, MakeParamJsonValue(pair.Value));
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

FString UEpisodeAuthoringSubsystem::GenerateStaticObstacleInstanceId()
{
	FString instanceId;
	do
	{
		instanceId = FString::Printf(TEXT("obstacle_%03d"), NextStaticObstacleIndex++);
	}
	while (StaticObstacleActors.Contains(instanceId));

	return instanceId;
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
