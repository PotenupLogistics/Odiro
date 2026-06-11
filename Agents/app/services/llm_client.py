from __future__ import annotations

from typing import Protocol

from app.models.llm import LlmGenerationRequest, LlmGenerationResponse


class BaseLlmClient(Protocol):
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        """Build an LLM generation response for the given request."""
