from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
MODEL_PATH = ROOT / "app" / "models" / "episode_spec.py"
ADAPTER_PATH = ROOT / "app" / "services" / "world_config_to_episode_spec_adapter.py"
VALIDATOR_PATH = ROOT / "app" / "services" / "episode_spec_validator.py"
HANDOFF_MODEL_PATH = ROOT / "app" / "models" / "handoff.py"
HANDOFF_SERVICE_PATH = ROOT / "app" / "services" / "ue5_world_config_handoff_service.py"
ROUTES_PATH = ROOT / "app" / "api" / "routes.py"
EXPORT_SCRIPT_PATH = ROOT / "scripts" / "export_ue5_handoff_payload.py"
DOC_PATH = ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_ADAPTER.md"
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
RAG_CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
EXPECTED_POLICY_CARD_COUNT = 9
EXPECTED_RAG_CHUNK_COUNT = 15
SCHEMA_PATH = ROOT / "schemas" / "world_config.schema.json"


def _jsonl_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip())


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
    return sorted(found)


def _detect_forbidden_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [MODEL_PATH, ADAPTER_PATH, VALIDATOR_PATH, HANDOFF_MODEL_PATH, HANDOFF_SERVICE_PATH, ROUTES_PATH, EXPORT_SCRIPT_PATH]:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def _export_help_has_format() -> bool:
    if not EXPORT_SCRIPT_PATH.exists():
        return False
    completed = subprocess.run(
        [sys.executable, str(EXPORT_SCRIPT_PATH), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    return completed.returncode == 0 and "--format" in completed.stdout


def run_check() -> dict[str, Any]:
    result: dict[str, Any] = {
        "check": "episode_spec_adapter",
        "passed": False,
        "warning": False,
        "modelExists": MODEL_PATH.exists(),
        "adapterExists": ADAPTER_PATH.exists(),
        "validatorExists": VALIDATOR_PATH.exists(),
        "docExists": DOC_PATH.exists(),
        "exportFormatOptionExists": _export_help_has_format(),
        "responseFormatCodeExists": False,
        "policyCardCount": _jsonl_count(POLICY_CARDS_PATH),
        "ragChunkCount": _jsonl_count(RAG_CHUNKS_PATH),
        "worldConfigSchemaModifiedCheck": SCHEMA_PATH.exists(),
        "forbiddenCode": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("modelExists", "app/models/episode_spec.py is missing."),
        ("adapterExists", "app/services/world_config_to_episode_spec_adapter.py is missing."),
        ("validatorExists", "app/services/episode_spec_validator.py is missing."),
        ("docExists", "docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_ADAPTER.md is missing."),
        ("exportFormatOptionExists", "export CLI --format option is missing."),
    ]:
        if not result[key]:
            result["errors"].append(message)

    combined_text = ""
    for path in [HANDOFF_MODEL_PATH, HANDOFF_SERVICE_PATH, ROUTES_PATH]:
        if path.exists():
            combined_text += path.read_text(encoding="utf-8-sig")
    result["responseFormatCodeExists"] = "responseFormat" in combined_text and "episode_spec" in combined_text
    if not result["responseFormatCodeExists"]:
        result["errors"].append("handoff responseFormat episode_spec support is missing.")

    if result["policyCardCount"] != EXPECTED_POLICY_CARD_COUNT:
        result["errors"].append(f"Policy card count changed from {EXPECTED_POLICY_CARD_COUNT}.")
    if result["ragChunkCount"] != EXPECTED_RAG_CHUNK_COUNT:
        result["errors"].append(f"Policy RAG chunk count changed from {EXPECTED_RAG_CHUNK_COUNT}.")

    result["forbiddenCode"] = _detect_forbidden_code()
    if result["forbiddenCode"]:
        result["errors"].append("OpenAI SDK import or call code detected.")

    result["forbiddenArtifacts"] = _detect_forbidden_artifacts()
    if result["forbiddenArtifacts"]:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE code artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
