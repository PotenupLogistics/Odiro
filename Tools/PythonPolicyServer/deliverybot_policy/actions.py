from __future__ import annotations

from typing import Any


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def make_action(
    steering: float,
    target_speed_kmh: float,
    direction: str = "Forward",
    brake: float | None = None,
) -> dict[str, Any]:
    safe_speed_kmh = max(float(target_speed_kmh), 0.0)
    safe_brake = 1.0 if safe_speed_kmh <= 0.0 else 0.0
    if brake is not None:
        safe_brake = clamp(float(brake), 0.0, 1.0)

    return {
        "steering": clamp(float(steering), -1.0, 1.0),
        "throttle": 1.0 if safe_speed_kmh > 0.0 and safe_brake <= 0.0 else 0.0,
        "brake": safe_brake,
        "targetSpeedKmh": safe_speed_kmh,
        "direction": direction if direction in {"Forward", "Reverse"} else "Forward",
    }


def make_stop_action(direction: str = "Forward") -> dict[str, Any]:
    return make_action(0.0, 0.0, direction=direction, brake=1.0)


def make_policy_candidate(
    policy_id: str,
    action: dict[str, Any],
    reason: str,
    priority: int,
    debug: dict[str, Any] | None = None,
) -> dict[str, Any]:
    return {
        "policyId": policy_id,
        "priority": int(priority),
        "reason": reason,
        "action": action,
        "debug": debug or {},
    }


def make_policy_response(
    observation: dict[str, Any],
    candidate: dict[str, Any],
    candidate_count: int,
    status: str = "ok",
) -> dict[str, Any]:
    debug = {
        "policyName": str(candidate.get("policyId", "unknown_policy")),
        "selectedPolicyId": str(candidate.get("policyId", "unknown_policy")),
        "selectedPolicyPriority": int(candidate.get("priority", 0) or 0),
        "reason": str(candidate.get("reason", "")),
        "candidateCount": int(candidate_count),
    }

    candidate_debug = candidate.get("debug", {})
    if isinstance(candidate_debug, dict):
        debug.update(candidate_debug)

    return {
        "sequence": int(observation.get("sequence", 0) or 0),
        "status": status,
        "debug": debug,
        "action": candidate.get("action", make_stop_action()),
    }
