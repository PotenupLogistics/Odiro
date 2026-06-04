from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]

REQUIRED_FILES = [
    ROOT / "app" / "models" / "run_queue.py",
    ROOT / "app" / "services" / "run_queue_validator.py",
    ROOT / "app" / "services" / "episode_variation_generator.py",
    ROOT / "app" / "services" / "setup_pair_queue_generator.py",
    ROOT / "app" / "services" / "run_queue_export_service.py",
    ROOT / "app" / "utils" / "run_queue_summary.py",
    ROOT / "app" / "utils" / "json_sanitizer.py",
    ROOT / "app" / "services" / "delivery_bot_setup_defaults.py",
    ROOT / "app" / "services" / "delivery_bot_setup_variation_policy.py",
    ROOT / "scripts" / "export_ue5_run_queue_package.py",
]
TEST_FILES = [
    ROOT / "tests" / "test_run_queue_model_and_validator.py",
    ROOT / "tests" / "test_episode_variation_generator.py",
    ROOT / "tests" / "test_setup_pair_queue_generator.py",
    ROOT / "tests" / "test_run_queue_export_service.py",
    ROOT / "tests" / "test_export_ue5_run_queue_package_tooling.py",
    ROOT / "tests" / "test_json_sanitizer.py",
    ROOT / "tests" / "test_delivery_bot_setup_variation_policy.py",
    ROOT / "tests" / "test_delivery_bot_setup_model_and_validator.py",
]
FORBIDDEN_ARTIFACTS = [
    ROOT / "samples",
    ROOT / "fixtures",
    ROOT / "ue",
    ROOT / "UE",
]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _imports_live_http_client(path: Path) -> bool:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name in {"urllib", "urllib.request", "requests", "httpx", "openai", "ollama"} for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module in {"urllib", "urllib.request", "requests", "httpx", "openai", "ollama"}:
            return True
    return False


def run_check() -> dict[str, Any]:
    gitignore_text = _read(ROOT / ".gitignore")
    model_text = _read(ROOT / "app" / "models" / "run_queue.py")
    validator_text = _read(ROOT / "app" / "services" / "run_queue_validator.py")
    script_text = _read(ROOT / "scripts" / "export_ue5_run_queue_package.py")
    sanitizer_text = _read(ROOT / "app" / "utils" / "json_sanitizer.py")
    defaults_text = _read(ROOT / "app" / "services" / "delivery_bot_setup_defaults.py")
    variation_text = _read(ROOT / "app" / "services" / "delivery_bot_setup_variation_policy.py")
    export_service_text = _read(ROOT / "app" / "services" / "run_queue_export_service.py")
    tests_text = "\n".join(_read(path) for path in TEST_FILES)
    forbidden_artifacts = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]

    result: dict[str, Any] = {
        "check": "run_queue_export",
        "passed": False,
        "warning": False,
        "requiredFilesExist": all(path.exists() for path in REQUIRED_FILES),
        "gitignoreIncludesExportDir": "data/run_queue_exports/" in gitignore_text,
        "runQueueModelUsesContractFieldsOnly": all(term in model_text for term in ["EpisodeRunQueue", "EpisodeRunQueueItem", "extra=\"forbid\""]),
        "validatorRejectsWrapperFields": all(term in validator_text for term in ["success", "diagnostics", "setupPairs", "forbidden_root_field"]),
        "testsCoverWrapperFields": all(term in tests_text for term in ["success", "diagnostics", "setupPairs", "Json/Input/"]),
        "nullFreeSanitizerExists": "remove_json_nulls" in sanitizer_text and "contains_json_null" in sanitizer_text,
        "deliveryBotDefaultsCatalogExists": all(term in defaults_text for term in ["DELIVERY_BOT_SETUP_DEFAULTS", "speed_limit_brake", "ignore_tags"]),
        "deliveryBotVariationPolicyExists": all(term in variation_text for term in ["delivery_bot_tuning_for_episode", "conservative_lidar", "slower_path_follow"]),
        "testsCoverPolicyComparison": all(term in tests_text for term in ["EpisodeSetup_narrow_sidewalk_fixed_center_block", "DeliveryBotSetup_policy_004_slower_path_follow", "same field sets"]) or "share_identical_field_sets" in tests_text,
        "exportBacksUpExistingTarget": "_backup" in export_service_text and "backup_summary.json" in export_service_text,
        "testsCoverNullFreeExport": "null" in tests_text and "explicit_null_field" in tests_text,
        "scriptSupportsRequestJsonAndDryRun": "--request-json" in script_text and "--dry-run" in script_text,
        "noLiveProviderCallsInRunQueueTooling": not any(_imports_live_http_client(path) for path in [REQUIRED_FILES[-1], Path(__file__)]),
        "forbiddenArtifacts": forbidden_artifacts,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("requiredFilesExist", "RunQueue export model/service/script files are missing."),
        ("gitignoreIncludesExportDir", ".gitignore must include data/run_queue_exports/."),
        ("runQueueModelUsesContractFieldsOnly", "RunQueue model must use UE contract fields only."),
        ("validatorRejectsWrapperFields", "RunQueue validator must reject wrapper fields."),
        ("testsCoverWrapperFields", "RunQueue tests must cover wrapper field rejection and UE paths."),
        ("nullFreeSanitizerExists", "Null-free sanitizer must exist."),
        ("deliveryBotDefaultsCatalogExists", "DeliveryBotSetup default catalog must exist."),
        ("deliveryBotVariationPolicyExists", "DeliveryBotSetup variation policy must exist."),
        ("testsCoverPolicyComparison", "Tests must cover fixed EpisodeSetup policy comparison RunQueue output."),
        ("exportBacksUpExistingTarget", "RunQueue export service must backup existing targets."),
        ("testsCoverNullFreeExport", "Tests must cover null-free export and explicit null rejection."),
        ("scriptSupportsRequestJsonAndDryRun", "RunQueue export script must support --request-json and --dry-run."),
        ("noLiveProviderCallsInRunQueueTooling", "RunQueue export tooling must not perform live provider calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    if forbidden_artifacts:
        result["warnings"].append("Forbidden sample/fixture/UE artifacts detected.")
    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
