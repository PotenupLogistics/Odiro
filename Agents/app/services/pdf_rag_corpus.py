"""PDF source corpus preparation and validation for the vector hybrid RAG path."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import Counter
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


# Repository root for Agents-local data paths.
ROOT = Path(__file__).resolve().parents[2]

# Schema label used by the PDF RAG source inventory.
PDF_RAG_SOURCE_INVENTORY_SCHEMA = "pdf_vector_hybrid_rag_source_inventory"

# Schema version for source inventory metadata consumed by the PDF RAG builder.
PDF_RAG_SOURCE_INVENTORY_VERSION = 2

# Schema version for parent/child chunk rows consumed by retrieval and indexing.
PDF_RAG_METADATA_SCHEMA_VERSION = "pdf-rag-metadata-v1"

# Chunker version included in build reports and Chroma manifests.
PDF_RAG_CHUNKER_VERSION = "pdf-rag-chunker-v1"

# Extractor version included in build reports and Chroma manifests.
PDF_RAG_EXTRACTOR_VERSION = "pypdf-page-text-v1"

# Table parser version included in build reports and Chroma manifests.
PDF_RAG_TABLE_PARSER_VERSION = "heuristic-table-qa-v1"

# Source types allowed after the validation gate.
ALLOWED_SOURCE_TYPES = {
    "law",
    "official_notice",
    "certification_guide",
    "research_paper",
    "internal_project_doc",
}

# Route names allowed on validated child chunks.
ALLOWED_ROUTE_NAMES = {
    "safety_certification",
    "legal_road_operation",
    "crosswalk_sidewalk",
    "experiment_coverage",
    "scenario_generation",
    "reward_sim_to_real",
}

# Retrieval scopes allowed on validated child chunks.
ALLOWED_USE_SCOPES = {
    "legal_basis",
    "road_operation_rule",
    "definition",
    "certification_requirement",
    "safety_rule",
    "experiment_design",
    "coverage_gap",
    "scenario_generation",
    "next_run_recommendation",
    "reward_design",
    "sim_to_real",
}

# Topic tags allowed on validated child chunks.
ALLOWED_TOPIC_TAGS = {
    "speed_policy",
    "emergency_stop",
    "obstacle_detection",
    "perception_requirement",
    "crosswalk_operation",
    "sidewalk_operation",
    "operator_control",
    "terrain_or_dynamic_safety",
    "coverage_model",
    "constraint_model",
    "scenario_specification",
}

# Source ids expected for the PDF RAG corpus migration.
REQUIRED_PDF_RAG_SOURCE_IDS = {
    "KOR-001",
    "KOR-002",
    "KOR-003",
    "KOR-004",
    "KOR-005",
    "KOR-006",
    "KOR-007",
    "KOR-008",
    "RSR-001",
    "RSR-002",
    "RSR-003",
    "RSR-004",
    "RSR-005",
    "RSR-006",
}

# Review statuses allowed for automatically generated chunks.
ALLOWED_REVIEW_STATUSES = {"candidate", "auto_validated", "sample_reviewed"}

# Confidence labels used by extraction/table QA.
ALLOWED_EXTRACTION_CONFIDENCE = {"high", "medium", "low"}


class PdfRagCorpusValidationError(ValueError):
    """Raised when PDF RAG source or chunk validation fails in strict mode."""


@dataclass(frozen=True)
class PdfRagSourceInventoryValidationResult:
    """Validation details for a PDF RAG source inventory file."""

    passed: bool
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    sources_by_id: dict[str, dict[str, Any]] = field(default_factory=dict)


@dataclass(frozen=True)
class PdfRagCorpusBuildResult:
    """Paths and counts produced by a PDF RAG corpus build."""

    candidate_path: Path
    validated_path: Path
    report_json_path: Path
    report_md_path: Path
    candidate_count: int
    validated_count: int
    warnings: list[str]


def normalize_text_for_hash(text: str) -> str:
    """Normalize text so stable ids survive whitespace-only extraction changes."""
    return re.sub(r"\s+", " ", text).strip()


def build_chunk_hash(text: str) -> str:
    """Return a stable content hash for chunk text."""
    normalized = normalize_text_for_hash(text)
    return "sha256:" + hashlib.sha256(normalized.encode("utf-8")).hexdigest()


def build_file_hash(path: Path) -> str:
    """Return a sha256 hash for a source or corpus artifact file."""
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def build_stable_chunk_id(
    *,
    source_id: str,
    hierarchy_path: Iterable[str],
    section_title: str,
    text: str,
) -> str:
    """Create a stable chunk id from source hierarchy and normalized text."""
    stable_basis = "\n".join(
        [
            source_id,
            "/".join(part.strip() for part in hierarchy_path if part.strip()),
            section_title.strip(),
            normalize_text_for_hash(text),
        ]
    )
    digest = hashlib.sha1(stable_basis.encode("utf-8")).hexdigest()[:12]
    return f"{source_id}-chunk-{digest}"


def _read_json(path: Path) -> dict[str, Any]:
    """Read a UTF-8 JSON object from disk."""
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise PdfRagCorpusValidationError(f"{path}: JSON root must be an object")
    return value


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    """Write a JSON object with stable UTF-8 formatting."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    """Write JSONL rows with one object per line."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows), encoding="utf-8")


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    """Read a JSONL object list from a corpus artifact."""
    rows: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        if not line.strip():
            continue
        value = json.loads(line)
        if not isinstance(value, dict):
            raise PdfRagCorpusValidationError(f"{path} line {line_number}: row must be an object")
        rows.append(value)
    return rows


def validate_pdf_rag_source_inventory(
    inventory_path: Path,
    *,
    root: Path = ROOT,
    strict: bool = False,
) -> PdfRagSourceInventoryValidationResult:
    """Validate source inventory metadata required by the PDF RAG corpus builder."""
    errors: list[str] = []
    warnings: list[str] = []
    inventory = _read_json(inventory_path)
    if inventory.get("schema") != PDF_RAG_SOURCE_INVENTORY_SCHEMA:
        errors.append(f"schema must be {PDF_RAG_SOURCE_INVENTORY_SCHEMA}")
    if inventory.get("version") != PDF_RAG_SOURCE_INVENTORY_VERSION:
        errors.append(f"version must be {PDF_RAG_SOURCE_INVENTORY_VERSION}")

    sources = inventory.get("sources")
    if not isinstance(sources, list):
        errors.append("sources must be a list")
        sources = []

    sources_by_id: dict[str, dict[str, Any]] = {}
    for index, source in enumerate(sources, start=1):
        if not isinstance(source, dict):
            errors.append(f"sources[{index}] must be an object")
            continue
        source_id = source.get("source_id")
        if not isinstance(source_id, str) or not source_id.strip():
            errors.append(f"sources[{index}].source_id must be a non-empty string")
            continue
        if source_id in sources_by_id:
            errors.append(f"duplicate source_id: {source_id}")
        sources_by_id[source_id] = source
        _validate_source_row(source, root=root, errors=errors, warnings=warnings)

    missing = sorted(REQUIRED_PDF_RAG_SOURCE_IDS - set(sources_by_id))
    if missing:
        warnings.append("missing PDF RAG source ids: " + ", ".join(missing))

    passed = not errors
    if strict and errors:
        raise PdfRagCorpusValidationError("; ".join(errors))
    return PdfRagSourceInventoryValidationResult(
        passed=passed,
        errors=errors,
        warnings=warnings,
        sources_by_id=sources_by_id,
    )


def _validate_source_row(
    source: dict[str, Any],
    *,
    root: Path,
    errors: list[str],
    warnings: list[str],
) -> None:
    """Validate one source inventory record."""
    source_id = str(source.get("source_id") or "")
    for field_name in (
        "source_type",
        "authority_rank",
        "version_status",
        "raw_file_path",
        "processed_file_path",
        "source_hash",
        "original_format",
        "stored_format",
        "effective_date",
    ):
        if field_name not in source:
            errors.append(f"{source_id}: missing {field_name}")

    source_type = source.get("source_type")
    if source_type not in ALLOWED_SOURCE_TYPES:
        errors.append(f"{source_id}: invalid source_type {source_type}")
    if source.get("version_status") not in {"active", "superseded"}:
        errors.append(f"{source_id}: version_status must be active or superseded")
    if not isinstance(source.get("authority_rank"), int):
        errors.append(f"{source_id}: authority_rank must be an integer")

    raw_file_path = source.get("raw_file_path")
    if isinstance(raw_file_path, str) and raw_file_path.strip():
        raw_path = root / raw_file_path
        if not raw_path.is_file():
            errors.append(f"{source_id}: raw_file_path does not exist: {raw_file_path}")
    processed_file_path = source.get("processed_file_path")
    if isinstance(processed_file_path, str) and processed_file_path.strip():
        processed_path = root / processed_file_path
        if not processed_path.exists():
            warnings.append(f"{source_id}: processed_file_path will be generated: {processed_file_path}")

    if source_id == "KOR-004":
        if source.get("original_format") != "pdf":
            errors.append("KOR-004: original_format must be pdf")
        if source.get("stored_format") != "pdf":
            errors.append("KOR-004: stored_format must be pdf")
        serialized = json.dumps(source, ensure_ascii=False)
        if "hwpx" in serialized.casefold() or "converted_by" in serialized:
            errors.append("KOR-004: hwpx/converted_by metadata is not allowed")


def validate_pdf_rag_chunk(chunk: dict[str, Any], *, strict: bool = False) -> list[str]:
    """Validate one parent or child chunk row against the PDF RAG metadata contract."""
    errors: list[str] = []
    for field_name in (
        "chunk_id",
        "source_id",
        "source_type",
        "authority_rank",
        "version_status",
        "section_title",
        "chunk_kind",
        "text",
        "chunk_hash",
        "review_status",
        "extraction_confidence",
    ):
        if field_name not in chunk:
            errors.append(f"missing {field_name}")

    chunk_kind = chunk.get("chunk_kind")
    if chunk_kind not in {"parent", "child"}:
        errors.append(f"invalid chunk_kind {chunk_kind}")
    if chunk_kind == "child" and not chunk.get("parent_chunk_id"):
        errors.append("child chunk requires parent_chunk_id")
    if chunk.get("source_type") not in ALLOWED_SOURCE_TYPES:
        errors.append(f"invalid source_type {chunk.get('source_type')}")
    if chunk.get("version_status") not in {"active", "superseded"}:
        errors.append(f"invalid version_status {chunk.get('version_status')}")
    if chunk.get("review_status") not in ALLOWED_REVIEW_STATUSES:
        errors.append(f"invalid review_status {chunk.get('review_status')}")
    if chunk.get("extraction_confidence") not in ALLOWED_EXTRACTION_CONFIDENCE:
        errors.append(f"invalid extraction_confidence {chunk.get('extraction_confidence')}")
    errors.extend(_validate_allowed_list(chunk, "route_names", ALLOWED_ROUTE_NAMES))
    errors.extend(_validate_allowed_list(chunk, "use_scope", ALLOWED_USE_SCOPES))
    errors.extend(_validate_allowed_list(chunk, "topic_tags", ALLOWED_TOPIC_TAGS))
    if strict and errors:
        raise PdfRagCorpusValidationError("; ".join(errors))
    return errors


def _validate_allowed_list(chunk: dict[str, Any], field_name: str, allowed: set[str]) -> list[str]:
    """Validate that a chunk list field only contains known values."""
    value = chunk.get(field_name, [])
    if value is None:
        return []
    if not isinstance(value, list):
        return [f"{field_name} must be a list"]
    return [f"invalid {field_name}: {item}" for item in value if item not in allowed]


def extract_pdf_page_texts(pdf_path: Path) -> list[tuple[int, str]]:
    """Extract page text from a PDF using pypdf when the dependency is installed."""
    try:
        from pypdf import PdfReader
    except ModuleNotFoundError as exc:
        raise RuntimeError("pypdf is required to extract PDF text") from exc
    reader = PdfReader(str(pdf_path))
    page_texts: list[tuple[int, str]] = []
    for index, page in enumerate(reader.pages, start=1):
        page_texts.append((index, page.extract_text() or ""))
    return page_texts


def write_processed_text(processed_path: Path, source_id: str, page_texts: list[tuple[int, str]]) -> None:
    """Persist extracted page text as markdown-like processed source text."""
    lines = [f"# {source_id}", ""]
    for page_number, text in page_texts:
        lines.append(f"<!-- page:{page_number} -->")
        lines.append(text.strip())
        lines.append("")
    processed_path.parent.mkdir(parents=True, exist_ok=True)
    processed_path.write_text("\n".join(lines).strip() + "\n", encoding="utf-8")


def read_processed_page_texts(processed_path: Path) -> list[tuple[int, str]]:
    """Read processed markdown text and recover page-number blocks."""
    if not processed_path.is_file():
        return []
    content = processed_path.read_text(encoding="utf-8-sig")
    matches = list(re.finditer(r"<!--\s*page:(\d+)\s*-->", content))
    if not matches:
        return [(1, content)]
    page_texts: list[tuple[int, str]] = []
    for index, match in enumerate(matches):
        page_number = int(match.group(1))
        start = match.end()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(content)
        page_texts.append((page_number, content[start:end].strip()))
    return page_texts


def chunk_korean_law_text(
    *,
    source_id: str,
    source_title: str,
    effective_date: str,
    page_texts: list[tuple[int, str]],
) -> list[dict[str, Any]]:
    """Split Korean law text into article parents and paragraph children."""
    merged_text = "\n".join(text for _, text in page_texts)
    article_matches = list(re.finditer(r"제\d+조(?:의\d+)?\s*\([^)]*\)", merged_text))
    chunks: list[dict[str, Any]] = []
    for index, match in enumerate(article_matches):
        article_title = re.sub(r"\s+", "", match.group(0))
        article_number = article_title.split("(", 1)[0]
        start = match.start()
        end = article_matches[index + 1].start() if index + 1 < len(article_matches) else len(merged_text)
        article_text = normalize_text_for_hash(merged_text[start:end])
        if not article_text:
            continue
        page_start = _page_for_offset(page_texts, merged_text, start)
        page_end = _page_for_offset(page_texts, merged_text, max(start, end - 1))
        parent_hierarchy = [article_number, f"article-{index + 1}", f"p{page_start}-{page_end}"]
        parent_id = build_stable_chunk_id(
            source_id=source_id,
            hierarchy_path=parent_hierarchy,
            section_title=article_title,
            text=article_text,
        )
        parent = _base_chunk(
            chunk_id=parent_id,
            parent_chunk_id=None,
            source_id=source_id,
            source_title=source_title,
            source_type="law",
            authority_rank=1,
            section_title=article_title,
            hierarchy_path=parent_hierarchy,
            topic_tags=[],
            use_scope=["legal_basis", "definition"] if "정의" in article_title else ["legal_basis"],
            route_names=["legal_road_operation"],
            chunk_kind="parent",
            text=article_text,
            page_start=page_start,
            page_end=page_end,
            effective_date=effective_date,
        )
        parent["article_number"] = article_number
        chunks.append(parent)
        chunks.extend(
            _build_law_child_chunks(
                source_id=source_id,
                source_title=source_title,
                effective_date=effective_date,
                article_number=article_number,
                article_title=article_title,
                parent_id=parent_id,
                article_text=article_text,
                page_start=parent["page_start"],
                page_end=parent["page_end"],
            )
        )
    return chunks


def _page_for_offset(page_texts: list[tuple[int, str]], merged_text: str, offset: int) -> int:
    """Approximate the source page number for a character offset in merged text."""
    cursor = 0
    for page_number, text in page_texts:
        cursor += len(text) + 1
        if offset <= cursor:
            return page_number
    return page_texts[-1][0] if page_texts else 1


def _build_law_child_chunks(
    *,
    source_id: str,
    source_title: str,
    effective_date: str,
    article_number: str,
    article_title: str,
    parent_id: str,
    article_text: str,
    page_start: int,
    page_end: int,
) -> list[dict[str, Any]]:
    """Create paragraph-level child chunks for one Korean law article."""
    paragraph_matches = list(re.finditer(r"[①②③④⑤⑥⑦⑧⑨⑩]", article_text))
    if not paragraph_matches:
        paragraph_matches = [re.match(r".", article_text)] if article_text else []
    children: list[dict[str, Any]] = []
    for index, match in enumerate(paragraph_matches):
        if match is None:
            continue
        start = match.start()
        end = paragraph_matches[index + 1].start() if index + 1 < len(paragraph_matches) else len(article_text)
        child_text = normalize_text_for_hash(article_text[start:end])
        if not child_text:
            continue
        section_title = f"{article_title} {match.group(0) if match.group(0) else ''}".strip()
        child_hierarchy = [article_number, parent_id, section_title, f"paragraph-{index + 1}", f"p{page_start}-{page_end}"]
        child_id = build_stable_chunk_id(
            source_id=source_id,
            hierarchy_path=child_hierarchy,
            section_title=section_title,
            text=child_text,
        )
        child = _base_chunk(
            chunk_id=child_id,
            parent_chunk_id=parent_id,
            source_id=source_id,
            source_title=source_title,
            source_type="law",
            authority_rank=1,
            section_title=section_title,
            hierarchy_path=child_hierarchy,
            topic_tags=[],
            use_scope=["legal_basis", "definition"] if "정의" in article_title else ["legal_basis"],
            route_names=["legal_road_operation"],
            chunk_kind="child",
            text=child_text,
            page_start=page_start,
            page_end=page_end,
            effective_date=effective_date,
        )
        child["article_number"] = article_number
        children.append(child)
    return children


def chunk_certification_text(source: dict[str, Any], page_texts: list[tuple[int, str]]) -> list[dict[str, Any]]:
    """Chunk KOR certification guide/notice text by section with table QA hints."""
    source_id = str(source["source_id"])
    source_title = str(source.get("description") or source_id)
    source_type = str(source["source_type"])
    authority_rank = int(source["authority_rank"])
    effective_date = str(source.get("effective_date") or "")
    sections = _split_headed_sections(page_texts)
    chunks: list[dict[str, Any]] = []
    for section_index, (section_title, text, page_start, page_end) in enumerate(sections):
        if _is_low_priority_text(section_title, text):
            continue
        topic_tags = _infer_topic_tags(section_title + " " + text)
        use_scope = ["certification_requirement", "safety_rule"]
        route_names = ["safety_certification"]
        if "횡단" in text or "보도" in text:
            route_names.append("crosswalk_sidewalk")
        parent_hierarchy = [section_title, f"section-{section_index + 1}", f"p{page_start}-{page_end}"]
        parent_id = build_stable_chunk_id(
            source_id=source_id,
            hierarchy_path=parent_hierarchy,
            section_title=section_title,
            text=text,
        )
        parent = _base_chunk(
            chunk_id=parent_id,
            parent_chunk_id=None,
            source_id=source_id,
            source_title=source_title,
            source_type=source_type,
            authority_rank=authority_rank,
            section_title=section_title,
            hierarchy_path=parent_hierarchy,
            topic_tags=topic_tags,
            use_scope=use_scope,
            route_names=route_names,
            chunk_kind="parent",
            text=text,
            page_start=page_start,
            page_end=page_end,
            effective_date=effective_date,
        )
        chunks.append(parent)
        for child_index, child_text in enumerate(_split_certification_children(text)):
            child_hierarchy = [
                section_title,
                f"section-{section_index + 1}",
                f"child-{child_index + 1}",
                f"p{page_start}-{page_end}",
                child_text[:40],
            ]
            child_id = build_stable_chunk_id(
                source_id=source_id,
                hierarchy_path=child_hierarchy,
                section_title=section_title,
                text=child_text,
            )
            child = _base_chunk(
                chunk_id=child_id,
                parent_chunk_id=parent_id,
                source_id=source_id,
                source_title=source_title,
                source_type=source_type,
                authority_rank=authority_rank,
                section_title=section_title,
                hierarchy_path=child_hierarchy,
                topic_tags=topic_tags,
                use_scope=use_scope,
                route_names=route_names,
                chunk_kind="child",
                text=child_text,
                page_start=page_start,
                page_end=page_end,
                effective_date=effective_date,
            )
            _apply_table_confidence(child)
            chunks.append(child)
    return chunks


def chunk_research_text(source: dict[str, Any], page_texts: list[tuple[int, str]]) -> list[dict[str, Any]]:
    """Chunk research papers by section while keeping them out of legal routes."""
    source_id = str(source["source_id"])
    source_title = str(source.get("description") or source_id)
    sections = _split_headed_sections(page_texts)
    chunks: list[dict[str, Any]] = []
    for section_index, (section_title, text, page_start, page_end) in enumerate(sections):
        if _is_research_tail(section_title, text):
            continue
        topic_tags = _infer_research_topic_tags(source_id, section_title + " " + text)
        use_scope = _infer_research_use_scope(source_id, section_title + " " + text)
        route_names = _infer_research_routes(source_id, use_scope)
        parent_hierarchy = [section_title, f"section-{section_index + 1}", f"p{page_start}-{page_end}"]
        parent_id = build_stable_chunk_id(
            source_id=source_id,
            hierarchy_path=parent_hierarchy,
            section_title=section_title,
            text=text,
        )
        parent = _base_chunk(
            chunk_id=parent_id,
            parent_chunk_id=None,
            source_id=source_id,
            source_title=source_title,
            source_type="research_paper",
            authority_rank=3,
            section_title=section_title,
            hierarchy_path=parent_hierarchy,
            topic_tags=topic_tags,
            use_scope=use_scope,
            route_names=route_names,
            chunk_kind="parent",
            text=text,
            page_start=page_start,
            page_end=page_end,
            effective_date=str(source.get("effective_date") or ""),
        )
        chunks.append(parent)
        for child_index, child_text in enumerate(_split_paragraph_children(text)):
            child_hierarchy = [
                section_title,
                f"section-{section_index + 1}",
                f"child-{child_index + 1}",
                f"p{page_start}-{page_end}",
                child_text[:40],
            ]
            child_id = build_stable_chunk_id(
                source_id=source_id,
                hierarchy_path=child_hierarchy,
                section_title=section_title,
                text=child_text,
            )
            chunks.append(
                _base_chunk(
                    chunk_id=child_id,
                    parent_chunk_id=parent_id,
                    source_id=source_id,
                    source_title=source_title,
                    source_type="research_paper",
                    authority_rank=3,
                    section_title=section_title,
                    hierarchy_path=child_hierarchy,
                    topic_tags=topic_tags,
                    use_scope=use_scope,
                    route_names=route_names,
                    chunk_kind="child",
                    text=child_text,
                    page_start=page_start,
                    page_end=page_end,
                    effective_date=str(source.get("effective_date") or ""),
                )
            )
    return chunks


def _base_chunk(
    *,
    chunk_id: str,
    parent_chunk_id: str | None,
    source_id: str,
    source_title: str,
    source_type: str,
    authority_rank: int,
    section_title: str,
    hierarchy_path: list[str],
    topic_tags: list[str],
    use_scope: list[str],
    route_names: list[str],
    chunk_kind: str,
    text: str,
    page_start: int,
    page_end: int,
    effective_date: str,
) -> dict[str, Any]:
    """Build a normalized parent or child chunk row."""
    parent_summary = normalize_text_for_hash(text)[:360]
    return {
        "chunk_id": chunk_id,
        "parent_chunk_id": parent_chunk_id,
        "source_id": source_id,
        "source_title": source_title,
        "source_type": source_type,
        "authority_rank": authority_rank,
        "version_status": "active",
        "page_start": page_start,
        "page_end": page_end,
        "section_title": section_title,
        "hierarchy_path": hierarchy_path,
        "topic_tags": topic_tags,
        "use_scope": use_scope,
        "route_names": route_names,
        "chunk_kind": chunk_kind,
        "language": "ko" if source_id.startswith("KOR-") else "en",
        "text": normalize_text_for_hash(text),
        "parent_summary": parent_summary if chunk_kind == "parent" else "",
        "chunk_hash": build_chunk_hash(text),
        "review_status": "auto_validated",
        "extraction_confidence": "high",
        "effective_date": effective_date,
    }


def _split_headed_sections(page_texts: list[tuple[int, str]]) -> list[tuple[str, str, int, int]]:
    """Split processed page text into rough headed sections."""
    sections: list[tuple[str, str, int, int]] = []
    heading_pattern = re.compile(r"^(#{1,4}\s+.+|(?:\d+(?:\.\d+)*)\s+.+|제\d+장\s+.+|별표\s*\d*.*)$")
    for page_number, page_text in page_texts:
        current_title = f"page {page_number}"
        current_lines: list[str] = []
        for raw_line in page_text.splitlines():
            line = raw_line.strip()
            if not line:
                continue
            if heading_pattern.match(line) and current_lines:
                sections.append((current_title, normalize_text_for_hash(" ".join(current_lines)), page_number, page_number))
                current_title = line.lstrip("# ").strip()
                current_lines = []
            elif heading_pattern.match(line):
                current_title = line.lstrip("# ").strip()
            else:
                current_lines.append(line)
        if current_lines:
            sections.append((current_title, normalize_text_for_hash(" ".join(current_lines)), page_number, page_number))
    if not sections and page_texts:
        text = normalize_text_for_hash("\n".join(text for _, text in page_texts))
        sections.append(("document", text, page_texts[0][0], page_texts[-1][0]))
    return [section for section in sections if section[1]]


def _is_low_priority_text(section_title: str, text: str) -> bool:
    """Return true for front matter that should not become default evidence."""
    lowered = f"{section_title} {text}".casefold()
    return any(token in lowered for token in ("목차", "머리말", "contents")) and len(text) < 1200


def _is_research_tail(section_title: str, text: str) -> bool:
    """Return true for reference-like research sections excluded from retrieval corpus."""
    lowered = f"{section_title} {text}".casefold()
    return any(token in lowered for token in ("references", "acknowledgement", "acknowledgment", "author bio"))


def _split_certification_children(text: str) -> list[str]:
    """Split certification text into requirement-sized child chunks."""
    table_rows = [line.strip() for line in text.splitlines() if "|" in line and line.count("|") >= 2]
    if table_rows:
        return table_rows
    return _split_paragraph_children(text)


def _split_paragraph_children(text: str, *, max_chars: int = 1400) -> list[str]:
    """Split text into paragraph children while keeping short meaningful units intact."""
    paragraphs = [normalize_text_for_hash(part) for part in re.split(r"(?:\n{2,}|(?<=다\.)\s+)", text) if part.strip()]
    if not paragraphs:
        return []
    children: list[str] = []
    buffer = ""
    for paragraph in paragraphs:
        candidate = f"{buffer} {paragraph}".strip()
        if buffer and len(candidate) > max_chars:
            children.append(buffer)
            buffer = paragraph
        else:
            buffer = candidate
    if buffer:
        children.append(buffer)
    return children


def _infer_topic_tags(text: str) -> list[str]:
    """Infer certification topic tags from Korean headings and requirement text."""
    tags: list[str] = []
    checks = [
        ("speed_policy", ("속도", "km/h", "운행속도")),
        ("emergency_stop", ("비상정지", "정지")),
        ("obstacle_detection", ("장애물", "충돌")),
        ("perception_requirement", ("인지", "감지", "센서")),
        ("crosswalk_operation", ("횡단보도", "횡단")),
        ("sidewalk_operation", ("보도", "인도")),
        ("operator_control", ("관제", "원격", "조작")),
        ("terrain_or_dynamic_safety", ("경사", "단차", "동적", "전복")),
    ]
    for tag, tokens in checks:
        if any(token in text for token in tokens):
            tags.append(tag)
    return tags[:4]


def _infer_research_topic_tags(source_id: str, text: str) -> list[str]:
    """Infer research-only topic tags from source id and section text."""
    lowered = text.casefold()
    tags: list[str] = []
    if source_id == "RSR-003" or "coverage" in lowered or "combinatorial" in lowered:
        tags.extend(["coverage_model", "constraint_model"])
    if source_id == "RSR-004" or "scenario" in lowered or "scenic" in lowered:
        tags.append("scenario_specification")
    return list(dict.fromkeys(tag for tag in tags if tag in ALLOWED_TOPIC_TAGS))


def _infer_research_use_scope(source_id: str, text: str) -> list[str]:
    """Infer allowed research scopes without granting legal authority."""
    lowered = text.casefold()
    if source_id == "RSR-003":
        return ["experiment_design", "coverage_gap", "next_run_recommendation"]
    if source_id in {"RSR-005", "RSR-006"} or "reward" in lowered or "sim-to-real" in lowered:
        return ["reward_design", "sim_to_real"]
    if source_id in {"RSR-002", "RSR-004"} or "scenario" in lowered:
        return ["experiment_design", "scenario_generation"]
    return ["experiment_design"]


def _infer_research_routes(source_id: str, use_scope: list[str]) -> list[str]:
    """Map research scopes to non-legal retrieval routes."""
    if source_id == "RSR-003" or "coverage_gap" in use_scope:
        return ["experiment_coverage"]
    if "reward_design" in use_scope or "sim_to_real" in use_scope:
        return ["reward_sim_to_real"]
    return ["scenario_generation"]


def _apply_table_confidence(chunk: dict[str, Any]) -> None:
    """Lower confidence when table-like numeric/unit requirements appear incomplete."""
    text = str(chunk.get("text") or "")
    has_unit = bool(re.search(r"\b(?:km/h|m/s|kg|m|cm|deg|°|dB)\b", text, flags=re.IGNORECASE))
    has_number = bool(re.search(r"\d", text))
    looks_table = "|" in text or "\t" in text or len(re.findall(r"\s{2,}", text)) >= 3
    if looks_table and has_number and not has_unit:
        chunk["extraction_confidence"] = "low"


def build_pdf_rag_corpus(
    *,
    inventory_path: Path = ROOT / "data" / "sources" / "source_inventory.json",
    output_dir: Path = ROOT / "data" / "rag" / "pdf_corpus",
    root: Path = ROOT,
) -> PdfRagCorpusBuildResult:
    """Build candidate and validated PDF RAG parent/child corpus artifacts."""
    inventory_result = validate_pdf_rag_source_inventory(inventory_path, root=root)
    if inventory_result.errors:
        raise PdfRagCorpusValidationError("; ".join(inventory_result.errors))

    candidates: list[dict[str, Any]] = []
    warnings = list(inventory_result.warnings)
    for source in inventory_result.sources_by_id.values():
        if source.get("version_status") != "active":
            continue
        page_texts = _load_page_texts_for_source(source, root=root, warnings=warnings)
        if not page_texts:
            warnings.append(f"{source['source_id']}: no processed or extracted text")
            continue
        candidates.extend(_chunk_source(source, page_texts))

    validated: list[dict[str, Any]] = []
    validation_errors: list[str] = []
    for chunk in candidates:
        errors = validate_pdf_rag_chunk(chunk)
        if errors:
            validation_errors.extend(f"{chunk.get('chunk_id', '<unknown>')}: {error}" for error in errors)
            continue
        validated.append(chunk)
    if validation_errors:
        raise PdfRagCorpusValidationError("; ".join(validation_errors))
    duplicate_chunk_ids = [
        chunk_id for chunk_id, count in Counter(str(chunk["chunk_id"]) for chunk in validated).items() if count > 1
    ]
    if duplicate_chunk_ids:
        raise PdfRagCorpusValidationError(f"duplicate chunk_id values: {', '.join(sorted(duplicate_chunk_ids)[:20])}")
    parent_ids = {str(chunk["chunk_id"]) for chunk in validated if chunk.get("chunk_kind") == "parent"}
    missing_parent_refs = [
        str(chunk["chunk_id"])
        for chunk in validated
        if chunk.get("chunk_kind") == "child" and str(chunk.get("parent_chunk_id") or "") not in parent_ids
    ]
    if missing_parent_refs:
        raise PdfRagCorpusValidationError(
            f"child chunks reference missing parents: {', '.join(sorted(missing_parent_refs)[:20])}"
        )

    candidate_path = output_dir / "chunk_candidates.jsonl"
    validated_path = output_dir / "validated_parent_child_chunks.jsonl"
    report_json_path = output_dir / "chunk_build_report.json"
    report_md_path = output_dir / "chunk_build_report.md"
    _write_jsonl(candidate_path, candidates)
    _write_jsonl(validated_path, validated)
    report = _build_report(candidates=candidates, validated=validated, warnings=warnings)
    _write_json(report_json_path, report)
    _write_report_md(report_md_path, report)
    return PdfRagCorpusBuildResult(
        candidate_path=candidate_path,
        validated_path=validated_path,
        report_json_path=report_json_path,
        report_md_path=report_md_path,
        candidate_count=len(candidates),
        validated_count=len(validated),
        warnings=warnings,
    )


def _load_page_texts_for_source(
    source: dict[str, Any],
    *,
    root: Path,
    warnings: list[str],
) -> list[tuple[int, str]]:
    """Load processed text or extract PDF text for one source inventory record."""
    processed_path = root / str(source["processed_file_path"])
    page_texts = read_processed_page_texts(processed_path)
    if page_texts:
        return page_texts
    raw_path = root / str(source["raw_file_path"])
    try:
        page_texts = extract_pdf_page_texts(raw_path)
    except RuntimeError as exc:
        warnings.append(f"{source['source_id']}: {exc}")
        return []
    write_processed_text(processed_path, str(source["source_id"]), page_texts)
    return page_texts


def _chunk_source(source: dict[str, Any], page_texts: list[tuple[int, str]]) -> list[dict[str, Any]]:
    """Dispatch a source to the document-family chunker."""
    source_id = str(source["source_id"])
    if source.get("source_type") == "law":
        return chunk_korean_law_text(
            source_id=source_id,
            source_title=str(source.get("description") or source_id),
            effective_date=str(source.get("effective_date") or ""),
            page_texts=page_texts,
        )
    if source_id in {"KOR-003", "KOR-004"}:
        return chunk_certification_text(source, page_texts)
    if str(source_id).startswith("RSR-"):
        return chunk_research_text(source, page_texts)
    return []


def _build_report(
    *,
    candidates: list[dict[str, Any]],
    validated: list[dict[str, Any]],
    warnings: list[str],
) -> dict[str, Any]:
    """Build a human-reviewable summary for generated corpus artifacts."""
    child_chunks = [chunk for chunk in validated if chunk.get("chunk_kind") == "child"]
    return {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "candidate_count": len(candidates),
        "validated_count": len(validated),
        "child_chunk_count": len(child_chunks),
        "chunks_by_source": dict(sorted(Counter(chunk["source_id"] for chunk in validated).items())),
        "chunks_by_route": dict(
            sorted(Counter(route for chunk in child_chunks for route in chunk.get("route_names", [])).items())
        ),
        "low_confidence_chunks": [
            chunk["chunk_id"] for chunk in validated if chunk.get("extraction_confidence") == "low"
        ],
        "warnings": warnings,
        "review_guidance": [
            "validated_parent_child_chunks.jsonl은 자동 생성 산출물이다.",
            "사람 검수는 이 report와 low_confidence/sample chunk 확인으로 제한한다.",
        ],
    }


def _write_report_md(path: Path, report: dict[str, Any]) -> None:
    """Write a concise markdown report for manual sample review."""
    lines = [
        "# PDF RAG Corpus Build Report",
        "",
        f"- generated_at: {report['generated_at']}",
        f"- candidate_count: {report['candidate_count']}",
        f"- validated_count: {report['validated_count']}",
        f"- child_chunk_count: {report['child_chunk_count']}",
        "",
        "## Chunks By Source",
    ]
    lines.extend(f"- {source_id}: {count}" for source_id, count in report["chunks_by_source"].items())
    lines.extend(["", "## Chunks By Route"])
    lines.extend(f"- {route}: {count}" for route, count in report["chunks_by_route"].items())
    lines.extend(["", "## Low Confidence Chunks"])
    low_confidence = report["low_confidence_chunks"] or ["(none)"]
    lines.extend(f"- {item}" for item in low_confidence)
    lines.extend(["", "## Warnings"])
    warnings = report["warnings"] or ["(none)"]
    lines.extend(f"- {item}" for item in warnings)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    """Create the CLI parser for building PDF RAG corpus artifacts."""
    parser = argparse.ArgumentParser(description="Build PDF Vector Hybrid RAG corpus artifacts.")
    parser.add_argument("--inventory", type=Path, default=ROOT / "data" / "sources" / "source_inventory.json")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "data" / "rag" / "pdf_corpus")
    return parser


def main(argv: list[str] | None = None) -> int:
    """Run the PDF RAG corpus builder from the command line."""
    args = build_parser().parse_args(argv)
    result = build_pdf_rag_corpus(inventory_path=args.inventory, output_dir=args.output_dir)
    print(f"candidate chunks: {result.candidate_count}")
    print(f"validated chunks: {result.validated_count}")
    print(f"report: {result.report_json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
