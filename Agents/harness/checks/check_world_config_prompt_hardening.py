from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCHEMA_SUMMARY_PATH = ROOT / "app" / "services" / "world_config_schema_summary.py"
PROMPT_BUILDER_PATH = ROOT / "app" / "services" / "world_config_prompt_builder.py"
SMOKE_SCRIPT_PATH = ROOT / "scripts" / "run_ollama_world_config_smoke.py"
GUIDE_PATH = ROOT / "docs" / "architecture" / "WORLD_CONFIG_PROMPT_HARDENING.md"
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
RAG_CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
EXPECTED_POLICY_CARD_COUNT = 11
EXPECTED_RAG_CHUNK_COUNT = 17


def _base_result() -> dict[str, Any]:
    return {
        "check": "world_config_prompt_hardening",
        "passed": False,
        "warning": False,
        "schemaSummaryExists": False,
        "guideExists": False,
        "requiredChecklistInPrompt": False,
        "extraKeyGuidanceInPrompt": False,
        "repairGuidanceInPrompt": False,
        "policyCardCount": 0,
        "ragChunkCount": 0,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }


def _count_jsonl(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip())


def _detect_openai_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [SCHEMA_SUMMARY_PATH, PROMPT_BUILDER_PATH, SMOKE_SCRIPT_PATH]:
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
    result["schemaSummaryExists"] = SCHEMA_SUMMARY_PATH.exists()
    result["guideExists"] = GUIDE_PATH.exists()
    result["policyCardCount"] = _count_jsonl(POLICY_CARDS_PATH)
    result["ragChunkCount"] = _count_jsonl(RAG_CHUNKS_PATH)

    if not result["schemaSummaryExists"]:
        result["errors"].append("app/services/world_config_schema_summary.py is missing.")
    if not result["guideExists"]:
        result["errors"].append("docs/architecture/WORLD_CONFIG_PROMPT_HARDENING.md is missing.")

    prompt_text = PROMPT_BUILDER_PATH.read_text(encoding="utf-8-sig") if PROMPT_BUILDER_PATH.exists() else ""
    result["requiredChecklistInPrompt"] = (
        "Required field checklist" in prompt_text
        or "build_world_config_required_field_checklist" in prompt_text
    )
    result["extraKeyGuidanceInPrompt"] = "Extra keys are not allowed" in prompt_text
    result["repairGuidanceInPrompt"] = (
        "Missing required fields" in prompt_text
        and "Remove schema-extra fields" in prompt_text
    )
    if not result["requiredChecklistInPrompt"]:
        result["errors"].append("Prompt builder does not include required field checklist guidance.")
    if not result["extraKeyGuidanceInPrompt"]:
        result["errors"].append("Prompt builder does not include extra key prohibition guidance.")
    if not result["repairGuidanceInPrompt"]:
        result["errors"].append("Repair prompt does not include missing/extra field guidance.")

    if result["policyCardCount"] != EXPECTED_POLICY_CARD_COUNT:
        result["errors"].append(f"Policy card count must remain {EXPECTED_POLICY_CARD_COUNT}.")
    if result["ragChunkCount"] != EXPECTED_RAG_CHUNK_COUNT:
        result["errors"].append(f"Policy RAG chunk count must remain {EXPECTED_RAG_CHUNK_COUNT}.")

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
