#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioSampler.h"

#include "Misc/AutomationTest.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioDocumentJson.h"

namespace
{
	FScenarioTemplateNumberValue MakeSamplerTestFixedNumber(double Value)
	{
		FScenarioTemplateNumberValue NumberValue;
		NumberValue.bIsSet = true;
		NumberValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
		NumberValue.FixedValue = Value;
		return NumberValue;
	}

	FScenarioTemplateNumberValue MakeSamplerTestRangeNumber(double MinValue, double MaxValue)
	{
		FScenarioTemplateNumberValue NumberValue;
		NumberValue.bIsSet = true;
		NumberValue.Mode = EScenarioTemplateNumberValueMode::Range;
		NumberValue.MinValue = MinValue;
		NumberValue.MaxValue = MaxValue;
		return NumberValue;
	}

	FScenarioTemplateStringValue MakeSamplerTestFixedString(const FString& Value)
	{
		FScenarioTemplateStringValue StringValue;
		StringValue.bIsSet = true;
		StringValue.Mode = EScenarioTemplateStringValueMode::Fixed;
		StringValue.FixedValue = Value;
		return StringValue;
	}

	FScenarioSamplerRequest MakeSamplerTestRequest()
	{
		FScenarioSamplerRequest Request;
		Request.SampleId = TEXT("000123");
		Request.ScenarioId = TEXT("fixed_curb_obstacle_000123");
		Request.Seed = 123;
		Request.SourceScenarioRef = TEXT("runs/test/snapshot/scenario.json");
		Request.SourceScenarioHash = TEXT("sha256:scenario123");
		Request.ProfileRef = TEXT("experiments/test/profile.json");
		Request.ProfileHash = TEXT("sha256:profile123");
		Request.SettingRef = TEXT("experiments/test/setting.json");
		Request.SettingHash = TEXT("sha256:setting123");
		Request.GeneratorVersion = TEXT("scenario_sampler_v1");
		return Request;
	}

	FScenarioDocument MakeSamplerTestScenario()
	{
		FScenarioDocument Document;
		Document.ScenarioId = TEXT("fixed_curb_obstacle");
		Document.Intent = TEXT("Generate a deterministic fixed obstacle relative to a curb edge.");
		Document.Corridor.Axis.PointsMeters = { FVector2D(0.0, 0.0), FVector2D(10.0, 0.0) };
		Document.Corridor.WalkwayWidthMeters = MakeSamplerTestFixedNumber(2.0);

		FScenarioTemplateLaneRule BuildingLane;
		BuildingLane.SurfaceId = TEXT("building");
		BuildingLane.WidthMeters = MakeSamplerTestFixedNumber(1.0);
		Document.Corridor.BuildingSide.Add(BuildingLane);

		FScenarioTemplateLaneRule CurbLane;
		CurbLane.SurfaceId = TEXT("road");
		CurbLane.WidthMeters = MakeSamplerTestFixedNumber(1.0);
		Document.Corridor.CurbSide.Add(CurbLane);

		FScenarioTemplateSegment MainSegment;
		MainSegment.SegmentId = TEXT("main");
		MainSegment.Type = EScenarioTemplateSegmentType::Straight;
		MainSegment.AlongRangeMeters.StartMeters = 0.0;
		MainSegment.AlongRangeMeters.EndMeters = 10.0;
		Document.Corridor.Segments.Add(MainSegment);

		Document.Obstacles.MinClearWidthMeters = MakeSamplerTestFixedNumber(1.0);

		FScenarioTemplateObstaclePlacement Placement;
		Placement.PlacementId = TEXT("crate_curb");
		Placement.Kind = EScenarioTemplateObstaclePlacementKind::Fixed;
		Placement.PropId = TEXT("crate");
		Placement.At.SegmentId = TEXT("main");
		Placement.At.AlongMeters = MakeSamplerTestFixedNumber(4.0);
		Placement.At.OffsetMeters = MakeSamplerTestFixedNumber(0.25);
		Placement.At.LaneId = TEXT("curb_edge");
		Placement.YawDegrees = MakeSamplerTestFixedNumber(15.0);
		Document.Obstacles.Placements.Add(Placement);

		Document.Pedestrians.Background.Count.FixedValue = 0;
		Document.Pedestrians.Background.SpeedMetersPerSecond = MakeSamplerTestFixedNumber(1.2);
		Document.Robot.Start.Type = EScenarioTemplateRobotAnchorType::Entry;
		Document.Robot.Goal.Type = EScenarioTemplateRobotAnchorType::Exit;
		return Document;
	}

	FScenarioDocument MakeSamplerTestScenarioWithRange()
	{
		FScenarioDocument Document = MakeSamplerTestScenario();
		Document.Corridor.WalkwayWidthMeters = MakeSamplerTestRangeNumber(2.0, 3.0);
		Document.Obstacles.Placements[0].At.AlongMeters = MakeSamplerTestRangeNumber(3.0, 5.0);
		return Document;
	}

	// Checks whether sampler validation emitted the expected diagnostic code.
	bool HasSamplerDiagnostic(
		const FScenarioSamplerResult& Result,
		const FString& Code)
	{
		return Result.Diagnostics.ContainsByPredicate(
			[&Code](const FScenarioSchemaDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == Code
					&& Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerFixedObstacleTest,
	"OdiroSim.Scenario.Sampler.FixedObstacle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerFixedObstacleTest::RunTest(const FString& Parameters)
{
	const FScenarioSamplerResult Result =
		FScenarioSampler::GenerateSample(MakeSamplerTestScenario(), MakeSamplerTestRequest());

	TestTrue(TEXT("sampler succeeds"), Result.bSuccess);
	TestEqual(TEXT("layout count"), Result.Document.Scenario.Semantic.Layout.Num(), 1);
	TestEqual(TEXT("layout lane count"), Result.Document.Scenario.Semantic.Layout[0].Lanes.Num(), 3);
	TestEqual(TEXT("robot start along"), Result.Document.Scenario.Semantic.Robot.Start.AlongMeters, 0.0);
	TestEqual(TEXT("robot goal along"), Result.Document.Scenario.Semantic.Robot.Goal.AlongMeters, 10.0);
	TestEqual(TEXT("static obstacle count"), Result.Document.Scenario.Semantic.StaticObstacles.Num(), 1);

	const FScenarioSampleStaticObstacle& Obstacle = Result.Document.Scenario.Semantic.StaticObstacles[0];
	TestEqual(TEXT("obstacle id"), Obstacle.ObstacleId, FString(TEXT("crate_curb")));
	TestEqual(TEXT("obstacle placed_by"), Obstacle.PlacedBy, FString(TEXT("crate_curb")));
	TestEqual(TEXT("curb edge relative offset resolves to axis offset"), Obstacle.OffsetMeters, 1.25);
	TestEqual(TEXT("obstacle yaw"), Obstacle.YawDegrees, 15.0);
	TestEqual(TEXT("clear width profile count"), Result.Document.Scenario.Semantic.ClearWidthProfile.Num(), 1);
	TestEqual(TEXT("min clear width"), Result.Document.Scenario.Semantic.Summary.GlobalMinClearWidthMeters, 1.5);

	FString Json;
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
	TestTrue(TEXT("sample JSON writes"), FScenarioSampleJson::TryWriteJson(Result.Document, Json, Diagnostics));

	const FScenarioCompileResult CompileResult =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Result.Document);
	TestTrue(TEXT("sample adapts to world spec"), CompileResult.bSuccess);
	TestEqual(TEXT("runtime corridor count"), CompileResult.WorldSpec.Corridors.Num(), 1);
	if (CompileResult.WorldSpec.Corridors.IsEmpty())
	{
		return false;
	}

	const FScenarioRuntimeCorridorSpec& RuntimeCorridor = CompileResult.WorldSpec.Corridors[0];
	TestEqual(TEXT("runtime corridor layout count"), RuntimeCorridor.Layout.Num(), 1);
	if (RuntimeCorridor.Layout.IsEmpty())
	{
		return false;
	}

	TestEqual(TEXT("runtime corridor lane count"), RuntimeCorridor.Layout[0].Lanes.Num(), 3);
	TestEqual(TEXT("generated ground regions"), CompileResult.WorldSpec.GroundRegions.Num(), 4);
	TestTrue(TEXT("generated walkway extension"), CompileResult.WorldSpec.GroundRegions.ContainsByPredicate(
		[](const FScenarioGroundRegionSpec& Region)
		{
			return Region.RegionId.Contains(TEXT("walkway_extension"))
				&& Region.SurfaceId == TEXT("walkway")
				&& Region.RegionType == EScenarioGroundRegionType::Walkable;
		}));
	TestTrue(TEXT("generated building footprint"), CompileResult.WorldSpec.GroundRegions.ContainsByPredicate(
		[](const FScenarioGroundRegionSpec& Region)
		{
			return Region.RegionId.Contains(TEXT("building"))
				&& Region.SurfaceId == TEXT("building")
				&& Region.RegionType == EScenarioGroundRegionType::Blocked;
		}));
	TestTrue(TEXT("generated road band"), CompileResult.WorldSpec.GroundRegions.ContainsByPredicate(
		[](const FScenarioGroundRegionSpec& Region)
		{
			return Region.RegionId.Contains(TEXT("road_2lane"))
				&& Region.SurfaceId == TEXT("road")
				&& Region.RegionType == EScenarioGroundRegionType::Penalty;
		}));
	TestTrue(TEXT("runtime walkway surface preserved"), RuntimeCorridor.Layout[0].Lanes.ContainsByPredicate(
		[](const FScenarioRuntimeCorridorLaneSpec& Lane)
		{
			return Lane.SurfaceId == TEXT("walkway");
		}));
	TestTrue(TEXT("runtime road surface preserved"), RuntimeCorridor.Layout[0].Lanes.ContainsByPredicate(
		[](const FScenarioRuntimeCorridorLaneSpec& Lane)
		{
			return Lane.SurfaceId == TEXT("road");
		}));
	TestTrue(TEXT("runtime building surface preserved"), RuntimeCorridor.Layout[0].Lanes.ContainsByPredicate(
		[](const FScenarioRuntimeCorridorLaneSpec& Lane)
		{
			return Lane.SurfaceId == TEXT("building");
		}));
	TestEqual(TEXT("runtime placeables"), CompileResult.WorldSpec.Placeables.Num(), 2);
	const FScenarioPlaceableInstanceSpec* RobotSpec = CompileResult.WorldSpec.Placeables.FindByPredicate(
		[](const FScenarioPlaceableInstanceSpec& Spec)
		{
			return Spec.Category == EScenarioActorCategory::DeliveryBot;
		});
	TestNotNull(TEXT("runtime robot spec"), RobotSpec);
	if (RobotSpec)
	{
		TestEqual(TEXT("runtime entry anchor inset"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm.X, 100.0);
		TestEqual(TEXT("runtime exit anchor inset"), RobotSpec->DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm.X, 900.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerSegmentReplacementSurfaceTypeTest,
	"OdiroSim.Scenario.Sampler.SegmentReplacementSurfaceType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerSegmentReplacementSurfaceTypeTest::RunTest(const FString& Parameters)
{
	FScenarioDocument ScenarioDocument = MakeSamplerTestScenario();
	ScenarioDocument.Corridor.Segments[0].ReplacedBySurfaceId = MakeSamplerTestFixedString(TEXT("building"));

	const FScenarioSamplerResult Result =
		FScenarioSampler::GenerateSample(ScenarioDocument, MakeSamplerTestRequest());

	TestTrue(TEXT("sampler succeeds"), Result.bSuccess);
	TestEqual(TEXT("layout count"), Result.Document.Scenario.Semantic.Layout.Num(), 1);
	if (Result.Document.Scenario.Semantic.Layout.IsEmpty())
	{
		return false;
	}

	const FScenarioSampleLayoutLane* SampleWalkwayLane =
		Result.Document.Scenario.Semantic.Layout[0].Lanes.FindByPredicate(
			[](const FScenarioSampleLayoutLane& Lane)
			{
				return Lane.LaneId == TEXT("walkway");
			});
	TestNotNull(TEXT("sample walkway lane"), SampleWalkwayLane);
	if (!SampleWalkwayLane)
	{
		return false;
	}

	TestEqual(TEXT("sample replacement surface"), SampleWalkwayLane->SurfaceId, FString(TEXT("building")));
	TestEqual(
		TEXT("sample replacement traversability"),
		static_cast<int32>(SampleWalkwayLane->Type),
		static_cast<int32>(EScenarioSampleLaneType::Blocked));

	const FScenarioCompileResult CompileResult =
		FScenarioSampleWorldSpecAdapter::CompileScenarioWorldSpecFromSampleDocument(Result.Document);
	TestTrue(TEXT("sample adapts to world spec"), CompileResult.bSuccess);
	TestEqual(TEXT("runtime corridor count"), CompileResult.WorldSpec.Corridors.Num(), 1);
	if (CompileResult.WorldSpec.Corridors.IsEmpty() || CompileResult.WorldSpec.Corridors[0].Layout.IsEmpty())
	{
		return false;
	}

	const FScenarioRuntimeCorridorLaneSpec* RuntimeWalkwayLane =
		CompileResult.WorldSpec.Corridors[0].Layout[0].Lanes.FindByPredicate(
			[](const FScenarioRuntimeCorridorLaneSpec& Lane)
			{
				return Lane.LaneId == TEXT("walkway");
			});
	TestNotNull(TEXT("runtime walkway lane"), RuntimeWalkwayLane);
	if (!RuntimeWalkwayLane)
	{
		return false;
	}

	TestEqual(TEXT("runtime replacement surface"), RuntimeWalkwayLane->SurfaceId, FString(TEXT("building")));
	TestEqual(
		TEXT("runtime replacement region type"),
		static_cast<int32>(RuntimeWalkwayLane->RegionType),
		static_cast<int32>(EScenarioGroundRegionType::Blocked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerDeterministicRangeTest,
	"OdiroSim.Scenario.Sampler.DeterministicRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerDeterministicRangeTest::RunTest(const FString& Parameters)
{
	const FScenarioDocument ScenarioDocument = MakeSamplerTestScenarioWithRange();
	const FScenarioSamplerRequest Request = MakeSamplerTestRequest();

	const FScenarioSamplerResult FirstResult =
		FScenarioSampler::GenerateSample(ScenarioDocument, Request);
	const FScenarioSamplerResult SecondResult =
		FScenarioSampler::GenerateSample(ScenarioDocument, Request);

	TestTrue(TEXT("first sampler succeeds"), FirstResult.bSuccess);
	TestTrue(TEXT("second sampler succeeds"), SecondResult.bSuccess);

	FString FirstJson;
	FString SecondJson;
	TArray<FScenarioSchemaDiagnostic> FirstDiagnostics;
	TArray<FScenarioSchemaDiagnostic> SecondDiagnostics;
	TestTrue(TEXT("first sample writes"), FScenarioSampleJson::TryWriteJson(FirstResult.Document, FirstJson, FirstDiagnostics));
	TestTrue(TEXT("second sample writes"), FScenarioSampleJson::TryWriteJson(SecondResult.Document, SecondJson, SecondDiagnostics));
	TestEqual(TEXT("same seed produces same sample JSON"), FirstJson, SecondJson);
	TestTrue(TEXT("range param captured"), FirstResult.Document.Scenario.Params.Contains(TEXT("corridor.walkway_width_m")));
	TestTrue(TEXT("obstacle range param captured"), FirstResult.Document.Scenario.Params.Contains(TEXT("obstacles.crate_curb.at.along_m")));
	const FScenarioSampleParamValue* WalkwayWidthParam =
		FirstResult.Document.Scenario.Params.Find(TEXT("corridor.walkway_width_m"));
	TestNotNull(TEXT("walkway width param"), WalkwayWidthParam);
	if (WalkwayWidthParam)
	{
		TestEqual(TEXT("walkway width matches editor projection"), WalkwayWidthParam->FloatValue, 2.5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerRejectsObstacleOutsideSegmentTest,
	"OdiroSim.Scenario.Sampler.RejectsObstacleOutsideSegment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerRejectsObstacleOutsideSegmentTest::RunTest(const FString& Parameters)
{
	FScenarioDocument ScenarioDocument = MakeSamplerTestScenario();
	ScenarioDocument.Obstacles.Placements[0].At.AlongMeters = MakeSamplerTestFixedNumber(12.0);

	const FScenarioSamplerResult Result =
		FScenarioSampler::GenerateSample(ScenarioDocument, MakeSamplerTestRequest());

	TestFalse(TEXT("sampler rejects outside-segment obstacle"), Result.bSuccess);
	TestTrue(
		TEXT("outside segment diagnostic"),
		HasSamplerDiagnostic(Result, TEXT("obstacle_along_outside_segment")));
	TestEqual(
		TEXT("invalid obstacle omitted"),
		Result.Document.Scenario.Semantic.StaticObstacles.Num(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerRejectsObstacleOutsideCorridorSurfaceTest,
	"OdiroSim.Scenario.Sampler.RejectsObstacleOutsideCorridorSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerRejectsObstacleOutsideCorridorSurfaceTest::RunTest(const FString& Parameters)
{
	FScenarioDocument ScenarioDocument = MakeSamplerTestScenario();
	ScenarioDocument.Obstacles.Placements[0].At.OffsetMeters = MakeSamplerTestFixedNumber(2.5);

	const FScenarioSamplerResult Result =
		FScenarioSampler::GenerateSample(ScenarioDocument, MakeSamplerTestRequest());

	TestFalse(TEXT("sampler rejects outside-surface obstacle"), Result.bSuccess);
	TestTrue(
		TEXT("outside corridor surface diagnostic"),
		HasSamplerDiagnostic(Result, TEXT("obstacle_outside_corridor_surface")));
	TestEqual(
		TEXT("invalid obstacle omitted"),
		Result.Document.Scenario.Semantic.StaticObstacles.Num(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerRejectsObstacleOnBlockedSurfaceTest,
	"OdiroSim.Scenario.Sampler.RejectsObstacleOnBlockedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerRejectsObstacleOnBlockedSurfaceTest::RunTest(const FString& Parameters)
{
	FScenarioDocument ScenarioDocument = MakeSamplerTestScenario();
	ScenarioDocument.Obstacles.Placements[0].At.LaneId = TEXT("building_edge");
	ScenarioDocument.Obstacles.Placements[0].At.OffsetMeters = MakeSamplerTestFixedNumber(-0.25);

	const FScenarioSamplerResult Result =
		FScenarioSampler::GenerateSample(ScenarioDocument, MakeSamplerTestRequest());

	TestFalse(TEXT("sampler rejects blocked-surface obstacle"), Result.bSuccess);
	TestTrue(
		TEXT("blocked surface diagnostic"),
		HasSamplerDiagnostic(Result, TEXT("obstacle_on_blocked_surface")));
	TestEqual(
		TEXT("invalid obstacle omitted"),
		Result.Document.Scenario.Semantic.StaticObstacles.Num(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSamplerVersionMismatchTest,
	"OdiroSim.Scenario.Sampler.VersionMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSamplerVersionMismatchTest::RunTest(const FString& Parameters)
{
	FScenarioDocument ScenarioDocument = MakeSamplerTestScenario();
	ScenarioDocument.Version = FScenarioDocumentJson::SupportedVersion + 1;

	const FScenarioSamplerResult Result =
		FScenarioSampler::GenerateSample(ScenarioDocument, MakeSamplerTestRequest());

	TestFalse(TEXT("version mismatch fails"), Result.bSuccess);
	TestTrue(TEXT("unsupported version diagnostic"), Result.Diagnostics.ContainsByPredicate(
		[](const FScenarioSchemaDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("unsupported_schema_version")
				&& Diagnostic.Severity == EScenarioSchemaDiagnosticSeverity::Error;
		}));
	return true;
}

#endif
