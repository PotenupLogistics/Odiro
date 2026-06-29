from __future__ import annotations

from app.agents.common.llm_json_client import AgentLlmJsonClient
from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider


class _CapturingLlmClient:
    """Test double that records the exact generation request."""

    def __init__(self) -> None:
        self.requests: list[LlmGenerationRequest] = []

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        """Return valid JSON while preserving the caller's request object."""
        self.requests.append(request)
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.openai,
            model=request.model,
            success=True,
            content='{"ok": true}',
        )


def test_agent_llm_json_client_raises_scenario_schema_output_budget() -> None:
    client = _CapturingLlmClient()
    agent_client = AgentLlmJsonClient(
        settings=Settings(_env_file=None, openaiMaxTokens=1200),
        client=client,
    )

    response = agent_client.generate_json(
        system_prompt="system",
        user_prompt="user",
        response_name="scenario_graph_template",
        response_schema={"name": "project_scenario_v1", "schema": {"type": "object"}, "strict": True},
    )

    assert response == {"ok": True}
    assert client.requests[0].maxTokens >= 4096


def test_agent_llm_json_client_keeps_default_budget_for_other_schemas() -> None:
    client = _CapturingLlmClient()
    agent_client = AgentLlmJsonClient(
        settings=Settings(_env_file=None, openaiMaxTokens=1200),
        client=client,
    )

    agent_client.generate_json(
        system_prompt="system",
        user_prompt="user",
        response_name="other_json",
        response_schema={"name": "small_json", "schema": {"type": "object"}, "strict": True},
    )

    assert client.requests[0].maxTokens == 1200
