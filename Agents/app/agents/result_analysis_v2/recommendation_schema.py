"""Structured-output schema for result-analysis recommendation LLM calls."""

from __future__ import annotations

from typing import Any

from app.agents.result_analysis_v2.policy_candidate_writer import PARAMETER_LIMITS, RUNTIME_CAP_PARAMETERS


# Supported policy cap keys and upper bounds accepted by RecommendationValidator.
POLICY_PARAMETER_LIMITS = {
    f"{attribute_name}_max": PARAMETER_LIMITS[limit_name]
    for attribute_name, limit_name in RUNTIME_CAP_PARAMETERS
}


def _episode_evidence_schema() -> dict[str, Any]:
    """Return the schema for one allowed episode evidence reference."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["experiment_id", "run_id", "episode_id"],
        "properties": {
            "experiment_id": {"type": "string"},
            "run_id": {"type": "string"},
            "episode_id": {"type": "string"},
        },
    }


def _policy_parameter_content_schema() -> dict[str, Any]:
    """Return the supported policy-parameter adjustment content schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": list(POLICY_PARAMETER_LIMITS),
        "properties": {
            key: {"type": "number", "minimum": 0.0, "maximum": limit}
            for key, limit in POLICY_PARAMETER_LIMITS.items()
        },
    }


def _environment_adjustment_content_schema() -> dict[str, Any]:
    """Return the supported environment adjustment content schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": [
            "increase_min_clear_width_m",
            "disable_allow_blocking",
            "increase_walkway_width_m",
            "reason",
        ],
        "properties": {
            "increase_min_clear_width_m": {"type": "boolean"},
            "disable_allow_blocking": {"type": "boolean"},
            "increase_walkway_width_m": {"type": "boolean"},
            "reason": {"type": "string"},
        },
    }


def _proposed_change_schema() -> dict[str, Any]:
    """Return proposed-change shapes that downstream artifact writers can consume."""
    return {
        "anyOf": [
            {
                "type": "object",
                "additionalProperties": False,
                "required": ["type", "content"],
                "properties": {
                    "type": {"type": "string", "const": "policy_parameter_adjustment"},
                    "content": _policy_parameter_content_schema(),
                },
            },
            {
                "type": "object",
                "additionalProperties": False,
                "required": ["type", "content"],
                "properties": {
                    "type": {"type": "string", "const": "environment_scenario_adjustment"},
                    "content": _environment_adjustment_content_schema(),
                },
            },
        ]
    }


def analysis_recommendations_v2_json_schema() -> dict[str, Any]:
    """Return the strict JSON schema for result-analysis LLM recommendation output."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["recommendations"],
        "properties": {
            "recommendations": {
                "type": "array",
                "items": {
                    "type": "object",
                    "additionalProperties": False,
                    "required": [
                        "id",
                        "target",
                        "priority",
                        "title",
                        "reason",
                        "recommendation",
                        "evidence",
                        "proposed_change",
                    ],
                    "properties": {
                        "id": {"type": "string"},
                        "target": {"type": "string", "enum": ["policy", "environment"]},
                        "priority": {"type": "string", "enum": ["high", "medium", "low"]},
                        "title": {"type": "string"},
                        "reason": {"type": "string"},
                        "recommendation": {"type": "string"},
                        "evidence": {
                            "type": "array",
                            "items": _episode_evidence_schema(),
                        },
                        "proposed_change": _proposed_change_schema(),
                    },
                },
            }
        },
    }


def analysis_recommendations_v2_response_schema() -> dict[str, Any]:
    """Return the response schema envelope expected by the common LLM JSON client."""
    return {
        "name": "analysis_recommendations_v2",
        "schema": analysis_recommendations_v2_json_schema(),
        "strict": True,
    }
