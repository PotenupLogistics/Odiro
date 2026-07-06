from __future__ import annotations

import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetricExtractor
from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetrics
from app.agents.result_analysis_v2.failure_pattern_detector import FailurePatternDetector
from app.main import app


@pytest.fixture(autouse=True)
def _disable_endpoint_llm(monkeypatch) -> None:
    """Keep endpoint schema parsing tests independent from local LLM settings."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")


def _episode_metrics(*, episode_id: str, failure_type: str | None = None, timeout: bool | None = None) -> EpisodeMetrics:
    """Create minimal EpisodeMetrics for pattern detector tests."""
    return EpisodeMetrics(
        experiment_id="Project1",
        run_id="000001",
        episode_id=episode_id,
        success=False,
        failure_type=failure_type,
        goal_reached=False,
        timeout=timeout,
        collision_count=0,
        near_miss_count=0,
        blocked_region_violation_count=0,
        penalty_region_violation_count=0,
        pedestrian_collision_count=0,
        static_obstacle_collision_count=0,
        duration_s=None,
        min_pedestrian_distance_m=None,
        min_obstacle_distance_m=None,
        average_speed_mps=None,
        max_speed_mps=None,
        stop_count=0,
        repath_count=0,
        policy_decision_error_count=0,
        stuck_count=0,
        robot_tip_over_count=0,
        stuck_duration_s=None,
        terminal_reason=None,
        setup_failure_details=None,
    )


def _write_summary_row(project: Path, *, terminal_reason: str, metrics: dict[str, int]) -> None:
    """Write the summary row used by public success and count calculations."""
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").write_text(
        json.dumps(
            {
                "schema": "run_summary",
                "version": 1,
                "run": {"run_id": "000001", "project_id": project.name},
                "rows": [
                    {
                        "episode_id": "000001",
                        "outcome": "Failure",
                        "terminal_reason": terminal_reason,
                        "duration_s": 10.0,
                        "metrics": metrics,
                    }
                ],
            }
        ),
        encoding="utf-8",
    )


def test_latest_result_summary_metrics_and_pascal_case_events_are_normalized() -> None:
    """Latest result.json fields and PascalCase events normalize to metrics."""
    metrics = EpisodeMetricExtractor().extract(
        experiment_id="Project1",
        run_id="000001",
        episode_id="000001",
        result={
            "schema": "episode_result",
            "version": 1,
            "summary": {"success": False, "terminal_reason": "Timeout"},
            "metrics": {
                "near_miss_count": 2,
                "static_obstacle_collision_count": 1,
                "duration_s": 12.5,
            },
            "event_summary": {
                "by_type": {
                    "PenaltyRegionViolation": 3,
                    "PedestrianCollision": 4,
                    "PolicyDecisionError": 1,
                    "Stuck": 1,
                    "RobotTipOver": 2,
                    "Repath": 5,
                }
            },
        },
        events=[
            {"event_type": "BlockedRegionCollision"},
            {"event_type": "PedestrianNearMiss"},
        ],
    )

    assert metrics.success is False
    assert metrics.failure_type == "timeout"
    assert metrics.timeout is True
    assert metrics.near_miss_count == 2
    assert metrics.static_obstacle_collision_count == 1
    assert metrics.pedestrian_collision_count == 4
    assert metrics.penalty_region_violation_count == 3
    assert metrics.blocked_region_violation_count == 1
    assert metrics.policy_decision_error_count == 1
    assert metrics.stuck_count == 1
    assert metrics.robot_tip_over_count == 2
    assert metrics.repath_count == 5
    assert metrics.duration_s == 12.5


def test_setup_failed_terminal_reason_is_normalized_without_policy_metrics() -> None:
    """SetupFailed remains a setup failure with structured details when available."""
    metrics = EpisodeMetricExtractor().extract(
        experiment_id="Project1",
        run_id="000001",
        episode_id="000001",
        result={
            "summary": {"success": False, "terminal_reason": "SetupFailed", "outcome": "setup aborted"},
            "pipeline": {
                "diagnostics": [
                    {
                        "code": "unknown_prop_id",
                        "message": "Static obstacle prop is not registered.",
                        "prop_id": "obstacle.road_cone_99",
                    }
                ]
            },
            "metrics": {"delivery_bot_failure_message": "policy should not use this as runtime failure"},
            "event_summary": {"by_type": {"Repath": 2, "PenaltyRegionViolation": 1}},
        },
        events=[],
    )

    assert metrics.success is False
    assert metrics.terminal_reason == "SetupFailed"
    assert metrics.failure_type == "setup_failed"
    assert metrics.setup_failure_details is not None
    assert metrics.setup_failure_details.error_code == "unknown_prop_id"
    assert metrics.setup_failure_details.resource_type == "prop"
    assert metrics.setup_failure_details.resource_id == "obstacle.road_cone_99"


def test_known_unknown_and_missing_terminal_reasons_are_preserved() -> None:
    """Known terminal reasons keep normalized types without inventing goal misses."""
    extractor = EpisodeMetricExtractor()

    cases = [
        ({"success": False, "terminal_reason": "GoalNotReached"}, "goal_not_reached", "GoalNotReached"),
        ({"success": False, "terminal_reason": "Timeout"}, "timeout", "Timeout"),
        ({"success": False, "terminal_reason": "StaticObstacleCollision"}, "static_obstacle_collision", "StaticObstacleCollision"),
        ({"success": False, "terminal_reason": "BlockedRegionCollision"}, "blocked_region_collision", "BlockedRegionCollision"),
        ({"success": False, "terminal_reason": "RobotTipOver"}, "robot_tip_over", "RobotTipOver"),
        ({"success": False, "terminal_reason": "UnexpectedShutdown"}, "unknown_failure", "UnexpectedShutdown"),
        ({"success": False}, None, None),
        ({"success": True, "terminal_reason": "GoalReached"}, None, "GoalReached"),
    ]

    for index, (summary, expected_failure_type, expected_terminal_reason) in enumerate(cases, start=1):
        metrics = extractor.extract(
            experiment_id="Project1",
            run_id="000001",
            episode_id=f"{index:06d}",
            result={"summary": summary},
            events=[],
        )

        assert metrics.failure_type == expected_failure_type
        assert metrics.terminal_reason == expected_terminal_reason


def test_timeout_pattern_deduplicates_failure_type_and_timeout_flag() -> None:
    """Timeout repeated count uses unique episode evidence."""
    patterns = FailurePatternDetector().detect(
        [
            _episode_metrics(episode_id="000001", failure_type="timeout", timeout=True),
            _episode_metrics(episode_id="000002", failure_type="timeout", timeout=True),
        ]
    )

    timeout_pattern = next(pattern for pattern in patterns if pattern["type"] == "timeout_repeated")
    evidence_episode_ids = [item["episode_id"] for item in timeout_pattern["evidence"]]
    assert timeout_pattern["count"] == 2
    assert evidence_episode_ids == ["000001", "000002"]


def test_latest_result_schema_drives_environment_review_finding(tmp_path: Path) -> None:
    """Latest result schema can produce an environment review finding."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps({"obstacles": {"placements": []}}), encoding="utf-8")
    episode_dir = project / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps(
            {
                "schema": "episode_result",
                "version": 1,
                "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
                "metrics": {},
                "event_summary": {"by_type": {"StaticObstacleCollision": 1}},
            }
        ),
        encoding="utf-8",
    )
    _write_summary_row(
        project,
        terminal_reason="StaticObstacleCollision",
        metrics={"goal_reached": 0, "static_obstacle_collision_count": 1},
    )

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = json.loads((review_dir / "report.json").read_text(encoding="utf-8"))
    recommendations = json.loads((review_dir / "recommendations.json").read_text(encoding="utf-8"))
    assert any(finding["type"] == "static_obstacle_collision" for finding in report["findings"])
    finding = next(finding for finding in report["findings"] if finding["type"] == "static_obstacle_collision")
    evidence = next(item for item in report["evidence"] if item["metric"] == "static_obstacle_collision_count")
    assert finding["title"] == "정적 장애물 충돌 근거가 확인되었습니다."
    assert finding["summary"] == "정적 장애물 충돌이 1개의 근거로 확인되었습니다."
    assert evidence["message"] == "정적 장애물 충돌이 1회 발생했습니다."
    assert recommendations["recommendation_type"] == "environment_review"


def test_environment_review_summary_uses_environment_message_when_penalty_also_exists(tmp_path: Path) -> None:
    """Environment review summary is not hijacked by a lower-priority penalty finding."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps({"obstacles": {"placements": []}}), encoding="utf-8")
    episode_dir = project / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps(
            {
                "schema": "episode_result",
                "version": 1,
                "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
                "metrics": {},
                "event_summary": {
                    "by_type": {
                        "StaticObstacleCollision": 74,
                        "PedestrianCollision": 104,
                        "PenaltyRegionViolation": 1,
                    }
                },
            }
        ),
        encoding="utf-8",
    )
    _write_summary_row(
        project,
        terminal_reason="StaticObstacleCollision",
        metrics={
            "goal_reached": 0,
            "static_obstacle_collision_count": 74,
            "pedestrian_collision_count": 104,
            "penalty_region_violation_count": 1,
        },
    )

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = json.loads((review_dir / "report.json").read_text(encoding="utf-8"))
    pedestrian_finding = next(finding for finding in report["findings"] if finding["type"] == "pedestrian_collision")
    pedestrian_evidence = next(item for item in report["evidence"] if item["metric"] == "pedestrian_collision_count")
    assert payload["summary"]["message"] == "장애물 주변 충돌 반복으로 환경 배치와 회피 판단 조건 점검 필요"
    assert "Penalty region violation" not in payload["summary"]["message"]
    assert report["summary"]["message"] == payload["summary"]["message"]
    assert payload["metrics"]["pedestrian_collision_count"] == 104
    assert pedestrian_finding["title"] == "보행자 충돌 근거가 확인되었습니다."
    assert pedestrian_finding["summary"] == "보행자 충돌이 1개의 근거로 확인되었습니다."
    assert pedestrian_evidence["message"] == "보행자 충돌이 104회 발생했습니다."
