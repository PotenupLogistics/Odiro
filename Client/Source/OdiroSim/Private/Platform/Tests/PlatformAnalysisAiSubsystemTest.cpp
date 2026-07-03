#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/PlatformAnalysisAiSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/SimulationSetupTypes.h"

namespace
{
	FString MakeAnalysisAiTestDirectory()
	{
		const FString Directory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("PlatformAnalysisAi"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*Directory, true);
		return Directory;
	}

	bool SaveAnalysisAiTestFile(const FString& FilePath, const FString& Contents)
	{
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

	bool WriteAnalysisProjectRunSnapshot(const FString& ProjectPath, FUserProjectRunSnapshotPaths& OutPaths)
	{
		OutPaths = FUserProjectRunSnapshot::BuildPaths(ProjectPath, TEXT("000001"));
		IFileManager::Get().MakeDirectory(*OutPaths.ReviewPath, true);
		IFileManager::Get().MakeDirectory(*OutPaths.EpisodesPath, true);
		IFileManager::Get().MakeDirectory(*OutPaths.PolicyPath, true);

		return SaveAnalysisAiTestFile(
				OutPaths.SettingPath,
				TEXT("{")
				TEXT("\"schema\":\"project_setting\",")
				TEXT("\"version\":1,")
				TEXT("\"project_id\":\"analysis_project\",")
				TEXT("\"sampling\":{\"base_seed\":1,\"episode_count\":1,\"generator_version\":\"0.1.0\"},")
				TEXT("\"runtime\":{\"map_id\":\"ScenarioSimulationMap\",\"fixed_fps\":30,\"time_scale\":1.0,\"max_duration_s\":60},")
				TEXT("\"evaluation\":{\"goal_acceptance_radius_m\":1.0,\"tip_over_angle_deg\":60,\"near_miss_distance_m\":0.5}")
				TEXT("}"))
			&& SaveAnalysisAiTestFile(
				OutPaths.ProfilePath,
				TEXT("{\"schema\":\"simulation_profile\",\"version\":1,\"profile_id\":\"analysis_profile\",\"robot\":{\"body\":{},\"drive\":{},\"lidar\":{}}}"))
			&& SaveAnalysisAiTestFile(
				OutPaths.ScenarioPath,
				TEXT("{")
				TEXT("\"schema\":\"scenario\",")
				TEXT("\"version\":1,")
				TEXT("\"scenario_id\":\"analysis_scenario\",")
				TEXT("\"intent\":\"analysis\",")
				TEXT("\"corridor\":{\"axis\":{\"type\":\"polyline\",\"points_m\":[[0.0,0.0],[10.0,0.0]]},\"walkway_width_m\":3.0,\"segments\":[{\"id\":\"main\",\"type\":\"straight\",\"along_range_m\":[0.0,10.0]}]},")
				TEXT("\"obstacles\":{},")
				TEXT("\"pedestrians\":{},")
				TEXT("\"robot\":{\"start\":{\"type\":\"entry\"},\"goal\":{\"type\":\"exit\"}}")
				TEXT("}"))
			&& SaveAnalysisAiTestFile(
				OutPaths.PolicyEntrypointPath,
				TEXT("def create_policy():\n    return None\n"))
			&& SaveAnalysisAiTestFile(
				OutPaths.SummaryPath,
				TEXT("{\"schema\":\"run_summary\",\"version\":1,\"run\":{\"run_id\":\"000001\"},\"rows\":[]}"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformAnalysisAiProjectRunRequestJsonBuildTest,
	"OdiroSim.Platform.AnalysisAi.ProjectRunRequestJsonBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformAnalysisAiProjectRunRequestJsonBuildTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ProjectPath = MakeAnalysisAiTestDirectory();
	FUserProjectRunSnapshotPaths Paths;
	TestTrue(TEXT("write project run snapshot"), WriteAnalysisProjectRunSnapshot(ProjectPath, Paths));

	FString RequestJson;
	TArray<FString> Diagnostics;
	TestTrue(
		TEXT("project run request json builds"),
		UPlatformAnalysisAiSubsystem::BuildAnalysisRequestJsonForProjectRun(
			ProjectPath,
			TEXT("000001"),
			RequestJson,
			Diagnostics));
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);

	TSharedPtr<FJsonObject> RequestObject;
	TestTrue(TEXT("request json parses"), ParseJsonObject(RequestJson, RequestObject));
	if (!RequestObject.IsValid())
	{
		return false;
	}

	FString RequestProjectPath;
	FString RequestRunId;
	TestTrue(TEXT("has project path"), RequestObject->TryGetStringField(TEXT("project_path"), RequestProjectPath));
	TestTrue(TEXT("has run id"), RequestObject->TryGetStringField(TEXT("run_id"), RequestRunId));
	TestEqual(TEXT("project path"), RequestProjectPath, Paths.ProjectPath);
	TestEqual(TEXT("run id"), RequestRunId, FString(TEXT("000001")));
	TestFalse(TEXT("no measurement log path"), RequestObject->HasField(TEXT("measurement_log_path")));

	IFileManager::Get().DeleteDirectory(*ProjectPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlatformAnalysisAiDisplayTextTest,
	"OdiroSim.Platform.AnalysisAi.DisplayText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlatformAnalysisAiDisplayTextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ResponseJson = TEXT(R"({
		"schema": "result_analysis_response",
		"version": 2,
		"status": "ok",
		"review_id": "0002",
		"run_id": "000005",
		"run_overview": {
			"display": {
				"total_play_time": "42.0초",
				"success_rate": "25%",
				"collision_count": "152회"
			}
		},
		"episodes": [
			{
				"episode_id": "000001",
				"display": {
					"duration": "10.5초",
					"outcome": "실패"
				}
			}
		],
		"summary": {
			"overall_judgement": "change_recommended",
			"message": "환경 또는 장애물 배치 검토가 필요합니다."
		},
		"insights": [
			{
				"severity": "high",
				"title": "정적 장애물 충돌 반복",
				"description": "정적 장애물 충돌이 반복되었습니다."
			}
		],
		"recommendations": [
			{
				"id": "REC-001",
				"target": "environment",
				"priority": "high",
				"title": "정적 장애물 배치와 통로 폭 검토",
				"reason": "정적 장애물 충돌이 반복되었습니다.",
				"recommendation": "최소 통로 폭을 늘린 환경 수정 후보로 재실행하세요."
			}
		],
		"warnings": [
			"skipped large file: runs/000005/episodes/000001/actions.jsonl",
			"runs/000005/episodes/000001/events.jsonl is missing."
		]
	})");

	TArray<FString> Diagnostics;
	const FString DisplayText = UPlatformAnalysisAiSubsystem::BuildDisplayTextFromAnalysisResponse(
		ResponseJson,
		Diagnostics);
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);
	TestTrue(TEXT("contains summary message"), DisplayText.Contains(TEXT("환경 또는 장애물")));
	TestTrue(TEXT("contains run overview display"), DisplayText.Contains(TEXT("42.0초")));
	TestTrue(TEXT("contains insight"), DisplayText.Contains(TEXT("충돌 반복")));
	TestTrue(TEXT("contains episode display"), DisplayText.Contains(TEXT("10.5초")));
	TestTrue(TEXT("contains v2 recommendation title"), DisplayText.Contains(TEXT("정적 장애물 배치")));
	TestTrue(TEXT("contains v2 recommendation text"), DisplayText.Contains(TEXT("최소 통로 폭")));
	TestFalse(TEXT("hides internal large file warning"), DisplayText.Contains(TEXT("skipped large file")));
	TestTrue(TEXT("contains actionable v2 warning"), DisplayText.Contains(TEXT("events.jsonl is missing")));

	const FString LegacyResponseJson = TEXT(R"({
		"schemaVersion": "1.0.0",
		"analysisId": "analysis-001",
		"generationMethod": "fallback_rules",
		"summary": "fallback 규칙 기반으로 추천을 생성했습니다.",
		"botSetupRecommendations": [
			{
				"param": "stop_distance_m",
				"current": 1.2,
				"suggested": 1.5,
				"reason": "near miss 때문에 정지 거리를 늘립니다."
			}
		],
		"llmWarnings": ["fallback_only=True"]
	})");

	TArray<FString> LegacyDiagnostics;
	const FString LegacyDisplayText = UPlatformAnalysisAiSubsystem::BuildDisplayTextFromAnalysisResponse(
		LegacyResponseJson,
		LegacyDiagnostics);
	TestEqual(TEXT("legacy diagnostics"), LegacyDiagnostics.Num(), 0);
	TestTrue(TEXT("legacy contains summary"), LegacyDisplayText.Contains(TEXT("fallback 규칙 기반")));
	TestTrue(TEXT("legacy contains recommendation"), LegacyDisplayText.Contains(TEXT("stop_distance_m")));
	TestTrue(TEXT("legacy contains warning"), LegacyDisplayText.Contains(TEXT("fallback_only=True")));

	return true;
}

#endif
