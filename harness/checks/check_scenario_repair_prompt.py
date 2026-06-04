from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
BUILDER_PATH = ROOT / "app" / "services" / "world_config_scenario_repair_prompt_builder.py"
ORCHESTRATOR_PATH = ROOT / "app" / "services" / "world_config_generation_orchestrator.py"
DOC_PATH = ROOT / "docs" / "architecture" / "SCENARIO_REPAIR_PROMPT.md"


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


def _detect_openai_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [BUILDER_PATH, ORCHESTRATOR_PATH]:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def run_check() -> dict[str, Any]:
    result: dict[str, Any] = {
        "check": "scenario_repair_prompt",
        "passed": False,
        "warning": False,
        "builderExists": BUILDER_PATH.exists(),
        "docExists": DOC_PATH.exists(),
        "orchestratorHasScenarioRepair": False,
        "promptHasRequiredInstructions": False,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }
    if not result["builderExists"]:
        result["errors"].append("app/services/world_config_scenario_repair_prompt_builder.py is missing.")
    if not result["docExists"]:
        result["errors"].append("docs/architecture/SCENARIO_REPAIR_PROMPT.md is missing.")

    orchestrator_text = ORCHESTRATOR_PATH.read_text(encoding="utf-8-sig") if ORCHESTRATOR_PATH.exists() else ""
    result["orchestratorHasScenarioRepair"] = "scenario_repair" in orchestrator_text
    if not result["orchestratorHasScenarioRepair"]:
        result["errors"].append("Orchestrator does not include scenario_repair prompt type.")

    builder_text = BUILDER_PATH.read_text(encoding="utf-8-sig") if BUILDER_PATH.exists() else ""
    required_terms = ["Kickboard", "blockingRatio", "pedestrians[].behavior", "Preserve required fields"]
    result["promptHasRequiredInstructions"] = all(term in builder_text for term in required_terms)
    if not result["promptHasRequiredInstructions"]:
        result["errors"].append("Scenario repair prompt lacks required semantic repair instructions.")

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
