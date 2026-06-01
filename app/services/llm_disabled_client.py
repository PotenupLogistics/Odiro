from __future__ import annotations

from app.models.llm import LlmError, LlmGenerationRequest, LlmGenerationResponse


class DisabledLlmClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=request.provider,
            model=request.model,
            success=False,
            content=None,
            rawContent=None,
            usage=None,
            error=LlmError(
                code="provider_disabled",
                message="LLM provider is disabled. Configure a real provider in a later step.",
            ),
            warnings=["No external LLM API was called."],
        )
