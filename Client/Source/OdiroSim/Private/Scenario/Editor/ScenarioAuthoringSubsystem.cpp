#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Components/ScenarioPathFollowerComponent.h"
#include "Scenario/Components/ScenarioPedestrianRuntimeComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/ScenarioCompiler.h"
#include "Shared/ScenarioTemplateJson.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioAuthoring, Log, All);

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
	const FString DefaultRobotInstanceId(TEXT("robot_01"));
	const FString DefaultRobotAssetId(TEXT("delivery_bot"));
	const FString RobotStartMarkerInstanceId(TEXT("robot_start_point"));
	const FString RobotGoalMarkerInstanceId(TEXT("robot_goal_point"));
	const FString RobotStartMarkerAssetId(TEXT("start_point"));
	const FString RobotGoalMarkerAssetId(TEXT("goal_point"));
	const FVector DefaultRobotStartLocationCm(-600.0, 0.0, 0.0);
	const FVector DefaultRobotGoalLocationCm(600.0, 0.0, 0.0);

	FScenarioParamValue MakeStringParamValue(const FString& value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::String;
		paramValue.StringValue = value;
		return paramValue;
	}

	FScenarioParamValue MakeBoolParamValue(bool value)
	{
		FScenarioParamValue paramValue;
		paramValue.Type = EScenarioParamValueType::Bool;
		paramValue.BoolValue = value;
		return paramValue;
	}
}

UScenarioAuthoringSubsystem::UScenarioAuthoringSubsystem()
{
	StaticObstacleClass = AScenarioStaticObstacle::StaticClass();
	PedestrianClass = AScenarioPedestrian::StaticClass();
	StaticObstaclePropCatalog = UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference();

	static ConstructorHelpers::FClassFinder<AScenarioPedestrian> pedestrianBlueprintClass(
		TEXT("/Game/Blueprints/Scenario/BP_ScenarioPedestrian"));
	if (pedestrianBlueprintClass.Succeeded())
	{
		PedestrianClass = pedestrianBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> startPointBlueprintClass(TEXT("/Game/Blueprints/Scenario/BP_StartPoint"));
	if (startPointBlueprintClass.Succeeded())
	{
		StartPointClass = startPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> goalPointBlueprintClass(TEXT("/Game/Blueprints/Scenario/BP_GoalPoint"));
	if (goalPointBlueprintClass.Succeeded())
	{
		GoalPointClass = goalPointBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> pedestrianVisualizationBlueprintClass(
		TEXT("/Game/Models/Placeable/StaticMeshes/BP_PlaceablePedestrian"));
	if (pedestrianVisualizationBlueprintClass.Succeeded())
	{
		PedestrianVisualizationActorClass = pedestrianVisualizationBlueprintClass.Class;
	}
	else
	{
		PedestrianVisualizationActorClass = PedestrianClass.Get();
		UE_LOG(
			LogScenarioAuthoring,
			Warning,
			TEXT("Pedestrian visualization actor class was not found. Falling back to pedestrian class: %s"),
			PedestrianVisualizationActorClass
				? *PedestrianVisualizationActorClass->GetPathName()
				: TEXT("<null>"));
	}
}

void UScenarioAuthoringSubsystem::Deinitialize()
{
	ClearDraft();
	Super::Deinitialize();
}

void UScenarioAuthoringSubsystem::ClearDraft()
{
	ClearEditorView();
	DraftScenarioTemplate = FScenarioTemplateDocument();
	DraftGroundRegions.Reset();
	DraftPedestrianSpecs.Reset();
	SourceScenarioTemplateJsonPath.Reset();
	bDirty = false;
	NextStaticObstacleIndex = 1;
	NextPedestrianIndex = 1;
	NextGroundRegionIndex = 1;
}

void UScenarioAuthoringSubsystem::NewDraft()
{
	ClearDraft();
	InitializeDraftDefaults();
	TArray<FString> diagnostics;
	if (!RebuildEditorViewFromDraft(diagnostics))
	{
		for (const FString& diagnostic : diagnostics)
		{
			UE_LOG(LogScenarioAuthoring, Warning, TEXT("New draft editor view rebuild failed | %s"), *diagnostic);
		}
	}
}

bool UScenarioAuthoringSubsystem::LoadScenarioSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	const FString trimmedJsonFilePath = jsonFilePath.TrimStartAndEnd();
	if (trimmedJsonFilePath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioSetup JSON file path is empty."));
		return false;
	}

	outResolvedJsonFilePath = ResolveScenarioSetupLoadPath(trimmedJsonFilePath);

	const FScenarioTemplateParseResult parseResult = FScenarioTemplateJson::ParseFromFile(outResolvedJsonFilePath);
	AppendSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("ScenarioTemplate JSON import failed validation."));
		return false;
	}

	DraftScenarioTemplate = parseResult.Document;
	DraftGroundRegions.Reset();
	DraftPedestrianSpecs.Reset();
	SourceScenarioTemplateJsonPath = outResolvedJsonFilePath;
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UScenarioAuthoringSubsystem::LoadScenarioSetupJsonString(
	const FString& jsonString,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (jsonString.IsEmpty())
	{
		outDiagnostics.Add(TEXT("ScenarioSetup JSON string is empty."));
		return false;
	}

	const FScenarioTemplateParseResult parseResult = FScenarioTemplateJson::ParseFromString(jsonString);
	AppendSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("ScenarioTemplate JSON import failed validation."));
		return false;
	}

	DraftScenarioTemplate = parseResult.Document;
	DraftGroundRegions.Reset();
	DraftPedestrianSpecs.Reset();
	SourceScenarioTemplateJsonPath.Reset();
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

bool UScenarioAuthoringSubsystem::ImportCompiledWorldSpec(
	const FScenarioWorldSpec& worldSpec,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	ImportWorldSpecAsScenarioTemplate(worldSpec);
	SourceScenarioTemplateJsonPath.Reset();
	bDirty = false;
	return RebuildEditorViewFromDraft(outDiagnostics);
}

void UScenarioAuthoringSubsystem::GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();

	const UScenarioStaticObstaclePropCatalog* propCatalog = GetStaticObstaclePropCatalog();
	if (!propCatalog) return;

	outEntries = propCatalog->GetEntries();
}

bool UScenarioAuthoringSubsystem::TryGetStaticObstaclePropEntry(
	FName propId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	return TryFindStaticObstacleProp(propId, outPropEntry);
}

void UScenarioAuthoringSubsystem::GetAuthoredStaticObstacleActors(TArray<AScenarioStaticObstacle*>& outActors) const
{
	outActors.Reset();
	outActors.Reserve(StaticObstacleActors.Num());

	for (const TPair<FString, TObjectPtr<AScenarioStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (AScenarioStaticObstacle* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}
}

void UScenarioAuthoringSubsystem::GetEditorPlacementIgnoredActors(TArray<AActor*>& outActors) const
{
	outActors.Reset();
	outActors.Reserve(StaticObstacleActors.Num() + PedestrianActors.Num() + RouteMarkerActors.Num());

	for (const TPair<FString, TObjectPtr<AScenarioStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (AActor* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}

	for (const TPair<FString, TObjectPtr<AActor>>& pair : PedestrianActors)
	{
		if (AActor* actor = pair.Value.Get())
		{
			outActors.Add(actor);
		}
	}

	for (const TObjectPtr<AActor>& markerActor : RouteMarkerActors)
	{
		if (AActor* actor = markerActor.Get())
		{
			outActors.Add(actor);
		}
	}
}

bool UScenarioAuthoringSubsystem::CanPlaceStaticObstacle(
	FName propId,
	const FTransform& transform,
	FString& outFailureReason) const
{
	return CanPlaceStaticObstacleInternal(propId, transform, FString(), outFailureReason);
}

bool UScenarioAuthoringSubsystem::CanPlaceEditorGroundActor(
	const FTransform& transform,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	const double locationZ = transform.GetLocation().Z;
	if (locationZ < -KINDA_SMALL_NUMBER)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be 0.00 cm or higher. Current Z: %.2f."),
			locationZ);
		return false;
	}
	if (locationZ > StaticObstacleGroundZToleranceCm)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be %.2f cm or lower. Current Z: %.2f."),
			StaticObstacleGroundZToleranceCm,
			locationZ);
		return false;
	}

	return true;
}

bool UScenarioAuthoringSubsystem::CanUpdateStaticObstacleTransform(
	const FString& instanceId,
	const FTransform& transform,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	if (instanceId.IsEmpty())
	{
		outFailureReason = TEXT("Static obstacle instance id is empty.");
		return false;
	}

	const FScenarioTemplateObstaclePlacement* placement = FindStaticObstaclePlacementByInstanceId(instanceId);
	if (!placement)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle spec '%s' was not found."), *instanceId);
		return false;
	}

	if (!FindStaticObstacleRecordByInstanceId(instanceId))
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle record '%s' was not found."), *instanceId);
		return false;
	}

	const TObjectPtr<AScenarioStaticObstacle>* actorPtr = StaticObstacleActors.Find(instanceId);
	if (!actorPtr || !actorPtr->Get())
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle actor '%s' was not found."), *instanceId);
		return false;
	}

	return CanPlaceStaticObstacleInternal(FName(*placement->PropId), transform, instanceId, outFailureReason);
}

bool UScenarioAuthoringSubsystem::CanPlaceStaticObstacleInternal(
	FName propId,
	const FTransform& transform,
	const FString& ignoredInstanceId,
	FString& outFailureReason) const
{
	outFailureReason.Reset();

	FScenarioStaticObstaclePropEntry candidateProp;
	if (!TryFindStaticObstacleProp(propId, candidateProp))
	{
		outFailureReason = FString::Printf(TEXT("Unknown static obstacle prop '%s'."), *propId.ToString());
		return false;
	}

	const FVector2D candidateHalfExtent = ComputePlacementHalfExtent2D(candidateProp);
	const FVector candidateLocation = transform.GetLocation();
	if (candidateLocation.Z < -KINDA_SMALL_NUMBER)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be 0.00 cm or higher. Current Z: %.2f."),
			candidateLocation.Z);
		return false;
	}
	if (candidateLocation.Z > StaticObstacleGroundZToleranceCm)
	{
		outFailureReason = FString::Printf(
			TEXT("Placement location Z must be %.2f cm or lower. Current Z: %.2f."),
			StaticObstacleGroundZToleranceCm,
			candidateLocation.Z);
		return false;
	}

	for (const FScenarioAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (!ignoredInstanceId.IsEmpty() && record.InstanceId == ignoredInstanceId)
		{
			continue;
		}

		if (StaticObstacleFootprintsOverlap(candidateLocation, candidateHalfExtent, record))
		{
			outFailureReason = FString::Printf(
				TEXT("Overlaps static obstacle '%s'."), *record.InstanceId);
			return false;
		}
	}

	return true;
}

bool UScenarioAuthoringSubsystem::AddStaticObstacle(
	FName propId,
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec)
{
	AScenarioStaticObstacle* spawnedActor = nullptr;
	return AddStaticObstacleInternal(propId, transform, outSpec, spawnedActor);
}

bool UScenarioAuthoringSubsystem::AddPedestrian(
	FName archetypeId,
	const FTransform& transform,
	FScenarioDynamicActorSpec& outSpec,
	AActor*& outActor,
	FString& outFailureReason)
{
	outSpec = FScenarioDynamicActorSpec();
	outActor = nullptr;
	outFailureReason.Reset();

	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString instanceId = GeneratePedestrianInstanceId();
	outSpec = MakePedestrianSpec(instanceId, archetypeId, transform);

	if (!SpawnEditorPedestrianActor(outSpec, outActor, outFailureReason))
	{
		return false;
	}

	DraftPedestrianSpecs.Add(outSpec);
	AddPedestrianViewRecord(outSpec, outActor);
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Added pedestrian | InstanceId: %s | AssetId: %s | Location: %s"),
		*outSpec.InstanceId,
		*outSpec.AssetId,
		*transform.GetLocation().ToCompactString());

	return true;
}

bool UScenarioAuthoringSubsystem::SetRobotStartLocation(
	FName assetId,
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec,
	AActor*& outMarker,
	FString& outFailureReason)
{
	outSpec = FScenarioPlaceableInstanceSpec();
	outMarker = nullptr;
	outFailureReason.Reset();

	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	outMarker = SpawnOrReplaceRouteMarker(
		RobotStartMarkerActor,
		StartPointClass,
		transform,
		EScenarioPlaceableAuthoringRole::RobotStartMarker,
		outFailureReason);
	if (!outMarker)
	{
		return false;
	}

	DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(transform.GetLocation());
	outSpec = MakeDeliveryBotSpecFromTemplateRobot();
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Set robot start | InstanceId: %s | Location: %s"),
		*outSpec.InstanceId,
		*transform.GetLocation().ToCompactString());

	return true;
}

bool UScenarioAuthoringSubsystem::SetRobotGoalLocation(
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec,
	AActor*& outMarker,
	FString& outFailureReason)
{
	outSpec = FScenarioPlaceableInstanceSpec();
	outMarker = nullptr;
	outFailureReason.Reset();

	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		outFailureReason = TEXT("Robot start point must be placed before a goal point.");
		return false;
	}

	outMarker = SpawnOrReplaceRouteMarker(
		RobotGoalMarkerActor,
		GoalPointClass,
		transform,
		EScenarioPlaceableAuthoringRole::RobotGoalMarker,
		outFailureReason);
	if (!outMarker)
	{
		return false;
	}

	DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(transform.GetLocation());
	outSpec = MakeDeliveryBotSpecFromTemplateRobot();
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Set robot goal | InstanceId: %s | Location: %s"),
		*outSpec.InstanceId,
		*transform.GetLocation().ToCompactString());

	return true;
}

bool UScenarioAuthoringSubsystem::UpdateStaticObstacleTransform(
	const FString& instanceId,
	const FTransform& transform,
	FString& outFailureReason)
{
	if (!CanUpdateStaticObstacleTransform(instanceId, transform, outFailureReason))
	{
		return false;
	}

	FScenarioTemplateObstaclePlacement* placement = FindStaticObstaclePlacementByInstanceId(instanceId);
	FScenarioAuthoringStaticObstacleRecord* record = FindStaticObstacleRecordByInstanceId(instanceId);
	if (!placement || !record)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle '%s' is not editable."), *instanceId);
		return false;
	}

	TObjectPtr<AScenarioStaticObstacle>* actorPtr = StaticObstacleActors.Find(instanceId);
	AScenarioStaticObstacle* actor = actorPtr ? actorPtr->Get() : nullptr;
	if (!actor)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle actor '%s' was not found."), *instanceId);
		return false;
	}

	const FName propId(*placement->PropId);
	*placement = MakeStaticObstaclePlacement(instanceId, propId, transform);
	record->Transform = transform;
	actor->SetActorTransform(transform, false, nullptr, ETeleportType::TeleportPhysics);

	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Verbose,
		TEXT("Updated static obstacle transform | InstanceId: %s, Location: %s, Yaw: %.2f"),
		*instanceId,
		*transform.GetLocation().ToCompactString(),
		transform.Rotator().Yaw);

	return true;
}

bool UScenarioAuthoringSubsystem::UpdateRobotStartPointTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		outFailureReason = TEXT("Robot route points are not initialized.");
		return false;
	}
	if (!IsValid(RobotStartMarkerActor))
	{
		outFailureReason = TEXT("Robot start marker actor was not found.");
		return false;
	}

	const FVector location = transform.GetLocation();
	DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(location);
	RobotStartMarkerActor->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);

	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::UpdateRobotGoalPointTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!CanPlaceEditorGroundActor(transform, outFailureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		outFailureReason = TEXT("Robot route points are not initialized.");
		return false;
	}
	if (!IsValid(RobotGoalMarkerActor))
	{
		outFailureReason = TEXT("Robot goal marker actor was not found.");
		return false;
	}

	const FVector location = transform.GetLocation();
	DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(location);
	RobotGoalMarkerActor->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);

	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::RenameStaticObstacleInstanceId(
	const FString& oldInstanceId,
	const FString& newInstanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	const FString trimmedNewInstanceId = newInstanceId.TrimStartAndEnd();
	if (oldInstanceId.IsEmpty())
	{
		outFailureReason = TEXT("Static obstacle instance id is empty.");
		return false;
	}
	if (trimmedNewInstanceId.IsEmpty())
	{
		outFailureReason = TEXT("New static obstacle instance id is empty.");
		return false;
	}
	if (oldInstanceId == trimmedNewInstanceId)
	{
		return true;
	}
	if (ContainsInstanceId(trimmedNewInstanceId))
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle instance id '%s' already exists."), *trimmedNewInstanceId);
		return false;
	}

	FScenarioTemplateObstaclePlacement* placement = FindStaticObstaclePlacementByInstanceId(oldInstanceId);
	FScenarioAuthoringStaticObstacleRecord* record = FindStaticObstacleRecordByInstanceId(oldInstanceId);
	if (!placement || !record)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle '%s' is not editable."), *oldInstanceId);
		return false;
	}

	TObjectPtr<AScenarioStaticObstacle>* actorPtr = StaticObstacleActors.Find(oldInstanceId);
	if (!actorPtr || !actorPtr->Get())
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle actor '%s' was not found."), *oldInstanceId);
		return false;
	}
	AScenarioStaticObstacle* actor = actorPtr->Get();

	placement->PlacementId = trimmedNewInstanceId;
	record->InstanceId = trimmedNewInstanceId;
	StaticObstacleActors.Remove(oldInstanceId);
	TObjectPtr<AScenarioStaticObstacle> renamedActorPtr = actor;
	StaticObstacleActors.Add(trimmedNewInstanceId, renamedActorPtr);

	if (UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = trimmedNewInstanceId;
	}

	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Renamed static obstacle instance | OldInstanceId: %s | NewInstanceId: %s"),
		*oldInstanceId,
		*trimmedNewInstanceId);

	return true;
}

bool UScenarioAuthoringSubsystem::RemoveStaticObstacle(
	const FString& instanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	if (instanceId.IsEmpty())
	{
		outFailureReason = TEXT("Static obstacle instance id is empty.");
		return false;
	}

	const int32 removedSpecCount = DraftScenarioTemplate.Obstacles.Placements.RemoveAll(
		[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
		{
			return placement.PlacementId == instanceId;
		});
	if (removedSpecCount <= 0)
	{
		outFailureReason = FString::Printf(TEXT("Static obstacle spec '%s' was not found."), *instanceId);
		return false;
	}

	StaticObstacleRecords.RemoveAll(
		[&instanceId](const FScenarioAuthoringStaticObstacleRecord& record)
		{
			return record.InstanceId == instanceId;
		});

	TObjectPtr<AScenarioStaticObstacle> actorPtr;
	StaticObstacleActors.RemoveAndCopyValue(instanceId, actorPtr);
	if (AScenarioStaticObstacle* actor = actorPtr.Get())
	{
		actor->Destroy();
	}

	bDirty = true;

	UE_LOG(LogScenarioAuthoring, Log, TEXT("Removed static obstacle | InstanceId: %s"), *instanceId);
	return true;
}

bool UScenarioAuthoringSubsystem::AddGroundRegion(
	EScenarioGroundRegionType regionType,
	const FVector& centerCm,
	const FVector2D& sizeCm,
	double yawDegrees,
	FScenarioGroundRegionSpec& outSpec,
	FString& outFailureReason)
{
	outSpec = FScenarioGroundRegionSpec();
	outFailureReason.Reset();

	if (sizeCm.X <= KINDA_SMALL_NUMBER || sizeCm.Y <= KINDA_SMALL_NUMBER)
	{
		outFailureReason = TEXT("Ground region size must be positive.");
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString regionId = GenerateGroundRegionId();
	outSpec = MakeGroundRegionSpec(regionId, regionType, centerCm, sizeCm, yawDegrees);

	AScenarioGroundRegion* spawnedActor = nullptr;
	if (!SpawnEditorGroundRegionActor(outSpec, spawnedActor, outFailureReason))
	{
		outSpec = FScenarioGroundRegionSpec();
		return false;
	}

	DraftGroundRegions.Add(outSpec);
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Added ground region | RegionId: %s | Type: %s | Center: %s | Size: %s"),
		*outSpec.RegionId,
		*GroundRegionTypeToString(outSpec.RegionType),
		*centerCm.ToCompactString(),
		*sizeCm.ToString());

	return true;
}

bool UScenarioAuthoringSubsystem::UpdateGroundRegionTransform(
	const FString& regionId,
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	if (regionId.IsEmpty())
	{
		outFailureReason = TEXT("Ground region id is empty.");
		return false;
	}

	FScenarioGroundRegionSpec* regionSpec = DraftGroundRegions.FindByPredicate(
		[&regionId](const FScenarioGroundRegionSpec& spec)
		{
			return spec.RegionId == regionId;
		});
	if (!regionSpec)
	{
		outFailureReason = FString::Printf(TEXT("Ground region spec '%s' was not found."), *regionId);
		return false;
	}

	// 이동(Center)과 yaw 회전만 반영하고 Size는 보존함.
	regionSpec->Center = transform.GetLocation();
	regionSpec->YawDegrees = transform.GetRotation().Rotator().Yaw;

	if (const TObjectPtr<AScenarioGroundRegion>* actorPtr = GroundRegionActors.Find(regionId))
	{
		if (AScenarioGroundRegion* actor = actorPtr->Get())
		{
			actor->ConfigureRegion(*regionSpec);
		}
	}

	bDirty = true;
	return true;
}

bool UScenarioAuthoringSubsystem::RemoveGroundRegion(
	const FString& regionId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	if (regionId.IsEmpty())
	{
		outFailureReason = TEXT("Ground region id is empty.");
		return false;
	}

	const int32 removedSpecCount = DraftGroundRegions.RemoveAll(
		[&regionId](const FScenarioGroundRegionSpec& spec)
		{
			return spec.RegionId == regionId;
		});
	if (removedSpecCount <= 0)
	{
		outFailureReason = FString::Printf(TEXT("Ground region spec '%s' was not found."), *regionId);
		return false;
	}

	TObjectPtr<AScenarioGroundRegion> actorPtr;
	GroundRegionActors.RemoveAndCopyValue(regionId, actorPtr);
	if (AScenarioGroundRegion* actor = actorPtr.Get())
	{
		actor->Destroy();
	}

	bDirty = true;

	UE_LOG(LogScenarioAuthoring, Log, TEXT("Removed ground region | RegionId: %s"), *regionId);
	return true;
}

FScenarioGroundRegionSpec UScenarioAuthoringSubsystem::MakeGroundRegionSpec(
	const FString& regionId,
	EScenarioGroundRegionType regionType,
	const FVector& centerCm,
	const FVector2D& sizeCm,
	double yawDegrees) const
{
	FScenarioGroundRegionSpec spec;
	spec.RegionId = regionId;
	spec.RegionType = regionType;
	spec.ShapeType = EScenarioGroundShapeType::Rectangle;
	spec.Center = centerCm;
	spec.Size = sizeCm;
	spec.YawDegrees = yawDegrees;

	switch (regionType)
	{
	case EScenarioGroundRegionType::Walkable:
		spec.TraversabilityScore = 1.0;
		break;
	case EScenarioGroundRegionType::Penalty:
		spec.TraversabilityScore = 0.5;
		break;
	case EScenarioGroundRegionType::Blocked:
		spec.TraversabilityScore = 0.0;
		break;
	default:
		break;
	}

	return spec;
}

FString UScenarioAuthoringSubsystem::GenerateGroundRegionId()
{
	FString regionId;
	do
	{
		regionId = FString::Printf(TEXT("region_%03d"), NextGroundRegionIndex++);
	}
	while (ContainsGroundRegionId(regionId));

	return regionId;
}

bool UScenarioAuthoringSubsystem::ContainsGroundRegionId(const FString& regionId) const
{
	for (const FScenarioGroundRegionSpec& spec : DraftGroundRegions)
	{
		if (spec.RegionId == regionId)
		{
			return true;
		}
	}

	return false;
}

bool UScenarioAuthoringSubsystem::SpawnEditorGroundRegionActor(
	const FScenarioGroundRegionSpec& spec,
	AScenarioGroundRegion*& outActor,
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

	if (spec.RegionId.IsEmpty())
	{
		outFailureReason = TEXT("RegionId is empty.");
		return false;
	}

	TSubclassOf<AScenarioGroundRegion> spawnClass = GroundRegionClass;
	if (!spawnClass)
	{
		spawnClass = AScenarioGroundRegion::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioGroundRegion* regionActor = world->SpawnActor<AScenarioGroundRegion>(
		spawnClass,
		FTransform::Identity,
		spawnParams);
	if (!regionActor)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return false;
	}

	regionActor->ConfigureRegion(spec);
	GroundRegionActors.Add(spec.RegionId, regionActor);
	outActor = regionActor;
	return true;
}

bool UScenarioAuthoringSubsystem::AddStaticObstacleInternal(
	FName propId,
	const FTransform& transform,
	FScenarioPlaceableInstanceSpec& outSpec,
	AScenarioStaticObstacle*& outActor)
{
	outActor = nullptr;
	outSpec = FScenarioPlaceableInstanceSpec();

	FString failureReason;
	if (!CanPlaceStaticObstacle(propId, transform, failureReason))
	{
		return false;
	}

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString instanceId = GenerateStaticObstacleInstanceId();
	outSpec = MakeStaticObstacleSpec(instanceId, propId, transform);
	const FScenarioTemplateObstaclePlacement placement = MakeStaticObstaclePlacement(instanceId, propId, transform);

	if (!SpawnEditorStaticObstacleActor(outSpec, outActor, failureReason))
	{
		return false;
	}

	FScenarioStaticObstaclePropEntry propEntry;
	TryFindStaticObstacleProp(propId, propEntry);
	AddStaticObstacleViewRecord(outSpec, propEntry, outActor);
	DraftScenarioTemplate.Obstacles.Placements.Add(placement);
	bDirty = true;
	return true;
}

TArray<FScenarioPlaceableInstanceSpec> UScenarioAuthoringSubsystem::GetAuthoredStaticObstacleSpecs() const
{
	TArray<FScenarioPlaceableInstanceSpec> staticObstacleSpecs;
	for (const FScenarioTemplateObstaclePlacement& placement : DraftScenarioTemplate.Obstacles.Placements)
	{
		if (placement.Kind != EScenarioTemplateObstaclePlacementKind::Fixed || placement.PropId.IsEmpty())
		{
			continue;
		}
		staticObstacleSpecs.Add(MakeStaticObstacleSpecFromPlacement(placement));
	}

	return staticObstacleSpecs;
}

bool UScenarioAuthoringSubsystem::ExportScenarioSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();
	if (!ValidateSingleRobotRouteSpecForExport(outDiagnostics))
	{
		return false;
	}

	TArray<FScenarioSchemaDiagnostic> schemaDiagnostics;
	const bool bWritten = FScenarioTemplateJson::TryWriteJson(DraftScenarioTemplate, outJsonString, schemaDiagnostics);
	AppendSchemaDiagnostics(schemaDiagnostics, outDiagnostics);
	return bWritten;
}

bool UScenarioAuthoringSubsystem::ExportAndValidateScenarioSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	if (!ExportScenarioSetupJsonString(outJsonString, outDiagnostics))
	{
		return false;
	}

	FScenarioTemplateParseResult parseResult = FScenarioTemplateJson::ParseFromString(outJsonString);
	AppendSchemaDiagnostics(parseResult.Diagnostics, outDiagnostics);
	if (!parseResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("Exported ScenarioTemplate JSON failed validation."));
	}

	return parseResult.bSuccess;
}

bool UScenarioAuthoringSubsystem::SaveScenarioSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (jsonFilePath.IsEmpty())
	{
		outResolvedJsonFilePath.Reset();
		outDiagnostics.Add(TEXT("ScenarioSetup JSON file path is empty."));
		return false;
	}

	outResolvedJsonFilePath = ResolveProjectRelativePath(jsonFilePath);

	FString jsonString;
	if (!ExportAndValidateScenarioSetupJsonString(jsonString, outDiagnostics))
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
		outDiagnostics.Add(FString::Printf(TEXT("Failed to save ScenarioSetup JSON to '%s'."), *outResolvedJsonFilePath));
		return false;
	}

	SourceScenarioTemplateJsonPath = outResolvedJsonFilePath;
	bDirty = false;
	return true;
}

FString UScenarioAuthoringSubsystem::ResolveProjectRelativePath(const FString& filePath)
{
	if (FPaths::IsRelative(filePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), filePath);
	}

	return filePath;
}

FString UScenarioAuthoringSubsystem::ResolveScenarioSetupLoadPath(const FString& filePath) const
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
			FPaths::Combine(FPaths::ProjectDir(), ScenarioSetupInputDirectory, normalizedPath));
	}

	return ResolveProjectRelativePath(normalizedPath);
}

FString UScenarioAuthoringSubsystem::CompileSeverityToString(EScenarioCompileDiagnosticSeverity severity)
{
	switch (severity)
	{
	case EScenarioCompileDiagnosticSeverity::Info:
		return TEXT("Info");
	case EScenarioCompileDiagnosticSeverity::Warning:
		return TEXT("Warning");
	case EScenarioCompileDiagnosticSeverity::Error:
		return TEXT("Error");
	default:
		return TEXT("Unknown");
	}
}

void UScenarioAuthoringSubsystem::AppendCompileDiagnostics(
	const FScenarioCompileResult& compileResult,
	TArray<FString>& outDiagnostics)
{
	for (const FScenarioCompileDiagnostic& diagnostic : compileResult.Diagnostics)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("[%s] %s: %s"),
			*CompileSeverityToString(diagnostic.Severity),
			*diagnostic.Code,
			*diagnostic.Message));
	}
}

FString UScenarioAuthoringSubsystem::GroundRegionTypeToString(EScenarioGroundRegionType regionType)
{
	switch (regionType)
	{
	case EScenarioGroundRegionType::Walkable:
		return TEXT("walkable");
	case EScenarioGroundRegionType::Penalty:
		return TEXT("penalty");
	case EScenarioGroundRegionType::Blocked:
		return TEXT("blocked");
	default:
		return TEXT("walkable");
	}
}

FString UScenarioAuthoringSubsystem::GroundShapeTypeToString(EScenarioGroundShapeType shapeType)
{
	switch (shapeType)
	{
	case EScenarioGroundShapeType::Rectangle:
		return TEXT("rectangle");
	case EScenarioGroundShapeType::ConvexPolygon:
		return TEXT("convex_polygon");
	default:
		return TEXT("rectangle");
	}
}

TArray<TSharedPtr<FJsonValue>> UScenarioAuthoringSubsystem::MakeXyArrayMeters(const FVector& locationCm)
{
	TArray<TSharedPtr<FJsonValue>> xyValues;
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.X * CentimetersToMeters));
	xyValues.Add(MakeShared<FJsonValueNumber>(locationCm.Y * CentimetersToMeters));
	return xyValues;
}

TArray<TSharedPtr<FJsonValue>> UScenarioAuthoringSubsystem::MakeSizeArrayMeters(const FVector2D& sizeCm)
{
	TArray<TSharedPtr<FJsonValue>> sizeValues;
	sizeValues.Add(MakeShared<FJsonValueNumber>(sizeCm.X * CentimetersToMeters));
	sizeValues.Add(MakeShared<FJsonValueNumber>(sizeCm.Y * CentimetersToMeters));
	return sizeValues;
}

TSharedPtr<FJsonObject> UScenarioAuthoringSubsystem::MakePropertiesObject(const TMap<FString, FScenarioParamValue>& properties)
{
	return MakeFilteredPropertiesObject(properties, TSet<FString>());
}

TSharedPtr<FJsonObject> UScenarioAuthoringSubsystem::MakeFilteredPropertiesObject(
	const TMap<FString, FScenarioParamValue>& properties,
	const TSet<FString>& excludedKeys)
{
	TSharedRef<FJsonObject> propertiesObject = MakeShared<FJsonObject>();
	int32 serializedCount = 0;
	for (const TPair<FString, FScenarioParamValue>& pair : properties)
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

TSharedPtr<FJsonValue> UScenarioAuthoringSubsystem::MakeParamJsonValue(const FScenarioParamValue& paramValue)
{
	switch (paramValue.Type)
	{
	case EScenarioParamValueType::Bool:
		return MakeShared<FJsonValueBoolean>(paramValue.BoolValue);
	case EScenarioParamValueType::Integer:
		return MakeShared<FJsonValueNumber>(paramValue.IntegerValue);
	case EScenarioParamValueType::Float:
		return MakeShared<FJsonValueNumber>(paramValue.FloatValue);
	case EScenarioParamValueType::String:
		return MakeShared<FJsonValueString>(paramValue.StringValue);
	case EScenarioParamValueType::Vector:
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

bool UScenarioAuthoringSubsystem::TryGetFloatProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	double& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue) return false;

	if (paramValue->Type == EScenarioParamValueType::Float)
	{
		outValue = paramValue->FloatValue;
		return true;
	}

	if (paramValue->Type == EScenarioParamValueType::Integer)
	{
		outValue = static_cast<double>(paramValue->IntegerValue);
		return true;
	}

	return false;
}

bool UScenarioAuthoringSubsystem::TryGetBoolProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	bool& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::Bool) return false;

	outValue = paramValue->BoolValue;
	return true;
}

bool UScenarioAuthoringSubsystem::TryGetStringProperty(
	const TMap<FString, FScenarioParamValue>& properties,
	const FString& key,
	FString& outValue)
{
	const FScenarioParamValue* paramValue = properties.Find(key);
	if (!paramValue || paramValue->Type != EScenarioParamValueType::String) return false;

	outValue = paramValue->StringValue;
	return true;
}

void UScenarioAuthoringSubsystem::AppendSchemaDiagnostics(
	const TArray<FScenarioSchemaDiagnostic>& schemaDiagnostics,
	TArray<FString>& outDiagnostics)
{
	for (const FScenarioSchemaDiagnostic& diagnostic : schemaDiagnostics)
	{
		FString severity = TEXT("Info");
		switch (diagnostic.Severity)
		{
		case EScenarioSchemaDiagnosticSeverity::Warning:
			severity = TEXT("Warning");
			break;
		case EScenarioSchemaDiagnosticSeverity::Repair:
			severity = TEXT("Repair");
			break;
		case EScenarioSchemaDiagnosticSeverity::Error:
			severity = TEXT("Error");
			break;
		case EScenarioSchemaDiagnosticSeverity::Info:
		default:
			break;
		}

		const FString pathSuffix = diagnostic.Path.IsEmpty()
			? FString()
			: FString::Printf(TEXT(" | %s"), *diagnostic.Path);
		outDiagnostics.Add(FString::Printf(
			TEXT("[%s] %s: %s%s"),
			*severity,
			*diagnostic.Code,
			*diagnostic.Message,
			*pathSuffix));
	}
}

FScenarioTemplateNumberValue UScenarioAuthoringSubsystem::MakeFixedTemplateNumber(double value)
{
	FScenarioTemplateNumberValue numberValue;
	numberValue.bIsSet = true;
	numberValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
	numberValue.FixedValue = value;
	return numberValue;
}

FScenarioTemplateIntegerValue UScenarioAuthoringSubsystem::MakeFixedTemplateInteger(int32 value)
{
	FScenarioTemplateIntegerValue integerValue;
	integerValue.bIsSet = true;
	integerValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
	integerValue.FixedValue = value;
	return integerValue;
}

double UScenarioAuthoringSubsystem::GetFixedTemplateNumber(
	const FScenarioTemplateNumberValue& value,
	double defaultValue)
{
	if (!value.bIsSet)
	{
		return defaultValue;
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return (value.MinValue + value.MaxValue) * 0.5;
	}

	return value.FixedValue;
}

UScenarioCompiler* UScenarioAuthoringSubsystem::CreateScenarioCompiler() const
{
	UScenarioCompiler* compiler = NewObject<UScenarioCompiler>();
	if (!compiler) return nullptr;

	compiler->StaticObstaclePropCatalog = StaticObstaclePropCatalog;
	return compiler;
}

const UScenarioStaticObstaclePropCatalog* UScenarioAuthoringSubsystem::GetStaticObstaclePropCatalog() const
{
	const UScenarioStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog))
	{
		UE_LOG(
			LogScenarioAuthoring,
			Warning,
			TEXT("Scenario static obstacle prop catalog is not configured or failed to load: %s"),
			*StaticObstaclePropCatalog.ToSoftObjectPath().ToString());
		return nullptr;
	}

	return propCatalog;
}

bool UScenarioAuthoringSubsystem::TryFindStaticObstacleProp(
	FName propId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	if (propId.IsNone()) return false;

	const UScenarioStaticObstaclePropCatalog* propCatalog = GetStaticObstaclePropCatalog();
	if (!propCatalog) return false;

	return propCatalog->FindPropEntryById(propId, outPropEntry);
}

double UScenarioAuthoringSubsystem::ComputePlacementRadius2D(const FScenarioStaticObstaclePropEntry& propEntry) const
{
	if (propEntry.SafetyRadius > 0.0)
	{
		return propEntry.SafetyRadius;
	}

	return FMath::Sqrt(FMath::Square(propEntry.FallbackBoxExtent.X) + FMath::Square(propEntry.FallbackBoxExtent.Y));
}

FVector2D UScenarioAuthoringSubsystem::ComputePlacementHalfExtent2D(
	const FScenarioStaticObstaclePropEntry& propEntry) const
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

bool UScenarioAuthoringSubsystem::StaticObstacleFootprintsOverlap(
	const FVector& candidateLocation,
	const FVector2D& candidateHalfExtent,
	const FScenarioAuthoringStaticObstacleRecord& record) const
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

FString UScenarioAuthoringSubsystem::GenerateStaticObstacleInstanceId()
{
	FString instanceId;
	do
	{
		instanceId = FString::Printf(TEXT("obstacle_%03d"), NextStaticObstacleIndex++);
	}
	while (ContainsInstanceId(instanceId));

	return instanceId;
}

FString UScenarioAuthoringSubsystem::GeneratePedestrianInstanceId()
{
	FString instanceId;
	do
	{
		instanceId = FString::Printf(TEXT("ped_%03d"), NextPedestrianIndex++);
	}
	while (ContainsInstanceId(instanceId));

	return instanceId;
}

bool UScenarioAuthoringSubsystem::ContainsInstanceId(const FString& instanceId) const
{
	if (DraftScenarioTemplate.Obstacles.Placements.ContainsByPredicate(
			[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
			{
				return placement.PlacementId == instanceId;
			}))
	{
		return true;
	}

	for (const FScenarioDynamicActorSpec& spec : DraftPedestrianSpecs)
	{
		if (spec.InstanceId == instanceId)
		{
			return true;
		}
	}

	return false;
}

bool UScenarioAuthoringSubsystem::IsDraftScenarioTemplateEmpty() const
{
	return DraftScenarioTemplate.TemplateId.IsEmpty()
		&& DraftScenarioTemplate.Intent.IsEmpty()
		&& DraftScenarioTemplate.Corridor.Axis.PointsMeters.IsEmpty()
		&& DraftScenarioTemplate.Corridor.Segments.IsEmpty()
		&& DraftScenarioTemplate.Obstacles.Placements.IsEmpty()
		&& DraftGroundRegions.IsEmpty()
		&& DraftPedestrianSpecs.IsEmpty();
}

void UScenarioAuthoringSubsystem::InitializeDraftDefaults()
{
	DraftScenarioTemplate = FScenarioTemplateDocument();
	DraftScenarioTemplate.TemplateId = ScenarioId;
	DraftScenarioTemplate.Intent = TEXT("Editor authored scenario template.");
	DraftScenarioTemplate.Corridor.Axis.Type = EScenarioCorridorAxisType::Polyline;
	DraftScenarioTemplate.Corridor.Axis.PointsMeters =
	{
		FVector2D(DefaultRobotStartLocationCm.X * CentimetersToMeters, DefaultRobotStartLocationCm.Y * CentimetersToMeters),
		FVector2D(DefaultRobotGoalLocationCm.X * CentimetersToMeters, DefaultRobotGoalLocationCm.Y * CentimetersToMeters)
	};
	DraftScenarioTemplate.Corridor.WalkwayWidthMeters = MakeFixedTemplateNumber(3.0);

	FScenarioTemplateSegment mainSegment;
	mainSegment.SegmentId = TEXT("main");
	mainSegment.Type = EScenarioTemplateSegmentType::Straight;
	mainSegment.AlongRangeMeters.StartMeters = 0.0;
	mainSegment.AlongRangeMeters.EndMeters =
		(DraftScenarioTemplate.Corridor.Axis.PointsMeters[1] - DraftScenarioTemplate.Corridor.Axis.PointsMeters[0]).Size();
	DraftScenarioTemplate.Corridor.Segments.Add(mainSegment);

	FScenarioTemplateLaneRule buildingLane;
	buildingLane.SurfaceId = TEXT("building");
	buildingLane.WidthMeters = MakeFixedTemplateNumber(1.0);
	DraftScenarioTemplate.Corridor.BuildingSide.Add(buildingLane);

	FScenarioTemplateLaneRule curbLane;
	curbLane.SurfaceId = TEXT("road");
	curbLane.WidthMeters = MakeFixedTemplateNumber(1.0);
	DraftScenarioTemplate.Corridor.CurbSide.Add(curbLane);

	DraftScenarioTemplate.Obstacles.MinClearWidthMeters = MakeFixedTemplateNumber(1.0);
	DraftScenarioTemplate.Pedestrians.Background.Count = MakeFixedTemplateInteger(0);
	DraftScenarioTemplate.Pedestrians.Background.SpeedMetersPerSecond = MakeFixedTemplateNumber(1.2);
	DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(DefaultRobotStartLocationCm);
	DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(DefaultRobotGoalLocationCm);
}

bool UScenarioAuthoringSubsystem::EnsureSingleRobotRouteSpec(
	TArray<FString>& outDiagnostics,
	bool& bOutDraftChanged)
{
	bOutDraftChanged = false;

	if (IsDraftScenarioTemplateEmpty())
	{
		InitializeDraftDefaults();
		bOutDraftChanged = true;
		outDiagnostics.Add(TEXT("Robot route was missing; default StartPoint and GoalPoint were added."));
	}

	if (DraftScenarioTemplate.TemplateId.IsEmpty())
	{
		DraftScenarioTemplate.TemplateId = ScenarioId;
		bOutDraftChanged = true;
	}

	if (DraftScenarioTemplate.Intent.IsEmpty())
	{
		DraftScenarioTemplate.Intent = TEXT("Editor authored scenario template.");
		bOutDraftChanged = true;
	}

	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() < 2)
	{
		DraftScenarioTemplate.Corridor.Axis.PointsMeters =
		{
			FVector2D(DefaultRobotStartLocationCm.X * CentimetersToMeters, DefaultRobotStartLocationCm.Y * CentimetersToMeters),
			FVector2D(DefaultRobotGoalLocationCm.X * CentimetersToMeters, DefaultRobotGoalLocationCm.Y * CentimetersToMeters)
		};
		bOutDraftChanged = true;
	}

	if (DraftScenarioTemplate.Corridor.Segments.IsEmpty())
	{
		FScenarioTemplateSegment mainSegment;
		mainSegment.SegmentId = TEXT("main");
		mainSegment.Type = EScenarioTemplateSegmentType::Straight;
		mainSegment.AlongRangeMeters.StartMeters = 0.0;
		mainSegment.AlongRangeMeters.EndMeters =
			(DraftScenarioTemplate.Corridor.Axis.PointsMeters.Last() - DraftScenarioTemplate.Corridor.Axis.PointsMeters[0]).Size();
		DraftScenarioTemplate.Corridor.Segments.Add(mainSegment);
		bOutDraftChanged = true;
	}

	if (!DraftScenarioTemplate.Corridor.WalkwayWidthMeters.bIsSet)
	{
		DraftScenarioTemplate.Corridor.WalkwayWidthMeters = MakeFixedTemplateNumber(3.0);
		bOutDraftChanged = true;
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateSingleRobotRouteSpecForExport(TArray<FString>& outDiagnostics) const
{
	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() < 2)
	{
		outDiagnostics.Add(TEXT("Robot route axis must contain at least two points."));
		return false;
	}

	return true;
}

FScenarioWorldSpec UScenarioAuthoringSubsystem::BuildDraftWorldSpecForPreview() const
{
	FScenarioWorldSpec worldSpec;
	worldSpec.RunConfig.TemplateId = DraftScenarioTemplate.TemplateId.IsEmpty() ? ScenarioId : DraftScenarioTemplate.TemplateId;
	worldSpec.RunConfig.TemplateVersion = DraftScenarioTemplate.Version > 0 ? DraftScenarioTemplate.Version : FScenarioTemplateJson::SupportedVersion;
	worldSpec.RunConfig.GeneratorVersion = FScenarioTemplateJson::SupportedVersion;
	worldSpec.RunConfig.BaseSeed = BaseSeed;
	worldSpec.RunConfig.IterationIndex = IterationIndex;

	FScenarioParamValue timeLimitParam;
	timeLimitParam.Type = EScenarioParamValueType::Float;
	timeLimitParam.FloatValue = TimeLimitSeconds;
	worldSpec.RunConfig.Parameters.Add(TEXT("time_limit_s"), timeLimitParam);

	worldSpec.Seeds.WorldSeed = BaseSeed;
	worldSpec.Seeds.LayoutSeed = BaseSeed + 101;
	worldSpec.Seeds.StaticObstacleSeed = BaseSeed + 202;
	worldSpec.Seeds.DynamicActorSeed = BaseSeed + 303;
	worldSpec.Seeds.EventSeed = BaseSeed + 404;
	worldSpec.Seeds.PolicySeed = BaseSeed + 505;

	worldSpec.Placeables.Add(MakeDeliveryBotSpecFromTemplateRobot());
	for (const FScenarioTemplateObstaclePlacement& placement : DraftScenarioTemplate.Obstacles.Placements)
	{
		if (placement.Kind != EScenarioTemplateObstaclePlacementKind::Fixed || placement.PropId.IsEmpty())
		{
			continue;
		}
		worldSpec.Placeables.Add(MakeStaticObstacleSpecFromPlacement(placement));
	}
	worldSpec.GroundRegions = DraftGroundRegions;
	worldSpec.DynamicActors = DraftPedestrianSpecs;
	return worldSpec;
}

FScenarioTemplateRobotAnchor UScenarioAuthoringSubsystem::MakeRobotAnchorFromLocationCm(const FVector& locationCm) const
{
	FScenarioTemplateRobotAnchor anchor;
	anchor.Type = EScenarioTemplateRobotAnchorType::CorridorPose;
	anchor.SegmentId = DraftScenarioTemplate.Corridor.Segments.IsEmpty()
		? TEXT("main")
		: DraftScenarioTemplate.Corridor.Segments[0].SegmentId;
	anchor.AlongMeters = MakeFixedTemplateNumber(locationCm.X * CentimetersToMeters);
	anchor.OffsetMeters = MakeFixedTemplateNumber(locationCm.Y * CentimetersToMeters);
	anchor.LaneId = TEXT("walkway");
	anchor.Heading = EScenarioTemplateRobotHeading::Forward;
	return anchor;
}

FVector UScenarioAuthoringSubsystem::ResolveRobotAnchorLocationCm(
	const FScenarioTemplateRobotAnchor& anchor,
	bool bGoalAnchor) const
{
	if (anchor.Type == EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		return FVector(
			GetFixedTemplateNumber(anchor.AlongMeters, bGoalAnchor ? DefaultRobotGoalLocationCm.X * CentimetersToMeters : DefaultRobotStartLocationCm.X * CentimetersToMeters) / CentimetersToMeters,
			GetFixedTemplateNumber(anchor.OffsetMeters, bGoalAnchor ? DefaultRobotGoalLocationCm.Y * CentimetersToMeters : DefaultRobotStartLocationCm.Y * CentimetersToMeters) / CentimetersToMeters,
			0.0);
	}

	if (DraftScenarioTemplate.Corridor.Axis.PointsMeters.Num() >= 2)
	{
		const FVector2D axisPointMeters = anchor.Type == EScenarioTemplateRobotAnchorType::Exit
			? DraftScenarioTemplate.Corridor.Axis.PointsMeters.Last()
			: DraftScenarioTemplate.Corridor.Axis.PointsMeters[0];
		return FVector(
			axisPointMeters.X / CentimetersToMeters,
			axisPointMeters.Y / CentimetersToMeters,
			0.0);
	}

	return bGoalAnchor ? DefaultRobotGoalLocationCm : DefaultRobotStartLocationCm;
}

FScenarioTemplateObstaclePlacement UScenarioAuthoringSubsystem::MakeStaticObstaclePlacement(
	const FString& placementId,
	FName propId,
	const FTransform& transform) const
{
	FScenarioTemplateObstaclePlacement placement;
	placement.PlacementId = placementId;
	placement.Kind = EScenarioTemplateObstaclePlacementKind::Fixed;
	placement.PropId = propId.ToString();
	placement.At.SegmentId = DraftScenarioTemplate.Corridor.Segments.IsEmpty()
		? TEXT("main")
		: DraftScenarioTemplate.Corridor.Segments[0].SegmentId;
	placement.At.AlongMeters = MakeFixedTemplateNumber(transform.GetLocation().X * CentimetersToMeters);
	placement.At.OffsetMeters = MakeFixedTemplateNumber(transform.GetLocation().Y * CentimetersToMeters);
	placement.At.LaneId = TEXT("walkway");
	placement.YawDegrees = MakeFixedTemplateNumber(transform.Rotator().Yaw);
	return placement;
}

FScenarioPlaceableInstanceSpec UScenarioAuthoringSubsystem::MakeStaticObstacleSpecFromPlacement(
	const FScenarioTemplateObstaclePlacement& placement) const
{
	FScenarioPlaceableInstanceSpec spec;
	spec.InstanceId = placement.PlacementId;
	spec.AssetId = placement.PropId;
	spec.Category = EScenarioActorCategory::StaticObstacle;
	const FVector locationCm(
		GetFixedTemplateNumber(placement.At.AlongMeters, 0.0) / CentimetersToMeters,
		GetFixedTemplateNumber(placement.At.OffsetMeters, 0.0) / CentimetersToMeters,
		0.0);
	spec.Transform = FTransform(FRotator(0.0, GetFixedTemplateNumber(placement.YawDegrees, 0.0), 0.0), locationCm);
	return spec;
}

FScenarioPlaceableInstanceSpec UScenarioAuthoringSubsystem::MakeDeliveryBotSpecFromTemplateRobot() const
{
	const FVector startLocationCm = ResolveRobotAnchorLocationCm(DraftScenarioTemplate.Robot.Start, false);
	const FVector goalLocationCm = ResolveRobotAnchorLocationCm(DraftScenarioTemplate.Robot.Goal, true);

	FScenarioPlaceableInstanceSpec robotSpec;
	robotSpec.InstanceId = DefaultRobotInstanceId;
	robotSpec.AssetId = DefaultRobotAssetId;
	robotSpec.Category = EScenarioActorCategory::DeliveryBot;
	robotSpec.Transform = FTransform(FRotator::ZeroRotator, startLocationCm);
	robotSpec.DeliveryBot.bSpawnOnly = false;
	robotSpec.DeliveryBot.bHasStartLocation = true;
	robotSpec.DeliveryBot.bHasGoalLocation = true;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = startLocationCm;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = goalLocationCm;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
	return robotSpec;
}

FScenarioTemplateObstaclePlacement* UScenarioAuthoringSubsystem::FindStaticObstaclePlacementByInstanceId(
	const FString& instanceId)
{
	return DraftScenarioTemplate.Obstacles.Placements.FindByPredicate(
		[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
		{
			return placement.PlacementId == instanceId;
		});
}

const FScenarioTemplateObstaclePlacement* UScenarioAuthoringSubsystem::FindStaticObstaclePlacementByInstanceId(
	const FString& instanceId) const
{
	return DraftScenarioTemplate.Obstacles.Placements.FindByPredicate(
		[&instanceId](const FScenarioTemplateObstaclePlacement& placement)
		{
			return placement.PlacementId == instanceId;
		});
}

void UScenarioAuthoringSubsystem::ImportWorldSpecAsScenarioTemplate(const FScenarioWorldSpec& worldSpec)
{
	InitializeDraftDefaults();
	DraftScenarioTemplate.TemplateId = worldSpec.RunConfig.TemplateId.IsEmpty() ? ScenarioId : worldSpec.RunConfig.TemplateId;
	DraftScenarioTemplate.Version = worldSpec.RunConfig.TemplateVersion > 0 ? worldSpec.RunConfig.TemplateVersion : FScenarioTemplateJson::SupportedVersion;
	DraftScenarioTemplate.Obstacles.Placements.Reset();
	DraftGroundRegions = worldSpec.GroundRegions;
	DraftPedestrianSpecs = worldSpec.DynamicActors;

	for (const FScenarioPlaceableInstanceSpec& spec : worldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			DraftScenarioTemplate.Robot.Start = MakeRobotAnchorFromLocationCm(spec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm);
			if (spec.DeliveryBot.bHasGoalLocation)
			{
				DraftScenarioTemplate.Robot.Goal = MakeRobotAnchorFromLocationCm(spec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm);
			}
			continue;
		}

		if (spec.Category == EScenarioActorCategory::StaticObstacle)
		{
			DraftScenarioTemplate.Obstacles.Placements.Add(
				MakeStaticObstaclePlacement(spec.InstanceId, FName(*spec.AssetId), spec.Transform));
		}
	}
}

void UScenarioAuthoringSubsystem::ClearEditorView()
{
	for (const TObjectPtr<AActor>& markerActor : RouteMarkerActors)
	{
		if (IsValid(markerActor))
		{
			markerActor->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AScenarioStaticObstacle>>& pair : StaticObstacleActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AActor>>& pair : PedestrianActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	for (const TPair<FString, TObjectPtr<AScenarioGroundRegion>>& pair : GroundRegionActors)
	{
		if (IsValid(pair.Value))
		{
			pair.Value->Destroy();
		}
	}

	RouteMarkerActors.Reset();
	RobotStartMarkerActor = nullptr;
	RobotGoalMarkerActor = nullptr;
	StaticObstacleRecords.Reset();
	StaticObstacleActors.Reset();
	PedestrianActors.Reset();
	GroundRegionActors.Reset();
	NextStaticObstacleIndex = 1;
	NextPedestrianIndex = 1;
	NextGroundRegionIndex = 1;
}

bool UScenarioAuthoringSubsystem::RebuildEditorViewFromDraft(TArray<FString>& outDiagnostics)
{
	bool bDraftChanged = false;
	if (!EnsureSingleRobotRouteSpec(outDiagnostics, bDraftChanged))
	{
		return false;
	}
	if (bDraftChanged)
	{
		bDirty = true;
	}

	ClearEditorView();
	const FScenarioWorldSpec previewWorldSpec = BuildDraftWorldSpecForPreview();

	bool bSucceeded = true;
	for (const FScenarioPlaceableInstanceSpec& spec : previewWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			if (!SpawnRobotRouteMarkers(spec, outDiagnostics))
			{
				bSucceeded = false;
			}
			continue;
		}

		if (spec.Category != EScenarioActorCategory::StaticObstacle)
		{
			continue;
		}

		AScenarioStaticObstacle* spawnedActor = nullptr;
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

		FScenarioStaticObstaclePropEntry propEntry;
		if (TryFindStaticObstacleProp(FName(*spec.AssetId), propEntry))
		{
			AddStaticObstacleViewRecord(spec, propEntry, spawnedActor);
		}
	}

	for (const FScenarioDynamicActorSpec& spec : previewWorldSpec.DynamicActors)
	{
		if (spec.Category != EScenarioActorCategory::Pedestrian)
		{
			continue;
		}

		AActor* spawnedActor = nullptr;
		FString failureReason;
		if (!SpawnEditorPedestrianActor(spec, spawnedActor, failureReason))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Failed to create editor view for pedestrian '%s': %s"),
				*spec.InstanceId,
				*failureReason));
			bSucceeded = false;
			continue;
		}

		AddPedestrianViewRecord(spec, spawnedActor);
	}

	for (const FScenarioGroundRegionSpec& regionSpec : previewWorldSpec.GroundRegions)
	{
		AScenarioGroundRegion* spawnedRegion = nullptr;
		FString failureReason;
		if (!SpawnEditorGroundRegionActor(regionSpec, spawnedRegion, failureReason))
		{
			outDiagnostics.Add(FString::Printf(
				TEXT("Failed to create editor view for ground region '%s': %s"),
				*regionSpec.RegionId,
				*failureReason));
			bSucceeded = false;
		}
	}

	return bSucceeded;
}

bool UScenarioAuthoringSubsystem::SpawnRobotRouteMarkers(
	const FScenarioPlaceableInstanceSpec& spec,
	TArray<FString>& outDiagnostics)
{
	if (!StartPointClass)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("StartPointClass is not set; robot start marker was not spawned for '%s'."),
			*spec.InstanceId));
		return false;
	}

	AActor* startMarker = SpawnEditorMarkerActor(StartPointClass, FTransform(spec.Transform));
	if (!startMarker)
	{
		outDiagnostics.Add(FString::Printf(TEXT("Failed to spawn robot start marker for '%s'."), *spec.InstanceId));
		return false;
	}
	FString markerFailureReason;
	if (!ConfigureRobotRouteMarkerActor(
		startMarker,
		EScenarioPlaceableAuthoringRole::RobotStartMarker,
		markerFailureReason))
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Failed to configure robot start marker for '%s': %s"),
			*spec.InstanceId,
			*markerFailureReason));
		startMarker->Destroy();
		return false;
	}

	RobotStartMarkerActor = startMarker;
	RouteMarkerActors.Add(startMarker);
	const auto cleanupStartMarker = [this, startMarker]()
	{
		RouteMarkerActors.RemoveAll(
			[startMarker](const TObjectPtr<AActor>& markerActor)
			{
				return !IsValid(markerActor) || markerActor.Get() == startMarker;
			});

		if (RobotStartMarkerActor.Get() == startMarker)
		{
			RobotStartMarkerActor = nullptr;
		}
		if (IsValid(startMarker))
		{
			startMarker->Destroy();
		}
	};

	if (!spec.DeliveryBot.bHasGoalLocation)
	{
		outDiagnostics.Add(FString::Printf(TEXT("Robot goal marker is missing for '%s'."), *spec.InstanceId));
		cleanupStartMarker();
		return false;
	}

	if (!GoalPointClass)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("GoalPointClass is not set; robot goal marker was not spawned for '%s'."),
			*spec.InstanceId));
		cleanupStartMarker();
		return false;
	}

	const FTransform goalTransform(FRotator::ZeroRotator, spec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm);
	AActor* goalMarker = SpawnEditorMarkerActor(GoalPointClass, goalTransform);
	if (!goalMarker)
	{
		outDiagnostics.Add(FString::Printf(TEXT("Failed to spawn robot goal marker for '%s'."), *spec.InstanceId));
		cleanupStartMarker();
		return false;
	}
	if (!ConfigureRobotRouteMarkerActor(
		goalMarker,
		EScenarioPlaceableAuthoringRole::RobotGoalMarker,
		markerFailureReason))
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Failed to configure robot goal marker for '%s': %s"),
			*spec.InstanceId,
			*markerFailureReason));
		goalMarker->Destroy();
		cleanupStartMarker();
		return false;
	}

	RobotGoalMarkerActor = goalMarker;
	RouteMarkerActors.Add(goalMarker);
	return true;
}

AActor* UScenarioAuthoringSubsystem::SpawnEditorMarkerActor(
	TSubclassOf<AActor> markerClass,
	const FTransform& transform)
{
	UWorld* world = GetWorld();
	if (!world || !markerClass) return nullptr;

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return world->SpawnActor<AActor>(markerClass, transform, spawnParams);
}

AActor* UScenarioAuthoringSubsystem::SpawnOrReplaceRouteMarker(
	TObjectPtr<AActor>& markerActor,
	TSubclassOf<AActor> markerClass,
	const FTransform& transform,
	EScenarioPlaceableAuthoringRole markerRole,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!markerClass)
	{
		outFailureReason = TEXT("Marker actor class is not set.");
		return nullptr;
	}

	AActor* spawnedMarker = SpawnEditorMarkerActor(markerClass, transform);
	if (!spawnedMarker)
	{
		outFailureReason = TEXT("Failed to spawn marker actor.");
		return nullptr;
	}
	if (!ConfigureRobotRouteMarkerActor(spawnedMarker, markerRole, outFailureReason))
	{
		spawnedMarker->Destroy();
		return nullptr;
	}

	RouteMarkerActors.RemoveAll(
		[&markerActor](const TObjectPtr<AActor>& existingMarker)
		{
			return !IsValid(existingMarker) || existingMarker.Get() == markerActor.Get();
		});

	if (IsValid(markerActor))
	{
		markerActor->Destroy();
	}

	markerActor = spawnedMarker;
	RouteMarkerActors.Add(markerActor);
	return markerActor.Get();
}

bool UScenarioAuthoringSubsystem::ConfigureRobotRouteMarkerActor(
	AActor* markerActor,
	EScenarioPlaceableAuthoringRole markerRole,
	FString& outFailureReason) const
{
	outFailureReason.Reset();
	if (!markerActor)
	{
		outFailureReason = TEXT("Marker actor is null.");
		return false;
	}

	UScenarioPlaceableComponent* placeableComponent =
		markerActor->FindComponentByClass<UScenarioPlaceableComponent>();
	if (!placeableComponent)
	{
		const FName componentName = MakeUniqueObjectName(
			markerActor,
			UScenarioPlaceableComponent::StaticClass(),
			TEXT("RouteMarkerPlaceableComponent"));
		placeableComponent = NewObject<UScenarioPlaceableComponent>(markerActor, componentName);
		if (!placeableComponent)
		{
			outFailureReason = TEXT("Failed to create route marker placeable component.");
			return false;
		}

		markerActor->AddInstanceComponent(placeableComponent);
		placeableComponent->RegisterComponent();
	}

	const bool bStartMarker = markerRole == EScenarioPlaceableAuthoringRole::RobotStartMarker;
	placeableComponent->InstanceId = bStartMarker ? RobotStartMarkerInstanceId : RobotGoalMarkerInstanceId;
	placeableComponent->AssetId = bStartMarker ? RobotStartMarkerAssetId : RobotGoalMarkerAssetId;
	placeableComponent->Category = EScenarioActorCategory::DeliveryBot;
	placeableComponent->AuthoringRole = markerRole;
	placeableComponent->bAuthoringSelectable = true;
	placeableComponent->bAuthoringRenamable = false;
	placeableComponent->bAuthoringDeletable = false;
	placeableComponent->bAuthoringAllowLocationEdit = true;
	placeableComponent->bAuthoringAllowRotationEdit = false;
	placeableComponent->bAuthoringAllowScaleEdit = false;

	USphereComponent* selectionComponent = nullptr;
	TArray<USphereComponent*> sphereComponents;
	markerActor->GetComponents(sphereComponents);
	const FName selectionComponentTag(TEXT("ScenarioRouteMarkerSelection"));
	for (USphereComponent* sphereComponent : sphereComponents)
	{
		if (sphereComponent && sphereComponent->ComponentTags.Contains(selectionComponentTag))
		{
			selectionComponent = sphereComponent;
			break;
		}
	}
	if (!selectionComponent)
	{
		const FName componentName = MakeUniqueObjectName(
			markerActor,
			USphereComponent::StaticClass(),
			TEXT("RouteMarkerSelectionComponent"));
		selectionComponent = NewObject<USphereComponent>(markerActor, componentName);
		if (selectionComponent)
		{
			markerActor->AddInstanceComponent(selectionComponent);
			if (USceneComponent* rootComponent = markerActor->GetRootComponent())
			{
				selectionComponent->SetupAttachment(rootComponent);
			}
			selectionComponent->RegisterComponent();
		}
	}
	if (!selectionComponent)
	{
		outFailureReason = TEXT("Failed to create route marker selection component.");
		return false;
	}
	if (selectionComponent)
	{
		selectionComponent->ComponentTags.AddUnique(selectionComponentTag);
		selectionComponent->SetSphereRadius(75.0f, true);
		selectionComponent->SetRelativeLocation(FVector::ZeroVector);
		selectionComponent->SetHiddenInGame(true);
		selectionComponent->SetVisibility(false);
		selectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		selectionComponent->SetCollisionObjectType(ECC_WorldDynamic);
		selectionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		selectionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		selectionComponent->SetGenerateOverlapEvents(false);
	}
	return true;
}

bool UScenarioAuthoringSubsystem::SpawnEditorStaticObstacleActor(
	const FScenarioPlaceableInstanceSpec& spec,
	AScenarioStaticObstacle*& outActor,
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

	TSubclassOf<AScenarioStaticObstacle> spawnClass = StaticObstacleClass;
	if (!spawnClass)
	{
		spawnClass = AScenarioStaticObstacle::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AScenarioStaticObstacle* staticObstacle = world->SpawnActor<AScenarioStaticObstacle>(
		spawnClass,
		spec.Transform,
		spawnParams);
	if (!staticObstacle)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return false;
	}

	FScenarioStaticObstaclePropEntry propEntry;
	if (!TryFindStaticObstacleProp(FName(*spec.AssetId), propEntry))
	{
		outFailureReason = FString::Printf(TEXT("Unknown prop '%s'."), *spec.AssetId);
		staticObstacle->Destroy();
		return false;
	}

	if (!staticObstacle->ApplyPropEntry(propEntry))
	{
		outFailureReason = FString::Printf(TEXT("Failed to apply prop '%s'."), *spec.AssetId);
		staticObstacle->Destroy();
		return false;
	}

	ConfigureAuthoredStaticObstacleActor(staticObstacle, spec);
	outActor = staticObstacle;
	return true;
}

bool UScenarioAuthoringSubsystem::SpawnEditorPedestrianActor(
	const FScenarioDynamicActorSpec& spec,
	AActor*& outActor,
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

	TSubclassOf<AActor> spawnClass = PedestrianVisualizationActorClass;
	if (!spawnClass)
	{
		spawnClass = PedestrianClass.Get();
	}
	if (!spawnClass)
	{
		spawnClass = AScenarioPedestrian::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* pedestrian = world->SpawnActor<AActor>(
		spawnClass,
		spec.InitialTransform,
		spawnParams);
	if (!pedestrian)
	{
		outFailureReason = FString::Printf(
			TEXT("SpawnActor failed for pedestrian class '%s'."),
			*spawnClass->GetPathName());
		return false;
	}

	if (AScenarioPedestrian* episodePedestrian = Cast<AScenarioPedestrian>(pedestrian))
	{
		if (episodePedestrian->PathFollowerComponent)
		{
			episodePedestrian->PathFollowerComponent->bAutoStart = false;
			episodePedestrian->PathFollowerComponent->StopFollowing();
		}
		if (episodePedestrian->PedestrianRuntimeComponent)
		{
			episodePedestrian->PedestrianRuntimeComponent->bAutoStart = false;
			episodePedestrian->PedestrianRuntimeComponent->bEnableRobotReaction = false;
			episodePedestrian->PedestrianRuntimeComponent->StopFollowing();
		}
	}

	if (UScenarioPlaceableComponent* placeableComponent = pedestrian->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = spec.InstanceId;
		placeableComponent->AssetId = spec.AssetId;
		placeableComponent->Category = EScenarioActorCategory::Pedestrian;
		placeableComponent->bAuthoringSelectable = false;
	}
	else
	{
		UE_LOG(
			LogScenarioAuthoring,
			Verbose,
			TEXT("Spawned pedestrian editor actor has no ScenarioPlaceableComponent | Class: %s | InstanceId: %s"),
			*spawnClass->GetPathName(),
			*spec.InstanceId);
	}

	outActor = pedestrian;
	return true;
}

void UScenarioAuthoringSubsystem::AddStaticObstacleViewRecord(
	const FScenarioPlaceableInstanceSpec& spec,
	const FScenarioStaticObstaclePropEntry& propEntry,
	AScenarioStaticObstacle* actor)
{
	FScenarioAuthoringStaticObstacleRecord record;
	record.InstanceId = spec.InstanceId;
	record.PropId = FName(*spec.AssetId);
	record.Transform = spec.Transform;
	record.PlacementRadius2D = ComputePlacementRadius2D(propEntry);
	record.PlacementHalfExtent2D = ComputePlacementHalfExtent2D(propEntry);

	StaticObstacleRecords.Add(record);
	StaticObstacleActors.Add(spec.InstanceId, actor);
}

void UScenarioAuthoringSubsystem::AddPedestrianViewRecord(const FScenarioDynamicActorSpec& spec, AActor* actor)
{
	if (!actor || spec.InstanceId.IsEmpty())
	{
		return;
	}

	PedestrianActors.Add(spec.InstanceId, actor);
}

FScenarioPlaceableInstanceSpec UScenarioAuthoringSubsystem::MakeStaticObstacleSpec(
	const FString& instanceId,
	FName propId,
	const FTransform& transform) const
{
	FScenarioPlaceableInstanceSpec spec;
	spec.InstanceId = instanceId;
	spec.AssetId = propId.ToString();
	spec.Category = EScenarioActorCategory::StaticObstacle;
	spec.Transform = transform;
	return spec;
}

FScenarioDynamicActorSpec UScenarioAuthoringSubsystem::MakePedestrianSpec(
	const FString& instanceId,
	FName archetypeId,
	const FTransform& transform) const
{
	FScenarioDynamicActorSpec spec;
	spec.InstanceId = instanceId;
	spec.AssetId = archetypeId.IsNone() ? TEXT("adult_pedestrian") : archetypeId.ToString();
	spec.Category = EScenarioActorCategory::Pedestrian;
	spec.InitialTransform = transform;
	spec.Properties.Add(MovementModelKey, MakeStringParamValue(TEXT("static_placement")));
	spec.Properties.Add(AutoStartKey, MakeBoolParamValue(false));
	return spec;
}

FScenarioAuthoringStaticObstacleRecord* UScenarioAuthoringSubsystem::FindStaticObstacleRecordByInstanceId(
	const FString& instanceId)
{
	for (FScenarioAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (record.InstanceId == instanceId)
		{
			return &record;
		}
	}

	return nullptr;
}

const FScenarioAuthoringStaticObstacleRecord* UScenarioAuthoringSubsystem::FindStaticObstacleRecordByInstanceId(
	const FString& instanceId) const
{
	for (const FScenarioAuthoringStaticObstacleRecord& record : StaticObstacleRecords)
	{
		if (record.InstanceId == instanceId)
		{
			return &record;
		}
	}

	return nullptr;
}

void UScenarioAuthoringSubsystem::ConfigureAuthoredStaticObstacleActor(
	AScenarioStaticObstacle* actor,
	const FScenarioPlaceableInstanceSpec& spec) const
{
	if (!actor) return;

	if (UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = spec.InstanceId;
		placeableComponent->AssetId = spec.AssetId;
		placeableComponent->Category = spec.Category;
		placeableComponent->AuthoringRole = EScenarioPlaceableAuthoringRole::Generic;
		placeableComponent->bAuthoringSelectable = true;
		placeableComponent->bAuthoringRenamable = true;
		placeableComponent->bAuthoringDeletable = true;
		placeableComponent->bAuthoringAllowLocationEdit = true;
		placeableComponent->bAuthoringAllowRotationEdit = true;
		placeableComponent->bAuthoringAllowScaleEdit = true;
	}
}
