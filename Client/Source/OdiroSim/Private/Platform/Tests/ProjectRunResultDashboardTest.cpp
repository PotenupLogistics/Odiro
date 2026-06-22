#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ProjectRunResultDashboard.h"

#include "Misc/AutomationTest.h"

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
		"review_id": "0002",
		"run_id": "000123",
		"analysis_mode": "llm",
		"summary": {
			"overall_judgement": "change_recommended",
			"message": "환경 또는 장애물 배치 검토가 필요합니다."
		},
		"analysis_text": "[결과 요약]\n정적 장애물 충돌이 반복되었습니다.",
		"recommendation_type": "environment_review",
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
		]
	})");

	FProjectRunResultDashboardData DashboardData;
	TestTrue(
		TEXT("AI response parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(ResponseJson, DashboardData));
	TestTrue(TEXT("AI loaded"), DashboardData.bAiLoaded);
	TestTrue(TEXT("analysis text parsed"), DashboardData.AiSummary.Contains(TEXT("정적 장애물 충돌")));
	TestEqual(TEXT("suggestion count"), DashboardData.Suggestions.Num(), 2);
	TestEqual(TEXT("string priority severity"), DashboardData.Suggestions[0].Severity, EProjectRunAiSuggestionSeverity::High);
	TestEqual(TEXT("medium priority severity"), DashboardData.Suggestions[1].Severity, EProjectRunAiSuggestionSeverity::Medium);
	TestTrue(TEXT("v2 suggestion title parsed"), DashboardData.Suggestions[0].Title.Contains(TEXT("정적 장애물 배치")));
	TestTrue(TEXT("v2 suggestion reason parsed"), DashboardData.Suggestions[0].Reason.Contains(TEXT("충돌")));
	TestTrue(TEXT("v2 suggestion recommendation parsed"), DashboardData.Suggestions[1].Recommendation.Contains(TEXT("look-ahead")));

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

	return true;
}

#endif
