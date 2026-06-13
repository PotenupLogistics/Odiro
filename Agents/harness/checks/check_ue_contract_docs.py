from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = ROOT.parent
DOCS = ROOT / "docs"
CONTRACT_SPECS = REPO_ROOT / "contracts" / "specs"
README = ROOT / "README.md"
DOCS_README = DOCS / "README.md"

CONTRACT_DOCS = [
    CONTRACT_SPECS / "EpisodeSetup.json.md",
    CONTRACT_SPECS / "DeliveryBotSetup.json.md",
    CONTRACT_SPECS / "RunQueue.json.md",
    CONTRACT_SPECS / "EpisodeEvaluationReport.json.md",
]

PLAN_DOCS = [
    DOCS / "architecture" / "SCENARIO_EPISODE_TERMINOLOGY.md",
    DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md",
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
    episode_setup = _read(CONTRACT_SPECS / "EpisodeSetup.json.md")
    delivery_bot_setup = _read(CONTRACT_SPECS / "DeliveryBotSetup.json.md")
    run_queue = _read(CONTRACT_SPECS / "RunQueue.json.md")
    evaluation_report = _read(CONTRACT_SPECS / "EpisodeEvaluationReport.json.md")
    terminology = _read(DOCS / "architecture" / "SCENARIO_EPISODE_TERMINOLOGY.md")
    migration_plan = _read(DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md")
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    forbidden_artifacts = [
        path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()
    ]

    result: dict[str, Any] = {
        "check": "ue_contract_docs",
        "passed": False,
        "warning": False,
        "contractDocsExist": all(path.exists() for path in CONTRACT_DOCS),
        "planDocsExist": all(path.exists() for path in PLAN_DOCS),
        "episodeSetupHasCoreFields": all(
            term in episode_setup for term in ["xy_m", "yaw_deg", "goal_xy_m", "center_xy_m", "points_xy_m"]
        ),
        "episodeSetupForbidsLegacyFields": all(
            term in episode_setup for term in ["transform", "location_m", "rotation_deg", "scale", "units", "출력하지 않는다"]
        ),
        "episodeSetupOwnsPlacement": all(term in episode_setup for term in ["로봇 배치", "로봇 목적지"]),
        "deliveryBotSetupHasTuningFields": all(
            term in delivery_bot_setup for term in ["drive", "path_follow", "lidar"]
        ),
        "deliveryBotSetupForbidsPlacementFields": all(
            term in delivery_bot_setup
            for term in ["run", "actors", "instance_id", "asset_id", "route", "location", "넣지 않는다"]
        ),
        "runQueueHasPairTerms": all(
            term in run_queue for term in ["pair_id", "episode_setup", "delivery_bot_setup", "순서대로 실행"]
        ),
        "evaluationReportLinkedOnly": all(
            term in evaluation_report
            for term in ["episode_evaluation_report", "summary", "metrics", "event_summary", "events", "usable_for_llm_tuning"]
        )
        and "다른 담당자 범위" in migration_plan,
        "terminologyDistinguishesScenarioEpisode": all(
            term in terminology
            for term in ["Scenario", "추상적인 상황 유형", "Episode", "구체 시뮬레이션 인스턴스", "EpisodeSetup + DeliveryBotSetup", "scenario_id", "pair_id"]
        ),
        "migrationPlanDescribesPairMigration": all(
            term in migration_plan
            for term in [
                "WorldConfig -> EpisodeSetup",
                "DeliveryBotSetup",
                "RunQueue",
                "/api/v1/scenarios/generate",
                "legacy `/api/v1/ue5/world-config/handoff` route는 제거한다",
            ]
        ),
        "legacyDocsArchived": (DOCS / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_ADAPTER.md").exists(),
        "readmesLinkPlans": all(
            term in readme_text
            for term in ["UE_CONTRACT_MIGRATION_PLAN.md", "SCENARIO_EPISODE_TERMINOLOGY.md", "contracts/specs", "archive/previous_episode_spec"]
        ),
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden_artifacts,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("contractDocsExist", "Latest UE contract docs are missing."),
        ("planDocsExist", "UE contract terminology/migration/cleanup docs are missing."),
        ("episodeSetupHasCoreFields", "EpisodeSetup doc must include xy/yaw/goal/center/path fields."),
        ("episodeSetupForbidsLegacyFields", "EpisodeSetup doc must state legacy transform/unit fields are not output."),
        ("episodeSetupOwnsPlacement", "EpisodeSetup doc must own robot placement and destination."),
        ("deliveryBotSetupHasTuningFields", "DeliveryBotSetup doc must include drive/path_follow/lidar."),
        ("deliveryBotSetupForbidsPlacementFields", "DeliveryBotSetup doc must exclude placement/run/route fields."),
        ("runQueueHasPairTerms", "RunQueue doc must describe ordered EpisodeSetup/DeliveryBotSetup pairs."),
        ("evaluationReportLinkedOnly", "EvaluationReport doc must be present while analysis implementation remains out of scope."),
        ("terminologyDistinguishesScenarioEpisode", "Scenario/Episode terminology doc must distinguish core terms."),
        ("migrationPlanDescribesPairMigration", "Migration plan must describe EpisodeSetup/DeliveryBotSetup pair migration."),
        ("legacyDocsArchived", "Legacy EpisodeSpec docs must be archived."),
        ("readmesLinkPlans", "README docs must link UE contract migration, terminology, and cleanup docs."),
        ("noLiveProviderCallsInHarness", "UE contract docs harness must not perform live provider calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)

    if forbidden_artifacts:
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
