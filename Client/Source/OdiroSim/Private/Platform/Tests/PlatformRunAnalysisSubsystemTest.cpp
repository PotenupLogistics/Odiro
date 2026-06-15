#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/PlatformRunAnalysisSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString MakeRunAnalysisTestDirectory()
	{
		const FString Directory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("PlatformRunAnalysis"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*Directory, true);
		return Directory;
	}

	bool SaveRunAnalysisTestFile(const FString& FilePath, const FString& Contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		return FFileHelper::SaveStringToFile(
			Contents,
			*FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	void WriteCanonicalRunArtifacts(
		const FString& RunDirectory,
		const FString& SampleId,
		FString& OutSummaryPath,
		FString& OutResultPath,
		FString& OutEventsPath)
	{
		const FString EpisodeDirectory = FPaths::Combine(RunDirectory, TEXT("episodes"), SampleId);
		OutSummaryPath = FPaths::Combine(RunDirectory, TEXT("summary.json"));
		OutResultPath = FPaths::Combine(EpisodeDirectory, TEXT("result.json"));
		OutEventsPath = FPaths::Combine(EpisodeDirectory, TEXT("events.jsonl"));

		const FString SummaryJson = TEXT(R"({
			"schema": "run_summary",
			"version": 1,
			"run": { "run_id": "run_001", "experiment_id": "analysis_test" },
			"rows": [
				{
					"episode_id": "episode_001",
					"sample_id": "sample_0000",
					"outcome": "Failure",
					"terminal_reason": "StaticObstacleCollision",
					"duration_s": 4.5
				}
			]
		})");

		const FString ResultJson = TEXT(R"({
			"schema": "episode_result",
			"version": 1,
			"sample": {
				"sample_id": "sample_0000",
				"scenario_id": "scenario_001",
				"template_id": "template_001",
				"seed": 42
			},
			"run": {
				"run_id": "run_001",
				"episode_id": "episode_001"
			},
			"summary": {
				"completed": true,
				"success": false,
				"outcome": "Failure",
				"terminal_reason": "StaticObstacleCollision",
				"terminal_event_index": 1,
				"duration_s": 4.5
			},
			"metrics": {
				"static_obstacle_collision_count": 1
			},
			"event_summary": {
				"total": 2,
				"terminal_event_index": 1
			}
		})");

		const FString EventsJsonl = TEXT(
			"{\"schema\":\"episode_event\",\"version\":1,\"event_index\":0,\"run_time_seconds\":1.0,\"source\":\"EvaluationSubsystem\",\"event_type\":\"Repath\",\"reason\":\"blocked_corridor\",\"message\":\"Repath requested.\",\"action_sequence\":null,\"properties\":{}}\n"
			"{\"schema\":\"episode_event\",\"version\":1,\"event_index\":1,\"run_time_seconds\":4.5,\"source\":\"EvaluationSubsystem\",\"event_type\":\"StaticObstacleCollision\",\"reason\":\"collision\",\"message\":\"Robot hit a static obstacle.\",\"action_sequence\":12,\"properties\":{\"target_id\":\"box_01\"}}\n");

		SaveRunAnalysisTestFile(OutSummaryPath, SummaryJson);
		SaveRunAnalysisTestFile(OutResultPath, ResultJson);
		SaveRunAnalysisTestFile(OutEventsPath, EventsJsonl);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformRunAnalysisRequestJsonBuildTest,
	"OdiroSim.Platform.RunAnalysis.RequestJsonBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformRunAnalysisRequestJsonBuildTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Directory = MakeRunAnalysisTestDirectory();
	const FString RunDirectory = FPaths::Combine(Directory, TEXT("runs"), TEXT("run_001"));

	FString SummaryPath;
	FString ResultPath;
	FString EventsPath;
	WriteCanonicalRunArtifacts(RunDirectory, TEXT("sample_0000"), SummaryPath, ResultPath, EventsPath);

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("request json builds"),
		UPlatformRunAnalysisSubsystem::BuildAnalysisRequestJsonFromEpisodeResult(
			ResultPath,
			true,
			RequestJson,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RequestObject;
	TestTrue(TEXT("request json parses"), ParseJsonObject(RequestJson, RequestObject));
	if (!RequestObject.IsValid())
	{
		return false;
	}

	FString Schema;
	FString RequestSummaryPath;
	FString RequestResultPath;
	FString RequestEventsPath;
	TestTrue(TEXT("has request schema"), RequestObject->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("request schema"), Schema, FString(TEXT("run_analysis_request")));
	TestTrue(TEXT("fallback only"), RequestObject->GetBoolField(TEXT("fallback_only")));
	TestTrue(TEXT("has summary path"), RequestObject->TryGetStringField(TEXT("run_summary_path"), RequestSummaryPath));
	TestTrue(TEXT("has result path"), RequestObject->TryGetStringField(TEXT("episode_result_path"), RequestResultPath));
	TestTrue(TEXT("has events path"), RequestObject->TryGetStringField(TEXT("episode_events_path"), RequestEventsPath));
	TestTrue(TEXT("summary path is absolute"), FPaths::IsRelative(RequestSummaryPath) == false);
	TestTrue(TEXT("result path is absolute"), FPaths::IsRelative(RequestResultPath) == false);
	TestTrue(TEXT("events path is absolute"), FPaths::IsRelative(RequestEventsPath) == false);

	const TSharedPtr<FJsonObject>* SummaryObject = nullptr;
	const TSharedPtr<FJsonObject>* ResultObject = nullptr;
	TestTrue(TEXT("has run_summary object"), RequestObject->TryGetObjectField(TEXT("run_summary"), SummaryObject));
	TestTrue(TEXT("has episode_result object"), RequestObject->TryGetObjectField(TEXT("episode_result"), ResultObject));

	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	TestTrue(TEXT("has event array"), RequestObject->TryGetArrayField(TEXT("episode_events"), Events));
	TestEqual(TEXT("event count"), Events ? Events->Num() : 0, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformRunAnalysisStaleAbsoluteResultPathRemapTest,
	"OdiroSim.Platform.RunAnalysis.StaleAbsoluteResultPathRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformRunAnalysisStaleAbsoluteResultPathRemapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString RelativeRunDirectory = FPaths::Combine(
		TEXT("Saved"),
		TEXT("SimulationRuns"),
		TEXT("AutomationRunAnalysis"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits),
		TEXT("runs"),
		TEXT("run_001"));
	const FString RunDirectory = FPaths::Combine(FPaths::ProjectDir(), RelativeRunDirectory);

	FString SummaryPath;
	FString ResultPath;
	FString EventsPath;
	WriteCanonicalRunArtifacts(RunDirectory, TEXT("sample_0000"), SummaryPath, ResultPath, EventsPath);

	const FString ResultTail = FPaths::Combine(
		RelativeRunDirectory,
		TEXT("episodes"),
		TEXT("sample_0000"),
		TEXT("result.json"));
	const FString StaleResultPath = FString::Printf(
		TEXT("C:/Users/old/Documents/Unreal Projects/Odiro/%s"),
		*ResultTail.Replace(TEXT("\\"), TEXT("/")));

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("request json builds from stale absolute path"),
		UPlatformRunAnalysisSubsystem::BuildAnalysisRequestJsonFromEpisodeResult(
			StaleResultPath,
			false,
			RequestJson,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RequestObject;
	TestTrue(TEXT("request json parses"), ParseJsonObject(RequestJson, RequestObject));
	if (!RequestObject.IsValid())
	{
		return false;
	}

	FString RequestResultPath;
	TestTrue(TEXT("has result path"), RequestObject->TryGetStringField(TEXT("episode_result_path"), RequestResultPath));

	FString ExpectedResultPath = ResultPath;
	FPaths::NormalizeFilename(ExpectedResultPath);
	FPaths::CollapseRelativeDirectories(ExpectedResultPath);
	TestEqual(TEXT("result path remapped"), RequestResultPath, ExpectedResultPath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformRunAnalysisMissingEventsTest,
	"OdiroSim.Platform.RunAnalysis.MissingEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformRunAnalysisMissingEventsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Directory = MakeRunAnalysisTestDirectory();
	const FString RunDirectory = FPaths::Combine(Directory, TEXT("runs"), TEXT("run_001"));

	FString SummaryPath;
	FString ResultPath;
	FString EventsPath;
	WriteCanonicalRunArtifacts(RunDirectory, TEXT("sample_0000"), SummaryPath, ResultPath, EventsPath);
	IFileManager::Get().Delete(*EventsPath);

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestFalse(
		TEXT("request json fails without events"),
		UPlatformRunAnalysisSubsystem::BuildAnalysisRequestJsonFromEpisodeResult(
			ResultPath,
			false,
			RequestJson,
			Diagnostics));
	TestTrue(TEXT("has missing events diagnostic"), Diagnostics.ContainsByPredicate(
		[](const FString& Diagnostic)
		{
			return Diagnostic.Contains(TEXT("episode_events_path file not found"));
		}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformRunAnalysisDisplayTextTest,
	"OdiroSim.Platform.RunAnalysis.DisplayText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformRunAnalysisDisplayTextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ResponseJson = TEXT(R"({
		"schema": "run_analysis_response",
		"version": 1,
		"analysis_id": "analysis-001",
		"generation_method": "fallback_rules",
		"summary": "Collision was caused by a narrow static obstacle placement.",
		"episode_statistics": {
			"outcome": "Failure",
			"terminal_reason": "StaticObstacleCollision",
			"duration_s": 4.5
		},
		"findings": [
			{
				"title": "Terminal event",
				"message": "The terminal event points to static obstacle box_01."
			}
		],
		"scenario_template_suggestions": [
			{
				"param": "static_obstacles[0].placement.kind",
				"current": "scatter",
				"suggested": "fixed",
				"reason": "Use a fixed probe obstacle when validating corridor clearance."
			}
		],
		"profile_suggestions": [
			{
				"param": "navigation.stop_distance_m",
				"current": 1.0,
				"suggested": 1.5,
				"reason": "Increase stopping margin for this profile."
			}
		],
		"warnings": ["fallback_only=true"]
	})");

	TArray<FString> Diagnostics;
	const FString DisplayText = UPlatformRunAnalysisSubsystem::BuildDisplayTextFromAnalysisResponse(
		ResponseJson,
		Diagnostics);
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);
	TestTrue(TEXT("contains summary"), DisplayText.Contains(TEXT("Collision was caused")));
	TestTrue(TEXT("contains terminal reason"), DisplayText.Contains(TEXT("StaticObstacleCollision")));
	TestTrue(TEXT("contains template suggestion"), DisplayText.Contains(TEXT("static_obstacles[0].placement.kind")));
	TestTrue(TEXT("contains profile suggestion"), DisplayText.Contains(TEXT("navigation.stop_distance_m")));
	TestTrue(TEXT("contains warning"), DisplayText.Contains(TEXT("fallback_only=true")));

	return true;
}

#endif
