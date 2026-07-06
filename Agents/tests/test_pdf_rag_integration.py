from __future__ import annotations

import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app.agents.result_analysis_v2.rag_context_builder import PdfRagRetrieverAdapterV2, RagContextBuilderV2
from app.main import app
from app.services import world_config_rag_context_builder
from app.services.pdf_rag_retriever import PdfRagContextPack, PdfRagEvidenceItem, PdfRagRetrievalResult
from app.services.world_config_rag_context_builder import build_policy_context_for_world_config
from app.models.generation import WorldConfigGenerationConstraints, WorldConfigGenerationRequest


FORBIDDEN_PUBLIC_TERMS = (
    "source_id",
    "chunk_id",
    "page",
    "score",
    "RAG",
    "rag",
    "Rag",
    "Chroma",
    "chroma",
    "embedding",
    "Embedding",
    "vector",
    "Vector",
    "retrieval_backend",
    "retrieval backend",
)


class _FakePdfRetriever:
    def __init__(
        self,
        *,
        available: bool = True,
        rag_error_type: str = "chroma_index_missing",
        embedding_error_type: str | None = None,
    ) -> None:
        self.available = available
        self.rag_error_type = rag_error_type
        self.embedding_error_type = embedding_error_type
        self.calls: list[dict] = []

    def retrieve(self, query: str, *, route_hint: str | None = None, metrics: dict | None = None) -> PdfRagRetrievalResult:
        self.calls.append({"query": query, "route_hint": route_hint, "metrics": metrics})
        if not self.available:
            return PdfRagRetrievalResult.unavailable(
                route_name=route_hint or "safety_certification",
                rag_error_type=self.rag_error_type,
                embedding_error_type=self.embedding_error_type,
            )
        return PdfRagRetrievalResult(
            available=True,
            route_name=route_hint or "safety_certification",
            context_pack=PdfRagContextPack(
                evidence_items=[
                    PdfRagEvidenceItem(
                        source_id="KOR-004",
                        source_title="산업부 고시",
                        source_type="official_notice",
                        chunk_id="KOR-004-child-speed",
                        parent_chunk_id="KOR-004-parent-speed",
                        section_title="운행속도",
                        page_range="8-9",
                        evidence_text="보호구역 운행속도 기준은 5 km/h 이하입니다.",
                        evidence_summary="보호구역 운행속도 기준",
                        parent_summary="운행속도 인증 기준",
                        topic_tags=["speed_policy"],
                        use_scope=["certification_requirement"],
                        score=10.0,
                    )
                ]
            ),
            diagnostic={"backend": "pdf_vector_hybrid", "used": True},
        )


def _world_config_request(prompt: str) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-PDF-RAG",
        generationType="world_config",
        prompt=prompt,
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle"],
        ),
    )


def _write_summary_rows(project: Path, rows: list[dict]) -> None:
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").write_text(
        json.dumps(
            {"schema": "run_summary", "version": 1, "run": {"run_id": "000001"}, "rows": rows},
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )


def test_result_analysis_adapter_uses_pdf_retriever_context_without_legacy_jsonl() -> None:
    fake = _FakePdfRetriever()
    adapter = PdfRagRetrieverAdapterV2(retriever=fake)

    contexts = adapter.retrieve(
        [{"query_id": "RAGQ-001", "query_type": "policy_safety", "natural_language_query": "보호구역 운행속도"}]
    )

    assert contexts == [
        {
            "query_id": "RAGQ-001",
            "query_type": "policy_safety",
            "context_text": "보호구역 운행속도 기준은 5 km/h 이하입니다.",
            "category": "speed_policy",
            "related_policy_params": [],
            "related_actions": [],
        }
    ]
    assert adapter.last_diagnostic["backend"] == "pdf_vector_hybrid"


def test_rag_context_builder_uses_internal_mode_only() -> None:
    context = RagContextBuilderV2().build_context(queries=[], retrieved_contexts=[])

    assert context["retrieval_mode"] == "disabled"


def test_world_config_builder_calls_pdf_facade(monkeypatch) -> None:
    fake = _FakePdfRetriever()
    monkeypatch.setattr(world_config_rag_context_builder, "get_pdf_rag_retriever", lambda: fake)

    contexts = build_policy_context_for_world_config(_world_config_request("보호구역 운행속도 기준"))

    assert contexts
    assert fake.calls
    assert contexts[0].category == "speed_policy"
    assert contexts[0].chunkId.startswith("PDF-RAG-")


@pytest.mark.parametrize(
    ("rag_error_type", "embedding_error_type"),
    [
        ("chroma_index_missing", None),
        ("chroma_index_stale", "model_mismatch"),
        ("embedding_model_mismatch", "model_mismatch"),
        ("query_embedding_failure", "api_key_missing"),
        ("embedding_timeout", "embedding_timeout"),
        ("chroma_query_failure", "RuntimeError"),
    ],
)
def test_v2_analysis_run_public_schema_same_when_pdf_rag_unavailable(
    monkeypatch,
    tmp_path: Path,
    rag_error_type: str,
    embedding_error_type: str | None,
) -> None:
    from app.agents.result_analysis_v2 import rag_context_builder as rag_module

    unavailable = _FakePdfRetriever(
        available=False,
        rag_error_type=rag_error_type,
        embedding_error_type=embedding_error_type,
    )
    monkeypatch.setattr(rag_module, "get_pdf_rag_retriever", lambda: unavailable)
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")
    project = tmp_path / "Project1"
    _write_summary_rows(
        project,
        [
            {
                "episode_id": "000001",
                "outcome": "Failure",
                "terminal_reason": "StaticObstacleCollision",
                "duration_s": 10.0,
                "metrics": {"goal_reached": 0, "static_obstacle_collision_count": 1},
            }
        ],
    )

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={"project_path": str(project), "run_id": "000001"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    public_payload = json.dumps(payload, ensure_ascii=False)
    response_path = project / "runs" / "000001" / "review" / payload["review_id"] / "response.json"
    assert json.loads(response_path.read_text(encoding="utf-8")) == payload
    for forbidden in FORBIDDEN_PUBLIC_TERMS:
        assert forbidden not in public_payload
