"""Embedding and Chroma index build helpers for PDF Vector Hybrid RAG."""

from __future__ import annotations

import gc
import json
import shutil
from dataclasses import dataclass, field
from datetime import datetime, timezone
from importlib import import_module
from pathlib import Path
from typing import Any, Protocol

from app.services.pdf_rag_corpus import (
    ALLOWED_ROUTE_NAMES,
    PDF_RAG_CHUNKER_VERSION,
    PDF_RAG_EXTRACTOR_VERSION,
    PDF_RAG_METADATA_SCHEMA_VERSION,
    PDF_RAG_TABLE_PARSER_VERSION,
    build_file_hash,
    read_jsonl,
)


# Version for the synonym expansion dictionary paired with the active index.
PDF_RAG_SYNONYM_VERSION = "pdf-rag-synonyms-v1"

# Default embedding provider used by the production index build.
DEFAULT_EMBEDDING_PROVIDER = "openai"

# Default OpenAI embedding model; callers may override via environment/settings.
DEFAULT_EMBEDDING_MODEL = "text-embedding-3-small"

# Default child chunk batch size for embedding requests to stay below provider request limits.
DEFAULT_EMBEDDING_BATCH_SIZE = 64

# Exit code returned when the active PDF RAG Chroma index is current.
PDF_RAG_INDEX_READY = 0

# Exit code returned when no active PDF RAG Chroma index manifest exists.
PDF_RAG_INDEX_MISSING = 10

# Exit code returned when the active PDF RAG Chroma index no longer matches the corpus or model.
PDF_RAG_INDEX_STALE = 11

# Exit code returned when the active PDF RAG Chroma manifest cannot be read safely.
PDF_RAG_INDEX_MANIFEST_INVALID = 12

# Exit code returned when the active Chroma collection count does not match child chunks.
PDF_RAG_INDEX_COLLECTION_MISMATCH = 13


class PdfRagEmbeddingError(RuntimeError):
    """Embedding failure with internal diagnostic categories for RAG unavailable responses."""

    def __init__(
        self,
        *,
        rag_error_type: str,
        embedding_error_type: str,
        detail: str | None = None,
    ) -> None:
        """Store error categories without exposing provider payloads."""
        self.rag_error_type = rag_error_type
        self.embedding_error_type = embedding_error_type
        self.detail = detail
        message = detail or embedding_error_type
        super().__init__(message)


class EmbeddingClient(Protocol):
    """Minimal embedding interface used by the index builder and runtime retriever."""

    @property
    def provider(self) -> str:
        """Return the embedding provider name."""

    @property
    def model(self) -> str:
        """Return the embedding model name."""

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """Embed child chunk texts or runtime queries for vector search."""


@dataclass
class FakeEmbeddingClient:
    """Deterministic embedding client for unit tests."""

    fail: bool = False
    provider: str = DEFAULT_EMBEDDING_PROVIDER
    model: str = DEFAULT_EMBEDDING_MODEL
    embedded_texts: list[str] = field(default_factory=list)

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """Record texts and return simple deterministic vectors."""
        if self.fail:
            raise RuntimeError("fake embedding failure")
        self.embedded_texts.extend(texts)
        return [[float(len(text)), float(sum(ord(ch) for ch in text) % 997)] for text in texts]


class OpenAIEmbeddingClient:
    """OpenAI embedding client with bounded timeout and retry policy."""

    def __init__(
        self,
        *,
        api_key: str,
        model: str = DEFAULT_EMBEDDING_MODEL,
        timeout_sec: float = 5.0,
        max_retries: int = 1,
    ) -> None:
        """Configure the OpenAI embedding client for index build or query embedding."""
        self._api_key = api_key
        self._model = model
        self._timeout_sec = timeout_sec
        self._max_retries = max(0, min(max_retries, 1))

    @property
    def provider(self) -> str:
        """Return the embedding provider name."""
        return DEFAULT_EMBEDDING_PROVIDER

    @property
    def model(self) -> str:
        """Return the configured embedding model."""
        return self._model

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """Embed texts through the OpenAI API with at most one retry."""
        if not self._api_key:
            raise PdfRagEmbeddingError(
                rag_error_type="query_embedding_failure",
                embedding_error_type="api_key_missing",
                detail="OPENAI_API_KEY is required for PDF RAG embedding",
            )
        try:
            openai_module = import_module("openai")
            client_type = getattr(openai_module, "OpenAI")
            client = client_type(api_key=self._api_key, timeout=self._timeout_sec, max_retries=self._max_retries)
            response = client.embeddings.create(model=self._model, input=texts)
        except Exception as exc:
            error_name = exc.__class__.__name__.casefold()
            if "timeout" in error_name:
                raise PdfRagEmbeddingError(
                    rag_error_type="embedding_timeout",
                    embedding_error_type="embedding_timeout",
                    detail=exc.__class__.__name__,
                ) from exc
            raise PdfRagEmbeddingError(
                rag_error_type="query_embedding_failure",
                embedding_error_type="provider_error",
                detail=exc.__class__.__name__,
            ) from exc
        return [list(item.embedding) for item in response.data]


@dataclass(frozen=True)
class PdfRagIndexBuildResult:
    """Summary returned after an atomic local index build."""

    promoted: bool
    active_dir: Path
    manifest_path: Path
    embedded_child_count: int
    collection_count: int


@dataclass(frozen=True)
class PdfRagIndexCheckResult:
    """Status returned by check-only PDF RAG index validation."""

    exit_code: int
    status: str
    message: str
    expected_child_count: int | None = None
    actual_collection_count: int | None = None


def build_chroma_manifest(
    *,
    chunk_file: Path,
    chunks: list[dict[str, Any]],
    embedding_provider: str,
    embedding_model: str,
    collection_name: str,
    source_hashes: dict[str, str],
) -> dict[str, Any]:
    """Build the manifest paired with one active Chroma index."""
    child_chunks = [chunk for chunk in chunks if chunk.get("chunk_kind") == "child"]
    return {
        "corpus_version": "pdf-rag-corpus-v1",
        "chunk_file_hash": build_file_hash(chunk_file),
        "chunk_count": len(chunks),
        "embedded_child_count": len(child_chunks),
        "embedding_provider": embedding_provider,
        "embedding_model": embedding_model,
        "collection_name": collection_name,
        "build_time": datetime.now(timezone.utc).isoformat(),
        "source_hashes": source_hashes,
        "validated_chunk_file_path": str(chunk_file),
        "extractor_version": PDF_RAG_EXTRACTOR_VERSION,
        "chunker_version": PDF_RAG_CHUNKER_VERSION,
        "table_parser_version": PDF_RAG_TABLE_PARSER_VERSION,
        "synonym_version": PDF_RAG_SYNONYM_VERSION,
        "metadata_schema_version": PDF_RAG_METADATA_SCHEMA_VERSION,
    }


def build_pdf_rag_index_atomic(
    *,
    chunk_file: Path,
    active_dir: Path,
    embedding_client: EmbeddingClient,
    collection_name: str = "pdf_rag_corpus",
    source_hashes: dict[str, str] | None = None,
    run_smoke_tests: bool = True,
    embedding_batch_size: int = DEFAULT_EMBEDDING_BATCH_SIZE,
) -> PdfRagIndexBuildResult:
    """Build a Chroma index in a tmp dir and promote it atomically on success."""
    chunks = read_jsonl(chunk_file)
    child_chunks = [chunk for chunk in chunks if chunk.get("chunk_kind") == "child"]
    if not child_chunks:
        raise RuntimeError("PDF RAG Chroma build requires at least one child chunk")
    tmp_dir = active_dir.parent / f"{active_dir.name}.tmp"
    backup_dir = active_dir.parent / f"{active_dir.name}.bak"
    if tmp_dir.exists():
        shutil.rmtree(tmp_dir)
    tmp_dir.mkdir(parents=True, exist_ok=True)
    try:
        texts = [str(chunk.get("text") or "") for chunk in child_chunks]
        vectors = _embed_texts_in_batches(
            embedding_client=embedding_client,
            texts=texts,
            batch_size=embedding_batch_size,
        )
        if len(vectors) != len(child_chunks):
            raise RuntimeError("embedding count does not match child chunk count")
        collection_count = _write_chroma_collection(
            active_dir=tmp_dir,
            collection_name=collection_name,
            child_chunks=child_chunks,
            texts=texts,
            vectors=vectors,
            run_smoke_tests=run_smoke_tests,
        )
        manifest = build_chroma_manifest(
            chunk_file=chunk_file,
            chunks=chunks,
            embedding_provider=embedding_client.provider,
            embedding_model=embedding_client.model,
            collection_name=collection_name,
            source_hashes=source_hashes or {},
        )
        _write_json(tmp_dir / "manifest.json", manifest)
        gc.collect()
        if backup_dir.exists():
            shutil.rmtree(backup_dir)
        if active_dir.exists():
            active_dir.rename(backup_dir)
        tmp_dir.rename(active_dir)
        if backup_dir.exists():
            shutil.rmtree(backup_dir)
    except Exception:
        gc.collect()
        if tmp_dir.exists():
            shutil.rmtree(tmp_dir)
        if backup_dir.exists() and not active_dir.exists():
            backup_dir.rename(active_dir)
        raise
    return PdfRagIndexBuildResult(
        promoted=True,
        active_dir=active_dir,
        manifest_path=active_dir / "manifest.json",
        embedded_child_count=len(child_chunks),
        collection_count=collection_count,
    )


def _embed_texts_in_batches(
    *,
    embedding_client: EmbeddingClient,
    texts: list[str],
    batch_size: int,
) -> list[list[float]]:
    """Embed texts in bounded batches while preserving the original order."""
    safe_batch_size = max(1, batch_size)
    vectors: list[list[float]] = []
    for start in range(0, len(texts), safe_batch_size):
        batch = texts[start : start + safe_batch_size]
        vectors.extend(embedding_client.embed_texts(batch))
    return vectors


def diagnose_chroma_staleness(
    *,
    chunk_file: Path,
    active_dir: Path,
    expected_embedding_model: str | None = None,
) -> dict[str, Any]:
    """Return stale-index diagnostics based on manifest, chunk hash, and model."""
    manifest_path = active_dir / "manifest.json"
    if not manifest_path.is_file():
        return {"stale": True, "rag_error_type": "chroma_index_missing", "embedding_error_type": None}
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    if expected_embedding_model and manifest.get("embedding_model") != expected_embedding_model:
        return {
            "stale": True,
            "rag_error_type": "embedding_model_mismatch",
            "embedding_error_type": "model_mismatch",
            "expected_embedding_model": expected_embedding_model,
            "actual_embedding_model": manifest.get("embedding_model"),
        }
    current_hash = build_file_hash(chunk_file)
    expected_hash = manifest.get("chunk_file_hash")
    if current_hash != expected_hash:
        return {
            "stale": True,
            "rag_error_type": "chroma_index_stale",
            "embedding_error_type": None,
            "expected_chunk_file_hash": expected_hash,
            "current_chunk_file_hash": current_hash,
        }
    return {"stale": False, "rag_error_type": None, "embedding_error_type": None}


def check_pdf_rag_index(
    *,
    chunk_file: Path,
    active_dir: Path,
    expected_embedding_model: str,
    expected_embedding_provider: str = DEFAULT_EMBEDDING_PROVIDER,
    collection_name: str = "pdf_rag_corpus",
) -> PdfRagIndexCheckResult:
    """Check local Chroma index freshness without embedding or OpenAI calls."""
    manifest_path = active_dir / "manifest.json"
    if not manifest_path.is_file():
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_MISSING,
            status="missing",
            message="[PDF RAG] Chroma index missing.",
        )

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError, UnicodeDecodeError) as exc:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_MANIFEST_INVALID,
            status="manifest_invalid",
            message=f"[PDF RAG] Chroma index manifest is invalid: {exc.__class__.__name__}.",
        )
    if not isinstance(manifest, dict):
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_MANIFEST_INVALID,
            status="manifest_invalid",
            message="[PDF RAG] Chroma index manifest is invalid: root must be an object.",
        )

    try:
        chunks = read_jsonl(chunk_file)
    except Exception as exc:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_MANIFEST_INVALID,
            status="chunk_file_invalid",
            message=f"[PDF RAG] Validated chunk file cannot be read: {exc.__class__.__name__}.",
        )
    expected_child_count = len([chunk for chunk in chunks if chunk.get("chunk_kind") == "child"])

    if manifest.get("embedding_provider") != expected_embedding_provider:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_STALE,
            status="stale",
            message="[PDF RAG] Chroma index stale: embedding provider mismatch.",
            expected_child_count=expected_child_count,
        )
    if manifest.get("embedding_model") != expected_embedding_model:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_STALE,
            status="stale",
            message="[PDF RAG] Chroma index stale: embedding model mismatch.",
            expected_child_count=expected_child_count,
        )
    if manifest.get("collection_name") != collection_name:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_STALE,
            status="stale",
            message="[PDF RAG] Chroma index stale: collection name mismatch.",
            expected_child_count=expected_child_count,
        )
    if manifest.get("chunk_file_hash") != build_file_hash(chunk_file):
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_STALE,
            status="stale",
            message="[PDF RAG] Chroma index stale: validated chunk file changed.",
            expected_child_count=expected_child_count,
        )
    if int(manifest.get("embedded_child_count") or -1) != expected_child_count:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_COLLECTION_MISMATCH,
            status="collection_count_mismatch",
            message="[PDF RAG] Chroma index collection count mismatch: manifest child count changed.",
            expected_child_count=expected_child_count,
        )

    try:
        actual_count = get_chroma_collection_count(active_dir, collection_name)
    except Exception as exc:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_COLLECTION_MISMATCH,
            status="collection_count_mismatch",
            message=f"[PDF RAG] Chroma index collection count mismatch: {exc.__class__.__name__}.",
            expected_child_count=expected_child_count,
        )
    if actual_count != expected_child_count:
        return PdfRagIndexCheckResult(
            exit_code=PDF_RAG_INDEX_COLLECTION_MISMATCH,
            status="collection_count_mismatch",
            message="[PDF RAG] Chroma index collection count mismatch.",
            expected_child_count=expected_child_count,
            actual_collection_count=actual_count,
        )

    return PdfRagIndexCheckResult(
        exit_code=PDF_RAG_INDEX_READY,
        status="up_to_date",
        message="[PDF RAG] Chroma index is up to date. Skipping build.",
        expected_child_count=expected_child_count,
        actual_collection_count=actual_count,
    )


def get_chroma_collection_count(active_dir: Path, collection_name: str) -> int:
    """Return the number of vectors in a local Chroma collection."""
    client = _create_chroma_client(active_dir)
    try:
        collection = client.get_collection(collection_name)
        return int(collection.count())
    finally:
        _close_chroma_client(client)


def read_chroma_child_metadata(active_dir: Path, collection_name: str) -> list[dict[str, Any]]:
    """Return a stable metadata subset for child chunks stored in Chroma."""
    client = _create_chroma_client(active_dir)
    try:
        collection = client.get_collection(collection_name)
        result = collection.get(include=["metadatas"])
        rows = []
        ids = result.get("ids") or []
        metadatas = result.get("metadatas") or []
        for chunk_id, metadata in zip(ids, metadatas, strict=False):
            meta = metadata or {}
            rows.append(
                {
                    "chunk_id": str(chunk_id),
                    "source_id": str(meta.get("source_id") or ""),
                    "parent_chunk_id": str(meta.get("parent_chunk_id") or ""),
                    "route_safety_certification": bool(meta.get("route_safety_certification")),
                }
            )
        return sorted(rows, key=lambda item: item["chunk_id"])
    finally:
        _close_chroma_client(client)


def _write_chroma_collection(
    *,
    active_dir: Path,
    collection_name: str,
    child_chunks: list[dict[str, Any]],
    texts: list[str],
    vectors: list[list[float]],
    run_smoke_tests: bool,
) -> int:
    """Create one Chroma collection containing only child chunk embeddings."""
    client = _create_chroma_client(active_dir)
    try:
        try:
            client.delete_collection(collection_name)
        except Exception:
            pass
        collection = client.get_or_create_collection(collection_name)
        collection.add(
            ids=[str(chunk["chunk_id"]) for chunk in child_chunks],
            documents=texts,
            embeddings=vectors,
            metadatas=[_chroma_metadata(chunk) for chunk in child_chunks],
        )
        count = int(collection.count())
        if count != len(child_chunks):
            raise RuntimeError("Chroma child count does not match validated corpus child count")
        if run_smoke_tests:
            _run_chroma_smoke_test(collection=collection, child_chunks=child_chunks, vectors=vectors)
        return count
    finally:
        _close_chroma_client(client)


def _run_chroma_smoke_test(*, collection: Any, child_chunks: list[dict[str, Any]], vectors: list[list[float]]) -> None:
    """Verify the built collection can return a known child chunk by vector query."""
    expected_id = str(child_chunks[0]["chunk_id"])
    result = collection.query(query_embeddings=[vectors[0]], n_results=1, include=["metadatas", "distances"])
    ids = result.get("ids") or [[]]
    if not ids or not ids[0] or expected_id not in ids[0]:
        raise RuntimeError("Chroma retrieval smoke test failed")


def _chroma_metadata(chunk: dict[str, Any]) -> dict[str, str | int | float | bool]:
    """Flatten chunk metadata into Chroma-supported primitive values."""
    metadata: dict[str, str | int | float | bool] = {
        "chunk_id": str(chunk.get("chunk_id") or ""),
        "parent_chunk_id": str(chunk.get("parent_chunk_id") or ""),
        "source_id": str(chunk.get("source_id") or ""),
        "source_type": str(chunk.get("source_type") or ""),
        "source_title": str(chunk.get("source_title") or chunk.get("source_id") or ""),
        "authority_rank": int(chunk.get("authority_rank") or 4),
        "version_status": str(chunk.get("version_status") or ""),
        "review_status": str(chunk.get("review_status") or ""),
        "section_title": str(chunk.get("section_title") or ""),
        "page_start": int(chunk.get("page_start") or 0),
        "page_end": int(chunk.get("page_end") or chunk.get("page_start") or 0),
        "use_scope": _pipe_list(chunk.get("use_scope")),
        "topic_tags": _pipe_list(chunk.get("topic_tags")),
        "route_names": _pipe_list(chunk.get("route_names")),
    }
    route_names = set(str(route) for route in (chunk.get("route_names") or []))
    for route_name in sorted(ALLOWED_ROUTE_NAMES):
        metadata[f"route_{route_name}"] = route_name in route_names
    return metadata


def _pipe_list(value: Any) -> str:
    """Serialize list metadata as a searchable primitive string."""
    if not isinstance(value, list):
        return ""
    return "|" + "|".join(str(item) for item in value) + "|" if value else ""


def _create_chroma_client(active_dir: Path) -> Any:
    """Create a Chroma PersistentClient for the supplied local index directory."""
    chromadb_module = import_module("chromadb")
    client_type = getattr(chromadb_module, "PersistentClient")
    active_dir.mkdir(parents=True, exist_ok=True)
    return client_type(path=str(active_dir))


def _close_chroma_client(client: Any) -> None:
    """Release Chroma system resources so Windows can rename/remove index dirs."""
    try:
        client.close()
    finally:
        try:
            client.clear_system_cache()
        finally:
            gc.collect()


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    """Write a UTF-8 JSON object to an index artifact path."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
