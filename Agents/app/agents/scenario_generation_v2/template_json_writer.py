from __future__ import annotations

from typing import Any

from app.agents.scenario_generation_v2.template_planner import TemplatePlan


class TemplateJsonWriter:
    def write(self, plan: TemplatePlan) -> dict[str, Any]:
        return {
            "schema": "scenario_template",
            "version": 2,
            "scenario_id": plan.scenario_id,
            "scenario_type": plan.scenario_type,
            "intent": {
                "summary": plan.summary,
                "risk_factors": plan.risk_factors,
            },
            "units": {"distance": "meter", "angle": "degree"},
            "ground_model": {
                "default_region_type": "walkable",
                "sidewalk": {"width_m": plan.sidewalk_width_m, "length_m": {"min": 8.0, "max": 15.0}},
            },
            "robot": {
                "type": "delivery_robot",
                "start_area": {"x_m": {"min": 0.0, "max": 1.0}, "y_m": {"min": -0.2, "max": 0.2}},
                "goal_area": {"x_m": {"min": 8.0, "max": 14.0}, "y_m": {"min": -0.2, "max": 0.2}},
            },
            "static_obstacles": {
                "count": plan.obstacle_count,
                "object_types": ["box"],
                "placement_area": {
                    "x_m": {"min": 3.0, "max": 7.0},
                    "lateral_offset_m": {"min": -0.4, "max": 0.4},
                },
            },
            "pedestrians": {
                "count": plan.pedestrian_count,
                "path": {
                    "start_x_m": {"min": 4.0, "max": 9.0},
                    "start_y_m": {"min": -1.0, "max": -0.6},
                    "end_y_m": {"min": 0.6, "max": 1.0},
                },
                "speed_mps": plan.pedestrian_speed_mps,
            },
        }
