from __future__ import annotations

from pathlib import Path

from app.services.pdf_rag_corpus import (
    REQUIRED_PDF_RAG_SOURCE_IDS,
    ROOT,
    validate_pdf_rag_source_inventory,
)


def test_repo_source_inventory_registers_all_pdf_rag_sources() -> None:
    result = validate_pdf_rag_source_inventory(ROOT / "data" / "sources" / "source_inventory.json")

    assert result.passed
    assert REQUIRED_PDF_RAG_SOURCE_IDS <= set(result.sources_by_id)
    assert result.sources_by_id["KOR-004"]["source_type"] == "official_notice"
    assert result.sources_by_id["KOR-004"]["authority_rank"] == 1
    assert result.sources_by_id["KOR-004"]["original_format"] == "pdf"
    assert result.sources_by_id["KOR-004"]["stored_format"] == "pdf"


def test_repo_source_inventory_raw_pdf_paths_exist() -> None:
    result = validate_pdf_rag_source_inventory(ROOT / "data" / "sources" / "source_inventory.json")

    for source_id in REQUIRED_PDF_RAG_SOURCE_IDS:
        source = result.sources_by_id[source_id]
        assert (ROOT / source["raw_file_path"]).is_file(), source_id
        assert source["raw_file_path"].endswith(".pdf")


def test_vector_cache_paths_are_not_tracked_data_artifacts() -> None:
    assert not (Path(ROOT) / "data" / "rag" / "chroma").exists()
    assert not (Path(ROOT) / "data" / "rag" / "embeddings").exists()
