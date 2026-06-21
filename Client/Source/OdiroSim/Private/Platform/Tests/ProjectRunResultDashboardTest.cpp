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
		"schema": "analysis_run_response_v2",
		"version": 2,
		"summary": {
			"overall_judgement": "needs_change",
			"message": "골 지점 접근 구간에서 개선 여지가 있습니다."
		},
		"recommendations": [
			{
				"severity": "high",
				"message": "브레이크 트리거를 앞당기세요."
			},
			{
				"priority": 2,
				"param": "steering_gain",
				"current": 0.8,
				"suggested": 0.65,
				"reason": "좌회전 구간 조향이 과도합니다."
			}
		]
	})");

	FProjectRunResultDashboardData DashboardData;
	TestTrue(
		TEXT("AI response parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(ResponseJson, DashboardData));
	TestTrue(TEXT("AI loaded"), DashboardData.bAiLoaded);
	TestTrue(TEXT("summary parsed"), DashboardData.AiSummary.Contains(TEXT("개선 여지")));
	TestEqual(TEXT("suggestion count"), DashboardData.Suggestions.Num(), 2);
	TestEqual(TEXT("explicit severity"), DashboardData.Suggestions[0].Severity, EProjectRunAiSuggestionSeverity::High);
	TestEqual(TEXT("priority severity"), DashboardData.Suggestions[1].Severity, EProjectRunAiSuggestionSeverity::Medium);
	TestTrue(TEXT("constructed message"), DashboardData.Suggestions[1].Message.Contains(TEXT("steering_gain")));

	const FString EmptyRecommendationsJson = TEXT(R"({
		"schema": "analysis_run_response_v2",
		"version": 2,
		"summary": { "message": "추가 개선 제안이 없습니다." },
		"recommendations": []
	})");

	FProjectRunResultDashboardData EmptyData;
	TestTrue(
		TEXT("empty recommendations parsed"),
		FProjectRunResultDashboardJson::AppendAiFromAnalysisResponseJsonString(EmptyRecommendationsJson, EmptyData));
	TestTrue(TEXT("empty AI loaded"), EmptyData.bAiLoaded);
	TestEqual(TEXT("empty suggestion count"), EmptyData.Suggestions.Num(), 0);
	TestTrue(TEXT("empty summary parsed"), EmptyData.AiSummary.Contains(TEXT("없습니다")));

	return true;
}

#endif
