from __future__ import annotations

import json
from pathlib import Path

from fastapi.testclient import TestClient

from app.agents.result_analysis_v2 import ResultAnalysisV2Agent
from app.core.settings import Settings
from app.main import app
from app.models.analysis_v2 import AnalysisRunV2Request


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


def _request(project: Path, run_id: str = "000001") -> dict:
    return {"project_path": str(project), "run_id": run_id}


def _request_model(project: Path, run_id: str = "000001") -> AnalysisRunV2Request:
    return AnalysisRunV2Request(project_path=str(project), run_id=run_id)


def _write_episode(project: Path, episode_id: str, result: dict, events: str = "") -> None:
    episode_dir = project / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(json.dumps(result), encoding="utf-8")
    if events:
        (episode_dir / "events.jsonl").write_text(events, encoding="utf-8")


def _write_blocked_episode(project: Path, episode_id: str) -> None:
    _write_episode(
        project,
        episode_id,
        {"success": False, "failure_type": "blocked_region_violation"},
        '{"event_type": "blocked_region_violation"}\n',
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


def test_v2_analysis_run_missing_requested_run_returns_insufficient_data(tmp_path) -> None:
    project = tmp_path / "Project1"
    project.mkdir()

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
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
    assert payload["recommendations"] == []
    assert any("events.jsonl" in warning for warning in payload["warnings"])


def test_v2_analysis_run_successful_episodes_do_not_generate_recommendations(tmp_path) -> None:
    project = tmp_path / "Project1"
    for episode_id in ("000001", "000002"):
        _write_episode(project, episode_id, {"success": True, "goal_reached": True, "duration_s": 12.0})

    response = TestClient(app).post("/api/v2/analysis/run", json=_request(project))

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 2}
    assert payload["summary"]["overall_judgement"] == "no_change_needed"
    assert payload["metrics"]["success_count"] == 2
    assert payload["metrics"]["failure_count"] == 0
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []


def test_v2_analysis_run_repeated_blocked_region_generates_policy_recommendation(tmp_path) -> None:
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
    assert payload["patterns"][0]["type"] == "blocked_region_violation_repeated"
    assert payload["recommendations"][0]["target"] == "policy"
    assert payload["recommendations"][0]["llm_recommendation"]
    assert payload["modified_policy_json"][0]["source_recommendation_id"] == payload["recommendations"][0]["id"]
    assert payload["modified_policy_json"][0]["target"] == "policy"
    assert payload["modified_environment_json"] == []


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
    _write_blocked_episode(project, "000001")
    _write_blocked_episode(project, "000002")
    fake = _FakeJsonClient([_llm_analysis(project.name, "000001")])
    agent = ResultAnalysisV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run(_request_model(project))

    assert response.analysis_mode == "llm"
    assert response.recommendations[0]["id"] == "REC-LLM-001"
    assert response.modified_policy_json[0]["source_recommendation_id"] == "REC-LLM-001"
    assert fake.calls[0]["response_name"] == "analysis_recommendations_v2"


def test_v2_analysis_agent_falls_back_when_llm_evidence_is_invalid(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_blocked_episode(project, "000001")
    _write_blocked_episode(project, "000002")
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
    _write_blocked_episode(project, "000001")
    _write_blocked_episode(project, "000002")
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

    assert "/api/v1/analysis/run" in schema["paths"]
    assert request_schema["required"] == ["project_path", "run_id"]
    assert set(request_schema["properties"]) == {"project_path", "run_id"}
