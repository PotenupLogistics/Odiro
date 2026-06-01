from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
MANUAL_CONFIRMATION_PATH = (
    ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
)
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
REQUIRED_CAUTION = (
    "이 카드는 원본 문서의 수동 확인 내용을 바탕으로 만든 프로젝트 내부 정책 기준이며, "
    "공식 인증 준수를 의미하지 않는다."
)
PROHIBITED_CLAIMS = ["공식 인증 준수", "인증을 보장", "ISO 준수", "법적 준수 보장"]
REQUIRED_FIELDS = {
    "cardId",
    "sourceIds",
    "category",
    "principle",
    "projectRule",
    "evidenceText",
    "evidenceLocation",
    "relatedPolicyParams",
    "relatedRequestFields",
    "relatedActions",
    "relatedMetrics",
    "sourceType",
    "caution",
    "createdFromCandidateId",
    "reviewer",
    "reviewedAt",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "policy_cards",
        "passed": False,
        "warning": False,
        "policyCardsExists": False,
        "cardCount": 0,
        "confirmedCandidateCount": 0,
        "duplicateCardIds": [],
        "parseErrors": [],
        "missingRequiredFields": [],
        "unknownSourceIds": [],
        "missingCautionCards": [],
        "emptyEvidenceCards": [],
        "prohibitedComplianceClaims": [],
        "nonConfirmedCandidateCards": [],
        "cardCountMismatch": False,
        "warnings": [],
        "errors": [],
    }


def _read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _registry_source_ids(payload: Any) -> set[str]:
    sources = payload.get("sources") if isinstance(payload, dict) else payload
    if not isinstance(sources, list):
        return set()
    return {
        source["sourceId"]
        for source in sources
        if isinstance(source, dict) and isinstance(source.get("sourceId"), str)
    }


def _manual_status_map(payload: Any) -> dict[str, str]:
    items = payload.get("items") if isinstance(payload, dict) else []
    if not isinstance(items, list):
        return {}
    return {
        item["candidateId"]: item.get("manualReviewStatus", "")
        for item in items
        if isinstance(item, dict) and isinstance(item.get("candidateId"), str)
    }


def _read_jsonl(path: Path) -> tuple[list[dict[str, Any]], list[str]]:
    cards: list[dict[str, Any]] = []
    errors: list[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            payload = json.loads(line)
        except json.JSONDecodeError as exc:
            errors.append(f"line {line_number}: {exc}")
            continue
        if not isinstance(payload, dict):
            errors.append(f"line {line_number}: card must be an object")
            continue
        cards.append(payload)
    return cards, errors


def _is_blank(value: Any) -> bool:
    return not isinstance(value, str) or not value.strip()


def _has_prohibited_claim(card: dict[str, Any]) -> bool:
    for field, value in card.items():
        if field == "caution":
            continue
        values: list[str] = []
        if isinstance(value, str):
            values.append(value)
        elif isinstance(value, list):
            values.extend(item for item in value if isinstance(item, str))
        if any(claim in text for claim in PROHIBITED_CLAIMS for text in values):
            return True
    return False


def run_check(
    policy_cards_path: Path = POLICY_CARDS_PATH,
    manual_confirmation_path: Path = MANUAL_CONFIRMATION_PATH,
    registry_path: Path = REGISTRY_PATH,
) -> dict[str, Any]:
    result = _base_result()

    try:
        source_ids = _registry_source_ids(_read_json(registry_path))
        manual_statuses = _manual_status_map(_read_json(manual_confirmation_path))
    except (OSError, json.JSONDecodeError) as exc:
        result["errors"].append(f"required input parse failed: {exc}")
        return result

    confirmed_ids = {
        candidate_id for candidate_id, status in manual_statuses.items() if status == "confirmed"
    }
    result["confirmedCandidateCount"] = len(confirmed_ids)

    if not policy_cards_path.exists():
        if confirmed_ids:
            result["errors"].append("Confirmed candidates exist but policy_knowledge_cards.jsonl is missing.")
        result["passed"] = not result["errors"]
        return result

    result["policyCardsExists"] = True
    cards, parse_errors = _read_jsonl(policy_cards_path)
    result["parseErrors"] = parse_errors
    result["cardCount"] = len(cards)

    if len(cards) != len(confirmed_ids):
        result["cardCountMismatch"] = True
        result["errors"].append(
            f"Generated card count {len(cards)} does not match confirmed count {len(confirmed_ids)}."
        )

    card_ids = [card.get("cardId") for card in cards]
    counts = Counter(card_ids)
    result["duplicateCardIds"] = sorted(card_id for card_id, count in counts.items() if count > 1)

    for card in cards:
        card_id = card.get("cardId", "<unknown>")
        missing = sorted(field for field in REQUIRED_FIELDS if field not in card)
        if missing:
            result["missingRequiredFields"].append({"cardId": card_id, "missingFields": missing})

        if _is_blank(card.get("evidenceText")) or _is_blank(card.get("evidenceLocation")):
            result["emptyEvidenceCards"].append(card_id)

        source_ids_value = card.get("sourceIds")
        if not isinstance(source_ids_value, list) or not source_ids_value:
            result["unknownSourceIds"].append({"cardId": card_id, "sourceIds": source_ids_value})
        else:
            unknown = [source_id for source_id in source_ids_value if source_id not in source_ids]
            if unknown:
                result["unknownSourceIds"].append({"cardId": card_id, "sourceIds": unknown})

        if REQUIRED_CAUTION not in str(card.get("caution", "")):
            result["missingCautionCards"].append(card_id)

        if _has_prohibited_claim(card):
            result["prohibitedComplianceClaims"].append(card_id)

        candidate_id = card.get("createdFromCandidateId")
        if candidate_id not in confirmed_ids:
            result["nonConfirmedCandidateCards"].append(
                {
                    "cardId": card_id,
                    "createdFromCandidateId": candidate_id,
                    "manualReviewStatus": manual_statuses.get(candidate_id),
                }
            )

    result["passed"] = not any(
        [
            result["errors"],
            result["parseErrors"],
            result["duplicateCardIds"],
            result["missingRequiredFields"],
            result["unknownSourceIds"],
            result["missingCautionCards"],
            result["emptyEvidenceCards"],
            result["prohibitedComplianceClaims"],
            result["nonConfirmedCandidateCards"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
