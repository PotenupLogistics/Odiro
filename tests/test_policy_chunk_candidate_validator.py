from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from scripts.validate_policy_chunk_candidates import validate_policy_chunk_candidates


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "validate_policy_chunk_candidates.py"


def _write_inventory(root: Path) -> None:
    source_dir = root / "data" / "sources"
    source_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema": "file_based_rag_source_inventory",
        "version": 1,
        "runtime_rag_store": "data/rag/policy_rag_chunks.jsonl",
        "knowledge_card_store": "data/rag/policy_knowledge_cards.jsonl",
        "sources": [
            {
                "source_id": "KOR-003",
                "status": "active",
                "role": "current runtime policy source",
                "usage": "runtime_rag_chunk_source",
            },
            {
                "source_id": "KOR-004",
                "status": "candidate_active",
                "role": "official notice candidate",
                "usage": "candidate for future policy chunk promotion",
            },
            {
                "source_id": "RSR-001",
                "status": "supporting_candidate",
                "role": "experiment design support",
                "usage": "pedestrian interaction support",
            },
            {
                "source_id": "KOR-001",
                "status": "reference_only",
                "role": "legal background",
                "usage": "reference only",
            },
            {
                "source_id": "KOR-005",
                "status": "review_candidate",
                "role": "operation criteria reference",
                "usage": "review candidate",
            },
        ],
    }
    (source_dir / "source_inventory.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def _candidate_dir(root: Path) -> Path:
    path = root / "data" / "sources" / "review" / "candidates"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _write_candidate(root: Path, payload: dict, name: str = "KOR-004_candidate.json") -> Path:
    path = _candidate_dir(root) / name
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return path


def _valid_candidate(**overrides: object) -> dict:
    payload = {
        "schema": "policy_chunk_candidate",
        "version": 1,
        "source_id": "KOR-004",
        "source_status_at_review": "candidate_active",
        "candidate_id": "CAND-KOR-004-PLACEHOLDER-001",
        "review_status": "candidate",
        "category": "placeholder_category",
        "chunkText": "PLACEHOLDER: verified evidence summary goes here.",
        "relatedActions": ["SlowDown"],
        "relatedPolicyParams": ["maxSpeedKmh"],
        "sourceEvidence": {
            "document_path": "data/sources/processed/korea/KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.md",
            "page": "PLACEHOLDER_PAGE",
            "section": "PLACEHOLDER_SECTION",
            "evidence_text": "PLACEHOLDER_EVIDENCE_TEXT",
        },
        "reviewNotes": "PLACEHOLDER_REVIEW_NOTES",
        "promotionDecision": {
            "can_promote_to_runtime": False,
            "decision_reason": "PLACEHOLDER_DECISION_REASON",
            "reviewed_by": "PLACEHOLDER_REVIEWER",
            "reviewed_at": "PLACEHOLDER_DATE",
        },
    }
    payload.update(overrides)
    return payload


def test_candidate_validator_passes_when_candidate_files_are_absent(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _candidate_dir(tmp_path)

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is True
    assert result.candidate_count == 0


def test_candidate_validator_cli_passes_repo_state() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )

    assert completed.returncode == 0
    assert "candidate files:" in completed.stdout
    assert "result: PASS" in completed.stdout


def test_valid_candidate_file_passes(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _write_candidate(tmp_path, _valid_candidate())

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is True
    assert result.candidate_count == 1


def test_candidate_with_unknown_source_id_fails(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _write_candidate(tmp_path, _valid_candidate(source_id="KOR-999"))

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is False
    assert any("source_id KOR-999 is not registered" in error for error in result.errors)


def test_candidate_with_invalid_review_status_fails(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _write_candidate(tmp_path, _valid_candidate(review_status="ready_for_runtime"))

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is False
    assert any("invalid review_status ready_for_runtime" in error for error in result.errors)


def test_candidate_related_actions_must_be_list(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _write_candidate(tmp_path, _valid_candidate(relatedActions="SlowDown"))

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is False
    assert any("relatedActions must be a list" in error for error in result.errors)


def test_candidate_related_policy_params_must_be_list(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _write_candidate(tmp_path, _valid_candidate(relatedPolicyParams="maxSpeedKmh"))

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is False
    assert any("relatedPolicyParams must be a list" in error for error in result.errors)


def test_candidate_chunk_text_must_not_be_empty(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    _write_candidate(tmp_path, _valid_candidate(chunkText=""))

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is False
    assert any("chunkText must be a non-empty string" in error for error in result.errors)


def test_candidate_requires_source_evidence(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    payload = _valid_candidate()
    del payload["sourceEvidence"]
    _write_candidate(tmp_path, payload)

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is False
    assert any("sourceEvidence must be an object" in error for error in result.errors)


def test_template_file_is_ignored(tmp_path: Path) -> None:
    _write_inventory(tmp_path)
    template_dir = tmp_path / "data" / "sources" / "review" / "templates"
    template_dir.mkdir(parents=True)
    (template_dir / "policy_chunk_candidate_template.json").write_text(
        json.dumps({"schema": "policy_chunk_candidate_template"}, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    result = validate_policy_chunk_candidates(tmp_path)

    assert result.passed is True
    assert result.candidate_count == 0
