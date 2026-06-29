from __future__ import annotations

import json
from pathlib import Path

from fastapi.testclient import TestClient

from app.agents.result_analysis_v2.analysis_text_builder import AnalysisTextBuilder
from app.main import app


def _read_json(path: Path) -> dict:
    """Read a review JSON artifact from a test project."""
    return json.loads(path.read_text(encoding="utf-8"))


def _write_episode(project: Path, result: dict) -> None:
    """Create one episode result fixture under run 000001."""
    episode_dir = project / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")
    _write_summary_row(project, result)


def _write_summary_row(project: Path, result: dict) -> None:
    """Create the summary row required by the public analysis response."""
    run_dir = project / "runs" / "000001"
    summary = result.get("summary") if isinstance(result.get("summary"), dict) else {}
    metrics = dict(result.get("metrics") if isinstance(result.get("metrics"), dict) else {})
    for key in (
        "goal_reached",
        "penalty_region_violation_count",
        "static_obstacle_collision_count",
        "pedestrian_collision_count",
        "repath_count",
        "duration_s",
    ):
        if key in result and key not in metrics:
            metrics[key] = result[key]
        if key in summary and key not in metrics:
            metrics[key] = summary[key]
    event_summary = result.get("event_summary") if isinstance(result.get("event_summary"), dict) else {}
    by_type = event_summary.get("by_type") if isinstance(event_summary.get("by_type"), dict) else {}
    event_mapping = {
        "StaticObstacleCollision": "static_obstacle_collision_count",
        "PedestrianCollision": "pedestrian_collision_count",
        "Repath": "repath_count",
        "Stuck": "stuck_count",
    }
    for event_type, metric_key in event_mapping.items():
        if event_type in by_type and metric_key not in metrics:
            metrics[metric_key] = by_type[event_type]
    if "goal_reached" in metrics and isinstance(metrics["goal_reached"], bool):
        metrics["goal_reached"] = 1 if metrics["goal_reached"] else 0
    success = summary.get("success", result.get("success"))
    row = {
        "episode_id": "000001",
        "outcome": result.get("outcome") or summary.get("outcome") or ("Success" if success is True else "Failure"),
        "terminal_reason": summary.get("terminal_reason") or result.get("terminal_reason") or result.get("failure_type", ""),
        "duration_s": result.get("duration_s", metrics.get("duration_s", 10.0)),
        "metrics": metrics,
    }
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").write_text(
        json.dumps(
            {
                "schema": "run_summary",
                "version": 1,
                "run": {"run_id": "000001", "project_id": project.name},
                "rows": [row],
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )


def _write_policy(project: Path) -> None:
    """Create a minimal root policy package for candidate generation."""
    policy_dir = project / "policy"
    policy_dir.mkdir(parents=True)
    (policy_dir / "__init__.py").write_text("def create_policy():\n    return None\n", encoding="utf-8")


def _write_static_obstacle_episode(project: Path) -> None:
    """Create a static obstacle collision episode for environment-review tests."""
    _write_episode(
        project,
        {
            "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
            "metrics": {},
            "event_summary": {"by_type": {"StaticObstacleCollision": 1}},
        },
    )


def _post_analysis(project: Path):
    """Run the v2 analysis endpoint for run 000001."""
    return TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )


def _assert_report_sections(text: str) -> None:
    """Assert the UI analysis text has the stable report sections."""
    assert "[결과 요약]" in text
    assert "[확인 내용]" in text
    assert "[주요 근거]" not in text
    assert "[판단]" in text
    assert "[추천]" in text


def test_analysis_text_is_response_only_for_policy_recommendation(tmp_path: Path) -> None:
    """Public response and review artifacts do not persist legacy analysis text."""
    project = tmp_path / "Project1"
    _write_policy(project)
    _write_episode(project, {"success": False, "goal_reached": False, "penalty_region_violation_count": 1})

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["recommendation_type"] == "policy_review"
    assert "analysis_text" not in payload
    assert payload["recommendations"][0]["target"] == "policy"

    review_dir = project / "runs" / "000001" / "review" / "0001"
    for filename in ("status.json", "request.json", "report.json", "recommendations.json", "manifest.json"):
        assert "analysis_text" not in _read_json(review_dir / filename)


def test_analysis_text_mentions_timeout_and_stuck_as_confirmed_policy_signals(tmp_path: Path) -> None:
    """Policy evidence text uses the natural confirmed-log wording for timeout and stuck."""
    text = AnalysisTextBuilder().build(
        recommendation_type="policy_review",
        artifacts={"policy": {"generated": True}},
        findings=[{"type": "timeout"}, {"type": "stuck"}],
    )
    assert "분석 로그에서 제한 시간 초과, 정체가 확인되었습니다." in text
    assert "분석 로그에 다음 항목이 나타났습니다: 제한 시간 초과, 정체." not in text
    assert "주요 근거" not in text
    assert "근거가 확인되었습니다" not in text
    assert "근거를 토대로" not in text
    assert "pipeline.diagnostics" not in text
    assert "evidence" not in text


def test_analysis_text_policy_evidence_fallback_uses_confirmed_log_wording() -> None:
    """Policy evidence fallback keeps the same natural confirmed-log tone."""
    text = AnalysisTextBuilder().build(
        recommendation_type="policy_review",
        artifacts={},
        metrics={},
        findings=[],
    )

    assert "분석 로그에서 정책 관련 검토 신호가 확인되었습니다." in text
    assert "분석 로그에 다음 항목이 나타났습니다: 정책 관련 검토 신호." not in text


def test_analysis_text_mentions_environment_candidate_when_generated(tmp_path: Path) -> None:
    """Environment recommendation still records generated scenario artifacts."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(
        json.dumps({"corridor": {"walkway_width_m": 2.0}, "obstacles": {"placements": []}}),
        encoding="utf-8",
    )
    _write_static_obstacle_episode(project)

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    recommendations = _read_json(review_dir / "recommendations.json")
    assert payload["recommendation_type"] == "environment_review"
    assert "analysis_text" not in payload
    assert recommendations["artifacts"]["environment"]["generated"] is True


def test_analysis_text_avoids_scope_like_episode_count_for_environment_evidence(tmp_path: Path) -> None:
    """Environment evidence text avoids wording that looks like total analyzed episodes."""
    text = AnalysisTextBuilder().build(
        recommendation_type="environment_review",
        artifacts={"environment": {"generated": True}},
        metrics={"static_obstacle_collision_count": 1},
        episodes_count=2,
        evidence=[{"metric": "static_obstacle_collision_count", "value": 1}],
    )
    assert "로그에서 정적 장애물 충돌이 총 1회 확인되었습니다." in text
    assert "분석 가능한 1개 에피소드에서 정적 장애물 충돌" not in text
    assert "분석 가능한 2개 에피소드에서 정적 장애물 충돌" not in text


def test_analysis_text_mentions_pedestrian_collision_in_environment_review(tmp_path: Path) -> None:
    """Environment review text includes pedestrian collisions alongside static obstacle collisions."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps({"obstacles": {"placements": []}}), encoding="utf-8")
    _write_episode(
        project,
        {
            "schema": "episode_result",
            "version": 1,
            "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
            "event_summary": {"by_type": {"StaticObstacleCollision": 74, "PedestrianCollision": 104}},
        },
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    assert payload["recommendation_type"] == "environment_review"
    assert payload["metrics"]["pedestrian_collision_count"] == 104
    assert "analysis_text" not in payload
    assert any(finding["type"] == "pedestrian_collision" for finding in report["findings"])
    assert any(item["metric"] == "pedestrian_collision_count" for item in report["evidence"])


def test_analysis_text_mentions_repath_when_environment_review_has_repath_metric(tmp_path: Path) -> None:
    """Environment review text can include repeated path replanning evidence."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps({"obstacles": {"placements": []}}), encoding="utf-8")
    _write_episode(
        project,
        {
            "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
            "event_summary": {"by_type": {"StaticObstacleCollision": 3, "Repath": 7}},
        },
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["recommendation_type"] == "environment_review"
    assert payload["metrics"]["repath_count"] == 7
    assert "analysis_text" not in payload


def test_analysis_text_distinguishes_missing_scenario_from_parse_failure(tmp_path: Path) -> None:
    """Missing and unreadable scenario candidates produce different display text."""
    missing_project = tmp_path / "MissingScenario"
    _write_static_obstacle_episode(missing_project)

    missing_response = _post_analysis(missing_project)

    assert missing_response.status_code == 200, missing_response.text
    missing_recommendations = _read_json(
        missing_project / "runs" / "000001" / "review" / "0001" / "recommendations.json"
    )
    assert missing_recommendations["artifacts"]["environment"]["generated"] is False

    parse_error_project = tmp_path / "ParseErrorScenario"
    parse_error_project.mkdir(parents=True, exist_ok=True)
    (parse_error_project / "scenario.json").write_text("{ invalid json", encoding="utf-8")
    _write_static_obstacle_episode(parse_error_project)

    parse_error_response = _post_analysis(parse_error_project)

    assert parse_error_response.status_code == 200, parse_error_response.text
    parse_error_recommendations = _read_json(
        parse_error_project / "runs" / "000001" / "review" / "0001" / "recommendations.json"
    )
    assert parse_error_recommendations["artifact_warnings"] != missing_recommendations["artifact_warnings"]


def test_analysis_text_mentions_no_candidate_for_none(tmp_path: Path) -> None:
    """No-change analysis text says no policy/environment candidate was made."""
    project = tmp_path / "Project1"
    _write_episode(project, {"success": True, "goal_reached": True})

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["recommendation_type"] == "none"
    assert payload["recommendations"] == []
    assert "analysis_text" not in payload
    assert "로그가 부족" not in payload["summary"]["message"]
    assert "판단하기 어렵" not in payload["summary"]["message"]


def test_analysis_text_mentions_insufficient_logs_when_run_is_missing(tmp_path: Path) -> None:
    """Insufficient-data analysis text asks for rerun or log checks."""
    project = tmp_path / "Project1"
    project.mkdir()

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    assert "analysis_text" not in payload
    assert "로그" in payload["summary"]["message"]
    assert "부족" in payload["summary"]["message"]
    assert "판단하기 어렵" in payload["summary"]["message"]
    assert not (project / "runs" / "000001" / "review").exists()
