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
		Document.Scenario.Params.Add(TEXT("time_limit_s"), TimeLimit);

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
		WalkwayLane.SurfaceId = TEXT("sidewalk");
		WalkwayLane.Type = EScenarioSampleLaneType::Walkable;

		FScenarioSampleLayoutEntry LayoutEntry;
		LayoutEntry.AlongRangeMeters.StartMeters = 0.0;
		LayoutEntry.AlongRangeMeters.EndMeters = 10.0;
		LayoutEntry.SegmentId = TEXT("main");
		LayoutEntry.Lanes.Add(WalkwayLane);
		Semantic.Layout.Add(LayoutEntry);

		FScenarioSampleStaticObstacle Obstacle;
		Obstacle.ObstacleId = TEXT("crate_01");
		Obstacle.PropId = TEXT("crate");
		Obstacle.PerceptionTag = TEXT("solid");
		Obstacle.ObstacleClass = EScenarioSampleObstacleClass::Blocking;
		Obstacle.SensorProfile = TEXT("solid");
		Obstacle.AlongMeters = 4.0;
		Obstacle.OffsetMeters = 0.5;
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
	TestEqual(TEXT("ground region count"), Result.WorldSpec.GroundRegions.Num(), 1);
	if (Result.WorldSpec.GroundRegions.IsEmpty())
	{
		return false;
	}
	TestEqual(TEXT("ground region surface"), Result.WorldSpec.GroundRegions[0].SurfaceId, FString(TEXT("sidewalk")));
	TestEqual(TEXT("placeable count"), Result.WorldSpec.Placeables.Num(), 2);
	TestEqual(TEXT("static obstacle count"), Result.WorldSpec.Placeables.FilterByPredicate(
		[](const FScenarioPlaceableInstanceSpec& Spec)
		{
			return Spec.Category == EScenarioActorCategory::StaticObstacle;
		}).Num(), 1);

	const FScenarioPlaceableInstanceSpec* RobotSpec = Result.WorldSpec.Placeables.FindByPredicate(
		[](const FScenarioPlaceableInstanceSpec& Spec)
		{
			return Spec.Category == EScenarioActorCategory::DeliveryBot;
		});
	TestNotNull(TEXT("robot spec"), RobotSpec);
	if (RobotSpec)
	{
		TestEqual(TEXT("robot start x"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.X, 0.0);
		TestEqual(TEXT("robot goal x"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.X, 1000.0);
		TestTrue(TEXT("robot has route"), RobotSpec->DeliveryBot.bHasStartLocation && RobotSpec->DeliveryBot.bHasGoalLocation);
		TestTrue(TEXT("robot setup has goal"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.bHasGoal);
	}

	TestFalse(TEXT("spec hash populated"), Result.WorldSpec.SpecHash.IsEmpty());
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
