from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
PROCESSOR_PATH = ROOT / "app" / "services" / "world_config_scenario_post_processor.py"
ORCHESTRATOR_PATH = ROOT / "app" / "services" / "world_config_generation_orchestrator.py"
SCENARIO_MODEL_PATH = ROOT / "app" / "models" / "scenario.py"
GENERATION_MODEL_PATH = ROOT / "app" / "models" / "generation.py"
DOC_PATH = ROOT / "docs" / "SCENARIO_POST_PROCESSING.md"
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
RAG_CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"


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
    for path in [PROCESSOR_PATH, ORCHESTRATOR_PATH]:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def _jsonl_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip())


def run_check() -> dict[str, Any]:
    result: dict[str, Any] = {
        "check": "scenario_post_processing",
        "passed": False,
        "warning": False,
        "processorExists": PROCESSOR_PATH.exists(),
        "docExists": DOC_PATH.exists(),
        "scenarioModelsPresent": False,
        "generationModelsPresent": False,
        "orchestratorIntegrated": False,
        "policyCardCount": _jsonl_count(POLICY_CARDS_PATH),
        "ragChunkCount": _jsonl_count(RAG_CHUNKS_PATH),
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if not result["processorExists"]:
        result["errors"].append("app/services/world_config_scenario_post_processor.py is missing.")
    if not result["docExists"]:
        result["errors"].append("docs/SCENARIO_POST_PROCESSING.md is missing.")

    scenario_text = SCENARIO_MODEL_PATH.read_text(encoding="utf-8-sig") if SCENARIO_MODEL_PATH.exists() else ""
    result["scenarioModelsPresent"] = (
        "ScenarioPostProcessPatch" in scenario_text
        and "ScenarioPostProcessResult" in scenario_text
    )
    if not result["scenarioModelsPresent"]:
        result["errors"].append("Scenario post-processing models are missing.")

    generation_text = GENERATION_MODEL_PATH.read_text(encoding="utf-8-sig") if GENERATION_MODEL_PATH.exists() else ""
    result["generationModelsPresent"] = (
        "scenarioPostProcessing" in generation_text
        and "scenarioPostProcessingApplied" in generation_text
        and "scenarioPostProcessingPatches" in generation_text
    )
    if not result["generationModelsPresent"]:
        result["errors"].append("Generation models do not expose scenario post-processing diagnostics.")

    orchestrator_text = ORCHESTRATOR_PATH.read_text(encoding="utf-8-sig") if ORCHESTRATOR_PATH.exists() else ""
    result["orchestratorIntegrated"] = (
        "apply_scenario_intent_to_world_config" in orchestrator_text
        and "scenarioPostProcessing" in orchestrator_text
    )
    if not result["orchestratorIntegrated"]:
        result["errors"].append("Orchestrator does not integrate scenario post-processing.")

    if result["policyCardCount"] != 9:
        result["errors"].append("Policy card count changed from 9.")
    if result["ragChunkCount"] != 9:
        result["errors"].append("Policy RAG chunk count changed from 9.")

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
