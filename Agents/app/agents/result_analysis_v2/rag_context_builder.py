"""PDF RAG adapter and context builder for result-analysis v2 internals."""

from __future__ import annotations

from typing import Any

from app.agents.result_analysis_v2.routes import (
    RAG_ROUTE_NO_QUERY,
    RAG_ROUTE_NO_RESULT,
    RAG_ROUTE_RETRIEVED,
    RAG_ROUTE_SEARCH_ERROR,
    RAG_ROUTE_SKIPPED,
    RAG_ROUTE_STORE_MISSING,
    RagRouteV2,
)
from app.services.pdf_rag_retriever import PDF_RAG_BACKEND, PdfVectorHybridRagRetriever, get_pdf_rag_retriever


# Stable backend identifier for internal diagnostics and route tests.
RAG_BACKEND_PDF_VECTOR_HYBRID = PDF_RAG_BACKEND


class PdfRagRetrieverAdapterV2:
    """Wrap PDF Vector Hybrid RAG retrieval for result-analysis internal state."""

    def __init__(self, retriever: PdfVectorHybridRagRetriever | None = None) -> None:
        """Initialize a retriever and diagnostic record for the last retrieval attempt."""
        self.retriever = retriever
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
            retriever = self.retriever or get_pdf_rag_retriever()
            for query in queries:
                query_text = self._query_text(query)
                if not query_text:
                    self.last_diagnostic = self._diagnostic(
                        route=RAG_ROUTE_NO_QUERY,
                        query_count=query_count,
                        fallback_reason="query_empty",
                    )
                    return []
                result = retriever.retrieve(query_text, route_hint=self._route_hint(query))
                if not result.available:
                    self.last_diagnostic = {
                        **self._diagnostic(
                            route=RAG_ROUTE_STORE_MISSING,
                            query_count=query_count,
                            fallback_reason=result.diagnostic.get("rag_error_type", "rag_unavailable"),
                        ),
                        **result.diagnostic,
                    }
                    return []
                for evidence in result.context_pack.evidence_items[:top_k]:
                    retrieved.append(
                        {
                            "query_id": str(query.get("query_id") or ""),
                            "query_type": str(query.get("query_type") or ""),
                            "context_text": evidence.evidence_text,
                            "category": evidence.topic_tags[0] if evidence.topic_tags else "",
                            "related_policy_params": [],
                            "related_actions": [],
                        }
                    )
        except FileNotFoundError:
            self.last_diagnostic = self._diagnostic(
                route=RAG_ROUTE_STORE_MISSING,
                query_count=query_count,
                fallback_reason="store_missing",
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

    def _route_hint(self, query: dict[str, Any]) -> str | None:
        """Map v2 query type labels to PDF RAG route hints."""
        query_type = str(query.get("query_type") or "")
        if query_type in {"policy_safety", "collision_prevention", "general_safety"}:
            return "safety_certification"
        if query_type == "pedestrian_safety":
            return "crosswalk_sidewalk"
        if query_type == "navigation_efficiency":
            return "experiment_coverage"
        return None

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
            "backend": RAG_BACKEND_PDF_VECTOR_HYBRID,
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
            "retrieval_mode": "disabled" if not retrieved_contexts else RAG_BACKEND_PDF_VECTOR_HYBRID,
        }


# Backward-compatible import name; implementation no longer calls legacy card JSONL.
FileBasedRagRetrieverAdapterV2 = PdfRagRetrieverAdapterV2
