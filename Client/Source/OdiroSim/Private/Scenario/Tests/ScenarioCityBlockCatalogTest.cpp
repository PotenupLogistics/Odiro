#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/Data/ScenarioCityBlockCatalog.h"

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioCityBlockCatalogLookupTest,
	"OdiroSim.Scenario.CityBlockCatalog.Lookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioCityBlockCatalogLookupTest::RunTest(const FString& Parameters)
{
	const TSoftObjectPtr<UScenarioCityBlockCatalog> defaultCatalog =
		UScenarioCityBlockCatalog::MakeDefaultCatalogReference();
	TestEqual(
		TEXT("City block catalog uses the project data-asset path"),
		defaultCatalog.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/Data/Scenario/DA_ScenarioCityBlockCatalog.DA_ScenarioCityBlockCatalog")));

	UScenarioCityBlockCatalog* catalog = NewObject<UScenarioCityBlockCatalog>();
	TestNotNull(TEXT("Catalog can be constructed for editor-authored entries"), catalog);

	FScenarioCityBlockCatalogEntry walkwayRoadBlock;
	walkwayRoadBlock.BlockId = TEXT("city.walkway_road_straight_10m");
	walkwayRoadBlock.BPClass = AActor::StaticClass();
	walkwayRoadBlock.Role = EScenarioCityBlockRole::WalkwayRoadStraight;
	walkwayRoadBlock.BoundsMeters.LengthMeters = 10.0;
	walkwayRoadBlock.BoundsMeters.WidthMeters = 8.0;
	walkwayRoadBlock.BoundsMeters.HeightMeters = 1.0;
	walkwayRoadBlock.SemanticProfile.ProfileId = TEXT("walkway_road");
	walkwayRoadBlock.SemanticProfile.SurfaceIds = { TEXT("walkway"), TEXT("road") };
	walkwayRoadBlock.SemanticProfile.PrimaryRegionType = EScenarioGroundRegionType::Walkable;
	walkwayRoadBlock.SemanticProfile.PenaltyKind = TEXT("road");
	catalog->Entries.Add(walkwayRoadBlock);

	FScenarioCityBlockCatalogEntry buildingBlock;
	buildingBlock.BlockId = TEXT("city.building_frontage_10m");
	buildingBlock.BPClass = AActor::StaticClass();
	buildingBlock.Role = EScenarioCityBlockRole::Building;
	buildingBlock.BoundsMeters.LengthMeters = 10.0;
	buildingBlock.BoundsMeters.WidthMeters = 10.0;
	buildingBlock.BoundsMeters.HeightMeters = 20.0;
	buildingBlock.SemanticProfile.ProfileId = TEXT("building");
	buildingBlock.SemanticProfile.SurfaceIds = { TEXT("building") };
	buildingBlock.SemanticProfile.PrimaryRegionType = EScenarioGroundRegionType::Blocked;
	buildingBlock.SemanticProfile.CollisionTag = TEXT("building");
	catalog->Entries.Add(buildingBlock);

	FScenarioCityBlockCatalogEntry foundBlock;
	TestTrue(
		TEXT("Catalog finds block entries by stable BlockId"),
		catalog->FindBlockEntryById(TEXT("city.walkway_road_straight_10m"), foundBlock));
	TestTrue(
		TEXT("Found block keeps its placement role"),
		foundBlock.Role == EScenarioCityBlockRole::WalkwayRoadStraight);
	TestEqual(TEXT("Found block keeps authored length"), foundBlock.BoundsMeters.LengthMeters, 10.0);
	TestEqual(TEXT("Found block keeps semantic surface count"), foundBlock.SemanticProfile.SurfaceIds.Num(), 2);
	TestFalse(TEXT("Catalog rejects empty BlockId"), catalog->FindBlockEntryById(NAME_None, foundBlock));
	TestFalse(
		TEXT("Catalog rejects unknown BlockId"),
		catalog->FindBlockEntryById(TEXT("city.unknown"), foundBlock));

	TArray<FScenarioCityBlockCatalogEntry> buildingEntries;
	catalog->GetEntriesForRole(EScenarioCityBlockRole::Building, buildingEntries);
	TestEqual(TEXT("Role lookup returns building BP entries"), buildingEntries.Num(), 1);
	if (buildingEntries.Num() == 1)
	{
		TestEqual(
			TEXT("Role lookup keeps building BlockId"),
			buildingEntries[0].BlockId.ToString(),
			FString(TEXT("city.building_frontage_10m")));
		TestTrue(
			TEXT("Building semantic profile expects Blocked backing GroundRegion"),
			buildingEntries[0].SemanticProfile.PrimaryRegionType == EScenarioGroundRegionType::Blocked);
	}

	TArray<FScenarioCityBlockCatalogEntry> cornerEntries;
	catalog->GetEntriesForRole(EScenarioCityBlockRole::Corner, cornerEntries);
	TestEqual(TEXT("Role lookup returns an empty array when no candidate exists"), cornerEntries.Num(), 0);

	return true;
}

#endif
