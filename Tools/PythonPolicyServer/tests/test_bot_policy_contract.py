from __future__ import annotations

import asyncio
import pathlib
import sys
import unittest


SERVER_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(SERVER_ROOT) not in sys.path:
    sys.path.insert(0, str(SERVER_ROOT))

from deliverybot_policy.bot_policy_contract import DecisionRequest, STATUS_OK
from deliverybot_policy.bot_policy_runtime import (
    RuntimeBotPolicy,
    configFromLegacyPayload,
    contextFromLegacyGrid,
    contextFromLegacyObservation,
    episodeSetupFromLegacyPayload,
)


def make_grid(width: int, height: int) -> dict:
    cells = []
    for y in range(height):
        for x in range(width):
            cells.append(
                {
                    "x": x,
                    "y": y,
                    "worldX": (x + 0.5) * 100.0,
                    "worldY": (y + 0.5) * 100.0,
                    "worldZ": 0.0,
                    "areaType": "Walkable",
                    "cost": 1.0,
                    "blocked": False,
                    "sourceCollisionProfile": "Walkable",
                    "slopeDegree": 0.0,
                }
            )

    return {
        "gridSizeX": width,
        "gridSizeY": height,
        "cellSizeCm": 100.0,
        "cellCount": len(cells),
        "originCm": {"x": 0.0, "y": 0.0, "z": 0.0},
        "cells": cells,
    }


class BotPolicyContractTests(unittest.TestCase):
    def test_runtime_bot_policy_decides_with_legacy_payload_adapters(self) -> None:
        policy = RuntimeBotPolicy(plannerMode="astar", dwaMode="off")
        config = configFromLegacyPayload(
            {
                "driveSpec": {"maxSpeedKmh": 8.0, "maxReverseSpeedKmh": 3.0},
                "lidarSpec": {"stopDistanceM": 1.0, "slowDownDistanceM": 5.0, "frontHalfAngleDegree": 30.0},
                "motionControlSpec": {
                    "targetSpeedKmh": 3.0,
                    "lookAheadDistanceM": 1.0,
                    "goalAcceptanceDistanceM": 0.5,
                    "steeringSensitivity": 0.8,
                },
            }
        )
        config_result = asyncio.run(policy.setConfig(config))
        self.assertEqual(config_result.status, STATUS_OK)
        self.assertEqual(config_result.configVersion, 1)

        setup = episodeSetupFromLegacyPayload(
            {
                "episodeId": "contract-test",
                "robotInstanceId": "DeliveryBot_1",
                "start": {"x": 150.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0},
                "goal": {"hasGoal": True, "x": 950.0, "y": 250.0, "z": 0.0},
            }
        )
        initialize_result = policy.initialize(setup)
        self.assertEqual(initialize_result.status, STATUS_OK)
        self.assertEqual(initialize_result.episodeVersion, 1)

        grid_result = asyncio.run(policy.setContext(contextFromLegacyGrid(make_grid(12, 6))))
        self.assertEqual(grid_result.status, STATUS_OK)
        self.assertEqual(grid_result.contextVersion, 1)
        self.assertEqual(grid_result.gridVersion, 1)

        observation_context = contextFromLegacyObservation(
            {
                "sequence": 7,
                "sensorSequence": 7,
                "worldTimeSeconds": 1.25,
                "robotState": {"x": 150.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0, "speedKmh": 0.0},
                "lidarRays": [],
                "observedObjects": [],
            }
        )
        observation_result = asyncio.run(policy.setContext(observation_context))
        self.assertEqual(observation_result.status, STATUS_OK)
        self.assertEqual(observation_result.contextVersion, 2)
        self.assertEqual(observation_result.gridVersion, 1)

        decision = policy.decide(DecisionRequest(sequence=7, worldTimeSeconds=1.25))

        self.assertEqual(decision.status, STATUS_OK)
        self.assertEqual(decision.sequence, 7)
        self.assertEqual(decision.episodeVersion, 1)
        self.assertEqual(decision.configVersion, 1)
        self.assertEqual(decision.contextVersion, 2)
        self.assertEqual(decision.gridVersion, 1)
        self.assertIsNotNone(decision.action)
        self.assertEqual(decision.action.direction, "Forward")
        self.assertGreater(decision.action.targetSpeedKmh, 0.0)
        self.assertEqual(decision.debug.reason, "path_follow_action_selected")


if __name__ == "__main__":
    unittest.main()

