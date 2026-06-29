from __future__ import annotations

import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app.main import app


def _read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _write_episode(project: Path, run_id: str, episode_id: str, result: dict, events: str = "") -> None:
    episode_dir = project / "runs" / run_id / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")
    if events:
        (episode_dir / "events.jsonl").write_text(events, encoding="utf-8")
    _upsert_summary_row(project, run_id, _summary_row_from_result(episode_id, result, events=events))


def _write_summary(project: Path, run_id: str, summary: dict) -> None:
    """Write the run summary used by the public analysis contract."""
    run_dir = project / "runs" / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").write_text(json.dumps(summary, ensure_ascii=False), encoding="utf-8")


def _upsert_summary_row(project: Path, run_id: str, row: dict) -> None:
    """Keep episode fixtures paired with summary rows used for public metrics."""
    run_dir = project / "runs" / run_id
    summary_path = run_dir / "summary.json"
    if summary_path.is_file():
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
    else:
        summary = {
            "schema": "run_summary",
            "version": 1,
            "run": {"run_id": run_id, "project_id": project.name},
            "rows": [],
        }
    rows = [item for item in summary.get("rows", []) if item.get("episode_id") != row["episode_id"]]
    rows.append(row)
    summary["rows"] = sorted(rows, key=lambda item: str(item.get("episode_id", "")))
    _write_summary(project, run_id, summary)


def _summary_row_from_result(episode_id: str, result: dict, *, events: str = "") -> dict:
    """Build a summary.json row that mirrors one episode result fixture."""
    summary = result.get("summary") if isinstance(result.get("summary"), dict) else {}
    result_metrics = result.get("metrics") if isinstance(result.get("metrics"), dict) else {}
    metrics = dict(result_metrics)
    for key in (
        "goal_reached",
        "goal_threshold_m",
        "blocked_region_collision_count",
        "blocked_region_violation_count",
        "pedestrian_collision_count",
        "static_obstacle_collision_count",
        "near_miss_count",
        "repath_count",
        "robot_tip_over_count",
        "penalty_region_violation_count",
        "duration_s",
    ):
        if key in result and key not in metrics:
            metrics[key] = result[key]
        if key in summary and key not in metrics:
            metrics[key] = summary[key]
    for key, value in _event_counts(events, result).items():
        metrics.setdefault(key, value)
    failure_type = str(result.get("failure_type") or summary.get("terminal_reason") or "")
    if failure_type == "static_obstacle_collision":
        metrics.setdefault("static_obstacle_collision_count", 1)
    elif failure_type == "pedestrian_collision":
        metrics.setdefault("pedestrian_collision_count", 1)
    elif failure_type in {"blocked_region_collision", "blocked_region_violation"}:
        metrics.setdefault("blocked_region_collision_count", 1)
    if "goal_reached" in metrics and isinstance(metrics["goal_reached"], bool):
        metrics["goal_reached"] = 1 if metrics["goal_reached"] else 0
    success = summary.get("success", result.get("success"))
    outcome = result.get("outcome") or summary.get("outcome") or ("Success" if success is True else "Failure")
    return {
        "episode_id": episode_id,
        "outcome": outcome,
        "terminal_reason": summary.get("terminal_reason") or result.get("terminal_reason") or result.get("failure_type", ""),
        "duration_s": result.get("duration_s", metrics.get("duration_s", 10.0)),
        "metrics": metrics,
    }


def _event_counts(events: str, result: dict) -> dict[str, int]:
    """Return summary-row metric counts from event fixtures and event summaries."""
    mapping = {
        "PenaltyRegionViolation": "penalty_region_violation_count",
        "penalty_region_violation": "penalty_region_violation_count",
        "StaticObstacleCollision": "static_obstacle_collision_count",
        "static_obstacle_collision": "static_obstacle_collision_count",
        "PedestrianCollision": "pedestrian_collision_count",
        "pedestrian_collision": "pedestrian_collision_count",
        "BlockedRegionCollision": "blocked_region_collision_count",
        "blocked_region_collision": "blocked_region_collision_count",
        "BlockedRegionViolation": "blocked_region_violation_count",
        "blocked_region_violation": "blocked_region_violation_count",
        "PedestrianNearMiss": "near_miss_count",
        "near_miss": "near_miss_count",
        "Repath": "repath_count",
        "repath": "repath_count",
        "RobotTipOver": "robot_tip_over_count",
        "robot_tip_over": "robot_tip_over_count",
    }
    counts: dict[str, int] = {}
    event_summary = result.get("event_summary") if isinstance(result.get("event_summary"), dict) else {}
    by_type = event_summary.get("by_type") if isinstance(event_summary.get("by_type"), dict) else {}
    for event_type, value in by_type.items():
        metric = mapping.get(str(event_type))
        if metric and isinstance(value, int | float):
            counts[metric] = counts.get(metric, 0) + int(value)
    for line in events.splitlines():
        try:
            item = json.loads(line)
        except json.JSONDecodeError:
            continue
        event_type = item.get("event_type") or item.get("event") or item.get("type")
        metric = mapping.get(str(event_type))
        if metric:
            counts[metric] = counts.get(metric, 0) + 1
    return counts


def _write_snapshot(project: Path, run_id: str, scenario_body: dict) -> None:
    snapshot_dir = project / "runs" / run_id / "snapshot"
    snapshot_dir.mkdir(parents=True)
    (snapshot_dir / "scenario.json").write_text(json.dumps(scenario_body), encoding="utf-8")


def _write_snapshot_policy_file(project: Path, run_id: str, relative_path: str, body: bytes) -> None:
    path = project / "runs" / run_id / "snapshot" / "policy" / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(body)


def _write_large_actions_file(project: Path, run_id: str, episode_id: str) -> None:
    """Create an actions.jsonl file that the scanner will skip as too large."""
    actions_path = project / "runs" / run_id / "episodes" / episode_id / "actions.jsonl"
    actions_path.parent.mkdir(parents=True, exist_ok=True)
    actions_path.write_bytes(b"{\"action\":\"noop\"}\n" + b" " * 5_000_001)


@pytest.fixture(autouse=True)
def _disable_endpoint_llm(monkeypatch) -> None:
    """Keep review artifact regression tests independent from provider credentials."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")


def test_v2_analysis_run_writes_review_artifacts_and_index(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_snapshot(project, "000001", {"scenario_id": "baseline"})
    _write_episode(
        project,
        "000001",
        "000001",
        {"success": True, "goal_reached": True, "penalty_region_violation_count": 2},
        '{"event_type": "PenaltyRegionViolation"}\n',
    )

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={
            "project_path": str(project),
            "run_id": "000001",
            "prompt": "장애물 때문인지 봐줘",
        },
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "패널티 구역 침범" in payload["summary"]["message"]
    review_dir = project / "runs" / "000001" / "review" / "0001"
    assert review_dir.is_dir()
    status = _read_json(review_dir / "status.json")
    request = _read_json(review_dir / "request.json")
    report = _read_json(review_dir / "report.json")
    recommendations = _read_json(review_dir / "recommendations.json")
    manifest = _read_json(review_dir / "manifest.json")
    index = _read_json(project / "analysis_index.json")

    assert payload["review_id"] == "0001"
    assert payload["run_id"] == "000001"
    assert payload["recommendation_type"] == "policy_review"
    assert status["status"] == "completed"
    assert status["review_id"] == "0001"
    assert status["run_id"] == "000001"
    assert status["completed_at"]
    assert status["error"] is None
    assert request["project_path"] == str(project)
    assert request["run_id"] == "000001"
    assert request["prompt"] == "장애물 때문인지 봐줘"
    assert report["data_coverage"]["summary_json"] == "present"
    assert report["data_coverage"]["result_file_count"] == 1
    assert report["data_coverage"]["events_file_count"] == 1
    assert report["findings"][0]["type"] == "penalty_region_violation"
    assert report["findings"][0]["evidence_ids"]
    assert "obstacle" not in {finding["type"] for finding in report["findings"]}
    assert report["evidence"][0]["run_id"] == "000001"
    assert recommendations["recommendation_type"] == "policy_review"
    assert recommendations["recommendation_type"] == payload["recommendation_type"]
    assert recommendations["evidence_ids"] == report["findings"][0]["evidence_ids"]
    assert manifest["snapshot_hashes"]["overall_hash"]
    assert manifest["comparison"]["comparison_status"] == "no_baseline"
    assert manifest["based_on_review_id"] is None
    assert "runs/000001/review/0001/report.json" in manifest["generated_files"]
    assert index["latest_review_id"] == "0001"
    assert index["reviews"][0]["based_on_review_id"] is None
    assert index["runs"]["000001"]["reviews"][0]["based_on_review_id"] is None
    assert index["runs"]["000001"]["reviews"][0]["recommendation_type"] == "policy_review"


def test_v2_analysis_run_allocates_next_review_and_compares_previous_snapshot(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_snapshot(project, "000001", {"scenario_id": "old"})
    _write_snapshot(project, "000002", {"scenario_id": "new"})
    _write_episode(project, "000001", "000001", {"success": True, "goal_reached": True})
    _write_episode(project, "000002", "000001", {"success": False, "failure_type": "timeout"})
    first_review = project / "runs" / "000002" / "review" / "0001"
    first_review.mkdir(parents=True)
    (first_review / "status.json").write_text(
        json.dumps({"review_id": "0001", "run_id": "000002", "status": "completed"}),
        encoding="utf-8",
    )

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000002"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000002" / "review" / "0002"
    request = _read_json(review_dir / "request.json")
    manifest = _read_json(review_dir / "manifest.json")
    recommendations = _read_json(review_dir / "recommendations.json")
    index = _read_json(project / "analysis_index.json")

    assert "based_on_review_id" not in request
    assert request["prompt"] is None
    assert manifest["based_on_review_id"] == "0001"
    assert manifest["comparison"]["previous_run_id"] == "000001"
    assert manifest["comparison"]["comparison_status"] == "changed"
    assert manifest["comparison"]["changed_artifacts"] == ["scenario.json"]
    assert recommendations["recommendation_type"] == "policy_review"
    assert payload["review_id"] == "0002"
    assert payload["run_id"] == "000002"
    assert payload["recommendation_type"] == recommendations["recommendation_type"]
    assert index["latest_review_id"] == "0002"
    assert index["reviews"][0]["based_on_review_id"] == "0001"
    assert [entry["review_id"] for entry in index["runs"]["000002"]["reviews"]] == ["0002"]
    assert index["runs"]["000002"]["reviews"][0]["based_on_review_id"] == "0001"


def test_v2_analysis_run_snapshot_hash_excludes_policy_runtime_cache_files(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_snapshot(project, "000001", {"scenario_id": "cache-filter"})
    _write_snapshot_policy_file(project, "000001", "rules.json", b'{"rule": true}')
    _write_snapshot_policy_file(project, "000001", "__pycache__/rules.cpython-312.pyc", b"cache")
    _write_snapshot_policy_file(project, "000001", ".DS_Store", b"metadata")
    _write_episode(project, "000001", "000001", {"success": True, "goal_reached": True})

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    manifest = _read_json(project / "runs" / "000001" / "review" / "0001" / "manifest.json")
    snapshot_files = manifest["snapshot_hashes"]["files"]
    assert "policy/rules.json" in snapshot_files
    assert "policy/__pycache__/rules.cpython-312.pyc" not in snapshot_files
    assert "policy/.DS_Store" not in snapshot_files
    assert "runs/000001/snapshot/policy/rules.json" in manifest["source_run_files"]
    assert "runs/000001/snapshot/policy/__pycache__/rules.cpython-312.pyc" not in manifest["source_run_files"]
    assert "runs/000001/snapshot/policy/.DS_Store" not in manifest["source_run_files"]


def test_v2_analysis_run_data_coverage_lists_broken_json_and_large_actions_by_requested_run(tmp_path) -> None:
    """Data coverage distinguishes skipped actions and run-local broken JSON paths."""
    project = tmp_path / "Project1"
    _write_episode(project, "000001", "000001", {"success": True, "goal_reached": True})
    _write_large_actions_file(project, "000001", "000001")
    _write_episode(project, "000002", "000001", {"success": True, "goal_reached": True})
    _write_large_actions_file(project, "000002", "000001")
    broken_snapshot = project / "runs" / "000002" / "snapshot" / "scenario.json"
    broken_snapshot.parent.mkdir(parents=True)
    broken_snapshot.write_text("{broken json}", encoding="utf-8")

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000002"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000002" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    coverage = report["data_coverage"]
    assert coverage["actions_file_count"] == 1
    assert coverage["parsed_actions_file_count"] == 0
    assert coverage["skipped_large_actions_file_count"] == 1
    assert coverage["broken_json_count"] == 1
    assert coverage["broken_json_paths"] == ["runs/000002/snapshot/scenario.json"]
    assert any("runs\\000002\\episodes\\000001\\actions.jsonl" in warning for warning in payload["warnings"])
    assert not any("runs\\000001\\episodes\\000001\\actions.jsonl" in warning for warning in payload["warnings"])


def test_v2_analysis_run_missing_run_does_not_create_review_directory(tmp_path) -> None:
    project = tmp_path / "Project1"
    project.mkdir()

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert "review_id" not in payload
    assert payload["run_id"] == "000001"
    assert payload["recommendation_type"] == "insufficient_data"
    assert not (project / "runs" / "000001").exists()
    assert not (project / "analysis_index.json").exists()


def test_v2_analysis_run_existing_empty_run_keeps_insufficient_data_summary(tmp_path) -> None:
    project = tmp_path / "Project1"
    (project / "runs" / "000001").mkdir(parents=True)

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert payload["review_id"] == "0001"
    assert payload["run_id"] == "000001"
    assert payload["recommendation_type"] == "insufficient_data"
    recommendations = _read_json(project / "runs" / "000001" / "review" / "0001" / "recommendations.json")
    assert recommendations["recommendation_type"] == "insufficient_data"
    assert recommendations["recommendation_type"] == payload["recommendation_type"]


def test_v2_analysis_run_writes_review_artifacts_for_failed_run(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_snapshot(project, "000001", {"scenario_id": "failed_run"})
    _write_episode(project, "000001", "000001", {"success": False, "failure_type": "timeout"})

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["review_id"] == "0001"
    assert payload["run_id"] == "000001"
    assert payload["recommendation_type"] == "policy_review"
    review_dir = project / "runs" / "000001" / "review" / "0001"
    assert _read_json(review_dir / "status.json")["status"] == "completed"
    assert _read_json(review_dir / "recommendations.json")["recommendation_type"] == "policy_review"
