#if WITH_DEV_AUTOMATION_TESTS

#include "Scenario/ScenarioTemplateSampler.h"

#include "Misc/AutomationTest.h"
#include "Scenario/ScenarioSampleWorldSpecAdapter.h"
#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioTemplateJson.h"

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

	FScenarioTemplateSampleRequest MakeSamplerTestRequest()
	{
		FScenarioTemplateSampleRequest Request;
		Request.SampleId = TEXT("000123");
		Request.ScenarioId = TEXT("fixed_curb_obstacle_000123");
		Request.Seed = 123;
		Request.TemplateRef = TEXT("templates/scenarios/fixed_curb_obstacle.template.json");
		Request.TemplateHash = TEXT("sha256:template123");
		Request.ProfileRef = TEXT("experiments/test/profile.json");
		Request.ProfileHash = TEXT("sha256:profile123");
		Request.SettingRef = TEXT("experiments/test/setting.json");
		Request.SettingHash = TEXT("sha256:setting123");
		Request.GeneratorVersion = TEXT("scenario_template_sampler_v1");
		return Request;
	}

	FScenarioTemplateDocument MakeSamplerTestTemplate()
	{
		FScenarioTemplateDocument Document;
		Document.TemplateId = TEXT("fixed_curb_obstacle");
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

	FScenarioTemplateDocument MakeSamplerTestTemplateWithRange()
	{
		FScenarioTemplateDocument Document = MakeSamplerTestTemplate();
		Document.Corridor.WalkwayWidthMeters = MakeSamplerTestRangeNumber(2.0, 3.0);
		Document.Obstacles.Placements[0].At.AlongMeters = MakeSamplerTestRangeNumber(3.0, 5.0);
		return Document;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioTemplateSamplerFixedObstacleTest,
	"OdiroSim.ScenarioTemplate.Sampler.FixedObstacle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioTemplateSamplerFixedObstacleTest::RunTest(const FString& Parameters)
{
	const FScenarioTemplateSampleResult Result =
		FScenarioTemplateSampler::GenerateSample(MakeSamplerTestTemplate(), MakeSamplerTestRequest());

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
	TestEqual(TEXT("runtime ground regions"), CompileResult.WorldSpec.GroundRegions.Num(), 3);
	TestEqual(TEXT("runtime placeables"), CompileResult.WorldSpec.Placeables.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioTemplateSamplerDeterministicRangeTest,
	"OdiroSim.ScenarioTemplate.Sampler.DeterministicRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioTemplateSamplerDeterministicRangeTest::RunTest(const FString& Parameters)
{
	const FScenarioTemplateDocument TemplateDocument = MakeSamplerTestTemplateWithRange();
	const FScenarioTemplateSampleRequest Request = MakeSamplerTestRequest();

	const FScenarioTemplateSampleResult FirstResult =
		FScenarioTemplateSampler::GenerateSample(TemplateDocument, Request);
	const FScenarioTemplateSampleResult SecondResult =
		FScenarioTemplateSampler::GenerateSample(TemplateDocument, Request);

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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioTemplateSamplerVersionMismatchTest,
	"OdiroSim.ScenarioTemplate.Sampler.VersionMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioTemplateSamplerVersionMismatchTest::RunTest(const FString& Parameters)
{
	FScenarioTemplateDocument TemplateDocument = MakeSamplerTestTemplate();
	TemplateDocument.Version = FScenarioTemplateJson::SupportedVersion + 1;

	const FScenarioTemplateSampleResult Result =
		FScenarioTemplateSampler::GenerateSample(TemplateDocument, MakeSamplerTestRequest());

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
