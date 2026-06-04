from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]

REQUIRED_FILES = [
    ROOT / "app" / "models" / "episode_setup.py",
    ROOT / "app" / "models" / "delivery_bot_setup.py",
    ROOT / "app" / "services" / "episode_setup_validator.py",
    ROOT / "app" / "services" / "delivery_bot_setup_validator.py",
    ROOT / "app" / "services" / "world_config_to_episode_setup_adapter.py",
    ROOT / "app" / "services" / "world_config_to_delivery_bot_setup_adapter.py",
    ROOT / "app" / "services" / "setup_pair_trace_builder.py",
]

FORBIDDEN_ARTIFACTS = [
    ROOT / "samples",
    ROOT / "fixtures",
    ROOT / "data" / "rag" / "vector_db",
    ROOT / "data" / "rag" / "embeddings",
    ROOT / "data" / "rag" / "chroma",
    ROOT / "ue",
    ROOT / "UE",
]

def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _imports_live_http_client(path: Path) -> bool:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name in {"urllib", "urllib.request", "requests", "httpx"} for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module in {"urllib", "urllib.request", "requests", "httpx"}:
            return True
    return False


def run_check() -> dict[str, Any]:
    episode_setup_text = _read(ROOT / "app" / "models" / "episode_setup.py")
    delivery_bot_text = _read(ROOT / "app" / "models" / "delivery_bot_setup.py")
    episode_adapter_text = _read(ROOT / "app" / "services" / "world_config_to_episode_setup_adapter.py")
    delivery_adapter_text = _read(ROOT / "app" / "services" / "world_config_to_delivery_bot_setup_adapter.py")
    forbidden_artifacts = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "setup_pair_models",
        "passed": False,
        "warning": False,
        "requiredFilesExist": all(path.exists() for path in REQUIRED_FILES),
        "episodeSetupUsesNewFields": all(
            term in episode_setup_text for term in ["xy_m", "yaw_deg", "goal_xy_m", "center_xy_m", "points_xy_m"]
        ),
        "episodeSetupExcludesLegacyFields": all(
            term not in episode_setup_text for term in ["location_m:", "rotation_deg:", "transform:", "scale:", "units:"]
        ),
        "deliveryBotSetupUsesTuningFieldsOnly": all(
            term in delivery_bot_text for term in ["drive", "path_follow", "lidar"]
        )
        and all(term not in delivery_bot_text for term in ["instance_id:", "route:", "xy_m:", "yaw_deg:"]),
        "episodeAdapterConvertsCmToM": "_cm_to_m" in episode_adapter_text and "center_xy_m" in episode_adapter_text,
        "deliveryAdapterHasDefaults": all(
            term in delivery_adapter_text for term in ["max_speed_kmh=max_speed", "stop_distance_m=stop_distance", "slow_down_distance_m=slow_down_distance"]
        ),
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden_artifacts,
        "errors": [],
        "warnings": [],
    }
    for key, message in [
        ("requiredFilesExist", "Setup pair model/validator/adapter files are missing."),
        ("episodeSetupUsesNewFields", "EpisodeSetup model must use latest UE coordinate fields."),
        ("episodeSetupExcludesLegacyFields", "EpisodeSetup model must not define legacy transform/unit fields."),
        ("deliveryBotSetupUsesTuningFieldsOnly", "DeliveryBotSetup model must only contain tuning field groups."),
        ("episodeAdapterConvertsCmToM", "EpisodeSetup adapter must convert WorldConfig cm fields to meter fields."),
        ("deliveryAdapterHasDefaults", "DeliveryBotSetup adapter must provide initial default tuning values."),
        ("noLiveProviderCallsInHarness", "Setup pair harness must not perform live provider calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    if forbidden_artifacts:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE artifacts detected.")
    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
