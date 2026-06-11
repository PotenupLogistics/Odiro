from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
REQUIRED_FIELDS = (
    "sourceId",
    "title",
    "filePath",
    "url",
    "sourceType",
    "accessType",
    "usagePurpose",
    "status",
    "notes",
)
VALID_STATUSES = {"to_review", "reviewed", "rejected"}
VALID_SOURCE_TYPES = {
    "law",
    "certification",
    "regulation",
    "guideline",
    "report",
    "paper",
    "standard",
    "internal",
}


def _base_result() -> dict[str, Any]:
    return {
        "registryPath": str(REGISTRY_PATH.relative_to(ROOT)).replace("\\", "/"),
        "registryExists": False,
        "jsonParsable": False,
        "sourceCount": 0,
        "passed": False,
        "duplicateSourceIds": [],
        "missingFiles": [],
        "missingRequiredFields": [],
        "invalidStatuses": [],
        "invalidSourceTypes": [],
        "invalidUsagePurposes": [],
        "errors": [],
    }


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not REGISTRY_PATH.exists():
        result["errors"].append("Registry file does not exist.")
        return result

    result["registryExists"] = True

    try:
        payload = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"Registry JSON parse failed: {exc}")
        return result

    result["jsonParsable"] = True

    if not isinstance(payload, list):
        result["errors"].append("Registry root must be a JSON list.")
        return result

    result["sourceCount"] = len(payload)

    seen_ids: set[str] = set()
    duplicate_ids: set[str] = set()

    for index, source in enumerate(payload):
        location = f"index {index}"

        if not isinstance(source, dict):
            result["errors"].append(f"{location}: source entry must be an object.")
            continue

        source_id = source.get("sourceId", f"<missing:{index}>")
        if source_id in seen_ids:
            duplicate_ids.add(str(source_id))
        else:
            seen_ids.add(str(source_id))

        missing_fields = [field for field in REQUIRED_FIELDS if field not in source]
        if missing_fields:
            result["missingRequiredFields"].append(
                {"sourceId": source.get("sourceId"), "fields": missing_fields}
            )

        file_path_value = source.get("filePath")
        if isinstance(file_path_value, str) and file_path_value:
            source_path = ROOT / file_path_value
            if not source_path.exists():
                result["missingFiles"].append(
                    {"sourceId": source.get("sourceId"), "filePath": file_path_value}
                )
        elif source.get("url"):
            pass
        else:
            result["missingFiles"].append(
                {"sourceId": source.get("sourceId"), "filePath": file_path_value}
            )

        status = source.get("status")
        if status not in VALID_STATUSES:
            result["invalidStatuses"].append(
                {"sourceId": source.get("sourceId"), "status": status}
            )

        source_type = source.get("sourceType")
        if source_type not in VALID_SOURCE_TYPES:
            result["invalidSourceTypes"].append(
                {"sourceId": source.get("sourceId"), "sourceType": source_type}
            )

        usage_purpose = source.get("usagePurpose")
        if not isinstance(usage_purpose, list) or not usage_purpose:
            result["invalidUsagePurposes"].append(
                {"sourceId": source.get("sourceId"), "usagePurpose": usage_purpose}
            )

    result["duplicateSourceIds"] = sorted(duplicate_ids)
    result["passed"] = not any(
        [
            result["errors"],
            result["duplicateSourceIds"],
            result["missingFiles"],
            result["missingRequiredFields"],
            result["invalidStatuses"],
            result["invalidSourceTypes"],
            result["invalidUsagePurposes"],
        ]
    )

    return result
