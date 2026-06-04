from __future__ import annotations

from pathlib import Path

from app.utils.handoff_response_summary import summarize_handoff_response


ROOT = Path(__file__).resolve().parents[1]


def _response() -> dict:
    return {
        "success": True,
        "handoffTarget": "ue5",
        "worldConfig": {
            "map": {"sidewalkWidthCm": 120},
            "robot": {
                "spawn": {"x": 0, "y": 0, "z": 0},
                "goal": {"x": 800, "y": 0, "z": 0},
            },
            "obstacles": [
                {
                    "type": "Obstacle",
                    "position": {"x": 400, "y": 0, "z": 0},
                    "blockingRatio": 0.6,
                }
            ],
            "pedestrians": [],
        },
        "episodeSpec": {
            "schema": "episode_actor_spawn_mvp",
            "units": {"distance": "m", "angle": "deg"},
            "run": {"time_limit_s": 60.0},
            "ground_model": {
                "regions": [
                    {
                        "region_id": "sidewalk_main",
                        "region_type": "walkable",
                        "shape": {"type": "rectangle", "center_m": [0.0, 0.0, 0.0], "size_m": [10.0, 1.2]},
                        "traversability_score": 1.0,
                    }
                ]
            },
            "paths": [],
            "actors": {
                "robot": {
                    "instance_id": "robot_01",
                    "transform": {"location_m": [0.0, 0.0, 0.0]},
                    "route": {"goal_m": [8.0, 0.0, 0.0]},
                },
                "pedestrians": [],
                "static_obstacles": [
                    {
                        "prop_id": "obstacle.box_01",
                        "transform": {"location_m": [4.0, 0.0, 0.0]},
                        "properties": {
                            "semantic_type": "Obstacle",
                            "blocking_ratio": 0.6,
                        }
                    }
                ],
            },
        },
        "episodeValidation": {"valid": True},
        "episodeScenarioReflection": {
            "passed": True,
            "staticObstacleCount": 1,
            "pedestrianCount": 0,
            "pathCount": 0,
            "ueCompilerReadiness": True,
        },
        "metadata": {"provider": "openai", "model": "gpt-test"},
        "validation": {
            "schemaValidationPassed": True,
            "scenarioReflectionPassed": True,
            "contractValidationPassed": True,
        },
        "scenarioReflection": {
            "passed": True,
            "checkedRequirements": [
                {"requirementId": "obstacle_generic"},
                {"requirementId": "obstacle_route_midpoint"},
            ],
        },
        "postProcessing": {
            "applied": True,
            "patches": [{"patchType": "add_generic_obstacle"}],
        },
        "diagnostics": {
            "effectiveResponseFormat": "both",
            "generationTrace": {
                "traceId": "TRACE-001",
                "requestId": "GEN-001",
                "createdAt": "2026-06-02T00:00:00+00:00",
                "summary": "summary evidence only",
                "warnings": [],
                "evidenceItems": [
                    {
                        "sourceType": "environment_sampling",
                        "fieldPath": "map.sidewalkWidthCm",
                        "valueSummary": 120,
                        "evidence": "environmentSampling.parameters.sidewalkWidthCm",
                        "rule": "fixed_parameter_override",
                        "reason": "fixedParameters.sidewalkWidthCm was provided",
                        "priority": 10,
                        "inputs": {"seed": 1001},
                        "calculation": None,
                    },
                    {
                        "sourceType": "placement_rule",
                        "fieldPath": "obstacles[0].position",
                        "valueSummary": "x=400, y=0, z=0",
                        "evidence": "route midpoint language",
                        "rule": "route_midpoint_with_lateral_offset",
                        "reason": "Obstacle position is derived from robot.spawn and robot.goal.",
                        "priority": 10,
                        "inputs": {},
                        "calculation": "midpoint(robot.spawn, robot.goal)",
                    },
                    {
                        "sourceType": "policy_rag",
                        "fieldPath": "relatedPolicyContext",
                        "valueSummary": "1 chunks",
                        "evidence": "perception_requirement",
                        "rule": None,
                        "reason": "Policy RAG used for safety context, not coordinate generation.",
                        "priority": 5,
                        "inputs": {},
                        "calculation": None,
                    },
                ],
            },
            "environmentSampling": {
                "enabled": True,
                "seed": 1001,
                "scenarioType": "obstacle_ahead",
                "parameters": {
                    "sidewalkWidthCm": 120,
                    "pedestrianCount": 1,
                    "obstacleBlockingRatio": 0.6,
                    "timeLimitSec": 60,
                },
            },
            "attempts": [
                {
                    "scenarioPostProcessingPatches": [
                        {"patchType": "set_obstacle_blocking_ratio_from_prompt"}
                    ]
                }
            ],
        },
        "warnings": [{"message": "test warning"}],
    }


def test_summary_extracts_world_config_and_episode_spec_values() -> None:
    summary = summarize_handoff_response(_response(), http_status=200)

    assert summary["httpStatus"] == 200
    assert summary["success"] is True
    assert summary["providerUsed"] == "openai"
    assert summary["effectiveResponseFormat"] == "both"
    assert summary["worldConfigExists"] is True
    assert summary["episodeSpecExists"] is True
    assert summary["sidewalkWidthCm"] == 120
    assert summary["obstacleExists"] is True
    assert summary["obstacleType"] == "Obstacle"
    assert summary["obstaclePosition"] == {"x": 400, "y": 0, "z": 0}
    assert summary["blockingRatio"] == 0.6
    assert summary["staticObstacleCount"] == 1
    assert summary["staticObstaclePropId"] == "obstacle.box_01"
    assert summary["staticObstacleSemanticType"] == "Obstacle"
    assert summary["staticObstacleBlockingRatio"] == 0.6
    assert summary["obstacleLocation"] == [4.0, 0.0, 0.0]
    assert summary["sidewalkWidthM"] == 1.2
    assert summary["runTimeLimitS"] == 60.0
    assert summary["ueCompilerReadiness"] is True
    assert summary["penaltiesFieldAbsent"] is True
    assert summary["propertiesAreShallow"] is True
    assert summary["propIdInCatalog"] is True
    assert summary["pedestriansEmpty"] is True
    assert summary["pathsEmpty"] is True
    assert summary["checkedRequirementsCount"] == 2
    assert summary["postProcessingApplied"] is True
    assert summary["appliedPatches"] == [
        "add_generic_obstacle",
        "set_obstacle_blocking_ratio_from_prompt",
    ]
    assert summary["environmentSamplingEnabled"] is True
    assert summary["sampledSidewalkWidthCm"] == 120
    assert summary["sampledPedestrianCount"] == 1
    assert summary["sampledObstacleBlockingRatio"] == 0.6
    assert summary["sampledTimeLimitSec"] == 60
    assert summary["generationTraceExists"] is True
    assert summary["traceItemCount"] == 3
    assert summary["traceSourceTypes"] == [
        "environment_sampling",
        "placement_rule",
        "policy_rag",
    ]
    assert summary["coordinateSource"] == "mixed"
    assert summary["policyRagUsedFor"] == "safety_context"
    assert summary["routeMidpointExpected"] is True
    assert summary["obstacleNearRouteMidpoint"] is True
    assert summary["obstacleDistanceFromMidpoint"] == 0.0


def test_summary_extracts_trace_status_failure_stage_and_trace_error() -> None:
    response = {
        "success": False,
        "diagnostics": {
            "generationTraceError": "trace boom",
            "generationTrace": {
                "summary": {
                    "status": "failed",
                    "failureStage": "episode_spec_adapter",
                    "errorSummary": "adapter boom",
                    "coordinateSource": "placement_rule",
                    "policyRagUsedFor": "safety_context",
                },
                "evidenceItems": [
                    {"sourceType": "placement_rule"},
                    {"sourceType": "validation"},
                ],
            },
        },
    }

    summary = summarize_handoff_response(response)

    assert summary["generationTraceExists"] is True
    assert summary["traceStatus"] == "failed"
    assert summary["traceFailureStage"] == "episode_spec_adapter"
    assert summary["generationTraceError"] == "trace boom"
    assert summary["coordinateSource"] == "placement_rule"
    assert summary["policyRagUsedFor"] == "safety_context"


def test_summary_flags_missing_trace_failure_stage_on_failed_response() -> None:
    response = {
        "success": False,
        "episodeSpec": None,
        "error": {"code": "provider_chain_failed"},
        "diagnostics": {
            "generationTrace": {
                "summary": "legacy summary",
                "evidenceItems": [{"sourceType": "validation"}],
            },
            "validation": {"status": "failed", "errors": ["provider chain failed"]},
        },
    }

    summary = summarize_handoff_response(response)

    assert summary["traceStatus"] == "unknown"
    assert summary["traceFailureStage"] is None
    assert summary["missingFailureStage"] is True
    assert summary["episodeSpecMissingReason"] == "provider_chain_failed"


def test_summary_flags_guide_and_midpoint_mismatches_without_payloads() -> None:
    response = _response()
    response["episodeSpec"]["ground_model"]["regions"][0]["penalties"] = []
    response["episodeSpec"]["actors"]["static_obstacles"][0]["transform"]["location_m"] = [8.0, 0.0, 0.0]
    response["episodeSpec"]["actors"]["static_obstacles"][0]["properties"]["nested"] = {"bad": True}

    summary = summarize_handoff_response(response)

    assert summary["penaltiesFieldAbsent"] is False
    assert summary["propertiesAreShallow"] is False
    assert summary["obstacleLocation"] == [8.0, 0.0, 0.0]
    assert summary["obstacleNearRouteMidpoint"] is False
    assert summary["obstacleDistanceFromMidpoint"] == 4.0
    assert "episodeSpec" not in summary


def test_summary_handles_null_episode_spec() -> None:
    response = _response()
    response["episodeSpec"] = None
    response["episodeValidation"] = None
    response["episodeScenarioReflection"] = None

    summary = summarize_handoff_response(response)

    assert summary["episodeSpecExists"] is False
    assert summary["episodeValidationPassed"] is None
    assert summary["episodeScenarioReflectionPassed"] is None
    assert summary["staticObstacleCount"] == 0


def test_summary_extracts_setup_pair_fields() -> None:
    response = _response()
    response["episodeSpec"] = None
    response["episodeSetup"] = {
        "ground_model": {"regions": [{"shape": {"size_m": [10.0, 1.2]}}]},
        "actors": {
            "robot": {"xy_m": [0.0, 0.0], "route": {"goal_xy_m": [8.0, 0.0]}},
            "static_obstacles": [{"xy_m": [4.0, 0.0]}],
            "pedestrians": [],
        },
    }
    response["deliveryBotSetup"] = {
        "robot": {
            "drive": {"max_speed_kmh": 10.0},
            "path_follow": {"target_speed_kmh": 10.0},
            "lidar": {"stop_distance_m": 1.2, "slow_down_distance_m": 3.5},
        }
    }
    response["episodeSetupValidation"] = {"valid": True}
    response["deliveryBotSetupValidation"] = {"valid": True}
    response["diagnostics"]["effectiveResponseFormat"] = "setup_pair"
    response["diagnostics"]["setupPairTrace"] = [{"valueSummary": "map.sidewalkWidthCm=120"}]

    summary = summarize_handoff_response(response)

    assert summary["effectiveResponseFormat"] == "setup_pair"
    assert summary["episodeSetupExists"] is True
    assert summary["deliveryBotSetupExists"] is True
    assert summary["episodeSetupValidationPassed"] is True
    assert summary["deliveryBotSetupValidationPassed"] is True
    assert summary["episodeSetupSidewalkWidthM"] == 1.2
    assert summary["episodeSetupStaticObstacleCount"] == 1
    assert summary["deliveryBotStopDistanceM"] == 1.2
    assert summary["deliveryBotSlowDownDistanceM"] == 3.5
    assert summary["setupPairTraceExists"] is True


def test_summary_handles_null_world_config() -> None:
    response = _response()
    response["worldConfig"] = None

    summary = summarize_handoff_response(response)

    assert summary["worldConfigExists"] is False
    assert summary["sidewalkWidthCm"] is None
    assert summary["obstacleExists"] is False
    assert summary["blockingRatio"] is None


def test_summary_does_not_include_full_payloads() -> None:
    summary = summarize_handoff_response(_response())

    assert "worldConfig" not in summary
    assert "episodeSpec" not in summary
    assert summary["fullPayloadStored"] is False
    assert summary["fullEpisodeSpecStored"] is False


def test_forbidden_artifacts_are_not_created() -> None:
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]:
        assert not path.exists(), f"Forbidden artifact exists: {path}"
