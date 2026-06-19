#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioEvaluationSubsystem.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/WorldInitializationValues.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Shared/ScenarioSpecTypes.h"

namespace
{
	// Owns one transient game world and its evaluation subsystem for one automation test case.
	struct FScenarioEvaluationTestWorld
	{
		// Creates a lightweight world without renderer, physics, navigation, or audio services.
		FScenarioEvaluationTestWorld()
		{
			const UWorld::InitializationValues initValues = UWorld::InitializationValues()
				.InitializeScenes(false)
				.AllowAudioPlayback(false)
				.RequiresHitProxies(false)
				.CreatePhysicsScene(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false)
				.CreateFXSystem(false);
			static int32 nextWorldIndex = 0;
			const FName worldName(*FString::Printf(
				TEXT("ScenarioEvaluationSubsystemTestWorld_%d"),
				++nextWorldIndex));

			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				worldName,
				GetTransientPackage(),
				false,
				ERHIFeatureLevel::Num,
				&initValues);
			if (World)
			{
				EvaluationSubsystem = World->GetSubsystem<UScenarioEvaluationSubsystem>();
			}
		}

		// Stops evaluation before destroying the transient world owned by this fixture.
		~FScenarioEvaluationTestWorld()
		{
			if (EvaluationSubsystem)
			{
				EvaluationSubsystem->StopEvaluation();
			}
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		// Spawns a simple actor with a movable scene root so transform-based detectors can query it.
		AActor* SpawnActor(const FVector& location, const FRotator& rotation = FRotator::ZeroRotator) const
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* actor = World->SpawnActor<AActor>(
				AActor::StaticClass(),
				FTransform(rotation, location),
				spawnParams);
			if (!actor)
			{
				return nullptr;
			}

			USceneComponent* rootComponent = NewObject<USceneComponent>(actor, TEXT("ScenarioEvaluationTestRoot"));
			actor->AddInstanceComponent(rootComponent);
			actor->SetRootComponent(rootComponent);
			rootComponent->RegisterComponent();
			actor->SetActorLocationAndRotation(location, rotation);
			return actor;
		}

		// Advances only the world clock counters used by timeout evaluation.
		void AdvanceWorldTime(float deltaSeconds) const
		{
			if (World)
			{
				World->TimeSeconds += deltaSeconds;
				World->UnpausedTimeSeconds += deltaSeconds;
				World->RealTimeSeconds += deltaSeconds;
				World->AudioTimeSeconds += deltaSeconds;
				World->DeltaTimeSeconds = deltaSeconds;
				World->DeltaRealTimeSeconds = deltaSeconds;
			}
		}

		// Transient world containing the test actors.
		UWorld* World = nullptr;
		// Runtime subsystem under test, owned by World.
		UScenarioEvaluationSubsystem* EvaluationSubsystem = nullptr;
	};

	// Returns thresholds that make the test actors trigger deterministic events.
	FScenarioEvaluationConfig MakeEvaluationTestConfig()
	{
		FScenarioEvaluationConfig config;
		config.GoalAcceptanceRadiusCm = 50.0;
		config.TipOverAngleDegrees = 45.0;
		config.NearMissDistanceCm = 100.0;
		return config;
	}

	// Builds the minimum runtime context required by StartEvaluation.
	FScenarioRuntimeContext MakeEvaluationTestContext(AActor* robotActor)
	{
		FScenarioRuntimeContext context;
		context.EpisodeId = TEXT("scenario_evaluation_test_episode");
		context.RobotInstanceId = TEXT("robot");
		context.RobotActor = robotActor;
		return context;
	}

	// Finds the first event of a type in the current evaluation result.
	const FEpisodeEvaluationEvent* FindEvaluationEvent(
		const FEpisodeEvaluationResult& result,
		EEpisodeEvaluationEventType eventType)
	{
		for (const FEpisodeEvaluationEvent& event : result.Events)
		{
			if (event.EventType == eventType)
			{
				return &event;
			}
		}

		return nullptr;
	}

	// Verifies that an event or metric parameter exists with the expected typed value kind.
	bool HasParamType(
		FAutomationTestBase& test,
		const TMap<FString, FScenarioParamValue>& params,
		const FString& key,
		EScenarioParamValueType expectedType)
	{
		const FScenarioParamValue* value = params.Find(key);
		if (!test.TestTrue(FString::Printf(TEXT("param exists: %s"), *key), value != nullptr))
		{
			return false;
		}

		return test.TestEqual(
			FString::Printf(TEXT("param type: %s"), *key),
			static_cast<int32>(value->Type),
			static_cast<int32>(expectedType));
	}

	// Verifies a numeric snapshot field.
	bool HasFloatParam(
		FAutomationTestBase& test,
		const TMap<FString, FScenarioParamValue>& params,
		const FString& key)
	{
		return HasParamType(test, params, key, EScenarioParamValueType::Float);
	}

	// Verifies a string snapshot field.
	bool HasStringParam(
		FAutomationTestBase& test,
		const TMap<FString, FScenarioParamValue>& params,
		const FString& key)
	{
		return HasParamType(test, params, key, EScenarioParamValueType::String);
	}

	// Spawns a configured rectangular region that contains the supplied center point.
	AScenarioGroundRegion* SpawnEvaluationTestRegion(
		UWorld* world,
		const FString& regionId,
		EScenarioGroundRegionType regionType,
		const FVector& center,
		double violationAfterSeconds = 0.0)
	{
		FScenarioGroundRegionSpec spec;
		spec.RegionId = regionId;
		spec.RegionType = regionType;
		spec.ShapeType = EScenarioGroundShapeType::Rectangle;
		spec.Center = center;
		spec.Size = FVector2D(400.0, 400.0);
		spec.ViolationAfterSeconds = violationAfterSeconds;

		FString failureReason;
		return AScenarioGroundRegion::SpawnConfigured(
			world,
			AScenarioGroundRegion::StaticClass(),
			spec,
			failureReason);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEvaluationGoalReachedRuntimeTest,
	"OdiroSim.ScenarioEvaluation.Events.GoalReached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEvaluationGoalReachedRuntimeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FScenarioEvaluationTestWorld testWorld;
	TestTrue(TEXT("world created"), testWorld.World != nullptr);
	TestTrue(TEXT("evaluation subsystem created"), testWorld.EvaluationSubsystem != nullptr);
	if (!testWorld.World || !testWorld.EvaluationSubsystem) return false;

	AActor* robotActor = testWorld.SpawnActor(FVector::ZeroVector);
	TestTrue(TEXT("robot actor spawned"), robotActor != nullptr);
	if (!robotActor) return false;

	FScenarioRuntimeContext context = MakeEvaluationTestContext(robotActor);
	context.bHasGoalLocation = true;
	context.GoalLocation = FVector(25.0, 0.0, 0.0);

	TestTrue(
		TEXT("evaluation started"),
		testWorld.EvaluationSubsystem->StartEvaluation(MakeEvaluationTestConfig(), context, 0.0));
	testWorld.EvaluationSubsystem->Tick(0.0f);

	const FEpisodeEvaluationResult result = testWorld.EvaluationSubsystem->GetCurrentResult();
	TestTrue(TEXT("goal reached completes episode"), result.bCompleted);
	TestTrue(TEXT("goal reached succeeds"), result.bSuccess);
	TestEqual(
		TEXT("goal reached terminal reason"),
		static_cast<int32>(result.TerminalReason),
		static_cast<int32>(EEpisodeEvaluationTerminalReason::GoalReached));
	HasFloatParam(*this, result.Metrics, TEXT("distance_to_goal_m"));
	HasFloatParam(*this, result.Metrics, TEXT("goal_threshold_m"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEvaluationRobotTipOverRuntimeTest,
	"OdiroSim.ScenarioEvaluation.Events.RobotTipOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEvaluationRobotTipOverRuntimeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FScenarioEvaluationTestWorld testWorld;
	TestTrue(TEXT("world created"), testWorld.World != nullptr);
	TestTrue(TEXT("evaluation subsystem created"), testWorld.EvaluationSubsystem != nullptr);
	if (!testWorld.World || !testWorld.EvaluationSubsystem) return false;

	AActor* robotActor = testWorld.SpawnActor(FVector::ZeroVector, FRotator(70.0, 0.0, 0.0));
	TestTrue(TEXT("robot actor spawned"), robotActor != nullptr);
	if (!robotActor) return false;

	TestTrue(
		TEXT("evaluation started"),
		testWorld.EvaluationSubsystem->StartEvaluation(MakeEvaluationTestConfig(), MakeEvaluationTestContext(robotActor), 0.0));
	testWorld.EvaluationSubsystem->Tick(0.0f);

	const FEpisodeEvaluationResult result = testWorld.EvaluationSubsystem->GetCurrentResult();
	TestTrue(TEXT("tip-over completes episode"), result.bCompleted);
	TestEqual(
		TEXT("tip-over terminal reason"),
		static_cast<int32>(result.TerminalReason),
		static_cast<int32>(EEpisodeEvaluationTerminalReason::RobotTipOver));

	const FEpisodeEvaluationEvent* event = FindEvaluationEvent(result, EEpisodeEvaluationEventType::RobotTipOver);
	TestTrue(TEXT("tip-over event recorded"), event != nullptr);
	if (!event) return false;

	HasFloatParam(*this, event->Properties, TEXT("roll_degree"));
	HasFloatParam(*this, event->Properties, TEXT("pitch_degree"));
	HasFloatParam(*this, event->Properties, TEXT("threshold_degree"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEvaluationTimeoutRuntimeTest,
	"OdiroSim.ScenarioEvaluation.Events.Timeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEvaluationTimeoutRuntimeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FScenarioEvaluationTestWorld testWorld;
	TestTrue(TEXT("world created"), testWorld.World != nullptr);
	TestTrue(TEXT("evaluation subsystem created"), testWorld.EvaluationSubsystem != nullptr);
	if (!testWorld.World || !testWorld.EvaluationSubsystem) return false;

	AActor* robotActor = testWorld.SpawnActor(FVector::ZeroVector);
	TestTrue(TEXT("robot actor spawned"), robotActor != nullptr);
	if (!robotActor) return false;

	TestTrue(
		TEXT("evaluation started"),
		testWorld.EvaluationSubsystem->StartEvaluation(MakeEvaluationTestConfig(), MakeEvaluationTestContext(robotActor), 0.1));
	testWorld.AdvanceWorldTime(0.25f);
	testWorld.EvaluationSubsystem->Tick(0.25f);

	const FEpisodeEvaluationResult result = testWorld.EvaluationSubsystem->GetCurrentResult();
	TestTrue(TEXT("timeout completes episode"), result.bCompleted);
	TestEqual(
		TEXT("timeout terminal reason"),
		static_cast<int32>(result.TerminalReason),
		static_cast<int32>(EEpisodeEvaluationTerminalReason::Timeout));

	const FEpisodeEvaluationEvent* event = FindEvaluationEvent(result, EEpisodeEvaluationEventType::Timeout);
	TestTrue(TEXT("timeout event recorded"), event != nullptr);
	if (!event) return false;

	HasFloatParam(*this, event->Properties, TEXT("duration_s"));
	HasFloatParam(*this, event->Properties, TEXT("max_duration_s"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEvaluationGroundRegionRuntimeTest,
	"OdiroSim.ScenarioEvaluation.Events.GroundRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEvaluationGroundRegionRuntimeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FScenarioEvaluationTestWorld blockedWorld;
	TestTrue(TEXT("blocked world created"), blockedWorld.World != nullptr);
	TestTrue(TEXT("blocked evaluation subsystem created"), blockedWorld.EvaluationSubsystem != nullptr);
	if (!blockedWorld.World || !blockedWorld.EvaluationSubsystem) return false;

	AActor* blockedRobot = blockedWorld.SpawnActor(FVector::ZeroVector);
	AScenarioGroundRegion* blockedRegion = SpawnEvaluationTestRegion(
		blockedWorld.World,
		TEXT("blocked_region"),
		EScenarioGroundRegionType::Blocked,
		FVector::ZeroVector);
	TestTrue(TEXT("blocked robot spawned"), blockedRobot != nullptr);
	TestTrue(TEXT("blocked region spawned"), blockedRegion != nullptr);
	if (!blockedRobot || !blockedRegion) return false;

	FScenarioRuntimeContext blockedContext = MakeEvaluationTestContext(blockedRobot);
	blockedContext.GroundRegionActors.Add(blockedRegion);
	TestTrue(
		TEXT("blocked evaluation started"),
		blockedWorld.EvaluationSubsystem->StartEvaluation(MakeEvaluationTestConfig(), blockedContext, 0.0));
	blockedWorld.EvaluationSubsystem->Tick(0.0f);

	const FEpisodeEvaluationResult blockedResult = blockedWorld.EvaluationSubsystem->GetCurrentResult();
	const FEpisodeEvaluationEvent* blockedEvent = FindEvaluationEvent(
		blockedResult,
		EEpisodeEvaluationEventType::BlockedRegionCollision);
	TestTrue(TEXT("blocked region event recorded"), blockedEvent != nullptr);
	if (!blockedEvent) return false;
	HasStringParam(*this, blockedEvent->Properties, TEXT("region_id"));

	FScenarioEvaluationTestWorld penaltyWorld;
	TestTrue(TEXT("penalty world created"), penaltyWorld.World != nullptr);
	TestTrue(TEXT("penalty evaluation subsystem created"), penaltyWorld.EvaluationSubsystem != nullptr);
	if (!penaltyWorld.World || !penaltyWorld.EvaluationSubsystem) return false;

	AActor* penaltyRobot = penaltyWorld.SpawnActor(FVector::ZeroVector);
	AScenarioGroundRegion* penaltyRegion = SpawnEvaluationTestRegion(
		penaltyWorld.World,
		TEXT("penalty_region"),
		EScenarioGroundRegionType::Penalty,
		FVector::ZeroVector);
	TestTrue(TEXT("penalty robot spawned"), penaltyRobot != nullptr);
	TestTrue(TEXT("penalty region spawned"), penaltyRegion != nullptr);
	if (!penaltyRobot || !penaltyRegion) return false;

	FScenarioRuntimeContext penaltyContext = MakeEvaluationTestContext(penaltyRobot);
	penaltyContext.GroundRegionActors.Add(penaltyRegion);
	TestTrue(
		TEXT("penalty evaluation started"),
		penaltyWorld.EvaluationSubsystem->StartEvaluation(MakeEvaluationTestConfig(), penaltyContext, 0.0));
	penaltyWorld.EvaluationSubsystem->Tick(0.0f);

	const FEpisodeEvaluationResult penaltyResult = penaltyWorld.EvaluationSubsystem->GetCurrentResult();
	const FEpisodeEvaluationEvent* penaltyEvent = FindEvaluationEvent(
		penaltyResult,
		EEpisodeEvaluationEventType::PenaltyRegionViolation);
	TestTrue(TEXT("penalty region event recorded"), penaltyEvent != nullptr);
	if (!penaltyEvent) return false;
	HasStringParam(*this, penaltyEvent->Properties, TEXT("region_id"));
	HasFloatParam(*this, penaltyEvent->Properties, TEXT("start_time_s"));
	HasFloatParam(*this, penaltyEvent->Properties, TEXT("duration_s"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioEvaluationPedestrianNearMissRuntimeTest,
	"OdiroSim.ScenarioEvaluation.Events.PedestrianNearMiss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioEvaluationPedestrianNearMissRuntimeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FScenarioEvaluationTestWorld testWorld;
	TestTrue(TEXT("world created"), testWorld.World != nullptr);
	TestTrue(TEXT("evaluation subsystem created"), testWorld.EvaluationSubsystem != nullptr);
	if (!testWorld.World || !testWorld.EvaluationSubsystem) return false;

	AActor* robotActor = testWorld.SpawnActor(FVector::ZeroVector);
	AActor* pedestrianActor = testWorld.SpawnActor(FVector(25.0, 0.0, 0.0));
	TestTrue(TEXT("robot actor spawned"), robotActor != nullptr);
	TestTrue(TEXT("pedestrian actor spawned"), pedestrianActor != nullptr);
	if (!robotActor || !pedestrianActor) return false;

	FScenarioRuntimeContext context = MakeEvaluationTestContext(robotActor);
	context.PedestrianActors.Add(pedestrianActor);
	context.PedestrianInstanceIds.Add(TEXT("pedestrian_001"));

	TestTrue(
		TEXT("evaluation started"),
		testWorld.EvaluationSubsystem->StartEvaluation(MakeEvaluationTestConfig(), context, 0.0));
	testWorld.EvaluationSubsystem->Tick(0.0f);

	FEpisodeEvaluationResult requestedResult;
	requestedResult.EpisodeId = context.EpisodeId;
	requestedResult.bSuccess = false;
	requestedResult.Outcome = EEpisodeEvaluationOutcome::Cancelled;
	requestedResult.TerminalReason = EEpisodeEvaluationTerminalReason::Cancelled;
	testWorld.EvaluationSubsystem->RequestEndEpisode(requestedResult);

	const FEpisodeEvaluationResult result = testWorld.EvaluationSubsystem->GetCurrentResult();
	const FEpisodeEvaluationEvent* event = FindEvaluationEvent(result, EEpisodeEvaluationEventType::PedestrianNearMiss);
	TestTrue(TEXT("near-miss event recorded"), event != nullptr);
	if (!event) return false;

	HasStringParam(*this, event->Properties, TEXT("target_id"));
	HasFloatParam(*this, event->Properties, TEXT("min_distance_m"));
	HasFloatParam(*this, event->Properties, TEXT("threshold_m"));
	return true;
}

#endif
