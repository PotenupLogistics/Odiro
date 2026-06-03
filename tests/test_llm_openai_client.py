from __future__ import annotations

import json

import httpx

from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest, LlmProvider
from app.services.llm_openai_client import OpenAILlmClient


def _request() -> LlmGenerationRequest:
    return LlmGenerationRequest(
        provider=LlmProvider.openai,
        model="openai-world-config",
        systemPrompt="system prompt",
        userPrompt="user prompt",
        temperature=0.1,
        maxTokens=1200,
        responseFormat="json_object",
        requestId="REQ-OPENAI-001",
    )


def _settings(api_key: str = "test-key") -> Settings:
    return Settings(
        _env_file=None,
        openaiApiKey=api_key,
        openaiModel="gpt-4o-mini",
        openaiTimeoutSec=30,
        openaiMaxTokens=1200,
        openaiTemperature=0.1,
    )


def test_openai_api_key_missing_does_not_call_api() -> None:
    called = False

    def handler(_request: httpx.Request) -> httpx.Response:
        nonlocal called
        called = True
        return httpx.Response(200, json={})

    response = OpenAILlmClient(
        settings=_settings(api_key=""),
        transport=httpx.MockTransport(handler),
    ).generate(_request())

    assert called is False
    assert response.success is False
    assert response.error is not None
    assert response.error.code == "openai_api_key_missing"


def test_openai_success_response_extracts_output_text_and_usage() -> None:
    captured: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        captured["url"] = str(request.url)
        captured["headers"] = dict(request.headers)
        captured["body"] = json.loads(request.content.decode("utf-8"))
        return httpx.Response(
            200,
            json={
                "output": [
                    {
                        "content": [
                            {
                                "type": "output_text",
                                "text": "{\"schemaVersion\":\"1.0\"}",
                            }
                        ]
                    }
                ],
                "usage": {
                    "input_tokens": 10,
                    "output_tokens": 5,
                    "total_tokens": 15,
                },
            },
        )

    response = OpenAILlmClient(
        settings=_settings(),
        transport=httpx.MockTransport(handler),
    ).generate(_request())

    assert response.success is True
    assert response.content == "{\"schemaVersion\":\"1.0\"}"
    assert response.rawContent is not None
    assert response.usage is not None
    assert response.usage.promptTokens == 10
    assert response.usage.completionTokens == 5
    assert response.usage.totalTokens == 15
    assert captured["url"] == "https://api.openai.com/v1/responses"
    assert captured["headers"]["authorization"] == "Bearer test-key"
    assert captured["body"]["model"] == "gpt-4o-mini"
    assert captured["body"]["text"]["format"]["type"] == "json_schema"
    assert captured["body"]["text"]["format"]["name"] == "world_config"
    assert captured["body"]["max_output_tokens"] == 1200


def test_openai_timeout_returns_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        raise httpx.TimeoutException("timeout")

    response = OpenAILlmClient(
        settings=_settings(),
        transport=httpx.MockTransport(handler),
    ).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "openai_timeout"


def test_openai_rate_limit_returns_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(429, json={"error": {"message": "rate limited"}})

    response = OpenAILlmClient(
        settings=_settings(),
        transport=httpx.MockTransport(handler),
    ).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "openai_rate_limited"


def test_openai_invalid_response_returns_error() -> None:
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(200, content=b"not-json")

    response = OpenAILlmClient(
        settings=_settings(),
        transport=httpx.MockTransport(handler),
    ).generate(_request())

    assert response.success is False
    assert response.error is not None
    assert response.error.code == "openai_invalid_response"


def _policy_recommendation_request() -> LlmGenerationRequest:
    return LlmGenerationRequest(
        provider=LlmProvider.openai,
        model="policy-recommendation",
        systemPrompt="system prompt",
        userPrompt="user prompt",
        temperature=0.1,
        maxTokens=1200,
        responseFormat="json_object",
        requestId="REQ-OPENAI-POLICY-001",
    )


def test_openai_policy_recommendation_uses_generic_json_object_format() -> None:
    captured: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        captured["body"] = json.loads(request.content.decode("utf-8"))
        return httpx.Response(
            200,
            json={
                "output": [
                    {
                        "content": [
                            {
                                "type": "output_text",
                                "text": "{\"recommendations\":[],\"summary\":\"none\"}",
                            }
                        ]
                    }
                ],
            },
        )

    response = OpenAILlmClient(
        settings=_settings(),
        transport=httpx.MockTransport(handler),
    ).generate(_policy_recommendation_request())

    assert response.success is True
    # WorldConfig schema 분기 침범하지 않고 json_object 모드 사용
    assert captured["body"]["text"]["format"]["type"] == "json_object"
    assert "name" not in captured["body"]["text"]["format"]
    assert "schema" not in captured["body"]["text"]["format"]
    # model 라벨은 settings의 openaiModel로 정규화
    assert captured["body"]["model"] == "gpt-4o-mini"
    assert response.model == "gpt-4o-mini"


def test_openai_world_config_request_still_uses_json_schema_format() -> None:
    """기존 WorldConfig 경로가 깨지지 않는지 회귀 검증."""
    captured: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        captured["body"] = json.loads(request.content.decode("utf-8"))
        return httpx.Response(
            200,
            json={
                "output": [
                    {"content": [{"type": "output_text", "text": "{}"}]}
                ],
            },
        )

    OpenAILlmClient(
        settings=_settings(),
        transport=httpx.MockTransport(handler),
    ).generate(_request())

    assert captured["body"]["text"]["format"]["type"] == "json_schema"
    assert captured["body"]["text"]["format"]["name"] == "world_config"
