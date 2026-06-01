from __future__ import annotations

from app.models.generation import RetrievedPolicyContext, WorldConfigGenerationRequest
from app.models.rag import RagSearchQuery
from app.services.natural_language_normalizer import infer_retrieval_queries
from app.services.policy_rag_retriever import search_policy_chunks
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent


def _shorten(text: str, limit: int = 320) -> str:
    normalized = " ".join(text.split())
    return normalized if len(normalized) <= limit else normalized[: limit - 3] + "..."


def build_policy_context_for_world_config(
    request: WorldConfigGenerationRequest,
    top_k: int = 5,
    compact: bool = False,
) -> list[RetrievedPolicyContext]:
    contexts_by_chunk_id: dict[str, RetrievedPolicyContext] = {}
    intent = extract_scenario_intent(request.prompt)
    retrieval_queries = infer_retrieval_queries(request.prompt)
    retrieval_queries.extend(intent.suggestedCategories)
    retrieval_queries.extend(intent.suggestedActions)
    retrieval_queries.extend(intent.suggestedPolicyParams)

    for retrieval_query in dict.fromkeys(retrieval_queries):
        result = search_policy_chunks(
            RagSearchQuery(
                query=retrieval_query,
                topK=top_k,
                categoryFilter=[retrieval_query] if retrieval_query in intent.suggestedCategories else None,
            )
        )
        for chunk in result.results:
            if chunk.chunkId in contexts_by_chunk_id:
                continue
            contexts_by_chunk_id[chunk.chunkId] = RetrievedPolicyContext(
                chunkId=chunk.chunkId,
                cardId=chunk.cardId,
                category=chunk.metadata.category,
                evidenceLocation=chunk.metadata.evidenceLocation,
                relatedActions=chunk.metadata.relatedActions,
                relatedPolicyParams=chunk.metadata.relatedPolicyParams,
                shortText=_shorten(chunk.chunkText, 120 if compact else 320),
                score=chunk.score,
            )

    if not contexts_by_chunk_id:
        for category in intent.suggestedCategories:
            result = search_policy_chunks(
                RagSearchQuery(query=category, topK=top_k, categoryFilter=[category])
            )
            for chunk in result.results:
                contexts_by_chunk_id[chunk.chunkId] = RetrievedPolicyContext(
                    chunkId=chunk.chunkId,
                    cardId=chunk.cardId,
                    category=chunk.metadata.category,
                    evidenceLocation=chunk.metadata.evidenceLocation,
                    relatedActions=chunk.metadata.relatedActions,
                    relatedPolicyParams=chunk.metadata.relatedPolicyParams,
                    shortText=_shorten(chunk.chunkText, 120 if compact else 320),
                    score=chunk.score,
                )

    contexts = sorted(
        contexts_by_chunk_id.values(),
        key=lambda item: (-item.score, item.chunkId),
    )
    return contexts[:top_k]
