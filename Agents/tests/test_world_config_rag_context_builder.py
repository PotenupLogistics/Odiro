from __future__ import annotations

from app.models.generation import (
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
)
from app.services import world_config_rag_context_builder
from app.services.pdf_rag_retriever import PdfRagContextPack, PdfRagEvidenceItem, PdfRagRetrievalResult
from app.services.world_config_rag_context_builder import build_policy_context_for_world_config


KOREAN_SCENARIO_PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


class _FakeWorldPdfRetriever:
    """Return deterministic PDF RAG evidence for world-config context tests."""

    def retrieve(self, query: str, *, route_hint: str | None = None, metrics: dict | None = None) -> PdfRagRetrievalResult:
        """Return one evidence item whose tag follows the query topic."""
        _ = route_hint, metrics
        category = "emergency_stop" if "비상정지" in query else "perception_requirement"
        return PdfRagRetrievalResult(
            available=True,
            route_name="safety_certification",
            context_pack=PdfRagContextPack(
                evidence_items=[
                    PdfRagEvidenceItem(
                        source_id="KOR-004",
                        source_title="산업부 고시",
                        source_type="official_notice",
                        chunk_id="KOR-004-child-context",
                        parent_chunk_id="KOR-004-parent-context",
                        section_title="운행안전",
                        page_range="1",
                        evidence_text="비상정지와 장애물 감지 기준은 안전 인증 판단에 사용한다.",
                        evidence_summary="안전 인증 기준",
                        parent_summary="안전 인증",
                        topic_tags=[category],
                        use_scope=["certification_requirement"],
                        score=10.0,
                    )
                ]
            ),
            diagnostic={"backend": "pdf_vector_hybrid", "used": True},
        )


def _request(prompt: str) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-RAG-001",
        generationType="world_config",
        prompt=prompt,
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle", "Kickboard"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=42,
            requireValidation=True,
        ),
        maxRepairAttempts=2,
    )


def test_rag_context_builder_returns_related_policy_chunks(monkeypatch) -> None:
    monkeypatch.setattr(world_config_rag_context_builder, "get_pdf_rag_retriever", lambda: _FakeWorldPdfRetriever())
    prompt = "\ube44\uc0c1\uc815\uc9c0\uac00 \ud544\uc694\ud55c \ucda9\ub3cc \uc704\ud5d8 \uc0c1\ud669"

    contexts = build_policy_context_for_world_config(_request(prompt))

    assert contexts
    assert any(context.category == "emergency_stop" for context in contexts)
    assert all(context.shortText for context in contexts)


def test_rag_context_builder_limits_results_to_top_five(monkeypatch) -> None:
    monkeypatch.setattr(world_config_rag_context_builder, "get_pdf_rag_retriever", lambda: _FakeWorldPdfRetriever())
    prompt = "\uc18d\ub3c4 \uc815\uc9c0 \uc7a5\uc560\ubb3c \ubcf4\ub3c4 \uad00\uc81c \uacbd\uc0ac"

    contexts = build_policy_context_for_world_config(_request(prompt))

    assert len(contexts) <= 5


def test_korean_scenario_prompt_returns_policy_contexts(monkeypatch) -> None:
    monkeypatch.setattr(world_config_rag_context_builder, "get_pdf_rag_retriever", lambda: _FakeWorldPdfRetriever())
    contexts = build_policy_context_for_world_config(_request(KOREAN_SCENARIO_PROMPT))

    assert contexts
    assert any(
        context.category in {"perception_requirement", "sidewalk_operation", "speed_policy"}
        for context in contexts
    )
