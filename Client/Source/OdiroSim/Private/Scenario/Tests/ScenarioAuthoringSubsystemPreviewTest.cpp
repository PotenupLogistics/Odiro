#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"

namespace
{
	// Owns an isolated world used by authoring preview automation tests.
	struct FScenarioAuthoringPreviewTestWorld
	{
		// Transient world that receives editor preview actors during one test.
		UWorld* World = nullptr;

		// Creates a minimal world so the authoring subsystem can rebuild preview actors without loading a map.
		FScenarioAuthoringPreviewTestWorld()
		{
			static int32 NextWorldIndex = 0;
			const FName WorldName(*FString::Printf(
				TEXT("ScenarioAuthoringPreviewTestWorld_%d"),
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

		// Destroys the isolated world after preview actors and subsystems have released ownership.
		~FScenarioAuthoringPreviewTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}
	};

	// Scenario JSON with building-side and road-side generated city expansion.
	FString MakeGeneratedCityPreviewScenarioJson()
	{
		return TEXT(R"({
			"schema": "scenario",
			"version": 1,
			"scenario_id": "authoring_generated_city_preview",
			"intent": "Preview generated city expansion.",
			"corridor": {
				"axis": {
					"type": "polyline",
					"points_m": [[0.0, 0.0], [30.0, 0.0]]
				},
				"walkway_width_m": 3.0,
				"segments": [
					{
						"id": "main",
						"type": "straight",
						"along_range_m": [0.0, 30.0]
					}
				],
				"building_side": [{"surface": "building", "width_m": 0.5}],
				"curb_side": [{"surface": "road", "width_m": 4.0}]
			},
			"obstacles": {"min_clear_width_m": 0.9, "placements": []},
			"pedestrians": {"background": {"count": 0}, "encounters": []},
			"robot": {
				"start": {"type": "corridor_pose", "segment": "main", "along_m": 1.0, "offset_m": 0.0, "heading": "forward"},
				"goal": {"type": "corridor_pose", "segment": "main", "along_m": 29.0, "offset_m": 0.0, "heading": "forward"}
			}
		})");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioAuthoringGeneratedCityPreviewGroundRegionsTest,
	"OdiroSim.Scenario.Authoring.GeneratedCityPreviewGroundRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioAuthoringGeneratedCityPreviewGroundRegionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScenarioAuthoringPreviewTestWorld TestWorld;
	TestNotNull(TEXT("Transient test world is available"), TestWorld.World);
	if (!TestWorld.World)
	{
		return false;
	}

	UScenarioAuthoringSubsystem* AuthoringSubsystem = TestWorld.World->GetSubsystem<UScenarioAuthoringSubsystem>();
	TestNotNull(TEXT("Authoring subsystem is available"), AuthoringSubsystem);
	if (!AuthoringSubsystem)
	{
		return false;
	}

	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("Scenario JSON loads into authoring preview"),
		AuthoringSubsystem->LoadProjectScenarioJsonString(MakeGeneratedCityPreviewScenarioJson(), Diagnostics));
	TestEqual(TEXT("Authoring diagnostics"), Diagnostics.Num(), 0);

	int32 GeneratedWalkwayActorCount = 0;
	int32 GeneratedBuildingActorCount = 0;
	int32 GeneratedRoadActorCount = 0;
	for (TActorIterator<AScenarioGroundRegion> It(TestWorld.World); It; ++It)
	{
		const FScenarioGroundRegionSpec& RegionSpec = It->RegionSpec;
		if (!RegionSpec.RegionId.StartsWith(TEXT("generated_city_")))
		{
			continue;
		}

		if (RegionSpec.SurfaceId.Equals(TEXT("walkway"), ESearchCase::IgnoreCase))
		{
			++GeneratedWalkwayActorCount;
		}
		else if (RegionSpec.SurfaceId.Equals(TEXT("building"), ESearchCase::IgnoreCase))
		{
			++GeneratedBuildingActorCount;
		}
		else if (RegionSpec.SurfaceId.Equals(TEXT("road"), ESearchCase::IgnoreCase))
		{
			++GeneratedRoadActorCount;
		}
	}

	TestEqual(TEXT("Generated building-side expansion GroundRegion preview spawns once"), GeneratedWalkwayActorCount, 1);
	TestEqual(TEXT("Generated building footprint is not a GroundRegion preview"), GeneratedBuildingActorCount, 0);
	TestEqual(TEXT("Generated road GroundRegions stay visual-only through CityBuildings preview"), GeneratedRoadActorCount, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
