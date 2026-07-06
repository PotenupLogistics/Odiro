from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
RAG_DIR = Path("data") / "rag"
RUNTIME_CHUNKS_FILE = RAG_DIR / "policy_rag_chunks.jsonl"
KNOWLEDGE_CARDS_FILE = RAG_DIR / "policy_knowledge_cards.jsonl"
SOURCE_INVENTORY_FILE = Path("data") / "sources" / "source_inventory.json"
PDF_RAG_SOURCE_INVENTORY_SCHEMA = "pdf_vector_hybrid_rag_source_inventory"
EXPECTED_RUNTIME_CHUNK_COUNT = 17
EXPECTED_KNOWLEDGE_CARD_COUNT = 11
REQUIRED_SOURCE_IDS = (
    "KOR-003",
    "KOR-004",
    "RSR-001",
    "KOR-001",
    "KOR-002",
    "KOR-005",
    "PRJ-AGENT",
    "PRJ-DOE",
    "PRJ-EVAL",
)
ALLOWED_SOURCE_STATUSES = (
    "active",
    "candidate_active",
    "supporting_candidate",
    "reference_only",
    "review_candidate",
    "active_internal",
)
RUNTIME_ALLOWED_SOURCE_STATUSES = ("active", "active_internal")
LEGACY_INTERNAL_SOURCE_IDS = ("PRJ-AGENT", "PRJ-DOE", "PRJ-EVAL")
FORBIDDEN_VECTOR_DB_DIRS = (
    RAG_DIR / "embeddings",
    RAG_DIR / "vector_db",
    RAG_DIR / "chroma",
)

CHUNK_TOP_LEVEL_FIELDS = ("chunkId", "cardId", "chunkText", "metadata")
CHUNK_METADATA_FIELDS = (
    "sourceIds",
    "category",
    "relatedPolicyParams",
    "relatedRequestFields",
    "relatedActions",
    "relatedMetrics",
    "evidenceLocation",
    "createdFromCandidateId",
    "status",
)


@dataclass(frozen=True)
class ValidationResult:
    runtime_chunk_file_exists: bool
    runtime_chunk_count: int
    knowledge_card_file_exists: bool
    knowledge_card_count: int
    source_inventory_exists: bool
    source_count: int
    source_ids: list[str]
    active_sources: list[str]
    candidate_sources: list[str]
    reference_only_sources: list[str]
    runtime_source_status_guard_passed: bool
    runtime_allowed_statuses: list[str]
    runtime_source_ids_used_by_chunks: list[str]
    candidate_reference_sources_excluded_from_runtime_chunks: bool
    vector_db_directories_absent: bool
    errors: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.errors


def _display_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _read_jsonl(path: Path, root: Path, errors: list[str]) -> list[tuple[int, dict[str, Any]]]:
    display_path = _display_path(path, root)
    if not path.exists():
        errors.append(f"{display_path}: missing file")
        return []

    rows: list[tuple[int, dict[str, Any]]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        if not line.strip():
            errors.append(f"{display_path} line {line_number}: empty line is not allowed")
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            errors.append(f"{display_path} line {line_number}: invalid JSON ({exc.msg})")
            continue
        if not isinstance(value, dict):
            errors.append(f"{display_path} line {line_number}: JSONL row must be an object")
            continue
        rows.append((line_number, value))
    return rows


def _read_json(path: Path, root: Path, errors: list[str]) -> dict[str, Any]:
    display_path = _display_path(path, root)
    if not path.exists():
        errors.append(f"{display_path}: missing file")
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        errors.append(f"{display_path}: invalid JSON ({exc.msg})")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{display_path}: JSON root must be an object")
        return {}
    return value


def _require_non_empty_string(
    value: object,
    field_name: str,
    line_number: int,
    display_path: str,
    errors: list[str],
) -> None:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{display_path} line {line_number}: {field_name} must be a non-empty string")


def _require_list(
    value: object,
    field_name: str,
    line_number: int,
    display_path: str,
    errors: list[str],
    require_non_empty: bool = False,
) -> None:
    if not isinstance(value, list):
        errors.append(f"{display_path} line {line_number}: {field_name} must be a list")
        return
    if require_non_empty and not value:
        errors.append(f"{display_path} line {line_number}: {field_name} must not be empty")


def _validate_chunks(
    rows: list[tuple[int, dict[str, Any]]],
    path: Path,
    root: Path,
    expected_count: int,
    errors: list[str],
) -> dict[str, list[int]]:
    display_path = _display_path(path, root)
    chunk_source_lines: dict[str, list[int]] = {}
    if len(rows) != expected_count:
        errors.append(f"{display_path}: expected {expected_count} chunks, found {len(rows)}")

    for line_number, chunk in rows:
        for field_name in CHUNK_TOP_LEVEL_FIELDS:
            if field_name not in chunk:
                errors.append(f"{display_path} line {line_number}: missing {field_name}")

        _require_non_empty_string(chunk.get("chunkId"), "chunkId", line_number, display_path, errors)
        _require_non_empty_string(chunk.get("cardId"), "cardId", line_number, display_path, errors)
        _require_non_empty_string(chunk.get("chunkText"), "chunkText", line_number, display_path, errors)

        metadata = chunk.get("metadata")
        if not isinstance(metadata, dict):
            errors.append(f"{display_path} line {line_number}: metadata must be an object")
            continue

        for field_name in CHUNK_METADATA_FIELDS:
            if field_name not in metadata:
                errors.append(f"{display_path} line {line_number}: missing metadata.{field_name}")

        _require_list(
            metadata.get("sourceIds"),
            "metadata.sourceIds",
            line_number,
            display_path,
            errors,
            require_non_empty=True,
        )
        if isinstance(metadata.get("sourceIds"), list):
            for source_id in metadata["sourceIds"]:
                if isinstance(source_id, str) and source_id.strip():
                    chunk_source_lines.setdefault(source_id, []).append(line_number)
        _require_non_empty_string(
            metadata.get("category"),
            "metadata.category",
            line_number,
            display_path,
            errors,
        )
        _require_list(
            metadata.get("relatedActions"),
            "metadata.relatedActions",
            line_number,
            display_path,
            errors,
        )
        _require_list(
            metadata.get("relatedPolicyParams"),
            "metadata.relatedPolicyParams",
            line_number,
            display_path,
            errors,
        )
    return chunk_source_lines


def _validate_cards(
    rows: list[tuple[int, dict[str, Any]]],
    path: Path,
    root: Path,
    expected_count: int,
    errors: list[str],
) -> None:
    display_path = _display_path(path, root)
    if len(rows) != expected_count:
        errors.append(f"{display_path}: expected {expected_count} knowledge cards, found {len(rows)}")

    for line_number, card in rows:
        source_ids = card.get("sourceIds")
        _require_list(
            source_ids,
            "sourceIds",
            line_number,
            display_path,
            errors,
            require_non_empty=True,
        )
        if isinstance(source_ids, list) and any(source_id not in {"KOR-003", "KOR-004"} for source_id in source_ids):
            errors.append(
                f"{display_path} line {line_number}: current knowledge cards must remain KOR-003 or KOR-004 based"
            )
        _require_non_empty_string(card.get("cardId"), "cardId", line_number, display_path, errors)
        _require_non_empty_string(card.get("category"), "category", line_number, display_path, errors)


def _validate_source_inventory(
    inventory: dict[str, Any],
    path: Path,
    root: Path,
    chunk_source_lines: dict[str, list[int]],
    errors: list[str],
) -> tuple[list[str], list[str], list[str], list[str], bool, bool]:
    display_path = _display_path(path, root)
    if not inventory:
        return [], [], [], [], False, False

    schema = inventory.get("schema")
    is_pdf_rag_inventory = schema == PDF_RAG_SOURCE_INVENTORY_SCHEMA
    if "schema" not in inventory:
        errors.append(f"{display_path}: missing schema")
    elif schema not in {"file_based_rag_source_inventory", PDF_RAG_SOURCE_INVENTORY_SCHEMA}:
        errors.append(
            f"{display_path}: schema must be file_based_rag_source_inventory or {PDF_RAG_SOURCE_INVENTORY_SCHEMA}"
        )

    if "version" not in inventory:
        errors.append(f"{display_path}: missing version")

    sources = inventory.get("sources")
    if not isinstance(sources, list):
        errors.append(f"{display_path}: sources must be a list")
        return [], [], [], [], False, False

    seen: set[str] = set()
    source_ids: list[str] = []
    status_by_source_id: dict[str, str] = {}
    for index, source in enumerate(sources, start=1):
        if not isinstance(source, dict):
            errors.append(f"{display_path} sources[{index}]: source must be an object")
            continue
        source_id = source.get("source_id")
        if not isinstance(source_id, str) or not source_id.strip():
            errors.append(f"{display_path} sources[{index}]: source_id must be a non-empty string")
            continue
        if source_id in seen:
            errors.append(f"{display_path}: duplicate source_id: {source_id}")
        seen.add(source_id)
        source_ids.append(source_id)

        if not is_pdf_rag_inventory:
            for field_name in ("status", "role", "usage"):
                value = source.get(field_name)
                if not isinstance(value, str) or not value.strip():
                    errors.append(f"{display_path} source_id {source_id}: {field_name} must be a non-empty string")

        status = source.get("status") or source.get("version_status")
        if isinstance(status, str):
            status_by_source_id[source_id] = status
            if not is_pdf_rag_inventory and status not in ALLOWED_SOURCE_STATUSES:
                errors.append(f"{display_path} source_id {source_id}: invalid status {status}")
            if is_pdf_rag_inventory and status not in {"active", "superseded"}:
                errors.append(f"{display_path} source_id {source_id}: invalid version_status {status}")

        for path_field in ("raw_file_path", "processed_file_path"):
            value = source.get(path_field)
            if (
                isinstance(value, str)
                and value.strip()
                and not (root / value).exists()
                and not (is_pdf_rag_inventory and path_field == "processed_file_path")
            ):
                errors.append(f"{display_path} source_id {source_id}: {path_field} does not exist: {value}")

    if is_pdf_rag_inventory:
        for legacy_source_id in LEGACY_INTERNAL_SOURCE_IDS:
            status_by_source_id.setdefault(legacy_source_id, "active_internal")
    else:
        for required_source_id in REQUIRED_SOURCE_IDS:
            if required_source_id not in seen:
                errors.append(f"{display_path}: missing required source_id: {required_source_id}")

    if not is_pdf_rag_inventory and "KOR-003" not in seen:
        errors.append(f"{display_path}: missing active runtime source KOR-003")

    virtual_source_ids = set(LEGACY_INTERNAL_SOURCE_IDS) if is_pdf_rag_inventory else set()
    unregistered_chunk_sources = sorted(
        source_id for source_id in chunk_source_lines if source_id not in seen and source_id not in virtual_source_ids
    )
    if unregistered_chunk_sources:
        errors.append(
            f"{display_path}: chunk sourceIds not registered in source inventory: "
            + ", ".join(unregistered_chunk_sources)
        )

    runtime_source_status_guard_passed = True
    for source_id in sorted(chunk_source_lines):
        status = status_by_source_id.get(source_id)
        if status is None or status in RUNTIME_ALLOWED_SOURCE_STATUSES:
            continue
        runtime_source_status_guard_passed = False
        for line_number in chunk_source_lines[source_id]:
            errors.append(
                f"chunk line {line_number} uses source_id {source_id} with status {status}, "
                "but runtime chunks only allow active or active_internal"
            )

    if not is_pdf_rag_inventory and status_by_source_id.get("KOR-004") != "active":
        errors.append(f"{display_path} source_id KOR-004: status must be active after confirmed runtime promotion")
    if not is_pdf_rag_inventory and status_by_source_id.get("RSR-001") != "supporting_candidate":
        errors.append(f"{display_path} source_id RSR-001: status must remain supporting_candidate")

    active_sources = [source_id for source_id in source_ids if status_by_source_id.get(source_id) == "active"]
    candidate_sources = [
        source_id
        for source_id in source_ids
        if status_by_source_id.get(source_id) in {"candidate_active", "supporting_candidate"}
    ]
    reference_only_sources = [
        source_id for source_id in source_ids if status_by_source_id.get(source_id) == "reference_only"
    ]
    candidate_reference_sources_excluded = runtime_source_status_guard_passed
    return (
        source_ids,
        active_sources,
        candidate_sources,
        reference_only_sources,
        runtime_source_status_guard_passed,
        candidate_reference_sources_excluded,
    )


def validate_file_based_rag_store(
    root: Path = ROOT,
    expected_runtime_chunk_count: int = EXPECTED_RUNTIME_CHUNK_COUNT,
    expected_knowledge_card_count: int = EXPECTED_KNOWLEDGE_CARD_COUNT,
) -> ValidationResult:
    root = root.resolve()
    errors: list[str] = []
    runtime_chunks_path = root / RUNTIME_CHUNKS_FILE
    knowledge_cards_path = root / KNOWLEDGE_CARDS_FILE
    source_inventory_path = root / SOURCE_INVENTORY_FILE

    chunk_rows = _read_jsonl(runtime_chunks_path, root, errors)
    card_rows = _read_jsonl(knowledge_cards_path, root, errors)
    source_inventory = _read_json(source_inventory_path, root, errors)

    chunk_source_lines: dict[str, list[int]] = {}
    if runtime_chunks_path.exists():
        chunk_source_lines = _validate_chunks(
            chunk_rows,
            runtime_chunks_path,
            root,
            expected_runtime_chunk_count,
            errors,
        )
    if knowledge_cards_path.exists():
        _validate_cards(
            card_rows,
            knowledge_cards_path,
            root,
            expected_knowledge_card_count,
            errors,
        )
    source_ids: list[str] = []
    active_sources: list[str] = []
    candidate_sources: list[str] = []
    reference_only_sources: list[str] = []
    runtime_source_status_guard_passed = False
    candidate_reference_sources_excluded = False
    if source_inventory_path.exists():
        (
            source_ids,
            active_sources,
            candidate_sources,
            reference_only_sources,
            runtime_source_status_guard_passed,
            candidate_reference_sources_excluded,
        ) = _validate_source_inventory(
            source_inventory,
            source_inventory_path,
            root,
            chunk_source_lines,
            errors,
        )

    vector_db_directories_absent = True
    for forbidden_dir in FORBIDDEN_VECTOR_DB_DIRS:
        path = root / forbidden_dir
        if path.exists():
            vector_db_directories_absent = False
            errors.append(f"{_display_path(path, root)} must be absent in the current file-based runtime")

    return ValidationResult(
        runtime_chunk_file_exists=runtime_chunks_path.exists(),
        runtime_chunk_count=len(chunk_rows),
        knowledge_card_file_exists=knowledge_cards_path.exists(),
        knowledge_card_count=len(card_rows),
        source_inventory_exists=source_inventory_path.exists(),
        source_count=len(source_ids),
        source_ids=source_ids,
        active_sources=active_sources,
        candidate_sources=candidate_sources,
        reference_only_sources=reference_only_sources,
        runtime_source_status_guard_passed=runtime_source_status_guard_passed,
        runtime_allowed_statuses=list(RUNTIME_ALLOWED_SOURCE_STATUSES),
        runtime_source_ids_used_by_chunks=sorted(chunk_source_lines),
        candidate_reference_sources_excluded_from_runtime_chunks=candidate_reference_sources_excluded,
        vector_db_directories_absent=vector_db_directories_absent,
        errors=errors,
    )


def _print_summary(result: ValidationResult) -> None:
    print(f"runtime chunk file: {'OK' if result.runtime_chunk_file_exists else 'MISSING'}")
    print(f"chunk count: {result.runtime_chunk_count}")
    print(f"knowledge card file: {'OK' if result.knowledge_card_file_exists else 'MISSING'}")
    print(f"knowledge card count: {result.knowledge_card_count}")
    print(f"source inventory: {'OK' if result.source_inventory_exists else 'MISSING'}")
    print(f"source count: {result.source_count}")
    print(f"active sources: {', '.join(result.active_sources) or '(none)'}")
    print(f"candidate sources: {', '.join(result.candidate_sources) or '(none)'}")
    print(f"reference-only sources: {', '.join(result.reference_only_sources) or '(none)'}")
    print(
        "runtime source status guard: "
        + ("OK" if result.runtime_source_status_guard_passed else "FAIL")
    )
    print(f"runtime allowed statuses: {', '.join(result.runtime_allowed_statuses)}")
    print(
        "runtime source ids used by chunks: "
        + (", ".join(result.runtime_source_ids_used_by_chunks) or "(none)")
    )
    print(
        "candidate/reference sources excluded from runtime chunks: "
        + (
            "OK"
            if result.candidate_reference_sources_excluded_from_runtime_chunks
            else "FAIL"
        )
    )
    print(
        "vector db directories: "
        + ("absent" if result.vector_db_directories_absent else "present")
    )
    if result.errors:
        print("errors:")
        for error in result.errors:
            print(f"- {error}")
    print(f"result: {'PASS' if result.passed else 'FAIL'}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate the file-based policy RAG store.")
    parser.add_argument(
        "--root",
        default=str(ROOT),
        help="Repository root. Defaults to the current Proto-AI checkout.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    result = validate_file_based_rag_store(Path(args.root))
    _print_summary(result)
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
