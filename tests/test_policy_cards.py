from __future__ import annotations

import json
from pathlib import Path

from harness.checks.check_policy_cards import REQUIRED_CAUTION, run_check


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "data" / "sources" / "policy_source_registry.json"
MANUAL_RESULTS = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"


def _write_jsonl(path: Path, cards: list[dict]) -> None:
    path.write_text(
        "".join(json.dumps(card, ensure_ascii=False) + "\n" for card in cards),
        encoding="utf-8",
    )


def _base_card(candidate_id: str = "CAND-KOR-003-050") -> dict:
    return {
        "cardId": "CARD-KOR-003-terrain_or_dynamic_safety-001",
        "sourceIds": ["KOR-003"],
        "category": "terrain_or_dynamic_safety",
        "principle": "수동 확인된 근거를 기준으로 지형 및 동적 안전 판단에 참고한다.",
        "projectRule": "프로젝트 내부 정책 기준으로만 사용한다.",
        "evidenceText": "원본 PDF에서 수동 확인한 짧은 문장",
        "evidenceLocation": "PDF p.13",
        "relatedPolicyParams": ["botMassKg", "robotWidthCm", "sidewalkWidthCm"],
        "relatedRequestFields": ["terrain", "botState.speedKmh"],
        "relatedActions": ["Stop", "ReplanPath", "RequestOperator"],
        "relatedMetrics": ["fallDetected", "sidewalkDepartureCount", "collisionCount"],
        "sourceType": "certification",
        "caution": REQUIRED_CAUTION,
        "createdFromCandidateId": candidate_id,
        "reviewer": "hh",
        "reviewedAt": "2026-05-31",
    }


def test_missing_policy_cards_fails_when_confirmed_candidates_exist(tmp_path: Path) -> None:
    missing_cards = tmp_path / "missing_policy_cards.jsonl"

    result = run_check(
        policy_cards_path=missing_cards,
        manual_confirmation_path=MANUAL_RESULTS,
        registry_path=REGISTRY,
    )

    assert result["passed"] is False
    assert result["confirmedCandidateCount"] == 9


def test_policy_cards_check_accepts_valid_cards_from_confirmed_candidates(tmp_path: Path) -> None:
    manual_payload = json.loads(MANUAL_RESULTS.read_text(encoding="utf-8-sig"))
    confirmed_ids = [
        item["candidateId"]
        for item in manual_payload["items"]
        if item["manualReviewStatus"] == "confirmed"
    ]
    cards_path = tmp_path / "policy_knowledge_cards.jsonl"
    _write_jsonl(
        cards_path,
        [
            {
                **_base_card(candidate_id),
                "cardId": f"CARD-KOR-003-test-{index:03d}",
            }
            for index, candidate_id in enumerate(confirmed_ids, start=1)
        ],
    )

    result = run_check(
        policy_cards_path=cards_path,
        manual_confirmation_path=MANUAL_RESULTS,
        registry_path=REGISTRY,
    )

    assert result["passed"] is True
    assert result["cardCount"] == 9


def test_policy_cards_check_fails_for_pending_or_rejected_candidate_card(tmp_path: Path) -> None:
    cards_path = tmp_path / "policy_knowledge_cards.jsonl"
    _write_jsonl(cards_path, [_base_card(candidate_id="CAND-KOR-003-026")])

    result = run_check(
        policy_cards_path=cards_path,
        manual_confirmation_path=MANUAL_RESULTS,
        registry_path=REGISTRY,
    )

    assert result["passed"] is False
    assert result["nonConfirmedCandidateCards"]


def test_policy_cards_check_fails_for_missing_evidence_location(tmp_path: Path) -> None:
    cards_path = tmp_path / "policy_knowledge_cards.jsonl"
    card = _base_card()
    card["evidenceLocation"] = ""
    _write_jsonl(cards_path, [card])

    result = run_check(
        policy_cards_path=cards_path,
        manual_confirmation_path=MANUAL_RESULTS,
        registry_path=REGISTRY,
    )

    assert result["passed"] is False
    assert result["emptyEvidenceCards"]


def test_policy_cards_check_fails_for_positive_compliance_claims(tmp_path: Path) -> None:
    cards_path = tmp_path / "policy_knowledge_cards.jsonl"
    card = _base_card()
    card["projectRule"] = "공식 인증 준수 기준으로 사용한다."
    _write_jsonl(cards_path, [card])

    result = run_check(
        policy_cards_path=cards_path,
        manual_confirmation_path=MANUAL_RESULTS,
        registry_path=REGISTRY,
    )

    assert result["passed"] is False
    assert result["prohibitedComplianceClaims"]
