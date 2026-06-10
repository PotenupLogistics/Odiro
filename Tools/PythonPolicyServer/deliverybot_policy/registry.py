from __future__ import annotations

from typing import Any, Callable

from deliverybot_policy.actions import make_policy_candidate, make_policy_response, make_stop_action
from deliverybot_policy.catalog import build_default_policy_spec, normalize_policy_spec
from deliverybot_policy.policies import (
    dwa_local_avoidance,
    front_obstacle_slowdown,
    front_obstacle_stop,
    normal_path_follow,
    reroute_when_blocked,
)
from deliverybot_policy.selection import select_policy_candidate


PolicyEvaluator = Callable[[dict[str, Any]], dict[str, Any] | None]

POLICY_EVALUATORS: dict[str, PolicyEvaluator] = {
    front_obstacle_stop.POLICY_ID: front_obstacle_stop.evaluate,
    reroute_when_blocked.POLICY_ID: reroute_when_blocked.evaluate,
    dwa_local_avoidance.POLICY_ID: dwa_local_avoidance.evaluate,
    front_obstacle_slowdown.POLICY_ID: front_obstacle_slowdown.evaluate,
    normal_path_follow.POLICY_ID: normal_path_follow.evaluate,
}


def build_runtime_policy_response(context: dict[str, Any]) -> dict[str, Any]:
    observation = context.get("observation", {})
    safe_observation = observation if isinstance(observation, dict) else {}
    catalog = context.get("policyCatalog", {})
    policy_spec = normalize_policy_spec(
        context.get("policySpec", {}) if isinstance(context.get("policySpec", {}), dict) else {},
        catalog if isinstance(catalog, dict) else {},
    )
    policy_spec = apply_dwa_mode_to_policy_spec(context, policy_spec, catalog if isinstance(catalog, dict) else {})
    candidates: list[dict[str, Any]] = []

    best_candidate_priority: int | None = None
    for policy_entry in sorted_enabled_policies(policy_spec):
        if not isinstance(policy_entry, dict):
            continue
        priority = int(policy_entry.get("priority", 100) or 100)
        if best_candidate_priority is not None and priority > best_candidate_priority:
            break

        policy_id = str(policy_entry.get("policyId", ""))
        evaluator = POLICY_EVALUATORS.get(policy_id)
        if evaluator is None:
            continue

        policy_context = dict(context)
        policy_context["policyEntry"] = policy_entry
        candidate = evaluator(policy_context)
        if candidate is not None:
            candidates.append(candidate)
            candidate_priority = int(candidate.get("priority", priority) or priority)
            best_candidate_priority = (
                candidate_priority
                if best_candidate_priority is None
                else min(best_candidate_priority, candidate_priority)
            )

    selected_candidate = select_policy_candidate(candidates)
    if selected_candidate is None:
        selected_candidate = make_policy_candidate(
            "fallback_stop",
            make_stop_action(),
            "no_enabled_policy_returned_action",
            9999,
            {"policySpec": policy_spec},
        )

    response = make_policy_response(safe_observation, selected_candidate, len(candidates))
    response["debug"]["policyCatalogVersion"] = int(policy_spec.get("catalogVersion", 0) or 0)
    response["debug"]["enabledPolicies"] = [
        str(policy.get("policyId", ""))
        for policy in policy_spec.get("enabledPolicies", [])
        if isinstance(policy, dict)
    ]
    return response


def sorted_enabled_policies(policy_spec: dict[str, Any]) -> list[dict[str, Any]]:
    enabled_policies = [
        policy
        for policy in policy_spec.get("enabledPolicies", [])
        if isinstance(policy, dict)
    ]
    return sorted(enabled_policies, key=lambda item: int(item.get("priority", 100) or 100))


def apply_dwa_mode_to_policy_spec(
    context: dict[str, Any],
    policy_spec: dict[str, Any],
    catalog: dict[str, Any],
) -> dict[str, Any]:
    dwa_mode = normalize_dwa_mode(context.get("dwaMode", "policy"))
    if dwa_mode == "policy":
        return policy_spec

    enabled_policies = [
        dict(policy)
        for policy in policy_spec.get("enabledPolicies", [])
        if isinstance(policy, dict)
    ]

    if dwa_mode == "off":
        return {
            **policy_spec,
            "enabledPolicies": [
                policy
                for policy in enabled_policies
                if str(policy.get("policyId", "")) != dwa_local_avoidance.POLICY_ID
            ],
        }

    if any(str(policy.get("policyId", "")) == dwa_local_avoidance.POLICY_ID for policy in enabled_policies):
        return policy_spec

    enabled_policies.append(build_default_dwa_policy_entry(catalog))
    enabled_policies.sort(key=lambda item: int(item.get("priority", 100) or 100))
    return {
        **policy_spec,
        "enabledPolicies": enabled_policies,
    }


def build_default_dwa_policy_entry(catalog: dict[str, Any]) -> dict[str, Any]:
    default_spec = build_default_policy_spec(catalog)
    for policy in default_spec.get("enabledPolicies", []):
        if isinstance(policy, dict) and str(policy.get("policyId", "")) == dwa_local_avoidance.POLICY_ID:
            return dict(policy)

    return {
        "policyId": dwa_local_avoidance.POLICY_ID,
        "priority": 25,
    }


def normalize_dwa_mode(value: Any) -> str:
    mode = str(value or "policy").strip().lower()
    return mode if mode in {"policy", "on", "off"} else "policy"
