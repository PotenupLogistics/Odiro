from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DOCS = [
    ROOT / "docs" / "archive" / "previous_episode_spec" / "UE_TEAM_HANDOFF_PACKAGE.md",
    ROOT / "docs" / "handoff" / "UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md",
    ROOT / "docs" / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md",
    ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md",
]


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


def _detect_openai_imports() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [ROOT / "app", ROOT / "scripts"]:
        if not path.exists():
            continue
        for file_path in path.rglob("*.py"):
            text = file_path.read_text(encoding="utf-8-sig")
            for term in terms:
                if term in text:
                    found.append(f"{file_path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def run_check() -> dict[str, Any]:
    missing_docs = [path.relative_to(ROOT).as_posix() for path in DOCS if not path.exists()]
    combined = "\n".join(path.read_text(encoding="utf-8-sig") for path in DOCS if path.exists())
    required_terms = [
        "responseFormat=episode_spec",
        "obstacle.kickboard",
        "obstacle.road_barrier_01",
        "semantic_type",
        "blocking_ratio",
        "semantic_behavior",
        "pedestrian_crossing",
    ]
    missing_terms = [term for term in required_terms if term not in combined]
    result: dict[str, Any] = {
        "check": "ue_team_handoff_package",
        "passed": False,
        "warning": False,
        "missingDocs": missing_docs,
        "missingTerms": missing_terms,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if missing_docs:
        result["errors"].append("UE team handoff package docs are missing.")
    if missing_terms:
        result["errors"].append("UE team handoff package docs are missing required terms.")

    result["openAiImports"] = _detect_openai_imports()
    if result["openAiImports"]:
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

