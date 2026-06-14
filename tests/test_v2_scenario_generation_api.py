from __future__ import annotations

from fastapi.testclient import TestClient

from app.agents.common.json_response_parser import parse_json_response
from app.agents.scenario_generation_v2 import ScenarioGenerationV2Agent
from app.core.settings import Settings
from app.main import app
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request


class _FakeJsonClient:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []

    def generate_json(self, *, system_prompt: str, user_prompt: str, response_name: str):
        self.calls.append(
            {
                "system_prompt": system_prompt,
                "user_prompt": user_prompt,
                "response_name": response_name,
            }
        )
        response = self.responses.pop(0)
        if isinstance(response, Exception):
            raise response
        return response


def _llm_template(scenario_id: str = "llm_narrow_sidewalk") -> dict:
    return {
        "schema": "scenario_template",
        "version": 2,
        "scenario_id": scenario_id,
        "intent": {
            "summary": "LLM generated narrow sidewalk risk template",
            "risk_factors": ["narrow_sidewalk", "static_obstacle_ahead"],
        },
        "ground_model": {"default_region_type": "walkable"},
        "robot": {"start_area": {}, "goal_area": {}},
        "static_obstacles": {"count": {"min": 1, "max": 2}},
        "pedestrians": {"count": {"min": 0, "max": 0}},
    }


def test_v2_scenario_generate_accepts_prompt_only() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물과 보행자 횡단 위험 시나리오를 만들어줘."},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["schema"] == "scenario_generate_response_v2"
    assert payload["version"] == 2
    assert payload["status"] == "success"
    assert payload["scenario_id"]
    assert isinstance(payload["scenario_template"], dict)
    assert payload["validation"]["valid"] is True
    assert payload["validation"]["errors"] == []
    assert isinstance(payload["assumptions"], list)


def test_v2_scenario_generate_rejects_blank_prompt() -> None:
    response = TestClient(app).post("/api/v2/scenarios/generate", json={"prompt": "   "})

    assert response.status_code == 422


def test_v2_scenario_generate_forbids_episode_count() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "test", "episode_count": 2},
    )

    assert response.status_code == 422


def test_v2_scenario_generate_forbids_run_count_aliases() -> None:
    client = TestClient(app)

    for field in ("count", "iterations", "run_count"):
        response = client.post(
            "/api/v2/scenarios/generate",
            json={"prompt": "test", field: 2},
        )

        assert response.status_code == 422


def test_v2_scenario_generate_returns_minimum_template_structure() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물이 있고 보행자가 가로지르는 상황"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    template = payload["scenario_template"]
    validation = payload["validation"]

    assert template["schema"] == "scenario_template"
    assert template["version"] == 2
    assert template["scenario_id"]
    assert template["intent"]["summary"]
    assert "ground_model" in template
    assert "robot" in template
    assert template["static_obstacles"]["count"]["min"] >= 1
    assert template["pedestrians"]["count"]["min"] >= 1
    assert isinstance(validation["errors"], list)
    assert isinstance(validation["warnings"], list)


def test_v2_scenario_agent_uses_deterministic_mode_when_llm_disabled() -> None:
    fake = _FakeJsonClient([_llm_template()])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "deterministic"
    assert fake.calls == []
    assert response.scenario_template is not None
    assert response.scenario_template["scenario_id"] != "llm_narrow_sidewalk"


def test_v2_scenario_agent_uses_llm_template_when_enabled() -> None:
    fake = _FakeJsonClient([_llm_template()])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm"
    assert response.scenario_id == "llm_narrow_sidewalk"
    assert response.scenario_template is not None
    assert response.scenario_template["intent"]["summary"]
    assert fake.calls[0]["response_name"] == "scenario_template_v2"


def test_v2_scenario_agent_repairs_invalid_llm_template() -> None:
    invalid_template = {"schema": "scenario_template", "version": 2}
    repaired_template = _llm_template("llm_repaired_template")
    fake = _FakeJsonClient([invalid_template, repaired_template])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm_repaired"
    assert response.scenario_id == "llm_repaired_template"
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "scenario_template_v2_repair"


def test_v2_scenario_agent_falls_back_when_llm_and_repair_fail() -> None:
    fake = _FakeJsonClient([ValueError("bad json"), ValueError("repair bad json")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "fallback"
    assert response.status == "success"
    assert response.validation.valid is True
    assert response.scenario_template is not None
    assert any(
        warning.message == "LLM output validation failed; deterministic fallback template was used."
        for warning in response.validation.warnings
    )


def test_v2_agent_json_parser_accepts_markdown_json_block() -> None:
    parsed = parse_json_response('```json\n{"schema": "scenario_template", "version": 2}\n```')

    assert parsed == {"schema": "scenario_template", "version": 2}


def test_v2_scenario_generate_openapi_requires_only_prompt() -> None:
    schema = TestClient(app).get("/openapi.json").json()
    request_ref = schema["paths"]["/api/v2/scenarios/generate"]["post"]["requestBody"]["content"][
        "application/json"
    ]["schema"]["$ref"]
    component_name = request_ref.rsplit("/", 1)[-1]
    request_schema = schema["components"]["schemas"][component_name]

    assert request_schema["required"] == ["prompt"]
    assert set(request_schema["properties"]) == {"prompt"}


def test_v1_scenario_generate_still_exists_in_openapi() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    assert "/api/v1/scenarios/generate" in schema["paths"]
