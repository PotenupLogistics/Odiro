"""Shared guardrails for text exposed in result analysis public responses."""

from __future__ import annotations

import re
from typing import Any

# Internal source markers that must not appear in user-facing analysis text.
PUBLIC_RESPONSE_FORBIDDEN_TERMS: tuple[str, ...] = (
    "KOR-",
    "policy card",
    "관련 정책 문서",
    "p.33",
    "근거 문서",
    "source_id",
    "chunk_id",
    "page",
    "score",
    "Chroma",
    "chroma",
    "vector",
    "Vector",
    "embedding",
    "Embedding",
    "retrieval_backend",
    "retrieval backend",
    "RAG",
    "rag",
    "Rag",
)

# Terms that are only forbidden when they look like internal metadata.
PUBLIC_RESPONSE_CONTEXTUAL_TERMS: frozenset[str] = frozenset({"score"})

# Internal source markers compiled with token boundaries to avoid normal word fragments.
PUBLIC_RESPONSE_FORBIDDEN_PATTERNS: tuple[re.Pattern[str], ...] = tuple(
    re.compile(rf"(?<![A-Za-z0-9_]){re.escape(term)}(?![A-Za-z0-9_])", re.IGNORECASE)
    for term in PUBLIC_RESPONSE_FORBIDDEN_TERMS
    if term.casefold() not in PUBLIC_RESPONSE_CONTEXTUAL_TERMS
)

# Metadata-shaped internal markers that are too common for broad substring blocking.
PUBLIC_RESPONSE_CONTEXTUAL_PATTERNS: tuple[re.Pattern[str], ...] = (
    re.compile(r"(?<![A-Za-z0-9_])score(?![A-Za-z0-9_])\s*[:=]", re.IGNORECASE),
)

# Recommendation fields copied to the public response surface.
PUBLIC_RECOMMENDATION_TEXT_FIELDS: tuple[str, ...] = ("title", "reason", "recommendation")

# Insight fields copied or normalized to the public response surface.
PUBLIC_INSIGHT_TEXT_FIELDS: tuple[str, ...] = ("title", "description", "detail")


def contains_forbidden_public_text(value: object) -> bool:
    """Return whether a public text value includes an internal source marker."""
    if not isinstance(value, str):
        return False
    return any(pattern.search(value) for pattern in PUBLIC_RESPONSE_FORBIDDEN_PATTERNS) or any(
        pattern.search(value) for pattern in PUBLIC_RESPONSE_CONTEXTUAL_PATTERNS
    )


def record_contains_forbidden_public_text(record: dict[str, Any], fields: tuple[str, ...]) -> bool:
    """Return whether selected public text fields include internal source markers."""
    return any(contains_forbidden_public_text(record.get(field)) for field in fields)
