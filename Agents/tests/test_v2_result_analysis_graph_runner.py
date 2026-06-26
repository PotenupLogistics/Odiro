from __future__ import annotations

import json

import pytest
from fastapi.testclient import TestClient

from app.agents.common.llm_json_client import AgentLlmJsonClient
from app.agents.result_analysis_v2.graph_runner import ResultAnalysisGraphRunnerV2
from app.core.settings import Settings
from app.main import app


def _write_blocked_episode(experiments, episode_id: str) -> None:
    episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": False, "failure_type": "blocked_region_violation"}),
        encoding="utf-8",
    )
    (episode_dir / "events.jsonl").write_text(
        '{"event_type": "blocked_region_violation", "time_s": 6.4}\n',
        encoding="utf-8",
    )


def _write_project_blocked_episode(project, episode_id: str) -> None:
    episode_dir = project / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": False, "failure_type": "blocked_region_violation"}),
        encoding="utf-8",
    )
    (episode_dir / "events.jsonl").write_text(
        '{"event_type": "blocked_region_violation", "time_s": 6.4}\n',
        encoding="utf-8",
    )


def _write_project_summary(project, summary: dict) -> None:
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True)
    (run_dir / "summary.json").write_text(json.dumps(summary), encoding="utf-8")


@pytest.fixture(autouse=True)
def _disable_endpoint_llm(monkeypatch) -> None:
    """Keep endpoint tests on the deterministic recommendation path."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")


def test_result_analysis_graph_runner_imports_without_langgraph_dependency() -> None:
    assert ResultAnalysisGraphRunnerV2


def test_graph_runner_builds_insufficient_data_response_without_langgraph(tmp_path) -> None:
    runner = ResultAnalysisGraphRunnerV2(experiments_root=tmp_path / "missing", settings=Settings(_env_file=None))

    response = runner.run()

    assert response.schema_ == "analysis_run_response_v2"
    assert response.summary.overall_judgement == "insufficient_data"


def test_graph_runner_generates_recommendations_for_repeated_blocked_region(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))

    response = runner.run()

    assert response.summary.overall_judgement == "change_recommended"
    assert response.patterns[0]["type"] == "blocked_region_violation_repeated"
    assert response.recommendations[0]["target"] == "policy"
    assert response.analysis_mode == "rule_based"


def test_graph_runner_exposes_node_state_without_response_schema_changes(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))

    response = runner.run()
    state = runner.last_state

    assert "episode_timelines" in state
    assert "representative_failed_episodes" in state
    assert "rag_queries" in state
    assert "retrieved_context" in state
    assert "analysis_context" in state
    assert "episode_timelines" not in response.model_dump(by_alias=True)
    assert "rag_queries" not in response.model_dump(by_alias=True)


def test_analysis_api_preserves_response_schema(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "episode_timelines" not in payload


def test_analysis_api_uses_graph_runner_path(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["analysis_mode"] == "rule_based"


def test_analysis_api_falls_back_when_openai_fails_without_ollama_attempt(monkeypatch, tmp_path) -> None:
    """Keep result analysis stable when the first configured LLM provider fails."""
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")
    providers: list[str] = []

    def fail_generate_json(self, **_kwargs) -> dict:
        providers.append(self.provider.value)
        raise ValueError("forced OpenAI failure")

    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "true")
    monkeypatch.setenv("LLM_PROVIDER_CHAIN", "openai,ollama")
    monkeypatch.setattr(AgentLlmJsonClient, "generate_json", fail_generate_json)

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert providers == ["openai"]
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["analysis_mode"] == "fallback"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["recommendations"]
    assert any("rule-based recommendation fallback" in warning for warning in payload["warnings"])


def test_analysis_api_falls_back_when_ollama_provider_fails(monkeypatch, tmp_path) -> None:
    """Treat a selected local Ollama provider failure as a non-fatal LLM failure."""
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")
    providers: list[str] = []

    def fail_generate_json(self, **_kwargs) -> dict:
        providers.append(self.provider.value)
        raise ValueError("forced Ollama failure")

    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "true")
    monkeypatch.setenv("LLM_PROVIDER_CHAIN", "ollama")
    monkeypatch.setattr(AgentLlmJsonClient, "generate_json", fail_generate_json)

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert providers == ["ollama"]
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["analysis_mode"] == "fallback"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["recommendations"]
    assert any("rule-based recommendation fallback" in warning for warning in payload["warnings"])


def test_analysis_api_uses_summary_without_episode_results(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_summary(project, {"episode_count": 2, "success_count": 1, "failure_count": 1})

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 2}
    assert payload["metrics"]["success_count"] == 1
    assert payload["metrics"]["failure_count"] == 1


def test_analysis_api_accepts_prompt_without_schema_changes(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={
            "project_path": str(project),
            "run_id": "000001",
            "prompt": "보도이탈과 페널티 중심으로 다시 분석해줘",
        },
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "사용자 요청 관점" in payload["summary"]["message"]
    assert "route_deviation" in payload["summary"]["message"]
    assert "penalty" in payload["summary"]["message"]
    assert "prompt_focus" not in payload
