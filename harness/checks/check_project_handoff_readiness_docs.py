from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REQUIRED_DOCS = [
    ROOT / "docs" / "handoff" / "UE_INTEGRATION_HANDOFF_INDEX.md",
    ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md",
    ROOT / "docs" / "handoff" / "UE_AI_INTEGRATION_ISSUES.md",
]
MESSAGE_DOC = ROOT / "docs" / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md"
DOCS_README = ROOT / "docs" / "README.md"
ROOT_README = ROOT / "README.md"


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


def run_check() -> dict[str, Any]:
    missing_docs = [path.relative_to(ROOT).as_posix() for path in REQUIRED_DOCS if not path.exists()]
    message_text = MESSAGE_DOC.read_text(encoding="utf-8-sig") if MESSAGE_DOC.exists() else ""
    readme_exists = DOCS_README.exists() or ROOT_README.exists()
    result: dict[str, Any] = {
        "check": "project_handoff_readiness_docs",
        "passed": False,
        "warning": False,
        "missingDocs": missing_docs,
        "messageDocExists": MESSAGE_DOC.exists(),
        "messageMentionsEpisodeSpec": "responseFormat=episode_spec" in message_text,
        "messageMentionsKickboard": "obstacle.kickboard" in message_text,
        "readmeExists": readme_exists,
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if missing_docs:
        result["errors"].append("Project handoff readiness docs are missing.")
    if not result["messageDocExists"]:
        result["errors"].append("UE_TEAM_MESSAGE_DRAFT.md is missing.")
    if not result["messageMentionsEpisodeSpec"]:
        result["errors"].append("UE_TEAM_MESSAGE_DRAFT.md does not mention responseFormat=episode_spec.")
    if not result["messageMentionsKickboard"]:
        result["errors"].append("UE_TEAM_MESSAGE_DRAFT.md does not mention obstacle.kickboard.")
    if not readme_exists:
        result["errors"].append("README.md or docs/README.md is missing.")

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
