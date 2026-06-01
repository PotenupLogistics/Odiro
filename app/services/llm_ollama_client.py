from __future__ import annotations

from typing import Any

import httpx

from app.core.settings import Settings
from app.models.llm import LlmError, LlmGenerationRequest, LlmGenerationResponse, LlmProvider, LlmUsage


class OllamaLlmClient:
    def __init__(
        self,
        settings: Settings | None = None,
        transport: httpx.BaseTransport | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.transport = transport

    def _endpoint(self) -> str:
        return self.settings.ollamaBaseUrl.rstrip("/") + "/api/chat"

    def _request_body(self, request: LlmGenerationRequest) -> dict[str, Any]:
        return {
            "model": self.settings.ollamaModel or request.model,
            "messages": [
                {"role": "system", "content": request.systemPrompt},
                {"role": "user", "content": request.userPrompt},
            ],
            "stream": False,
            "format": "json" if request.responseFormat == "json_object" else request.responseFormat,
            "options": {
                "temperature": request.temperature,
                "num_predict": request.maxTokens,
            },
        }

    def _error_response(
        self,
        request: LlmGenerationRequest,
        code: str,
        message: str,
    ) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.ollama,
            model=request.model,
            success=False,
            content=None,
            rawContent=None,
            usage=None,
            error=LlmError(code=code, message=message),
            warnings=[],
        )

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        try:
            with httpx.Client(
                timeout=request.timeoutSec or self.settings.ollamaTimeoutSec,
                transport=self.transport,
            ) as client:
                response = client.post(self._endpoint(), json=self._request_body(request))
        except httpx.TimeoutException as exc:
            return self._error_response(request, "ollama_timeout", str(exc) or "Ollama request timed out.")
        except httpx.ConnectError as exc:
            return self._error_response(
                request,
                "ollama_connection_failed",
                str(exc) or "Failed to connect to Ollama.",
            )
        except httpx.HTTPError as exc:
            return self._error_response(request, "ollama_connection_failed", str(exc))

        if response.status_code < 200 or response.status_code >= 300:
            return self._error_response(
                request,
                "ollama_http_error",
                f"Ollama returned HTTP {response.status_code}.",
            )

        try:
            payload = response.json()
        except ValueError as exc:
            return self._error_response(request, "ollama_invalid_response", str(exc))

        message = payload.get("message")
        content = message.get("content") if isinstance(message, dict) else None
        if not isinstance(content, str) or not content.strip():
            return self._error_response(
                request,
                "ollama_missing_content",
                "Ollama response did not include message.content.",
            )

        prompt_tokens = payload.get("prompt_eval_count")
        completion_tokens = payload.get("eval_count")
        usage = None
        if isinstance(prompt_tokens, int) or isinstance(completion_tokens, int):
            usage = LlmUsage(
                promptTokens=prompt_tokens if isinstance(prompt_tokens, int) else None,
                completionTokens=completion_tokens if isinstance(completion_tokens, int) else None,
                totalTokens=(
                    (prompt_tokens if isinstance(prompt_tokens, int) else 0)
                    + (completion_tokens if isinstance(completion_tokens, int) else 0)
                ),
            )

        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.ollama,
            model=request.model,
            success=True,
            content=content,
            rawContent=content,
            usage=usage,
            error=None,
            warnings=[],
        )
