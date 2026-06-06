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


def test_scenario_generation_service_passes_environment_sampling_to_queue_generator(monkeypatch) -> None:
    observed_payloads = []

    def stub_generate_world_config(*args, **kwargs):
        request = args[0]
        return WorldConfigGenerationResult(
            requestId="GEN-SCENARIO-GENERATE-001",
            generationType="world_config",
            targetContractType="world_config",
            success=True,
            generatedPayload={**_world_config(), "scenarioId": "llm_custom_scenario"},
            validation=WorldConfigValidationSummary(status="passed"),
            attempts=[],
            retrievedContexts=[],
            assumptions=[],
            warnings=[],
            environmentSampling={
                "enabled": True,
                "seed": request.constraints.environmentSampling["seed"],
                "scenarioType": request.constraints.environmentSampling["scenarioType"],
                "parameters": {},
                "fixedParameters": request.constraints.environmentSampling["fixedParameters"],
                "warnings": [],
            },
        )

    def stub_generate_setup_pair_queue(payload, **kwargs):
        observed_payloads.append(payload)
        from app.models.run_queue import EpisodeRunQueue

        return type(
            "QueueResult",
            (),
            {
                "run_queue": EpisodeRunQueue(),
            },
        )()

    monkeypatch.setattr(scenario_generation_service, "generate_world_config", stub_generate_world_config)
    monkeypatch.setattr(scenario_generation_service, "generate_setup_pair_queue", stub_generate_setup_pair_queue)
    monkeypatch.setattr(scenario_generation_service, "export_run_queue_package", lambda queue: None)

    scenario_generation_service.generate_scenario_run_queue(
        ScenarioGenerateRequest(prompt="보행자가 횡단하는 상황", episode_count=3)
    )

    assert observed_payloads[0]["scenarioId"] == "llm_custom_scenario"
    assert observed_payloads[0]["environmentSampling"]["scenarioType"] == "pedestrian_crossing"
    assert observed_payloads[0]["environmentSampling"]["enabled"] is True
