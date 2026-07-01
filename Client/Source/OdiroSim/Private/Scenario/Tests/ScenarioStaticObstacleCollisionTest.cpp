#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Actors/ScenarioStaticObstacle.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"

namespace
{
	// Owns an isolated world used by static obstacle collision source tests.
	struct FScenarioStaticObstacleCollisionTestWorld
	{
		// Transient world that receives one obstacle actor during a test.
		UWorld* World = nullptr;

		// Creates a minimal world so component collision state and placement queries can be exercised without loading a map.
		FScenarioStaticObstacleCollisionTestWorld()
		{
			static int32 NextWorldIndex = 0;
			const FName WorldName(*FString::Printf(
				TEXT("ScenarioStaticObstacleCollisionTestWorld_%d"),
				++NextWorldIndex));

			UWorld::InitializationValues InitValues;
			InitValues.AllowAudioPlayback(false)
				.CreatePhysicsScene(true)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false)
				.CreateFXSystem(false);

			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				WorldName,
				GetTransientPackage(),
				false,
				ERHIFeatureLevel::Num,
				&InitValues);
			if (GEngine && World)
			{
				FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
				WorldContext.SetCurrentWorld(World);
			}
		}

		// Destroys the isolated world after spawned component ownership has been released.
		~FScenarioStaticObstacleCollisionTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
			}
		}
	};

	// Loads the engine cube mesh used as a stable simple-collision test fixture.
	UStaticMesh* LoadBasicCubeMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	// Returns true when a mesh has simple collision data available to query components.
	bool MeshHasSimpleCollision(UStaticMesh* Mesh)
	{
		if (!Mesh)
		{
			return false;
		}

		const UBodySetup* BodySetup = Mesh->GetBodySetup();
		return BodySetup && BodySetup->AggGeom.GetElementCount() > 0;
	}

	// Adds an extra static mesh component that mimics a mesh nested inside a Blueprint obstacle.
	UStaticMeshComponent* AddObstacleStaticMeshComponent(
		AScenarioStaticObstacle* Obstacle,
		UStaticMesh* Mesh,
		FName ComponentName)
	{
		if (!Obstacle)
		{
			return nullptr;
		}

		UStaticMeshComponent* StaticMeshComponent =
			NewObject<UStaticMeshComponent>(Obstacle, ComponentName);
		Obstacle->AddInstanceComponent(StaticMeshComponent);
		StaticMeshComponent->SetupAttachment(Obstacle->SceneRoot);
		StaticMeshComponent->SetStaticMesh(Mesh);
		StaticMeshComponent->RegisterComponent();
		return StaticMeshComponent;
	}

	// Adds a non-static-mesh primitive to ensure mesh source mode does not enable arbitrary primitives.
	UBoxComponent* AddObstacleBoxComponent(AScenarioStaticObstacle* Obstacle, FName ComponentName)
	{
		if (!Obstacle)
		{
			return nullptr;
		}

		UBoxComponent* BoxComponent = NewObject<UBoxComponent>(Obstacle, ComponentName);
		Obstacle->AddInstanceComponent(BoxComponent);
		BoxComponent->SetupAttachment(Obstacle->SceneRoot);
		BoxComponent->RegisterComponent();
		return BoxComponent;
	}

	// Creates a catalog entry with authored bounds so source-mode priority is observable.
	FScenarioStaticObstaclePropEntry MakeCollisionSourceTestEntry(
		UStaticMesh* Mesh,
		EScenarioStaticObstacleCollisionSourceMode CollisionSourceMode)
	{
		FScenarioStaticObstaclePropEntry Entry;
		Entry.PropId = TEXT("test.static_obstacle_collision_source");
		Entry.SemanticTypeId = TEXT("test_obstacle");
		Entry.DisplayName = FText::FromString(TEXT("Static obstacle collision source"));
		Entry.Category = EScenarioStaticObstaclePropCategory::StreetFurniture;
		Entry.ObstacleActorClass = AScenarioStaticObstacle::StaticClass();
		Entry.StaticMeshAsset = Mesh;
		Entry.BoundsSizeMeters = FVector(2.0, 2.0, 2.0);
		Entry.BoundsCenterOffsetMeters = FVector(0.0, 0.0, 1.0);
		Entry.FallbackBoxExtent = FVector(50.0, 50.0, 50.0);
		Entry.bUsePhysicalCollision = true;
		Entry.bUseSafetyQuery = true;
		Entry.CollisionSourceMode = CollisionSourceMode;
		Entry.bUseMeshSimpleCollision = false;
		Entry.bUseFallbackBoxCollision = true;
		return Entry;
	}

	// Creates a transient catalog with intentionally oversized authored bounds for placement-query tests.
	UScenarioStaticObstaclePropCatalog* MakePlacementQueryTestCatalog(
		UObject* Outer,
		UStaticMesh* Mesh)
	{
		UScenarioStaticObstaclePropCatalog* Catalog =
			NewObject<UScenarioStaticObstaclePropCatalog>(Outer, TEXT("ScenarioStaticObstaclePlacementQueryCatalog"));
		if (!Catalog)
		{
			return nullptr;
		}

		FScenarioStaticObstaclePropEntry Entry = MakeCollisionSourceTestEntry(
			Mesh,
			EScenarioStaticObstacleCollisionSourceMode::MeshSimpleCollision);
		Entry.PropId = TEXT("test.mesh_source_large_bounds");
		Entry.BoundsSizeMeters = FVector(10.0, 10.0, 2.0);
		Entry.BoundsCenterOffsetMeters = FVector(0.0, 0.0, 1.0);
		Entry.SafetyRadius = 0.0;
		Catalog->Entries.Add(Entry);
		return Catalog;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioStaticObstacleMeshSimpleCollisionSourceTest,
	"OdiroSim.Scenario.StaticObstacle.MeshSimpleCollisionSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioStaticObstacleMeshSimpleCollisionSourceTest::RunTest(const FString& Parameters)
{
	FScenarioStaticObstacleCollisionTestWorld TestWorld;
	TestNotNull(TEXT("test world"), TestWorld.World);
	if (!TestWorld.World)
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadBasicCubeMesh();
	TestNotNull(TEXT("basic cube mesh"), CubeMesh);
	if (!CubeMesh)
	{
		return false;
	}

	if (!MeshHasSimpleCollision(CubeMesh))
	{
		AddWarning(TEXT("Skipping static obstacle mesh-source collision test because the engine cube has no simple collision."));
		return true;
	}

	AScenarioStaticObstacle* Obstacle = TestWorld.World->SpawnActor<AScenarioStaticObstacle>(
		AScenarioStaticObstacle::StaticClass(),
		FTransform::Identity);
	TestNotNull(TEXT("static obstacle actor"), Obstacle);
	if (!Obstacle)
	{
		return false;
	}

	UStaticMeshComponent* ExtraStaticMeshComponent = AddObstacleStaticMeshComponent(
		Obstacle,
		CubeMesh,
		TEXT("CollisionSourceExtraStaticMesh"));
	TestNotNull(TEXT("extra static mesh component"), ExtraStaticMeshComponent);

	UBoxComponent* ExtraBoxComponent = AddObstacleBoxComponent(
		Obstacle,
		TEXT("CollisionSourceExtraBox"));
	TestNotNull(TEXT("extra box component"), ExtraBoxComponent);

	const FScenarioStaticObstaclePropEntry Entry = MakeCollisionSourceTestEntry(
		CubeMesh,
		EScenarioStaticObstacleCollisionSourceMode::MeshSimpleCollision);
	TestTrue(TEXT("prop entry applies"), Obstacle->ApplyPropEntry(Entry));

	TestEqual(
		TEXT("MeshRoot collision source"),
		static_cast<int32>(Obstacle->MeshRoot->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::QueryAndPhysics));
	TestEqual(
		TEXT("extra static mesh collision source"),
		static_cast<int32>(ExtraStaticMeshComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::QueryAndPhysics));
	TestEqual(
		TEXT("authored bounds disabled for mesh source mode"),
		static_cast<int32>(Obstacle->CollisionBoundsComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::NoCollision));
	TestEqual(
		TEXT("non-static-mesh primitive disabled for mesh source mode"),
		static_cast<int32>(ExtraBoxComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::NoCollision));
	TestTrue(
		TEXT("object type tag applied"),
		Obstacle->Tags.Contains(FName(TEXT("ObjectType.test_obstacle"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioStaticObstacleAutoCollisionSourceTest,
	"OdiroSim.Scenario.StaticObstacle.AutoCollisionSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioStaticObstacleAutoCollisionSourceTest::RunTest(const FString& Parameters)
{
	FScenarioStaticObstacleCollisionTestWorld TestWorld;
	TestNotNull(TEXT("test world"), TestWorld.World);
	if (!TestWorld.World)
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadBasicCubeMesh();
	TestNotNull(TEXT("basic cube mesh"), CubeMesh);
	if (!CubeMesh)
	{
		return false;
	}

	AScenarioStaticObstacle* Obstacle = TestWorld.World->SpawnActor<AScenarioStaticObstacle>(
		AScenarioStaticObstacle::StaticClass(),
		FTransform::Identity);
	TestNotNull(TEXT("static obstacle actor"), Obstacle);
	if (!Obstacle)
	{
		return false;
	}

	const FScenarioStaticObstaclePropEntry Entry = MakeCollisionSourceTestEntry(
		CubeMesh,
		EScenarioStaticObstacleCollisionSourceMode::Auto);
	TestTrue(TEXT("prop entry applies"), Obstacle->ApplyPropEntry(Entry));

	TestEqual(
		TEXT("Auto keeps authored bounds as canonical collision"),
		static_cast<int32>(Obstacle->CollisionBoundsComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::QueryAndPhysics));
	TestEqual(
		TEXT("Auto disables MeshRoot when authored bounds exist"),
		static_cast<int32>(Obstacle->MeshRoot->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::NoCollision));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioStaticObstaclePlacementCollisionQueryTest,
	"OdiroSim.Scenario.StaticObstacle.PlacementCollisionQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioStaticObstaclePlacementCollisionQueryTest::RunTest(const FString& Parameters)
{
	FScenarioStaticObstacleCollisionTestWorld TestWorld;
	TestNotNull(TEXT("test world"), TestWorld.World);
	if (!TestWorld.World)
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadBasicCubeMesh();
	TestNotNull(TEXT("basic cube mesh"), CubeMesh);
	if (!CubeMesh)
	{
		return false;
	}

	if (!MeshHasSimpleCollision(CubeMesh))
	{
		AddWarning(TEXT("Skipping static obstacle placement query test because the engine cube has no simple collision."));
		return true;
	}

	UScenarioAuthoringSubsystem* AuthoringSubsystem =
		TestWorld.World->GetSubsystem<UScenarioAuthoringSubsystem>();
	TestNotNull(TEXT("authoring subsystem"), AuthoringSubsystem);
	if (!AuthoringSubsystem)
	{
		return false;
	}

	UScenarioStaticObstaclePropCatalog* TestCatalog =
		MakePlacementQueryTestCatalog(GetTransientPackage(), CubeMesh);
	TestNotNull(TEXT("test static obstacle catalog"), TestCatalog);
	if (!TestCatalog)
	{
		return false;
	}

	AuthoringSubsystem->StaticObstaclePropCatalog = TestCatalog;
	AuthoringSubsystem->StaticObstacleFootprintClearanceCm = 0.0;
	AuthoringSubsystem->StaticObstacleGroundZToleranceCm = 200.0;

	const FName PropId(TEXT("test.mesh_source_large_bounds"));
	FScenarioPlaceableInstanceSpec FirstSpec;
	TestTrue(
		TEXT("first mesh-source obstacle is placed"),
		AuthoringSubsystem->AddStaticObstacle(
			PropId,
			FTransform(FRotator::ZeroRotator, FVector::ZeroVector),
			FirstSpec));

	FString FailureReason;
	TestTrue(
		TEXT("placement uses simple collision instead of oversized authored bounds"),
		AuthoringSubsystem->CanPlaceStaticObstacle(
			PropId,
			FTransform(FRotator::ZeroRotator, FVector(400.0, 0.0, 0.0)),
			FailureReason));
	TestEqual(TEXT("non-overlap failure reason"), FailureReason, FString());

	TestFalse(
		TEXT("placement still rejects actual simple-collision overlap"),
		AuthoringSubsystem->CanPlaceStaticObstacle(
			PropId,
			FTransform(FRotator::ZeroRotator, FVector(75.0, 0.0, 0.0)),
			FailureReason));
	TestTrue(
		TEXT("overlap failure identifies the existing obstacle"),
		FailureReason.Contains(FirstSpec.InstanceId));

	return true;
}

#endif
