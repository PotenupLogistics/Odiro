from __future__ import annotations

from typing import Any, Callable

from deliverybot_policy.actions import make_policy_candidate, make_policy_response, make_stop_action
from deliverybot_policy.catalog import normalize_policy_spec
from deliverybot_policy.policies import (
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
    candidates: list[dict[str, Any]] = []

    for policy_entry in policy_spec.get("enabledPolicies", []):
        if not isinstance(policy_entry, dict):
            continue

        policy_id = str(policy_entry.get("policyId", ""))
        evaluator = POLICY_EVALUATORS.get(policy_id)
        if evaluator is None:
            continue

        policy_context = dict(context)
        policy_context["policyEntry"] = policy_entry
        candidate = evaluator(policy_context)
        if candidate is not None:
            candidates.append(candidate)

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
