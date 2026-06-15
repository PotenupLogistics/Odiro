from __future__ import annotations

from typing import Any

from app.agents.scenario_generation_v2.template_planner import TemplatePlan


class TemplateJsonWriter:
    """Builds current scenario_template v1 JSON objects without file or run ownership."""

    def write(self, plan: TemplatePlan) -> dict[str, Any]:
        """Return a deterministic template object for the selected alpha pattern."""
        placements = []
        if plan.include_obstacle:
            placements.append(
                {
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
            )

        encounters = []
        if plan.pattern == "static_obstacle_ahead":
            background_count = {"min": 0, "max": 0}
        else:
            background_count = {"min": 0, "max": 1}
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

        return {
            "schema": "scenario_template",
            "version": 1,
            "template_id": plan.template_id,
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
            "robot": {"start": {"type": "entry"}, "goal": {"type": "exit"}},
        }
