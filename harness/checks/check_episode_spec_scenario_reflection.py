from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SERVICE_PATH = ROOT / "app" / "services" / "episode_spec_scenario_reflection.py"
DOC_PATH = ROOT / "docs" / "UE5_EPISODE_SPEC_SCENARIO_REFLECTION.md"
SCRIPT_PATH = ROOT / "scripts" / "run_ue5_episode_spec_controlled_smoke.py"
REPORT_PATH = ROOT / "harness" / "reports" / "ue5_episode_spec_controlled_scenario_smoke.json"


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
        "check": "episode_spec_scenario_reflection",
        "passed": False,
        "warning": False,
        "serviceExists": SERVICE_PATH.exists(),
        "docExists": DOC_PATH.exists(),
        "scriptExists": SCRIPT_PATH.exists(),
        "controlledSmokeReportExists": REPORT_PATH.exists(),
        "episodeScenarioReflectionPassedRecorded": False,
        "hasKickboardSemanticRecorded": False,
        "hasBlockingRatioRecorded": False,
        "hasCrossingPedestrianRecorded": False,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if not result["serviceExists"]:
        result["errors"].append("app/services/episode_spec_scenario_reflection.py is missing.")
    if not result["docExists"]:
        result["errors"].append("docs/UE5_EPISODE_SPEC_SCENARIO_REFLECTION.md is missing.")
    if not result["scriptExists"]:
        result["errors"].append("scripts/run_ue5_episode_spec_controlled_smoke.py is missing.")
    if not result["controlledSmokeReportExists"]:
        result["errors"].append("controlled scenario smoke report is missing.")

    if REPORT_PATH.exists():
        payload = json.loads(REPORT_PATH.read_text(encoding="utf-8-sig"))
        result["episodeScenarioReflectionPassedRecorded"] = "episodeScenarioReflectionPassed" in payload
        result["hasKickboardSemanticRecorded"] = "hasKickboardSemantic" in payload
        result["hasBlockingRatioRecorded"] = "hasBlockingRatio" in payload
        result["hasCrossingPedestrianRecorded"] = "hasCrossingPedestrian" in payload
        for key, message in [
            ("episodeScenarioReflectionPassedRecorded", "report does not record episodeScenarioReflectionPassed."),
            ("hasKickboardSemanticRecorded", "report does not record hasKickboardSemantic."),
            ("hasBlockingRatioRecorded", "report does not record hasBlockingRatio."),
            ("hasCrossingPedestrianRecorded", "report does not record hasCrossingPedestrian."),
        ]:
            if not result[key]:
                result["errors"].append(message)

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

