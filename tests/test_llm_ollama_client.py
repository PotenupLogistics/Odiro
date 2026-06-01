from __future__ import annotations

import json

import httpx

from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest, LlmProvider
from app.services.llm_ollama_client import OllamaLlmClient


def _request() -> LlmGenerationRequest:
    return LlmGenerationRequest(
        provider=LlmProvider.ollama,
        model="llama3.1:8b",
        systemPrompt="system prompt",
        userPrompt="user prompt",
        temperature=0.1,
        maxTokens=1200,
        responseFormat="json_object",
        requestId="REQ-OLLAMA-001",
    )


def _settings() -> Settings:
    return Settings(_env_file=None, ollamaBaseUrl="http://ollama.test", ollamaModel="llama3.1:8b")


def test_ollama_client_builds_expected_request_body() -> None:
    captured: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        captured["url"] = str(request.url)
        captured["body"] = json.loads(request.content.decode("utf-8"))
        return httpx.Response(
            200,
            json={"message": {"content": "{\"worldId\":\"world-1\"}"}},
        )

    response = OllamaLlmClient(settings=_settings(), transport=httpx.MockTransport(handler)).generate(_request())

    assert response.success is True
    assert captured["url"] == "http://ollama.test/api/chat"
    assert captured["body"]["model"] == "llama3.1:8b"
    assert captured["body"]["stream"] is False
    assert captured["body"]["format"] == "json"
    assert captured["body"]["messages"] == [
        {"role": "system", "content": "system prompt"},
        {"role": "user", "content": "user prompt"},
    ]
    assert captured["body"]["options"]["temperature"] == 0.1
    assert captured["body"]["options"]["num_predict"] == 1200


def test_ollama_success_response_extracts_message_content() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(
            200,
            json={
                "message": {"content": "{\"schemaVersion\":\"1.0\"}"},
                "prompt_eval_count": 10,
                "eval_count": 5,
            },
        )

    response = OllamaLlmClient(settings=_settings(), transport=httpx.MockTransport(handler)).generate(_request())

    assert response.success is True
    assert response.content == "{\"schemaVersion\":\"1.0\"}"
    assert response.rawContent == response.content
    assert response.usage is not None
    assert response.usage.promptTokens == 10
    assert response.usage.completionTokens == 5
    assert response.usage.totalTokens == 15


def test_ollama_timeout_returns_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        raise httpx.TimeoutException("timeout")

    response = OllamaLlmClient(settings=_settings(), transport=httpx.MockTransport(handler)).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "ollama_timeout"


def test_ollama_connection_failure_returns_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        raise httpx.ConnectError("connection failed")

    response = OllamaLlmClient(settings=_settings(), transport=httpx.MockTransport(handler)).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "ollama_connection_failed"


def test_ollama_non_2xx_returns_http_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(500, json={"error": "server error"})

    response = OllamaLlmClient(settings=_settings(), transport=httpx.MockTransport(handler)).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "ollama_http_error"


def test_ollama_missing_content_returns_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(200, json={"message": {}})

    response = OllamaLlmClient(settings=_settings(), transport=httpx.MockTransport(handler)).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "ollama_missing_content"


def test_ollama_request_accepts_timeout_override() -> None:
    request = _request().model_copy(update={"timeoutSec": 180})

    assert request.timeoutSec == 180
