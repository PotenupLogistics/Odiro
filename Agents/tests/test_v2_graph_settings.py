from __future__ import annotations

import json

from fastapi.testclient import TestClient

from app.agents.result_analysis_v2.graph_runner import ResultAnalysisGraphRunnerV2, StateGraph as AnalysisStateGraph
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2, StateGraph as ScenarioStateGraph
from app.main import app


def test_v2_graph_runner_imports_without_langgraph_dependency() -> None:
    assert ScenarioGenerationGraphRunnerV2
    assert ResultAnalysisGraphRunnerV2
    assert ScenarioStateGraph is None or ScenarioStateGraph.__name__ == "StateGraph"
    assert AnalysisStateGraph is None or AnalysisStateGraph.__name__ == "StateGraph"


def test_v2_analysis_api_keeps_response_schema_without_internal_state(tmp_path) -> None:
    project = tmp_path / "Project1"
    episode_dir = project / "runs" / "000001" / "episodes" / "000001"
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": True, "goal_reached": True}),
        encoding="utf-8",
    )
    (project / "runs" / "000001" / "summary.json").write_text(
        json.dumps(
            {
                "schema": "run_summary",
                "version": 1,
                "run": {"run_id": "000001", "project_id": project.name},
                "rows": [
                    {
                        "episode_id": "000001",
                        "outcome": "Success",
                        "terminal_reason": "GoalReached",
                        "duration_s": 10.0,
                        "metrics": {"goal_reached": 1},
                    }
                ],
            }
        ),
        encoding="utf-8",
    )

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["analysis_scope"]["episodes_count"] == 1
    assert "episode_timelines" not in payload
