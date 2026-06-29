#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioCityBlockMaterializer.h"

#include "Components/SplineMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/ScenarioCorridorGeometry.h"

namespace
{
	// Owns the transient world used by city-block materializer automation tests.
	struct FScenarioCityBlockMaterializerTestWorld
	{
		// Transient world that receives visual-only block actors during one test.
		UWorld* World = nullptr;

		// Creates an isolated world so the materializer can exercise SpawnActor without loading maps.
		FScenarioCityBlockMaterializerTestWorld()
		{
			static int32 nextWorldIndex = 0;
			const FName worldName(*FString::Printf(
				TEXT("ScenarioCityBlockMaterializerTestWorld_%d"),
				++nextWorldIndex));

			UWorld::InitializationValues initValues;
			initValues.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false)
				.CreateFXSystem(false);

			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				worldName,
				GetTransientPackage(),
				false,
				ERHIFeatureLevel::Num,
				&initValues);
		}

		// Destroys the isolated world after spawned actor ownership has been released.
		~FScenarioCityBlockMaterializerTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}
	};

	// Creates a road-side generated GroundRegion with enough metadata to exercise catalog matching.
	FScenarioGroundRegionSpec MakeGeneratedRoadSideRegion(
		const FString& regionId,
		EScenarioGroundRegionType regionType,
		const FVector& center,
		const FVector2D& size,
		const FString& collisionTag = FString(),
		const FString& penaltyKind = FString(),
		double yawDegrees = 0.0)
	{
		FScenarioGroundRegionSpec regionSpec;
		regionSpec.RegionId = regionId;
		regionSpec.RegionType = regionType;
		regionSpec.SurfaceId = TEXT("road");
		regionSpec.ShapeType = EScenarioGroundShapeType::Rectangle;
		regionSpec.Center = center;
		regionSpec.Size = size;
		regionSpec.YawDegrees = yawDegrees;
		regionSpec.CollisionTag = collisionTag;
		regionSpec.PenaltyKind = penaltyKind;
		return regionSpec;
	}

	// Creates a generated building-side GroundRegion used to exercise elevated visual block snapping.
	FScenarioGroundRegionSpec MakeGeneratedBuildingRegion(
		const FString& regionId,
		const FVector& center,
		const FVector2D& size,
		double yawDegrees = 0.0)
	{
		FScenarioGroundRegionSpec regionSpec;
		regionSpec.RegionId = regionId;
		regionSpec.RegionType = EScenarioGroundRegionType::Blocked;
		regionSpec.SurfaceId = TEXT("building");
		regionSpec.ShapeType = EScenarioGroundShapeType::Rectangle;
		regionSpec.Center = center;
		regionSpec.Size = size;
		regionSpec.YawDegrees = yawDegrees;
		regionSpec.CollisionTag = TEXT("building");
		return regionSpec;
	}

	// Creates a generated building-side walkway extension that should not trigger RoadStraight blocks.
	FScenarioGroundRegionSpec MakeGeneratedWalkwayExtensionRegion(
		const FString& regionId,
		const FVector& center,
		const FVector2D& size)
	{
		FScenarioGroundRegionSpec regionSpec;
		regionSpec.RegionId = regionId;
		regionSpec.RegionType = EScenarioGroundRegionType::Walkable;
		regionSpec.SurfaceId = TEXT("walkway");
		regionSpec.ShapeType = EScenarioGroundShapeType::Rectangle;
		regionSpec.Center = center;
		regionSpec.Size = size;
		regionSpec.YawDegrees = 0.0;
		return regionSpec;
	}

	// Creates a root-backed engine actor entry so tests do not depend on project content assets.
	FScenarioCityBlockCatalogEntry MakeMaterializerTestEntry(
		FName blockId,
		EScenarioCityBlockRole role,
		EScenarioGroundRegionType primaryRegionType,
		double widthMeters)
	{
		FScenarioCityBlockCatalogEntry blockEntry;
		blockEntry.BlockId = blockId;
		blockEntry.BPClass = AScenarioGroundRegion::StaticClass();
		blockEntry.Role = role;
		blockEntry.BoundsMeters.LengthMeters = 10.0;
		blockEntry.BoundsMeters.WidthMeters = widthMeters;
		blockEntry.BoundsMeters.HeightMeters = 1.0;
		blockEntry.SemanticProfile.SurfaceIds = { TEXT("road") };
		blockEntry.SemanticProfile.PrimaryRegionType = primaryRegionType;
		return blockEntry;
	}

	// Creates the RoadStraight entry used for generated curb+road road-side visuals.
	FScenarioCityBlockCatalogEntry MakeRoadSideCompositeTestEntry(FName blockId)
	{
		FScenarioCityBlockCatalogEntry blockEntry = MakeMaterializerTestEntry(
			blockId,
			EScenarioCityBlockRole::RoadStraight,
			EScenarioGroundRegionType::Penalty,
			FScenarioCorridorGeometry::GeneratedCityCurbWidthMeters
				+ FScenarioCorridorGeometry::GeneratedCityTwoLaneRoadWidthMeters);
		blockEntry.SemanticProfile.PenaltyKind = TEXT("road");
		blockEntry.PlacementProfile.LateralAnchor = EScenarioCityBlockLateralAnchor::RegionInnerEdge;
		return blockEntry;
	}

	// Returns the world-space start point for a generated RoadStraight spline mesh section.
	FVector GetSplineStartWorldCm(const AActor& ownerActor, const USplineMeshComponent& splineMeshComponent)
	{
		return ownerActor.GetActorLocation() + splineMeshComponent.GetStartPosition();
	}

	// Returns the world-space end point for a generated RoadStraight spline mesh section.
	FVector GetSplineEndWorldCm(const AActor& ownerActor, const USplineMeshComponent& splineMeshComponent)
	{
		return ownerActor.GetActorLocation() + splineMeshComponent.GetEndPosition();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeCoversRoadBand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerRoadCompositeTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadSideCompositeBlock = MakeRoadSideCompositeTestEntry(
		TEXT("city.road_straight_10m"));
	roadSideCompositeBlock.PlacementProfile.Priority = 10;
	catalog->Entries.Add(roadSideCompositeBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_curb_00_00"),
		EScenarioGroundRegionType::Blocked,
		FVector(0.0, 25.0, 0.0),
		FVector2D(2300.0, 50.0),
		TEXT("curb")));
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(0.0, 370.0, 0.0),
		FVector2D(2300.0, 640.0),
		FString(),
		TEXT("road")));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("only the generated road band is a materializer candidate"), result.CandidateRegionCount, 1);
	TestEqual(TEXT("one RoadStraight road-side composite candidate is configured"), result.RoadSideCompositeCandidateCount, 1);
	TestEqual(TEXT("RoadStraight composite stretches into one actor"), result.SpawnedActorCount, 1);
	TestEqual(TEXT("spawned actor is counted as a road-side composite"), result.SpawnedRoadSideCompositeCount, 1);
	TestEqual(TEXT("configured composite avoids road-side composite no-entry skips"), result.SkippedRoadSideCompositeNoEntryCount, 0);
	TestEqual(TEXT("configured entries avoid no-entry skips"), result.SkippedNoEntryCount, 0);
	TestEqual(TEXT("one visual actor remains owned by the materializer"), spawnedActors.Num(), 1);
	if (spawnedActors.Num() == 1 && spawnedActors[0])
	{
		TArray<USplineMeshComponent*> splineMeshComponents;
		spawnedActors[0]->GetComponents<USplineMeshComponent>(splineMeshComponents);
		TestEqual(TEXT("RoadStraight is represented by one spline mesh section"), splineMeshComponents.Num(), 1);
		TestTrue(
			TEXT("RoadStraight owner origin sits on the generated road span start"),
			FMath::IsNearlyEqual(spawnedActors[0]->GetActorLocation().X, -1150.0, 0.1));
		TestTrue(
			TEXT("RoadStraight owner origin stays on the RoadStraight width centerline"),
			FMath::IsNearlyEqual(spawnedActors[0]->GetActorLocation().Y, 345.0, 0.1));
		TestEqual(TEXT("road-side composite snaps to road height"), spawnedActors[0]->GetActorLocation().Z, 0.0);
		if (splineMeshComponents.Num() == 1 && splineMeshComponents[0])
		{
			const FVector startWorldCm = GetSplineStartWorldCm(*spawnedActors[0], *splineMeshComponents[0]);
			const FVector endWorldCm = GetSplineEndWorldCm(*spawnedActors[0], *splineMeshComponents[0]);
			TestTrue(
				TEXT("RoadStraight spline starts at the generated road span start"),
				startWorldCm.Equals(FVector(-1150.0, 345.0, 0.0), 0.1));
			TestTrue(
				TEXT("RoadStraight spline stretches to the generated road span end"),
				endWorldCm.Equals(FVector(1150.0, 345.0, 0.0), 0.1));
		}
	}

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeRequiresRoadStraightTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeRequiresRoadStraight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerRoadCompositeRequiresRoadStraightTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_curb_00_00"),
		EScenarioGroundRegionType::Blocked,
		FVector(0.0, 25.0, 0.0),
		FVector2D(1000.0, 50.0),
		TEXT("curb")));
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(0.0, 370.0, 0.0),
		FVector2D(1000.0, 640.0),
		FString(),
		TEXT("road")));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("only the generated road band is a materializer candidate"), result.CandidateRegionCount, 1);
	TestEqual(TEXT("no RoadStraight road-side composite candidate is configured"), result.RoadSideCompositeCandidateCount, 0);
	TestEqual(TEXT("no RoadStraight composite spawns without a catalog entry"), result.SpawnedActorCount, 0);
	TestEqual(TEXT("no road-side composite actor is spawned"), result.SpawnedRoadSideCompositeCount, 0);
	TestEqual(TEXT("missing RoadStraight composite is diagnosed"), result.SkippedRoadSideCompositeNoEntryCount, 1);
	TestEqual(TEXT("the road candidate lacks an entry"), result.SkippedNoEntryCount, 1);
	TestEqual(TEXT("no visual actors remain owned by the materializer"), spawnedActors.Num(), 0);

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeStartRootTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeSupportsStartRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeBlueprintTemplateTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeUsesBlueprintStaticMeshTemplate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerRoadCompositeBlueprintTemplateTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadSideCompositeBlock = MakeRoadSideCompositeTestEntry(TEXT("road"));
	roadSideCompositeBlock.BPClass = TSoftClassPtr<AActor>(FSoftObjectPath(
		TEXT("/Game/Templates/CityBuildings/Blueprints/BP_Road_Straight.BP_Road_Straight_C")));
	roadSideCompositeBlock.BoundsMeters.LengthMeters = 28.0;
	roadSideCompositeBlock.BoundsMeters.WidthMeters = 6.9;
	roadSideCompositeBlock.BoundsMeters.HeightMeters = 0.17;
	roadSideCompositeBlock.BoundsMeters.CenterOffsetMeters = FVector(14.0, 0.0, 0.0);
	catalog->Entries.Add(roadSideCompositeBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(3000.0, 4520.0, 0.0),
		FVector2D(6000.0, 640.0),
		FString(),
		TEXT("road")));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("actual RoadStraight BP road band is a materializer candidate"), result.CandidateRegionCount, 1);
	TestEqual(TEXT("actual RoadStraight BP spline actor spawns"), result.SpawnedActorCount, 1);
	TestEqual(TEXT("actual RoadStraight BP does not fail mesh template lookup"), result.SkippedSpawnFailureCount, 0);
	TestEqual(TEXT("actual RoadStraight BP counts as a road-side composite"), result.SpawnedRoadSideCompositeCount, 1);
	TestEqual(TEXT("one actual RoadStraight spline actor remains owned"), spawnedActors.Num(), 1);
	if (spawnedActors.Num() == 1 && spawnedActors[0])
	{
		TArray<USplineMeshComponent*> splineMeshComponents;
		spawnedActors[0]->GetComponents<USplineMeshComponent>(splineMeshComponents);
		TestEqual(TEXT("actual RoadStraight BP supplies one spline mesh section"), splineMeshComponents.Num(), 1);
		if (splineMeshComponents.Num() == 1 && splineMeshComponents[0])
		{
			TestTrue(
				TEXT("actual RoadStraight BP static mesh template is assigned to the spline section"),
				splineMeshComponents[0]->GetStaticMesh() != nullptr);
		}
	}

	spawnedActors.Reset();
	return true;
}

bool FScenarioCityBlockMaterializerRoadCompositeStartRootTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadSideCompositeBlock = MakeRoadSideCompositeTestEntry(
		TEXT("city.road_straight_10m"));
	catalog->Entries.Add(roadSideCompositeBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(0.0, 370.0, 0.0),
		FVector2D(1000.0, 640.0),
		FString(),
		TEXT("road")));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("RoadStraight composite spawns once"), result.SpawnedActorCount, 1);
	TestEqual(TEXT("one start-rooted actor remains owned by the materializer"), spawnedActors.Num(), 1);
	if (spawnedActors.Num() == 1 && spawnedActors[0])
	{
		TArray<USplineMeshComponent*> splineMeshComponents;
		spawnedActors[0]->GetComponents<USplineMeshComponent>(splineMeshComponents);
		TestEqual(TEXT("start-rooted RoadStraight uses one spline mesh section"), splineMeshComponents.Num(), 1);
		TestTrue(
			TEXT("BP origin can sit on the generated road span start"),
			FMath::IsNearlyEqual(spawnedActors[0]->GetActorLocation().X, -500.0, 0.1));
		TestTrue(
			TEXT("BP origin stays on the RoadStraight width centerline"),
			FMath::IsNearlyEqual(spawnedActors[0]->GetActorLocation().Y, 345.0, 0.1));
		TestEqual(TEXT("start-rooted composite still snaps to road height"), spawnedActors[0]->GetActorLocation().Z, 0.0);
		if (splineMeshComponents.Num() == 1 && splineMeshComponents[0])
		{
			const FVector endWorldCm = GetSplineEndWorldCm(*spawnedActors[0], *splineMeshComponents[0]);
			TestTrue(
				TEXT("start-rooted RoadStraight spline stretches to the segment end"),
				endWorldCm.Equals(FVector(500.0, 345.0, 0.0), 0.1));
		}
	}

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCornerTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadSideRightAngleCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeChainsSegmentsTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeChainsConnectedSegments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerRoadCompositeChainsSegmentsTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadSideCompositeBlock = MakeRoadSideCompositeTestEntry(
		TEXT("city.road_straight_10m"));
	catalog->Entries.Add(roadSideCompositeBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_a_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(500.0, 370.0, 0.0),
		FVector2D(1000.0, 640.0),
		FString(),
		TEXT("road")));
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_b_upper_road_2lane_01_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(1500.0, 370.0, 0.0),
		FVector2D(1000.0, 640.0),
		FString(),
		TEXT("road")));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("two generated road bands are materializer candidates"), result.CandidateRegionCount, 2);
	TestEqual(TEXT("two RoadStraight road-side composite candidates are configured"), result.RoadSideCompositeCandidateCount, 2);
	TestEqual(TEXT("connected RoadStraight regions share one owner actor"), result.SpawnedActorCount, 1);
	TestEqual(TEXT("one road-side spline actor owns the connected sections"), result.SpawnedRoadSideCompositeCount, 1);
	TestEqual(TEXT("one visual actor remains owned by the materializer"), spawnedActors.Num(), 1);
	if (spawnedActors.Num() == 1 && spawnedActors[0])
	{
		TArray<USplineMeshComponent*> splineMeshComponents;
		spawnedActors[0]->GetComponents<USplineMeshComponent>(splineMeshComponents);
		TestEqual(TEXT("connected regions remain separate spline mesh sections under one actor"), splineMeshComponents.Num(), 2);
		TestTrue(
			TEXT("shared RoadStraight owner starts at the first segment start"),
			spawnedActors[0]->GetActorLocation().Equals(FVector(0.0, 345.0, 0.0), 0.1));
	}

	spawnedActors.Reset();
	return true;
}

bool FScenarioCityBlockMaterializerRoadCornerTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadSideCompositeBlock = MakeRoadSideCompositeTestEntry(
		TEXT("city.road_straight_10m"));
	roadSideCompositeBlock.PlacementProfile.Priority = 10;
	catalog->Entries.Add(roadSideCompositeBlock);

	FScenarioCityBlockCatalogEntry cornerBlock = MakeMaterializerTestEntry(
		TEXT("city.walkway_road_corner_10m"),
		EScenarioCityBlockRole::Corner,
		EScenarioGroundRegionType::Penalty,
		10.0);
	cornerBlock.BoundsMeters.LengthMeters = 8.0;
	cornerBlock.BoundsMeters.CenterOffsetMeters = FVector(4.0, 0.0, 0.0);
	cornerBlock.SemanticProfile.SurfaceIds = { TEXT("road"), TEXT("walkway") };
	cornerBlock.PlacementProfile.Priority = 20;
	catalog->Entries.Add(cornerBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_a_upper_curb_00_00"),
		EScenarioGroundRegionType::Blocked,
		FVector(1000.0, 25.0, 0.0),
		FVector2D(2000.0, 50.0),
		TEXT("curb")));
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_a_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(1000.0, 370.0, 0.0),
		FVector2D(2000.0, 640.0),
		FString(),
		TEXT("road")));
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_b_upper_curb_00_00"),
		EScenarioGroundRegionType::Blocked,
		FVector(2025.0, -1000.0, 0.0),
		FVector2D(2000.0, 50.0),
		TEXT("curb"),
		FString(),
		-90.0));
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_b_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(2370.0, -1000.0, 0.0),
		FVector2D(2000.0, 640.0),
		FString(),
		TEXT("road"),
		-90.0));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("two generated road bands are materializer candidates"), result.CandidateRegionCount, 2);
	TestEqual(TEXT("one road-side corner is inferred"), result.CornerCandidateCount, 1);
	TestEqual(TEXT("two RoadStraight road-side composite candidates are configured"), result.RoadSideCompositeCandidateCount, 2);
	TestEqual(TEXT("straight composites and the corner actor spawn"), result.SpawnedActorCount, 3);
	TestEqual(TEXT("two road-side straight composites spawn"), result.SpawnedRoadSideCompositeCount, 2);
	TestEqual(TEXT("configured corner entry avoids corner no-entry skips"), result.SkippedCornerNoEntryCount, 0);
	TestEqual(TEXT("three visual actors remain owned by the materializer"), spawnedActors.Num(), 3);

	bool bFoundHorizontalStraight = false;
	bool bFoundVerticalStraight = false;
	bool bFoundCornerAnchor = false;
	int32 roadSplineSectionCount = 0;
	for (const TObjectPtr<AActor>& spawnedActor : spawnedActors)
	{
		if (!spawnedActor)
		{
			continue;
		}

		const FVector location = spawnedActor->GetActorLocation();
		TArray<USplineMeshComponent*> splineMeshComponents;
		spawnedActor->GetComponents<USplineMeshComponent>(splineMeshComponents);
		roadSplineSectionCount += splineMeshComponents.Num();
		if (splineMeshComponents.IsEmpty())
		{
			bFoundCornerAnchor |= FMath::IsNearlyEqual(location.X, 2000.0, 0.1)
				&& FMath::IsNearlyEqual(location.Y, 0.0, 0.1);
			continue;
		}

		for (const USplineMeshComponent* splineMeshComponent : splineMeshComponents)
		{
			if (!splineMeshComponent)
			{
				continue;
			}

			const FVector startWorldCm = GetSplineStartWorldCm(*spawnedActor, *splineMeshComponent);
			const FVector endWorldCm = GetSplineEndWorldCm(*spawnedActor, *splineMeshComponent);
			bFoundHorizontalStraight |= FMath::IsNearlyEqual(startWorldCm.Y, 345.0, 0.1)
				&& FMath::IsNearlyEqual(endWorldCm.Y, 345.0, 0.1);
			bFoundVerticalStraight |= FMath::IsNearlyEqual(startWorldCm.X, 2345.0, 0.1)
				&& FMath::IsNearlyEqual(endWorldCm.X, 2345.0, 0.1);
		}
	}

	TestTrue(TEXT("horizontal straight composite starts at the corner mesh edge"), bFoundHorizontalStraight);
	TestTrue(TEXT("vertical straight composite starts at the corner mesh edge"), bFoundVerticalStraight);
	TestEqual(TEXT("two road-side straight spline sections spawn"), roadSplineSectionCount, 2);
	TestTrue(TEXT("corner actor is anchored at the continuous walkway-curb seam intersection"), bFoundCornerAnchor);

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerBuildingFrontageTest,
	"OdiroSim.Scenario.CityBlockMaterializer.BuildingFrontageUsesBoundsAndInnerEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerBuildingFrontageTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry buildingBlock = MakeMaterializerTestEntry(
		TEXT("city.building_frontage_18m"),
		EScenarioCityBlockRole::Building,
		EScenarioGroundRegionType::Blocked,
		22.0);
	buildingBlock.BoundsMeters.LengthMeters = 18.0;
	buildingBlock.SemanticProfile.SurfaceIds = { TEXT("building") };
	buildingBlock.SemanticProfile.CollisionTag = TEXT("building");
	catalog->Entries.Add(buildingBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedBuildingRegion(
		TEXT("generated_city_main_upper_building_00_00"),
		FVector(0.0, 1000.0, 0.0),
		FVector2D(6000.0, 1000.0)));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("building band is a materializer candidate"), result.CandidateRegionCount, 1);
	TestEqual(TEXT("building frontage uses floor fit based on authored bounds length"), result.SpawnedActorCount, 3);
	TestEqual(TEXT("three building actors remain owned by the materializer"), spawnedActors.Num(), 3);

	bool bFoundFirst = false;
	bool bFoundSecond = false;
	bool bFoundThird = false;
	for (const TObjectPtr<AActor>& spawnedActor : spawnedActors)
	{
		if (!spawnedActor)
		{
			continue;
		}

		const FVector location = spawnedActor->GetActorLocation();
		bFoundFirst |= FMath::IsNearlyEqual(location.X, -1800.0, 0.1)
			&& FMath::IsNearlyEqual(location.Y, 1600.0, 0.1);
		bFoundSecond |= FMath::IsNearlyEqual(location.X, 0.0, 0.1)
			&& FMath::IsNearlyEqual(location.Y, 1600.0, 0.1);
		bFoundThird |= FMath::IsNearlyEqual(location.X, 1800.0, 0.1)
			&& FMath::IsNearlyEqual(location.Y, 1600.0, 0.1);
		TestTrue(
			TEXT("building center is placed outside the generated building inner edge"),
			location.Y >= 1600.0 - 0.1);
	}

	TestTrue(TEXT("first bounds-fitted building frontage actor is placed"), bFoundFirst);
	TestTrue(TEXT("second bounds-fitted building frontage actor is placed"), bFoundSecond);
	TestTrue(TEXT("third bounds-fitted building frontage actor is placed"), bFoundThird);

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerBuildingFrontageOverlapTest,
	"OdiroSim.Scenario.CityBlockMaterializer.BuildingFrontageSkipsOverlappingSegments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerBuildingFrontageOverlapTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry buildingBlock = MakeMaterializerTestEntry(
		TEXT("city.building_frontage_18m"),
		EScenarioCityBlockRole::Building,
		EScenarioGroundRegionType::Blocked,
		22.0);
	buildingBlock.BoundsMeters.LengthMeters = 18.0;
	buildingBlock.SemanticProfile.SurfaceIds = { TEXT("building") };
	buildingBlock.SemanticProfile.CollisionTag = TEXT("building");
	catalog->Entries.Add(buildingBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedBuildingRegion(
		TEXT("generated_city_main_upper_building_00_00"),
		FVector(0.0, 1000.0, 0.0),
		FVector2D(6000.0, 1000.0)));
	groundRegions.Add(MakeGeneratedBuildingRegion(
		TEXT("generated_city_main_upper_building_01_00"),
		FVector(1000.0, 1600.0, 0.0),
		FVector2D(6000.0, 1000.0),
		-90.0));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("two building bands are materializer candidates"), result.CandidateRegionCount, 2);
	TestEqual(TEXT("overlapping second-segment building frontages are skipped"), result.SkippedBuildingOverlapCount, 3);
	TestEqual(TEXT("overlap skips are not spawn failures"), result.SkippedSpawnFailureCount, 0);
	TestEqual(TEXT("only the first non-overlapping frontage row spawns"), result.SpawnedActorCount, 3);
	TestEqual(TEXT("three building actors remain owned by the materializer"), spawnedActors.Num(), 3);

	for (const TObjectPtr<AActor>& spawnedActor : spawnedActors)
	{
		if (!spawnedActor)
		{
			continue;
		}

		const FVector location = spawnedActor->GetActorLocation();
		TestTrue(
			TEXT("accepted building actor remains on the first frontage row"),
			FMath::IsNearlyEqual(location.Y, 1600.0, 0.1));
	}

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeSkipsWalkwayExtensionTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeSkipsWalkwayExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerRoadCompositeSkipsWalkwayExtensionTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadSideCompositeBlock = MakeRoadSideCompositeTestEntry(
		TEXT("city.road_straight_10m"));
	catalog->Entries.Add(roadSideCompositeBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedWalkwayExtensionRegion(
		TEXT("generated_city_main_upper_walkway_extension_00_00"),
		FVector(0.0, 250.0, FScenarioCorridorGeometry::DefaultSurfaceTopZCm),
		FVector2D(1000.0, 500.0)));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("walkway extension is a materializer candidate"), result.CandidateRegionCount, 1);
	TestEqual(TEXT("RoadStraight composite does not spawn for building-side walkway extension"), result.SpawnedActorCount, 0);
	TestEqual(TEXT("walkway extension without a walkway-building entry is skipped as no entry"), result.SkippedNoEntryCount, 1);
	TestEqual(TEXT("no visual actors remain owned by the materializer"), spawnedActors.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerSurfaceHeightTest,
	"OdiroSim.Scenario.CityBlockMaterializer.SurfaceHeightSnap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerSurfaceHeightTest::RunTest(const FString& Parameters)
{
	FScenarioCityBlockMaterializerTestWorld testWorld;
	TestNotNull(TEXT("Transient test world is available"), testWorld.World);
	if (!testWorld.World)
	{
		return false;
	}

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for materializer tests"), catalog);
	if (!catalog)
	{
		return false;
	}

	FScenarioCityBlockCatalogEntry roadBlock = MakeMaterializerTestEntry(
		TEXT("city.road_straight_10m"),
		EScenarioCityBlockRole::RoadStraight,
		EScenarioGroundRegionType::Penalty,
		6.4);
	roadBlock.SemanticProfile.PenaltyKind = TEXT("road");
	catalog->Entries.Add(roadBlock);

	FScenarioCityBlockCatalogEntry buildingBlock = MakeMaterializerTestEntry(
		TEXT("city.building_straight_10m"),
		EScenarioCityBlockRole::Building,
		EScenarioGroundRegionType::Blocked,
		10.0);
	buildingBlock.SemanticProfile.SurfaceIds = { TEXT("building") };
	buildingBlock.SemanticProfile.CollisionTag = TEXT("building");
	catalog->Entries.Add(buildingBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_road_2lane_00_00"),
		EScenarioGroundRegionType::Penalty,
		FVector(0.0, 0.0, 123.0),
		FVector2D(1000.0, 640.0),
		FString(),
		TEXT("road")));
	groundRegions.Add(MakeGeneratedBuildingRegion(
		TEXT("generated_city_main_upper_building_00_00"),
		FVector(0.0, 1000.0, 123.0),
		FVector2D(1000.0, 1000.0)));

	TArray<TObjectPtr<AActor>> spawnedActors;
	FScenarioCityBlockMaterializationOptions options;
	options.LogContext = TEXT("ScenarioCityBlockMaterializerTest");
	const FScenarioCityBlockMaterializationResult result =
		FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
			testWorld.World,
			catalog,
			groundRegions,
			spawnedActors,
			options);

	TestEqual(TEXT("road and building bands are materializer candidates"), result.CandidateRegionCount, 2);
	TestEqual(TEXT("road and building blocks spawn"), result.SpawnedActorCount, 2);
	TestEqual(TEXT("configured entries avoid no-entry skips"), result.SkippedNoEntryCount, 0);
	TestEqual(TEXT("two visual actors remain owned by the materializer"), spawnedActors.Num(), 2);
	bool bFoundRoadBlockAtRoadHeight = false;
	bool bFoundBuildingBlockAtWalkwayHeight = false;
	for (const TObjectPtr<AActor>& spawnedActor : spawnedActors)
	{
		if (!spawnedActor)
		{
			continue;
		}

		TArray<USplineMeshComponent*> splineMeshComponents;
		spawnedActor->GetComponents<USplineMeshComponent>(splineMeshComponents);
		if (splineMeshComponents.IsEmpty())
		{
			bFoundBuildingBlockAtWalkwayHeight |= FMath::IsNearlyEqual(
				spawnedActor->GetActorLocation().Z,
				FScenarioCorridorGeometry::DefaultSurfaceTopZCm,
				0.1);
		}
		else
		{
			bFoundRoadBlockAtRoadHeight |= FMath::IsNearlyEqual(spawnedActor->GetActorLocation().Z, 0.0, 0.1);
		}
	}
	TestTrue(TEXT("road block snaps to road height"), bFoundRoadBlockAtRoadHeight);
	TestTrue(TEXT("building block snaps to walkway height"), bFoundBuildingBlockAtWalkwayHeight);

	spawnedActors.Reset();
	return true;
}

#endif
