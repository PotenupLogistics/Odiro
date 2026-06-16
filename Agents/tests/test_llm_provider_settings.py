from __future__ import annotations

from pathlib import Path

from app.core.settings import Settings
from app.models.llm import LlmProvider
from app.services.llm_provider_policy import get_provider_chain, get_provider_status


ROOT = Path(__file__).resolve().parents[1]


def test_default_settings_load_without_env_file() -> None:
    settings = Settings(_env_file=None)

    assert settings.llmProvider == LlmProvider.openai
    assert settings.llmProviderChain == ["openai", "ollama"]
    assert settings.openaiApiKey == ""
    assert settings.ollamaBaseUrl == "http://localhost:11434"
    assert settings.v2AgentGraphEnabled is False


def test_default_provider_chain_is_openai_then_ollama() -> None:
    settings = Settings(_env_file=None)

    assert get_provider_chain(settings) == [LlmProvider.openai, LlmProvider.ollama]


def test_openai_without_api_key_is_unavailable() -> None:
    settings = Settings(_env_file=None, openaiApiKey="")

    status = get_provider_status(LlmProvider.openai, settings)

    assert status.available is False
    assert status.provider == LlmProvider.openai
    assert "API key" in status.reason


def test_ollama_with_base_url_and_model_is_configured_without_ping() -> None:
    settings = Settings(_env_file=None)

    status = get_provider_status(LlmProvider.ollama, settings)

    assert status.available is True
    assert status.model == settings.ollamaModel
    assert "configured" in status.reason


def test_env_example_exists_with_empty_openai_key() -> None:
    env_example = ROOT / ".env.example"

    assert env_example.exists()
    assert "OPENAI_API_KEY=\n" in env_example.read_text(encoding="utf-8")
