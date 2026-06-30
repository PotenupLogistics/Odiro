#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Actors/ScenarioGroundRegion.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"

namespace
{
	// Owns an isolated world used by GroundRegion component automation tests.
	struct FScenarioGroundRegionTestWorld
	{
		// Transient world that receives GroundRegion actors during one test.
		UWorld* World = nullptr;

		// Creates a minimal world so component state can be exercised without loading a map.
		FScenarioGroundRegionTestWorld()
		{
			static int32 NextWorldIndex = 0;
			const FName WorldName(*FString::Printf(
				TEXT("ScenarioGroundRegionTestWorld_%d"),
				++NextWorldIndex));

			UWorld::InitializationValues InitValues;
			InitValues.AllowAudioPlayback(false)
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
				WorldName,
				GetTransientPackage(),
				false,
				ERHIFeatureLevel::Num,
				&InitValues);
		}

		// Destroys the isolated world after spawned component ownership has been released.
		~FScenarioGroundRegionTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}
	};

	// Creates a rectangular GroundRegion spec with explicit generated-city metadata.
	FScenarioGroundRegionSpec MakeGroundRegionVisualTestSpec(
		const FString& RegionId,
		const FString& SurfaceId,
		EScenarioGroundRegionType RegionType)
	{
		FScenarioGroundRegionSpec Spec;
		Spec.RegionId = RegionId;
		Spec.RegionType = RegionType;
		Spec.SurfaceId = SurfaceId;
		Spec.ShapeType = EScenarioGroundShapeType::Rectangle;
		Spec.Center = FVector::ZeroVector;
		Spec.Size = FVector2D(1000.0, 500.0);
		Spec.YawDegrees = 0.0;
		if (RegionType == EScenarioGroundRegionType::Blocked)
		{
			Spec.CollisionTag = TEXT("building");
		}
		if (RegionType == EScenarioGroundRegionType::Penalty)
		{
			Spec.PenaltyKind = TEXT("road");
		}
		return Spec;
	}

	// Creates a convex-polygon GroundRegion spec used by generated city expansion previews.
	FScenarioGroundRegionSpec MakeGroundRegionPolygonTestSpec()
	{
		FScenarioGroundRegionSpec Spec;
		Spec.RegionId = TEXT("generated_city_lower_building_expansion_00_00");
		Spec.RegionType = EScenarioGroundRegionType::Walkable;
		Spec.SurfaceId = TEXT("walkway");
		Spec.ShapeType = EScenarioGroundShapeType::ConvexPolygon;
		Spec.Center = FVector(5000.0, 2500.0, 0.0);
		Spec.Size = FVector2D(10000.0, 3000.0);
		Spec.PolygonVertices = {
			FVector2D(-1000.0, -1500.0),
			FVector2D(5000.0, -1500.0),
			FVector2D(1000.0, 1500.0),
			FVector2D(-5000.0, 1500.0)
		};
		return Spec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioGroundRegionProceduralVisualTest,
	"OdiroSim.Scenario.GroundRegion.ProceduralVisualAndCollisionProxy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioGroundRegionProceduralVisualTest::RunTest(const FString& Parameters)
{
	FScenarioGroundRegionTestWorld TestWorld;
	TestNotNull(TEXT("Transient test world is available"), TestWorld.World);
	if (!TestWorld.World)
	{
		return false;
	}

	FString FailureReason;
	AScenarioGroundRegion* WalkwayRegion = AScenarioGroundRegion::SpawnConfigured(
		TestWorld.World,
		AScenarioGroundRegion::StaticClass(),
		MakeGroundRegionVisualTestSpec(
			TEXT("generated_city_main_upper_walkway_extension_00_00"),
			TEXT("walkway"),
			EScenarioGroundRegionType::Walkable),
		FailureReason);

	TestNotNull(TEXT("Walkway GroundRegion spawns"), WalkwayRegion);
	TestTrue(TEXT("Walkway spawn has no failure reason"), FailureReason.IsEmpty());
	if (!WalkwayRegion)
	{
		return false;
	}

	TestNotNull(TEXT("Walkway visual mesh component exists"), WalkwayRegion->RegionVisualMeshComponent.Get());
	TestNotNull(TEXT("Walkway collision proxy exists"), WalkwayRegion->RegionBoundsComponent.Get());
	if (!WalkwayRegion->RegionVisualMeshComponent || !WalkwayRegion->RegionBoundsComponent)
	{
		return false;
	}

	TestFalse(
		TEXT("Generated walkway GroundRegion is not authoring selectable"),
		WalkwayRegion->PlaceableComponent->bAuthoringSelectable);
	TestTrue(
		TEXT("Walkway visual mesh is visible"),
		WalkwayRegion->RegionVisualMeshComponent->IsVisible());
	TestEqual(
		TEXT("Walkway visual mesh has no collision"),
		static_cast<int32>(WalkwayRegion->RegionVisualMeshComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::NoCollision));
	TestFalse(
		TEXT("Walkway bounds proxy is hidden"),
		WalkwayRegion->RegionBoundsComponent->IsVisible());
	TestEqual(
		TEXT("Walkway bounds proxy keeps the Walkable profile"),
		WalkwayRegion->RegionBoundsComponent->GetCollisionProfileName(),
		FName(TEXT("Walkable")));

	AScenarioGroundRegion* RoadRegion = AScenarioGroundRegion::SpawnConfigured(
		TestWorld.World,
		AScenarioGroundRegion::StaticClass(),
		MakeGroundRegionVisualTestSpec(
			TEXT("generated_city_main_upper_road_2lane_00_00"),
			TEXT("road"),
			EScenarioGroundRegionType::Penalty),
		FailureReason);

	TestNotNull(TEXT("Road GroundRegion spawns"), RoadRegion);
	TestTrue(TEXT("Road spawn has no failure reason"), FailureReason.IsEmpty());
	if (!RoadRegion)
	{
		return false;
	}

	TestNotNull(TEXT("Road visual mesh component exists"), RoadRegion->RegionVisualMeshComponent.Get());
	TestNotNull(TEXT("Road collision proxy exists"), RoadRegion->RegionBoundsComponent.Get());
	if (!RoadRegion->RegionVisualMeshComponent || !RoadRegion->RegionBoundsComponent)
	{
		return false;
	}

	TestFalse(
		TEXT("Generated road visual is hidden so CityBuildings composite mesh owns the road image"),
		RoadRegion->RegionVisualMeshComponent->IsVisible());
	TestEqual(
		TEXT("Road bounds proxy keeps the Penalty profile"),
		RoadRegion->RegionBoundsComponent->GetCollisionProfileName(),
		FName(TEXT("Penalty")));

	AScenarioGroundRegion* PolygonRegion = AScenarioGroundRegion::SpawnConfigured(
		TestWorld.World,
		AScenarioGroundRegion::StaticClass(),
		MakeGroundRegionPolygonTestSpec(),
		FailureReason);

	TestNotNull(TEXT("Polygon GroundRegion spawns"), PolygonRegion);
	TestTrue(TEXT("Polygon spawn has no failure reason"), FailureReason.IsEmpty());
	if (!PolygonRegion)
	{
		return false;
	}

	TestTrue(
		TEXT("Polygon visual mesh is visible"),
		PolygonRegion->RegionVisualMeshComponent->IsVisible());
	TestEqual(
		TEXT("Polygon visual mesh owns collision"),
		static_cast<int32>(PolygonRegion->RegionVisualMeshComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::QueryAndPhysics));
	TestEqual(
		TEXT("Polygon visual mesh keeps the Walkable profile"),
		PolygonRegion->RegionVisualMeshComponent->GetCollisionProfileName(),
		FName(TEXT("Walkable")));
	TestEqual(
		TEXT("Polygon bounds proxy has no collision"),
		static_cast<int32>(PolygonRegion->RegionBoundsComponent->GetCollisionEnabled()),
		static_cast<int32>(ECollisionEnabled::NoCollision));
	TestTrue(
		TEXT("Polygon contains an interior point"),
		PolygonRegion->ContainsWorldLocation2D(FVector(5000.0, 2500.0, 0.0)));
	TestFalse(
		TEXT("Polygon rejects an outside point"),
		PolygonRegion->ContainsWorldLocation2D(FVector(0.0, 1000.0, 0.0)));
	const FProcMeshSection* PolygonMeshSection = PolygonRegion->RegionVisualMeshComponent->GetProcMeshSection(0);
	TestNotNull(TEXT("Polygon visual mesh section exists"), PolygonMeshSection);
	if (PolygonMeshSection && PolygonMeshSection->ProcIndexBuffer.Num() >= 3)
	{
		TestEqual(TEXT("Polygon first triangle starts at fan origin"), PolygonMeshSection->ProcIndexBuffer[0], 0);
		TestEqual(TEXT("Polygon first triangle uses reversed winding"), PolygonMeshSection->ProcIndexBuffer[1], 2);
		TestEqual(TEXT("Polygon first triangle ends at first edge"), PolygonMeshSection->ProcIndexBuffer[2], 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
