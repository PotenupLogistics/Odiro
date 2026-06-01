from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REPORT_JSON = ROOT / "harness" / "reports" / "ue5_episode_spec_handoff_smoke.json"
REPORT_MD = ROOT / "harness" / "reports" / "ue5_episode_spec_handoff_smoke.md"
SUMMARY_DOC = ROOT / "docs" / "UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md"


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
    result: dict[str, Any] = {
        "check": "ue5_episode_spec_handoff_smoke",
        "passed": False,
        "warning": False,
        "reportJsonExists": REPORT_JSON.exists(),
        "reportMarkdownExists": REPORT_MD.exists(),
        "summaryDocExists": SUMMARY_DOC.exists(),
        "episodeSpecExistsRecorded": False,
        "conversionWarningsRecorded": False,
        "kickboardMappingRecorded": False,
        "forbiddenArtifacts": [],
        "openAiImports": [],
        "errors": [],
        "warnings": [],
    }

    if not result["reportJsonExists"]:
        result["errors"].append("harness/reports/ue5_episode_spec_handoff_smoke.json is missing.")
    if not result["reportMarkdownExists"]:
        result["errors"].append("harness/reports/ue5_episode_spec_handoff_smoke.md is missing.")
    if not result["summaryDocExists"]:
        result["errors"].append("docs/UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md is missing.")

    if REPORT_JSON.exists():
        payload = json.loads(REPORT_JSON.read_text(encoding="utf-8-sig"))
        episode_result = payload.get("responseFormatEpisodeSpec", {})
        result["episodeSpecExistsRecorded"] = "episodeSpecExists" in episode_result
        result["conversionWarningsRecorded"] = "conversionWarnings" in episode_result
        result["kickboardMappingRecorded"] = "kickboardMapping" in episode_result
        if not result["episodeSpecExistsRecorded"]:
            result["errors"].append("smoke report does not record episodeSpecExists.")
        if not result["conversionWarningsRecorded"]:
            result["errors"].append("smoke report does not record conversionWarnings.")
        if not result["kickboardMappingRecorded"]:
            result["errors"].append("smoke report does not record kickboardMapping.")

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
