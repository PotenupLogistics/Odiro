from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = ROOT.parent
DOCS = ROOT / "docs"
README = ROOT / "README.md"
DOCS_README = DOCS / "README.md"

CONTRACT_SPECS = REPO_ROOT / "contracts" / "specs"
EPISODE_SETUP_CONTRACT = CONTRACT_SPECS / "EpisodeSetup.json.md"
DELIVERY_BOT_CONTRACT = CONTRACT_SPECS / "DeliveryBotSetup.json.md"
RUN_QUEUE_CONTRACT = CONTRACT_SPECS / "RunQueue.json.md"
EVALUATION_REPORT_CONTRACT = CONTRACT_SPECS / "EpisodeEvaluationReport.json.md"
POLICY_DECISION_CONTRACT = DOCS / "policy_server" / "POLICY_DECISION_JSON_GUIDE.md"

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
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    forbidden_code_paths = [
        path.relative_to(ROOT).as_posix()
        for path in FORBIDDEN_CODE_PATHS
        if path.exists()
    ]

    result: dict[str, Any] = {
        "check": "docs_inventory",
        "passed": False,
        "warning": False,
        "temporaryManagementDocsRemoved": not any(
            (DOCS / name).exists()
            for name in ["DOCS_INVENTORY.md", "KOREAN_DOCS_CONVERSION_PLAN.md", "DOCUMENT_CLEANUP_PLAN.md"]
        ),
        "expectedFoldersExist": all(
            (DOCS / path).exists()
            for path in [
                "archive/previous_episode_spec",
                "providers",
                "research",
                "references",
                "handoff",
                "architecture",
                "environment",
                "policy",
                "rag",
                "manual_review",
                "tooling",
                "json_contracts",
            ]
        ),
        "canonicalContractPathsExist": all(
            path.exists()
            for path in [
                EPISODE_SETUP_CONTRACT,
                DELIVERY_BOT_CONTRACT,
                RUN_QUEUE_CONTRACT,
                EVALUATION_REPORT_CONTRACT,
                POLICY_DECISION_CONTRACT,
            ]
        ),
        "legacyDocsArchived": all(
            (DOCS / "archive" / "previous_episode_spec" / name).exists()
            for name in [
                "UE5_EPISODE_SPEC_ADAPTER.md",
                "UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md",
                "UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md",
                "UE5_EPISODE_SPEC_SCENARIO_REFLECTION.md",
                "UE_EPISODE_SPEC_JSON_GUIDE.md",
                "UE5_WORLD_CONFIG_FIELD_MAPPING.md",
            ]
        ),
        "currentDocsMoved": all(
            path.exists()
            for path in [
                DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md",
                DOCS / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md",
                DOCS / "providers" / "OPENAI_PROVIDER_GUIDE.md",
                DOCS / "providers" / "OLLAMA_PROVIDER_GUIDE.md",
                DOCS / "research" / "RESEARCH_ALIGNMENT.md",
                DOCS / "architecture" / "SCENARIO_EPISODE_TERMINOLOGY.md",
                DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md",
                DOCS / "environment" / "ENVIRONMENT_PARAMETER_SPEC.md",
                DOCS / "policy" / "POLICY_SOURCE_REGISTRY.md",
                DOCS / "rag" / "RAG_RETRIEVAL_STRATEGY.md",
                DOCS / "tooling" / "HARNESS_GUIDE.md",
                DOCS / "json_contracts" / "JSON_CONTRACTS.md",
            ]
        ),
        "readmesLinkCanonicalContractPaths": all(
            term in readme_text
            for term in [
                "contracts/specs/EpisodeSetup.json.md",
                "contracts/specs/DeliveryBotSetup.json.md",
                "contracts/specs/RunQueue.json.md",
                "contracts/specs/EpisodeEvaluationReport.json.md",
                "docs/policy_server/POLICY_DECISION_JSON_GUIDE.md",
            ]
        ),
        "readmesLinkNewStructure": all(
            term in readme_text
            for term in [
                "docs/architecture/UE_CONTRACT_MIGRATION_PLAN.md",
                "docs/providers/OPENAI_PROVIDER_GUIDE.md",
                "archive/previous_episode_spec",
            ]
        ),
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "noUeCodeGenerated": not forbidden_code_paths,
        "forbiddenCodePaths": forbidden_code_paths,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("temporaryManagementDocsRemoved", "Temporary docs management files must be removed after cleanup."),
        ("expectedFoldersExist", "Expected docs structure folders are missing."),
        ("canonicalContractPathsExist", "Canonical UE or policy contract document path is missing."),
        ("legacyDocsArchived", "Legacy EpisodeSpec documents must be archived under docs/archive/previous_episode_spec."),
        ("currentDocsMoved", "Current docs must be in their semantic folders."),
        ("readmesLinkCanonicalContractPaths", "README docs must link canonical contract paths."),
        ("readmesLinkNewStructure", "README docs must link the final docs folder structure."),
        ("noLiveProviderCallsInHarness", "Docs inventory harness must not perform live provider calls."),
        ("noUeCodeGenerated", "Docs inventory work must not generate UE code directories."),
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
