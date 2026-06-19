#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/ScenarioSampleJson.h"
#include "Shared/ScenarioDocumentJson.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
	bool HasScenarioSchemaDiagnosticCode(
		const TArray<FScenarioSchemaDiagnostic>& Diagnostics,
		const FString& Code)
	{
		return Diagnostics.ContainsByPredicate(
			[&Code](const FScenarioSchemaDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == Code;
			});
	}

	FScenarioTemplateNumberValue MakeTestTemplateNumber(double Value)
	{
		FScenarioTemplateNumberValue NumberValue;
		NumberValue.bIsSet = true;
		NumberValue.Mode = EScenarioTemplateNumberValueMode::Fixed;
		NumberValue.FixedValue = Value;
		return NumberValue;
	}

	FScenarioDocument MakeMinimalScenarioDocument()
	{
		FScenarioDocument Document;
		Document.ScenarioId = TEXT("pinch_oncoming_low_coop");
		Document.Intent = TEXT("Validate a narrow corridor preview scenario.");
		Document.Corridor.Axis.PointsMeters = { FVector2D(0.0, 0.0), FVector2D(12.0, 0.0) };
		Document.Corridor.WalkwayWidthMeters = MakeTestTemplateNumber(2.5);

		FScenarioTemplateLaneRule BuildingLane;
		BuildingLane.SurfaceId = TEXT("building");
		BuildingLane.WidthMeters = MakeTestTemplateNumber(1.0);
		Document.Corridor.BuildingSide.Add(BuildingLane);

		FScenarioTemplateLaneRule CurbLane;
		CurbLane.SurfaceId = TEXT("road");
		CurbLane.WidthMeters = MakeTestTemplateNumber(1.0);
		Document.Corridor.CurbSide.Add(CurbLane);

		FScenarioTemplateSegment Segment;
		Segment.SegmentId = TEXT("entry");
		Segment.Type = EScenarioTemplateSegmentType::Straight;
		Segment.AlongRangeMeters.StartMeters = 0.0;
		Segment.AlongRangeMeters.EndMeters = 12.0;
		Document.Corridor.Segments.Add(Segment);

		Document.Obstacles.MinClearWidthMeters = MakeTestTemplateNumber(1.2);
		Document.Robot.Start.Type = EScenarioTemplateRobotAnchorType::Entry;
		Document.Robot.Goal.Type = EScenarioTemplateRobotAnchorType::Exit;
		return Document;
	}

	FScenarioSampleParamValue MakeTestSampleFloatParam(double Value)
	{
		FScenarioSampleParamValue ParamValue;
		ParamValue.Type = EScenarioSampleParamValueType::Float;
		ParamValue.FloatValue = Value;
		return ParamValue;
	}

	FScenarioSampleDocument MakeMinimalSampleDocument()
	{
		FScenarioSampleDocument Document;
		Document.Sample.SampleId = TEXT("000001");
		Document.Sample.ScenarioId = TEXT("pinch_oncoming_low_coop_000001");
		Document.Sample.Source.TemplateRef = TEXT("runs/test/snapshot/scenario.json");
		Document.Sample.Source.TemplateHash = TEXT("sha256:scenariohash0001");
		Document.Sample.Source.ProfileRef = TEXT("experiments/E/profile.json");
		Document.Sample.Source.ProfileHash = TEXT("sha256:profilehash0001");
		Document.Sample.Source.SettingRef = TEXT("experiments/E/setting.json");
		Document.Sample.Source.SettingHash = TEXT("sha256:settinghash0001");
		Document.Sample.Source.Seed = 3007;
		Document.Sample.Source.GeneratorVersion = TEXT("0.1.0");

		Document.Scenario.Params.Add(TEXT("corridor.walkway_width_m"), MakeTestSampleFloatParam(2.7));
		Document.Scenario.Semantic.RouteAxis.OriginXYMeters = FVector2D::ZeroVector;
		Document.Scenario.Semantic.RouteAxis.PointsMeters = { FVector2D(0.0, 0.0), FVector2D(10.0, 0.0) };
		Document.Scenario.Semantic.RouteAxis.LengthMeters = 10.0;

		Document.Scenario.Semantic.Robot.Start.SegmentId = TEXT("entry");
		Document.Scenario.Semantic.Robot.Start.LaneId = TEXT("walkway");
		Document.Scenario.Semantic.Robot.Start.SourceAnchorType = EScenarioTemplateRobotAnchorType::Entry;
		Document.Scenario.Semantic.Robot.Goal.SegmentId = TEXT("exit");
		Document.Scenario.Semantic.Robot.Goal.AlongMeters = 10.0;
		Document.Scenario.Semantic.Robot.Goal.LaneId = TEXT("walkway");
		Document.Scenario.Semantic.Robot.Goal.SourceAnchorType = EScenarioTemplateRobotAnchorType::Exit;

		FScenarioSampleLayoutLane Lane;
		Lane.LaneId = TEXT("walkway");
		Lane.OffsetRangeMeters.MinMeters = -1.0;
		Lane.OffsetRangeMeters.MaxMeters = 1.0;
		Lane.SurfaceId = TEXT("sidewalk");
		Lane.Type = EScenarioSampleLaneType::Walkable;

		FScenarioSampleLayoutEntry LayoutEntry;
		LayoutEntry.AlongRangeMeters.StartMeters = 0.0;
		LayoutEntry.AlongRangeMeters.EndMeters = 10.0;
		LayoutEntry.SegmentId = TEXT("entry");
		LayoutEntry.Lanes.Add(Lane);
		Document.Scenario.Semantic.Layout.Add(LayoutEntry);

		FScenarioSampleClearWidthEntry ClearWidth;
		ClearWidth.AlongRangeMeters.StartMeters = 0.0;
		ClearWidth.AlongRangeMeters.EndMeters = 10.0;
		ClearWidth.ClearWidthMeters = 2.0;
		ClearWidth.LimitedBy = TEXT("none");
		Document.Scenario.Semantic.ClearWidthProfile.Add(ClearWidth);

		Document.Scenario.Semantic.Summary.GlobalMinClearWidthMeters = 2.0;
		Document.Scenario.Semantic.Summary.MinClearAtAlongMeters = 3.0;
		Document.Scenario.Semantic.Summary.TotalLengthMeters = 10.0;
		return Document;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioDocumentJsonParseValidTest,
	"OdiroSim.ScenarioDocument.Json.ParseValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioDocumentJsonParseValidTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"json(
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "pinch_oncoming_low_coop",
  "intent": "Validate a narrow corridor preview scenario.",
  "corridor": {
    "axis": { "type": "polyline", "points_m": [[0, 0], [12, 0]] },
    "walkway_width_m": 2.5,
    "building_side": [{ "surface": "building", "width_m": 1.0 }],
    "curb_side": [{ "surface": "road", "width_m": 1.0 }],
    "segments": [{ "id": "entry", "type": "straight", "along_range_m": [0, 12] }]
  },
  "obstacles": {
    "min_clear_width_m": 1.2,
    "placements": [
      {
        "id": "crate_1",
        "kind": "fixed",
        "prop": "crate",
        "at": { "segment": "entry", "along_m": 4.0, "offset_m": 0.0, "lane": "walkway" },
        "yaw_deg": 0.0,
        "allow_blocking": false
      }
    ]
  },
  "pedestrians": { "background": { "count": 0, "speed_mps": 1.0 }, "encounters": [] },
  "robot": { "start": { "type": "entry" }, "goal": { "type": "exit" } }
}
)json");

	const FScenarioDocumentParseResult Result = FScenarioDocumentJson::ParseFromString(Json);
	TestTrue(TEXT("scenario parses"), Result.bSuccess);
	TestEqual(TEXT("scenario diagnostics"), Result.Diagnostics.Num(), 0);
	TestEqual(TEXT("scenario id"), Result.Document.ScenarioId, FString(TEXT("pinch_oncoming_low_coop")));
	TestEqual(TEXT("segment count"), Result.Document.Corridor.Segments.Num(), 1);
	TestEqual(TEXT("placement prop"), Result.Document.Obstacles.Placements[0].PropId, FString(TEXT("crate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioDocumentJsonInvalidProjectScenarioRejectedTest,
	"OdiroSim.ScenarioDocument.Json.InvalidProjectScenarioRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioDocumentJsonInvalidProjectScenarioRejectedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Json = TEXT(R"json(
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "invalid_blank_sidewalk",
  "intent": "Invalid project scenario.",
  "corridor": {
    "segments": [
      {
        "id": "main",
        "along_range_m": [0.0, 12.0]
      }
    ],
    "building_side": [{ "surface": "wall", "width_m": 0.5 }],
    "curb_side": [{ "surface": "road", "width_m": 4.0 }]
  },
  "obstacles": { "min_clear_width_m": 0.9, "placements": [] },
  "pedestrians": { "background": { "count": 0 }, "encounters": [] },
  "robot": {
    "start": { "segment": "main", "along_m": 1.0, "offset_m": 0.0 },
    "goal": { "segment": "main", "along_m": 11.0, "offset_m": 0.0 }
  }
}
)json");

	const FScenarioDocumentParseResult Result = FScenarioDocumentJson::ParseProjectScenarioFromString(Json);
	TestFalse(TEXT("invalid project scenario is rejected"), Result.bSuccess);
	TestTrue(TEXT("missing axis diagnostic"), HasScenarioSchemaDiagnosticCode(Result.Diagnostics, TEXT("missing_axis")));
	TestTrue(TEXT("missing walkway width diagnostic"), HasScenarioSchemaDiagnosticCode(Result.Diagnostics, TEXT("missing_walkway_width_m")));
	TestTrue(TEXT("missing robot anchor type diagnostic"), HasScenarioSchemaDiagnosticCode(Result.Diagnostics, TEXT("missing_type")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioDocumentJsonProjectPresetFilesParseTest,
	"OdiroSim.ScenarioDocument.Json.ProjectPresetFilesParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioDocumentJsonProjectPresetFilesParseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString presetRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("static"), TEXT("templates"), TEXT("scenario")));
	const FString presetIds[] = { TEXT("blank"), TEXT("curved-road"), TEXT("demo") };
	for (const FString& presetId : presetIds)
	{
		const FString scenarioPath = FPaths::Combine(presetRoot, FString::Printf(TEXT("%s.json"), *presetId));
		const FScenarioDocumentParseResult result = FScenarioDocumentJson::ParseProjectScenarioFromFile(scenarioPath);
		TestTrue(FString::Printf(TEXT("%s scenario parses"), *presetId), result.bSuccess);
		TestEqual(FString::Printf(TEXT("%s diagnostics"), *presetId), result.Diagnostics.Num(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioDocumentJsonVersionMismatchTest,
	"OdiroSim.ScenarioDocument.Json.VersionMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioDocumentJsonVersionMismatchTest::RunTest(const FString& Parameters)
{
	FScenarioDocument Document = MakeMinimalScenarioDocument();
	Document.Version = FScenarioDocumentJson::SupportedVersion + 1;

	TArray<FScenarioSchemaDiagnostic> Diagnostics;
	TestFalse(TEXT("version mismatch fails"), FScenarioDocumentJson::ValidateDocument(Document, Diagnostics));
	TestTrue(TEXT("unsupported version diagnostic"), HasScenarioSchemaDiagnosticCode(Diagnostics, TEXT("unsupported_schema_version")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioDocumentJsonRoundTripTest,
	"OdiroSim.ScenarioDocument.Json.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioDocumentJsonRoundTripTest::RunTest(const FString& Parameters)
{
	const FScenarioDocument Document = MakeMinimalScenarioDocument();

	FString Json;
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
	TestTrue(TEXT("scenario writes"), FScenarioDocumentJson::TryWriteJson(Document, Json, Diagnostics));
	TestEqual(TEXT("write diagnostics"), Diagnostics.Num(), 0);

	const FScenarioDocumentParseResult Result = FScenarioDocumentJson::ParseFromString(Json);
	TestTrue(TEXT("scenario roundtrip parses"), Result.bSuccess);
	TestEqual(TEXT("roundtrip scenario id"), Result.Document.ScenarioId, Document.ScenarioId);
	TestEqual(TEXT("roundtrip walkway width"), Result.Document.Corridor.WalkwayWidthMeters.FixedValue, 2.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioProjectScenarioJsonRoundTripTest,
	"OdiroSim.ScenarioDocument.Json.ProjectScenarioRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioProjectScenarioJsonRoundTripTest::RunTest(const FString& Parameters)
{
	const FScenarioDocument Document = MakeMinimalScenarioDocument();

	FString Json;
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
	TestTrue(TEXT("project scenario writes"), FScenarioDocumentJson::TryWriteProjectScenarioJson(Document, Json, Diagnostics));
	TestEqual(TEXT("project write diagnostics"), Diagnostics.Num(), 0);
	TestTrue(TEXT("project scenario schema field"), Json.Contains(TEXT("\"schema\": \"scenario\"")));
	TestTrue(TEXT("project scenario id field"), Json.Contains(TEXT("\"scenario_id\": \"pinch_oncoming_low_coop\"")));
	TestFalse(TEXT("project scenario omits template id field"), Json.Contains(TEXT("\"template_id\"")));

	const FScenarioDocumentParseResult Result = FScenarioDocumentJson::ParseProjectScenarioFromString(Json);
	TestTrue(TEXT("project scenario roundtrip parses"), Result.bSuccess);
	TestEqual(TEXT("project roundtrip diagnostics"), Result.Diagnostics.Num(), 0);
	TestEqual(TEXT("project roundtrip scenario id"), Result.Document.ScenarioId, Document.ScenarioId);
	TestEqual(TEXT("project roundtrip keeps scenario schema"), Result.Document.Schema, FString(TEXT("scenario")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleJsonParseValidTest,
	"OdiroSim.ScenarioSample.Json.ParseValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleJsonParseValidTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"json(
{
  "schema": "scenario_sample",
  "version": 1,
  "sample": {
    "sample_id": "000001",
    "scenario_id": "pinch_oncoming_low_coop_000001",
    "source": {
      "template_ref": "runs/test/snapshot/scenario.json",
      "template_hash": "sha256:scenariohash0001",
      "profile_ref": "experiments/E/profile.json",
      "profile_hash": "sha256:profilehash0001",
      "setting_ref": "experiments/E/setting.json",
      "setting_hash": "sha256:settinghash0001",
      "seed": 3007,
      "generator_version": "0.1.0"
    }
  },
  "scenario": {
    "params": {
      "corridor.walkway_width_m": 2.7,
      "pedestrians.background.count": 2
    },
    "semantic": {
      "route_axis": {
        "type": "polyline",
        "origin_xy_m": [0, 0],
        "heading_deg": 0,
        "points_m": [[0, 0], [10, 0]],
        "length_m": 10
      },
      "robot": {
        "start": { "segment": "entry", "along_m": 0, "offset_m": 0, "lane": "walkway", "heading_deg": 0, "source_anchor_type": "entry" },
        "goal": { "segment": "exit", "along_m": 10, "offset_m": 0, "lane": "walkway", "heading_deg": 0, "source_anchor_type": "exit" }
      },
      "layout": [
        {
          "along_range_m": [0, 10],
          "segment": "entry",
          "lanes": [{ "lane": "walkway", "offset_range_m": [-1, 1], "surface": "sidewalk", "type": "walkable" }]
        }
      ],
      "static_obstacles": [],
      "pedestrians": [],
      "clear_width_profile": [{ "along_range_m": [0, 10], "clear_width_m": 2.0, "limited_by": "none" }],
      "summary": {
        "global_min_clear_width_m": 2.0,
        "min_clear_at_along_m": 3.0,
        "total_length_m": 10,
        "encounter_in_min_clear_zone": false
      }
    }
  },
  "validation": { "edited_by_user": false, "diagnostics": [] }
}
)json");

	const FScenarioSampleParseResult Result = FScenarioSampleJson::ParseFromString(Json);
	TestTrue(TEXT("sample parses"), Result.bSuccess);
	TestEqual(TEXT("sample diagnostics"), Result.Diagnostics.Num(), 0);
	TestEqual(TEXT("sample id"), Result.Document.Sample.SampleId, FString(TEXT("000001")));
	TestEqual(TEXT("route axis length"), Result.Document.Scenario.Semantic.RouteAxis.LengthMeters, 10.0);
	TestTrue(TEXT("float param exists"), Result.Document.Scenario.Params.Contains(TEXT("corridor.walkway_width_m")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleJsonVersionMismatchTest,
	"OdiroSim.ScenarioSample.Json.VersionMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleJsonVersionMismatchTest::RunTest(const FString& Parameters)
{
	FScenarioSampleDocument Document = MakeMinimalSampleDocument();
	Document.Version = FScenarioSampleJson::SupportedVersion + 1;

	TArray<FScenarioSchemaDiagnostic> Diagnostics;
	TestFalse(TEXT("version mismatch fails"), FScenarioSampleJson::ValidateDocument(Document, Diagnostics));
	TestTrue(TEXT("unsupported version diagnostic"), HasScenarioSchemaDiagnosticCode(Diagnostics, TEXT("unsupported_schema_version")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioSampleJsonRoundTripTest,
	"OdiroSim.ScenarioSample.Json.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioSampleJsonRoundTripTest::RunTest(const FString& Parameters)
{
	const FScenarioSampleDocument Document = MakeMinimalSampleDocument();

	FString Json;
	TArray<FScenarioSchemaDiagnostic> Diagnostics;
	TestTrue(TEXT("sample writes"), FScenarioSampleJson::TryWriteJson(Document, Json, Diagnostics));
	TestEqual(TEXT("write diagnostics"), Diagnostics.Num(), 0);

	const FScenarioSampleParseResult Result = FScenarioSampleJson::ParseFromString(Json);
	TestTrue(TEXT("sample roundtrip parses"), Result.bSuccess);
	TestEqual(TEXT("roundtrip sample id"), Result.Document.Sample.SampleId, Document.Sample.SampleId);
	TestEqual(TEXT("roundtrip total length"), Result.Document.Scenario.Semantic.Summary.TotalLengthMeters, 10.0);
	return true;
}

#endif
