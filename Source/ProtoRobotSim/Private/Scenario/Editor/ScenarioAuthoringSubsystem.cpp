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
		TEXT("/Game/Blueprints/Episode/BP_EpisodePedestrian"));
	if (pedestrianBlueprintClass.Succeeded())
	{
		PedestrianClass = pedestrianBlueprintClass.Class;
	}

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
	DraftWorldSpec = FScenarioWorldSpec();
	SourceEpisodeSetupJsonPath.Reset();
	bDirty = false;
	NextStaticObstacleIndex = 1;
	NextPedestrianIndex = 1;
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

bool UScenarioAuthoringSubsystem::LoadEpisodeSetupJsonFile(
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

	UScenarioCompiler* compiler = CreateScenarioCompiler();
	if (!compiler)
	{
		outDiagnostics.Add(TEXT("Episode compiler creation failed."));
		return false;
	}

	const FScenarioCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonFile(outResolvedJsonFilePath);
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

bool UScenarioAuthoringSubsystem::LoadEpisodeSetupJsonString(
	const FString& jsonString,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	if (jsonString.IsEmpty())
	{
		outDiagnostics.Add(TEXT("EpisodeSetup JSON string is empty."));
		return false;
	}

	UScenarioCompiler* compiler = CreateScenarioCompiler();
	if (!compiler)
	{
		outDiagnostics.Add(TEXT("Episode compiler creation failed."));
		return false;
	}

	const FScenarioCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonString(jsonString);
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

bool UScenarioAuthoringSubsystem::ImportCompiledWorldSpec(
	const FScenarioWorldSpec& worldSpec,
	TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	DraftWorldSpec = worldSpec;
	SourceEpisodeSetupJsonPath.Reset();
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

	const FScenarioPlaceableInstanceSpec* spec = FindStaticObstacleSpecByInstanceId(instanceId);
	if (!spec)
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

	return CanPlaceStaticObstacleInternal(FName(*spec->AssetId), transform, instanceId, outFailureReason);
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

	if (DraftWorldSpec.RunConfig.TemplateId.IsEmpty()
		&& DraftWorldSpec.Placeables.IsEmpty()
		&& DraftWorldSpec.DynamicActors.IsEmpty()
		&& DraftWorldSpec.GroundRegions.IsEmpty()
		&& DraftWorldSpec.Paths.IsEmpty())
	{
		InitializeDraftDefaults();
	}

	const FString instanceId = GeneratePedestrianInstanceId();
	outSpec = MakePedestrianSpec(instanceId, archetypeId, transform);

	if (!SpawnEditorPedestrianActor(outSpec, outActor, outFailureReason))
	{
		return false;
	}

	DraftWorldSpec.DynamicActors.Add(outSpec);
	AddPedestrianViewRecord(outSpec, outActor);
	DraftWorldSpec.SpecHash.Reset();
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

	if (DraftWorldSpec.RunConfig.TemplateId.IsEmpty()
		&& DraftWorldSpec.Placeables.IsEmpty()
		&& DraftWorldSpec.DynamicActors.IsEmpty()
		&& DraftWorldSpec.GroundRegions.IsEmpty()
		&& DraftWorldSpec.Paths.IsEmpty())
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

	FScenarioPlaceableInstanceSpec* robotSpec = FindDeliveryBotSpec();
	if (!robotSpec)
	{
		FScenarioPlaceableInstanceSpec newRobotSpec;
		newRobotSpec.InstanceId = ContainsInstanceId(TEXT("robot_01")) ? FString::Printf(TEXT("robot_%03d"), DraftWorldSpec.Placeables.Num() + 1) : TEXT("robot_01");
		newRobotSpec.AssetId = assetId.IsNone() ? TEXT("delivery_bot") : assetId.ToString();
		newRobotSpec.Category = EScenarioActorCategory::DeliveryBot;
		newRobotSpec.DeliveryBot.bSpawnOnly = true;
		DraftWorldSpec.Placeables.Add(newRobotSpec);
		robotSpec = &DraftWorldSpec.Placeables.Last();
	}

	robotSpec->Transform = transform;
	robotSpec->AssetId = assetId.IsNone() ? TEXT("delivery_bot") : assetId.ToString();
	robotSpec->DeliveryBot.bHasStartLocation = true;
	robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = transform.GetLocation();
	if (!robotSpec->DeliveryBot.bHasGoalLocation)
	{
		robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = transform.GetLocation();
	}

	outSpec = *robotSpec;
	DraftWorldSpec.SpecHash.Reset();
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Set robot start | InstanceId: %s | Location: %s"),
		*robotSpec->InstanceId,
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

	FScenarioPlaceableInstanceSpec* robotSpec = FindDeliveryBotSpec();
	if (!robotSpec || !robotSpec->DeliveryBot.bHasStartLocation)
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

	robotSpec->DeliveryBot.bSpawnOnly = false;
	robotSpec->DeliveryBot.bHasGoalLocation = true;
	robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = transform.GetLocation();
	robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;

	outSpec = *robotSpec;
	DraftWorldSpec.SpecHash.Reset();
	bDirty = true;

	UE_LOG(
		LogScenarioAuthoring,
		Log,
		TEXT("Set robot goal | InstanceId: %s | Location: %s"),
		*robotSpec->InstanceId,
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

	FScenarioPlaceableInstanceSpec* spec = FindStaticObstacleSpecByInstanceId(instanceId);
	FScenarioAuthoringStaticObstacleRecord* record = FindStaticObstacleRecordByInstanceId(instanceId);
	if (!spec || !record)
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

	spec->Transform = transform;
	record->Transform = transform;
	actor->SetActorTransform(transform, false, nullptr, ETeleportType::TeleportPhysics);

	DraftWorldSpec.SpecHash.Reset();
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

	FScenarioPlaceableInstanceSpec* robotSpec = FindDeliveryBotSpec();
	if (!robotSpec || !robotSpec->DeliveryBot.bHasStartLocation || !robotSpec->DeliveryBot.bHasGoalLocation)
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
	robotSpec->Transform.SetLocation(location);
	robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = location;
	RobotStartMarkerActor->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);

	DraftWorldSpec.SpecHash.Reset();
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

	FScenarioPlaceableInstanceSpec* robotSpec = FindDeliveryBotSpec();
	if (!robotSpec || !robotSpec->DeliveryBot.bHasStartLocation || !robotSpec->DeliveryBot.bHasGoalLocation)
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
	robotSpec->DeliveryBot.bSpawnOnly = false;
	robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = location;
	robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
	RobotGoalMarkerActor->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);

	DraftWorldSpec.SpecHash.Reset();
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

	FScenarioPlaceableInstanceSpec* spec = FindStaticObstacleSpecByInstanceId(oldInstanceId);
	FScenarioAuthoringStaticObstacleRecord* record = FindStaticObstacleRecordByInstanceId(oldInstanceId);
	if (!spec || !record)
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

	spec->InstanceId = trimmedNewInstanceId;
	record->InstanceId = trimmedNewInstanceId;
	StaticObstacleActors.Remove(oldInstanceId);
	TObjectPtr<AScenarioStaticObstacle> renamedActorPtr = actor;
	StaticObstacleActors.Add(trimmedNewInstanceId, renamedActorPtr);

	if (UScenarioPlaceableComponent* placeableComponent = actor->FindComponentByClass<UScenarioPlaceableComponent>())
	{
		placeableComponent->InstanceId = trimmedNewInstanceId;
	}

	DraftWorldSpec.SpecHash.Reset();
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

	const int32 removedSpecCount = DraftWorldSpec.Placeables.RemoveAll(
		[&instanceId](const FScenarioPlaceableInstanceSpec& spec)
		{
			return spec.InstanceId == instanceId && spec.Category == EScenarioActorCategory::StaticObstacle;
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

	DraftWorldSpec.SpecHash.Reset();
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

	if (DraftWorldSpec.RunConfig.TemplateId.IsEmpty()
		&& DraftWorldSpec.Placeables.IsEmpty()
		&& DraftWorldSpec.DynamicActors.IsEmpty()
		&& DraftWorldSpec.GroundRegions.IsEmpty()
		&& DraftWorldSpec.Paths.IsEmpty())
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

	DraftWorldSpec.GroundRegions.Add(outSpec);
	DraftWorldSpec.SpecHash.Reset();
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

	FScenarioGroundRegionSpec* regionSpec = DraftWorldSpec.GroundRegions.FindByPredicate(
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

	DraftWorldSpec.SpecHash.Reset();
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

	const int32 removedSpecCount = DraftWorldSpec.GroundRegions.RemoveAll(
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

	DraftWorldSpec.SpecHash.Reset();
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
	for (const FScenarioGroundRegionSpec& spec : DraftWorldSpec.GroundRegions)
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

	FScenarioStaticObstaclePropEntry propEntry;
	TryFindStaticObstacleProp(propId, propEntry);
	AddStaticObstacleViewRecord(outSpec, propEntry, outActor);
	DraftWorldSpec.Placeables.Add(outSpec);
	DraftWorldSpec.SpecHash.Reset();
	bDirty = true;
	return true;
}

TArray<FScenarioPlaceableInstanceSpec> UScenarioAuthoringSubsystem::GetAuthoredStaticObstacleSpecs() const
{
	TArray<FScenarioPlaceableInstanceSpec> staticObstacleSpecs;
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::StaticObstacle)
		{
			staticObstacleSpecs.Add(spec);
		}
	}

	return staticObstacleSpecs;
}

bool UScenarioAuthoringSubsystem::ExportEpisodeSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();
	if (!ValidateSingleRobotRouteSpecForExport(outDiagnostics))
	{
		return false;
	}

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
	for (const FScenarioGroundRegionSpec& regionSpec : DraftWorldSpec.GroundRegions)
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
	for (const FScenarioPathSpec& pathSpec : DraftWorldSpec.Paths)
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
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category != EScenarioActorCategory::StaticObstacle)
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
	for (const FScenarioDynamicActorSpec& dynamicActorSpec : DraftWorldSpec.DynamicActors)
	{
		if (dynamicActorSpec.Category != EScenarioActorCategory::Pedestrian)
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
		const bool bPlannedTrajectory = movementModel.Equals(TEXT("planned_trajectory"), ESearchCase::IgnoreCase);
		const bool bStaticPlacement = movementModel.Equals(TEXT("static_placement"), ESearchCase::IgnoreCase);
		if (!bPlannedTrajectory && !bStaticPlacement)
		{
			pedestrianObject->SetStringField(TEXT("path_id"), dynamicActorSpec.PathId);
		}
		else if (bPlannedTrajectory)
		{
			FVector plannedStartCm = dynamicActorSpec.InitialTransform.GetLocation();
			FVector plannedGoalCm = FVector::ZeroVector;
			const FScenarioParamValue* startParam = dynamicActorSpec.Properties.Find(PlannedStartCmKey);
			const FScenarioParamValue* goalParam = dynamicActorSpec.Properties.Find(PlannedGoalCmKey);
			if (startParam && startParam->Type == EScenarioParamValueType::Vector)
			{
				plannedStartCm = startParam->VectorValue;
			}
			if (goalParam && goalParam->Type == EScenarioParamValueType::Vector)
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

	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category != EScenarioActorCategory::DeliveryBot)
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

bool UScenarioAuthoringSubsystem::ExportAndValidateEpisodeSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	if (!ExportEpisodeSetupJsonString(outJsonString, outDiagnostics))
	{
		return false;
	}

	UScenarioCompiler* compiler = CreateScenarioCompiler();
	if (!compiler)
	{
		outDiagnostics.Add(TEXT("Episode compiler creation failed."));
		return false;
	}

	const FScenarioCompileResult compileResult = compiler->CompileEpisodeWorldSpecFromJsonString(outJsonString);
	AppendCompileDiagnostics(compileResult, outDiagnostics);

	if (!compileResult.bSuccess)
	{
		outDiagnostics.Add(TEXT("Exported EpisodeSetup JSON failed compiler validation."));
	}

	return compileResult.bSuccess;
}

bool UScenarioAuthoringSubsystem::SaveEpisodeSetupJsonFile(
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

FString UScenarioAuthoringSubsystem::ResolveProjectRelativePath(const FString& filePath)
{
	if (FPaths::IsRelative(filePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), filePath);
	}

	return filePath;
}

FString UScenarioAuthoringSubsystem::ResolveEpisodeSetupLoadPath(const FString& filePath) const
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
			TEXT("Episode static obstacle prop catalog is not configured or failed to load: %s"),
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
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.InstanceId == instanceId)
		{
			return true;
		}
	}

	for (const FScenarioDynamicActorSpec& spec : DraftWorldSpec.DynamicActors)
	{
		if (spec.InstanceId == instanceId)
		{
			return true;
		}
	}

	return false;
}

void UScenarioAuthoringSubsystem::InitializeDraftDefaults()
{
	DraftWorldSpec = FScenarioWorldSpec();
	DraftWorldSpec.RunConfig.TemplateId = ScenarioId;
	DraftWorldSpec.RunConfig.TemplateVersion = 1;
	DraftWorldSpec.RunConfig.BaseSeed = BaseSeed;
	DraftWorldSpec.RunConfig.IterationIndex = IterationIndex;

	FScenarioParamValue timeLimitParam;
	timeLimitParam.Type = EScenarioParamValueType::Float;
	timeLimitParam.FloatValue = TimeLimitSeconds;
	DraftWorldSpec.RunConfig.Parameters.Add(TEXT("time_limit_s"), timeLimitParam);

	DraftWorldSpec.Seeds.WorldSeed = BaseSeed;
	DraftWorldSpec.Seeds.LayoutSeed = BaseSeed + 101;
	DraftWorldSpec.Seeds.StaticObstacleSeed = BaseSeed + 202;
	DraftWorldSpec.Seeds.DynamicActorSeed = BaseSeed + 303;
	DraftWorldSpec.Seeds.EventSeed = BaseSeed + 404;
	DraftWorldSpec.Seeds.PolicySeed = BaseSeed + 505;

	FScenarioPlaceableInstanceSpec robotSpec;
	robotSpec.InstanceId = DefaultRobotInstanceId;
	robotSpec.AssetId = DefaultRobotAssetId;
	robotSpec.Category = EScenarioActorCategory::DeliveryBot;
	robotSpec.Transform = FTransform(FRotator::ZeroRotator, DefaultRobotStartLocationCm);
	robotSpec.DeliveryBot.bSpawnOnly = false;
	robotSpec.DeliveryBot.bHasStartLocation = true;
	robotSpec.DeliveryBot.bHasGoalLocation = true;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = DefaultRobotStartLocationCm;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = DefaultRobotGoalLocationCm;
	robotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
	DraftWorldSpec.Placeables.Add(robotSpec);
}

bool UScenarioAuthoringSubsystem::EnsureSingleRobotRouteSpec(
	TArray<FString>& outDiagnostics,
	bool& bOutDraftChanged)
{
	bOutDraftChanged = false;

	FScenarioPlaceableInstanceSpec* robotSpec = nullptr;
	int32 robotSpecCount = 0;
	for (FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			++robotSpecCount;
			if (!robotSpec)
			{
				robotSpec = &spec;
			}
		}
	}

	if (robotSpecCount > 1)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Episode editor requires exactly one robot route, but %d robot specs were found."),
			robotSpecCount));
		return false;
	}

	if (!robotSpec)
	{
		FScenarioPlaceableInstanceSpec newRobotSpec;
		newRobotSpec.InstanceId = DefaultRobotInstanceId;
		newRobotSpec.AssetId = DefaultRobotAssetId;
		newRobotSpec.Category = EScenarioActorCategory::DeliveryBot;
		newRobotSpec.Transform = FTransform(FRotator::ZeroRotator, DefaultRobotStartLocationCm);
		newRobotSpec.DeliveryBot.bSpawnOnly = false;
		newRobotSpec.DeliveryBot.bHasStartLocation = true;
		newRobotSpec.DeliveryBot.bHasGoalLocation = true;
		newRobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = DefaultRobotStartLocationCm;
		newRobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm = DefaultRobotGoalLocationCm;
		newRobotSpec.DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
		DraftWorldSpec.Placeables.Add(newRobotSpec);
		bOutDraftChanged = true;
		outDiagnostics.Add(TEXT("Robot route was missing; default StartPoint and GoalPoint were added."));
		return true;
	}

	if (robotSpec->InstanceId.IsEmpty())
	{
		robotSpec->InstanceId = ContainsInstanceId(DefaultRobotInstanceId)
			? FString::Printf(TEXT("robot_%03d"), DraftWorldSpec.Placeables.Num() + 1)
			: DefaultRobotInstanceId;
		bOutDraftChanged = true;
	}
	if (robotSpec->AssetId.IsEmpty())
	{
		robotSpec->AssetId = DefaultRobotAssetId;
		bOutDraftChanged = true;
	}

	if (!robotSpec->DeliveryBot.bHasStartLocation)
	{
		robotSpec->Transform.SetLocation(DefaultRobotStartLocationCm);
		robotSpec->DeliveryBot.bHasStartLocation = true;
		robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = DefaultRobotStartLocationCm;
		bOutDraftChanged = true;
	}
	else if (!robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.Equals(robotSpec->Transform.GetLocation()))
	{
		robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm = robotSpec->Transform.GetLocation();
		bOutDraftChanged = true;
	}

	if (!robotSpec->DeliveryBot.bHasGoalLocation)
	{
		const FVector defaultRouteOffset = DefaultRobotGoalLocationCm - DefaultRobotStartLocationCm;
		robotSpec->DeliveryBot.bHasGoalLocation = true;
		robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm =
			robotSpec->Transform.GetLocation() + defaultRouteOffset;
		bOutDraftChanged = true;
		outDiagnostics.Add(TEXT("Robot goal was missing; a default GoalPoint was added."));
	}

	if (robotSpec->DeliveryBot.bSpawnOnly)
	{
		robotSpec->DeliveryBot.bSpawnOnly = false;
		bOutDraftChanged = true;
	}
	if (!robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute)
	{
		robotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bAutoStartRoute = true;
		bOutDraftChanged = true;
	}

	return true;
}

bool UScenarioAuthoringSubsystem::ValidateSingleRobotRouteSpecForExport(TArray<FString>& outDiagnostics) const
{
	const FScenarioPlaceableInstanceSpec* robotSpec = nullptr;
	int32 robotSpecCount = 0;
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			++robotSpecCount;
			if (!robotSpec)
			{
				robotSpec = &spec;
			}
		}
	}

	if (robotSpecCount != 1 || !robotSpec)
	{
		outDiagnostics.Add(FString::Printf(
			TEXT("Episode must contain exactly one robot route before export. Found: %d."),
			robotSpecCount));
		return false;
	}
	if (!robotSpec->DeliveryBot.bHasStartLocation)
	{
		outDiagnostics.Add(TEXT("Robot StartPoint is missing."));
		return false;
	}
	if (!robotSpec->DeliveryBot.bHasGoalLocation)
	{
		outDiagnostics.Add(TEXT("Robot GoalPoint is missing."));
		return false;
	}
	if (robotSpec->DeliveryBot.bSpawnOnly)
	{
		outDiagnostics.Add(TEXT("Robot route cannot be exported as spawn_only when StartPoint and GoalPoint are authored."));
		return false;
	}

	return true;
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
		DraftWorldSpec.SpecHash.Reset();
		bDirty = true;
	}

	ClearEditorView();

	bool bSucceeded = true;
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
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

	for (const FScenarioDynamicActorSpec& spec : DraftWorldSpec.DynamicActors)
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

	for (const FScenarioGroundRegionSpec& regionSpec : DraftWorldSpec.GroundRegions)
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
	const FName selectionComponentTag(TEXT("EpisodeRouteMarkerSelection"));
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

FScenarioPlaceableInstanceSpec* UScenarioAuthoringSubsystem::FindDeliveryBotSpec()
{
	for (FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			return &spec;
		}
	}

	return nullptr;
}

const FScenarioPlaceableInstanceSpec* UScenarioAuthoringSubsystem::FindDeliveryBotSpec() const
{
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.Category == EScenarioActorCategory::DeliveryBot)
		{
			return &spec;
		}
	}

	return nullptr;
}

FScenarioPlaceableInstanceSpec* UScenarioAuthoringSubsystem::FindStaticObstacleSpecByInstanceId(
	const FString& instanceId)
{
	for (FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.InstanceId == instanceId && spec.Category == EScenarioActorCategory::StaticObstacle)
		{
			return &spec;
		}
	}

	return nullptr;
}

const FScenarioPlaceableInstanceSpec* UScenarioAuthoringSubsystem::FindStaticObstacleSpecByInstanceId(
	const FString& instanceId) const
{
	for (const FScenarioPlaceableInstanceSpec& spec : DraftWorldSpec.Placeables)
	{
		if (spec.InstanceId == instanceId && spec.Category == EScenarioActorCategory::StaticObstacle)
		{
			return &spec;
		}
	}

	return nullptr;
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
