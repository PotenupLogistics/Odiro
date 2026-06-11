from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from scripts.validate_file_based_rag_store import (
    EXPECTED_KNOWLEDGE_CARD_COUNT,
    EXPECTED_RUNTIME_CHUNK_COUNT,
    REQUIRED_SOURCE_IDS,
    RUNTIME_ALLOWED_SOURCE_STATUSES,
    validate_file_based_rag_store,
)


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "validate_file_based_rag_store.py"


def _write_jsonl(path: Path, rows: list[dict] | list[str]) -> None:
    lines: list[str] = []
    for row in rows:
        if isinstance(row, str):
            lines.append(row)
        else:
            lines.append(json.dumps(row, ensure_ascii=False))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _base_chunk() -> dict:
    return {
        "chunkId": "CHUNK-CARD-KOR-003-speed_policy-001",
        "cardId": "CARD-KOR-003-speed_policy-001",
        "chunkText": "category: speed_policy\nrelatedActions: SlowDown\nrelatedPolicyParams: maxSpeedKmh",
        "metadata": {
            "sourceIds": ["KOR-003"],
            "category": "speed_policy",
            "relatedPolicyParams": ["maxSpeedKmh"],
            "relatedRequestFields": ["botState.speedKmh"],
            "relatedActions": ["SlowDown"],
            "relatedMetrics": ["deliveryTimeSec"],
            "evidenceLocation": "PDF p.30",
            "createdFromCandidateId": "CAND-KOR-003-052",
            "status": "confirmed_policy_card",
        },
    }


def _base_card() -> dict:
    return {
        "cardId": "CARD-KOR-003-speed_policy-001",
        "sourceIds": ["KOR-003"],
        "category": "speed_policy",
        "principle": "속도 정책 근거",
        "projectRule": "프로젝트 내부 정책 기준으로 사용한다.",
        "evidenceText": "속도 근거",
        "evidenceLocation": "PDF p.30",
        "relatedPolicyParams": ["maxSpeedKmh"],
        "relatedRequestFields": ["botState.speedKmh"],
        "relatedActions": ["SlowDown"],
        "relatedMetrics": ["deliveryTimeSec"],
        "sourceType": "certification",
        "caution": "프로젝트 내부 정책 기준",
        "createdFromCandidateId": "CAND-KOR-003-052",
        "reviewer": "hh",
        "reviewedAt": "2026-05-31",
    }


def _base_source_inventory(sources: list[dict] | None = None) -> dict:
    return {
        "schema": "file_based_rag_source_inventory",
        "version": 1,
        "runtime_rag_store": "data/rag/policy_rag_chunks.jsonl",
        "knowledge_card_store": "data/rag/policy_knowledge_cards.jsonl",
        "sources": sources
        or [
            {
                "source_id": "KOR-003",
                "status": "active",
                "source_type": "guidebook",
                "role": "current runtime policy source",
                "usage": "runtime_rag_chunk_source",
                "description": "KIRIA guidebook",
                "path_status": "verified",
            },
            {
                "source_id": "KOR-004",
                "status": "candidate_active",
                "source_type": "notice",
                "role": "official notice candidate",
                "usage": "candidate for future policy chunk promotion",
                "description": "MOTIE notice",
                "path_status": "verified",
            },
            {
                "source_id": "RSR-001",
                "status": "supporting_candidate",
                "source_type": "research_report",
                "role": "experiment design support",
                "usage": "pedestrian interaction support",
                "description": "METRANS report",
                "path_status": "verified",
            },
            {
                "source_id": "KOR-001",
                "status": "reference_only",
                "source_type": "law",
                "role": "legal background",
                "usage": "reference only",
                "description": "Intelligent Robots Act",
                "path_status": "verified",
            },
            {
                "source_id": "KOR-002",
                "status": "reference_only",
                "source_type": "law",
                "role": "traffic law support",
                "usage": "reference only",
                "description": "Road Traffic Act",
                "path_status": "verified",
            },
            {
                "source_id": "KOR-005",
                "status": "reference_only",
                "source_type": "reference",
                "role": "subordinate rule reference",
                "usage": "reference or review candidate",
                "description": "operation criteria reference",
                "path_status": "verified",
            },
            {
                "source_id": "PRJ-AGENT",
                "status": "active_internal",
                "source_type": "project_internal",
                "role": "analysis and policy server logic",
                "usage": "runtime_rag_chunk_source",
                "description": "internal agent policy",
                "path_status": "internal",
            },
            {
                "source_id": "PRJ-DOE",
                "status": "active_internal",
                "source_type": "project_internal",
                "role": "scenario difficulty and experiment design",
                "usage": "runtime_rag_chunk_source",
                "description": "internal DOE policy",
                "path_status": "internal",
            },
            {
                "source_id": "PRJ-EVAL",
                "status": "active_internal",
                "source_type": "project_internal",
                "role": "evaluation criteria",
                "usage": "runtime_rag_chunk_source",
                "description": "internal evaluation policy",
                "path_status": "internal",
            },
        ],
    }


def _write_inventory(root: Path, inventory: dict | None = None) -> None:
    source_dir = root / "data" / "sources"
    source_dir.mkdir(parents=True, exist_ok=True)
    (source_dir / "source_inventory.json").write_text(
        json.dumps(inventory or _base_source_inventory(), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def test_validator_passes_current_file_based_rag_store() -> None:
    result = validate_file_based_rag_store(ROOT)

    assert result.passed is True
    assert result.runtime_chunk_count == EXPECTED_RUNTIME_CHUNK_COUNT
    assert result.knowledge_card_count == EXPECTED_KNOWLEDGE_CARD_COUNT
    assert result.vector_db_directories_absent is True
    assert result.source_inventory_exists is True
    assert result.source_count == len(REQUIRED_SOURCE_IDS)
    assert result.runtime_source_status_guard_passed is True


def test_cli_prints_human_readable_pass_summary() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )

    assert completed.returncode == 0
    assert "runtime chunk file: OK" in completed.stdout
    assert "chunk count: 15" in completed.stdout
    assert "knowledge card count: 9" in completed.stdout
    assert "source inventory: OK" in completed.stdout
    assert "source count: 9" in completed.stdout
    assert "active sources: KOR-003" in completed.stdout
    assert "candidate sources: KOR-004, RSR-001" in completed.stdout
    assert "reference-only sources: KOR-001, KOR-002, KOR-005" in completed.stdout
    assert "runtime source status guard: OK" in completed.stdout
    assert "runtime allowed statuses: active, active_internal" in completed.stdout
    assert "runtime source ids used by chunks: KOR-003, PRJ-AGENT, PRJ-DOE, PRJ-EVAL" in completed.stdout
    assert "candidate/reference sources excluded from runtime chunks: OK" in completed.stdout
    assert "vector db directories: absent" in completed.stdout
    assert "result: PASS" in completed.stdout


def test_validator_reports_missing_required_chunk_field(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    del chunk["metadata"]["relatedActions"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("line 1" in error and "metadata.relatedActions" in error for error in result.errors)


def test_validator_reports_invalid_json_line(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [_base_chunk()])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", ["{not-json"])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("policy_knowledge_cards.jsonl line 1" in error for error in result.errors)


def test_validator_reports_vector_db_directory(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [_base_chunk()])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    (rag_dir / "chroma").mkdir()
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("data/rag/chroma must be absent" in error for error in result.errors)


def test_validator_requires_current_cards_to_be_kor_003_based(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    card = _base_card()
    card["sourceIds"] = ["KOR-004"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [_base_chunk()])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [card])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("must remain KOR-003 based" in error for error in result.errors)


def test_source_inventory_contains_required_sources() -> None:
    result = validate_file_based_rag_store(ROOT)

    assert result.passed is True
    assert set(result.source_ids) == set(REQUIRED_SOURCE_IDS)
    assert "KOR-003" in result.active_sources
    assert result.candidate_sources == ["KOR-004", "RSR-001"]
    assert result.reference_only_sources == ["KOR-001", "KOR-002", "KOR-005"]


def test_validator_reports_duplicate_source_id(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [_base_chunk()])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    sources = _base_source_inventory()["sources"]
    _write_inventory(tmp_path, _base_source_inventory([sources[0], sources[0], *sources[1:]]))

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("duplicate source_id: KOR-003" in error for error in result.errors)


def test_validator_reports_invalid_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [_base_chunk()])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    inventory = _base_source_inventory()
    inventory["sources"][0]["status"] = "active_postgres"
    _write_inventory(tmp_path, inventory)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("KOR-003" in error and "invalid status" in error for error in result.errors)


def test_validator_reports_chunk_source_missing_from_inventory(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    chunk["metadata"]["sourceIds"] = ["KOR-999"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any("sourceIds not registered in source inventory: KOR-999" in error for error in result.errors)


def test_runtime_chunk_allows_active_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [_base_chunk()])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is True
    assert result.runtime_source_status_guard_passed is True
    assert tuple(result.runtime_allowed_statuses) == RUNTIME_ALLOWED_SOURCE_STATUSES


def test_runtime_chunk_allows_active_internal_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    chunk["metadata"]["sourceIds"] = ["PRJ-DOE"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is True
    assert result.runtime_source_status_guard_passed is True
    assert result.runtime_source_ids_used_by_chunks == ["PRJ-DOE"]


def test_runtime_chunk_rejects_candidate_active_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    chunk["metadata"]["sourceIds"] = ["KOR-004"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any(
        "chunk line 1 uses source_id KOR-004 with status candidate_active" in error
        and "active or active_internal" in error
        for error in result.errors
    )


def test_runtime_chunk_rejects_supporting_candidate_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    chunk["metadata"]["sourceIds"] = ["RSR-001"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any(
        "chunk line 1 uses source_id RSR-001 with status supporting_candidate" in error
        and "active or active_internal" in error
        for error in result.errors
    )


def test_runtime_chunk_rejects_reference_only_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    chunk["metadata"]["sourceIds"] = ["KOR-001"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    _write_inventory(tmp_path)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any(
        "chunk line 1 uses source_id KOR-001 with status reference_only" in error
        and "active or active_internal" in error
        for error in result.errors
    )


def test_runtime_chunk_rejects_review_candidate_source_status(tmp_path: Path) -> None:
    rag_dir = tmp_path / "data" / "rag"
    rag_dir.mkdir(parents=True)
    chunk = _base_chunk()
    chunk["metadata"]["sourceIds"] = ["KOR-005"]
    _write_jsonl(rag_dir / "policy_rag_chunks.jsonl", [chunk])
    _write_jsonl(rag_dir / "policy_knowledge_cards.jsonl", [_base_card()])
    inventory = _base_source_inventory()
    for source in inventory["sources"]:
        if source["source_id"] == "KOR-005":
            source["status"] = "review_candidate"
    _write_inventory(tmp_path, inventory)

    result = validate_file_based_rag_store(
        tmp_path,
        expected_runtime_chunk_count=1,
        expected_knowledge_card_count=1,
    )

    assert result.passed is False
    assert any(
        "chunk line 1 uses source_id KOR-005 with status review_candidate" in error
        and "active or active_internal" in error
        for error in result.errors
    )
