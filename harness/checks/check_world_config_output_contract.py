from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
BUILDER_PATH = ROOT / "app" / "services" / "world_config_output_contract_builder.py"
PROMPT_BUILDER_PATH = ROOT / "app" / "services" / "world_config_prompt_builder.py"
DOC_PATH = ROOT / "docs" / "WORLD_CONFIG_OUTPUT_CONTRACT.md"


def _base_result() -> dict[str, Any]:
    return {
        "check": "world_config_output_contract",
        "passed": False,
        "warning": False,
        "builderExists": False,
        "docExists": False,
        "promptBuilderHasOutputContract": False,
        "requiredPathsPresent": False,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }


def _detect_openai_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [BUILDER_PATH, PROMPT_BUILDER_PATH]:
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
    result["builderExists"] = BUILDER_PATH.exists()
    result["docExists"] = DOC_PATH.exists()
    if not result["builderExists"]:
        result["errors"].append("app/services/world_config_output_contract_builder.py is missing.")
    if not result["docExists"]:
        result["errors"].append("docs/WORLD_CONFIG_OUTPUT_CONTRACT.md is missing.")

    prompt_text = PROMPT_BUILDER_PATH.read_text(encoding="utf-8-sig") if PROMPT_BUILDER_PATH.exists() else ""
    result["promptBuilderHasOutputContract"] = "Output Contract" in prompt_text or "build_world_config_output_contract" in prompt_text
    if not result["promptBuilderHasOutputContract"]:
        result["errors"].append("Prompt builder does not include Output Contract guidance.")

    builder_text = BUILDER_PATH.read_text(encoding="utf-8-sig") if BUILDER_PATH.exists() else ""
    required_terms = [
        "map.lengthCm",
        "map.sidewalkWidthCm",
        "robot.botId",
        "robot.spawn",
        "robot.goal",
        "runtime.maxDurationSec",
    ]
    result["requiredPathsPresent"] = all(term in builder_text or term in (DOC_PATH.read_text(encoding="utf-8-sig") if DOC_PATH.exists() else "") for term in required_terms)
    if not result["requiredPathsPresent"]:
        result["errors"].append("Output contract required path guidance is incomplete.")

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
