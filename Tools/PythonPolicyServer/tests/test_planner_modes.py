from __future__ import annotations

import pathlib
import sys
import unittest


SERVER_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(SERVER_ROOT) not in sys.path:
    sys.path.insert(0, str(SERVER_ROOT))

from deliverybot_policy.hybrid_astar import build_hybrid_astar_options, find_hybrid_astar_path, is_pose_clear
from deliverybot_policy.planning import (
    PlannedPathResult,
    choose_lookahead_target,
    choose_lookahead_target_info,
    find_path_for_policy,
    merge_hybrid_astar_runtime_spec,
)
from deliverybot_policy.policies import dwa_local_avoidance, front_obstacle_stop, normal_path_follow
from deliverybot_policy.reeds_shepp import find_reeds_shepp_path
from deliverybot_policy.registry import apply_dwa_mode_to_policy_spec


def make_grid(width: int, height: int, blocked: set[tuple[int, int]] | None = None) -> tuple[dict, dict]:
    blocked = blocked or set()
    cells = []
    lookup = {}
    for y in range(height):
        for x in range(width):
            is_blocked = (x, y) in blocked
            cell = {
                "x": x,
                "y": y,
                "worldX": (x + 0.5) * 100.0,
                "worldY": (y + 0.5) * 100.0,
                "worldZ": 0.0,
                "areaType": "Blocked" if is_blocked else "Walkable",
                "cost": 1.0e30 if is_blocked else 1.0,
                "blocked": is_blocked,
                "sourceCollisionProfile": "Blocked" if is_blocked else "Walkable",
            }
            cells.append(cell)
            lookup[(x, y)] = cell

    return {
        "gridSizeX": width,
        "gridSizeY": height,
        "cellSizeCm": 100.0,
        "cellCount": len(cells),
        "originCm": {"x": 0.0, "y": 0.0, "z": 0.0},
        "cells": cells,
    }, lookup


def make_context(
    planner_mode: str = "astar",
    policy_entry: dict | None = None,
    blocked: set[tuple[int, int]] | None = None,
    observation: dict | None = None,
    right_of_way_mode: str = "policy",
) -> dict:
    grid_info, lookup = make_grid(12, 6, blocked)
    obs = observation or {
        "sequence": 1,
        "worldTimeSeconds": 0.0,
        "robotState": {"x": 150.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0, "speedKmh": 1.0},
        "lidarRays": [],
        "observedObjects": [],
    }
    return {
        "observation": obs,
        "gridInfo": grid_info,
        "gridCellLookup": lookup,
        "episodeInfo": {
            "hasGoal": True,
            "goal": {"x": 950.0, "y": 250.0, "z": 0.0},
        },
        "configInfo": {
            "driveSpec": {"maxSpeedKmh": 8.0, "maxReverseSpeedKmh": 3.0},
            "lidarSpec": {"stopDistanceM": 1.0, "slowDownDistanceM": 5.0, "frontHalfAngleDegree": 30.0},
            "motionControlSpec": {
                "targetSpeedKmh": 3.0,
                "lookAheadDistanceM": 1.0,
                "goalAcceptanceDistanceM": 0.5,
                "steeringSensitivity": 0.8,
                "obstacleSlowSpeedKmh": 1.0,
            },
        },
        "policyEntry": policy_entry or {"policyId": "normal_path_follow", "priority": 100},
        "policyRuntimeState": {},
        "plannerMode": planner_mode,
        "rightOfWayMode": right_of_way_mode,
    }


def max_segment_distance_cm(poses: list) -> float:
    distances = [
        ((current.x_cm - previous.x_cm) ** 2 + (current.y_cm - previous.y_cm) ** 2) ** 0.5
        for previous, current in zip(poses, poses[1:])
    ]
    return max(distances) if distances else 0.0


class PlannerModeTests(unittest.TestCase):
    def test_reeds_shepp_forward_straight_shot(self) -> None:
        path = find_reeds_shepp_path(
            0.0,
            0.0,
            0.0,
            300.0,
            0.0,
            0.0,
            100.0,
            25.0,
            allow_reverse=True,
        )

        self.assertIsNotNone(path)
        self.assertEqual(path.segments[0].direction, "Forward")
        self.assertAlmostEqual(path.poses[-1].x_cm, 300.0)
        self.assertAlmostEqual(path.poses[-1].y_cm, 0.0)

    def test_reeds_shepp_reverse_straight_shot(self) -> None:
        path = find_reeds_shepp_path(
            300.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            100.0,
            25.0,
            allow_reverse=True,
        )

        self.assertIsNotNone(path)
        self.assertEqual(path.segments[0].direction, "Reverse")
        self.assertAlmostEqual(path.poses[-1].x_cm, 0.0)
        self.assertAlmostEqual(path.poses[-1].y_cm, 0.0)

    def test_astar_planner_routes_around_blocked_cell(self) -> None:
        context = make_context(
            "astar",
            blocked={(4, 2), (5, 2), (6, 2)},
        )

        result = find_path_for_policy(context)

        self.assertEqual(result.planner_id, "astar")
        self.assertEqual(result.status, "ok")
        self.assertGreater(len(result.world_path), 2)
        self.assertFalse(any(index in {(4, 2), (5, 2), (6, 2)} for index in result.grid_path))

    def test_path_result_is_cached_for_same_runtime_state(self) -> None:
        context = make_context("astar", blocked={(4, 2)})

        first = find_path_for_policy(context)
        second = find_path_for_policy(context)

        self.assertFalse(first.path_cache_hit)
        self.assertTrue(second.path_cache_hit)
        self.assertEqual(first.grid_path, second.grid_path)

    def test_hybrid_astar_can_start_with_reverse_segment(self) -> None:
        grid_info, lookup = make_grid(12, 6)

        result = find_hybrid_astar_path(
            grid_info,
            lookup,
            {"x": 850.0, "y": 250.0, "yawDegree": 0.0},
            {"x": 250.0, "y": 250.0, "z": 0.0},
            {
                "stepDistanceCm": 75.0,
                "maxContinuousReverseDistanceCm": 900.0,
                "goalAcceptanceDistanceCm": 75.0,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 4000,
            },
        )

        self.assertEqual(result.status, "ok")
        self.assertTrue(any(pose.direction == "Reverse" for pose in result.poses[1:]))

    def test_hybrid_astar_reeds_shepp_analytic_expansion_connects_goal(self) -> None:
        grid_info, lookup = make_grid(12, 6)

        result = find_hybrid_astar_path(
            grid_info,
            lookup,
            {"x": 150.0, "y": 250.0, "yawDegree": 0.0},
            {"x": 650.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0},
            {
                "analyticExpansionEnabled": True,
                "analyticExpansionMaxDistanceCm": 700.0,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 1,
                "postProcessEnabled": False,
            },
        )

        self.assertEqual(result.status, "ok")
        self.assertAlmostEqual(result.poses[-1].x_cm, 650.0)
        self.assertLessEqual(result.expanded_nodes, 0)

    def test_hybrid_astar_reeds_shepp_analytic_expansion_can_be_disabled(self) -> None:
        grid_info, lookup = make_grid(12, 6)

        result = find_hybrid_astar_path(
            grid_info,
            lookup,
            {"x": 150.0, "y": 250.0, "yawDegree": 0.0},
            {"x": 650.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0},
            {
                "analyticExpansionEnabled": False,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 1,
                "postProcessEnabled": False,
            },
        )

        self.assertEqual(result.status, "max_expanded_nodes_reached")

    def test_hybrid_astar_reverse_primitive_moves_backward(self) -> None:
        grid_info, lookup = make_grid(12, 6, {(9, 2), (10, 2), (11, 2)})

        result = find_hybrid_astar_path(
            grid_info,
            lookup,
            {"x": 850.0, "y": 250.0, "yawDegree": 0.0},
            {"x": 250.0, "y": 250.0, "z": 0.0},
            {
                "maxContinuousReverseDistanceCm": 900.0,
                "goalAcceptanceDistanceCm": 75.0,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 4000,
            },
        )

        self.assertEqual(result.status, "ok")
        self.assertGreater(len(result.poses), 1)
        self.assertEqual(result.poses[1].direction, "Reverse")
        self.assertLess(result.poses[1].x_cm, 850.0)

    def test_hybrid_astar_post_processing_resamples_shortcut_path(self) -> None:
        grid_info, lookup = make_grid(12, 6, {(9, 2), (10, 2), (11, 2)})

        result = find_hybrid_astar_path(
            grid_info,
            lookup,
            {"x": 850.0, "y": 250.0, "yawDegree": 0.0},
            {"x": 250.0, "y": 250.0, "z": 0.0},
            {
                "maxContinuousReverseDistanceCm": 900.0,
                "goalAcceptanceDistanceCm": 75.0,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 4000,
                "postProcessEnabled": True,
                "resampleDistanceCm": 50.0,
            },
        )

        self.assertEqual(result.status, "ok")
        self.assertTrue(result.post_processed)
        self.assertGreater(len(result.poses), 2)
        self.assertLessEqual(max_segment_distance_cm(result.poses), 50.0 + 1.0e-6)
        self.assertFalse(any(index in {(9, 2), (10, 2), (11, 2)} for index in result.grid_path))

    def test_hybrid_astar_post_processing_can_be_disabled(self) -> None:
        grid_info, lookup = make_grid(12, 6, {(9, 2), (10, 2), (11, 2)})

        result = find_hybrid_astar_path(
            grid_info,
            lookup,
            {"x": 850.0, "y": 250.0, "yawDegree": 0.0},
            {"x": 250.0, "y": 250.0, "z": 0.0},
            {
                "maxContinuousReverseDistanceCm": 900.0,
                "goalAcceptanceDistanceCm": 75.0,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 4000,
                "postProcessEnabled": False,
            },
        )

        self.assertEqual(result.status, "ok")
        self.assertFalse(result.post_processed)
        self.assertEqual(result.raw_pose_count, len(result.poses))

    def test_hybrid_astar_footprint_check_rejects_blocked_neighbor_overlap(self) -> None:
        grid_info, lookup = make_grid(5, 3, {(2, 0)})
        options = build_hybrid_astar_options(
            {
                "clearanceRadiusCm": 0.0,
                "footprintCheckEnabled": True,
                "footprintHalfLengthCm": 40.0,
                "footprintHalfWidthCm": 80.0,
                "footprintSampleStepCm": 40.0,
            }
        )

        self.assertFalse(is_pose_clear(grid_info, lookup, 250.0, 150.0, 0.0, options, set()))

    def test_hybrid_astar_footprint_check_is_opt_in(self) -> None:
        grid_info, lookup = make_grid(5, 3, {(2, 0)})
        options = build_hybrid_astar_options(
            {
                "clearanceRadiusCm": 0.0,
                "footprintCheckEnabled": False,
                "footprintHalfLengthCm": 40.0,
                "footprintHalfWidthCm": 80.0,
                "footprintSampleStepCm": 40.0,
            }
        )

        self.assertTrue(is_pose_clear(grid_info, lookup, 250.0, 150.0, 0.0, options, set()))

    def test_hybrid_astar_runtime_vehicle_spec_fills_footprint_when_enabled(self) -> None:
        context = make_context("hybrid-astar")
        context["observation"]["vehicleSpec"] = {
            "minTurningRadiusCm": 420.0,
            "robotBoxExtentCm": {"x": 60.0, "y": 90.0, "z": 25.0},
        }

        spec = merge_hybrid_astar_runtime_spec({"footprintCheckEnabled": True}, context)

        self.assertEqual(spec["minTurningRadiusCm"], 420.0)
        self.assertEqual(spec["footprintHalfLengthCm"], 60.0)
        self.assertEqual(spec["footprintHalfWidthCm"], 90.0)

    def test_normal_path_follow_uses_hybrid_planner_from_context(self) -> None:
        context = make_context("hybrid-astar", blocked={(9, 2), (10, 2), (11, 2)})
        context["observation"]["robotState"] = {
            "x": 850.0,
            "y": 250.0,
            "z": 0.0,
            "yawDegree": 0.0,
            "speedKmh": 0.2,
        }
        context["episodeInfo"]["goal"] = {"x": 250.0, "y": 250.0, "z": 0.0}
        context["policyEntry"] = {
            "policyId": "normal_path_follow",
            "priority": 100,
            "hybridAStar": {
                "maxContinuousReverseDistanceCm": 900.0,
                "goalAcceptanceDistanceCm": 75.0,
                "clearanceRadiusCm": 0.0,
                "maxExpandedNodes": 4000,
            },
        }

        candidate = normal_path_follow.evaluate(context)

        self.assertIsNotNone(candidate)
        self.assertEqual(candidate["debug"]["planner"], "hybrid_astar")
        self.assertEqual(candidate["action"]["direction"], "Reverse")

    def test_lookahead_does_not_cross_hybrid_gear_switch(self) -> None:
        result = PlannedPathResult(
            "hybrid_astar",
            "ok",
            [],
            [
                {"x": 0.0, "y": 0.0, "z": 0.0},
                {"x": -20.0, "y": 0.0, "z": 0.0},
                {"x": 80.0, "y": 0.0, "z": 0.0},
            ],
            [
                {"x": 0.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Forward"},
                {"x": -20.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Reverse"},
                {"x": 80.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Forward"},
            ],
        )

        target, direction = choose_lookahead_target(
            result,
            {"x": 0.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0},
            {"lookAheadDistanceM": 0.5},
        )

        self.assertEqual(direction, "Reverse")
        self.assertEqual(target["x"], -20.0)

    def test_lookahead_starts_from_nearest_current_path_point(self) -> None:
        result = PlannedPathResult(
            "astar",
            "ok",
            [],
            [
                {"x": 0.0, "y": 0.0, "z": 0.0},
                {"x": 100.0, "y": 0.0, "z": 0.0},
                {"x": 200.0, "y": 0.0, "z": 0.0},
                {"x": 300.0, "y": 0.0, "z": 0.0},
            ],
            [
                {"x": 0.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Forward"},
                {"x": 100.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Forward"},
                {"x": 200.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Forward"},
                {"x": 300.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0, "direction": "Forward"},
            ],
        )

        target_info = choose_lookahead_target_info(
            result,
            {"x": 210.0, "y": 0.0, "z": 0.0, "yawDegree": 0.0},
            {"lookAheadDistanceM": 0.5},
        )

        self.assertEqual(target_info.direction, "Forward")
        self.assertEqual(target_info.nearest_index, 2)
        self.assertEqual(target_info.target_index, 3)
        self.assertEqual(target_info.target_world["x"], 300.0)
        self.assertAlmostEqual(target_info.distance_to_path_cm, 10.0)

    def test_dwa_can_follow_reverse_hybrid_path_direction(self) -> None:
        observation = {
            "sequence": 1,
            "worldTimeSeconds": 0.0,
            "robotState": {"x": 850.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0, "speedKmh": 0.2},
            "lidarRays": [{"hit": True, "rayYawDegree": 0.0, "distanceM": 1.0}],
            "observedObjects": [],
        }
        context = make_context(
            "dwa",
            policy_entry={
                "policyId": "dwa_local_avoidance",
                "priority": 25,
                "planner": "dwa",
                "globalPlanner": "hybrid_astar",
                "hybridAStar": {
                    "maxContinuousReverseDistanceCm": 900.0,
                    "goalAcceptanceDistanceCm": 75.0,
                    "clearanceRadiusCm": 0.0,
                    "maxExpandedNodes": 4000,
                },
                "dwa": {
                    "activationDistanceM": 5.0,
                    "safetyDistanceM": 0.4,
                    "maxReverseSpeedKmh": 2.0,
                    "speedSampleCount": 2,
                    "reverseSpeedSampleCount": 2,
                    "steeringSampleCount": 5,
                    "reverseScorePenalty": 0.0,
                },
            },
            blocked={(9, 2), (10, 2), (11, 2)},
            observation=observation,
        )
        context["episodeInfo"]["goal"] = {"x": 250.0, "y": 250.0, "z": 0.0}

        candidate = dwa_local_avoidance.evaluate(context)

        self.assertIsNotNone(candidate)
        self.assertEqual(candidate["debug"]["planner"], "hybrid_astar")
        self.assertEqual(candidate["debug"]["pathDirection"], "Reverse")
        self.assertEqual(candidate["action"]["direction"], "Reverse")
        self.assertTrue(candidate["debug"]["dwaClearanceRecoverySelected"])

    def test_dwa_uses_static_grid_obstacles_when_lidar_is_empty(self) -> None:
        context = make_context(
            "dwa",
            policy_entry={
                "policyId": "dwa_local_avoidance",
                "priority": 25,
                "planner": "dwa",
                "dwa": {
                    "includeGridObstacles": True,
                    "gridObstacleMaxDistanceM": 5.0,
                    "safetyDistanceM": 0.3,
                    "speedSampleCount": 2,
                    "steeringSampleCount": 5,
                },
            },
            blocked={(4, 2)},
        )

        candidate = dwa_local_avoidance.evaluate(context)

        self.assertIsNotNone(candidate)
        self.assertEqual(candidate["debug"]["dwaLidarObstacleCount"], 0)
        self.assertGreater(candidate["debug"]["dwaGridObstacleCount"], 0)
        self.assertNotEqual(candidate["action"]["steering"], 0.0)
        self.assertGreaterEqual(candidate["debug"]["lookAheadPathIndex"], 2)

    def test_normal_path_follow_previews_turn_before_grid_obstacle(self) -> None:
        context = make_context("astar", blocked={(4, 2)})

        candidate = normal_path_follow.evaluate(context)

        self.assertIsNotNone(candidate)
        self.assertLess(candidate["action"]["steering"], 0.0)
        self.assertGreaterEqual(candidate["debug"]["lookAheadPathIndex"], 2)

    def test_dwa_samples_grid_obstacle_perimeter_for_vehicle_clearance(self) -> None:
        points = dwa_local_avoidance.sample_grid_obstacle_points(100.0, 200.0, 50.0)

        self.assertEqual(len(points), 9)
        self.assertIn((100.0, 200.0), points)
        self.assertIn((50.0, 150.0), points)
        self.assertIn((150.0, 250.0), points)

    def test_dwa_required_clearance_includes_vehicle_footprint(self) -> None:
        context = make_context(
            "dwa",
            policy_entry={
                "policyId": "dwa_local_avoidance",
                "priority": 25,
                "planner": "dwa",
                "dwa": {
                    "safetyDistanceM": 0.65,
                    "robotCollisionRadiusM": 0.9,
                    "clearanceMarginM": 0.15,
                    "obstacleInflationM": 0.1,
                },
            },
        )

        config = dwa_local_avoidance.build_dwa_config(context)

        self.assertAlmostEqual(dwa_local_avoidance.get_required_clearance_cm(config), 115.0)

    def test_dwa_reverse_recovery_when_robot_is_stuck_near_obstacle(self) -> None:
        observation = {
            "sequence": 1,
            "worldTimeSeconds": 0.0,
            "robotState": {"x": 150.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0, "speedKmh": 0.0},
            "lidarRays": [{"hit": True, "rayYawDegree": 60.0, "distanceM": 0.8}],
            "observedObjects": [],
        }
        context = make_context(
            "dwa",
            policy_entry={
                "policyId": "dwa_local_avoidance",
                "priority": 25,
                "planner": "dwa",
                "dwa": {
                    "stuckTimeSeconds": 0.8,
                    "stuckRecoveryReverseSpeedKmh": 1.0,
                    "safetyDistanceM": 0.3,
                    "speedSampleCount": 2,
                    "steeringSampleCount": 5,
                },
            },
            observation=observation,
        )

        first = dwa_local_avoidance.evaluate(context)
        context["observation"]["worldTimeSeconds"] = 0.4
        second = dwa_local_avoidance.evaluate(context)
        context["observation"]["worldTimeSeconds"] = 0.9
        third = dwa_local_avoidance.evaluate(context)

        self.assertIsNotNone(first)
        self.assertIsNotNone(second)
        self.assertIsNotNone(third)
        self.assertEqual(third["action"]["direction"], "Reverse")
        self.assertEqual(third["reason"], "dwa_stuck_reverse_recovery")
        self.assertGreater(
            third["debug"]["dwaRecoveryEndClearanceCm"],
            third["debug"]["dwaRecoveryStartClearanceCm"],
        )

    def test_dwa_grid_obstacles_can_be_disabled(self) -> None:
        context = make_context(
            "dwa",
            policy_entry={
                "policyId": "dwa_local_avoidance",
                "priority": 25,
                "planner": "dwa",
                "dwa": {
                    "includeGridObstacles": False,
                    "gridObstacleMaxDistanceM": 5.0,
                },
            },
            blocked={(4, 2)},
        )

        self.assertIsNone(dwa_local_avoidance.evaluate(context))

    def test_dwa_mode_off_removes_dwa_policy(self) -> None:
        policy_spec = {
            "catalogId": "default_delivery",
            "catalogVersion": 2,
            "enabledPolicies": [
                {"policyId": "front_obstacle_stop", "priority": 10},
                {"policyId": "dwa_local_avoidance", "priority": 25},
                {"policyId": "normal_path_follow", "priority": 100},
            ],
        }

        result = apply_dwa_mode_to_policy_spec({"dwaMode": "off"}, policy_spec, {})

        self.assertNotIn(
            "dwa_local_avoidance",
            [policy["policyId"] for policy in result["enabledPolicies"]],
        )

    def test_dwa_mode_on_adds_default_dwa_policy(self) -> None:
        policy_spec = {
            "catalogId": "default_delivery",
            "catalogVersion": 2,
            "enabledPolicies": [
                {"policyId": "normal_path_follow", "priority": 100},
            ],
        }
        catalog = {
            "catalogId": "default_delivery",
            "catalogVersion": 2,
            "policies": [
                {
                    "policyId": "dwa_local_avoidance",
                    "defaultEnabled": True,
                    "defaultPriority": 25,
                    "parameters": {"dwa": {"activationDistanceM": 5.0}},
                }
            ],
        }

        result = apply_dwa_mode_to_policy_spec({"dwaMode": "on"}, policy_spec, catalog)

        dwa_policy = next(
            policy
            for policy in result["enabledPolicies"]
            if policy["policyId"] == "dwa_local_avoidance"
        )
        self.assertEqual(dwa_policy["priority"], 25)
        self.assertEqual(dwa_policy["parameters"]["dwa"]["activationDistanceM"], 5.0)

    def test_robot_priority_obstacle_reroute_does_not_count_success_as_failed_attempt(self) -> None:
        obstacle_observation = {
            "sequence": 1,
            "worldTimeSeconds": 1.0,
            "robotState": {"x": 150.0, "y": 250.0, "z": 0.0, "yawDegree": 0.0, "speedKmh": 1.0},
            "observedObjects": [
                {
                    "actorName": "BP_Box_01",
                    "actorTags": ["ObjectType.box"],
                    "closestDistanceM": 3.0,
                    "closestRayYawDegree": 0.0,
                    "inFront": True,
                }
            ],
            "lidarRays": [
                {
                    "hit": True,
                    "rayYawDegree": 0.0,
                    "distanceM": 3.0,
                    "actorName": "BP_Box_01",
                    "actorTags": ["ObjectType.box"],
                }
            ],
        }
        context = make_context(
            "astar",
            policy_entry={
                "policyId": "front_obstacle_stop",
                "priority": 10,
                "dynamicObstacles": {
                    "enabled": True,
                    "frontOnly": False,
                    "inflationRadiusM": 0.5,
                    "maxDistanceM": 5.0,
                    "persistenceSeconds": 2.0,
                },
            },
            blocked=set(),
            observation=obstacle_observation,
            right_of_way_mode="robot",
        )

        first = front_obstacle_stop.evaluate(context)
        second = front_obstacle_stop.evaluate(context)

        self.assertIsNotNone(first)
        self.assertIsNotNone(second)
        self.assertEqual(first["debug"]["pathStatus"], "ok")
        self.assertEqual(second["debug"]["pathStatus"], "ok")
        self.assertEqual(context["policyRuntimeState"]["front_obstacle_stop"].get("rerouteAttemptCount", 0), 0)
        self.assertGreaterEqual(
            context["policyRuntimeState"]["front_obstacle_stop"].get("rerouteSuccessCount", 0),
            2,
        )


if __name__ == "__main__":
    unittest.main()
