from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.models.generation import (
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
)
from app.services.world_config_prompt_builder import build_world_config_prompt_package


ROOT = Path(__file__).resolve().parents[2]
CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
EXPECTED_RAG_CHUNK_COUNT = 17
REQUIRED_FILES = {
    "app/models/generation.py",
    "app/services/natural_language_normalizer.py",
    "app/services/world_config_rag_context_builder.py",
    "app/services/world_config_prompt_builder.py",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "world_config_prompt_builder",
        "passed": False,
        "warning": False,
        "missingFiles": [],
        "chunkCount": 0,
        "schemaReferenced": False,
        "promptPackageBuilds": False,
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for path in [
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "chroma",
    ]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
    if (ROOT / "samples").exists():
        found.append("samples/")
    if (ROOT / "fixtures").exists():
        found.append("fixtures/")
    names = {"world_config.json", "policy_config.json", "decision_request.json", "decision_response.json"}
    for folder in ["data", "docs", "harness", "scripts", "tests", "app"]:
        root = ROOT / folder
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.name in names:
                found.append(path.relative_to(ROOT).as_posix())
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["missingFiles"] = sorted(path for path in REQUIRED_FILES if not (ROOT / path).exists())
    if result["missingFiles"]:
        return result

    if not CHUNKS_PATH.exists():
        result["errors"].append("policy_rag_chunks.jsonl is missing.")
    else:
        chunks = [
            json.loads(line)
            for line in CHUNKS_PATH.read_text(encoding="utf-8-sig").splitlines()
            if line.strip()
        ]
        result["chunkCount"] = len(chunks)
        if len(chunks) != EXPECTED_RAG_CHUNK_COUNT:
            result["errors"].append(
                f"policy_rag_chunks.jsonl must contain {EXPECTED_RAG_CHUNK_COUNT} chunks."
            )

    builder_text = (ROOT / "app" / "services" / "world_config_prompt_builder.py").read_text(
        encoding="utf-8-sig"
    )
    result["schemaReferenced"] = "world_config.schema.json" in builder_text
    if not result["schemaReferenced"]:
        result["errors"].append("Prompt builder must reference world_config.schema.json.")

    request = WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="HARNESS-WORLD-PROMPT-001",
        generationType="world_config",
        prompt="비상정지가 필요한 좁은 보도 상황",
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=1,
            requireValidation=True,
        ),
        maxRepairAttempts=2,
    )
    package = build_world_config_prompt_package(request)
    result["promptPackageBuilds"] = bool(package.systemPrompt and package.userPrompt)
    if not result["promptPackageBuilds"]:
        result["errors"].append("Prompt package build failed.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding artifacts detected.")

    result["passed"] = not any([result["errors"], result["missingFiles"]])
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
