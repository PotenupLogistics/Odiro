from __future__ import annotations

from typing import Any

from app.agents.scenario_generation_v2.template_planner import TemplatePlan


class TemplateJsonWriter:
    """Builds current project scenario v1 JSON objects without file or run ownership."""

    def write(self, plan: TemplatePlan) -> dict[str, Any]:
        """Return a deterministic scenario object for the selected alpha pattern."""
        placements = []
        requested_count = self._requested_obstacle_count(plan)
        if plan.include_obstacle and requested_count == 2:
            placements.extend(self._gate_pair_placements())
        elif plan.include_obstacle:
            placements.extend(self._fixed_obstacle_placements(requested_count or 1, explicit_blocking=plan.explicit_blocking))

        pedestrians = {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}
        robot = {
            "start": {
                "type": "corridor_pose",
                "segment": "approach",
                "along_m": 1.0,
                "offset_m": 0.0,
                "lane": "walkway",
                "heading": "forward",
            },
            "goal": {
                "type": "corridor_pose",
                "segment": "exit",
                "along_m": 16.0,
                "offset_m": 0.0,
                "lane": "walkway",
                "heading": "forward",
            },
        }
        if plan.robot_anchor_only and plan.robot_start_anchor is not None and plan.robot_goal_anchor is not None:
            robot = {"start": plan.robot_start_anchor, "goal": plan.robot_goal_anchor}

        return {
            "schema": "scenario",
            "version": 1,
            "scenario_id": plan.scenario_id,
            "intent": plan.intent,
            "corridor": {
                "axis": {"type": "polyline", "points_m": [[0.0, 0.0], [18.0, 0.0]]},
                "walkway_width_m": {"min": 1.4, "max": 1.8},
                "building_side": [{"surface": "wall", "width_m": 0.3}],
                "curb_side": [{"surface": "road", "width_m": 4.0}],
                "segments": [
                    {"id": "approach", "type": "straight", "along_range_m": [0.0, 5.0]},
                    {"id": "conflict", "type": "narrowing", "along_range_m": [5.0, 11.0]},
                    {"id": "exit", "type": "straight", "along_range_m": [11.0, 18.0]},
                ],
            },
            "obstacles": {"min_clear_width_m": 0.9, "placements": placements},
            "pedestrians": pedestrians,
            "robot": robot,
        }

    def _requested_obstacle_count(self, plan: TemplatePlan) -> int | None:
        """Return the explicit obstacle count carried by the parsed user request."""
        if plan.requested_obstacle_count is not None:
            return max(0, int(plan.requested_obstacle_count))
        if plan.requested_gate_obstacle_count is not None:
            return max(0, int(plan.requested_gate_obstacle_count))
        return None

    def _fixed_obstacle_placements(self, count: int, *, explicit_blocking: bool) -> list[dict[str, Any]]:
        """Return catalog-safe fixed obstacle placements inside the conflict segment."""
        placements = []
        offsets = [
            {"min": 0.45, "max": 0.75},
            {"min": -0.35, "max": -0.05},
            {"min": 0.05, "max": 0.35},
        ]
        for index in range(count):
            placement: dict[str, Any] = {
                "kind": "fixed",
                "id": "center_obstacle" if index == 0 else f"center_obstacle_{index + 1}",
                "prop": "obstacle.road_cone_01",
                "at": {
                    "segment": "conflict",
                    "along_m": {"min": 6.5 + index * 0.2, "max": 8.5 + index * 0.2},
                    "offset_m": offsets[index % len(offsets)],
                    "lane": "walkway",
                },
                "yaw_deg": 0,
            }
            if explicit_blocking:
                placement["allow_blocking"] = True
            placements.append(placement)
        return placements

    def _gate_pair_placements(self) -> list[dict[str, Any]]:
        """Return exactly one left/right gate pair for prompts that request two obstacles."""
        return [
            {
                "kind": "fixed",
                "id": "gate_panel_left",
                "prop": "obstacle.road_cone_01",
                "at": {
                    "segment": "conflict",
                    "along_m": {"min": 6.8, "max": 7.2},
                    "offset_m": {"min": -0.35, "max": -0.25},
                    "lane": "walkway",
                },
                "yaw_deg": 0,
                "allow_blocking": False,
            },
            {
                "kind": "fixed",
                "id": "gate_panel_right",
                "prop": "obstacle.road_cone_01",
                "at": {
                    "segment": "conflict",
                    "along_m": {"min": 6.8, "max": 7.2},
                    "offset_m": {"min": 0.25, "max": 0.35},
                    "lane": "walkway",
                },
                "yaw_deg": 0,
                "allow_blocking": False,
            },
        ]
