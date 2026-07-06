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
from app.services.pdf_rag_index import OpenAIEmbeddingClient, build_pdf_rag_index_atomic


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
    return parser


def main(argv: list[str] | None = None) -> int:
    """Build and promote the local PDF RAG vector cache."""
    args = build_parser().parse_args(argv)
    client = OpenAIEmbeddingClient(
        api_key=os.environ.get("OPENAI_API_KEY", ""),
        model=args.model,
        timeout_sec=args.timeout_sec,
        max_retries=args.max_retries,
    )
    result = build_pdf_rag_index_atomic(
        chunk_file=args.chunk_file,
        active_dir=args.active_dir,
        embedding_client=client,
        collection_name=args.collection_name,
    )
    print(f"promoted: {result.promoted}")
    print(f"embedded child chunks: {result.embedded_child_count}")
    print(f"chroma collection count: {result.collection_count}")
    print(f"manifest: {result.manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
