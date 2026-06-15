from __future__ import annotations

from fastapi.testclient import TestClient

from app.agents.common.json_response_parser import parse_json_response
from app.agents.scenario_generation_v2 import ScenarioGenerationV2Agent
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2
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
        "version": 1,
        "template_id": scenario_id,
        "intent": "LLM generated narrow sidewalk risk template",
        "corridor": {
            "axis": {"type": "polyline", "points_m": [[0.0, 0.0], [18.0, 0.0]]},
            "walkway_width_m": {"min": 1.4, "max": 1.8},
            "building_side": [{"surface": "wall", "width_m": 0.3}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": [
                {"id": "approach", "type": "straight", "along_range_m": [0.0, 5.0]},
                {"id": "conflict", "type": "narrowing", "along_range_m": [5.0, 11.0]},
                {"id": "exit", "type": "straight", "along_range_m": [11.0, 18.0]},
            ],
        },
        "obstacles": {"min_clear_width_m": 0.9, "placements": []},
        "pedestrians": {
            "background": {"count": {"min": 0, "max": 1}, "speed_mps": {"min": 0.8, "max": 1.2}},
            "encounters": [],
        },
        "robot": {"start": {"type": "entry"}, "goal": {"type": "exit"}},
    }


def test_v2_scenario_generate_accepts_prompt_only() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물과 보행자 횡단 위험 시나리오를 만들어줘."},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["status"] == "success"
    assert payload["template_id"]
    assert isinstance(payload["template"], dict)
    assert "template_path" not in payload
    assert payload["validation"]["valid"] is True
    assert payload["validation"]["errors"] == []
    assert isinstance(payload["assumptions"], list)


def test_v2_scenario_graph_runner_uses_langgraph_compile_invoke() -> None:
    runner = ScenarioGenerationGraphRunnerV2(settings=Settings(_env_file=None, v2AgentLlmEnabled=False))

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert runner.compiled_graph is not None
    assert runner.last_state["output"] is response
    assert response.template is not None
    assert response.template["schema"] == "scenario_template"
    forbidden_template_fields = {"ground_model", "static_obstacles", "scenario_id", "sample_count", "base_seed"}
    assert forbidden_template_fields.isdisjoint(response.template)


def test_v2_scenario_graph_endpoint_keeps_prompt_only_contract(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["generation_mode"] == "langgraph"
    assert payload["template"]["schema"] == "scenario_template"
    assert "template_path" not in payload
    assert "scenario_path" not in payload
    assert "sample_id" not in payload
    assert "generated_count" not in payload
    assert "ue_payload" not in payload["template"]
    assert "policy" not in payload["template"]
    assert "robot_setup" not in payload["template"]


def test_v2_scenario_endpoint_uses_langgraph_when_graph_flag_is_false(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_GRAPH_ENABLED", "false")
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    assert payload["generation_mode"] == "langgraph"
    assert payload["validation"]["valid"] is True


def test_v2_scenario_graph_calls_llm_node_and_falls_back_when_invalid() -> None:
    fake = _FakeJsonClient([{"schema": "scenario_template", "version": 1}])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.validation.valid is True
    assert response.template is not None
    assert response.template["schema"] == "scenario_template"
    assert fake.calls
    assert fake.calls[0]["response_name"] == "scenario_graph_intent"
    assert any(
        warning.message == "LLM output validation failed; deterministic fallback template was used."
        for warning in response.validation.warnings
    )


def test_v2_scenario_graph_uses_valid_llm_template_without_fallback_warning() -> None:
    fake = _FakeJsonClient([_llm_template("valid_llm_template")])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.template_id == "valid_llm_template"
    assert response.validation.valid is True
    assert response.template is not None
    assert response.template["template_id"] == "valid_llm_template"
    assert not any("fallback template" in warning.message for warning in response.validation.warnings)


def test_v2_scenario_llm_prompt_includes_validator_required_template_shape() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(_env_file=None, v2AgentLlmEnabled=True))

    prompt = agent._template_user_prompt("좁은 보도에서 대향 보행자")

    required_fragments = [
        '"schema": "scenario_template"',
        '"version": 1',
        '"axis":',
        '"type": "polyline"',
        '"points_m"',
        '"walkway_width_m"',
        '"segments"',
        '"obstacles":',
        '"placements": []',
        '"pedestrians":',
        '"encounters"',
        '"robot":',
        '"start": {"type": "entry"}',
        '"goal": {"type": "exit"}',
        "pedestrians.path",
        "robot.start_area",
        "robot.goal_area",
    ]
    for fragment in required_fragments:
        assert fragment in prompt


def test_v2_scenario_graph_endpoint_returns_same_template_for_same_prompt(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")
    client = TestClient(app)
    request = {"prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"}

    first = client.post("/api/v2/scenarios/generate", json=request)
    second = client.post("/api/v2/scenarios/generate", json=request)

    assert first.status_code == 200, first.text
    assert second.status_code == 200, second.text
    first_payload = first.json()
    second_payload = second.json()
    assert first_payload["generation_mode"] == "langgraph"
    assert second_payload["generation_mode"] == "langgraph"
    assert first_payload["template_id"] == second_payload["template_id"]
    assert first_payload["template"] == second_payload["template"]
    assert first_payload["template"]["schema"] == "scenario_template"
    assert second_payload["template"]["schema"] == "scenario_template"
    assert first_payload["validation"]["valid"] is True
    assert second_payload["validation"]["valid"] is True
    forbidden_fields = {
        "base_seed",
        "experiment_id",
        "generated_count",
        "run_id",
        "sample_count",
        "sample_id",
        "scenario_path",
        "seed",
        "template_path",
    }
    assert forbidden_fields.isdisjoint(first_payload)
    assert forbidden_fields.isdisjoint(second_payload)
    assert forbidden_fields.isdisjoint(first_payload["template"])
    assert forbidden_fields.isdisjoint(second_payload["template"])


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
    template = payload["template"]
    validation = payload["validation"]

    assert template["schema"] == "scenario_template"
    assert template["version"] == 1
    assert template["template_id"]
    assert template["intent"]
    assert "corridor" in template
    assert "robot" in template
    assert "ground_model" not in template
    assert "static_obstacles" not in template
    assert "path" not in template["pedestrians"]
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
    assert response.template is not None
    assert response.template["template_id"] != "llm_narrow_sidewalk"


def test_v2_scenario_agent_uses_llm_template_when_enabled() -> None:
    fake = _FakeJsonClient([_llm_template()])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm"
    assert response.template_id == "llm_narrow_sidewalk"
    assert response.template is not None
    assert response.template["intent"]
    assert fake.calls[0]["response_name"] == "scenario_template"


def test_v2_scenario_agent_repairs_invalid_llm_template() -> None:
    invalid_template = {"schema": "scenario_template", "version": 1}
    repaired_template = _llm_template("llm_repaired_template")
    fake = _FakeJsonClient([invalid_template, repaired_template])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm_repaired"
    assert response.template_id == "llm_repaired_template"
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "scenario_template_repair"


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
    assert response.template is not None
    assert any(
        warning.message == "LLM output validation failed; deterministic fallback template was used."
        for warning in response.validation.warnings
    )


def test_v2_scenario_generate_uses_current_template_contract() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 로봇 전방 장애물이 있고 보행자가 옆에서 지나가는 위험 상황"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    template = payload["template"]
    segment_ids = {segment["id"] for segment in template["corridor"]["segments"]}

    assert payload["template_id"] == template["template_id"]
    assert {"schema", "version", "template_id", "intent", "corridor", "robot"} <= set(template)
    assert all(placement["at"]["segment"] in segment_ids for placement in template["obstacles"]["placements"])
    assert all(encounter["at"] in segment_ids for encounter in template["pedestrians"]["encounters"])
    assert "encounters" in template["pedestrians"]
    assert "placements" in template["obstacles"]
    forbidden = {"sample_count", "base_seed", "experiment_id", "run_id", "scenario_id"}
    assert forbidden.isdisjoint(template)


def test_v2_validator_rejects_catalog_violations() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    template = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 보행자가 가로지르는 상황")).template
    assert template is not None
    template["corridor"]["building_side"][0]["surface"] = "marble"
    template["pedestrians"]["encounters"][0]["type"] = "teleport"
    template["pedestrians"]["encounters"][0]["persona"] = "rude"

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "corridor.building_side[0].surface" in fields
    assert "pedestrians.encounters[0].type" in fields
    assert "pedestrians.encounters[0].persona" in fields


def test_v2_agent_json_parser_accepts_markdown_json_block() -> None:
    parsed = parse_json_response('```json\n{"schema": "scenario_template", "version": 1}\n```')

    assert parsed == {"schema": "scenario_template", "version": 1}


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
