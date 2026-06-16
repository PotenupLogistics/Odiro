from __future__ import annotations

import json

from fastapi.testclient import TestClient

from app.agents.result_analysis_v2.graph_runner import ResultAnalysisGraphRunnerV2, StateGraph as AnalysisStateGraph
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2, StateGraph as ScenarioStateGraph
from app.core.settings import Settings
from app.main import app


def test_v2_agent_graph_enabled_defaults_false() -> None:
    assert Settings(_env_file=None).v2AgentGraphEnabled is False


def test_v2_graph_runner_imports_without_langgraph_dependency() -> None:
    assert ScenarioGenerationGraphRunnerV2
    assert ResultAnalysisGraphRunnerV2
    assert ScenarioStateGraph is None or ScenarioStateGraph.__name__ == "StateGraph"
    assert AnalysisStateGraph is None or AnalysisStateGraph.__name__ == "StateGraph"


def test_graph_disabled_keeps_existing_v2_api_behavior(monkeypatch, tmp_path) -> None:
    experiments = tmp_path / "experiments"
    episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": True, "goal_reached": True}),
        encoding="utf-8",
    )
    monkeypatch.setenv("V2_AGENT_GRAPH_ENABLED", "false")
    monkeypatch.setenv("ODIROSIM_EXPERIMENTS_DIR", str(experiments))

    response = TestClient(app).post("/api/v2/analysis/run", json={})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["analysis_scope"]["episodes_count"] == 1
    assert "episode_timelines" not in payload
