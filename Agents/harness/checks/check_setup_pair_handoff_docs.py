from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
README = ROOT / "README.md"
DOCS_README = DOCS / "README.md"
GITIGNORE = ROOT / ".gitignore"

HANDOFF_RESULT = DOCS / "handoff" / "HANDOFF_RELEASE_NOTES.md"
HANDOFF_PACKAGE = DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md"
UE_TEAM_MESSAGE = DOCS / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md"

FORBIDDEN_CODE_PATHS = [
    ROOT / "ue",
    ROOT / "UE",
]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _imports_live_http_client(path: Path) -> bool:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name in {"urllib", "urllib.request", "requests", "httpx"} for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module in {"urllib", "urllib.request", "requests", "httpx"}:
            return True
    return False


def run_check() -> dict[str, Any]:
    result_text = _read(HANDOFF_RESULT)
    package_text = _read(HANDOFF_PACKAGE)
    message_text = _read(UE_TEAM_MESSAGE)
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    gitignore_text = _read(GITIGNORE)
    forbidden_code_paths = [
        path.relative_to(ROOT).as_posix()
        for path in FORBIDDEN_CODE_PATHS
        if path.exists()
    ]

    result: dict[str, Any] = {
        "check": "setup_pair_handoff_docs",
        "passed": False,
        "warning": False,
        "handoffResultExists": HANDOFF_RESULT.exists(),
        "handoffPackageExists": HANDOFF_PACKAGE.exists(),
        "ueTeamMessageMentionsScenarioGenerate": "/api/v1/scenarios/generate" in message_text,
        "ueTeamMessageMentionsSetupPair": "EpisodeSetup + DeliveryBotSetup pair" in message_text,
        "readmeLinksHandoffResult": "HANDOFF_RELEASE_NOTES.md" in readme_text,
        "gitignoreIncludesFineTuningCandidates": "data/fine_tuning_candidates/" in gitignore_text,
        "resultDocumentsSetupPair": all(
            term in result_text
            for term in [
                "setup pair",
                "EpisodeSetup + DeliveryBotSetup pair",
                "episodeSetupValidationPassed",
                "deliveryBotSetupValidationPassed",
                "setupPairTraceExists=true",
            ]
        ),
        "packageDocumentsLocalCandidatePolicy": all(
            term in package_text
            for term in [
                "data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001",
                "git commit 대상이 아님",
                "EpisodeSetup JSON",
                "DeliveryBotSetup JSON",
            ]
        ),
        "packageDocumentsUeChecklist": all(
            term in package_text
            for term in [
                "EpisodeSetup JSON이 UE compiler",
                "FDeliveryBotSetupInfo",
                "robot",
                "obstacle",
                "stop_distance_m=1.2",
                "slow_down_distance_m=3.5",
            ]
        ),
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "noUeCodeGenerated": not forbidden_code_paths,
        "forbiddenCodePaths": forbidden_code_paths,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("handoffResultExists", "Handoff release notes document is missing."),
        ("handoffPackageExists", "UE setup pair handoff package document is missing."),
        ("ueTeamMessageMentionsScenarioGenerate", "UE team message draft must mention /api/v1/scenarios/generate."),
        ("ueTeamMessageMentionsSetupPair", "UE team message draft must mention setup pair delivery scope."),
        ("readmeLinksHandoffResult", "README or docs README must link HANDOFF_RELEASE_NOTES."),
        ("gitignoreIncludesFineTuningCandidates", ".gitignore must include data/fine_tuning_candidates/."),
        ("resultDocumentsSetupPair", "Setup pair handoff result must document the verified setup_pair smoke."),
        ("packageDocumentsLocalCandidatePolicy", "UE package must document local candidate path and no-commit policy."),
        ("packageDocumentsUeChecklist", "UE package must document UE compile/spawn/tuning checklist."),
        ("noLiveProviderCallsInHarness", "Setup pair handoff docs harness must not perform live provider calls."),
        ("noUeCodeGenerated", "Setup pair handoff docs work must not generate UE code directories."),
    ]:
        if not result[key]:
            result["errors"].append(message)

    result["passed"] = not result["errors"]
    result["warning"] = False
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
