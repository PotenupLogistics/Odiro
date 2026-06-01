from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"

REQUIRED_DOCS = [
    DOCS / "UE_HANDOFF_DELIVERY_MANIFEST.md",
    DOCS / "HANDOFF_RELEASE_NOTES.md",
    DOCS / "HANDOFF_READINESS_CHECKLIST.md",
    DOCS / "HARNESS_WARNING_EXPLANATION.md",
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
    readme_text = (ROOT / "README.md").read_text(encoding="utf-8-sig")
    docs_readme_text = (DOCS / "README.md").read_text(encoding="utf-8-sig")
    manifest_text = (
        (DOCS / "UE_HANDOFF_DELIVERY_MANIFEST.md").read_text(encoding="utf-8-sig")
        if (DOCS / "UE_HANDOFF_DELIVERY_MANIFEST.md").exists()
        else ""
    )

    missing_docs = [path.relative_to(ROOT).as_posix() for path in REQUIRED_DOCS if not path.exists()]
    result: dict[str, Any] = {
        "check": "handoff_release_readiness",
        "passed": False,
        "warning": False,
        "missingDocs": missing_docs,
        "readmeLinksManifest": "UE_HANDOFF_DELIVERY_MANIFEST.md" in readme_text,
        "docsReadmeLinksReleaseNotes": "HANDOFF_RELEASE_NOTES.md" in docs_readme_text,
        "manifestMentionsEpisodeSpecResponse": "responseFormat=episode_spec" in manifest_text,
        "manifestMentionsKickboard": "obstacle.kickboard" in manifest_text,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if missing_docs:
        result["errors"].append("Required handoff release readiness documents are missing.")
    if not result["readmeLinksManifest"]:
        result["errors"].append("Root README.md does not link UE_HANDOFF_DELIVERY_MANIFEST.md.")
    if not result["docsReadmeLinksReleaseNotes"]:
        result["errors"].append("docs/README.md does not link HANDOFF_RELEASE_NOTES.md.")
    if not result["manifestMentionsEpisodeSpecResponse"]:
        result["errors"].append("Delivery manifest does not mention responseFormat=episode_spec.")
    if not result["manifestMentionsKickboard"]:
        result["errors"].append("Delivery manifest does not mention obstacle.kickboard.")

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
