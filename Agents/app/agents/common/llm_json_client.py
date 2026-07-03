from __future__ import annotations

from typing import Any, Protocol
from uuid import uuid4

from app.agents.common.json_response_parser import parse_json_response
from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest
from app.services.llm_client import BaseLlmClient
from app.services.llm_client_factory import create_llm_client, get_configured_provider_chain


# Structured outputs with verbose recommendation text need room to finish valid JSON.
SCENARIO_STRUCTURED_OUTPUT_MIN_TOKENS = 4096
SCENARIO_STRUCTURED_OUTPUT_SCHEMA_NAME = "project_scenario_v1"
RESULT_ANALYSIS_RECOMMENDATION_STRUCTURED_OUTPUT_MIN_TOKENS = 4096
RESULT_ANALYSIS_RECOMMENDATION_STRUCTURED_OUTPUT_SCHEMA_NAME = "analysis_recommendations_v2"


class AgentLlmClient(Protocol):
    def generate_json(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        response_name: str,
        response_schema: dict[str, Any] | None = None,
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
        response_schema: dict[str, Any] | None = None,
    ) -> dict:
        schema = response_schema or {}
        response = self.client.generate(
            LlmGenerationRequest(
                provider=self.provider,
                model=self._model_name(),
                systemPrompt=system_prompt,
                userPrompt=user_prompt,
                temperature=self.settings.openaiTemperature,
                maxTokens=self._max_tokens_for_schema(schema),
                responseFormat="json_object",
                responseJsonSchema=schema.get("schema") if isinstance(schema.get("schema"), dict) else None,
                responseSchemaName=schema.get("name") if isinstance(schema.get("name"), str) else None,
                responseSchemaStrict=bool(schema.get("strict", False)),
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

    def _max_tokens_for_schema(self, schema: dict[str, Any]) -> int:
        """Return the output budget needed for the requested JSON schema."""
        if schema.get("name") == SCENARIO_STRUCTURED_OUTPUT_SCHEMA_NAME:
            return max(self.settings.openaiMaxTokens, SCENARIO_STRUCTURED_OUTPUT_MIN_TOKENS)
        if schema.get("name") == RESULT_ANALYSIS_RECOMMENDATION_STRUCTURED_OUTPUT_SCHEMA_NAME:
            return max(self.settings.openaiMaxTokens, RESULT_ANALYSIS_RECOMMENDATION_STRUCTURED_OUTPUT_MIN_TOKENS)
        return self.settings.openaiMaxTokens
