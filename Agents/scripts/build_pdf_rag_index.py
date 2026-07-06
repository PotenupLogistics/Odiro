"""CLI for building the local PDF Vector Hybrid RAG index cache."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


# Agents root must be importable when this file runs as a script.
ROOT_FOR_IMPORTS = Path(__file__).resolve().parents[1]
if str(ROOT_FOR_IMPORTS) not in sys.path:
    sys.path.insert(0, str(ROOT_FOR_IMPORTS))

from app.services.pdf_rag_corpus import ROOT
from app.services.pdf_rag_index import (
    OpenAIEmbeddingClient,
    PdfRagEmbeddingError,
    build_pdf_rag_index_atomic,
    check_pdf_rag_index,
)


# Default validated corpus path produced by the PDF RAG corpus build.
DEFAULT_CHUNK_FILE = ROOT / "data" / "rag" / "pdf_corpus" / "validated_parent_child_chunks.jsonl"

# Default local vector cache path; this must stay outside tracked data/rag.
DEFAULT_ACTIVE_DIR = ROOT / ".cache" / "rag" / "chroma" / "pdf_corpus"


def build_parser() -> argparse.ArgumentParser:
    """Create the CLI parser for the PDF RAG index builder."""
    parser = argparse.ArgumentParser(description="Build the local PDF RAG Chroma cache.")
    parser.add_argument("--chunk-file", type=Path, default=DEFAULT_CHUNK_FILE)
    parser.add_argument("--active-dir", type=Path, default=DEFAULT_ACTIVE_DIR)
    parser.add_argument("--model", default=os.environ.get("PDF_RAG_EMBEDDING_MODEL", "text-embedding-3-small"))
    parser.add_argument("--timeout-sec", type=float, default=float(os.environ.get("PDF_RAG_QUERY_TIMEOUT_SEC", "5")))
    parser.add_argument("--max-retries", type=int, default=int(os.environ.get("PDF_RAG_QUERY_MAX_RETRIES", "1")))
    parser.add_argument("--collection-name", default="pdf_rag_corpus")
    parser.add_argument("--check-only", action="store_true", help="Check active index freshness without API calls.")
    return parser


def main(argv: list[str] | None = None) -> int:
    """Build and promote the local PDF RAG vector cache."""
    args = build_parser().parse_args(argv)
    if args.check_only:
        check = check_pdf_rag_index(
            chunk_file=args.chunk_file,
            active_dir=args.active_dir,
            expected_embedding_model=args.model,
            collection_name=args.collection_name,
        )
        print(check.message)
        if check.expected_child_count is not None:
            print(f"expected child chunks: {check.expected_child_count}")
        if check.actual_collection_count is not None:
            print(f"chroma collection count: {check.actual_collection_count}")
        return check.exit_code

    client = OpenAIEmbeddingClient(
        api_key=os.environ.get("OPENAI_API_KEY", ""),
        model=args.model,
        timeout_sec=args.timeout_sec,
        max_retries=args.max_retries,
    )
    try:
        result = build_pdf_rag_index_atomic(
            chunk_file=args.chunk_file,
            active_dir=args.active_dir,
            embedding_client=client,
            collection_name=args.collection_name,
        )
    except PdfRagEmbeddingError as exc:
        if exc.embedding_error_type == "api_key_missing":
            print("[PDF RAG] OPENAI_API_KEY is not set. Cannot build the local Chroma index.")
            print("[PDF RAG] To build later: cd Agents; uv run python scripts/build_pdf_rag_index.py")
            return 20
        print(f"[PDF RAG] Embedding failed: {exc.embedding_error_type}.")
        return 21
    except Exception as exc:
        print(f"[PDF RAG] Chroma index build failed: {exc.__class__.__name__}.")
        return 1
    print(f"promoted: {result.promoted}")
    print(f"embedded child chunks: {result.embedded_child_count}")
    print(f"chroma collection count: {result.collection_count}")
    print(f"manifest: {result.manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
