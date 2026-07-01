"""Shared guardrails for text exposed in result analysis public responses."""

from __future__ import annotations

from typing import Any

# Internal source markers that must not appear in user-facing analysis text.
PUBLIC_RESPONSE_FORBIDDEN_TERMS: tuple[str, ...] = (
    "KOR-",
    "policy card",
    "관련 정책 문서",
    "p.33",
    "근거 문서",
    "RAG",
)

# Recommendation fields copied to the public response surface.
PUBLIC_RECOMMENDATION_TEXT_FIELDS: tuple[str, ...] = ("title", "reason", "recommendation")

# Insight fields copied or normalized to the public response surface.
PUBLIC_INSIGHT_TEXT_FIELDS: tuple[str, ...] = ("title", "description", "detail")


def contains_forbidden_public_text(value: object) -> bool:
    """Return whether a public text value includes an internal source marker."""
    if not isinstance(value, str):
        return False
    return any(term in value for term in PUBLIC_RESPONSE_FORBIDDEN_TERMS)


def record_contains_forbidden_public_text(record: dict[str, Any], fields: tuple[str, ...]) -> bool:
    """Return whether selected public text fields include internal source markers."""
    return any(contains_forbidden_public_text(record.get(field)) for field in fields)
