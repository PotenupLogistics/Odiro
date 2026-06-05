from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = {
    "package": ROOT / "docs" / "archive" / "previous_episode_spec" / "UE_TEAM_HANDOFF_PACKAGE.md",
    "endpoint": ROOT / "docs" / "handoff" / "UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md",
    "message": ROOT / "docs" / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md",
    "smoke": ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md",
}


def test_ue_team_handoff_package_docs_exist() -> None:
    for path in DOCS.values():
        assert path.exists()


def test_handoff_package_mentions_episode_spec_format() -> None:
    text = DOCS["package"].read_text(encoding="utf-8-sig")
    assert "responseFormat=episode_spec" in text
    assert "semantic_type" in text
    assert "blocking_ratio" in text
    assert "semantic_behavior" in text
    assert "pedestrian_crossing" in text


def test_message_draft_requests_kickboard_catalog_confirmation() -> None:
    text = DOCS["message"].read_text(encoding="utf-8-sig")
    assert "/api/v1/scenarios/generate" in text
    assert "FastAPI/OpenAPI에서 제거" in text
    assert "obstacle.kickboard" in text
    assert "obstacle.road_barrier_01" in text


def test_controlled_smoke_result_mentions_semantic_checks() -> None:
    text = DOCS["smoke"].read_text(encoding="utf-8-sig")
    assert "hasKickboardSemantic" in text
    assert "hasBlockingRatio" in text
    assert "hasCrossingPedestrian" in text


def test_no_forbidden_artifacts_created_for_ue_docs() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
