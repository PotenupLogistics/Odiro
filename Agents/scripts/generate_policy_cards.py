from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANUAL_CONFIRMATION_PATH = (
    ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
)
DEFAULT_REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
DEFAULT_OUTPUT_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
DEFAULT_REPORT_JSON_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards_report.json"
DEFAULT_REPORT_MD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards_report.md"

REQUIRED_CAUTION = (
    "이 카드는 원본 문서의 수동 확인 내용을 바탕으로 만든 프로젝트 내부 정책 기준이며, "
    "공식 인증 준수를 의미하지 않는다."
)

RELATED_MAPPING: dict[str, dict[str, list[str]]] = {
    "speed_policy": {
        "relatedPolicyParams": ["maxSpeedKmh", "lowSpeedZoneSpeedKmh"],
        "relatedRequestFields": ["botState.speedKmh", "environments[].type"],
        "relatedActions": ["SlowDown", "Stop"],
        "relatedMetrics": ["deliveryTimeSec", "nearMissCount"],
    },
    "emergency_stop": {
        "relatedPolicyParams": ["emergencyStopDistanceCm", "ttcThresholdSec"],
        "relatedRequestFields": [
            "detectedObjects[].distanceCm",
            "detectedObjects[].timeToCollisionSec",
            "event.severity",
        ],
        "relatedActions": ["EmergencyStop", "Stop"],
        "relatedMetrics": ["collisionCount", "nearMissCount", "minPedestrianDistanceCm"],
    },
    "perception_requirement": {
        "relatedPolicyParams": ["safeDistanceCm", "ttcThresholdSec", "perceptionMinRangeM"],
        "relatedRequestFields": ["detectedObjects[]", "event.confidence", "event.source"],
        "relatedActions": ["SlowDown", "Stop", "LocalAvoidance", "ReplanPath"],
        "relatedMetrics": ["nearMissCount", "collisionCount", "rerouteCount"],
    },
    "operator_control": {
        "relatedPolicyParams": ["waitTimeoutSec", "maxRerouteAttempts", "operatorOverrideEnabled"],
        "relatedRequestFields": ["event.type", "botState", "pathContext", "communicationStatus"],
        "relatedActions": ["RequestOperator", "Stop", "EmergencyStop"],
        "relatedMetrics": ["manualOverrideRequired", "stopCount"],
    },
    "sidewalk_operation": {
        "relatedPolicyParams": ["maxSpeedKmh", "safeDistanceCm", "lowSpeedZoneSpeedKmh"],
        "relatedRequestFields": [
            "environments[].type",
            "environments[].state",
            "detectedObjects[].isOnPath",
        ],
        "relatedActions": ["SlowDown", "YieldWait", "Stop", "Continue"],
        "relatedMetrics": ["sidewalkDepartureCount", "nearMissCount", "stopCount"],
    },
    "terrain_or_dynamic_safety": {
        "relatedPolicyParams": [
            "traversabilityThreshold",
            "rollPitchThresholdDeg",
            "lowSpeedZoneSpeedKmh",
            "botMassKg",
        ],
        "relatedRequestFields": [
            "terrain",
            "botState.pitchDegree",
            "botState.rollDegree",
            "botState.speedKmh",
        ],
        "relatedActions": ["SlowDown", "Stop", "ReplanPath", "RequestOperator"],
        "relatedMetrics": ["fallDetected", "sidewalkDepartureCount", "collisionCount"],
    },
}

SPECIAL_OVERRIDES: dict[str, dict[str, list[str]]] = {
    "CAND-KOR-003-050": {
        "relatedPolicyParams": ["botMassKg", "robotWidthCm", "sidewalkWidthCm"],
        "relatedRequestFields": ["botState.massKg", "botState.widthCm", "environments[].widthCm"],
        "relatedActions": ["Stop", "ReplanPath", "RequestOperator"],
        "relatedMetrics": ["sidewalkDepartureCount", "collisionCount"],
    },
    "CAND-KOR-003-059": {
        "relatedRequestFields": ["environments[].type", "environments[].state"],
        "relatedActions": ["Stop", "YieldWait", "Continue"],
        "relatedMetrics": ["stopCount", "deliveryTimeSec", "nearMissCount"],
    },
    "CAND-KOR-003-062": {
        "relatedActions": ["RequestOperator", "EmergencyStop", "Stop"],
        "relatedRequestFields": ["event.type", "botState", "communicationStatus"],
        "relatedMetrics": ["manualOverrideRequired", "stopCount"],
    },
}


def _read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _as_registry_map(payload: Any) -> dict[str, dict[str, Any]]:
    sources = payload.get("sources") if isinstance(payload, dict) else payload
    if not isinstance(sources, list):
        raise ValueError("registry must be a list or contain a sources list")
    return {
        source["sourceId"]: source
        for source in sources
        if isinstance(source, dict) and isinstance(source.get("sourceId"), str)
    }


def _manual_items(payload: Any) -> list[dict[str, Any]]:
    items = payload.get("items") if isinstance(payload, dict) else None
    if not isinstance(items, list):
        raise ValueError("manual confirmation file must contain an items list")
    return [item for item in items if isinstance(item, dict)]


def _is_filled(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _validate_confirmed_item(item: dict[str, Any], registry: dict[str, dict[str, Any]]) -> None:
    required = [
        "candidateId",
        "sourceId",
        "category",
        "confirmedText",
        "reviewer",
        "reviewedAt",
        "decisionReason",
        "nextAction",
    ]
    missing = [field for field in required if not _is_filled(item.get(field))]
    if not (_is_filled(item.get("rawPdfPage")) or _is_filled(item.get("rawPdfSection"))):
        missing.append("rawPdfPage_or_rawPdfSection")
    if missing:
        raise ValueError(f"{item.get('candidateId', '<unknown>')} missing required fields: {missing}")
    if item["sourceId"] not in registry:
        raise ValueError(f"{item['candidateId']} sourceId not found in registry: {item['sourceId']}")


def _evidence_location(item: dict[str, Any]) -> str:
    parts = [
        str(item.get("rawPdfPage", "")).strip(),
        str(item.get("rawPdfSection", "")).strip(),
    ]
    return " / ".join(part for part in parts if part)


def _short_text(text: str, limit: int = 140) -> str:
    normalized = " ".join(text.split())
    if len(normalized) <= limit:
        return normalized
    return normalized[:limit].rstrip() + "..."


def _merge_related(category: str, candidate_id: str) -> dict[str, list[str]]:
    related = {key: list(value) for key, value in RELATED_MAPPING.get(category, {}).items()}
    override = SPECIAL_OVERRIDES.get(candidate_id, {})
    for key, values in override.items():
        merged = list(dict.fromkeys([*related.get(key, []), *values]))
        related[key] = merged
    related.setdefault("relatedPolicyParams", [])
    related.setdefault("relatedRequestFields", [])
    related.setdefault("relatedActions", [])
    related.setdefault("relatedMetrics", [])
    return related


def _build_card(
    item: dict[str, Any],
    source: dict[str, Any],
    sequence: int,
) -> dict[str, Any]:
    source_id = item["sourceId"]
    category = item["category"]
    confirmed_text = item["confirmedText"].strip()
    related = _merge_related(category, item["candidateId"])
    return {
        "cardId": f"CARD-{source_id}-{category}-{sequence:03d}",
        "sourceIds": [source_id],
        "category": category,
        "principle": f"수동 확인 근거: {_short_text(confirmed_text)}",
        "projectRule": f"프로젝트 내부 정책 기준으로 {category} 판단 시 수동 확인된 근거와 위치를 참고한다.",
        "evidenceText": confirmed_text,
        "evidenceLocation": _evidence_location(item),
        "relatedPolicyParams": related["relatedPolicyParams"],
        "relatedRequestFields": related["relatedRequestFields"],
        "relatedActions": related["relatedActions"],
        "relatedMetrics": related["relatedMetrics"],
        "sourceType": source.get("sourceType", ""),
        "caution": REQUIRED_CAUTION,
        "createdFromCandidateId": item["candidateId"],
        "reviewer": item["reviewer"],
        "reviewedAt": item["reviewedAt"],
    }


def _write_cards(path: Path, cards: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join(json.dumps(card, ensure_ascii=False) + "\n" for card in cards),
        encoding="utf-8",
    )


def _build_report(
    cards: list[dict[str, Any]],
    confirmed_count: int,
    skipped_pending_count: int,
    skipped_rejected_count: int,
) -> dict[str, Any]:
    cards_by_category = Counter(card["category"] for card in cards)
    cards_by_source = Counter(card["sourceIds"][0] for card in cards)
    return {
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "confirmedCandidateCount": confirmed_count,
        "generatedCardCount": len(cards),
        "skippedPendingCount": skipped_pending_count,
        "skippedRejectedCount": skipped_rejected_count,
        "cardsBySource": dict(sorted(cards_by_source.items())),
        "cardsByCategory": dict(sorted(cards_by_category.items())),
        "cautionSummary": {
            "requiredCautionIncluded": all(REQUIRED_CAUTION in card["caution"] for card in cards),
            "caution": REQUIRED_CAUTION,
        },
        "nextSteps": [
            "policy card 검증",
            "RAG index 생성 준비",
            "이후 Policy Config JSON 스키마 설계",
        ],
    }


def _write_report_md(path: Path, report: dict[str, Any]) -> None:
    lines = [
        "# Policy Knowledge Cards Report",
        "",
        f"- Generated at: {report['generatedAt']}",
        f"- Confirmed candidate count: {report['confirmedCandidateCount']}",
        f"- Generated card count: {report['generatedCardCount']}",
        f"- Skipped pending count: {report['skippedPendingCount']}",
        f"- Skipped rejected count: {report['skippedRejectedCount']}",
        "",
        "## Cards By Source",
    ]
    lines.extend([f"- {source_id}: {count}" for source_id, count in report["cardsBySource"].items()] or ["- None"])
    lines.extend(["", "## Cards By Category"])
    lines.extend([f"- {category}: {count}" for category, count in report["cardsByCategory"].items()] or ["- None"])
    lines.extend(
        [
            "",
            "## Caution",
            "",
            f"- {report['cautionSummary']['caution']}",
            "",
            "## Next Steps",
            "",
        ]
    )
    lines.extend([f"{index}. {step}" for index, step in enumerate(report["nextSteps"], start=1)])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_policy_cards(
    manual_confirmation_path: Path,
    registry_path: Path,
    output_path: Path,
    report_json_path: Path,
    report_md_path: Path,
) -> dict[str, Any]:
    registry = _as_registry_map(_read_json(registry_path))
    items = _manual_items(_read_json(manual_confirmation_path))
    confirmed = [item for item in items if item.get("manualReviewStatus") == "confirmed"]
    skipped_pending_count = sum(1 for item in items if item.get("manualReviewStatus") == "pending_manual_confirmation")
    skipped_rejected_count = sum(1 for item in items if item.get("manualReviewStatus") == "rejected")

    cards: list[dict[str, Any]] = []
    sequence_by_key: Counter[tuple[str, str]] = Counter()
    for item in confirmed:
        _validate_confirmed_item(item, registry)
        key = (item["sourceId"], item["category"])
        sequence_by_key[key] += 1
        cards.append(_build_card(item, registry[item["sourceId"]], sequence_by_key[key]))

    report = _build_report(cards, len(confirmed), skipped_pending_count, skipped_rejected_count)
    _write_json(report_json_path, report)
    _write_report_md(report_md_path, report)

    if cards:
        _write_cards(output_path, cards)

    return report


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate policy knowledge cards from confirmed candidates.")
    parser.add_argument("--manual-confirmation", type=Path, default=DEFAULT_MANUAL_CONFIRMATION_PATH)
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY_PATH)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT_PATH)
    parser.add_argument("--report-json", type=Path, default=DEFAULT_REPORT_JSON_PATH)
    parser.add_argument("--report-md", type=Path, default=DEFAULT_REPORT_MD_PATH)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report = generate_policy_cards(
            args.manual_confirmation,
            args.registry,
            args.output,
            args.report_json,
            args.report_md,
        )
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"Policy card generation failed: {exc}", file=sys.stderr)
        return 1

    if report["generatedCardCount"] == 0:
        print("No confirmed candidates found. policy_knowledge_cards.jsonl was not created.")
    else:
        print(f"Generated policy cards: {report['generatedCardCount']}")
    print(f"Report written to: {args.report_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
