from __future__ import annotations

from app.core.settings import Settings
from app.models.llm import LlmProvider, LlmProviderStatus


def get_provider_chain(settings: Settings) -> list[LlmProvider]:
    providers: list[LlmProvider] = []
    for item in settings.llmProviderChain:
        try:
            provider = LlmProvider(item)
        except ValueError:
            continue
        if provider not in providers:
            providers.append(provider)
    return providers or [settings.llmProvider]


def get_provider_status(provider: LlmProvider, settings: Settings) -> LlmProviderStatus:
    if provider == LlmProvider.disabled:
        return LlmProviderStatus(
            provider=provider,
            available=True,
            reason="disabled provider is always available for safe no-op generation",
            model="disabled",
        )
    if provider == LlmProvider.openai:
        if not settings.openaiApiKey.strip():
            return LlmProviderStatus(
                provider=provider,
                available=False,
                reason="OpenAI API key is not configured.",
                model=settings.openaiModel,
            )
        return LlmProviderStatus(
            provider=provider,
            available=True,
            reason="OpenAI provider is configured; no API validation call was made.",
            model=settings.openaiModel,
        )
    if provider == LlmProvider.ollama:
        configured = bool(settings.ollamaBaseUrl.strip() and settings.ollamaModel.strip())
        return LlmProviderStatus(
            provider=provider,
            available=configured,
            reason=(
                "Ollama provider is configured; no server ping was made."
                if configured
                else "Ollama base URL or model is not configured."
            ),
            model=settings.ollamaModel if configured else None,
        )
    return LlmProviderStatus(
        provider=provider,
        available=False,
        reason=f"{provider.value} provider is not implemented in this stage.",
        model=None,
    )
