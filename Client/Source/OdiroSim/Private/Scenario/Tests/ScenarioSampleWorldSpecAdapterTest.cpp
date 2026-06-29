#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioSampleWorldSpecAdapter.h"

#include "Misc/AutomationTest.h"
#include "Shared/ScenarioSampleJson.h"

namespace
{
	FScenarioSampleDocument MakeAdapterTestSampleDocument()
	{
		FScenarioSampleDocument Document;
		Document.Sample.SampleId = TEXT("000042");
		Document.Sample.ScenarioId = TEXT("editor_visualization_sample_000042");
		Document.Sample.Source.TemplateRef = TEXT("templates/editor_visualization.template.json");
		Document.Sample.Source.TemplateHash = TEXT("sha256:templatehash0042");
		Document.Sample.Source.ProfileRef = TEXT("experiments/editor/profile.json");
		Document.Sample.Source.ProfileHash = TEXT("sha256:profilehash0042");
		Document.Sample.Source.SettingRef = TEXT("experiments/editor/setting.json");
		Document.Sample.Source.SettingHash = TEXT("sha256:settinghash0042");
		Document.Sample.Source.Seed = 4242;
		Document.Sample.Source.GeneratorVersion = TEXT("0.1.0");

		FScenarioSampleParamValue TimeLimit;
		TimeLimit.Type = EScenarioSampleParamValueType::Float;
		TimeLimit.FloatValue = 45.0;
		Document.Scenario.Params.Add(TEXT("max_duration_s"), TimeLimit);

		FScenarioSampleSemantic& Semantic = Document.Scenario.Semantic;
		Semantic.RouteAxis.OriginXYMeters = FVector2D::ZeroVector;
		Semantic.RouteAxis.HeadingDegrees = 0.0;
		Semantic.RouteAxis.PointsMeters = { FVector2D(0.0, 0.0), FVector2D(10.0, 0.0) };
		Semantic.RouteAxis.LengthMeters = 10.0;

		Semantic.Robot.Start.SegmentId = TEXT("main");
		Semantic.Robot.Start.AlongMeters = 0.0;
		Semantic.Robot.Start.OffsetMeters = 0.0;
		Semantic.Robot.Start.LaneId = TEXT("walkway");
		Semantic.Robot.Start.HeadingDegrees = 0.0;
		Semantic.Robot.Start.SourceAnchorType = EScenarioTemplateRobotAnchorType::Entry;
		Semantic.Robot.Goal.SegmentId = TEXT("main");
		Semantic.Robot.Goal.AlongMeters = 10.0;
		Semantic.Robot.Goal.OffsetMeters = 0.0;
		Semantic.Robot.Goal.LaneId = TEXT("walkway");
		Semantic.Robot.Goal.HeadingDegrees = 0.0;
		Semantic.Robot.Goal.SourceAnchorType = EScenarioTemplateRobotAnchorType::Exit;

		FScenarioSampleLayoutLane WalkwayLane;
		WalkwayLane.LaneId = TEXT("walkway");
		WalkwayLane.OffsetRangeMeters.MinMeters = -1.0;
		WalkwayLane.OffsetRangeMeters.MaxMeters = 1.0;
		WalkwayLane.SurfaceId = TEXT("walkway");
		WalkwayLane.Type = EScenarioSampleLaneType::Walkable;

		FScenarioSampleLayoutLane CurbLane;
		CurbLane.LaneId = TEXT("curb_edge");
		CurbLane.OffsetRangeMeters.MinMeters = 1.0;
		CurbLane.OffsetRangeMeters.MaxMeters = 1.5;
		CurbLane.SurfaceId = TEXT("road");
		CurbLane.Type = EScenarioSampleLaneType::Blocked;

		FScenarioSampleLayoutLane RoadLane;
		RoadLane.LaneId = TEXT("road_2lane");
		RoadLane.OffsetRangeMeters.MinMeters = 1.5;
		RoadLane.OffsetRangeMeters.MaxMeters = 7.9;
		RoadLane.SurfaceId = TEXT("road");
		RoadLane.Type = EScenarioSampleLaneType::Penalty;

		FScenarioSampleLayoutEntry LayoutEntry;
		LayoutEntry.AlongRangeMeters.StartMeters = 0.0;
		LayoutEntry.AlongRangeMeters.EndMeters = 10.0;
		LayoutEntry.SegmentId = TEXT("main");
		LayoutEntry.Lanes.Add(WalkwayLane);
		LayoutEntry.Lanes.Add(CurbLane);
		LayoutEntry.Lanes.Add(RoadLane);
		Semantic.Layout.Add(LayoutEntry);

		FScenarioSampleStaticObstacle Obstacle;
		Obstacle.ObstacleId = TEXT("crate_01");
		Obstacle.PropId = TEXT("crate");
		Obstacle.PerceptionTag = TEXT("solid");
		Obstacle.ObstacleClass = EScenarioSampleObstacleClass::Blocking;
		Obstacle.SensorProfile = TEXT("solid");
		Obstacle.AlongMeters = 4.0;
		Obstacle.OffsetMeters = 1.75;
		Obstacle.YawDegrees = 15.0;
		Obstacle.FootprintMeters = FVector2D(0.5, 0.5);
		Obstacle.PlacedBy = TEXT("crate_fixed");
		Obstacle.ClearWidthRemainingMeters = 1.5;
		Semantic.StaticObstacles.Add(Obstacle);

		FScenarioSampleClearWidthEntry ClearWidth;
		ClearWidth.AlongRangeMeters.StartMeters = 0.0;
		ClearWidth.AlongRangeMeters.EndMeters = 10.0;
		ClearWidth.ClearWidthMeters = 1.5;
		ClearWidth.LimitedBy = TEXT("crate_01");
		Semantic.ClearWidthProfile.Add(ClearWidth);
		Semantic.Summary.GlobalMinClearWidthMeters = 1.5;
		Semantic.Summary.MinClearAtAlongMeters = 4.0;
		Semantic.Summary.TotalLengthMeters = 10.0;
		return Document;
	}

	// Creates a two-segment right-angle sample whose building side encloses one rectangular city block.
	FScenarioSampleDocument MakeRightAngleBuildingExpansionSampleDocument()
	{
		FScenarioSampleDocument Document;
		Document.Sample.SampleId = TEXT("000043");
		Document.Sample.ScenarioId = TEXT("right_angle_building_expansion_sample_000043");
		Document.Sample.Source.TemplateRef = TEXT("templates/right_angle_building_expansion.template.json");
		Document.Sample.Source.TemplateHash = TEXT("sha256:templatehash0043");
		Document.Sample.Source.ProfileRef = TEXT("experiments/right_angle/profile.json");
		Document.Sample.Source.ProfileHash = TEXT("sha256:profilehash0043");
		Document.Sample.Source.SettingRef = TEXT("experiments/right_angle/setting.json");
		Document.Sample.Source.SettingHash = TEXT("sha256:settinghash0043");
		Document.Sample.Source.Seed = 4343;
		Document.Sample.Source.GeneratorVersion = TEXT("0.1.0");

		FScenarioSampleParamValue TimeLimit;
		TimeLimit.Type = EScenarioSampleParamValueType::Float;
		TimeLimit.FloatValue = 45.0;
		Document.Scenario.Params.Add(TEXT("max_duration_s"), TimeLimit);

		FScenarioSampleSemantic& Semantic = Document.Scenario.Semantic;
		Semantic.RouteAxis.OriginXYMeters = FVector2D::ZeroVector;
		Semantic.RouteAxis.HeadingDegrees = 0.0;
		Semantic.RouteAxis.PointsMeters = {
			FVector2D(0.0, 40.0),
			FVector2D(60.0, 40.0),
			FVector2D(60.0, 0.0)
		};
		Semantic.RouteAxis.LengthMeters = 100.0;

		Semantic.Robot.Start.SegmentId = TEXT("east_straight");
		Semantic.Robot.Start.AlongMeters = 1.0;
		Semantic.Robot.Start.OffsetMeters = 0.0;
		Semantic.Robot.Start.LaneId = TEXT("walkway");
		Semantic.Robot.Start.HeadingDegrees = 0.0;
		Semantic.Robot.Start.SourceAnchorType = EScenarioTemplateRobotAnchorType::Entry;
		Semantic.Robot.Goal.SegmentId = TEXT("south_straight");
		Semantic.Robot.Goal.AlongMeters = 99.0;
		Semantic.Robot.Goal.OffsetMeters = 0.0;
		Semantic.Robot.Goal.LaneId = TEXT("walkway");
		Semantic.Robot.Goal.HeadingDegrees = -90.0;
		Semantic.Robot.Goal.SourceAnchorType = EScenarioTemplateRobotAnchorType::Exit;

		FScenarioSampleLayoutLane WalkwayLane;
		WalkwayLane.LaneId = TEXT("walkway");
		WalkwayLane.OffsetRangeMeters.MinMeters = -1.5;
		WalkwayLane.OffsetRangeMeters.MaxMeters = 1.5;
		WalkwayLane.SurfaceId = TEXT("walkway");
		WalkwayLane.Type = EScenarioSampleLaneType::Walkable;

		FScenarioSampleLayoutLane BuildingExpansionLane;
		BuildingExpansionLane.LaneId = TEXT("building_walkway_extension");
		BuildingExpansionLane.OffsetRangeMeters.MinMeters = -6.5;
		BuildingExpansionLane.OffsetRangeMeters.MaxMeters = -1.5;
		BuildingExpansionLane.SurfaceId = TEXT("walkway");
		BuildingExpansionLane.Type = EScenarioSampleLaneType::Walkable;

		FScenarioSampleLayoutLane BuildingLane;
		BuildingLane.LaneId = TEXT("building_edge");
		BuildingLane.OffsetRangeMeters.MinMeters = -16.5;
		BuildingLane.OffsetRangeMeters.MaxMeters = -6.5;
		BuildingLane.SurfaceId = TEXT("building");
		BuildingLane.Type = EScenarioSampleLaneType::Blocked;

		FScenarioSampleLayoutEntry EastLayoutEntry;
		EastLayoutEntry.AlongRangeMeters.StartMeters = 0.0;
		EastLayoutEntry.AlongRangeMeters.EndMeters = 60.0;
		EastLayoutEntry.SegmentId = TEXT("east_straight");
		EastLayoutEntry.Lanes.Add(WalkwayLane);
		EastLayoutEntry.Lanes.Add(BuildingExpansionLane);
		EastLayoutEntry.Lanes.Add(BuildingLane);
		Semantic.Layout.Add(EastLayoutEntry);

		FScenarioSampleLayoutEntry SouthLayoutEntry;
		SouthLayoutEntry.AlongRangeMeters.StartMeters = 60.0;
		SouthLayoutEntry.AlongRangeMeters.EndMeters = 100.0;
		SouthLayoutEntry.SegmentId = TEXT("south_straight");
		SouthLayoutEntry.Lanes.Add(WalkwayLane);
		SouthLayoutEntry.Lanes.Add(BuildingExpansionLane);
		SouthLayoutEntry.Lanes.Add(BuildingLane);
		Semantic.Layout.Add(SouthLayoutEntry);

		Semantic.Summary.TotalLengthMeters = 100.0;
		return Document;
	}

	// Checks whether adapter validation emitted the expected diagnostic code.
	bool HasAdapterDiagnostic(
		const FScenarioCompileResult& Result,
		const FString& Code)
	{
		return Result.Diagnostics.ContainsByPredicate(
			[&Code](const FScenarioCompileDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == Code
					&& Diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error;
			});
	}

	// Finds one generated city GroundRegion by its deterministic adapter id.
	const FScenarioGroundRegionSpec* FindGeneratedCityRegion(
		const FScenarioCompileResult& Result,
		const FString& RegionId)
	{
		return Result.WorldSpec.GroundRegions.FindByPredicate(
			[&RegionId](const FScenarioGroundRegionSpec& Region)
			{
				return Region.RegionId == RegionId;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleWorldSpecAdapterValidTest,
	"OdiroSim.ScenarioSample.WorldSpecAdapter.Valid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleWorldSpecAdapterValidTest::RunTest(const FString& Parameters)
{
	const FScenarioSampleDocument Document = MakeAdapterTestSampleDocument();
	const FScenarioCompileResult Result =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Document);

	TestTrue(TEXT("sample adapts"), Result.bSuccess);
	TestEqual(TEXT("scenario id"), Result.WorldSpec.RunConfig.TemplateId, Document.Sample.ScenarioId);
	TestEqual(TEXT("base seed"), Result.WorldSpec.RunConfig.BaseSeed, Document.Sample.Source.Seed);
	TestEqual(TEXT("max duration seconds"), Result.WorldSpec.RunConfig.MaxDurationSeconds, 45.0);
	TestEqual(TEXT("runtime corridor count"), Result.WorldSpec.Corridors.Num(), 1);
	if (Result.WorldSpec.Corridors.IsEmpty())
	{
		return false;
	}
	TestEqual(TEXT("generated ground region count"), Result.WorldSpec.GroundRegions.Num(), 2);
	const FScenarioGroundRegionSpec* GeneratedCurbRegion =
		FindGeneratedCityRegion(Result, TEXT("generated_city_main_upper_curb_00_00"));
	TestNotNull(TEXT("generated curb region"), GeneratedCurbRegion);
	if (GeneratedCurbRegion)
	{
		TestEqual(
			TEXT("generated curb region type"),
			static_cast<int32>(GeneratedCurbRegion->RegionType),
			static_cast<int32>(EScenarioGroundRegionType::Blocked));
		TestEqual(TEXT("generated curb surface"), GeneratedCurbRegion->SurfaceId, FString(TEXT("road")));
		TestEqual(TEXT("generated curb collision tag"), GeneratedCurbRegion->CollisionTag, FString(TEXT("curb")));
		TestEqual(TEXT("generated curb width cm"), GeneratedCurbRegion->Size.Y, 50.0);
		TestEqual(TEXT("generated curb top z cm"), GeneratedCurbRegion->Center.Z, 0.0);
	}
	const FScenarioGroundRegionSpec* GeneratedRoadRegion =
		FindGeneratedCityRegion(Result, TEXT("generated_city_main_upper_road_2lane_00_00"));
	TestNotNull(TEXT("generated road region"), GeneratedRoadRegion);
	if (GeneratedRoadRegion)
	{
		TestEqual(
			TEXT("generated road region type"),
			static_cast<int32>(GeneratedRoadRegion->RegionType),
			static_cast<int32>(EScenarioGroundRegionType::Penalty));
		TestEqual(TEXT("generated road surface"), GeneratedRoadRegion->SurfaceId, FString(TEXT("road")));
		TestEqual(TEXT("generated road penalty kind"), GeneratedRoadRegion->PenaltyKind, FString(TEXT("road")));
		TestEqual(TEXT("generated road width cm"), GeneratedRoadRegion->Size.Y, 640.0);
		TestEqual(TEXT("generated road top z cm"), GeneratedRoadRegion->Center.Z, 0.0);
	}
	const FScenarioRuntimeCorridorSpec& RuntimeCorridor = Result.WorldSpec.Corridors[0];
	TestEqual(TEXT("runtime corridor layout count"), RuntimeCorridor.Layout.Num(), 1);
	if (RuntimeCorridor.Layout.IsEmpty() || RuntimeCorridor.Layout[0].Lanes.IsEmpty())
	{
		return false;
	}
	TestEqual(TEXT("runtime corridor lane surface"), RuntimeCorridor.Layout[0].Lanes[0].SurfaceId, FString(TEXT("walkway")));
	const FScenarioRuntimeCorridorLaneSpec* CurbRuntimeLane = RuntimeCorridor.Layout[0].Lanes.FindByPredicate(
		[](const FScenarioRuntimeCorridorLaneSpec& Lane)
		{
			return Lane.LaneId == TEXT("curb_edge");
		});
	TestNotNull(TEXT("runtime curb lane"), CurbRuntimeLane);
	if (CurbRuntimeLane)
	{
		TestEqual(TEXT("runtime curb lane z offset"), CurbRuntimeLane->SurfaceZOffsetCm, 0.0);
		TestEqual(
			TEXT("runtime curb lane region type"),
			static_cast<int32>(CurbRuntimeLane->RegionType),
			static_cast<int32>(EScenarioGroundRegionType::Blocked));
	}
	TestEqual(TEXT("placeable count"), Result.WorldSpec.Placeables.Num(), 2);
	TestEqual(TEXT("static obstacle count"), Result.WorldSpec.Placeables.FilterByPredicate(
		[](const FScenarioPlaceableInstanceSpec& Spec)
		{
			return Spec.Category == EScenarioActorCategory::StaticObstacle;
		}).Num(), 1);
	const FScenarioPlaceableInstanceSpec* StaticObstacleSpec = Result.WorldSpec.Placeables.FindByPredicate(
		[](const FScenarioPlaceableInstanceSpec& Spec)
		{
			return Spec.Category == EScenarioActorCategory::StaticObstacle;
		});
	TestNotNull(TEXT("static obstacle spec"), StaticObstacleSpec);
	if (StaticObstacleSpec)
	{
		TestEqual(TEXT("static obstacle surface z"), StaticObstacleSpec->Transform.GetLocation().Z, 0.0);
	}

	const FScenarioPlaceableInstanceSpec* RobotSpec = Result.WorldSpec.Placeables.FindByPredicate(
		[](const FScenarioPlaceableInstanceSpec& Spec)
		{
			return Spec.Category == EScenarioActorCategory::DeliveryBot;
		});
	TestNotNull(TEXT("robot spec"), RobotSpec);
	if (RobotSpec)
	{
		TestEqual(TEXT("robot start x"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.X, 100.0);
		TestEqual(TEXT("robot goal x"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.X, 900.0);
		TestEqual(TEXT("robot start surface z"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.Z, 0.0);
		TestEqual(TEXT("robot goal surface z"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.Z, 0.0);
		TestTrue(TEXT("robot has route"), RobotSpec->DeliveryBot.bHasStartLocation && RobotSpec->DeliveryBot.bHasGoalLocation);
		TestTrue(TEXT("robot setup has goal"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bHasGoal);

		const FScenarioParamValue* SampleStartAlong = RobotSpec->Properties.Find(TEXT("sample_start_along_m"));
		const FScenarioParamValue* SampleGoalAlong = RobotSpec->Properties.Find(TEXT("sample_goal_along_m"));
		const FScenarioParamValue* RuntimeStartAlong = RobotSpec->Properties.Find(TEXT("runtime_start_along_m"));
		const FScenarioParamValue* RuntimeGoalAlong = RobotSpec->Properties.Find(TEXT("runtime_goal_along_m"));
		TestNotNull(TEXT("sample start along property"), SampleStartAlong);
		TestNotNull(TEXT("sample goal along property"), SampleGoalAlong);
		TestNotNull(TEXT("runtime start along property"), RuntimeStartAlong);
		TestNotNull(TEXT("runtime goal along property"), RuntimeGoalAlong);
		if (SampleStartAlong && SampleGoalAlong && RuntimeStartAlong && RuntimeGoalAlong)
		{
			TestEqual(TEXT("sample start along"), SampleStartAlong->FloatValue, 0.0);
			TestEqual(TEXT("sample goal along"), SampleGoalAlong->FloatValue, 10.0);
			TestEqual(TEXT("runtime start along"), RuntimeStartAlong->FloatValue, 1.0);
			TestEqual(TEXT("runtime goal along"), RuntimeGoalAlong->FloatValue, 9.0);
		}
	}

	TestFalse(TEXT("spec hash populated"), Result.WorldSpec.SpecHash.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleWorldSpecAdapterRightAngleBuildingExpansionTest,
	"OdiroSim.ScenarioSample.WorldSpecAdapter.RightAngleBuildingExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleWorldSpecAdapterRightAngleBuildingExpansionTest::RunTest(const FString& Parameters)
{
	const FScenarioSampleDocument Document = MakeRightAngleBuildingExpansionSampleDocument();
	const FScenarioCompileResult Result =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Document);

	TestTrue(TEXT("right-angle sample adapts"), Result.bSuccess);
	TestEqual(TEXT("one generated building-side expansion"), Result.WorldSpec.GroundRegions.Num(), 1);
	if (Result.WorldSpec.GroundRegions.IsEmpty())
	{
		return false;
	}

	const FScenarioGroundRegionSpec& ExpansionRegion = Result.WorldSpec.GroundRegions[0];
	TestEqual(
		TEXT("building-side expansion id"),
		ExpansionRegion.RegionId,
		FString(TEXT("generated_city_lower_building_expansion_00_00")));
	TestEqual(TEXT("building-side expansion surface"), ExpansionRegion.SurfaceId, FString(TEXT("walkway")));
	TestEqual(
		TEXT("building-side expansion region type"),
		static_cast<int32>(ExpansionRegion.RegionType),
		static_cast<int32>(EScenarioGroundRegionType::Walkable));
	TestEqual(TEXT("building-side expansion center x cm"), ExpansionRegion.Center.X, 3000.0);
	TestEqual(TEXT("building-side expansion center y cm"), ExpansionRegion.Center.Y, 2000.0);
	TestEqual(TEXT("building-side expansion length cm"), ExpansionRegion.Size.X, 6000.0);
	TestEqual(TEXT("building-side expansion width cm"), ExpansionRegion.Size.Y, 4000.0);
	TestEqual(TEXT("building-side expansion yaw"), ExpansionRegion.YawDegrees, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleWorldSpecAdapterRejectsObstacleOutsideSurfaceTest,
	"OdiroSim.ScenarioSample.WorldSpecAdapter.RejectsObstacleOutsideSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleWorldSpecAdapterRejectsObstacleOutsideSurfaceTest::RunTest(const FString& Parameters)
{
	FScenarioSampleDocument Document = MakeAdapterTestSampleDocument();
	Document.Scenario.Semantic.StaticObstacles[0].OffsetMeters = 4.0;

	const FScenarioCompileResult Result =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Document);

	TestFalse(TEXT("adapter rejects outside-surface obstacle"), Result.bSuccess);
	TestTrue(
		TEXT("outside surface diagnostic"),
		HasAdapterDiagnostic(Result, TEXT("sample_obstacle_surface_missing")));
	TestEqual(TEXT("only robot placeable remains"), Result.WorldSpec.Placeables.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleWorldSpecAdapterRejectsObstacleOnBlockedSurfaceTest,
	"OdiroSim.ScenarioSample.WorldSpecAdapter.RejectsObstacleOnBlockedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleWorldSpecAdapterRejectsObstacleOnBlockedSurfaceTest::RunTest(const FString& Parameters)
{
	FScenarioSampleDocument Document = MakeAdapterTestSampleDocument();

	FScenarioSampleLayoutLane BlockedLane;
	BlockedLane.LaneId = TEXT("building_edge");
	BlockedLane.OffsetRangeMeters.MinMeters = -2.0;
	BlockedLane.OffsetRangeMeters.MaxMeters = -1.0;
	BlockedLane.SurfaceId = TEXT("building");
	BlockedLane.Type = EScenarioSampleLaneType::Blocked;
	Document.Scenario.Semantic.Layout[0].Lanes.Add(BlockedLane);
	Document.Scenario.Semantic.StaticObstacles[0].OffsetMeters = -1.5;

	const FScenarioCompileResult Result =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Document);

	TestFalse(TEXT("adapter rejects blocked-surface obstacle"), Result.bSuccess);
	TestTrue(
		TEXT("blocked surface diagnostic"),
		HasAdapterDiagnostic(Result, TEXT("sample_obstacle_on_blocked_surface")));
	TestEqual(TEXT("only robot placeable remains"), Result.WorldSpec.Placeables.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleWorldSpecAdapterVersionMismatchTest,
	"OdiroSim.ScenarioSample.WorldSpecAdapter.VersionMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleWorldSpecAdapterVersionMismatchTest::RunTest(const FString& Parameters)
{
	FScenarioSampleDocument Document = MakeAdapterTestSampleDocument();
	Document.Version = FScenarioSampleJson::SupportedVersion + 1;

	const FScenarioCompileResult Result =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Document);

	TestFalse(TEXT("version mismatch fails"), Result.bSuccess);
	TestTrue(TEXT("unsupported version diagnostic"), Result.Diagnostics.ContainsByPredicate(
		[](const FScenarioCompileDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("unsupported_schema_version")
				&& Diagnostic.Severity == EScenarioCompileDiagnosticSeverity::Error;
		}));
	return true;
}

#endif
