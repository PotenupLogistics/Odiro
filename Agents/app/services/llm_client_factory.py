from __future__ import annotations

from app.core.settings import Settings
from app.models.llm import LlmProvider
from app.services.llm_client import BaseLlmClient
from app.services.llm_disabled_client import DisabledLlmClient
from app.services.llm_ollama_client import OllamaLlmClient
from app.services.llm_openai_client import OpenAILlmClient
from app.services.llm_provider_policy import get_provider_chain


def get_configured_provider_chain(settings: Settings | None = None) -> list[LlmProvider]:
    return get_provider_chain(settings or Settings())


def create_llm_client(
    provider: LlmProvider,
    settings: Settings | None = None,
) -> BaseLlmClient:
    if provider == LlmProvider.disabled:
        return DisabledLlmClient()
    if provider == LlmProvider.openai:
        return OpenAILlmClient(settings=settings)
    if provider == LlmProvider.ollama:
        return OllamaLlmClient(settings=settings)
    raise NotImplementedError(
        f"LLM provider '{provider.value}' is not implemented in this stage."
    )
