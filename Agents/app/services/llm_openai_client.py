from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import httpx

from app.core.settings import Settings
from app.models.llm import LlmError, LlmGenerationRequest, LlmGenerationResponse, LlmProvider, LlmUsage


AGENTS_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = AGENTS_ROOT.parent
WORLD_CONFIG_SCHEMA_PATH = REPO_ROOT / "contracts" / "schemas" / "world_config.schema.json"

# request.model 값이 이 집합이면 자유형 json_object 모드. 그 외(빈문자열/openai-world-config 포함)는 기존 WorldConfig schema 모드.
GENERIC_JSON_MODELS = {"policy-recommendation", "integrated-recommendation"}


class OpenAILlmClient:
    def __init__(
        self,
        settings: Settings | None = None,
        transport: httpx.BaseTransport | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.transport = transport

    def _endpoint(self) -> str:
        return "https://api.openai.com/v1/responses"

    def _world_config_schema(self) -> dict[str, Any]:
        schema = json.loads(WORLD_CONFIG_SCHEMA_PATH.read_text(encoding="utf-8"))
        schema.pop("$schema", None)
        schema.pop("$id", None)
        return schema

    def _resolved_model(self, request: LlmGenerationRequest) -> str:
        if request.model in {"openai-world-config", ""} or request.model in GENERIC_JSON_MODELS:
            return self.settings.openaiModel
        return request.model

    def _request_body(self, request: LlmGenerationRequest) -> dict[str, Any]:
        body: dict[str, Any] = {
            "model": self._resolved_model(request),
            "input": [
                {"role": "system", "content": request.systemPrompt},
                {"role": "user", "content": request.userPrompt},
            ],
            "temperature": self.settings.openaiTemperature,
            "max_output_tokens": self.settings.openaiMaxTokens,
        }
        if request.responseJsonSchema is not None:
            body["text"] = {
                "format": {
                    "type": "json_schema",
                    "name": request.responseSchemaName or "structured_output",
                    "schema": request.responseJsonSchema,
                    "strict": request.responseSchemaStrict,
                }
            }
        elif request.model in GENERIC_JSON_MODELS:
            body["text"] = {"format": {"type": "json_object"}}
        else:
            body["text"] = {
                "format": {
                    "type": "json_schema",
                    "name": "world_config",
                    "description": "Validated WorldConfig JSON object for Proto-AI.",
                    "schema": self._world_config_schema(),
                    "strict": False,
                }
            }
        return body

    def _response_model_label(self, request: LlmGenerationRequest) -> str:
        if request.model == "openai-world-config" or request.model in GENERIC_JSON_MODELS:
            return self.settings.openaiModel
        return request.model

    def _error_response(
        self,
        request: LlmGenerationRequest,
        code: str,
        message: str,
    ) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.openai,
            model=self._response_model_label(request),
            success=False,
            content=None,
            rawContent=None,
            usage=None,
            error=LlmError(code=code, message=message),
            warnings=[],
        )

    def _extract_text(self, payload: dict[str, Any]) -> str | None:
        output_text = payload.get("output_text")
        if isinstance(output_text, str) and output_text.strip():
            return output_text
        for item in payload.get("output") or []:
            if not isinstance(item, dict):
                continue
            for content_item in item.get("content") or []:
                if not isinstance(content_item, dict):
                    continue
                text = content_item.get("text")
                if isinstance(text, str) and text.strip():
                    return text
        return None

    def _usage(self, payload: dict[str, Any]) -> LlmUsage | None:
        usage = payload.get("usage")
        if not isinstance(usage, dict):
            return None
        prompt_tokens = usage.get("input_tokens")
        completion_tokens = usage.get("output_tokens")
        total_tokens = usage.get("total_tokens")
        return LlmUsage(
            promptTokens=prompt_tokens if isinstance(prompt_tokens, int) else None,
            completionTokens=completion_tokens if isinstance(completion_tokens, int) else None,
            totalTokens=total_tokens if isinstance(total_tokens, int) else None,
        )

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        api_key = self.settings.openaiApiKey.strip()
        if not api_key:
            return self._error_response(
                request,
                "openai_api_key_missing",
                "OpenAI API key is not configured.",
            )

        try:
            with httpx.Client(
                timeout=request.timeoutSec or self.settings.openaiTimeoutSec,
                transport=self.transport,
            ) as client:
                response = client.post(
                    self._endpoint(),
                    headers={
                        "Authorization": f"Bearer {api_key}",
                        "Content-Type": "application/json",
                    },
                    json=self._request_body(request),
                )
        except httpx.TimeoutException as exc:
            return self._error_response(request, "openai_timeout", str(exc) or "OpenAI request timed out.")
        except httpx.HTTPError as exc:
            return self._error_response(request, "openai_provider_error", str(exc))

        if response.status_code == 429:
            return self._error_response(request, "openai_rate_limited", "OpenAI returned HTTP 429.")
        if response.status_code < 200 or response.status_code >= 300:
            return self._error_response(
                request,
                "openai_http_error",
                f"OpenAI returned HTTP {response.status_code}.",
            )

        try:
            payload = response.json()
        except ValueError as exc:
            return self._error_response(request, "openai_invalid_response", str(exc))

        content = self._extract_text(payload)
        if not content:
            return self._error_response(
                request,
                "openai_missing_content",
                "OpenAI response did not include output text.",
            )

        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.openai,
            model=self._response_model_label(request),
            success=True,
            content=content,
            rawContent=json.dumps(payload, ensure_ascii=False),
            usage=self._usage(payload),
            error=None,
            warnings=[],
        )
