from __future__ import annotations

import math
from typing import Any

from deliverybot_policy.actions import make_action, make_policy_candidate, make_stop_action
from deliverybot_policy.context import (
    get_float_field,
    get_lidar_spec,
    get_motion_control_spec,
    get_nearest_observed_object,
    get_policy_priority,
    get_robot_state,
)
from deliverybot_policy.dynamic_obstacles import build_dynamic_obstacle_reroute_context
from deliverybot_policy.policies.normal_path_follow import build_path_follow_candidate


POLICY_ID = "front_obstacle_stop"
DEFAULT_STOP_REROUTE_DELAY_SECONDS = 3.0
DEFAULT_REROUTE_ATTEMPT_LIMIT = 3
DEFAULT_REROUTE_ATTEMPT_INTERVAL_SECONDS = 0.75
DEFAULT_RECOVERY_STRATEGY = "reverse_then_reroute"
DEFAULT_REVERSE_DURATION_SECONDS = 0.8
DEFAULT_REVERSE_SPEED_KMH = 1.0
DEFAULT_GRACE_DURATION_SECONDS = 1.5
DEFAULT_GRACE_SPEED_KMH = 1.0
DEFAULT_HARD_STOP_DISTANCE_M = 0.55
DEFAULT_TIME_TO_COLLISION_SECONDS = 0.8
DEFAULT_MIN_SPEED_KMH_FOR_TTC = 0.2


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    observation = context.get("observation", {})
    if not isinstance(observation, dict):
        return None

    nearest_object = get_nearest_observed_object(observation, require_in_front=True)
    if nearest_object is None:
        clear_stop_sustain_state(context)
        return None

    lidar_spec = get_lidar_spec(context)
    stop_distance_m = get_float_field(lidar_spec, "stopDistanceM", 1.5)
    slow_down_distance_m = get_float_field(lidar_spec, "slowDownDistanceM", max(stop_distance_m, 5.0))
    distance_m = float(nearest_object.get("closestDistanceM", 0.0) or 0.0)

    if distance_m > stop_distance_m:
        clear_stop_sustain_state(context)
        return None

    should_stop, safety_debug = should_stop_for_imminent_collision(context, distance_m)
    if not should_stop:
        clear_stop_sustain_state(context)
        return None

    priority = get_policy_priority(context, 10)
    actor_tags = nearest_object.get("actorTags", [])
    safe_actor_tags = actor_tags if isinstance(actor_tags, list) else []
    stop_sustain_seconds = update_stop_sustain_state(context, nearest_object)
    reroute_delay_seconds = get_stop_reroute_delay_seconds(context)
    common_debug = {
        "nearestObjectActor": str(nearest_object.get("actorName", "")),
        "nearestObjectTags": [str(tag) for tag in safe_actor_tags],
        "nearestObjectDistanceM": distance_m,
        "stopDistanceM": stop_distance_m,
        "stopSustainSeconds": stop_sustain_seconds,
        "stopRerouteDelaySeconds": reroute_delay_seconds,
    }
    common_debug.update(safety_debug)

    if stop_sustain_seconds >= reroute_delay_seconds:
        return build_sustained_stop_reroute_candidate(
            context,
            priority,
            max(stop_distance_m, slow_down_distance_m, distance_m),
            common_debug,
        )

    return make_policy_candidate(
        POLICY_ID,
        make_stop_action(),
        "front_object_imminent_collision_stop",
        priority,
        common_debug,
    )


def build_sustained_stop_reroute_candidate(
    context: dict[str, Any],
    priority: int,
    max_distance_m: float,
    debug: dict[str, Any],
) -> dict[str, Any]:
    recovery_candidate = build_active_recovery_candidate(context, priority, debug)
    if recovery_candidate is not None:
        return recovery_candidate

    policy_state = get_stop_policy_state(context)
    recovery_spec = get_recovery_spec(context)
    recovery_strategy = get_recovery_strategy(context)
    attempt_limit = get_int_recovery_setting(
        recovery_spec,
        ("rerouteAttemptLimit", "reroute_attempt_limit"),
        DEFAULT_REROUTE_ATTEMPT_LIMIT,
    )

    if recovery_strategy != "reroute" and attempt_limit > 0:
        reroute_attempt_count = int(policy_state.get("rerouteAttemptCount", 0) or 0)
        if reroute_attempt_count >= attempt_limit:
            return start_recovery_candidate(context, priority, debug)

    register_reroute_attempt(context)
    motion_spec = get_motion_control_spec(context)
    reroute_speed_kmh = get_float_field(
        motion_spec,
        "stopRerouteSpeedKmh",
        get_float_field(motion_spec, "obstacleSlowSpeedKmh", 1.0),
    )
    reroute_context = build_dynamic_obstacle_reroute_context(context, max_distance_m)
    dynamic_debug = reroute_context.pop("dynamicObstacleDebug", {})

    candidate = build_path_follow_candidate(
        reroute_context,
        POLICY_ID,
        "front_obstacle_stop_sustained_dynamic_reroute",
        speed_limit_kmh=reroute_speed_kmh,
    )
    candidate["priority"] = priority

    candidate_debug = candidate.setdefault("debug", {})
    if isinstance(candidate_debug, dict):
        candidate_debug.update(debug)
        candidate_debug["stopRerouteSpeedKmh"] = reroute_speed_kmh
        candidate_debug.update(build_recovery_debug(context))
        if isinstance(dynamic_debug, dict):
            candidate_debug.update(dynamic_debug)

    return candidate


def build_active_recovery_candidate(
    context: dict[str, Any],
    priority: int,
    debug: dict[str, Any],
) -> dict[str, Any] | None:
    policy_state = get_stop_policy_state(context)
    recovery_mode = str(policy_state.get("recoveryMode", ""))
    if not recovery_mode:
        return None

    recovery_spec = get_recovery_spec(context)
    current_time_seconds = get_observation_time_seconds(context)
    recovery_start_time_seconds = float(policy_state.get("recoveryStartTimeSeconds", current_time_seconds))
    recovery_elapsed_seconds = max(current_time_seconds - recovery_start_time_seconds, 0.0)

    if recovery_mode == "reverse_then_reroute":
        reverse_duration_seconds = get_float_recovery_setting(
            recovery_spec,
            ("reverseDurationSeconds", "reverse_duration_seconds"),
            DEFAULT_REVERSE_DURATION_SECONDS,
        )
        if recovery_elapsed_seconds < reverse_duration_seconds:
            return make_reverse_recovery_candidate(
                context,
                priority,
                debug,
                recovery_elapsed_seconds,
                reverse_duration_seconds,
            )

        finish_recovery_cycle(policy_state, current_time_seconds)
        return None

    if recovery_mode == "grace_forward":
        grace_duration_seconds = get_float_recovery_setting(
            recovery_spec,
            ("graceDurationSeconds", "grace_duration_seconds"),
            DEFAULT_GRACE_DURATION_SECONDS,
        )
        if recovery_elapsed_seconds < grace_duration_seconds:
            return make_grace_forward_candidate(
                context,
                priority,
                debug,
                recovery_elapsed_seconds,
                grace_duration_seconds,
            )

        finish_recovery_cycle(policy_state, current_time_seconds)
        return None

    finish_recovery_cycle(policy_state, current_time_seconds)
    return None


def start_recovery_candidate(
    context: dict[str, Any],
    priority: int,
    debug: dict[str, Any],
) -> dict[str, Any]:
    recovery_strategy = get_recovery_strategy(context)
    policy_state = get_stop_policy_state(context)
    current_time_seconds = get_observation_time_seconds(context)

    if recovery_strategy == "stop":
        return make_policy_candidate(
            POLICY_ID,
            make_stop_action(),
            "front_obstacle_stop_reroute_attempt_limit_stop",
            priority,
            {**debug, **build_recovery_debug(context)},
        )

    if recovery_strategy == "grace_forward":
        policy_state["recoveryMode"] = "grace_forward"
        policy_state["recoveryStartTimeSeconds"] = current_time_seconds
        return build_active_recovery_candidate(context, priority, debug) or make_policy_candidate(
            POLICY_ID,
            make_stop_action(),
            "front_obstacle_stop_recovery_failed",
            priority,
            {**debug, **build_recovery_debug(context)},
        )

    policy_state["recoveryMode"] = "reverse_then_reroute"
    policy_state["recoveryStartTimeSeconds"] = current_time_seconds
    return build_active_recovery_candidate(context, priority, debug) or make_policy_candidate(
        POLICY_ID,
        make_stop_action(),
        "front_obstacle_stop_recovery_failed",
        priority,
        {**debug, **build_recovery_debug(context)},
    )


def make_reverse_recovery_candidate(
    context: dict[str, Any],
    priority: int,
    debug: dict[str, Any],
    recovery_elapsed_seconds: float,
    recovery_duration_seconds: float,
) -> dict[str, Any]:
    recovery_spec = get_recovery_spec(context)
    reverse_speed_kmh = get_float_recovery_setting(
        recovery_spec,
        ("reverseSpeedKmh", "reverse_speed_kmh"),
        DEFAULT_REVERSE_SPEED_KMH,
    )
    reverse_steering = get_float_recovery_setting(
        recovery_spec,
        ("reverseSteering", "reverse_steering"),
        0.0,
    )
    recovery_debug = build_recovery_debug(context)
    recovery_debug.update(
        {
            "recoveryElapsedSeconds": recovery_elapsed_seconds,
            "recoveryDurationSeconds": recovery_duration_seconds,
            "reverseSpeedKmh": reverse_speed_kmh,
            "reverseSteering": reverse_steering,
        }
    )

    return make_policy_candidate(
        POLICY_ID,
        make_action(reverse_steering, reverse_speed_kmh, direction="Reverse"),
        "front_obstacle_stop_recovery_reverse",
        priority,
        {**debug, **recovery_debug},
    )


def make_grace_forward_candidate(
    context: dict[str, Any],
    priority: int,
    debug: dict[str, Any],
    recovery_elapsed_seconds: float,
    recovery_duration_seconds: float,
) -> dict[str, Any]:
    recovery_spec = get_recovery_spec(context)
    grace_speed_kmh = get_float_recovery_setting(
        recovery_spec,
        ("graceSpeedKmh", "grace_speed_kmh"),
        DEFAULT_GRACE_SPEED_KMH,
    )
    candidate = build_path_follow_candidate(
        context,
        POLICY_ID,
        "front_obstacle_stop_recovery_grace_forward",
        speed_limit_kmh=grace_speed_kmh,
    )
    candidate["priority"] = priority

    candidate_debug = candidate.setdefault("debug", {})
    if isinstance(candidate_debug, dict):
        candidate_debug.update(debug)
        candidate_debug.update(build_recovery_debug(context))
        candidate_debug.update(
            {
                "recoveryElapsedSeconds": recovery_elapsed_seconds,
                "recoveryDurationSeconds": recovery_duration_seconds,
                "graceSpeedKmh": grace_speed_kmh,
            }
        )

    return candidate


def register_reroute_attempt(context: dict[str, Any]) -> None:
    recovery_spec = get_recovery_spec(context)
    attempt_interval_seconds = get_float_recovery_setting(
        recovery_spec,
        ("rerouteAttemptIntervalSeconds", "reroute_attempt_interval_seconds"),
        DEFAULT_REROUTE_ATTEMPT_INTERVAL_SECONDS,
    )
    current_time_seconds = get_observation_time_seconds(context)
    policy_state = get_stop_policy_state(context)
    last_attempt_time_seconds = policy_state.get("lastRerouteAttemptTimeSeconds")

    if (
        last_attempt_time_seconds is not None
        and current_time_seconds - float(last_attempt_time_seconds) < attempt_interval_seconds
    ):
        return

    policy_state["lastRerouteAttemptTimeSeconds"] = current_time_seconds
    policy_state["rerouteAttemptCount"] = int(policy_state.get("rerouteAttemptCount", 0) or 0) + 1


def finish_recovery_cycle(policy_state: dict[str, Any], current_time_seconds: float) -> None:
    policy_state.pop("recoveryMode", None)
    policy_state.pop("recoveryStartTimeSeconds", None)
    policy_state["rerouteAttemptCount"] = 0
    policy_state["lastRerouteAttemptTimeSeconds"] = current_time_seconds
    policy_state["lastRecoveryEndTimeSeconds"] = current_time_seconds


def get_stop_reroute_delay_seconds(context: dict[str, Any]) -> float:
    policy_entry = context.get("policyEntry", {})
    safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}
    parameters = safe_policy_entry.get("parameters", {})
    safe_parameters = parameters if isinstance(parameters, dict) else {}

    for source in (safe_policy_entry, safe_parameters):
        for field_name in ("stopRerouteDelaySeconds", "stop_reroute_delay_seconds"):
            if field_name in source:
                return max(float(source.get(field_name, DEFAULT_STOP_REROUTE_DELAY_SECONDS) or 0.0), 0.0)

    return DEFAULT_STOP_REROUTE_DELAY_SECONDS


def get_recovery_spec(context: dict[str, Any]) -> dict[str, Any]:
    policy_entry = context.get("policyEntry", {})
    safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}

    recovery = safe_policy_entry.get("recovery", {})
    if isinstance(recovery, dict):
        return recovery

    parameters = safe_policy_entry.get("parameters", {})
    if isinstance(parameters, dict) and isinstance(parameters.get("recovery", {}), dict):
        return parameters["recovery"]

    return {}


def get_safety_stop_spec(context: dict[str, Any]) -> dict[str, Any]:
    policy_entry = context.get("policyEntry", {})
    safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}

    for source in (
        safe_policy_entry,
        safe_policy_entry.get("parameters", {}),
    ):
        if not isinstance(source, dict):
            continue

        for field_name in ("safetyStop", "safety_stop"):
            value = source.get(field_name, {})
            if isinstance(value, dict):
                return value

    return {}


def get_float_safety_setting(
    safety_spec: dict[str, Any],
    field_names: tuple[str, ...],
    default_value: float,
) -> float:
    for field_name in field_names:
        if field_name not in safety_spec:
            continue

        try:
            return max(float(safety_spec.get(field_name, default_value) or 0.0), 0.0)
        except (TypeError, ValueError):
            return default_value

    return default_value


def should_stop_for_imminent_collision(
    context: dict[str, Any],
    distance_m: float,
) -> tuple[bool, dict[str, Any]]:
    safety_spec = get_safety_stop_spec(context)
    hard_stop_distance_m = get_float_safety_setting(
        safety_spec,
        ("hardStopDistanceM", "hard_stop_distance_m"),
        DEFAULT_HARD_STOP_DISTANCE_M,
    )
    ttc_threshold_seconds = get_float_safety_setting(
        safety_spec,
        ("timeToCollisionSeconds", "time_to_collision_seconds"),
        DEFAULT_TIME_TO_COLLISION_SECONDS,
    )
    min_speed_kmh_for_ttc = get_float_safety_setting(
        safety_spec,
        ("minSpeedKmhForTtc", "min_speed_kmh_for_ttc"),
        DEFAULT_MIN_SPEED_KMH_FOR_TTC,
    )

    robot_state = get_robot_state(context)
    speed_kmh = abs(get_float_field(robot_state, "speedKmh", 0.0))
    speed_mps = speed_kmh / 3.6
    time_to_collision_seconds = math.inf
    if speed_mps > 1.0e-3:
        time_to_collision_seconds = distance_m / speed_mps

    b_hard_stop_distance_triggered = distance_m <= hard_stop_distance_m
    b_time_to_collision_triggered = (
        speed_kmh >= min_speed_kmh_for_ttc
        and time_to_collision_seconds <= ttc_threshold_seconds
    )
    b_should_stop = b_hard_stop_distance_triggered or b_time_to_collision_triggered
    time_to_collision_debug = (
        time_to_collision_seconds
        if math.isfinite(time_to_collision_seconds)
        else None
    )

    return b_should_stop, {
        "safetyStopStatus": "imminent" if b_should_stop else "defer_to_local_planner",
        "hardStopDistanceM": hard_stop_distance_m,
        "timeToCollisionSeconds": time_to_collision_debug,
        "timeToCollisionThresholdSeconds": ttc_threshold_seconds,
        "minSpeedKmhForTtc": min_speed_kmh_for_ttc,
        "robotSpeedKmh": speed_kmh,
        "bHardStopDistanceTriggered": b_hard_stop_distance_triggered,
        "bTimeToCollisionTriggered": b_time_to_collision_triggered,
    }


def get_recovery_strategy(context: dict[str, Any]) -> str:
    recovery_spec = get_recovery_spec(context)
    strategy = str(recovery_spec.get("strategy", "") or "").strip()
    if not strategy:
        policy_entry = context.get("policyEntry", {})
        safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}
        strategy = str(
            safe_policy_entry.get("recoveryStrategy", safe_policy_entry.get("recovery_strategy", ""))
            or ""
        ).strip()

    normalized_strategy = strategy.lower().replace("-", "_")
    if normalized_strategy in {"reverse", "reverse_then_reroute", "back_up"}:
        return "reverse_then_reroute"
    if normalized_strategy in {"grace", "grace_forward", "ignore_stop_grace"}:
        return "grace_forward"
    if normalized_strategy in {"stop", "safe_stop"}:
        return "stop"
    if normalized_strategy in {"reroute", "keep_rerouting", "none", "disabled"}:
        return "reroute"

    return DEFAULT_RECOVERY_STRATEGY


def get_float_recovery_setting(
    recovery_spec: dict[str, Any],
    field_names: tuple[str, ...],
    default_value: float,
) -> float:
    for field_name in field_names:
        if field_name not in recovery_spec:
            continue

        try:
            return max(float(recovery_spec.get(field_name, default_value) or 0.0), 0.0)
        except (TypeError, ValueError):
            return default_value

    return default_value


def get_int_recovery_setting(
    recovery_spec: dict[str, Any],
    field_names: tuple[str, ...],
    default_value: int,
) -> int:
    for field_name in field_names:
        if field_name not in recovery_spec:
            continue

        try:
            return max(int(recovery_spec.get(field_name, default_value) or 0), 0)
        except (TypeError, ValueError):
            return default_value

    return default_value


def build_recovery_debug(context: dict[str, Any]) -> dict[str, Any]:
    policy_state = get_stop_policy_state(context)
    recovery_spec = get_recovery_spec(context)

    return {
        "recoveryStrategy": get_recovery_strategy(context),
        "rerouteAttemptCount": int(policy_state.get("rerouteAttemptCount", 0) or 0),
        "rerouteAttemptLimit": get_int_recovery_setting(
            recovery_spec,
            ("rerouteAttemptLimit", "reroute_attempt_limit"),
            DEFAULT_REROUTE_ATTEMPT_LIMIT,
        ),
        "recoveryMode": str(policy_state.get("recoveryMode", "")),
    }


def get_observation_time_seconds(context: dict[str, Any]) -> float:
    observation = context.get("observation", {})
    safe_observation = observation if isinstance(observation, dict) else {}
    return get_float_field(safe_observation, "worldTimeSeconds", 0.0)


def update_stop_sustain_state(context: dict[str, Any], nearest_object: dict[str, Any]) -> float:
    current_time_seconds = get_observation_time_seconds(context)
    policy_state = get_stop_policy_state(context)
    obstacle_key = build_obstacle_state_key(nearest_object)

    if policy_state.get("obstacleKey") != obstacle_key:
        policy_state.clear()
        policy_state["obstacleKey"] = obstacle_key
        policy_state["startTimeSeconds"] = current_time_seconds

    start_time_seconds = float(policy_state.get("startTimeSeconds", current_time_seconds))
    stop_sustain_seconds = max(current_time_seconds - start_time_seconds, 0.0)
    policy_state["lastTimeSeconds"] = current_time_seconds
    policy_state["stopSustainSeconds"] = stop_sustain_seconds
    return stop_sustain_seconds


def clear_stop_sustain_state(context: dict[str, Any]) -> None:
    policy_runtime_state = context.get("policyRuntimeState", {})
    if not isinstance(policy_runtime_state, dict):
        return

    policy_runtime_state.pop(POLICY_ID, None)


def get_stop_policy_state(context: dict[str, Any]) -> dict[str, Any]:
    policy_runtime_state = context.get("policyRuntimeState", {})
    if not isinstance(policy_runtime_state, dict):
        return {}

    policy_state = policy_runtime_state.get(POLICY_ID)
    if not isinstance(policy_state, dict):
        policy_state = {}
        policy_runtime_state[POLICY_ID] = policy_state

    return policy_state


def build_obstacle_state_key(nearest_object: dict[str, Any]) -> str:
    actor_name = str(nearest_object.get("actorName", ""))
    if actor_name:
        return actor_name

    return f"unknown:{float(nearest_object.get('closestRayYawDegree', 0.0) or 0.0):.1f}"
