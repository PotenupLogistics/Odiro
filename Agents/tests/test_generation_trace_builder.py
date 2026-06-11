from __future__ import annotations

import json

from app.models.generation import (
    RetrievedPolicyContext,
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
    WorldConfigGenerationResult,
    WorldConfigValidationSummary,
)
from app.models.scenario import ScenarioPostProcessPatch, ScenarioPostProcessResult
from app.services.generation_trace_builder import build_generation_trace


PROMPT = (
    "보도 폭과 장애물 차단 정도는 environmentSampling 결과를 우선 적용해줘. "
    "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
    "로봇 경로 중앙 근처에 정적 장애물 1개가 경로를 막고 있는 상황을 만들어줘. "
    "보행자는 없는 시나리오로 만들어줘."
)


def _request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0.0",
        requestId="GEN-TRACE-001",
        generationType="world_config",
        targetContractType="world_config",
        prompt=PROMPT,
        policyId="policy_v1_basic_safety",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["Sidewalk"],
            allowedObjectTypes=["Obstacle"],
            fixedPolicyId="policy_v1_basic_safety",
            defaultSeed=1001,
            requireValidation=True,
            environmentSampling={
                "enabled": True,
                "seed": 1001,
                "scenarioType": "obstacle_ahead",
                "fixedParameters": {
                    "sidewalkWidthCm": 120,
                    "obstacleBlockingRatio": 0.6,
                    "timeLimitSec": 60,
                },
            },
        ),
    )


def _world_config() -> dict:
    return {
        "schemaVersion": "1.0",
        "worldId": "world-trace",
        "scenarioId": "scenario-trace",
        "seed": 1001,
        "map": {
            "type": "Sidewalk",
            "lengthCm": 1000,
            "sidewalkWidthCm": 120,
            "surfaceCondition": "dry",
            "slopeDegree": 0,
        },
        "robot": {
            "botId": "robot_01",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 800, "y": 0, "z": 0},
            "policyId": "policy_v1_basic_safety",
        },
        "obstacles": [
            {
                "objectId": "obstacle_001",
                "type": "Obstacle",
                "position": {"x": 400, "y": 0, "z": 0},
                "blockingRatio": 0.6,
            }
        ],
        "pedestrians": [],
        "environmentObjects": [],
        "runtime": {
            "maxDurationSec": 60,
            "captureReplay": False,
            "emitEventLog": True,
        },
    }


def _result() -> WorldConfigGenerationResult:
    return WorldConfigGenerationResult(
        requestId="GEN-TRACE-001",
        generationType="world_config",
        targetContractType="world_config",
        success=True,
        generatedPayload=_world_config(),
        validation=WorldConfigValidationSummary(status="passed"),
        retrievedContexts=[
            RetrievedPolicyContext(
                chunkId="chunk-1",
                cardId="card-1",
                category="perception_requirement",
                evidenceLocation="p.1",
                relatedActions=["Stop"],
                relatedPolicyParams=["safeDistanceCm"],
                shortText="장애물 감지와 정지 정책",
                score=1.0,
            )
        ],
        scenarioPostProcessing=ScenarioPostProcessResult(
            applied=True,
            patchedPayload=_world_config(),
            patches=[
                ScenarioPostProcessPatch(
                    patchId="PATCH-001",
                    patchType="set_obstacle_position_to_route_midpoint",
                    targetPath="obstacles[obstacle_001].position",
                    beforeValue={"x": 800, "y": 0, "z": 0},
                    afterValue={"x": 400, "y": 0, "z": 0},
                    reason="User prompt places the obstacle near the route midpoint.",
                )
            ],
        ),
        environmentSampling={
            "enabled": True,
            "seed": 1001,
            "scenarioType": "obstacle_ahead",
            "parameters": {
                "sidewalkWidthCm": 120,
                "obstacleBlockingRatio": 0.6,
                "timeLimitSec": 60,
            },
        },
    )


def _episode_spec() -> dict:
    return {
        "schema": "episode_actor_spawn_mvp",
        "ground_model": {
            "regions": [
                {"shape": {"size_m": [10.0, 1.2]}}
            ]
        },
        "actors": {
            "static_obstacles": [
                {
                    "properties": {"semantic_type": "Obstacle", "blocking_ratio": 0.6},
                    "transform": {"location_m": [4.0, 0.0, 0.0]},
                }
            ]
        },
    }


def test_generation_trace_records_environment_sampling_and_placement_sources() -> None:
    trace = build_generation_trace(_request(), _result(), episode_spec=_episode_spec())
    items = [item.model_dump(mode="json") for item in trace.evidenceItems]

    assert any(item["sourceType"] == "environment_sampling" and item["fieldPath"] == "map.sidewalkWidthCm" and item["valueSummary"] == 120 for item in items)
    assert any(item["sourceType"] == "environment_sampling" and item["fieldPath"] == "obstacles[].blockingRatio" and item["valueSummary"] == 0.6 for item in items)
    assert any(item["sourceType"] == "placement_rule" and item["rule"] == "route_midpoint_with_lateral_offset" for item in items)
    assert any(item["sourceType"] == "episode_spec_adapter" and item["rule"] == "cm_to_m" for item in items)
    assert any(item["sourceType"] == "policy_rag" and "safety context" in item["reason"] for item in items)
    assert isinstance(trace.summary, dict)
    assert trace.summary["status"] == "success"
    assert trace.summary["failureStage"] is None


def test_generation_trace_does_not_store_full_payload_or_raw_content() -> None:
    trace = build_generation_trace(_request(), _result(), episode_spec=_episode_spec())
    serialized = json.dumps(trace.model_dump(mode="json"), ensure_ascii=False)

    assert "rawContent" not in serialized
    assert '"worldConfig"' not in serialized
    assert '"episodeSpec"' not in serialized
    assert "OPENAI_API_KEY" not in serialized


def test_generation_trace_records_failure_stage_without_full_payload() -> None:
    result = WorldConfigGenerationResult(
        requestId="GEN-TRACE-001",
        generationType="world_config",
        targetContractType="world_config",
        success=False,
        generatedPayload=None,
        validation=WorldConfigValidationSummary(status="failed", errors=["missing map"]),
        warnings=[],
    )

    trace = build_generation_trace(
        _request(),
        result,
        failure_stage="world_config_validation",
        error_summary="missing map",
    )
    items = [item.model_dump(mode="json") for item in trace.evidenceItems]
    serialized = json.dumps(trace.model_dump(mode="json"), ensure_ascii=False)

    assert isinstance(trace.summary, dict)
    assert trace.summary["status"] == "failed"
    assert trace.summary["failureStage"] == "world_config_validation"
    assert trace.summary["errorSummary"] == "missing map"
    assert any(item["sourceType"] == "validation" and item["fieldPath"] == "failureStage" for item in items)
    assert "rawContent" not in serialized
    assert '"worldConfig"' not in serialized
    assert '"episodeSpec"' not in serialized


def test_generation_trace_defaults_failed_world_config_stage_when_result_failed() -> None:
    result = WorldConfigGenerationResult(
        requestId="GEN-TRACE-001",
        generationType="world_config",
        targetContractType="world_config",
        success=False,
        generatedPayload=None,
        validation=WorldConfigValidationSummary(status="failed", errors=["provider chain failed"]),
        warnings=[],
    )

    trace = build_generation_trace(_request(), result)
    items = [item.model_dump(mode="json") for item in trace.evidenceItems]

    assert isinstance(trace.summary, dict)
    assert trace.summary["status"] == "failed"
    assert trace.summary["failureStage"] == "world_config_validation"
    assert trace.summary["errorSummary"] == "provider chain failed"
    assert any(item["fieldPath"] == "failureStage" and item["valueSummary"] == "world_config_validation" for item in items)
