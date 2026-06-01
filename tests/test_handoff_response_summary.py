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
    assert summary["staticObstacleSemanticType"] == "Obstacle"
    assert summary["staticObstacleBlockingRatio"] == 0.6
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
    assert summary["routeMidpointExpected"] is True
    assert summary["obstacleNearRouteMidpoint"] is True
    assert summary["obstacleDistanceFromMidpoint"] == 0.0


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
