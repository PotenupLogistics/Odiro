from __future__ import annotations

import json
from pathlib import Path

from fastapi.testclient import TestClient

from app.main import app


def _read_json(path: Path) -> dict:
    """Read a JSON artifact from a test project."""
    return json.loads(path.read_text(encoding="utf-8"))


def _write_episode(project: Path, result: dict, events: str = "") -> None:
    """Create one episode result fixture under run 000001."""
    episode_dir = project / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")
    if events:
        (episode_dir / "events.jsonl").write_text(events, encoding="utf-8")


def _write_policy(project: Path) -> Path:
    """Create a small policy package with runtime cache files."""
    policy_dir = project / "policy"
    policy_dir.mkdir(parents=True)
    (policy_dir / "__init__.py").write_text(
        "from .path_follower import PathFollower\n\n\ndef create_policy():\n    return PathFollower()\n",
        encoding="utf-8",
    )
    path_follower = policy_dir / "path_follower.py"
    path_follower.write_text(
        "class PathFollower:\n"
        "    def __init__(self):\n"
        "        self.followSpeedKmh = 5.0\n"
        "        self.maxPathErrorM = 1.2\n"
        "        self.lookAheadDistanceM = 1.8\n"
        "        self.pathSmoothingDistanceM = 0.5\n"
        "        self.maxSteeringDelta = 0.12\n"
        "\n"
        "    def decide(self, observation):\n"
        "        return {'action': 'follow_path', 'speed_mps': 1.0}\n",
        encoding="utf-8",
    )
    (policy_dir / "__pycache__").mkdir()
    (policy_dir / "__pycache__" / "path_follower.cpython-312.pyc").write_bytes(b"cache")
    (policy_dir / ".DS_Store").write_bytes(b"metadata")
    return path_follower


def _write_nested_bom_path_follower_policy(project: Path) -> Path:
    """Create a BOM-encoded nested path_follower policy using snake_case defaults."""
    policy_dir = project / "policy" / "policies"
    policy_dir.mkdir(parents=True)
    (project / "policy" / "__init__.py").write_text("", encoding="utf-8")
    (policy_dir / "__init__.py").write_text("", encoding="utf-8")
    path_follower = policy_dir / "path_follower.py"
    path_follower.write_text(
        "def clamp(value, lower, upper):\n"
        "    return max(lower, min(value, upper))\n"
        "\n"
        "class PathFollower:\n"
        "    def __init__(\n"
        "        self,\n"
        "        follow_speed_kmh: float = 4.5,\n"
        "        look_ahead_distance_m: float = 1.2,\n"
        "        path_smoothing_distance_m: float = 0.35,\n"
        "        max_steering_delta: float = 0.09,\n"
        "        max_path_error_m: float = 1.2,\n"
        "    ):\n"
        "        self.followSpeedKmh = follow_speed_kmh\n"
        "        self.lookAheadDistanceM = look_ahead_distance_m\n"
        "        self.pathSmoothingDistanceM = path_smoothing_distance_m\n"
        "        self.maxSteeringDelta = max_steering_delta\n"
        "        self.maxPathErrorM = max_path_error_m\n"
        "        self.maxSteering = 0.5\n"
        "\n"
        "    def configure_from_start(self, request) -> None:\n"
        "        control_spec = request.controlSpec or {}\n"
        "        self.followSpeedKmh = float(control_spec.get('targetSpeedKmh', self.followSpeedKmh))\n"
        "        self.maxPathErrorM = float(control_spec.get('maxPathErrorM', self.maxPathErrorM))\n"
        "        self.lookAheadDistanceM = float(control_spec.get('lookAheadDistanceM', self.lookAheadDistanceM))\n"
        "        self.pathSmoothingDistanceM = float(control_spec.get('pathSmoothingDistanceM', self.pathSmoothingDistanceM))\n"
        "        self.maxSteeringDelta = float(control_spec.get('maxSteeringDelta', self.maxSteeringDelta))\n"
        "        self.followSpeedKmh = max(0.0, self.followSpeedKmh)\n"
        "        self.lookAheadDistanceM = max(0.1, self.lookAheadDistanceM)\n"
        "        self.pathSmoothingDistanceM = clamp(self.pathSmoothingDistanceM, 0.0, 2.0)\n"
        "        self.maxSteeringDelta = clamp(self.maxSteeringDelta, 0.001, self.maxSteering)\n",
        encoding="utf-8-sig",
    )
    return path_follower


def _write_unmodifiable_policy(project: Path) -> Path:
    """Create a Python policy file without supported conservative parameters."""
    policy_dir = project / "policy"
    policy_dir.mkdir(parents=True)
    path = policy_dir / "user_agent.py"
    path.write_text("class UserAgent:\n    pass\n", encoding="utf-8")
    return path


def _write_scenario(project: Path, body: dict) -> None:
    """Create the root scenario.json fixture."""
    project.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps(body, ensure_ascii=False), encoding="utf-8")


def _write_bom_scenario(project: Path, body: dict) -> bytes:
    """Create a root scenario.json fixture encoded with a UTF-8 BOM."""
    project.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(body, ensure_ascii=False)
    path = project / "scenario.json"
    path.write_text(payload, encoding="utf-8-sig")
    return path.read_bytes()


def _post_analysis(project: Path):
    """Run the v2 analysis endpoint for run 000001."""
    return TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )


def _numeric_assignment_value(source: str, parameter_name: str) -> float:
    """Return a simple numeric policy parameter assignment from source text."""
    import ast

    tree = ast.parse(source)
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        if not isinstance(node.value, ast.Constant) or not isinstance(node.value.value, int | float):
            continue
        for target in node.targets:
            if isinstance(target, ast.Attribute) and target.attr == parameter_name:
                return float(node.value.value)
            if isinstance(target, ast.Name) and target.id == parameter_name:
                return float(node.value.value)
    raise AssertionError(f"{parameter_name} assignment not found")


def test_policy_recommendation_copies_and_modifies_only_review_policy(tmp_path: Path) -> None:
    """Policy recommendations copy and modify only review/policy."""
    project = tmp_path / "Project1"
    original_policy_path = _write_policy(project)
    original_policy_text = original_policy_path.read_text(encoding="utf-8")
    _write_episode(
        project,
        {"success": False, "goal_reached": False, "penalty_region_violation_count": 1},
        '{"event_type": "PenaltyRegionViolation"}\n',
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    review_policy_dir = review_dir / "policy"
    copied_policy_path = review_policy_dir / "path_follower.py"
    assert review_policy_dir.is_dir()
    assert copied_policy_path.is_file()
    assert original_policy_path.read_text(encoding="utf-8") == original_policy_text
    copied_text = copied_policy_path.read_text(encoding="utf-8")
    assert copied_text != original_policy_text
    assert "ANALYSIS_REVIEW_POLICY_CANDIDATE" in copied_text
    changed_parameters = {
        name
        for name in (
            "followSpeedKmh",
            "maxPathErrorM",
            "lookAheadDistanceM",
            "pathSmoothingDistanceM",
            "maxSteeringDelta",
        )
        if _numeric_assignment_value(copied_text, name) < _numeric_assignment_value(original_policy_text, name)
    }
    assert changed_parameters
    assert _numeric_assignment_value(copied_text, "followSpeedKmh") <= 3.5
    assert _numeric_assignment_value(copied_text, "maxPathErrorM") <= 0.8
    assert _numeric_assignment_value(copied_text, "lookAheadDistanceM") <= 1.0
    assert _numeric_assignment_value(copied_text, "pathSmoothingDistanceM") <= 0.25
    assert _numeric_assignment_value(copied_text, "maxSteeringDelta") <= 0.06
    compile(copied_text, str(copied_policy_path), "exec")
    assert not (review_policy_dir / "__pycache__").exists()
    assert not (review_policy_dir / ".DS_Store").exists()

    recommendations = _read_json(review_dir / "recommendations.json")
    manifest = _read_json(review_dir / "manifest.json")
    expected_artifacts = {
        "policy": {"generated": True, "path": "runs/000001/review/0001/policy"},
        "environment": {"generated": False, "path": None},
    }
    assert payload["recommendation_type"] == "policy_review"
    assert payload["recommendation_type"] == recommendations["recommendation_type"]
    assert recommendations["recommendation_type"] == "policy_review"
    assert recommendations["reason"] == "주행 정책 검토가 필요한 실패 근거가 확인되었습니다."
    assert recommendations["recommendations"] == payload["recommendations"]
    assert recommendations["recommendations"]
    recommendation = recommendations["recommendations"][0]
    assert recommendation["target"] == "policy"
    assert recommendation["recommendation"]
    assert "llm_recommendation" not in recommendation
    assert isinstance(recommendation["proposed_change"]["content"], dict)
    assert payload["modified_policy_json"][0]["source_recommendation_id"] == recommendation["id"]
    assert recommendations["artifacts"] == expected_artifacts
    assert manifest["artifacts"] == expected_artifacts
    assert "runs/000001/review/0001/policy/__init__.py" in manifest["generated_files"]
    assert "runs/000001/review/0001/policy/path_follower.py" in manifest["generated_files"]


def test_successful_policy_recommendation_reason_avoids_failure_wording(tmp_path: Path) -> None:
    """Successful runs with safety evidence use a non-failure top-level recommendation reason."""
    project = tmp_path / "Project1"
    _write_policy(project)
    _write_episode(
        project,
        {"success": True, "goal_reached": True, "penalty_region_violation_count": 2},
        '{"event_type": "Repath"}\n{"event_type": "Repath"}\n',
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    recommendations = _read_json(review_dir / "recommendations.json")
    assert payload["recommendation_type"] == "policy_review"
    assert recommendations["recommendation_type"] == "policy_review"
    assert recommendations["reason"] == (
        "주행은 성공했지만, 패널티 구역 침범과 경로 재탐색 반복 등 정책 검토가 필요한 근거가 확인되었습니다."
    )
    assert "실패 근거" not in recommendations["reason"]
    assert "실패 근거" not in payload["summary"]["message"]
    assert "실패 근거" not in payload["analysis_text"]
    assert all("실패 근거" not in recommendation["reason"] for recommendation in payload["recommendations"])


def test_environment_recommendation_copies_and_modifies_only_root_scenario(tmp_path: Path) -> None:
    """Environment recommendations modify only review/scenario.json."""
    project = tmp_path / "Project1"
    scenario = {
        "schema": "scenario",
        "version": 1,
        "corridor": {"walkway_width_m": 2.0, "segments": []},
        "obstacles": {
            "min_clear_width_m": 0.9,
            "placements": [{"id": "box_1", "allow_blocking": True}],
        },
    }
    _write_scenario(project, scenario)
    snapshot_dir = project / "runs" / "000001" / "snapshot"
    snapshot_dir.mkdir(parents=True)
    (snapshot_dir / "scenario.json").write_text(json.dumps({"scenario_id": "snapshot"}), encoding="utf-8")
    _write_episode(
        project,
        {
            "schema": "episode_result",
            "version": 1,
            "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
            "metrics": {},
            "event_summary": {"by_type": {"StaticObstacleCollision": 1}},
        },
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    review_scenario_path = review_dir / "scenario.json"
    assert review_scenario_path.is_file()
    assert _read_json(project / "scenario.json") == scenario
    review_scenario = _read_json(review_scenario_path)
    assert review_scenario != scenario
    assert review_scenario["obstacles"]["min_clear_width_m"] > scenario["obstacles"]["min_clear_width_m"]
    assert review_scenario["obstacles"]["placements"][0]["allow_blocking"] is False
    assert review_scenario["corridor"]["walkway_width_m"] > scenario["corridor"]["walkway_width_m"]
    assert not any(key.startswith("_") for key in review_scenario)

    recommendations = _read_json(review_dir / "recommendations.json")
    manifest = _read_json(review_dir / "manifest.json")
    expected_artifacts = {
        "policy": {"generated": False, "path": None},
        "environment": {"generated": True, "path": "runs/000001/review/0001/scenario.json"},
    }
    assert payload["recommendation_type"] == "environment_review"
    assert payload["recommendation_type"] == recommendations["recommendation_type"]
    assert recommendations["recommendation_type"] == "environment_review"
    assert recommendations["reason"] == "환경 또는 장애물 배치와 관련된 실패 근거가 확인되었습니다."
    assert recommendations["recommendations"] == payload["recommendations"]
    assert recommendations["recommendations"]
    recommendation = recommendations["recommendations"][0]
    assert recommendation["target"] == "environment"
    assert recommendation["recommendation"]
    assert "llm_recommendation" not in recommendation
    assert isinstance(recommendation["proposed_change"]["content"], dict)
    assert payload["modified_environment_json"][0]["source_recommendation_id"] == recommendation["id"]
    assert recommendations["artifacts"] == expected_artifacts
    assert manifest["artifacts"] == expected_artifacts
    assert "runs/000001/review/0001/scenario.json" in manifest["generated_files"]


def test_environment_recommendation_accepts_bom_scenario_and_records_generated_file(tmp_path: Path) -> None:
    """Environment candidates are created from BOM-encoded root scenario.json."""
    project = tmp_path / "Project1"
    scenario = {
        "schema": "scenario",
        "version": 1,
        "corridor": {"walkway_width_m": 2.0},
        "obstacles": {"min_clear_width_m": 0.9, "placements": [{"allow_blocking": True}]},
    }
    original_bytes = _write_bom_scenario(project, scenario)
    _write_episode(
        project,
        {
            "schema": "episode_result",
            "version": 1,
            "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
            "metrics": {},
            "event_summary": {"by_type": {"StaticObstacleCollision": 1}},
        },
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    review_dir = project / "runs" / "000001" / "review" / "0001"
    review_scenario_path = review_dir / "scenario.json"
    assert review_scenario_path.is_file()
    assert (project / "scenario.json").read_bytes() == original_bytes
    review_scenario = _read_json(review_scenario_path)
    assert review_scenario["obstacles"]["min_clear_width_m"] > scenario["obstacles"]["min_clear_width_m"]
    assert review_scenario["obstacles"]["placements"][0]["allow_blocking"] is False

    recommendations = _read_json(review_dir / "recommendations.json")
    manifest = _read_json(review_dir / "manifest.json")
    assert recommendations["artifacts"]["environment"]["generated"] is True
    assert manifest["artifacts"]["environment"]["generated"] is True
    assert "runs/000001/review/0001/scenario.json" in manifest["generated_files"]


def test_policy_recommendation_modifies_snake_case_defaults_and_inserts_runtime_caps(tmp_path: Path) -> None:
    """Policy candidates cap snake_case defaults and runtime control_spec overrides."""
    project = tmp_path / "Project1"
    original_policy_path = _write_nested_bom_path_follower_policy(project)
    original_bytes = original_policy_path.read_bytes()
    _write_episode(
        project,
        {"success": False, "goal_reached": False, "penalty_region_violation_count": 1},
        '{"event_type": "PenaltyRegionViolation"}\n',
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    review_dir = project / "runs" / "000001" / "review" / "0001"
    copied_policy_path = review_dir / "policy" / "policies" / "path_follower.py"
    assert original_policy_path.read_bytes() == original_bytes
    copied_text = copied_policy_path.read_text(encoding="utf-8-sig")
    assert copied_text != original_policy_path.read_text(encoding="utf-8-sig")
    assert "follow_speed_kmh: float = 3.5" in copied_text
    assert "look_ahead_distance_m: float = 1" in copied_text
    assert "path_smoothing_distance_m: float = 0.25" in copied_text
    assert "max_steering_delta: float = 0.06" in copied_text
    assert "max_path_error_m: float = 0.8" in copied_text
    assert "self.followSpeedKmh = min(self.followSpeedKmh, 3.5)" in copied_text
    assert "self.maxPathErrorM = min(self.maxPathErrorM, 0.8)" in copied_text
    assert "self.lookAheadDistanceM = min(self.lookAheadDistanceM, 1)" in copied_text
    assert "self.pathSmoothingDistanceM = min(self.pathSmoothingDistanceM, 0.25)" in copied_text
    assert "self.maxSteeringDelta = min(self.maxSteeringDelta, 0.06)" in copied_text
    assert copied_text.count("ANALYSIS_REVIEW_POLICY_CANDIDATE") == 1
    compile(copied_policy_path.read_text(encoding="utf-8-sig"), str(copied_policy_path), "exec")

    recommendations = _read_json(review_dir / "recommendations.json")
    assert not any("syntax check failed" in warning.casefold() for warning in recommendations["artifact_warnings"])


def test_policy_recommendation_warns_when_copy_has_no_supported_parameter_changes(tmp_path: Path) -> None:
    """Copy-only policy candidates leave a warning when no safe edit point is found."""
    project = tmp_path / "Project1"
    _write_unmodifiable_policy(project)
    _write_episode(
        project,
        {"success": False, "goal_reached": False, "penalty_region_violation_count": 1},
        '{"event_type": "PenaltyRegionViolation"}\n',
    )

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    review_dir = project / "runs" / "000001" / "review" / "0001"
    recommendations = _read_json(review_dir / "recommendations.json")
    warning_text = " ".join(recommendations["artifact_warnings"])
    assert "copied" in warning_text
    assert "supported policy parameters" in warning_text
    assert recommendations["artifacts"]["policy"]["generated"] is True


def test_none_and_insufficient_data_do_not_create_candidate_artifacts(tmp_path: Path) -> None:
    """No-op recommendation types keep optional candidate artifacts absent."""
    project = tmp_path / "Project1"
    _write_policy(project)
    _write_scenario(project, {"schema": "scenario", "version": 1})
    _write_episode(project, {"success": True, "goal_reached": True})

    response = _post_analysis(project)

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    recommendations = _read_json(review_dir / "recommendations.json")
    assert payload["recommendation_type"] == "none"
    assert payload["recommendation_type"] == recommendations["recommendation_type"]
    assert recommendations["recommendation_type"] == "none"
    assert recommendations["reason"] == "정책 또는 환경 수정이 필요하다고 판단할 만한 반복 근거가 확인되지 않았습니다."
    assert recommendations["recommendations"] == []
    assert recommendations["artifacts"] == {
        "policy": {"generated": False, "path": None},
        "environment": {"generated": False, "path": None},
    }
    assert not (review_dir / "policy").exists()
    assert not (review_dir / "scenario.json").exists()

    empty_project = tmp_path / "EmptyProject"
    (empty_project / "runs" / "000001").mkdir(parents=True)
    response = _post_analysis(empty_project)

    assert response.status_code == 200, response.text
    empty_payload = response.json()
    empty_review_dir = empty_project / "runs" / "000001" / "review" / "0001"
    empty_recommendations = _read_json(empty_review_dir / "recommendations.json")
    assert empty_payload["recommendation_type"] == "insufficient_data"
    assert empty_payload["recommendation_type"] == empty_recommendations["recommendation_type"]
    assert empty_recommendations["recommendation_type"] == "insufficient_data"
    assert empty_recommendations["reason"] == "분석에 필요한 실행 로그가 부족하여 정책 또는 환경 수정 여부를 판단하기 어렵습니다."
    assert empty_recommendations["recommendations"] == []
    assert empty_recommendations["artifacts"] == {
        "policy": {"generated": False, "path": None},
        "environment": {"generated": False, "path": None},
    }
    assert not (empty_review_dir / "policy").exists()
    assert not (empty_review_dir / "scenario.json").exists()
