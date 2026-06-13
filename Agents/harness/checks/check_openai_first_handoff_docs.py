from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CONTRACT_SCHEMA_DIR = ROOT.parent / "contracts" / "schemas"
RESULT_DOC = ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md"
ROOT_README = ROOT / "README.md"
DOCS_README = ROOT / "docs" / "README.md"

RELATED_DOCS = [
    ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md",
    ROOT / "docs" / "handoff" / "UE_HANDOFF_DELIVERY_MANIFEST.md",
    ROOT / "docs" / "archive" / "previous_episode_spec" / "UE_TEAM_HANDOFF_PACKAGE.md",
    ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md",
    ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md",
    ROOT / "docs" / "providers" / "OPENAI_PROVIDER_GUIDE.md",
    ROOT / "docs" / "providers" / "LLM_PROVIDER_CONFIGURATION.md",
    ROOT / "docs" / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md",
]

FORBIDDEN_ARTIFACTS = [
    ROOT / "samples",
    ROOT / "fixtures",
    ROOT / "data" / "rag" / "vector_db",
    ROOT / "data" / "rag" / "embeddings",
    ROOT / "data" / "rag" / "chroma",
    ROOT / "ue",
    ROOT / "UE",
]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _detect_hardcoded_keys() -> list[str]:
    found: list[str] = []
    for root in [ROOT / "app", ROOT / "scripts", ROOT / "docs"]:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.suffix.lower() not in {".py", ".md"}:
                continue
            text = _read(path)
            for term in ["sk-", "api_key=\"", "api_key='"]:
                if term in text:
                    found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def run_check() -> dict[str, Any]:
    result_doc_text = _read(RESULT_DOC)
    root_readme_text = _read(ROOT_README)
    docs_readme_text = _read(DOCS_README)
    related_text = "\n".join(_read(path) for path in RELATED_DOCS)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]

    result: dict[str, Any] = {
        "check": "openai_first_handoff_docs",
        "passed": False,
        "warning": False,
        "resultDocExists": RESULT_DOC.exists(),
        "rootReadmeLinksResult": "HANDOFF_RELEASE_NOTES.md" in root_readme_text,
        "docsReadmeLinksResult": "HANDOFF_RELEASE_NOTES.md" in docs_readme_text,
        "mentionsOpenAiFirst": "OpenAI-first" in related_text,
        "mentionsOllamaFallback": "Ollama fallback" in related_text,
        "recordsProviderUsedOpenAi": "providerUsed=openai" in related_text,
        "recordsFallbackUsedFalse": "fallbackUsed=false" in related_text,
        "mentionsApiKeyNotStored": "API key" in related_text and "저장하지" in related_text,
        "mentionsFullPayloadNotStored": "full WorldConfig" in related_text and "full EpisodeSpec" in related_text,
        "hardcodedKeyWarnings": _detect_hardcoded_keys(),
        "forbiddenArtifacts": forbidden,
        "schemaFilesPresent": (CONTRACT_SCHEMA_DIR / "world_config.schema.json").exists(),
        "errors": [],
        "warnings": [],
    }

    required_truths = [
        ("resultDocExists", "docs/handoff/HANDOFF_RELEASE_NOTES.md is missing."),
        ("rootReadmeLinksResult", "README.md must link docs/handoff/HANDOFF_RELEASE_NOTES.md."),
        ("docsReadmeLinksResult", "docs/README.md must link HANDOFF_RELEASE_NOTES.md."),
        ("mentionsOpenAiFirst", "Related docs must mention OpenAI-first."),
        ("mentionsOllamaFallback", "Related docs must mention Ollama fallback."),
        ("recordsProviderUsedOpenAi", "Related docs must record providerUsed=openai."),
        ("recordsFallbackUsedFalse", "Related docs must record fallbackUsed=false."),
        ("mentionsApiKeyNotStored", "Related docs must state API key is not stored."),
        ("mentionsFullPayloadNotStored", "Related docs must state full payloads are not stored."),
        ("schemaFilesPresent", "world_config JSON Schema file is missing."),
    ]
    for key, message in required_truths:
        if not result[key]:
            result["errors"].append(message)
    if result["hardcodedKeyWarnings"]:
        result["errors"].append("Potential hardcoded API key detected.")
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
