from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
PROCESSED_DIR = ROOT / "data" / "sources" / "processed" / "korea"
PROCESSING_REPORT_PATH = ROOT / "data" / "sources" / "processed" / "source_processing_report.json"
VALID_EXTRACTION_STATUSES = {"success", "partial", "failed", "needs_manual_review"}
EXPECTED_SOURCE_IDS = {"KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"}


def _base_result() -> dict[str, Any]:
    return {
        "check": "processed_sources",
        "passed": False,
        "warning": False,
        "sourceCount": 0,
        "missingProcessedFiles": [],
        "missingMetadataSections": [],
        "missingSourceIdsInContent": [],
        "missingExtractionStatuses": [],
        "invalidExtractionStatuses": [],
        "manualReviewSources": [],
        "processingReportExists": False,
        "errors": [],
    }


def _processed_path_for(file_path: str) -> Path:
    return PROCESSED_DIR / f"{Path(file_path).stem}.md"


def _extract_status(content: str) -> str | None:
    match = re.search(r"^extractionStatus:\s*(\S+)\s*$", content, flags=re.MULTILINE)
    if match:
        return match.group(1)
    return None


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not REGISTRY_PATH.exists():
        result["errors"].append("Registry file does not exist.")
        return result

    try:
        registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"Registry JSON parse failed: {exc}")
        return result

    sources = [source for source in registry if source.get("sourceId") in EXPECTED_SOURCE_IDS]
    result["sourceCount"] = len(sources)

    for source in sources:
        source_id = source["sourceId"]
        processed_path = _processed_path_for(source["filePath"])

        if not processed_path.exists():
            result["missingProcessedFiles"].append(
                {
                    "sourceId": source_id,
                    "processedFilePath": processed_path.relative_to(ROOT).as_posix(),
                }
            )
            continue

        content = processed_path.read_text(encoding="utf-8-sig")
        if "## 1. Source Metadata" not in content:
            result["missingMetadataSections"].append(source_id)
        if source_id not in content:
            result["missingSourceIdsInContent"].append(source_id)

        status = _extract_status(content)
        if status is None:
            result["missingExtractionStatuses"].append(source_id)
        elif status not in VALID_EXTRACTION_STATUSES:
            result["invalidExtractionStatuses"].append(
                {"sourceId": source_id, "extractionStatus": status}
            )
        elif status in {"failed", "needs_manual_review", "partial"}:
            result["manualReviewSources"].append(
                {"sourceId": source_id, "extractionStatus": status}
            )

    result["processingReportExists"] = PROCESSING_REPORT_PATH.exists()
    if not result["processingReportExists"]:
        result["errors"].append("source_processing_report.json does not exist.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingProcessedFiles"],
            result["missingMetadataSections"],
            result["missingSourceIdsInContent"],
            result["missingExtractionStatuses"],
            result["invalidExtractionStatuses"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["manualReviewSources"])

    return result
