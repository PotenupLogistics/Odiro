from __future__ import annotations

from app.core.settings import Settings
from app.models.llm import LlmFallbackDecision, LlmProvider, LlmProviderStatus


FALLBACK_ELIGIBLE_ERRORS = {
    "openai_api_key_missing",
    "openai_timeout",
    "openai_rate_limited",
    "openai_http_error",
    "openai_invalid_response",
    "openai_missing_content",
    "openai_provider_error",
    "validation_failed_after_repair",
}
FALLBACK_BLOCKED_ERRORS = {
    "prompt_too_ambiguous",
    "missing_required_user_intent",
    "unsupported_request",
}


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


def should_fallback(
    error_code: str,
    validation_failed_count: int,
    settings: Settings,
    from_provider: LlmProvider = LlmProvider.openai,
) -> LlmFallbackDecision:
    if error_code in FALLBACK_BLOCKED_ERRORS:
        return LlmFallbackDecision(
            shouldFallback=False,
            fromProvider=from_provider,
            toProvider=None,
            reason=f"{error_code} requires clearer user input, not provider fallback.",
            costSensitive=True,
        )
    if error_code not in FALLBACK_ELIGIBLE_ERRORS and validation_failed_count <= 0:
        return LlmFallbackDecision(
            shouldFallback=False,
            fromProvider=from_provider,
            toProvider=None,
            reason=f"{error_code} is not eligible for fallback.",
            costSensitive=True,
        )
    if from_provider != LlmProvider.openai:
        return LlmFallbackDecision(
            shouldFallback=False,
            fromProvider=from_provider,
            toProvider=None,
            reason=f"{from_provider.value} does not have a configured fallback target.",
            costSensitive=True,
        )
    if not settings.llmAllowOpenaiFallback:
        return LlmFallbackDecision(
            shouldFallback=False,
            fromProvider=from_provider,
            toProvider=None,
            reason="Provider fallback is disabled by settings.",
            costSensitive=True,
        )
    if settings.openaiDailyRequestLimit <= 0 and error_code != "openai_api_key_missing":
        return LlmFallbackDecision(
            shouldFallback=True,
            fromProvider=LlmProvider.openai,
            toProvider=LlmProvider.ollama,
            reason="OpenAI daily request limit is exhausted; fallback to Ollama is allowed.",
            costSensitive=True,
        )
    ollama_status = get_provider_status(LlmProvider.ollama, settings)
    if not ollama_status.available:
        return LlmFallbackDecision(
            shouldFallback=False,
            fromProvider=from_provider,
            toProvider=None,
            reason=ollama_status.reason,
            costSensitive=True,
        )
    return LlmFallbackDecision(
        shouldFallback=True,
        fromProvider=LlmProvider.openai,
        toProvider=LlmProvider.ollama,
        reason=(
            f"{error_code} is eligible for Ollama fallback after "
            f"{validation_failed_count} validation failure(s)."
        ),
        costSensitive=True,
    )


def select_next_provider(
    current_provider: LlmProvider,
    settings: Settings,
    reason: str,
) -> LlmProvider | None:
    chain = get_provider_chain(settings)
    if current_provider not in chain:
        return None
    decision = should_fallback(
        reason,
        validation_failed_count=0,
        settings=settings,
        from_provider=current_provider,
    )
    if not decision.shouldFallback:
        return None
    current_index = chain.index(current_provider)
    for provider in chain[current_index + 1 :]:
        if provider == decision.toProvider:
            return provider
    return None
