"""Validate PDF RAG source inventory and generated corpus artifacts."""

from __future__ import annotations

import argparse
from collections import Counter
import sys
from pathlib import Path


# Agents root must be importable when this file runs as a script.
ROOT_FOR_IMPORTS = Path(__file__).resolve().parents[1]
if str(ROOT_FOR_IMPORTS) not in sys.path:
    sys.path.insert(0, str(ROOT_FOR_IMPORTS))

from app.services.pdf_rag_corpus import ROOT, read_jsonl, validate_pdf_rag_chunk, validate_pdf_rag_source_inventory


# Default source inventory path for PDF RAG.
DEFAULT_INVENTORY = ROOT / "data" / "sources" / "source_inventory.json"

# Default validated corpus path for PDF RAG.
DEFAULT_VALIDATED_CHUNKS = ROOT / "data" / "rag" / "pdf_corpus" / "validated_parent_child_chunks.jsonl"


def build_parser() -> argparse.ArgumentParser:
    """Create the CLI parser for validating PDF RAG artifacts."""
    parser = argparse.ArgumentParser(description="Validate PDF RAG source inventory and chunk corpus.")
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--chunks", type=Path, default=DEFAULT_VALIDATED_CHUNKS)
    parser.add_argument("--allow-missing-chunks", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    """Run PDF RAG inventory and chunk validation."""
    args = build_parser().parse_args(argv)
    inventory = validate_pdf_rag_source_inventory(args.inventory, strict=False)
    errors = list(inventory.errors)
    warnings = list(inventory.warnings)
    chunk_count = 0
    if args.chunks.is_file():
        chunks = read_jsonl(args.chunks)
        chunk_count = len(chunks)
        for chunk in chunks:
            errors.extend(validate_pdf_rag_chunk(chunk))
        if chunk_count == 0 and not args.allow_missing_chunks:
            errors.append(f"empty validated chunks: {args.chunks}")
        duplicate_chunk_ids = [
            chunk_id for chunk_id, count in Counter(str(chunk.get("chunk_id")) for chunk in chunks).items() if count > 1
        ]
        errors.extend(f"duplicate chunk_id: {chunk_id}" for chunk_id in sorted(duplicate_chunk_ids))
        parent_ids = {str(chunk.get("chunk_id")) for chunk in chunks if chunk.get("chunk_kind") == "parent"}
        for chunk in chunks:
            if chunk.get("chunk_kind") == "child" and str(chunk.get("parent_chunk_id") or "") not in parent_ids:
                errors.append(f"missing parent for child chunk: {chunk.get('chunk_id')}")
    elif not args.allow_missing_chunks:
        errors.append(f"missing validated chunks: {args.chunks}")

    print(f"source inventory: {'OK' if inventory.passed else 'FAIL'}")
    print(f"source count: {len(inventory.sources_by_id)}")
    print(f"validated chunk count: {chunk_count}")
    for warning in warnings:
        print(f"warning: {warning}")
    for error in errors:
        print(f"error: {error}")
    print(f"result: {'PASS' if not errors else 'FAIL'}")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
