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
    """Ensure public display text does not expose internal evidence plumbing."""
    recommendation_text = "\n".join(
        f"{item.get('title', '')}\n{item.get('reason', '')}\n{item.get('recommendation', '')}"
        for item in payload.get("recommendations", [])
    )
    insight_text = "\n".join(
        f"{item.get('title', '')}\n{item.get('description', '')}" for item in payload.get("insights", [])
    )
    user_text = f"{payload['summary']['message']}\n{recommendation_text}\n{insight_text}"
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
        insights=[],
        patterns=[],
        recommendations=[],
        warnings=[],
    )


def _write_episode(project: Path, episode_id: str, result: dict, events: str = "") -> None:
    episode_dir = project / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")
    if events:
        (episode_dir / "events.jsonl").write_text(events, encoding="utf-8")
    _upsert_summary_row(project, _summary_row_from_result(episode_id, result, events=events))


def _write_summary(project: Path, summary: dict) -> None:
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").write_text(json.dumps(summary), encoding="utf-8")


def _write_summary_rows(project: Path, rows: list[dict]) -> None:
    """Create a run summary with dashboard rows."""
    _write_summary(
        project,
        {
            "schema": "run_summary",
            "version": 1,
            "run": {"run_id": "000001", "project_id": project.name},
            "rows": rows,
        },
    )


def _upsert_summary_row(project: Path, row: dict) -> None:
    """Keep episode fixtures paired with the summary rows used by the public API."""
    run_dir = project / "runs" / "000001"
    summary_path = run_dir / "summary.json"
    if summary_path.is_file():
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
    else:
        summary = {
            "schema": "run_summary",
            "version": 1,
            "run": {"run_id": "000001", "project_id": project.name},
            "rows": [],
        }
    rows = [item for item in summary.get("rows", []) if item.get("episode_id") != row["episode_id"]]
    rows.append(row)
    summary["rows"] = sorted(rows, key=lambda item: str(item.get("episode_id", "")))
    _write_summary(project, summary)


def _summary_row_from_result(episode_id: str, result: dict, *, events: str = "") -> dict:
    """Build the matching summary.json row for an episode result fixture."""
    summary = result.get("summary") if isinstance(result.get("summary"), dict) else {}
    result_metrics = result.get("metrics") if isinstance(result.get("metrics"), dict) else {}
    metrics = dict(result_metrics)
    for key in (
        "goal_reached",
        "goal_threshold_m",
        "blocked_region_collision_count",
        "pedestrian_collision_count",
        "static_obstacle_collision_count",
        "near_miss_count",
        "repath_count",
        "robot_tip_over_count",
        "blocked_region_violation_count",
        "penalty_region_violation_count",
        "duration_s",
    ):
        if key in result and key not in metrics:
            metrics[key] = result[key]
        if key in summary and key not in metrics:
            metrics[key] = summary[key]
    for key, value in _event_counts(events).items():
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
    duration_s = result.get("duration_s", metrics.get("duration_s", 0.0))
    return {
        "episode_id": episode_id,
        "outcome": outcome,
        "terminal_reason": summary.get("terminal_reason") or result.get("terminal_reason") or result.get("failure_type", ""),
        "duration_s": duration_s,
        "metrics": metrics,
    }


def _event_counts(events: str) -> dict[str, int]:
    """Return summary-row metric counts from JSONL event fixture text."""
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

    assert empty_response.status_code == 400
    assert missing_body_response.status_code == 400
    assert invalid_run_response.status_code == 400
    assert empty_response.json() == {
        "schema": "analysis_run_response_v2",
        "version": 2,
        "status": "failed",
        "error": {
            "code": "INVALID_ANALYSIS_REQUEST",
            "message": "분석 요청 형식이 올바르지 않습니다.",
            "phase": "request_validation",
        },
        "warnings": [],
    }
    assert "run_id" not in missing_body_response.json()
    assert invalid_run_response.json()["run_id"] == "latest"


def test_v2_analysis_run_rejects_unknown_extra_field() -> None:
    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": "X:/missing", "run_id": "000001", "unexpected": "value"},
    )

    assert response.status_code == 400


def test_non_analysis_v2_validation_errors_keep_fastapi_default() -> None:
    """Only the result analysis endpoint uses the custom failed validation body."""
    response = TestClient(app).post("/api/v2/scenarios/generate", json={})

    assert response.status_code == 422
    assert "detail" in response.json()


def test_v2_analysis_run_summary_rows_match_ue_success_and_display_contract(tmp_path) -> None:
    """Public overview and episode rows reproduce the UE dashboard success rules."""
    project = tmp_path / "Project1"
    _write_summary_rows(
        project,
        [
            {
                "episode_id": "000001",
                "outcome": "Success",
                "terminal_reason": "GoalReached",
                "duration_s": 12.2,
                "metrics": {"goal_reached": 1, "static_obstacle_collision_count": 1},
            },
            {
                "episode_id": "000002",
                "outcome": "Failure",
                "terminal_reason": "Timeout",
                "duration_s": 47.6,
                "metrics": {"goal_reached": 0, "blocked_region_collision_count": 2},
            },
            {
                "episode_id": "000003",
                "outcome": "Failure",
                "terminal_reason": "GoalReached",
                "duration_s": 8.4,
                "metrics": {"goal_reached": 1, "pedestrian_collision_count": 3},
            },
            {
                "episode_id": "000004",
                "outcome": "Success",
                "terminal_reason": "GoalReached",
                "duration_s": 0.0,
                "metrics": {"goal_reached": 1, "goal_threshold_m": 1.0},
            },
            {
                "episode_id": "000005",
                "outcome": "Success",
                "terminal_reason": "GoalReached",
                "duration_s": 5.0,
                "metrics": {"goal_reached": 1, "goal_threshold_m": 1.0},
                "scenario_semantic": {
                    "robot": {
                        "start": {"segment": "main", "along_m": 1.0, "offset_m": 0.0},
                        "goal": {"segment": "main", "along_m": 1.5, "offset_m": 0.0},
                    }
                },
            },
        ],
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["run_overview"] == {
        "total_play_time_s": 73.2,
        "success_rate": 0.4,
        "collision_count": 6,
        "episode_count": 5,
        "display": {
            "total_play_time": "01:13",
            "success_rate": "40%",
            "collision_count": "6회",
        },
    }
    assert payload["metrics"]["success_count"] == 2
    assert payload["metrics"]["failure_count"] == 3
    assert payload["metrics"]["collision_count"] == 6
    assert [item["outcome"] for item in payload["episodes"]] == [
        "success",
        "failure",
        "success",
        "failure",
        "failure",
    ]
    assert payload["episodes"][3]["display"]["outcome"] == "실패"
    assert payload["episodes"][4]["display"]["outcome"] == "실패"


def test_v2_analysis_run_success_rate_display_uses_ue_round_half_up(tmp_path) -> None:
    """The display percent uses UE-style half-up rounding rather than Python bankers rounding."""
    project = tmp_path / "Project1"
    rows = [
        {
            "episode_id": f"{index:06d}",
            "outcome": "Success" if index == 1 else "Failure",
            "terminal_reason": "GoalReached" if index == 1 else "Timeout",
            "duration_s": 7.5,
            "metrics": {"goal_reached": 1 if index == 1 else 0},
        }
        for index in range(1, 9)
    ]
    _write_summary_rows(project, rows)

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["run_overview"]["success_rate"] == 0.125
    assert payload["run_overview"]["display"]["success_rate"] == "13%"
    assert payload["run_overview"]["display"]["total_play_time"] == "01:00"


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
    assert "review_id" not in payload
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
    _write_summary_rows(
        project,
        [
            {
                "episode_id": "000001",
                "outcome": "Success",
                "terminal_reason": "GoalReached",
                "duration_s": 10.0,
                "metrics": {"goal_reached": 1, "static_obstacle_collision_count": 1, "near_miss_count": 2},
            },
            {
                "episode_id": "000002",
                "outcome": "Success",
                "terminal_reason": "GoalReached",
                "duration_s": 10.0,
                "metrics": {"goal_reached": 1, "static_obstacle_collision_count": 1, "near_miss_count": 2},
            },
            {
                "episode_id": "000003",
                "outcome": "Failure",
                "terminal_reason": "Timeout",
                "duration_s": 10.0,
                "metrics": {"goal_reached": 0, "blocked_region_collision_count": 1},
            },
        ],
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 3}
    assert payload["metrics"]["success_count"] == 2
    assert payload["metrics"]["failure_count"] == 1
    assert payload["metrics"]["collision_count"] == 3
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


def test_v2_analysis_run_fills_repath_from_result_event_summary_when_summary_rows_omit_it(tmp_path) -> None:
    """Result event summaries supply public Repath counts when dashboard rows omit that metric."""
    project = tmp_path / "Project1"
    _write_episode(
        project,
        "000001",
        {
            "summary": {"success": False, "goal_reached": False, "terminal_reason": "Timeout"},
            "metrics": {"goal_reached": 0, "duration_s": 60.0},
            "event_summary": {"by_type": {"Repath": 5}},
        },
    )
    _write_episode(
        project,
        "000002",
        {
            "summary": {"success": False, "goal_reached": False, "terminal_reason": "Timeout"},
            "metrics": {"goal_reached": 0, "duration_s": 60.0},
            "event_summary": {"by_type": {"Repath": 4}},
        },
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    report = json.loads((project / "runs" / "000001" / "review" / "0001" / "report.json").read_text(encoding="utf-8"))
    assert payload["metrics"]["repath_count"] == 9
    assert report["metrics"]["repath_count"] == 9


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
    assert "analysis_text" not in payload
    assert "analysis_mode" not in payload
    assert "modified_policy_json" not in payload
    assert "modified_environment_json" not in payload


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
    assert "반복 실패" not in payload["recommendations"][0]["recommendation"]
    assert "경로 재탐색" in payload["recommendations"][0]["recommendation"]
    assert "analysis_text" not in payload


def test_v2_analysis_run_stuck_timeout_uses_specific_public_wording(tmp_path) -> None:
    """Stuck timeout failures explain the stop-before-timeout pattern in public text."""
    project = tmp_path / "Project1"
    _write_episode(
        project,
        "000001",
        {
            "summary": {"success": False, "terminal_reason": "Timeout"},
            "metrics": {"goal_reached": 0, "duration_s": 60.0, "stuck_count": 1},
        },
    )

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    insight = next(item for item in payload["insights"] if "제한 시간" in item["title"])
    assert payload["recommendation_type"] == "policy_review"
    assert payload["summary"]["message"] == "정체 이후 제한 시간 초과로 종료되어 주행 정책 검토가 필요합니다."
    assert insight["title"] == "정체 후 제한 시간 초과"
    assert "정체 신호" in insight["description"]
    assert "제한 시간 내 목표에 도달하지 못했습니다" in insight["description"]
    assert payload["recommendations"][0]["title"] == "정체와 제한 시간 초과 대응 정책 검토"
    assert payload["recommendations"][0]["reason"] == (
        "정체 이후 제한 시간 초과가 확인되어 감속, 정지, 재경로 탐색 조건을 검토할 필요가 있습니다."
    )
    assert "stuck_count" not in payload["metrics"]
    _assert_display_text_hides_internal_evidence(payload)


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
    assert "id" not in payload["recommendations"][0]
    assert "proposed_change" not in payload["recommendations"][0]
    review_dir = project / "runs" / "000001" / "review" / "0001"
    recommendation_artifact = _read_json(review_dir / "recommendations.json")
    detailed_recommendation = recommendation_artifact["recommendations"][0]
    assert recommendation_artifact["modified_environment_json"][0]["source_recommendation_id"] == detailed_recommendation["id"]
    assert recommendation_artifact["modified_environment_json"][0]["target"] == "environment"
    assert recommendation_artifact["modified_policy_json"] == []


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
    assert "modified_policy_json" not in payload
    assert "modified_environment_json" not in payload
    assert _read_json(review_dir / "manifest.json")["artifacts"] == {
        "policy": {"generated": False, "path": None},
        "environment": {"generated": False, "path": None},
    }
    assert "세팅 단계" in payload["summary"]["message"]
    assert "obstacle.road_cone_99" in payload["summary"]["message"]
    assert "analysis_text" not in payload
    assert any(insight["title"] == "세팅 단계 중단" for insight in payload["insights"])
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
    assert "modified_policy_json" not in payload
    assert "modified_environment_json" not in payload
    assert "세팅 단계" in payload["summary"]["message"]
    assert "scenario" in payload["summary"]["message"]
    assert "catalog" in payload["summary"]["message"] or "카탈로그" in payload["summary"]["message"]
    assert "obstacle.road_cone_99" not in payload["summary"]["message"]
    assert all("obstacle.road_cone_99" not in insight["description"] for insight in payload["insights"])
    assert all("asset.delivery" not in insight["description"] for insight in payload["insights"])
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
    assert "analysis_text" not in payload
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
    assert "세팅 단계" in payload["summary"]["message"]
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
    assert "세팅 단계" in payload["summary"]["message"]
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
    assert any("obstacle.road_cone_99" in item["message"] for item in report["evidence"])
    assert any("asset.delivery_cart_01" in item["message"] for item in report["evidence"])
    assert payload["recommendations"] == []
    assert "modified_policy_json" not in payload
    assert "modified_environment_json" not in payload
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
            assert payload["recommendation_type"] == "environment_review"
            assert any(insight["title"] == "충돌 관련 이벤트 확인" for insight in payload["insights"])
        if case_name == "RobotTipOver":
            assert payload["recommendation_type"] == "policy_review"
            assert any(insight["title"] == "정책 검토 우선" for insight in payload["insights"])


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

    assert "analysis_mode" not in response.model_dump(by_alias=True)
    assert fake.calls == []
    recommendations = _read_json(project / "runs" / "000001" / "review" / "0001" / "recommendations.json")
    assert recommendations["recommendations"][0]["id"] == "REC-001"


def test_v2_analysis_agent_uses_valid_llm_recommendation(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_penalty_episode(project, "000001")
    fake = _FakeJsonClient([_llm_analysis(project.name, "000001")])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert "analysis_mode" not in response.model_dump(by_alias=True)
    assert fake.calls[0]["response_name"] == "analysis_recommendations_v2"
    assert response.recommendations[0]["recommendation"] == "Prefer slow down or stop before leaving the walkable region."
    assert "id" not in response.recommendations[0]
    assert "llm_recommendation" not in response.recommendations[0]
    recommendations = _read_json(project / "runs" / "000001" / "review" / "0001" / "recommendations.json")
    assert recommendations["recommendations"][0]["id"] == "REC-LLM-001"
    assert recommendations["modified_policy_json"][0]["source_recommendation_id"] == "REC-LLM-001"


def test_v2_analysis_agent_falls_back_when_llm_evidence_is_invalid(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_penalty_episode(project, "000001")
    fake = _FakeJsonClient([_llm_analysis(project.name, "999999")])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert "analysis_mode" not in response.model_dump(by_alias=True)
    assert "id" not in response.recommendations[0]
    recommendations = _read_json(project / "runs" / "000001" / "review" / "0001" / "recommendations.json")
    assert recommendations["recommendations"][0]["id"] == "REC-001"
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

    assert "analysis_mode" not in response.model_dump(by_alias=True)
    assert "id" not in response.recommendations[0]
    recommendations = _read_json(project / "runs" / "000001" / "review" / "0001" / "recommendations.json")
    assert recommendations["recommendations"][0]["id"] == "REC-001"
    assert recommendations["modified_policy_json"][0]["source_recommendation_id"] == "REC-001"
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
