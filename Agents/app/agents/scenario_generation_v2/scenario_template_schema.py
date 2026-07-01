from __future__ import annotations

from typing import Any

from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids


def _range_schema() -> dict[str, Any]:
    """Return the shared numeric min/max range schema used by project scenario v1."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["min", "max"],
        "properties": {
            "min": {"type": "number"},
            "max": {"type": "number"},
        },
    }


def _number_or_range_schema() -> dict[str, Any]:
    """Return a schema accepting either a fixed number or a numeric min/max range."""
    return {
        "anyOf": [
            {"type": "number"},
            _range_schema(),
        ],
    }


def _integer_or_range_schema() -> dict[str, Any]:
    """Return a schema accepting either a fixed integer or an integer min/max range."""
    return {
        "anyOf": [
            {"type": "integer"},
            {
                "type": "object",
                "additionalProperties": False,
                "required": ["min", "max"],
                "properties": {
                    "min": {"type": "integer"},
                    "max": {"type": "integer"},
                },
            },
        ],
    }


def _string_choices_schema() -> dict[str, Any]:
    """Return a schema accepting a fixed string or a choices list."""
    return {
        "anyOf": [
            {"type": "string"},
            {
                "type": "object",
                "additionalProperties": False,
                "required": ["choices"],
                "properties": {
                    "choices": {
                        "type": "array",
                        "items": {"type": "string"},
                    },
                },
            },
        ],
    }


def _nullable(schema: dict[str, Any]) -> dict[str, Any]:
    """Return a nullable union schema for OpenAI strict structured outputs."""
    return {"anyOf": [schema, {"type": "null"}]}


def project_scenario_v1_json_schema() -> dict[str, Any]:
    """Return a structured-output schema for current Project Scenario v1 LLM calls."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": [
            "schema",
            "version",
            "scenario_id",
            "intent",
            "corridor",
            "obstacles",
            "robot",
        ],
        "properties": {
            "schema": {"type": "string", "const": "scenario"},
            "version": {"type": "integer", "const": 1},
            "scenario_id": {"type": "string"},
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
                    "walkway_width_m": _number_or_range_schema(),
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
                            "required": ["id", "type", "along_range_m", "replaced_by"],
                            "properties": {
                                "id": {"type": "string"},
                                "type": {"type": "string", "enum": ["straight"]},
                                "along_range_m": {
                                    "type": "array",
                                    "minItems": 2,
                                    "maxItems": 2,
                                    "items": {"type": "number"},
                                },
                                "replaced_by": _nullable(_string_choices_schema()),
                            },
                        },
                    },
                },
            },
            "obstacles": _nullable({
                "type": "object",
                "additionalProperties": False,
                "required": ["min_clear_width_m", "placements"],
                "properties": {
                    "min_clear_width_m": _number_or_range_schema(),
                    "placements": {
                        "type": "array",
                        "items": {
                            "type": "object",
                            "additionalProperties": False,
                            "required": [
                                "kind",
                                "id",
                                "prop",
                                "at",
                                "yaw_deg",
                                "allow_blocking",
                            ],
                            "properties": {
                                "kind": {"type": "string", "enum": ["fixed"]},
                                "id": {"type": "string"},
                                "prop": {
                                    "type": ["string", "null"],
                                    "enum": [*sorted(get_allowed_static_obstacle_prop_ids()), None],
                                },
                                "at": {
                                    "type": "object",
                                    "additionalProperties": False,
                                    "required": ["segment", "along_m", "offset_m", "lane"],
                                    "properties": {
                                        "segment": {"type": "string"},
                                        "along_m": _number_or_range_schema(),
                                        "offset_m": _number_or_range_schema(),
                                        "lane": {"type": "string"},
                                    },
                                },
                                "yaw_deg": _nullable(_number_or_range_schema()),
                                "allow_blocking": {"type": ["boolean", "null"]},
                            },
                        },
                    },
                },
            }),
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


def project_scenario_v1_response_schema() -> dict[str, Any]:
    """Return the response schema envelope expected by the common LLM JSON client."""
    return {
        "name": "project_scenario_v1",
        "schema": project_scenario_v1_json_schema(),
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
            "width_m": _number_or_range_schema(),
        },
    }


def _spawn_zone_schema() -> dict[str, Any]:
    """Return optional background pedestrian spawn-zone schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["segments"],
        "properties": {
            "segments": {
                "type": "array",
                "items": {"type": "string"},
            },
        },
    }


def _scatter_zone_schema() -> dict[str, Any]:
    """Return scatter placement zone schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["segments", "lanes"],
        "properties": {
            "segments": {
                "type": "array",
                "items": {"type": "string"},
            },
            "lanes": {
                "type": "array",
                "items": {"type": "string"},
            },
        },
    }


def _palette_schema() -> dict[str, Any]:
    """Return scatter placement palette schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["categories", "classes"],
        "properties": {
            "categories": {
                "type": "array",
                "items": {"type": "string"},
            },
            "classes": {
                "type": "array",
                "items": {"type": "string"},
            },
        },
    }


def _robot_anchor_schema() -> dict[str, Any]:
    """Return the robot start/goal anchor schema."""
    return {
        "anyOf": [
            _abstract_robot_anchor_schema("entry"),
            _abstract_robot_anchor_schema("exit"),
            _corridor_pose_robot_anchor_schema(),
        ],
    }


def _abstract_robot_anchor_schema(anchor_type: str) -> dict[str, Any]:
    """Return an abstract entry/exit robot anchor schema without concrete pose fields."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["type"],
        "properties": {
            "type": {"type": "string", "const": anchor_type},
        },
    }


def _corridor_pose_robot_anchor_schema() -> dict[str, Any]:
    """Return a concrete corridor-local robot anchor schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["type", "segment", "along_m", "offset_m", "lane", "heading"],
        "properties": {
            "type": {"type": "string", "const": "corridor_pose"},
            "segment": {"type": "string"},
            "along_m": _number_or_range_schema(),
            "offset_m": _number_or_range_schema(),
            "lane": {"type": ["string", "null"]},
            "heading": {
                "type": ["string", "null"],
                "enum": ["forward", "backward", "auto", None],
            },
        },
    }
