from __future__ import annotations

from typing import Any


def _range_schema() -> dict[str, Any]:
    """Return the shared numeric min/max range schema used by scenario_template v1."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["min", "max"],
        "properties": {
            "min": {"type": "number"},
            "max": {"type": "number"},
        },
    }


def scenario_template_v1_json_schema() -> dict[str, Any]:
    """Return a structured-output schema for current scenario_template v1 LLM calls."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": [
            "schema",
            "version",
            "template_id",
            "intent",
            "corridor",
            "obstacles",
            "pedestrians",
            "robot",
        ],
        "properties": {
            "schema": {"type": "string", "const": "scenario_template"},
            "version": {"type": "integer", "const": 1},
            "template_id": {"type": "string"},
            "intent": {"type": "string"},
            "corridor": {
                "type": "object",
                "additionalProperties": False,
                "required": ["axis", "walkway_width_m", "building_side", "curb_side", "segments"],
                "properties": {
                    "axis": {
                        "type": "object",
                        "additionalProperties": False,
                        "required": ["type", "points_m"],
                        "properties": {
                            "type": {"type": "string", "const": "polyline"},
                            "points_m": {
                                "type": "array",
                                "minItems": 2,
                                "items": {
                                    "type": "array",
                                    "minItems": 2,
                                    "maxItems": 2,
                                    "items": {"type": "number"},
                                },
                            },
                        },
                    },
                    "walkway_width_m": _range_schema(),
                    "building_side": {
                        "type": "array",
                        "items": _surface_lane_schema(),
                    },
                    "curb_side": {
                        "type": "array",
                        "items": _surface_lane_schema(),
                    },
                    "segments": {
                        "type": "array",
                        "minItems": 1,
                        "items": {
                            "type": "object",
                            "additionalProperties": False,
                            "required": ["id", "type", "along_range_m"],
                            "properties": {
                                "id": {"type": "string"},
                                "type": {"type": "string", "enum": ["straight", "narrowing", "crosswalk", "entrance"]},
                                "along_range_m": {
                                    "type": "array",
                                    "minItems": 2,
                                    "maxItems": 2,
                                    "items": {"type": "number"},
                                },
                            },
                        },
                    },
                },
            },
            "obstacles": {
                "type": "object",
                "additionalProperties": False,
                "required": ["min_clear_width_m", "placements"],
                "properties": {
                    "min_clear_width_m": {"type": "number"},
                    "placements": {
                        "type": "array",
                        "items": {
                            "type": "object",
                            "additionalProperties": False,
                            "required": ["kind", "id", "prop", "at", "yaw_deg"],
                            "properties": {
                                "kind": {"type": "string", "enum": ["fixed", "pattern", "scatter"]},
                                "id": {"type": "string"},
                                "prop": {"type": "string", "enum": ["traffic_cone_01"]},
                                "at": {
                                    "type": "object",
                                    "additionalProperties": False,
                                    "required": ["segment", "along_m", "offset_m", "lane"],
                                    "properties": {
                                        "segment": {"type": "string"},
                                        "along_m": _range_schema(),
                                        "offset_m": _range_schema(),
                                        "lane": {"type": "string"},
                                    },
                                },
                                "yaw_deg": {"type": "number"},
                            },
                        },
                    },
                },
            },
            "pedestrians": {
                "type": "object",
                "additionalProperties": False,
                "required": ["background", "encounters"],
                "properties": {
                    "background": {
                        "type": "object",
                        "additionalProperties": False,
                        "required": ["count", "speed_mps"],
                        "properties": {
                            "count": _range_schema(),
                            "speed_mps": _range_schema(),
                        },
                    },
                    "encounters": {
                        "type": "array",
                        "items": {
                            "type": "object",
                            "additionalProperties": False,
                            "required": ["id", "type", "at", "meet_offset_m", "persona", "overrides"],
                            "properties": {
                                "id": {"type": "string"},
                                "type": {
                                    "type": "string",
                                    "enum": ["oncoming_pass", "overtake", "cross_path", "standing_group"],
                                },
                                "at": {"type": "string"},
                                "meet_offset_m": {"type": "number"},
                                "persona": {"type": "string", "enum": ["passive", "normal", "assertive", "vulnerable"]},
                                "overrides": {
                                    "type": "object",
                                    "additionalProperties": False,
                                    "required": ["cooperation"],
                                    "properties": {
                                        "cooperation": _range_schema(),
                                    },
                                },
                            },
                        },
                    },
                },
            },
            "robot": {
                "type": "object",
                "additionalProperties": False,
                "required": ["start", "goal"],
                "properties": {
                    "start": _robot_anchor_schema(),
                    "goal": _robot_anchor_schema(),
                },
            },
        },
    }


def scenario_template_v1_response_schema() -> dict[str, Any]:
    """Return the response schema envelope expected by the common LLM JSON client."""
    return {
        "name": "scenario_template_v1",
        "schema": scenario_template_v1_json_schema(),
        "strict": True,
    }


def _surface_lane_schema() -> dict[str, Any]:
    """Return the corridor side surface schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["surface", "width_m"],
        "properties": {
            "surface": {
                "type": "string",
                "enum": ["sidewalk", "crosswalk_stripe", "grass", "road", "driveway", "wall", "building"],
            },
            "width_m": {"type": "number"},
        },
    }


def _robot_anchor_schema() -> dict[str, Any]:
    """Return the robot start/goal anchor schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["type"],
        "properties": {
            "type": {"type": "string", "enum": ["entry", "exit"]},
        },
    }
