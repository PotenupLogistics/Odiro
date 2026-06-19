from __future__ import annotations

import json
from pathlib import Path

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


def _write_snapshot(project: Path, run_id: str, scenario_body: dict) -> None:
    snapshot_dir = project / "runs" / run_id / "snapshot"
    snapshot_dir.mkdir(parents=True)
    (snapshot_dir / "scenario.json").write_text(json.dumps(scenario_body), encoding="utf-8")


def _write_snapshot_policy_file(project: Path, run_id: str, relative_path: str, body: bytes) -> None:
    path = project / "runs" / run_id / "snapshot" / "policy" / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(body)


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
    assert "penalty region violation" in payload["summary"]["message"]
    review_dir = project / "runs" / "000001" / "review" / "0001"
    assert review_dir.is_dir()
    status = _read_json(review_dir / "status.json")
    request = _read_json(review_dir / "request.json")
    report = _read_json(review_dir / "report.json")
    recommendations = _read_json(review_dir / "recommendations.json")
    manifest = _read_json(review_dir / "manifest.json")
    index = _read_json(project / "analysis_index.json")

    assert status["status"] == "completed"
    assert status["review_id"] == "0001"
    assert status["run_id"] == "000001"
    assert status["completed_at"]
    assert status["error"] is None
    assert request["project_path"] == str(project)
    assert request["run_id"] == "000001"
    assert request["prompt"] == "장애물 때문인지 봐줘"
    assert report["data_coverage"]["summary_json"] == "missing"
    assert report["data_coverage"]["result_file_count"] == 1
    assert report["data_coverage"]["events_file_count"] == 1
    assert report["findings"][0]["type"] == "penalty_region_violation"
    assert report["findings"][0]["evidence_ids"]
    assert "obstacle" not in {finding["type"] for finding in report["findings"]}
    assert report["evidence"][0]["run_id"] == "000001"
    assert recommendations["recommendation_type"] == "policy_review"
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


def test_v2_analysis_run_missing_run_does_not_create_review_directory(tmp_path) -> None:
    project = tmp_path / "Project1"
    project.mkdir()

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    assert response.json()["summary"]["overall_judgement"] == "insufficient_data"
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
    assert response.json()["summary"]["overall_judgement"] == "insufficient_data"
    recommendations = _read_json(project / "runs" / "000001" / "review" / "0001" / "recommendations.json")
    assert recommendations["recommendation_type"] == "insufficient_data"


def test_v2_analysis_graph_mode_writes_review_artifacts(monkeypatch, tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_snapshot(project, "000001", {"scenario_id": "graph"})
    _write_episode(project, "000001", "000001", {"success": False, "failure_type": "timeout"})
    monkeypatch.setenv("V2_AGENT_GRAPH_ENABLED", "true")

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    assert response.json()["summary"]["overall_judgement"] == "change_recommended"
    review_dir = project / "runs" / "000001" / "review" / "0001"
    assert _read_json(review_dir / "status.json")["status"] == "completed"
    assert _read_json(review_dir / "recommendations.json")["recommendation_type"] == "policy_review"
