from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
HANDOFF_MODEL = ROOT / "app" / "models" / "handoff.py"
HANDOFF_SERVICE = ROOT / "app" / "services" / "ue5_world_config_handoff_service.py"
ROUTES = ROOT / "app" / "api" / "routes.py"
SUMMARY = ROOT / "app" / "utils" / "handoff_response_summary.py"
EXPORT_SCRIPT = ROOT / "scripts" / "export_ue5_handoff_payload.py"
SMOKE_SCRIPT = ROOT / "scripts" / "run_ue5_handoff_smoke.py"

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
    model_text = _read(HANDOFF_MODEL)
    service_text = _read(HANDOFF_SERVICE)
    routes_text = _read(ROUTES)
    summary_text = _read(SUMMARY)
    export_text = _read(EXPORT_SCRIPT)
    smoke_text = _read(SMOKE_SCRIPT)
    forbidden_artifacts = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "setup_pair_handoff",
        "passed": False,
        "warning": False,
        "modelAllowsSetupPair": "setup_pair" in model_text,
        "responseHasSetupPairFields": all(
            term in model_text
            for term in ["episodeSetup", "deliveryBotSetup", "episodeSetupValidation", "deliveryBotSetupValidation"]
        ),
        "routeAllowsSetupPair": "setup_pair" in routes_text,
        "serviceCallsSetupPairAdapters": all(
            term in service_text
            for term in [
                "convert_world_config_to_episode_setup",
                "convert_world_config_to_delivery_bot_setup",
                "validate_episode_setup",
                "validate_delivery_bot_setup",
                "setupPairTrace",
            ]
        ),
        "summaryIncludesSetupPairFields": all(
            term in summary_text
            for term in [
                "episodeSetupExists",
                "deliveryBotSetupExists",
                "episodeSetupValidationPassed",
                "deliveryBotStopDistanceM",
                "setupPairTraceExists",
            ]
        ),
        "scriptsAcceptSetupPair": "setup_pair" in export_text and "setup_pair" in smoke_text,
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden_artifacts,
        "errors": [],
        "warnings": [],
    }
    for key, message in [
        ("modelAllowsSetupPair", "Handoff request model must allow responseFormat=setup_pair."),
        ("responseHasSetupPairFields", "Handoff response model must include setup pair fields."),
        ("routeAllowsSetupPair", "UE5 handoff route must allow setup_pair query override."),
        ("serviceCallsSetupPairAdapters", "Handoff service must call setup pair adapters and validators."),
        ("summaryIncludesSetupPairFields", "Handoff summary must include setup pair fields."),
        ("scriptsAcceptSetupPair", "Export/smoke scripts must accept setup_pair format."),
        ("noLiveProviderCallsInHarness", "Setup pair handoff harness must not perform live provider calls."),
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
