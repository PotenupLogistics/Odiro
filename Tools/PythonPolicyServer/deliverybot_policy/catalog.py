from __future__ import annotations

import json
from pathlib import Path
from typing import Any


DEFAULT_CATALOG_PATH = Path(__file__).resolve().parents[1] / "policy_catalog.json"
DEFAULT_CATALOGS_DIR = Path(__file__).resolve().parents[1] / "policy_catalogs"
DEFAULT_CATALOG_ID = "default_delivery"
FALLBACK_POLICY_ID = "normal_path_follow"


def load_policy_catalog(catalog_path: Path | None = None) -> dict[str, Any]:
    path = catalog_path or get_policy_catalog_path(DEFAULT_CATALOG_ID)
    with path.open("r", encoding="utf-8") as file:
        catalog = json.load(file)

    return normalize_policy_catalog(catalog, path)


def normalize_policy_catalog(catalog: Any, catalog_path: Path | None = None) -> dict[str, Any]:
    if not isinstance(catalog, dict):
        raise ValueError("policy catalog root must be an object")

    policies = catalog.get("policies", [])
    if not isinstance(policies, list):
        raise ValueError("policy catalog policies must be an array")

    catalog.setdefault("catalogVersion", 1)
    if catalog_path is not None:
        catalog.setdefault("catalogId", catalog_path.stem)
        catalog.setdefault("relativePath", catalog_path.name)

    return catalog


def get_policy_catalog_path(catalog_id: str, catalog_dir: Path | None = None) -> Path:
    safe_catalog_id = sanitize_catalog_id(catalog_id or DEFAULT_CATALOG_ID)
    search_dir = catalog_dir or DEFAULT_CATALOGS_DIR
    catalog_path = search_dir / f"{safe_catalog_id}.json"

    if catalog_path.exists():
        return catalog_path

    if catalog_id == DEFAULT_CATALOG_ID and DEFAULT_CATALOG_PATH.exists():
        return DEFAULT_CATALOG_PATH

    raise FileNotFoundError(f"policy catalog not found: {catalog_id}")


def sanitize_catalog_id(catalog_id: str) -> str:
    safe_chars = []
    for char in catalog_id:
        if char.isalnum() or char in {"_", "-"}:
            safe_chars.append(char)

    return "".join(safe_chars)


def load_policy_catalog_by_id(catalog_id: str, catalog_dir: Path | None = None) -> dict[str, Any]:
    return load_policy_catalog(get_policy_catalog_path(catalog_id, catalog_dir))


def build_policy_catalog_source_info(catalog_path: Path) -> dict[str, Any] | None:
    try:
        catalog = load_policy_catalog(catalog_path)
    except (OSError, ValueError, json.JSONDecodeError):
        return None

    policies = catalog.get("policies", [])
    safe_policies = policies if isinstance(policies, list) else []

    return {
        "catalogId": str(catalog.get("catalogId", catalog_path.stem)),
        "catalogVersion": int(catalog.get("catalogVersion", 1) or 1),
        "displayName": str(catalog.get("displayName", catalog_path.stem)),
        "description": str(catalog.get("description", "")),
        "relativePath": catalog_path.name,
        "policyCount": len(safe_policies),
    }


def list_policy_catalog_sources(catalog_dir: Path | None = None) -> list[dict[str, Any]]:
    search_dir = catalog_dir or DEFAULT_CATALOGS_DIR
    sources: list[dict[str, Any]] = []

    if search_dir.exists():
        for catalog_path in sorted(search_dir.glob("*.json")):
            source_info = build_policy_catalog_source_info(catalog_path)
            if source_info is not None:
                sources.append(source_info)

    if not sources and DEFAULT_CATALOG_PATH.exists():
        source_info = build_policy_catalog_source_info(DEFAULT_CATALOG_PATH)
        if source_info is not None:
            sources.append(source_info)

    return sources


def get_policy_ids(catalog: dict[str, Any]) -> set[str]:
    policies = catalog.get("policies", [])
    safe_policies = policies if isinstance(policies, list) else []
    policy_ids: set[str] = set()

    for policy in safe_policies:
        if isinstance(policy, dict):
            policy_id = str(policy.get("policyId", ""))
            if policy_id:
                policy_ids.add(policy_id)

    return policy_ids


def build_default_policy_spec(catalog: dict[str, Any]) -> dict[str, Any]:
    policies = catalog.get("policies", [])
    safe_policies = policies if isinstance(policies, list) else []
    enabled_policies: list[dict[str, Any]] = []

    for policy in safe_policies:
        if not isinstance(policy, dict) or not bool(policy.get("defaultEnabled", False)):
            continue

        policy_id = str(policy.get("policyId", ""))
        if not policy_id:
            continue

        enabled_policy = {
            "policyId": policy_id,
            "priority": int(policy.get("defaultPriority", 100) or 100),
        }
        copy_policy_runtime_settings(policy, enabled_policy)
        enabled_policies.append(enabled_policy)

    enabled_policies.sort(key=lambda item: int(item.get("priority", 100) or 100))

    return {
        "catalogId": str(catalog.get("catalogId", "")),
        "catalogVersion": int(catalog.get("catalogVersion", 1) or 1),
        "enabledPolicies": enabled_policies,
    }


def normalize_policy_spec(policy_spec: dict[str, Any] | None, catalog: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(policy_spec, dict) or not policy_spec:
        return build_default_policy_spec(catalog)

    valid_policy_ids = get_policy_ids(catalog)
    enabled_policies = normalize_enabled_policies(policy_spec, valid_policy_ids)

    if not enabled_policies:
        enabled_policies = build_default_policy_spec(catalog)["enabledPolicies"]

    if not any(policy.get("policyId") == FALLBACK_POLICY_ID for policy in enabled_policies):
        enabled_policies.append({"policyId": FALLBACK_POLICY_ID, "priority": 1000})

    enabled_policies.sort(key=lambda item: int(item.get("priority", 100) or 100))

    return {
        "catalogId": str(policy_spec.get("catalogId", catalog.get("catalogId", ""))),
        "catalogVersion": int(policy_spec.get("catalogVersion", catalog.get("catalogVersion", 1)) or 1),
        "enabledPolicies": enabled_policies,
    }


def normalize_enabled_policies(policy_spec: dict[str, Any], valid_policy_ids: set[str]) -> list[dict[str, Any]]:
    enabled_policies = policy_spec.get("enabledPolicies", [])
    if isinstance(enabled_policies, list) and enabled_policies:
        return normalize_enabled_policy_objects(enabled_policies, valid_policy_ids)

    legacy_enabled_rules = policy_spec.get("enabledPolicyRules", [])
    legacy_priority_order = policy_spec.get("policyPriorityOrder", [])
    legacy_policy_name = str(policy_spec.get("policyName", ""))

    requested_ids: list[str] = []
    if isinstance(legacy_priority_order, list):
        requested_ids.extend(str(value) for value in legacy_priority_order)
    if isinstance(legacy_enabled_rules, list):
        requested_ids.extend(str(value) for value in legacy_enabled_rules)
    if legacy_policy_name:
        requested_ids.append(legacy_policy_name)

    normalized: list[dict[str, Any]] = []
    seen_policy_ids: set[str] = set()

    for index, policy_id in enumerate(requested_ids):
        if policy_id not in valid_policy_ids or policy_id in seen_policy_ids:
            continue

        seen_policy_ids.add(policy_id)
        normalized.append(
            {
                "policyId": policy_id,
                "priority": (index + 1) * 10,
            }
        )

    return normalized


def normalize_enabled_policy_objects(
    enabled_policies: list[Any],
    valid_policy_ids: set[str],
) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    seen_policy_ids: set[str] = set()

    for index, policy in enumerate(enabled_policies):
        if isinstance(policy, str):
            policy_id = policy
            priority = (index + 1) * 10
        elif isinstance(policy, dict):
            policy_id = str(policy.get("policyId", ""))
            priority = int(policy.get("priority", (index + 1) * 10) or (index + 1) * 10)
        else:
            continue

        if policy_id not in valid_policy_ids or policy_id in seen_policy_ids:
            continue

        seen_policy_ids.add(policy_id)
        normalized_policy = {
            "policyId": policy_id,
            "priority": priority,
        }

        if isinstance(policy, dict):
            copy_policy_runtime_settings(policy, normalized_policy)

        normalized.append(normalized_policy)

    return normalized


def copy_policy_runtime_settings(source: dict[str, Any], target: dict[str, Any]) -> None:
    for field_name in (
        "pathfinding",
        "dynamicObstacles",
        "dynamic_obstacles",
        "recovery",
        "parameters",
        "safetyStop",
        "safety_stop",
        "dwa",
        "hybridAStar",
        "hybrid_astar",
    ):
        value = source.get(field_name)
        if isinstance(value, dict):
            target[field_name] = value

    for field_name in (
        "stopRerouteDelaySeconds",
        "stop_reroute_delay_seconds",
        "recoveryStrategy",
        "recovery_strategy",
        "planner",
        "plannerMode",
        "pathPlanner",
        "globalPlanner",
    ):
        if field_name in source:
            target[field_name] = source[field_name]
