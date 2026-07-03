#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ProjectRunResultDashboard.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectRunResultDashboardSummaryTest,
	"OdiroSim.ProjectRun.ResultDashboard.Summary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectRunResultDashboardSummaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString SummaryJson = TEXT(R"({
		"schema": "run_summary",
		"version": 1,
		"run": { "run_id": "000123", "project_id": "Full" },
		"rows": [
			{
				"episode_id": "000001",
				"outcome": "Success",
				"terminal_reason": "GoalReached",
				"duration_s": 9.7,
				"metrics": {
					"goal_reached": 1,
					"blocked_region_collision_count": 1,
					"pedestrian_collision_count": 0,
					"static_obstacle_collision_count": 0
				}
			},
			{
				"episode_id": "000002",
				"outcome": "Failure",
				"terminal_reason": "StaticObstacleCollision",
				"duration_s": 8.4,
				"metrics": {
					"goal_reached": 0,
					"blocked_region_collision_count": 0,
					"pedestrian_collision_count": 2,
					"static_obstacle_collision_count": 0
				}
			},
			{
				"episode_id": "000003",
				"outcome": "Failure",
				"terminal_reason": "GoalReached",
				"duration_s": 8.4,
				"metrics": {
					"goal_reached": 1,
					"blocked_region_collision_count": 0,
					"pedestrian_collision_count": 0,
					"static_obstacle_collision_count": 3
				}
			},
			{
				"episode_id": "000004",
				"outcome": "Success",
				"terminal_reason": "GoalReached",
				"duration_s": 0,
				"metrics": {
					"goal_reached": 1,
					"goal_threshold_m": 1,
					"blocked_region_collision_count": 0,
					"pedestrian_collision_count": 0,
					"static_obstacle_collision_count": 0
				},
				"scenario_semantic": {
					"robot": {
						"start": {
							"segment": "main",
							"along_m": 1,
							"offset_m": 0
						},
						"goal": {
							"segment": "main",
							"along_m": 2,
							"offset_m": 0
						}
					}
				}
			}
		]
	})");

	FProjectRunResultDashboardData DashboardData;
	TestTrue(
		TEXT("summary dashboard parsed"),
		FProjectRunResultDashboardJson::BuildFromSummaryJsonString(
			SummaryJson,
			TEXT("X:/Odiro/build/test-project/Full/runs/000123"),
			DashboardData));
	TestEqual(TEXT("run id"), DashboardData.RunId, FString(TEXT("000123")));
	TestEqual(TEXT("episode count"), DashboardData.EpisodeCount, 4);
	TestEqual(TEXT("success count excludes immediate goal configuration"), DashboardData.SuccessCount, 2);
	TestEqual(TEXT("collision count"), DashboardData.CollisionCount, 6);
	TestEqual(TEXT("episode items"), DashboardData.Episodes.Num(), 4);
	TestEqual(TEXT("duration sum"), static_cast<int32>(FMath::RoundToInt(DashboardData.TotalDurationSeconds * 10.0)), 265);
	TestFalse(TEXT("immediate goal episode is not successful"), DashboardData.Episodes[3].bSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectRunResultDashboardAiTest,
	"OdiroSim.ProjectRun.ResultDashboard.Ai",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectRunResultDashboardAiTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ResponseJson = TEXT(R"({
		"schema": "analysis_run_response_v2",
		"version": 2,
		"status": "success",
		"review_id": "0002",
		"run_id": "000123",
		"run_overview": {
			"total_play_time_s": 26.5,
			"episode_count": 4,
			"success_rate": 0.5,
			"collision_count": 6,
			"display": {
				"total_play_time": "26.5초",
				"success_rate": "50%",
				"collision_count": "6회"
			}
		},
		"episodes": [
			{
				"episode_id": "000001",
				"duration_s": 9.7,
				"outcome": "success",
				"display": {
					"duration": "9.7초",
					"outcome": "성공"
				}
			},
			{
				"episode_id": "000002",
				"duration_s": 8.4,
				"outcome": "failure",
				"display": {
					"duration": "8.4초",
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
			},
			{
				"id": "REC-002",
				"target": "policy",
				"priority": "medium",
				"title": "경로 추종 파라미터 검토",
				"reason": "재탐색 이벤트가 반복되었습니다.",
				"recommendation": "look-ahead 거리와 조향 변화량 상한을 보수적으로 조정하세요."
			}
		],
		"warnings": [
			"일부 episode 로그가 누락되었습니다.",
			"skipped large file: runs/000123/episodes/000001/actions.jsonl"
		]
	})");

	FProjectRunResultDashboardData DashboardData;
	TestTrue(
		TEXT("AI response parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(ResponseJson, DashboardData));
	TestTrue(TEXT("AI loaded"), DashboardData.bAiLoaded);
	TestTrue(TEXT("summary message parsed"), DashboardData.AiSummary.Contains(TEXT("환경 또는 장애물")));
	TestEqual(TEXT("display total play time parsed"), DashboardData.TotalPlayTimeLabel, FString(TEXT("26.5초")));
	TestEqual(TEXT("display success rate parsed"), DashboardData.SuccessRateLabel, FString(TEXT("50%")));
	TestEqual(TEXT("display collision count parsed"), DashboardData.CollisionCountLabel, FString(TEXT("6회")));
	TestEqual(TEXT("episode display count"), DashboardData.Episodes.Num(), 2);
	TestEqual(TEXT("episode display duration parsed"), DashboardData.Episodes[0].DurationLabel, FString(TEXT("9.7초")));
	TestEqual(TEXT("episode display outcome parsed"), DashboardData.Episodes[1].OutcomeLabel, FString(TEXT("실패")));
	TestEqual(TEXT("insight count"), DashboardData.Insights.Num(), 1);
	TestEqual(TEXT("insight severity parsed"), DashboardData.Insights[0].Severity, EProjectRunAiSuggestionSeverity::High);
	TestTrue(TEXT("insight title parsed"), DashboardData.Insights[0].Title.Contains(TEXT("충돌")));
	TestTrue(TEXT("insight description parsed"), DashboardData.Insights[0].Description.Contains(TEXT("반복")));
	TestEqual(TEXT("warning count filters internal warnings"), DashboardData.Warnings.Num(), 1);
	if (DashboardData.Warnings.Num() > 0)
	{
		TestEqual(TEXT("warning keeps actionable text"), DashboardData.Warnings[0], FString(TEXT("일부 episode 로그가 누락되었습니다.")));
	}
	TestEqual(TEXT("suggestion count"), DashboardData.Suggestions.Num(), 2);
	TestEqual(TEXT("string priority severity"), DashboardData.Suggestions[0].Severity, EProjectRunAiSuggestionSeverity::High);
	TestEqual(TEXT("medium priority severity"), DashboardData.Suggestions[1].Severity, EProjectRunAiSuggestionSeverity::Medium);
	TestTrue(TEXT("v2 suggestion title parsed"), DashboardData.Suggestions[0].Title.Contains(TEXT("정적 장애물 배치")));
	TestTrue(TEXT("v2 suggestion reason parsed"), DashboardData.Suggestions[0].Reason.Contains(TEXT("충돌")));
	TestTrue(TEXT("v2 suggestion recommendation parsed"), DashboardData.Suggestions[1].Recommendation.Contains(TEXT("look-ahead")));
	TestEqual(TEXT("v2 suggestion target parsed"), DashboardData.Suggestions[0].ParameterName, FString(TEXT("environment")));

	const FString EmptyRecommendationsJson = TEXT(R"({
		"summary": {
			"overall_judgement": "no_change_recommended",
			"message": "추가 개선 제안이 없습니다."
		},
		"recommendation_type": "none",
		"recommendations": []
	})");

	FProjectRunResultDashboardData EmptyData;
	TestTrue(
		TEXT("empty recommendations parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(EmptyRecommendationsJson, EmptyData));
	TestTrue(TEXT("empty AI loaded"), EmptyData.bAiLoaded);
	TestEqual(TEXT("empty suggestion count"), EmptyData.Suggestions.Num(), 0);
	TestTrue(TEXT("empty summary parsed"), EmptyData.AiSummary.Contains(TEXT("없습니다")));

	const FString LegacyRecommendationsJson = TEXT(R"({
		"summary": "fallback 규칙 기반으로 추천을 생성했습니다.",
		"botSetupRecommendations": [
			{
				"param": "stop_distance_m",
				"current": 1.2,
				"suggested": 1.5,
				"reason": "near miss 때문에 정지 거리를 늘립니다."
			}
		],
		"policyServerRecommendations": [
			{
				"param": "max_speed_mps",
				"current": 2.0,
				"suggested": 1.6,
				"reason": "회전 구간 속도가 높습니다."
			}
		]
	})");

	FProjectRunResultDashboardData LegacyData;
	TestTrue(
		TEXT("legacy recommendations parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(LegacyRecommendationsJson, LegacyData));
	TestTrue(TEXT("legacy AI loaded"), LegacyData.bAiLoaded);
	TestEqual(TEXT("legacy suggestion count"), LegacyData.Suggestions.Num(), 2);
	TestEqual(TEXT("legacy suggestion parameter parsed"), LegacyData.Suggestions[0].ParameterName, FString(TEXT("stop_distance_m")));
	TestEqual(TEXT("legacy suggestion current parsed"), LegacyData.Suggestions[0].CurrentValue, FString(TEXT("1.2")));
	TestEqual(TEXT("legacy suggestion suggested parsed"), LegacyData.Suggestions[0].SuggestedValue, FString(TEXT("1.5")));
	TestTrue(TEXT("legacy suggestion reason parsed"), LegacyData.Suggestions[1].Reason.Contains(TEXT("속도")));

	const FString InsufficientDataJson = TEXT(R"({
		"summary": {
			"overall_judgement": "insufficient_data",
			"message": "분석 가능한 로그가 부족합니다."
		},
		"recommendation_type": "insufficient_data",
		"recommendations": []
	})");

	FProjectRunResultDashboardData InsufficientData;
	TestTrue(
		TEXT("insufficient data recommendations parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(InsufficientDataJson, InsufficientData));
	TestTrue(TEXT("insufficient data AI loaded"), InsufficientData.bAiLoaded);
	TestEqual(TEXT("insufficient data suggestion count"), InsufficientData.Suggestions.Num(), 0);
	TestTrue(TEXT("insufficient data summary parsed"), InsufficientData.AiSummary.Contains(TEXT("부족합니다")));

	const FString FailedResponseJson = TEXT(R"({
		"schema": "result_analysis_response",
		"version": 2,
		"status": "failed",
		"error": {
			"message": "결과 분석 요청 처리에 실패했습니다."
		},
		"warnings": [
			"review 폴더를 확인하세요."
		]
	})");

	FProjectRunResultDashboardData FailedData;
	TestTrue(
		TEXT("failed AI response parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(FailedResponseJson, FailedData));
	TestTrue(TEXT("failed AI loaded"), FailedData.bAiLoaded);
	TestTrue(TEXT("failed error summary parsed"), FailedData.AiSummary.Contains(TEXT("실패했습니다")));
	TestEqual(TEXT("failed warning count"), FailedData.Warnings.Num(), 1);

	const FString TestRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/ProjectRunResultDashboard"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	const FString RunDirectory = FPaths::Combine(TestRoot, TEXT("runs/000123"));
	const FString ReviewDirectory = FPaths::Combine(RunDirectory, TEXT("review"));
	TestTrue(TEXT("create review fixture directory"), IFileManager::Get().MakeDirectory(*ReviewDirectory, true));

	const FString SummaryJson = TEXT(R"({
		"schema": "run_summary",
		"version": 1,
		"run": { "run_id": "000123" },
		"rows": []
	})");
	TestTrue(
		TEXT("write summary fixture"),
		FFileHelper::SaveStringToFile(
			SummaryJson,
			*FPaths::Combine(RunDirectory, TEXT("summary.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(
		TEXT("write saved AI analysis fixture"),
		FFileHelper::SaveStringToFile(
			ResponseJson,
			*FPaths::Combine(ReviewDirectory, TEXT("analysis_run_response_v2.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FProjectRunResultDashboardData SavedResponseData;
	TestTrue(
		TEXT("dashboard reloads saved AI analysis response"),
		FProjectRunResultDashboardJson::BuildFromRunDirectory(RunDirectory, SavedResponseData));
	TestTrue(TEXT("saved AI response loaded"), SavedResponseData.bAiLoaded);
	TestTrue(TEXT("saved AI summary parsed"), SavedResponseData.AiSummary.Contains(TEXT("환경 또는 장애물")));
	TestEqual(TEXT("saved AI response suggestions"), SavedResponseData.Suggestions.Num(), 2);
	TestEqual(TEXT("saved AI response episodes"), SavedResponseData.Episodes.Num(), 2);

	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

#endif
