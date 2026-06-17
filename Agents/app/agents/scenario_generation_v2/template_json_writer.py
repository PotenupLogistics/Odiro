from __future__ import annotations

from typing import Any

from app.agents.scenario_generation_v2.template_planner import TemplatePlan


class TemplateJsonWriter:
    """Builds current project scenario v1 JSON objects without file or run ownership."""

    def write(self, plan: TemplatePlan) -> dict[str, Any]:
        """Return a deterministic scenario object for the selected alpha pattern."""
        placements = []
        if plan.include_obstacle and plan.requested_gate_obstacle_count == 2:
            placements.extend(self._gate_pair_placements())
        elif plan.include_obstacle:
            placement = {
                "kind": "fixed",
                "id": "center_obstacle",
                "prop": "traffic_cone_01",
                "at": {
                    "segment": "conflict",
                    "along_m": {"min": 6.5, "max": 8.5},
                    "offset_m": {"min": -0.2, "max": 0.2},
                    "lane": "center",
                },
                "yaw_deg": 0,
            }
            if plan.explicit_blocking:
                placement["allow_blocking"] = True
            placements.append(placement)

        encounters = []
        if plan.pattern in {"static_obstacle_ahead", "corridor_pose_navigation"}:
            background_count = {"min": 0, "max": 0}
        else:
            background_count = {"min": 0, "max": 0} if plan.single_pedestrian else {"min": 0, "max": 1}
            encounter = {
                "id": "main_conflict",
                "type": plan.encounter_type,
                "at": "conflict",
                "persona": plan.persona,
                "overrides": {
                    "personal_space_m": {"min": 0.6, "max": 0.9},
                    "awareness_horizon_s": {"min": 1.5, "max": 2.5},
                },
            }
            if plan.encounter_type == "cross_path":
                encounter.update(
                    {
                        "id": "main_crossing",
                        "trigger_distance_m": {"min": 3.0, "max": 5.0},
                        "from": "curb_side",
                    }
                )
            else:
                encounter.update({"meet_offset_m": 0.0})
            encounters.append(encounter)
        robot = {"start": {"type": "entry"}, "goal": {"type": "exit"}}
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
            "pedestrians": {
                "background": {"count": background_count, "speed_mps": plan.pedestrian_speed_mps},
                "encounters": encounters,
            },
            "robot": robot,
        }

    def _gate_pair_placements(self) -> list[dict[str, Any]]:
        """Return exactly one left/right gate pair for prompts that request two obstacles."""
        return [
            {
                "kind": "fixed",
                "id": "gate_panel_left",
                "prop": "traffic_cone_01",
                "at": {
                    "segment": "conflict",
                    "along_m": {"min": 6.8, "max": 7.2},
                    "offset_m": {"min": -0.35, "max": -0.25},
                    "lane": "center",
                },
                "yaw_deg": 0,
                "allow_blocking": False,
            },
            {
                "kind": "fixed",
                "id": "gate_panel_right",
                "prop": "traffic_cone_01",
                "at": {
                    "segment": "conflict",
                    "along_m": {"min": 6.8, "max": 7.2},
                    "offset_m": {"min": 0.25, "max": 0.35},
                    "lane": "center",
                },
                "yaw_deg": 0,
                "allow_blocking": False,
            },
        ]
