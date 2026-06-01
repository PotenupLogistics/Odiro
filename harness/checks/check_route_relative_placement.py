from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
UTIL = ROOT / "app" / "services" / "route_geometry_utils.py"
DOC = ROOT / "docs" / "ROUTE_RELATIVE_PLACEMENT.md"
INTENT = ROOT / "app" / "services" / "world_config_scenario_intent_extractor.py"
POST = ROOT / "app" / "services" / "world_config_scenario_post_processor.py"
EPISODE_REFLECTION = ROOT / "app" / "services" / "episode_spec_scenario_reflection.py"

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
    intent_text = _read(INTENT)
    post_text = _read(POST)
    episode_reflection_text = _read(EPISODE_REFLECTION)
    doc_text = _read(DOC)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "route_relative_placement",
        "passed": False,
        "warning": False,
        "utilityExists": UTIL.exists(),
        "docExists": DOC.exists(),
        "intentExtractorHasRouteMidpoint": "route_midpoint" in intent_text and "경로 중앙" in intent_text,
        "postProcessorSetsRouteMidpoint": "set_obstacle_position_to_route_midpoint" in post_text
        and "add_generic_obstacle_at_route_midpoint" in post_text,
        "episodeReflectionChecksMidpoint": "obstacle_not_near_route_midpoint" in episode_reflection_text
        and "route_midpoint" in episode_reflection_text,
        "docStatesDeterministicRule": "deterministic geometry rule" in doc_text and "midpoint" in doc_text,
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("utilityExists", "app/services/route_geometry_utils.py is missing."),
        ("docExists", "docs/ROUTE_RELATIVE_PLACEMENT.md is missing."),
        ("intentExtractorHasRouteMidpoint", "Scenario intent extractor must detect route_midpoint expressions."),
        ("postProcessorSetsRouteMidpoint", "Post-processor must add/set obstacles at route midpoint."),
        ("episodeReflectionChecksMidpoint", "EpisodeSpec reflection must validate route midpoint placement."),
        ("docStatesDeterministicRule", "Route placement doc must describe deterministic midpoint handling."),
        ("noLiveProviderCallsInHarness", "Harness check must not perform live provider calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    if forbidden:
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
