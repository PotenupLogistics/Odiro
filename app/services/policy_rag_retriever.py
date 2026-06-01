from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

from app.models.rag import RagChunkMetadata, RagRetrievedChunk, RagSearchQuery, RagSearchResult


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"


def load_chunks(path: str = str(DEFAULT_CHUNKS_PATH)) -> list[dict[str, Any]]:
    chunk_path = Path(path)
    return [
        json.loads(line)
        for line in chunk_path.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]


def _tokens(query: str) -> list[str]:
    return [token for token in re.split(r"\s+", query.strip().lower()) if token]


def _contains_any(values: list[str], filters: list[str] | None) -> bool:
    if not filters:
        return True
    value_set = {value.lower() for value in values}
    return any(item.lower() in value_set for item in filters)


def _passes_filters(chunk: dict[str, Any], query: RagSearchQuery) -> bool:
    metadata = chunk["metadata"]
    if query.categoryFilter and metadata.get("category") not in query.categoryFilter:
        return False
    if not _contains_any(metadata.get("relatedActions", []), query.actionFilter):
        return False
    if not _contains_any(metadata.get("relatedPolicyParams", []), query.policyParamFilter):
        return False
    if not _contains_any(metadata.get("sourceIds", []), query.sourceFilter):
        return False
    return True


def _score_chunk(chunk: dict[str, Any], query: RagSearchQuery) -> tuple[float, list[str]]:
    metadata = chunk["metadata"]
    chunk_text = str(chunk.get("chunkText", ""))
    haystack = chunk_text.lower()
    score = 0.0
    matched: list[str] = []

    for token in _tokens(query.query):
        if token in haystack:
            score += 2.0
            matched.append(f"chunkText:{token}")
        if token == str(metadata.get("category", "")).lower():
            score += 1.5
            matched.append(f"category:{token}")

    if query.categoryFilter and metadata.get("category") in query.categoryFilter:
        score += 3.0
        matched.append("categoryFilter")
    if query.actionFilter:
        matched_actions = [
            action for action in metadata.get("relatedActions", []) if action in query.actionFilter
        ]
        if matched_actions:
            score += 3.0 + len(matched_actions)
            matched.append("actionFilter:" + ",".join(matched_actions))
    if query.policyParamFilter:
        matched_params = [
            param for param in metadata.get("relatedPolicyParams", []) if param in query.policyParamFilter
        ]
        if matched_params:
            score += 3.0 + len(matched_params)
            matched.append("policyParamFilter:" + ",".join(matched_params))
    if query.sourceFilter:
        matched_sources = [
            source for source in metadata.get("sourceIds", []) if source in query.sourceFilter
        ]
        if matched_sources:
            score += 1.0 + len(matched_sources)
            matched.append("sourceFilter:" + ",".join(matched_sources))

    if not query.query and not any(
        [query.categoryFilter, query.actionFilter, query.policyParamFilter, query.sourceFilter]
    ):
        score = 0.1
        matched.append("default")

    return score, matched


def search_policy_chunks(query: RagSearchQuery) -> RagSearchResult:
    chunks = load_chunks()
    results: list[RagRetrievedChunk] = []
    for chunk in chunks:
        if not _passes_filters(chunk, query):
            continue
        score, matched_fields = _score_chunk(chunk, query)
        if score <= 0 and query.query:
            continue
        results.append(
            RagRetrievedChunk(
                chunkId=chunk["chunkId"],
                cardId=chunk["cardId"],
                chunkText=chunk["chunkText"],
                metadata=RagChunkMetadata.model_validate(chunk["metadata"]),
                score=score,
                matchedFields=matched_fields,
            )
        )

    results.sort(key=lambda item: (-item.score, item.chunkId))
    trimmed = results[: query.topK]
    return RagSearchResult(query=query, resultCount=len(trimmed), results=trimmed)


def search_by_category(category: str, topK: int = 5) -> RagSearchResult:
    return search_policy_chunks(RagSearchQuery(query="", topK=topK, categoryFilter=[category]))


def search_by_action(action: str, topK: int = 5) -> RagSearchResult:
    return search_policy_chunks(RagSearchQuery(query="", topK=topK, actionFilter=[action]))


def search_by_policy_param(param: str, topK: int = 5) -> RagSearchResult:
    return search_policy_chunks(RagSearchQuery(query="", topK=topK, policyParamFilter=[param]))
