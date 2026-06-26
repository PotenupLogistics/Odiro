from __future__ import annotations

from pathlib import Path

from app.core.settings import Settings
from app.models.llm import LlmProvider
from app.services import llm_provider_policy
from app.services.llm_provider_policy import get_provider_chain


ROOT = Path(__file__).resolve().parents[1]


def test_default_provider_chain_is_openai_only() -> None:
    assert get_provider_chain(Settings(_env_file=None)) == [LlmProvider.openai]


def test_provider_policy_does_not_expose_retry_selector() -> None:
    assert "should" + "_fallback" not in vars(llm_provider_policy)
    assert "select" + "_next_provider" not in vars(llm_provider_policy)


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
