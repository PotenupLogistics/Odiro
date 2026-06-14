from __future__ import annotations

from typing import Protocol
from uuid import uuid4

from app.agents.common.json_response_parser import parse_json_response
from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest
from app.services.llm_client import BaseLlmClient
from app.services.llm_client_factory import create_llm_client, get_configured_provider_chain


class AgentLlmClient(Protocol):
    def generate_json(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        response_name: str,
    ) -> dict:
        ...


class AgentLlmJsonClient:
    def __init__(
        self,
        *,
        settings: Settings | None = None,
        client: BaseLlmClient | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.provider = get_configured_provider_chain(self.settings)[0]
        self.client = client or create_llm_client(self.provider, settings=self.settings)

    def generate_json(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        response_name: str,
    ) -> dict:
        response = self.client.generate(
            LlmGenerationRequest(
                provider=self.provider,
                model=self._model_name(),
                systemPrompt=system_prompt,
                userPrompt=user_prompt,
                temperature=self.settings.openaiTemperature,
                maxTokens=self.settings.openaiMaxTokens,
                responseFormat="json_object",
                requestId=f"{response_name}-{uuid4().hex}",
                timeoutSec=self.settings.openaiTimeoutSec,
            )
        )
        if not response.success or response.content is None:
            message = response.error.message if response.error else "LLM generation failed."
            raise ValueError(message)
        return parse_json_response(response.content)

    def _model_name(self) -> str:
        if self.provider.value == "ollama":
            return self.settings.ollamaModel
        return self.settings.openaiModel
