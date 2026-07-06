from __future__ import annotations

import json
from pathlib import Path

import pytest

from app.services.pdf_rag_corpus import (
    ALLOWED_ROUTE_NAMES,
    ALLOWED_SOURCE_TYPES,
    ALLOWED_TOPIC_TAGS,
    ALLOWED_USE_SCOPES,
    PdfRagCorpusValidationError,
    build_chunk_hash,
    build_stable_chunk_id,
    chunk_korean_law_text,
    validate_pdf_rag_chunk,
    validate_pdf_rag_source_inventory,
)
from scripts.validate_pdf_rag_corpus import main as validate_pdf_rag_corpus_main


def _source_inventory(tmp_path: Path) -> Path:
    raw = tmp_path / "data" / "sources" / "raw" / "korea" / "KOR-004.pdf"
    raw.parent.mkdir(parents=True, exist_ok=True)
    raw.write_bytes(b"%PDF-1.4\n")
    processed = tmp_path / "data" / "sources" / "processed" / "korea" / "KOR-004.md"
    processed.parent.mkdir(parents=True, exist_ok=True)
    processed.write_text("# KOR-004\n", encoding="utf-8")
    inventory = tmp_path / "data" / "sources" / "source_inventory.json"
    inventory.parent.mkdir(parents=True, exist_ok=True)
    inventory.write_text(
        json.dumps(
            {
                "schema": "pdf_vector_hybrid_rag_source_inventory",
                "version": 2,
                "sources": [
                    {
                        "source_id": "KOR-004",
                        "source_type": "official_notice",
                        "authority_rank": 1,
                        "version_status": "active",
                        "raw_file_path": "data/sources/raw/korea/KOR-004.pdf",
                        "processed_file_path": "data/sources/processed/korea/KOR-004.md",
                        "source_hash": "sha256:test",
                        "original_format": "pdf",
                        "stored_format": "pdf",
                        "effective_date": "2026-01-01",
                    }
                ],
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    return inventory


def test_allowed_values_match_pdf_rag_contract() -> None:
    assert "official_notice" in ALLOWED_SOURCE_TYPES
    assert "safety_certification" in ALLOWED_ROUTE_NAMES
    assert "certification_requirement" in ALLOWED_USE_SCOPES
    assert "speed_policy" in ALLOWED_TOPIC_TAGS


def test_source_inventory_accepts_pdf_kor_004_without_hwpx_or_converted_by(tmp_path: Path) -> None:
    result = validate_pdf_rag_source_inventory(_source_inventory(tmp_path), root=tmp_path)

    assert result.passed
    assert result.sources_by_id["KOR-004"]["original_format"] == "pdf"
    assert "hwpx" not in json.dumps(result.sources_by_id["KOR-004"])
    assert "converted_by" not in json.dumps(result.sources_by_id["KOR-004"])


def test_source_inventory_rejects_unknown_values_at_validation_stage(tmp_path: Path) -> None:
    inventory_path = _source_inventory(tmp_path)
    payload = json.loads(inventory_path.read_text(encoding="utf-8"))
    payload["sources"][0]["source_type"] = "guidebook"
    inventory_path.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")

    with pytest.raises(PdfRagCorpusValidationError):
        validate_pdf_rag_source_inventory(inventory_path, root=tmp_path, strict=True)


def test_korean_law_chunker_preserves_article_parent_and_paragraph_child() -> None:
    chunks = chunk_korean_law_text(
        source_id="KOR-001",
        source_title="지능형 로봇 개발 및 보급 촉진법",
        effective_date="2025-10-01",
        page_texts=[
            (
                1,
                "제2조(정의) ① 이 법에서 지능형 로봇이란 외부환경을 스스로 인식하고 "
                "상황을 판단하여 자율적으로 동작하는 기계를 말한다. "
                "② 실외이동로봇이란 도로 등에서 이동하는 지능형 로봇을 말한다.\n"
                "제3조(책무) ① 국가는 지능형 로봇의 안전한 이용을 촉진한다.",
            )
        ],
    )

    parents = [chunk for chunk in chunks if chunk["chunk_kind"] == "parent"]
    children = [chunk for chunk in chunks if chunk["chunk_kind"] == "child"]
    assert [parent["section_title"] for parent in parents] == ["제2조(정의)", "제3조(책무)"]
    assert {child["article_number"] for child in children} == {"제2조", "제3조"}
    assert all(child["source_title"] == "지능형 로봇 개발 및 보급 촉진법" for child in children)
    assert all(child["effective_date"] == "2025-10-01" for child in children)


def test_chunk_id_is_stable_and_chunk_hash_changes_with_text() -> None:
    first_id = build_stable_chunk_id(
        source_id="KOR-004",
        hierarchy_path=["별표 1", "운행속도"],
        section_title="운행속도",
        text="보호구역에서는 5 km/h 이하로 운행한다.",
    )
    second_id = build_stable_chunk_id(
        source_id="KOR-004",
        hierarchy_path=["별표 1", "운행속도"],
        section_title="운행속도",
        text="보호구역에서는 5 km/h 이하로 운행한다.",
    )
    assert first_id == second_id
    assert build_chunk_hash("5 km/h") != build_chunk_hash("6 km/h")


def test_validated_child_chunk_rejects_undefined_topic_tag() -> None:
    valid_chunk = {
        "chunk_id": "KOR-004-child-speed",
        "parent_chunk_id": "KOR-004-parent-speed",
        "source_id": "KOR-004",
        "source_type": "official_notice",
        "authority_rank": 1,
        "version_status": "active",
        "page_start": 8,
        "page_end": 9,
        "section_title": "운행속도",
        "hierarchy_path": ["별표 1", "운행속도"],
        "topic_tags": ["speed_policy"],
        "use_scope": ["certification_requirement", "safety_rule"],
        "route_names": ["safety_certification"],
        "chunk_kind": "child",
        "language": "ko",
        "text": "보호구역에서는 5 km/h 이하로 운행한다.",
        "chunk_hash": build_chunk_hash("보호구역에서는 5 km/h 이하로 운행한다."),
        "review_status": "auto_validated",
        "extraction_confidence": "high",
    }
    validate_pdf_rag_chunk(valid_chunk, strict=True)
    invalid_chunk = dict(valid_chunk)
    invalid_chunk["topic_tags"] = ["not_defined"]

    with pytest.raises(PdfRagCorpusValidationError):
        validate_pdf_rag_chunk(invalid_chunk, strict=True)


def test_validate_pdf_rag_corpus_rejects_empty_validated_chunk_file(tmp_path: Path) -> None:
    inventory_path = _source_inventory(tmp_path)
    chunks_path = tmp_path / "data" / "rag" / "pdf_corpus" / "validated_parent_child_chunks.jsonl"
    chunks_path.parent.mkdir(parents=True, exist_ok=True)
    chunks_path.write_text("", encoding="utf-8")

    result = validate_pdf_rag_corpus_main(
        [
            "--inventory",
            str(inventory_path),
            "--chunks",
            str(chunks_path),
        ]
    )

    assert result == 1
