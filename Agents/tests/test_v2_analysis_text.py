from __future__ import annotations

import json
from pathlib import Path

from fastapi.testclient import TestClient

from app.main import app


def _read_json(path: Path) -> dict:
    """Read a review JSON artifact from a test project."""
    return json.loads(path.read_text(encoding="utf-8"))


def _write_episode(project: Path, result: dict) -> None:
    """Create one episode result fixture under run 000001."""
    episode_dir = project / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")


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
    assert "[주요 근거]" in text
    assert "[판단]" in text
    assert "[추천]" in text


def test_analysis_text_is_response_only_for_policy_recommendation(tmp_path: Path) -> None:
    """Policy analysis text appears in the response and not artifacts."""
    project = tmp_path / "Project1"
    _write_policy(project)
    _write_episode(project, {"success": False, "goal_reached": False, "penalty_region_violation_count": 1})

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    assert "정책" in payload["analysis_text"]
    assert "수정 후보" in payload["analysis_text"]
    _assert_report_sections(payload["analysis_text"])

    review_dir = project / "runs" / "000001" / "review" / "0001"
    for filename in ("status.json", "request.json", "report.json", "recommendations.json", "manifest.json"):
        assert "analysis_text" not in _read_json(review_dir / filename)


def test_analysis_text_mentions_environment_candidate_when_generated(tmp_path: Path) -> None:
    """Environment analysis text says a scenario candidate was generated."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(
        json.dumps({"corridor": {"walkway_width_m": 2.0}, "obstacles": {"placements": []}}),
        encoding="utf-8",
    )
    _write_static_obstacle_episode(project)

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    text = response.json()["analysis_text"]
    assert "환경" in text
    assert "환경 수정 후보가 생성되었습니다" in text
    _assert_report_sections(text)


def test_analysis_text_counts_environment_evidence_episodes_not_trace_only_episodes(tmp_path: Path) -> None:
    """Environment evidence counts ignore episodes that only have auxiliary logs."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps({"obstacles": {"placements": []}}), encoding="utf-8")
    _write_static_obstacle_episode(project)
    trace_only_dir = project / "runs" / "000001" / "episodes" / "000002"
    trace_only_dir.mkdir(parents=True)
    (trace_only_dir / "actions.jsonl").write_text('{"action": "noop"}\n', encoding="utf-8")

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    text = response.json()["analysis_text"]
    assert "분석 가능한 1개 에피소드에서 정적 장애물 충돌" in text
    assert "분석 가능한 2개 에피소드에서 정적 장애물 충돌" not in text


def test_analysis_text_distinguishes_missing_scenario_from_parse_failure(tmp_path: Path) -> None:
    """Missing and unreadable scenario candidates produce different display text."""
    missing_project = tmp_path / "MissingScenario"
    _write_static_obstacle_episode(missing_project)

    missing_response = _post_analysis(missing_project)

    assert missing_response.status_code == 200, missing_response.text
    missing_text = missing_response.json()["analysis_text"]
    assert "scenario.json을 찾지 못해" in missing_text
    _assert_report_sections(missing_text)

    parse_error_project = tmp_path / "ParseErrorScenario"
    parse_error_project.mkdir(parents=True, exist_ok=True)
    (parse_error_project / "scenario.json").write_text("{ invalid json", encoding="utf-8")
    _write_static_obstacle_episode(parse_error_project)

    parse_error_response = _post_analysis(parse_error_project)

    assert parse_error_response.status_code == 200, parse_error_response.text
    parse_error_text = parse_error_response.json()["analysis_text"]
    assert "scenario.json을 읽는 중 오류가 발생해" in parse_error_text
    assert parse_error_text != missing_text
    _assert_report_sections(parse_error_text)


def test_analysis_text_mentions_no_candidate_for_none(tmp_path: Path) -> None:
    """No-change analysis text says no policy/environment candidate was made."""
    project = tmp_path / "Project1"
    _write_episode(project, {"success": True, "goal_reached": True})

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    text = response.json()["analysis_text"]
    assert "정책" in text
    assert "환경" in text
    assert "별도 수정 후보를 생성하지 않는 것이 적절" in text
    assert "로그가 부족" not in text
    assert "판단하기 어렵" not in text
    _assert_report_sections(text)


def test_analysis_text_mentions_insufficient_logs_when_run_is_missing(tmp_path: Path) -> None:
    """Insufficient-data analysis text asks for rerun or log checks."""
    project = tmp_path / "Project1"
    project.mkdir()

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    text = response.json()["analysis_text"]
    assert "로그" in text
    assert "부족" in text
    assert "판단하기 어렵" in text
    assert "result.json" in text
    assert "events.jsonl" in text
    assert "다시 분석" in text
    assert "별도 수정 후보를 생성하지 않는 것이 적절" not in text
    _assert_report_sections(text)
    assert not (project / "runs" / "000001" / "review").exists()
