from __future__ import annotations

from pathlib import Path

import pytest

from app.models.llm import LlmGenerationRequest, LlmProvider
from app.services.llm_client_factory import create_llm_client
from app.services.llm_disabled_client import DisabledLlmClient
from app.services.llm_ollama_client import OllamaLlmClient
from app.services.llm_openai_client import OpenAILlmClient


ROOT = Path(__file__).resolve().parents[1]


def _request(provider: LlmProvider = LlmProvider.disabled) -> LlmGenerationRequest:
    return LlmGenerationRequest(
        provider=provider,
        model="disabled-model",
        systemPrompt="system",
        userPrompt="user",
        temperature=0,
        maxTokens=512,
        responseFormat="json_object",
        requestId="REQ-LLM-001",
    )


def test_llm_provider_enum_contains_expected_values() -> None:
    assert {item.value for item in LlmProvider} == {
        "disabled",
        "openai",
        "gemini",
        "ollama",
        "custom",
    }


def test_disabled_client_returns_failure_without_content() -> None:
    response = DisabledLlmClient().generate(_request())

    assert response.success is False
    assert response.content is None
    assert response.rawContent is None
    assert response.error is not None
    assert "disabled" in response.error.message.lower()


def test_factory_returns_disabled_client_for_disabled_provider() -> None:
    client = create_llm_client(LlmProvider.disabled)

    assert isinstance(client, DisabledLlmClient)


@pytest.mark.parametrize(
    "provider",
    [LlmProvider.gemini, LlmProvider.custom],
)
def test_factory_rejects_real_providers_until_implemented(provider: LlmProvider) -> None:
    with pytest.raises(NotImplementedError):
        create_llm_client(provider)


def test_factory_returns_ollama_client_for_ollama_provider() -> None:
    assert isinstance(create_llm_client(LlmProvider.ollama), OllamaLlmClient)


def test_factory_returns_openai_client_for_openai_provider() -> None:
    assert isinstance(create_llm_client(LlmProvider.openai), OpenAILlmClient)


def test_no_api_key_or_external_llm_call_code_is_added() -> None:
    forbidden_terms = [
        "GEMINI_API_KEY",
        "ANTHROPIC_API_KEY",
        "OpenAI(",
        "AsyncOpenAI(",
        "google.generativeai",
        "genai.",
        "chat.completions",
    ]
    for path in [ROOT / "app" / "models" / "llm.py", *list((ROOT / "app" / "services").glob("llm*.py"))]:
        text = path.read_text(encoding="utf-8-sig")
        for term in forbidden_terms:
            assert term not in text


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
