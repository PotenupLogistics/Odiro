from __future__ import annotations

import json

from fastapi.testclient import TestClient

from app.agents.result_analysis_v2 import ResultAnalysisV2Agent
from app.core.settings import Settings
from app.main import app


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


def _write_blocked_episode(experiments, episode_id: str) -> None:
    episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": False, "failure_type": "blocked_region_violation"}),
        encoding="utf-8",
    )
    (episode_dir / "events.jsonl").write_text(
        '{"event_type": "blocked_region_violation"}\n',
        encoding="utf-8",
    )


def _llm_analysis(evidence_episode_id: str = "000001") -> dict:
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
                        "experiment_id": "Experiment1",
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


def test_v2_analysis_run_empty_body_returns_insufficient_data(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(tmp_path / "experiments"))

    response = TestClient(app).post("/api/v2/analysis/run", json={})

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["version"] == 2
    assert payload["status"] == "success"
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []
    assert "분석 가능한 experiment 실행 결과를 찾지 못했습니다." in payload["warnings"]


def test_v2_analysis_run_empty_experiments_root_returns_insufficient_data(monkeypatch, tmp_path) -> None:
    experiments = tmp_path / "experiments"
    experiments.mkdir()
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(experiments))

    response = TestClient(app).post("/api/v2/analysis/run", json={})

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 0, "runs_count": 0, "episodes_count": 0}
    assert payload["summary"]["overall_judgement"] == "insufficient_data"
    assert payload["recommendations"] == []


def test_v2_analysis_run_accepts_missing_body(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(tmp_path / "experiments"))

    response = TestClient(app).post("/api/v2/analysis/run")

    assert response.status_code == 200, response.text
    assert response.json()["schema"] == "analysis_run_response_v2"


def test_v2_analysis_run_records_broken_jsonl_warning(monkeypatch, tmp_path) -> None:
    experiments = tmp_path / "experiments"
    episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / "000003"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": False, "failure_type": "timeout", "near_miss_count": 1}),
        encoding="utf-8",
    )
    (episode_dir / "events.jsonl").write_text(
        '{"event": "near_miss", "distance_m": 0.4}\n{broken json}\n',
        encoding="utf-8",
    )
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(experiments))

    response = TestClient(app).post("/api/v2/analysis/run", json={})

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"]["experiments_count"] == 1
    assert payload["analysis_scope"]["runs_count"] == 1
    assert payload["analysis_scope"]["episodes_count"] == 1
    assert payload["recommendations"] == []
    assert any("events.jsonl" in warning for warning in payload["warnings"])


def test_v2_analysis_run_successful_episodes_do_not_generate_recommendations(monkeypatch, tmp_path) -> None:
    experiments = tmp_path / "experiments"
    for episode_id in ("000001", "000002"):
        episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / episode_id
        episode_dir.mkdir(parents=True)
        (episode_dir / "result.json").write_text(
            json.dumps({"success": True, "goal_reached": True, "duration_s": 12.0}),
            encoding="utf-8",
        )
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(experiments))

    response = TestClient(app).post("/api/v2/analysis/run", json={})

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 2}
    assert payload["summary"]["overall_judgement"] == "no_change_needed"
    assert payload["metrics"]["success_count"] == 2
    assert payload["metrics"]["failure_count"] == 0
    assert payload["recommendations"] == []
    assert payload["modified_policy_json"] == []
    assert payload["modified_environment_json"] == []


def test_v2_analysis_run_repeated_blocked_region_generates_policy_recommendation(monkeypatch, tmp_path) -> None:
    experiments = tmp_path / "experiments"
    for episode_id in ("000001", "000002"):
        episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / episode_id
        episode_dir.mkdir(parents=True)
        (episode_dir / "result.json").write_text(
            json.dumps({"success": False, "failure_type": "blocked_region_violation"}),
            encoding="utf-8",
        )
        (episode_dir / "events.jsonl").write_text(
            '{"event_type": "blocked_region_violation"}\n{"type": "penalty_region_violation"}\n',
            encoding="utf-8",
        )
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(experiments))

    response = TestClient(app).post("/api/v2/analysis/run", json={})

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
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    fake = _FakeJsonClient([_llm_analysis()])
    agent = ResultAnalysisV2Agent(
        experiments_root=experiments,
        settings=Settings(v2AgentLlmEnabled=False),
        llm_client=fake,
    )

    response = agent.run()

    assert response.analysis_mode == "rule_based"
    assert fake.calls == []
    assert response.recommendations[0]["id"] == "REC-001"


def test_v2_analysis_agent_uses_valid_llm_recommendation(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    fake = _FakeJsonClient([_llm_analysis("000001")])
    agent = ResultAnalysisV2Agent(
        experiments_root=experiments,
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run()

    assert response.analysis_mode == "llm"
    assert response.recommendations[0]["id"] == "REC-LLM-001"
    assert response.modified_policy_json[0]["source_recommendation_id"] == "REC-LLM-001"
    assert fake.calls[0]["response_name"] == "analysis_recommendations_v2"


def test_v2_analysis_agent_falls_back_when_llm_evidence_is_invalid(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    fake = _FakeJsonClient([_llm_analysis("999999")])
    agent = ResultAnalysisV2Agent(
        experiments_root=experiments,
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run()

    assert response.analysis_mode == "fallback"
    assert response.recommendations[0]["id"] == "REC-001"
    assert any("LLM recommendation failed; rule-based recommendation fallback was used." in warning for warning in response.warnings)


def test_v2_analysis_agent_falls_back_when_llm_json_fails(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    fake = _FakeJsonClient([ValueError("invalid json")])
    agent = ResultAnalysisV2Agent(
        experiments_root=experiments,
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.run()

    assert response.analysis_mode == "fallback"
    assert response.recommendations[0]["id"] == "REC-001"
    assert response.modified_policy_json[0]["source_recommendation_id"] == "REC-001"
    assert any("LLM recommendation failed; rule-based recommendation fallback was used." in warning for warning in response.warnings)


def test_v2_analysis_run_openapi_exposes_empty_schema() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    assert "/api/v2/analysis/run" in schema["paths"]
    assert "/api/v1/analysis/run" in schema["paths"]
