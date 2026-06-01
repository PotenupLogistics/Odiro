from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "generate_policy_cards.py"
SOURCE_RESULTS = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
REGISTRY = ROOT / "data" / "sources" / "policy_source_registry.json"


def _copy_results(tmp_path: Path) -> Path:
    target = tmp_path / "manual_confirmation_results.json"
    shutil.copyfile(SOURCE_RESULTS, target)
    return target


def _run_generator(
    manual_path: Path,
    output_path: Path,
    report_json_path: Path,
    report_md_path: Path,
    registry_path: Path = REGISTRY,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--manual-confirmation",
            str(manual_path),
            "--registry",
            str(registry_path),
            "--output",
            str(output_path),
            "--report-json",
            str(report_json_path),
            "--report-md",
            str(report_md_path),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _write_payload(path: Path, items: list[dict]) -> None:
    path.write_text(
        json.dumps({"items": items}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def _read_cards(path: Path) -> list[dict]:
    return [
        json.loads(line)
        for line in path.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]


def test_confirmed_candidates_generate_nine_cards(tmp_path: Path) -> None:
    manual_path = _copy_results(tmp_path)
    output_path = tmp_path / "policy_knowledge_cards.jsonl"
    report_json = tmp_path / "report.json"
    report_md = tmp_path / "report.md"

    completed = _run_generator(manual_path, output_path, report_json, report_md)

    assert completed.returncode == 0, completed.stderr
    cards = _read_cards(output_path)
    report = _load_json(report_json)
    assert len(cards) == 9
    assert report["confirmedCandidateCount"] == 9
    assert report["generatedCardCount"] == 9
    assert report["skippedPendingCount"] == 30
    assert report["skippedRejectedCount"] == 4


def test_pending_and_rejected_candidates_are_not_generated(tmp_path: Path) -> None:
    manual_path = _copy_results(tmp_path)
    output_path = tmp_path / "policy_knowledge_cards.jsonl"
    completed = _run_generator(
        manual_path,
        output_path,
        tmp_path / "report.json",
        tmp_path / "report.md",
    )

    assert completed.returncode == 0, completed.stderr
    manual = _load_json(manual_path)
    confirmed_ids = {
        item["candidateId"]
        for item in manual["items"]
        if item["manualReviewStatus"] == "confirmed"
    }
    cards = _read_cards(output_path)
    assert {card["createdFromCandidateId"] for card in cards} == confirmed_ids


def test_generated_cards_have_required_fields_and_caution(tmp_path: Path) -> None:
    manual_path = _copy_results(tmp_path)
    output_path = tmp_path / "policy_knowledge_cards.jsonl"
    completed = _run_generator(
        manual_path,
        output_path,
        tmp_path / "report.json",
        tmp_path / "report.md",
    )

    assert completed.returncode == 0, completed.stderr
    required = {
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
    registry_ids = {source["sourceId"] for source in _load_json(REGISTRY)}
    for card in _read_cards(output_path):
        assert required.issubset(card)
        assert card["evidenceText"]
        assert card["evidenceLocation"]
        assert "공식 인증 준수를 의미하지 않는다" in card["caution"]
        assert set(card["sourceIds"]).issubset(registry_ids)


def test_special_candidate_mappings_are_applied(tmp_path: Path) -> None:
    manual_path = _copy_results(tmp_path)
    output_path = tmp_path / "policy_knowledge_cards.jsonl"
    completed = _run_generator(
        manual_path,
        output_path,
        tmp_path / "report.json",
        tmp_path / "report.md",
    )

    assert completed.returncode == 0, completed.stderr
    cards = {card["createdFromCandidateId"]: card for card in _read_cards(output_path)}
    assert {"botMassKg", "robotWidthCm", "sidewalkWidthCm"}.issubset(
        set(cards["CAND-KOR-003-050"]["relatedPolicyParams"])
    )
    assert {"Stop", "YieldWait", "Continue"}.issubset(
        set(cards["CAND-KOR-003-059"]["relatedActions"])
    )
    assert {"RequestOperator", "EmergencyStop"}.issubset(
        set(cards["CAND-KOR-003-062"]["relatedActions"])
    )


def test_generator_is_idempotent_for_same_input(tmp_path: Path) -> None:
    manual_path = _copy_results(tmp_path)
    output_path = tmp_path / "policy_knowledge_cards.jsonl"
    report_json = tmp_path / "report.json"
    report_md = tmp_path / "report.md"

    first = _run_generator(manual_path, output_path, report_json, report_md)
    second = _run_generator(manual_path, output_path, report_json, report_md)

    assert first.returncode == 0, first.stderr
    assert second.returncode == 0, second.stderr
    cards = _read_cards(output_path)
    assert len(cards) == 9
    assert len({card["cardId"] for card in cards}) == 9


def test_confirmed_candidate_with_unknown_source_fails(tmp_path: Path) -> None:
    payload = _load_json(SOURCE_RESULTS)
    confirmed = next(item for item in payload["items"] if item["manualReviewStatus"] == "confirmed")
    broken = dict(confirmed)
    broken["sourceId"] = "NOPE-001"
    manual_path = tmp_path / "manual_confirmation_results.json"
    _write_payload(manual_path, [broken])

    completed = _run_generator(
        manual_path,
        tmp_path / "policy_knowledge_cards.jsonl",
        tmp_path / "report.json",
        tmp_path / "report.md",
    )

    assert completed.returncode != 0
    assert "sourceId not found in registry" in completed.stderr
