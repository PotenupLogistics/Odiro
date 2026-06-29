#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioCityBlockMaterializer.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
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

	// Creates a generated building-side walkway extension that should not trigger road composite blocks.
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
		blockEntry.BPClass = AStaticMeshActor::StaticClass();
		blockEntry.Role = role;
		blockEntry.BoundsMeters.LengthMeters = 10.0;
		blockEntry.BoundsMeters.WidthMeters = widthMeters;
		blockEntry.BoundsMeters.HeightMeters = 1.0;
		blockEntry.SemanticProfile.SurfaceIds = { TEXT("road") };
		blockEntry.SemanticProfile.PrimaryRegionType = primaryRegionType;
		return blockEntry;
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

	FScenarioCityBlockCatalogEntry roadBlock = MakeMaterializerTestEntry(
		TEXT("city.road_straight_10m"),
		EScenarioCityBlockRole::RoadStraight,
		EScenarioGroundRegionType::Penalty,
		6.4);
	roadBlock.SemanticProfile.PenaltyKind = TEXT("road");
	catalog->Entries.Add(roadBlock);

	FScenarioCityBlockCatalogEntry compositeBlock = MakeMaterializerTestEntry(
		TEXT("city.walkway_curb_road_straight_10m"),
		EScenarioCityBlockRole::WalkwayRoadStraight,
		EScenarioGroundRegionType::Blocked,
		7.0);
	compositeBlock.SemanticProfile.SurfaceIds = { TEXT("road"), TEXT("walkway") };
	compositeBlock.SemanticProfile.CollisionTag = TEXT("curb");
	compositeBlock.PlacementProfile.Priority = 10;
	compositeBlock.PlacementProfile.LateralAnchor = EScenarioCityBlockLateralAnchor::RegionInnerEdge;
	catalog->Entries.Add(compositeBlock);

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

	TestEqual(TEXT("curb and road bands are both materializer candidates"), result.CandidateRegionCount, 2);
	TestEqual(TEXT("curb-triggered composite spawns once"), result.SpawnedActorCount, 1);
	TestEqual(TEXT("road band is skipped when covered by the composite"), result.SkippedCoveredByCompositeCount, 1);
	TestEqual(TEXT("configured entries avoid no-entry skips"), result.SkippedNoEntryCount, 0);
	TestEqual(TEXT("one visual actor remains owned by the materializer"), spawnedActors.Num(), 1);
	if (spawnedActors.Num() == 1 && spawnedActors[0])
	{
		TestTrue(
			TEXT("composite inner edge aligns to the curb inner edge"),
			FMath::IsNearlyEqual(spawnedActors[0]->GetActorLocation().Y, 350.0, 0.1));
		TestEqual(TEXT("road-side composite snaps to road height"), spawnedActors[0]->GetActorLocation().Z, 0.0);
	}

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCompositeSeamRootTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadCompositeSupportsSeamRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockMaterializerRoadCompositeSeamRootTest::RunTest(const FString& Parameters)
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

	FScenarioCityBlockCatalogEntry compositeBlock = MakeMaterializerTestEntry(
		TEXT("city.walkway_curb_road_straight_10m"),
		EScenarioCityBlockRole::WalkwayRoadStraight,
		EScenarioGroundRegionType::Blocked,
		7.0);
	compositeBlock.SemanticProfile.SurfaceIds = { TEXT("road"), TEXT("walkway") };
	compositeBlock.SemanticProfile.CollisionTag = TEXT("curb");
	compositeBlock.PlacementProfile.LateralAnchor = EScenarioCityBlockLateralAnchor::RegionInnerEdge;
	compositeBlock.BoundsMeters.CenterOffsetMeters = FVector(0.0, 3.5, 0.0);
	catalog->Entries.Add(compositeBlock);

	TArray<FScenarioGroundRegionSpec> groundRegions;
	groundRegions.Add(MakeGeneratedRoadSideRegion(
		TEXT("generated_city_main_upper_curb_00_00"),
		EScenarioGroundRegionType::Blocked,
		FVector(0.0, 25.0, 0.0),
		FVector2D(1000.0, 50.0),
		TEXT("curb")));

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

	TestEqual(TEXT("curb-triggered composite spawns once"), result.SpawnedActorCount, 1);
	TestEqual(TEXT("one seam-rooted actor remains owned by the materializer"), spawnedActors.Num(), 1);
	if (spawnedActors.Num() == 1 && spawnedActors[0])
	{
		TestTrue(
			TEXT("BP origin can sit on the continuous walkway-curb seam"),
			FMath::IsNearlyEqual(spawnedActors[0]->GetActorLocation().Y, 0.0, 0.1));
		TestEqual(TEXT("seam-rooted composite still snaps to road height"), spawnedActors[0]->GetActorLocation().Z, 0.0);
	}

	spawnedActors.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockMaterializerRoadCornerTest,
	"OdiroSim.Scenario.CityBlockMaterializer.RoadSideRightAngleCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

	FScenarioCityBlockCatalogEntry compositeBlock = MakeMaterializerTestEntry(
		TEXT("city.walkway_curb_road_straight_10m"),
		EScenarioCityBlockRole::WalkwayRoadStraight,
		EScenarioGroundRegionType::Blocked,
		7.0);
	compositeBlock.SemanticProfile.SurfaceIds = { TEXT("road"), TEXT("walkway") };
	compositeBlock.SemanticProfile.CollisionTag = TEXT("curb");
	compositeBlock.PlacementProfile.Priority = 10;
	compositeBlock.PlacementProfile.LateralAnchor = EScenarioCityBlockLateralAnchor::RegionInnerEdge;
	catalog->Entries.Add(compositeBlock);

	FScenarioCityBlockCatalogEntry cornerBlock = MakeMaterializerTestEntry(
		TEXT("city.walkway_road_corner_10m"),
		EScenarioCityBlockRole::Corner,
		EScenarioGroundRegionType::Penalty,
		10.0);
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

	TestEqual(TEXT("two curb bands and two covered road bands are candidates"), result.CandidateRegionCount, 4);
	TestEqual(TEXT("one road-side corner is inferred"), result.CornerCandidateCount, 1);
	TestEqual(TEXT("straight composites and the corner actor spawn"), result.SpawnedActorCount, 3);
	TestEqual(TEXT("road bands are skipped when covered by road-side composites"), result.SkippedCoveredByCompositeCount, 2);
	TestEqual(TEXT("configured corner entry avoids corner no-entry skips"), result.SkippedCornerNoEntryCount, 0);
	TestEqual(TEXT("three visual actors remain owned by the materializer"), spawnedActors.Num(), 3);

	bool bFoundHorizontalStraight = false;
	bool bFoundVerticalStraight = false;
	bool bFoundCornerAnchor = false;
	for (const TObjectPtr<AActor>& spawnedActor : spawnedActors)
	{
		if (!spawnedActor)
		{
			continue;
		}

		const FVector location = spawnedActor->GetActorLocation();
		bFoundHorizontalStraight |= FMath::IsNearlyEqual(location.X, 750.0, 0.1)
			&& FMath::IsNearlyEqual(location.Y, 350.0, 0.1);
		bFoundVerticalStraight |= FMath::IsNearlyEqual(location.X, 2350.0, 0.1)
			&& FMath::IsNearlyEqual(location.Y, -1250.0, 0.1);
		bFoundCornerAnchor |= FMath::IsNearlyEqual(location.X, 2000.0, 0.1)
			&& FMath::IsNearlyEqual(location.Y, 0.0, 0.1);
	}

	TestTrue(TEXT("horizontal straight composite leaves a corner reservation"), bFoundHorizontalStraight);
	TestTrue(TEXT("vertical straight composite leaves a corner reservation"), bFoundVerticalStraight);
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

	FScenarioCityBlockCatalogEntry compositeBlock = MakeMaterializerTestEntry(
		TEXT("city.walkway_curb_road_straight_10m"),
		EScenarioCityBlockRole::WalkwayRoadStraight,
		EScenarioGroundRegionType::Blocked,
		7.0);
	compositeBlock.SemanticProfile.SurfaceIds = { TEXT("road"), TEXT("walkway") };
	compositeBlock.SemanticProfile.CollisionTag = TEXT("curb");
	compositeBlock.PlacementProfile.LateralAnchor = EScenarioCityBlockLateralAnchor::RegionInnerEdge;
	catalog->Entries.Add(compositeBlock);

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
	TestEqual(TEXT("road-side composite does not spawn for building-side walkway extension"), result.SpawnedActorCount, 0);
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
	if (spawnedActors.Num() == 2 && spawnedActors[0] && spawnedActors[1])
	{
		TestEqual(TEXT("road block snaps to road height"), spawnedActors[0]->GetActorLocation().Z, 0.0);
		TestEqual(
			TEXT("building block snaps to walkway height"),
			spawnedActors[1]->GetActorLocation().Z,
			FScenarioCorridorGeometry::DefaultSurfaceTopZCm);
	}

	spawnedActors.Reset();
	return true;
}

#endif
