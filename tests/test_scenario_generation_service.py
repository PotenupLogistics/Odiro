from __future__ import annotations

from app.models.generation import WorldConfigGenerationResult, WorldConfigValidationSummary
from app.models.scenario_generation import ScenarioGenerateRequest
from app.services import scenario_generation_service


def _world_config() -> dict:
    return {
        "scenarioId": "obstacle_ahead",
        "seed": 1001,
        "map": {"type": "Sidewalk", "lengthCm": 800, "sidewalkWidthCm": 120},
        "robot": {"botId": "robot_01", "spawn": {"x": 0, "y": 0}, "goal": {"x": 800, "y": 0}},
        "obstacles": [{"objectId": "obstacle_01", "type": "Obstacle", "position": {"x": 400, "y": 0}, "blockingRatio": 0.6}],
        "pedestrians": [],
        "runtime": {"maxDurationSec": 60},
    }


def test_scenario_generation_service_uses_one_generation_result_and_returns_run_queue(monkeypatch, tmp_path) -> None:
    calls = []

    def stub_generate_world_config(*args, **kwargs):
        calls.append((args, kwargs))
        return WorldConfigGenerationResult(
            requestId="GEN-SCENARIO-GENERATE-001",
            generationType="world_config",
            targetContractType="world_config",
            success=True,
            generatedPayload=_world_config(),
            validation=WorldConfigValidationSummary(status="passed"),
            attempts=[],
            retrievedContexts=[],
            assumptions=[],
            warnings=[],
        )

    monkeypatch.setattr(scenario_generation_service, "generate_world_config", stub_generate_world_config)
    monkeypatch.setattr(
        scenario_generation_service,
        "export_run_queue_package",
        lambda queue: None,
    )

    queue = scenario_generation_service.generate_scenario_run_queue(
        ScenarioGenerateRequest(prompt="장애물이 경로를 막는 상황")
    )

    assert len(calls) == 1
    assert len(queue.runs) == 5
    assert set(queue.model_dump(mode="json", by_alias=True)) == {"schema", "version", "runs"}


def test_scenario_generation_service_passes_episode_count_to_queue_generator(monkeypatch) -> None:
    def stub_generate_world_config(*args, **kwargs):
        return WorldConfigGenerationResult(
            requestId="GEN-SCENARIO-GENERATE-001",
            generationType="world_config",
            targetContractType="world_config",
            success=True,
            generatedPayload=_world_config(),
            validation=WorldConfigValidationSummary(status="passed"),
            attempts=[],
            retrievedContexts=[],
            assumptions=[],
            warnings=[],
        )

    monkeypatch.setattr(scenario_generation_service, "generate_world_config", stub_generate_world_config)
    monkeypatch.setattr(scenario_generation_service, "export_run_queue_package", lambda queue: None)

    queue = scenario_generation_service.generate_scenario_run_queue(
        ScenarioGenerateRequest(prompt="장애물이 경로를 막는 상황", episode_count=3)
    )

    assert len(queue.runs) == 3
