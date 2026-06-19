from __future__ import annotations


class FileBasedRagRetrieverAdapterV2:
    def retrieve(
        self,
        queries: list[dict],
        *,
        top_k: int = 3,
    ) -> list[dict]:
        return []


class RagContextBuilderV2:
    def build_context(
        self,
        *,
        queries: list[dict],
        retrieved_contexts: list[dict],
    ) -> dict:
        return {
            "rag_queries": queries,
            "retrieved_contexts": retrieved_contexts,
            "retrieval_mode": "disabled" if not retrieved_contexts else "file_based",
        }
