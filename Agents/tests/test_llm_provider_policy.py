from __future__ import annotations

from pathlib import Path

from app.core.settings import Settings
from app.models.llm import LlmProvider
from app.services.llm_provider_policy import get_provider_chain, select_next_provider, should_fallback


ROOT = Path(__file__).resolve().parents[1]


def _settings_with_key(**overrides) -> Settings:
    values = {
        "_env_file": None,
        "openaiApiKey": "test-key",
        "openaiDailyRequestLimit": 30,
        "llmAllowOpenaiFallback": True,
    }
    values.update(overrides)
    return Settings(**values)


def test_default_provider_chain_is_openai_then_ollama() -> None:
    assert get_provider_chain(Settings(_env_file=None)) == [LlmProvider.openai, LlmProvider.ollama]


def test_openai_timeout_allows_ollama_fallback() -> None:
    decision = should_fallback(
        "openai_timeout",
        validation_failed_count=0,
        settings=_settings_with_key(),
    )

    assert decision.shouldFallback is True
    assert decision.fromProvider == LlmProvider.openai
    assert decision.toProvider == LlmProvider.ollama
    assert decision.costSensitive is True


def test_prompt_too_ambiguous_does_not_fallback() -> None:
    decision = should_fallback(
        "prompt_too_ambiguous",
        validation_failed_count=0,
        settings=_settings_with_key(),
    )

    assert decision.shouldFallback is False
    assert decision.toProvider is None


def test_missing_openai_key_blocks_fallback() -> None:
    decision = should_fallback(
        "openai_api_key_missing",
        validation_failed_count=0,
        settings=Settings(_env_file=None, openaiApiKey=""),
    )

    assert decision.shouldFallback is True
    assert decision.toProvider == LlmProvider.ollama


def test_daily_request_limit_blocks_fallback_when_exceeded() -> None:
    decision = should_fallback(
        "openai_timeout",
        validation_failed_count=0,
        settings=_settings_with_key(openaiDailyRequestLimit=0),
    )

    assert decision.shouldFallback is True
    assert decision.toProvider == LlmProvider.ollama


def test_select_next_provider_uses_provider_chain() -> None:
    next_provider = select_next_provider(
        LlmProvider.openai,
        _settings_with_key(),
        "openai_timeout",
    )

    assert next_provider == LlmProvider.ollama


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
