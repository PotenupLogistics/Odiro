from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "README.md"


REQUIRED_PHRASES = [
    "# Proto-AI",
    "WorldConfig",
    "EpisodeSpec",
    "UE5 Handoff",
    "responseFormat=episode_spec",
    "uv run python -m harness.checks.check_all",
    "uv run pytest",
    "obstacle.kickboard",
    "obstacle.road_barrier_01",
    'semantic_type="Kickboard"',
]


def _detect_forbidden_artifacts() -> list[str]:
    forbidden_paths = [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]
    return sorted(path.relative_to(ROOT).as_posix() for path in forbidden_paths if path.exists())


def _detect_openai_imports() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    matches: list[str] = []
    for folder in [ROOT / "app", ROOT / "scripts"]:
        if not folder.exists():
            continue
        for file_path in folder.rglob("*.py"):
            text = file_path.read_text(encoding="utf-8-sig")
            for term in terms:
                if term in text:
                    matches.append(f"{file_path.relative_to(ROOT).as_posix()} contains {term}")
    return matches


def run_check() -> dict[str, Any]:
    text = README.read_text(encoding="utf-8-sig") if README.exists() else ""
    missing_phrases = [phrase for phrase in REQUIRED_PHRASES if phrase not in text]

    result: dict[str, Any] = {
        "check": "root_readme",
        "passed": False,
        "warning": False,
        "readmeExists": README.exists(),
        "readmeNotEmpty": bool(text.strip()),
        "missingPhrases": missing_phrases,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if not result["readmeExists"]:
        result["errors"].append("Root README.md is missing.")
    if not result["readmeNotEmpty"]:
        result["errors"].append("Root README.md is empty.")
    if missing_phrases:
        result["errors"].append("Root README.md is missing required project entrypoint content.")

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
