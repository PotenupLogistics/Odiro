"""File-based RAG adapter and context builder for result-analysis v2 internals."""

from __future__ import annotations

import json
from typing import Any

from app.agents.result_analysis_v2.routes import (
    RAG_ROUTE_JSONL_ERROR,
    RAG_ROUTE_NO_QUERY,
    RAG_ROUTE_NO_RESULT,
    RAG_ROUTE_RETRIEVED,
    RAG_ROUTE_SEARCH_ERROR,
    RAG_ROUTE_SKIPPED,
    RAG_ROUTE_STORE_MISSING,
    RagRouteV2,
)
from app.models.rag import RagSearchQuery
from app.services.policy_rag_retriever import search_policy_chunks


# Stable backend identifier for internal diagnostics and route tests.
RAG_BACKEND_FILE_BASED_JSONL = "file_based_jsonl"


class FileBasedRagRetrieverAdapterV2:
    """Wrap the file-based policy RAG search for result-analysis internal state."""

    def __init__(self) -> None:
        """Initialize a diagnostic record for the last retrieval attempt."""
        self.last_diagnostic = self._diagnostic(route=RAG_ROUTE_SKIPPED)

    def retrieve(
        self,
        queries: list[dict],
        *,
        top_k: int = 3,
    ) -> list[dict]:
        """Return sanitized RAG context records while keeping raw evidence out of artifacts."""
        query_count = len(queries)
        if not queries:
            self.last_diagnostic = self._diagnostic(
                route=RAG_ROUTE_NO_QUERY,
                query_count=query_count,
                fallback_reason="query_empty",
            )
            return []

        retrieved: list[dict[str, Any]] = []
        try:
            for query in queries:
                query_text = self._query_text(query)
                if not query_text:
                    self.last_diagnostic = self._diagnostic(
                        route=RAG_ROUTE_NO_QUERY,
                        query_count=query_count,
                        fallback_reason="query_empty",
                    )
                    return []
                result = search_policy_chunks(RagSearchQuery(query=query_text, topK=top_k))
                for chunk in result.results:
                    retrieved.append(
                        {
                            "query_id": str(query.get("query_id") or ""),
                            "query_type": str(query.get("query_type") or ""),
                            "context_text": chunk.chunkText,
                            "category": chunk.metadata.category,
                            "related_policy_params": list(chunk.metadata.relatedPolicyParams),
                            "related_actions": list(chunk.metadata.relatedActions),
                        }
                    )
        except FileNotFoundError:
            self.last_diagnostic = self._diagnostic(
                route=RAG_ROUTE_STORE_MISSING,
                query_count=query_count,
                fallback_reason="store_missing",
            )
            return []
        except (json.JSONDecodeError, UnicodeDecodeError):
            self.last_diagnostic = self._diagnostic(
                route=RAG_ROUTE_JSONL_ERROR,
                query_count=query_count,
                fallback_reason="jsonl_parse_failed",
            )
            return []
        except Exception:
            self.last_diagnostic = self._diagnostic(
                route=RAG_ROUTE_SEARCH_ERROR,
                query_count=query_count,
                fallback_reason="retriever_error",
            )
            return []

        if not retrieved:
            self.last_diagnostic = self._diagnostic(
                route=RAG_ROUTE_NO_RESULT,
                query_count=query_count,
                fallback_reason="no_chunks_found",
            )
            return []

        self.last_diagnostic = self._diagnostic(
            route=RAG_ROUTE_RETRIEVED,
            query_count=query_count,
            retrieved_chunk_count=len(retrieved),
            used=True,
        )
        return retrieved

    def _query_text(self, query: dict[str, Any]) -> str:
        """Build the file-search query text from a v2 analysis RAG query."""
        natural_language_query = str(query.get("natural_language_query") or "").strip()
        if natural_language_query:
            return natural_language_query
        keywords = query.get("keywords")
        if isinstance(keywords, list):
            return " ".join(str(keyword) for keyword in keywords if str(keyword).strip()).strip()
        return ""

    def _diagnostic(
        self,
        *,
        route: RagRouteV2,
        query_count: int = 0,
        retrieved_chunk_count: int = 0,
        used: bool = False,
        fallback_reason: str | None = None,
    ) -> dict[str, Any]:
        """Create a path-free retrieval diagnostic for internal state only."""
        return {
            "enabled": True,
            "backend": RAG_BACKEND_FILE_BASED_JSONL,
            "used": used,
            "query_count": query_count,
            "retrieved_chunk_count": retrieved_chunk_count,
            "route": route,
            "fallback_reason": fallback_reason,
        }


class RagContextBuilderV2:
    """Build the internal analysis context section for retrieved policy hints."""

    def build_context(
        self,
        *,
        queries: list[dict],
        retrieved_contexts: list[dict],
    ) -> dict:
        """Return RAG context for the LLM/rule-based analysis context only."""
        return {
            "rag_queries": queries,
            "retrieved_contexts": retrieved_contexts,
            "retrieval_mode": "disabled" if not retrieved_contexts else RAG_BACKEND_FILE_BASED_JSONL,
        }
