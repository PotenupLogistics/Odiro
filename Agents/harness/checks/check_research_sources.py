from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
RSR_001_PATH = ROOT / "data" / "sources" / "raw" / "research" / "RSR-001_METRANS_Sidewalk_ADR_Interactions.pdf"
EXPECTED_KOR_IDS = {"KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"}
EXPECTED_RSR_IDS = {"RSR-001", "RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"}
REQUIRED_FIELDS = {"sourceType", "accessType", "usagePurpose"}


def _base_result() -> dict[str, Any]:
    return {
        "check": "research_sources",
        "passed": False,
        "warning": False,
        "korSourceCount": 0,
        "rsrSourceCount": 0,
        "rsr001PdfExists": False,
        "urlOnlySources": [],
        "missingResearchSourceIds": [],
        "missingKorSourceIds": [],
        "missingRequiredFields": [],
        "missingUrls": [],
        "invalidStatuses": [],
        "missingFiles": [],
        "warnings": [],
        "errors": [],
    }


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not REGISTRY_PATH.exists():
        result["errors"].append("Registry file does not exist.")
        return result

    try:
        entries = json.loads(REGISTRY_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"Registry JSON parse failed: {exc}")
        return result

    sources = {entry.get("sourceId"): entry for entry in entries if isinstance(entry, dict)}
    kor_ids = {source_id for source_id in sources if str(source_id).startswith("KOR-")}
    rsr_ids = {source_id for source_id in sources if str(source_id).startswith("RSR-")}
    result["korSourceCount"] = len(kor_ids)
    result["rsrSourceCount"] = len(rsr_ids)
    result["missingKorSourceIds"] = sorted(EXPECTED_KOR_IDS - kor_ids)
    result["missingResearchSourceIds"] = sorted(EXPECTED_RSR_IDS - rsr_ids)

    for source_id in sorted(EXPECTED_RSR_IDS & rsr_ids):
        source = sources[source_id]
        missing = sorted(field for field in REQUIRED_FIELDS if field not in source)
        if missing:
            result["missingRequiredFields"].append({"sourceId": source_id, "fields": missing})

        if not source.get("url"):
            result["missingUrls"].append(source_id)
        if source.get("status") != "to_review":
            result["invalidStatuses"].append(
                {"sourceId": source_id, "status": source.get("status")}
            )

        file_path = source.get("filePath")
        if file_path:
            if not (ROOT / file_path).exists():
                result["missingFiles"].append({"sourceId": source_id, "filePath": file_path})
        else:
            result["urlOnlySources"].append(source_id)

    result["rsr001PdfExists"] = RSR_001_PATH.exists()
    if not result["rsr001PdfExists"]:
        result["warnings"].append("RSR-001 PDF is not downloaded.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingResearchSourceIds"],
            result["missingKorSourceIds"],
            result["missingRequiredFields"],
            result["missingUrls"],
            result["invalidStatuses"],
            result["missingFiles"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
