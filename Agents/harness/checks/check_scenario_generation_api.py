from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
ROUTES = ROOT / "app" / "api" / "routes.py"
MODEL = ROOT / "app" / "models" / "scenario_generation.py"
SERVICE = ROOT / "app" / "services" / "scenario_generation_service.py"
TESTS = [
    ROOT / "tests" / "test_scenario_generation_api.py",
    ROOT / "tests" / "test_scenario_generation_service.py",
]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _imports_live_http_client(path: Path) -> bool:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name in {"urllib", "urllib.request", "requests", "httpx", "openai", "ollama"} for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module in {"urllib", "urllib.request", "requests", "httpx", "openai", "ollama"}:
            return True
    return False


def run_check() -> dict[str, Any]:
    routes_text = _read(ROUTES)
    model_text = _read(MODEL)
    service_text = _read(SERVICE)
    tests_text = "\n".join(_read(path) for path in TESTS)
    result: dict[str, Any] = {
        "check": "scenario_generation_api",
        "passed": False,
        "warning": False,
        "routeExists": "/api/v1/scenarios/generate" in routes_text,
        "promptWithOptionalEpisodeCountModel": all(
            term in model_text
            for term in ["ScenarioGenerateRequest", "extra=\"forbid\"", "prompt", "episode_count", "ge=1", "strict=True"]
        ),
        "serviceUsesOpenAiOnly": "provider=LlmProvider.openai" in service_text and "llmProviderChain=[\"openai\"]" in service_text,
        "serviceUsesRunQueueGenerator": "generate_setup_pair_queue" in service_text and "export_run_queue_package" in service_text,
        "testsCoverRequestValidation": all(term in tests_text for term in ["episode_count", "episodeCount", "1.5", "\"3\"", "openapi"]),
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "errors": [],
        "warnings": [],
    }
    for key, message in [
        ("routeExists", "Scenario generation API route is missing."),
        ("promptWithOptionalEpisodeCountModel", "Scenario generation request model must accept prompt and optional positive integer episode_count only."),
        ("serviceUsesOpenAiOnly", "Scenario generation service must call the selected OpenAI provider only."),
        ("serviceUsesRunQueueGenerator", "Scenario generation service must generate/export RunQueue packages."),
        ("testsCoverRequestValidation", "Scenario generation tests must cover episode_count validation and extra field rejection."),
        ("noLiveProviderCallsInHarness", "Scenario generation harness must not perform live provider calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    result["passed"] = not result["errors"]
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
