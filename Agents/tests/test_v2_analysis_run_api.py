from __future__ import annotations

import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app.api import routes
from app.agents.result_analysis_v2 import ResultAnalysisV2Agent
from app.core.settings import Settings
from app.main import app
from app.models.analysis_v2 import AnalysisRunV2Request, AnalysisRunV2Response


FORBIDDEN_USER_TEXT = (
    "주요 근거",
    "근거가 확인되었습니다",
    "근거를 토대로",
    "pipeline.diagnostics",
    "evidence",
)


class _FakeJsonClient:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []

    def generate_json(self, *, system_prompt: str, user_prompt: str, response_name: str):
        self.calls.append(
            {
                "system_prompt": system_prompt,
                "user_prompt": user_prompt,
                "response_name": response_name,
            }
        )
        response = self.responses.pop(0)
        if isinstance(response, Exception):
            raise response
        return response


@pytest.fixture(autouse=True)
def _disable_endpoint_llm(monkeypatch) -> None:
    """Keep endpoint regression tests independent from local provider settings."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")


def _request(project: Path, run_id: str = "000001") -> dict:
    return {"project_path": str(project), "run_id": run_id}


def _request_model(project: Path, run_id: str = "000001") -> AnalysisRunV2Request:
    return AnalysisRunV2Request(project_path=str(project), run_id=run_id)


def _read_json(path: Path) -> dict:
    """Read a generated review JSON fixture."""
    return json.loads(path.read_text(encoding="utf-8"))


def _assert_display_text_hides_internal_evidence(payload: dict) -> None:
    """Ensure summary and analysis text do not expose internal evidence plumbing."""
    user_text = f"{payload['summary']['message']}\n{payload['analysis_text']}"
    for forbidden in FORBIDDEN_USER_TEXT:
        assert forbidden not in user_text


def _empty_analysis_response(run_id: str = "000001") -> AnalysisRunV2Response:
    """Build a minimal response used by route-level runner dispatch tests."""
    return AnalysisRunV2Response(
        run_id=run_id,
        review_id=None,
        analysis_scope={"experiments_count": 0, "runs_count": 0, "episodes_count": 0},
        summary={"overall_judgement": "insufficient_data", "message": "graph runner response"},
        metrics={
            "success_count": 0,
            "failure_count": 0,
            "collision_count": 0,
            "static_obstacle_collision_count": 0,
            "pedestrian_collision_count": 0,
            "near_miss_count": 0,
            "repath_count": 0,
            "robot_tip_over_count": 0,
            "blocked_region_violation_count": 0,
            "penalty_region_violation_count": 0,
        },
        recommendation_type="insufficient_data",
        analysis_text="graph runner response",
    )


def _write_episode(project: Path, episode_id: str, result: dict, events: str = "") -> None:
    episode_dir = project / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")
    if events:
        (episode_dir / "events.jsonl").write_text(events, encoding="utf-8")


def _write_summary(project: Path, summary: dict) -> None:
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True)
    (run_dir / "summary.json").write_text(json.dumps(summary), encoding="utf-8")


def _write_blocked_episode(project: Path, episode_id: str) -> None:
    _write_episode(
        project,
        episode_id,
        {"success": False, "failure_type": "blocked_region_violation"},
        '{"event_type": "blocked_region_violation"}\n',
    )


def _write_penalty_episode(project: Path, episode_id: str) -> None:
    """Create a policy-review episode with penalty-region evidence."""
    _write_episode(
        project,
        episode_id,
        {"success": False, "goal_reached": False, "penalty_region_violation_count": 1},
        '{"event_type": "PenaltyRegionViolation"}\n',
    )


def _write_policy_source(project: Path) -> None:
    """Create a minimal policy source that candidate generation can copy and adjust."""
    policy_dir = project / "policy"
    policy_dir.mkdir(parents=True, exist_ok=True)
    (policy_dir / "path_follower.py").write_text(
        "\n".join(
            [
                "followSpeedKmh = 6.0",
                "maxPathErrorM = 1.4",
                "lookAheadDistanceM = 2.0",
                "pathSmoothingDistanceM = 0.7",
                "maxSteeringDelta = 0.12",
            ]
        ),
        encoding="utf-8",
    )


def _write_setup_failed_episode(project: Path, episode_id: str, *, with_prop_detail: bool = False) -> None:
    """Create a setup-failed episode with optional structured diagnostics."""
    diagnostic = (
        {
            "code": "unknown_prop_id",
            "message": "Static obstacle prop is not registered.",
            "prop_id": "obstacle.road_cone_99",
        }
        if with_prop_detail
        else "World setup failed before simulation start."
    )
    _write_episode(
        project,
        episode_id,
        {
            "summary": {
                "success": False,
                "terminal_reason": "SetupFailed",
                "outcome": "setup aborted before simulation",
            },
            "pipeline": {
                "world_setup_succeeded": False,
                "evaluation_completed": False,
                "diagnostics": [diagnostic],
            },
            "event_summary": {"by_type": {"Repath": 2, "PenaltyRegionViolation": 1}},
        },
    )


def _write_setup_failed_episode_without_details(project: Path, episode_id: str) -> None:
    """Create a setup-failed episode that has no structured diagnostic detail."""
    _write_episode(
        project,
        episode_id,
        {
            "summary": {"success": False, "terminal_reason": "SetupFailed"},
            "pipeline": {
                "world_setup_succeeded": False,
                "evaluation_completed": False,
            },
        },
    )


def _write_setup_failed_episode_with_diagnostic(project: Path, episode_id: str, diagnostic: dict) -> None:
    """Create a setup-failed episode with a caller-supplied structured diagnostic."""
    _write_episode(
        project,
        episode_id,
        {
            "summary": {"success": False, "terminal_reason": "SetupFailed"},
            "pipeline": {
                "world_setup_succeeded": False,
                "evaluation_completed": False,
                "diagnostics": [diagnostic],
            },
        },
    )


def _llm_analysis(project_id: str, evidence_episode_id: str = "000001") -> dict:
    return {
        "summary_message": "LLM recommends policy change.",
        "overall_judgement": "change_recommended",
        "recommendations": [
            {
                "id": "REC-LLM-001",
                "target": "policy",
                "priority": "high",
                "title": "LLM policy recommendation",
                "reason": "Repeated blocked region violation evidence exists.",
                "evidence": [
                    {
                        "experiment_id": project_id,
                        "run_id": "000001",
                        "episode_id": evidence_episode_id,
                    }
                ],
                "llm_recommendation": "Prefer slow down or stop before leaving the walkable region.",
                "proposed_change": {
                    "type": "policy_rule_spec",
                    "content": {
                        "rule_name": "llm_stop_first",
                        "condition": {"blocked_region_violation_repeated": True},
                        "action": "slow_down_or_stop",
                        "priority": "high",
                    },
                },
            }
        ],
    }


def test_v2_analysis_run_requires_project_path_and_run_id() -> None:
    client = TestClient(app)

    empty_response = client.post("/api/v2/analysis/run", json={})
    missing_body_response = client.post("/api/v2/analysis/run")
    invalid_run_response = client.post(
        "/api/v2/analysis/run",
        json={"project_path": "X:/missing", "run_id": "latest"},
    )

    assert empty_response.status_code == 422
    assert missing_body_response.status_code == 422
    assert invalid_run_response.status_code == 422


def test_v2_analysis_run_rejects_unknown_extra_field() -> None:
    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": "X:/missing", "run_id": "000001", "unexpected": "value"},
    )

    assert response.status_code == 422


def test_v2_analysis_run_uses_graph_runner_without_env_flag(monkeypatch, tmp_path) -> None:
    """Verify the v2 analysis route always dispatches through the graph runner."""
    calls: list[AnalysisRunV2Request] = []

    class _FakeGraphRunner:
        def __init__(self, *, settings: Settings) -> None:
            self.settings = settings

        def run(self, request: AnalysisRunV2Request) -> AnalysisRunV2Response:
            calls.append(request)
            return _empty_analysis_response(run_id=request.run_id)

    monkeypatch.setattr(routes, "ResultAnalysisGraphRunnerV2", _FakeGraphRunner)
    project = tmp_path / "Project1"

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    assert [call.run_id for call in calls] == ["000001"]
    assert response.json()["summary"]["message"] == "graph runner response"


def test_v2_analysis_run_missing_requested_run_returns_insufficient_data(tmp_path) -> None:
    project = tmp_path / "Project1"
    project.mkdir()

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["run_id"] == "000001"
    assert payload["review_id"] is None
    assert payload["recommendation_type"] == "insufficient_data"
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert any("run directory does not exist" in warning for warning in payload["warnings"])


def test_v2_analysis_run_records_broken_jsonl_warning(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_episode(
        project,
        "000003",
        {"success": False, "failure_type": "timeout", "near_miss_count": 1},
        '{"event": "near_miss", "distance_m": 0.4}\n{broken json}\n',
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 1}
    assert payload["recommendations"][0]["target"] == "policy"
    assert payload["recommendations"][0]["recommendation"]
    assert any("events.jsonl" in warning for warning in payload["warnings"])


def test_v2_analysis_run_uses_summary_when_episode_results_are_missing(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_summary(
        project,
        {
            "episode_count": 3,
            "success_count": 2,
            "failure_count": 1,
            "collision_count": 1,
            "static_obstacle_collision_count": 2,
            "near_miss_count": 4,
        },
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 3}
    assert payload["metrics"]["success_count"] == 2
    assert payload["metrics"]["failure_count"] == 1
    assert payload["metrics"]["collision_count"] == 1
    assert payload["metrics"]["static_obstacle_collision_count"] == 2
    assert payload["metrics"]["near_miss_count"] == 4
    assert payload["recommendation_type"] == "none"
    assert payload["recommendations"] == []


def test_v2_analysis_run_accepts_prompt_without_changing_log_metrics(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_episode(project, "000001", {"success": True, "goal_reached": True})

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={
            **_request(project),
            "prompt": "장애물 충돌 때문에 실패했는지 중심으로 다시 분석해줘",
        },
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["summary"]["overall_judgement"] == "no_change_needed"
    assert payload["metrics"]["success_count"] == 1
    assert payload["metrics"]["failure_count"] == 0
    assert payload["recommendations"] == []
    assert "사용자 요청 관점" in payload["summary"]["message"]
    assert "obstacle" in payload["summary"]["message"]
    assert "collision" in payload["summary"]["message"]


def test_v2_analysis_run_treats_blank_prompt_like_missing_prompt(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_episode(project, "000001", {"success": True, "goal_reached": True})

    response = TestClient(app).post("/api/v2/analysis/run", json={**_request(project), "prompt": "   "})

    assert response.status_code == 200, response.text
    payload = response.json()
    assert "사용자 요청 관점" not in payload["summary"]["message"]


def test_v2_analysis_run_warns_for_broken_summary_without_500(tmp_path) -> None:
    project = tmp_path / "Project1"
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True)
    (run_dir / "summary.json").write_text("{broken json}", encoding="utf-8")

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert "로그" in payload["summary"]["message"]
    assert "부족" in payload["summary"]["message"]
    assert "판단하기 어렵" in payload["summary"]["message"]
    assert "반복적인 실패 패턴이 확인되지 않아" not in payload["summary"]["message"]
    assert any("summary.json" in warning and "JSON parse failed" in warning for warning in payload["warnings"])


def test_v2_analysis_run_missing_result_basis_uses_insufficient_data_message(tmp_path) -> None:
    """Existing run folders without result or summary basis use a log-shortage message."""
    project = tmp_path / "Project1"
    for episode_id in ("000001", "000002"):
        episode_dir = project / "runs" / "000001" / "episodes" / episode_id
        episode_dir.mkdir(parents=True)
        (episode_dir / "actions.jsonl").write_text('{"action": "noop"}\n', encoding="utf-8")

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["recommendation_type"] == "insufficient_data"
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert "로그" in payload["summary"]["message"]
    assert "부족" in payload["summary"]["message"]
    assert "판단하기 어렵" in payload["summary"]["message"]
    assert "result/events" in payload["summary"]["message"]
    assert "반복적인 실패 패턴이 확인되지 않아" not in payload["summary"]["message"]


def test_v2_analysis_run_warns_for_missing_episode_artifacts(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_episode(project, "000001", {"success": True, "goal_reached": True})

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert not any("result.json is missing" in warning for warning in payload["warnings"])
    assert any("events.jsonl is missing" in warning for warning in payload["warnings"])
    assert any("actions.jsonl is missing" in warning for warning in payload["warnings"])
    assert any("trace.jsonl is missing" in warning for warning in payload["warnings"])


def test_v2_analysis_run_exposes_repath_and_tip_over_metrics(tmp_path) -> None:
    """PascalCase Repath and RobotTipOver events are exposed in response and report metrics."""
    project = tmp_path / "Project1"
    _write_episode(
        project,
        "000001",
        {"success": False, "goal_reached": False},
        '{"event_type": "Repath"}\n{"event_type": "Repath"}\n{"event_type": "RobotTipOver"}\n',
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = json.loads((review_dir / "report.json").read_text(encoding="utf-8"))
    assert payload["metrics"]["repath_count"] == 2
    assert payload["metrics"]["robot_tip_over_count"] == 1
    assert report["metrics"]["repath_count"] == 2
    assert report["metrics"]["robot_tip_over_count"] == 1
    assert any(finding["type"] == "repath" for finding in report["findings"])
    assert any(finding["type"] == "robot_tip_over" for finding in report["findings"])


def test_v2_analysis_run_successful_episodes_do_not_generate_recommendations(tmp_path) -> None:
    project = tmp_path / "Project1"
    for episode_id in ("000001", "000002"):
        _write_episode(project, episode_id, {"success": True, "goal_reached": True, "duration_s": 12.0})

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 2}
    assert payload["summary"]["overall_judgement"] == "no_change_needed"
    assert "반복적인 실패 패턴이 확인되지 않아" in payload["summary"]["message"]
    assert "로그가 부족" not in payload["summary"]["message"]
    assert "판단하기 어렵" not in payload["summary"]["message"]
    assert payload["run_id"] == "000001"
    assert payload["review_id"] == "0001"
    assert payload["recommendation_type"] == "none"
    assert payload["metrics"]["success_count"] == 2
    assert payload["metrics"]["failure_count"] == 0
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []


def test_v2_analysis_run_success_with_policy_evidence_uses_safety_review_wording(tmp_path) -> None:
    """Successful runs with safety evidence avoid failure-only recommendation wording."""
    project = tmp_path / "Project1"
    _write_episode(
        project,
        "000001",
        {"success": True, "goal_reached": True, "penalty_region_violation_count": 2},
        '{"event_type": "Repath"}\n{"event_type": "Repath"}\n',
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["recommendation_type"] == "policy_review"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "주행은 성공했지만" in payload["summary"]["message"]
    assert "실패 근거" not in payload["summary"]["message"]
    assert "실패 근거" not in payload["analysis_text"]
    assert "반복 실패" not in payload["analysis_text"]
    assert "실패 지표" not in payload["analysis_text"]
    assert "반복 실패" not in payload["recommendations"][0]["recommendation"]
    assert "경로 재탐색" in payload["analysis_text"]
    assert "경로 재탐색" in payload["recommendations"][0]["recommendation"]


def test_v2_analysis_run_repeated_blocked_region_generates_environment_recommendation(tmp_path) -> None:
    project = tmp_path / "Project1"
    for episode_id in ("000001", "000002"):
        _write_episode(
            project,
            episode_id,
            {"success": False, "failure_type": "blocked_region_violation"},
            '{"event_type": "blocked_region_violation"}\n{"type": "penalty_region_violation"}\n',
        )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["metrics"]["blocked_region_violation_count"] == 2
    assert payload["metrics"]["penalty_region_violation_count"] == 2
    assert payload["metrics"]["static_obstacle_collision_count"] == 0
    assert payload["run_id"] == "000001"
    assert payload["review_id"] == "0001"
    assert payload["recommendation_type"] == "environment_review"
    assert payload["patterns"][0]["type"] == "blocked_region_violation_repeated"
    assert payload["recommendations"][0]["target"] == "environment"
    assert payload["recommendations"][0]["recommendation"]
    assert "llm_recommendation" not in payload["recommendations"][0]
    assert payload["modified_environment_json"][0]["source_recommendation_id"] == payload["recommendations"][0]["id"]
    assert payload["modified_environment_json"][0]["target"] == "environment"
    assert payload["modified_policy_json"] == []


def test_v2_analysis_run_setup_failed_only_uses_setup_text_and_no_candidates(tmp_path) -> None:
    """Setup-only runs are not treated as policy failures or successful no-change runs."""
    project = tmp_path / "Project1"
    _write_setup_failed_episode(project, "000001", with_prop_detail=True)

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    recommendations = _read_json(review_dir / "recommendations.json")
    finding_types = {finding["type"] for finding in report["findings"]}
    assert payload["recommendation_type"] == "none"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["metrics"]["failure_count"] == 1
    assert "setup_failed" in finding_types
    assert "goal_not_reached" not in finding_types
    assert "penalty_region_violation" not in finding_types
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []
    assert _read_json(review_dir / "manifest.json")["artifacts"] == {
        "policy": {"generated": False, "path": None},
        "environment": {"generated": False, "path": None},
    }
    assert "세팅 단계" in payload["summary"]["message"]
    assert "obstacle.road_cone_99" in payload["summary"]["message"]
    assert "정책 성능" in payload["analysis_text"]
    assert "obstacle.road_cone_99" in payload["analysis_text"]
    assert "환경 카탈로그" in payload["analysis_text"]
    _assert_display_text_hides_internal_evidence(payload)
    assert "수정 추천을 생성하지 않았습니다" not in payload["summary"]["message"]
    assert "수정이 필요하다고 판단할 만한" not in recommendations["reason"]
    assert "setup" in recommendations["reason"].casefold()
    assert recommendations["evidence_ids"] == ["EV-0001"]
    assert report["evidence"][0]["message"]
    assert "obstacle.road_cone_99" in report["evidence"][0]["message"]
    assert not (review_dir / "policy").exists()
    assert not (review_dir / "scenario.json").exists()


def test_v2_analysis_run_setup_failed_only_without_details_uses_generic_checks(tmp_path) -> None:
    """Setup-only runs without detail do not invent prop or asset identifiers."""
    project = tmp_path / "Project1"
    _write_setup_failed_episode_without_details(project, "000001")

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    finding_types = {finding["type"] for finding in report["findings"]}
    assert payload["recommendation_type"] == "none"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert finding_types == {"setup_failed"}
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []
    assert "세팅 단계" in payload["summary"]["message"]
    assert "scenario" in payload["analysis_text"]
    assert "catalog" in payload["analysis_text"] or "카탈로그" in payload["analysis_text"]
    assert "obstacle.road_cone_99" not in payload["summary"]["message"]
    assert "obstacle.road_cone_99" not in payload["analysis_text"]
    assert "asset.delivery" not in payload["analysis_text"]
    _assert_display_text_hides_internal_evidence(payload)


def test_v2_analysis_run_setup_failed_with_success_still_needs_setup_review(tmp_path) -> None:
    """Setup failures mixed with successes are not ordinary no-change runs."""
    project = tmp_path / "Project1"
    _write_setup_failed_episode(project, "000001")
    _write_episode(project, "000002", {"success": True, "goal_reached": True})

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["recommendation_type"] == "none"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["metrics"]["success_count"] == 1
    assert payload["metrics"]["failure_count"] == 1
    assert payload["recommendations"] == []
    assert "세팅 단계" in payload["summary"]["message"]
    assert "별도 수정 후보를 생성하지 않는 것이 적절" not in payload["analysis_text"]
    _assert_display_text_hides_internal_evidence(payload)
    review_dir = project / "runs" / "000001" / "review" / "0001"
    assert not (review_dir / "policy").exists()
    assert not (review_dir / "scenario.json").exists()


def test_v2_analysis_run_setup_failed_mixed_with_policy_failure_uses_runtime_policy_failure(tmp_path) -> None:
    """Setup failures do not block policy review from runtime failure episodes."""
    project = tmp_path / "Project1"
    _write_policy_source(project)
    _write_setup_failed_episode(project, "000001")
    _write_penalty_episode(project, "000002")

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    finding_types = {finding["type"] for finding in report["findings"]}
    assert payload["recommendation_type"] == "policy_review"
    assert "setup_failed" in finding_types
    assert "penalty_region_violation" in finding_types
    assert "goal_not_reached" not in finding_types
    assert payload["recommendations"][0]["target"] == "policy"
    assert "세팅 단계" in payload["analysis_text"]
    _assert_display_text_hides_internal_evidence(payload)
    recommendations = _read_json(review_dir / "recommendations.json")
    setup_evidence_id = next(finding["evidence_ids"][0] for finding in report["findings"] if finding["type"] == "setup_failed")
    runtime_evidence_id = next(
        finding["evidence_ids"][0] for finding in report["findings"] if finding["type"] == "penalty_region_violation"
    )
    assert setup_evidence_id not in recommendations["evidence_ids"]
    assert recommendations["evidence_ids"] == [runtime_evidence_id]
    assert (review_dir / "policy").is_dir()
    assert not (review_dir / "scenario.json").exists()


def test_v2_analysis_run_setup_failed_mixed_with_environment_failure_uses_runtime_environment_failure(tmp_path) -> None:
    """Setup failures do not block environment review from runtime failure episodes."""
    project = tmp_path / "Project1"
    (project / "scenario.json").parent.mkdir(parents=True, exist_ok=True)
    (project / "scenario.json").write_text(json.dumps({"obstacles": {"placements": []}}), encoding="utf-8")
    _write_setup_failed_episode(project, "000001")
    _write_episode(
        project,
        "000002",
        {
            "summary": {"success": False, "terminal_reason": "StaticObstacleCollision"},
            "event_summary": {"by_type": {"StaticObstacleCollision": 1}},
        },
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    finding_types = {finding["type"] for finding in report["findings"]}
    assert payload["recommendation_type"] == "environment_review"
    assert "setup_failed" in finding_types
    assert "static_obstacle_collision" in finding_types
    assert "goal_not_reached" not in finding_types
    assert payload["recommendations"][0]["target"] == "environment"
    assert "세팅 단계" in payload["analysis_text"]
    _assert_display_text_hides_internal_evidence(payload)
    recommendations = _read_json(review_dir / "recommendations.json")
    setup_evidence_id = next(finding["evidence_ids"][0] for finding in report["findings"] if finding["type"] == "setup_failed")
    runtime_evidence_id = next(
        finding["evidence_ids"][0] for finding in report["findings"] if finding["type"] == "static_obstacle_collision"
    )
    assert setup_evidence_id not in recommendations["evidence_ids"]
    assert recommendations["evidence_ids"] == [runtime_evidence_id]
    assert not (review_dir / "policy").exists()
    assert (review_dir / "scenario.json").is_file()


def test_v2_analysis_run_multiple_setup_failures_show_logged_details_only(tmp_path) -> None:
    """Multiple setup failures keep setup findings without making runtime recommendations."""
    project = tmp_path / "Project1"
    _write_setup_failed_episode_with_diagnostic(
        project,
        "000001",
        {
            "code": "unknown_prop_id",
            "message": "Static obstacle prop is not registered.",
            "prop_id": "obstacle.road_cone_99",
        },
    )
    _write_setup_failed_episode_with_diagnostic(
        project,
        "000002",
        {
            "code": "asset_load_failed",
            "message": "Asset failed to load.",
            "asset_id": "asset.delivery_cart_01",
        },
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = _read_json(review_dir / "report.json")
    setup_finding = next(finding for finding in report["findings"] if finding["type"] == "setup_failed")
    assert payload["recommendation_type"] == "none"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert setup_finding["evidence_ids"] == ["EV-0001", "EV-0002"]
    assert "goal_not_reached" not in {finding["type"] for finding in report["findings"]}
    assert "obstacle.road_cone_99" in payload["analysis_text"]
    assert "asset.delivery_cart_01" in payload["analysis_text"]
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []
    _assert_display_text_hides_internal_evidence(payload)
    assert not (review_dir / "policy").exists()
    assert not (review_dir / "scenario.json").exists()


def test_v2_analysis_run_terminal_reason_controls_goal_not_reached_finding(tmp_path) -> None:
    """Only explicit GoalNotReached creates the goal_not_reached finding."""
    cases = [
        ("GoalNotReached", {"success": False, "terminal_reason": "GoalNotReached"}, {"goal_not_reached"}),
        ("Timeout", {"success": False, "terminal_reason": "Timeout"}, {"timeout"}),
        ("StaticObstacleCollision", {"success": False, "terminal_reason": "StaticObstacleCollision"}, {"static_obstacle_collision"}),
        ("BlockedRegionCollision", {"success": False, "terminal_reason": "BlockedRegionCollision"}, {"blocked_region_collision"}),
        ("RobotTipOver", {"success": False, "terminal_reason": "RobotTipOver"}, {"robot_tip_over"}),
        ("UnknownTerminal", {"success": False, "terminal_reason": "UnexpectedShutdown"}, set()),
        ("MissingTerminal", {"success": False}, set()),
        ("GoalReached", {"success": True, "terminal_reason": "GoalReached"}, set()),
    ]

    for case_name, summary, expected_findings in cases:
        project = tmp_path / case_name
        _write_episode(project, "000001", {"summary": summary})

        response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

        assert response.status_code == 200, response.text
        payload = response.json()
        report = _read_json(project / "runs" / "000001" / "review" / "0001" / "report.json")
        finding_types = {finding["type"] for finding in report["findings"]}
        assert expected_findings <= finding_types
        if case_name != "GoalNotReached":
            assert "goal_not_reached" not in finding_types
        if case_name == "BlockedRegionCollision":
            assert "차단 구역" in payload["analysis_text"]
            assert "차단 구역 충돌, 근접 위험 항목은 확인되지 않았습니다" not in payload["analysis_text"]
        if case_name == "RobotTipOver":
            assert "로봇 전도" in payload["analysis_text"]


def test_v2_analysis_agent_keeps_rule_based_mode_when_llm_disabled(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_blocked_episode(project, "000001")
    _write_blocked_episode(project, "000002")
    fake = _FakeJsonClient([_llm_analysis(project.name)])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert response.analysis_mode == "rule_based"
    assert fake.calls == []
    assert response.recommendations[0]["id"] == "REC-001"


def test_v2_analysis_agent_uses_valid_llm_recommendation(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_penalty_episode(project, "000001")
    fake = _FakeJsonClient([_llm_analysis(project.name, "000001")])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert response.analysis_mode == "llm"
    assert response.recommendations[0]["id"] == "REC-LLM-001"
    assert response.recommendations[0]["recommendation"] == "Prefer slow down or stop before leaving the walkable region."
    assert "llm_recommendation" not in response.recommendations[0]
    assert response.modified_policy_json[0]["source_recommendation_id"] == "REC-LLM-001"
    assert fake.calls[0]["response_name"] == "analysis_recommendations_v2"


def test_v2_analysis_agent_falls_back_when_llm_evidence_is_invalid(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_penalty_episode(project, "000001")
    fake = _FakeJsonClient([_llm_analysis(project.name, "999999")])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert response.analysis_mode == "fallback"
    assert response.recommendations[0]["id"] == "REC-001"
    assert any("LLM recommendation failed; rule-based recommendation fallback was used." in warning for warning in response.warnings)


def test_v2_analysis_agent_falls_back_when_llm_json_fails(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_penalty_episode(project, "000001")
    fake = _FakeJsonClient([ValueError("invalid json")])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert response.analysis_mode == "fallback"
    assert response.recommendations[0]["id"] == "REC-001"
    assert response.modified_policy_json[0]["source_recommendation_id"] == "REC-001"
    assert any("LLM recommendation failed; rule-based recommendation fallback was used." in warning for warning in response.warnings)


def test_v2_analysis_run_openapi_requires_project_path_and_run_id() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    operation = schema["paths"]["/api/v2/analysis/run"]["post"]
    request_ref = operation["requestBody"]["content"]["application/json"]["schema"]["$ref"]
    component_name = request_ref.rsplit("/", 1)[-1]
    request_schema = schema["components"]["schemas"][component_name]
    response_ref = operation["responses"]["200"]["content"]["application/json"]["schema"]["$ref"]
    response_component_name = response_ref.rsplit("/", 1)[-1]
    response_schema = schema["components"]["schemas"][response_component_name]

    assert "/api/v1/analysis/run" in schema["paths"]
    assert request_schema["required"] == ["project_path", "run_id"]
    assert set(request_schema["properties"]) == {"project_path", "run_id", "prompt"}
    assert "review_id" in response_schema["properties"]
    assert "run_id" in response_schema["properties"]
    assert "recommendation_type" in response_schema["properties"]
    assert "static_obstacle_collision_count" in schema["components"]["schemas"]["AnalysisMetricsV2"]["properties"]
