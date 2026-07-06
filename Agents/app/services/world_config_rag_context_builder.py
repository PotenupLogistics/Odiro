from __future__ import annotations

from app.models.generation import RetrievedPolicyContext, WorldConfigGenerationRequest
from app.services.natural_language_normalizer import infer_retrieval_queries
from app.services.pdf_rag_retriever import get_pdf_rag_retriever, route_query
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent


def _shorten(text: str, limit: int = 320) -> str:
    normalized = " ".join(text.split())
    return normalized if len(normalized) <= limit else normalized[: limit - 3] + "..."


def build_policy_context_for_world_config(
    request: WorldConfigGenerationRequest,
    top_k: int = 5,
    compact: bool = False,
) -> list[RetrievedPolicyContext]:
    contexts_by_key: dict[str, RetrievedPolicyContext] = {}
    intent = extract_scenario_intent(request.prompt)
    retrieval_queries = infer_retrieval_queries(request.prompt)
    retrieval_queries.extend(intent.suggestedCategories)
    retrieval_queries.extend(intent.suggestedActions)
    retrieval_queries.extend(intent.suggestedPolicyParams)
    if intent.pathBlockingHints or "Obstacle" in intent.obstacleHints:
        retrieval_queries.extend(
            [
                "장애물 감지",
                "장애물 회피",
                "경로 차단",
                "perception_requirement",
            ]
        )

    retriever = get_pdf_rag_retriever()
    context_index = 1
    for retrieval_query in dict.fromkeys(retrieval_queries):
        result = retriever.retrieve(retrieval_query, route_hint=route_query(retrieval_query))
        if not result.available:
            continue
        for evidence in result.context_pack.evidence_items[:top_k]:
            key = f"{evidence.source_title}:{evidence.section_title}:{evidence.evidence_summary}"
            if key in contexts_by_key:
                continue
            contexts_by_key[key] = RetrievedPolicyContext(
                chunkId=f"PDF-RAG-{context_index:03d}",
                cardId=f"PDF-RAG-{context_index:03d}",
                category=evidence.topic_tags[0] if evidence.topic_tags else "pdf_rag_context",
                evidenceLocation=evidence.section_title,
                relatedActions=[],
                relatedPolicyParams=[],
                shortText=_shorten(evidence.evidence_text, 120 if compact else 320),
                score=float(top_k - min(context_index, top_k) + 1),
            )
            context_index += 1

    contexts = sorted(
        contexts_by_key.values(),
        key=lambda item: (-item.score, item.chunkId),
    )
    return contexts[:top_k]
