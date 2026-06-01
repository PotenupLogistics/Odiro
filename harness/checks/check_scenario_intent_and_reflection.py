from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]


def _base_result() -> dict[str, Any]:
    return {
        "check": "scenario_intent_and_reflection",
        "passed": False,
        "warning": False,
        "filesExist": {},
        "promptBuilderHasScenarioRequirements": False,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }


def _detect_openai_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    paths = [
        ROOT / "app" / "models" / "scenario.py",
        ROOT / "app" / "services" / "world_config_scenario_intent_extractor.py",
        ROOT / "app" / "services" / "world_config_scenario_reflection.py",
        ROOT / "app" / "services" / "world_config_prompt_builder.py",
        ROOT / "app" / "services" / "world_config_generation_orchestrator.py",
    ]
    found: list[str] = []
    for path in paths:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
    ]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    required_files = [
        ROOT / "app" / "models" / "scenario.py",
        ROOT / "app" / "services" / "world_config_scenario_intent_extractor.py",
        ROOT / "app" / "services" / "world_config_scenario_reflection.py",
        ROOT / "docs" / "SCENARIO_INTENT_EXTRACTION.md",
        ROOT / "docs" / "SCENARIO_REFLECTION_VALIDATION.md",
    ]
    for path in required_files:
        key = path.relative_to(ROOT).as_posix()
        result["filesExist"][key] = path.exists()
        if not path.exists():
            result["errors"].append(f"{key} is missing.")

    prompt_builder = ROOT / "app" / "services" / "world_config_prompt_builder.py"
    text = prompt_builder.read_text(encoding="utf-8-sig") if prompt_builder.exists() else ""
    result["promptBuilderHasScenarioRequirements"] = "Scenario Requirements" in text
    if not result["promptBuilderHasScenarioRequirements"]:
        result["errors"].append("Prompt builder does not include Scenario Requirements guidance.")

    result["openAiImports"] = _detect_openai_code()
    if result["openAiImports"]:
        result["errors"].append("OpenAI SDK import or call code detected.")

    result["forbiddenArtifacts"] = _detect_forbidden_artifacts()
    if result["forbiddenArtifacts"]:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
