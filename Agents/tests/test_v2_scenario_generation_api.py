from __future__ import annotations

from fastapi.testclient import TestClient

from app.agents.common.json_response_parser import parse_json_response
from app.agents.scenario_generation_v2 import ScenarioGenerationV2Agent
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2
from app.agents.scenario_generation_v2.scenario_template_schema import scenario_template_v1_json_schema
from app.core.settings import Settings
from app.main import app
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request


class _FakeJsonClient:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []

    def generate_json(self, *, system_prompt: str, user_prompt: str, response_name: str, response_schema=None):
        self.calls.append(
            {
                "system_prompt": system_prompt,
                "user_prompt": user_prompt,
                "response_name": response_name,
                "response_schema": response_schema,
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


def _assert_raw_scenario_template(payload: dict) -> None:
    assert payload["schema"] == "scenario_template"
    assert payload["version"] == 1
    assert payload["template_id"]
    assert payload["intent"]
    assert "corridor" in payload
    assert "obstacles" in payload
    assert "pedestrians" in payload
    assert "robot" in payload
    wrapper_fields = {"status", "summary", "template", "validation", "assumptions", "generation_mode"}
    assert wrapper_fields.isdisjoint(payload)
    legacy_fields = {"ground_model", "static_obstacles"}
    assert legacy_fields.isdisjoint(payload)
    assert "path" not in payload["pedestrians"]


def test_v2_scenario_generate_accepts_prompt_only() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물과 보행자 횡단 위험 시나리오를 만들어줘."},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_raw_scenario_template(payload)
    assert "template_path" not in payload


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
    _assert_raw_scenario_template(payload)
    assert "template_path" not in payload
    assert "scenario_path" not in payload
    assert "sample_id" not in payload
    assert "generated_count" not in payload
    assert "ue_payload" not in payload
    assert "policy" not in payload
    assert "robot_setup" not in payload


def test_v2_scenario_endpoint_uses_langgraph_when_graph_flag_is_false(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_GRAPH_ENABLED", "false")
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_raw_scenario_template(payload)


def test_v2_scenario_graph_calls_llm_repair_and_falls_back_when_invalid() -> None:
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
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "scenario_graph_repair"
    assert any(
        warning.message == "LLM-assisted repair failed; deterministic fallback template was used."
        for warning in response.validation.warnings
    )


def test_v2_scenario_template_schema_includes_validator_required_shape() -> None:
    schema = scenario_template_v1_json_schema()

    assert schema["type"] == "object"
    assert set(schema["required"]) == {
        "schema",
        "version",
        "template_id",
        "intent",
        "corridor",
        "obstacles",
        "pedestrians",
        "robot",
    }
    assert schema["properties"]["schema"]["const"] == "scenario_template"
    assert schema["properties"]["version"]["const"] == 1
    corridor = schema["properties"]["corridor"]
    assert {"axis", "walkway_width_m", "building_side", "curb_side", "segments"} <= set(corridor["required"])
    assert corridor["properties"]["axis"]["properties"]["type"]["const"] == "polyline"
    assert "placements" in schema["properties"]["obstacles"]["required"]
    encounter = schema["properties"]["pedestrians"]["properties"]["encounters"]["items"]
    assert set(encounter["properties"]["type"]["enum"]) >= {"oncoming_pass", "cross_path"}
    assert set(encounter["properties"]["persona"]["enum"]) >= {"normal", "assertive"}
    assert {"start", "goal"} <= set(schema["properties"]["robot"]["required"])


def test_v2_scenario_template_contract_accepts_corridor_pose_and_fixed_numbers() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["corridor"]["walkway_width_m"] = 1.6
    template["obstacles"]["min_clear_width_m"] = 0.95
    template["pedestrians"]["background"]["count"] = 1
    template["pedestrians"]["background"]["speed_mps"] = 1.0
    template["pedestrians"]["background"]["spawn_zone"] = {"segments": ["approach", "exit"]}
    template["robot"]["start"] = {
        "type": "corridor_pose",
        "segment": "approach",
        "along_m": 1.0,
        "offset_m": 0.0,
        "lane": "walkway",
        "heading": "forward",
    }
    template["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 15.0,
        "offset_m": {"min": -0.1, "max": 0.1},
        "lane": "walkway",
        "heading": "auto",
    }
    template["pedestrians"]["encounters"][0]["overrides"] = {
        "cooperation": {"min": 0.15, "max": 0.4},
        "personal_space_m": {"min": 0.6, "max": 0.9},
        "awareness_horizon_s": 2.0,
    }
    template["pedestrians"]["encounters"][0]["meet_offset_m"] = {"min": -0.1, "max": 0.1}
    template["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "traffic_cone_01",
            "at": {
                "segment": "conflict",
                "along_m": 7.0,
                "offset_m": 0.0,
                "lane": "center",
            },
            "yaw_deg": 0,
            "allow_blocking": True,
        }
    ]

    validation = agent.validator.validate(template)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_template_contract_rejects_invalid_corridor_pose_and_spawn_zone() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "missing",
        "along_m": 99.0,
        "offset_m": 0.0,
    }
    template["pedestrians"]["background"]["spawn_zone"] = {"segments": ["missing"]}

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.goal.segment" in fields
    assert "pedestrians.background.spawn_zone.segments[0]" in fields


def test_v2_scenario_template_contract_rejects_corridor_pose_along_out_of_range() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 99.0,
        "offset_m": 0.0,
    }

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.goal.along_m" in fields


def test_v2_scenario_template_contract_accepts_abstract_robot_anchors() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"] = {"start": {"type": "entry"}, "goal": {"type": "exit"}}

    validation = agent.validator.validate(template)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_template_contract_rejects_mixed_abstract_robot_anchors() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"] = {
        "start": {
            "type": "entry",
            "segment": "approach",
            "along_m": 1.0,
            "offset_m": 0.0,
            "lane": "center",
            "heading": "forward",
        },
        "goal": {
            "type": "exit",
            "segment": "exit",
            "along_m": {"min": 14.5, "max": 17.5},
            "offset_m": 0.0,
        },
    }

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.start.segment" in fields
    assert "robot.start.along_m" in fields
    assert "robot.start.offset_m" in fields
    assert "robot.goal.segment" in fields
    assert "robot.goal.along_m" in fields
    assert "robot.goal.offset_m" in fields


def test_v2_scenario_template_contract_rejects_corridor_pose_missing_required_pose_fields() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"]["start"] = {"type": "corridor_pose", "segment": "approach", "along_m": 1.0}

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.start.offset_m" in fields


def test_v2_repair_handler_converts_mixed_abstract_anchor_to_corridor_pose() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"]["start"] = {
        "type": "entry",
        "segment": "approach",
        "along_m": {"min": 0.5, "max": 1.5},
        "offset_m": 0.0,
        "lane": "center",
        "heading": "forward",
    }
    template["robot"]["goal"] = {
        "type": "exit",
        "lane": "center",
        "heading": "forward",
    }

    repaired = agent.repair_handler.repair(template)

    assert repaired["robot"]["start"] == {
        "type": "corridor_pose",
        "segment": "approach",
        "along_m": {"min": 0.5, "max": 1.5},
        "offset_m": 0.0,
        "lane": "center",
        "heading": "forward",
    }
    assert repaired["robot"]["goal"] == {"type": "exit"}
    assert agent.validator.validate(repaired).valid is True


def test_v2_scenario_template_structured_schema_allows_contract_extensions() -> None:
    schema = scenario_template_v1_json_schema()

    walkway_width = schema["properties"]["corridor"]["properties"]["walkway_width_m"]
    assert {"type": "number"} in walkway_width["anyOf"]
    placement = schema["properties"]["obstacles"]["properties"]["placements"]["items"]
    assert "allow_blocking" in placement["required"]
    assert "allow_blocking" in placement["properties"]
    assert set(placement["properties"]["kind"]["enum"]) >= {"fixed", "pattern", "scatter"}
    assert {"pattern", "zone", "density_per_10m", "palette"} <= set(placement["properties"])
    background = schema["properties"]["pedestrians"]["properties"]["background"]
    assert "spawn_zone" in background["required"]
    assert "spawn_zone" in background["properties"]
    encounter = schema["properties"]["pedestrians"]["properties"]["encounters"]["items"]
    override_keys = encounter["properties"]["overrides"]["properties"]
    assert {
        "cooperation",
        "evasiveness",
        "personal_space_m",
        "awareness_horizon_s",
        "max_yield_wait_s",
        "sidestep_distance_m",
    } <= set(override_keys)
    robot_anchor = schema["properties"]["robot"]["properties"]["goal"]
    robot_anchor_variants = robot_anchor["anyOf"]
    assert {"entry", "exit", "corridor_pose"} == {variant["properties"]["type"]["const"] for variant in robot_anchor_variants}
    corridor_pose_variant = next(
        variant for variant in robot_anchor_variants if variant["properties"]["type"]["const"] == "corridor_pose"
    )
    assert {"type", "segment", "along_m", "offset_m", "lane", "heading"} <= set(corridor_pose_variant["required"])
    assert {"segment", "along_m", "offset_m", "lane", "heading"} <= set(corridor_pose_variant["properties"])


def test_v2_repair_handler_removes_nullable_structured_output_fields() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["robot"]["start"] = {
        "type": "entry",
        "segment": None,
        "along_m": None,
        "offset_m": None,
        "lane": None,
        "heading": None,
    }
    template["pedestrians"]["background"]["spawn_zone"] = None
    template["pedestrians"]["encounters"][0]["overrides"]["cooperation"] = None
    template["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "traffic_cone_01",
            "at": {
                "segment": "conflict",
                "along_m": 7.0,
                "offset_m": 0.0,
                "lane": "center",
            },
            "yaw_deg": 0,
            "allow_blocking": None,
        }
    ]

    repaired = agent.repair_handler.repair(template)

    assert repaired["robot"]["start"] == {"type": "entry"}
    assert "spawn_zone" not in repaired["pedestrians"]["background"]
    assert "cooperation" not in repaired["pedestrians"]["encounters"][0]["overrides"]
    assert "allow_blocking" not in repaired["obstacles"]["placements"][0]


def test_v2_scenario_template_contract_accepts_pattern_and_scatter_placements() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "traffic_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.0, "lane": "center"},
            "yaw_deg": {"min": -5.0, "max": 5.0},
            "allow_blocking": False,
        },
        {
            "kind": "pattern",
            "id": "gate_obstacles",
            "pattern": "gate",
            "prop": "traffic_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.0, "lane": "across"},
            "count": {"min": 2, "max": 3},
            "spacing_m": 0.6,
            "gap_width_m": {"min": 0.8, "max": 1.0},
            "yaw_deg": 0,
            "allow_blocking": False,
        },
        {
            "kind": "scatter",
            "id": "random_small_obstacles",
            "zone": {"segments": ["approach", "conflict"], "lanes": ["center"]},
            "density_per_10m": {"min": 0.5, "max": 1.0},
            "palette": {"categories": ["small_obstacle"], "classes": ["traffic_cone_01"]},
            "allow_blocking": False,
        },
    ]

    validation = agent.validator.validate(template)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_template_contract_rejects_invalid_pattern_and_scatter_placements() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["obstacles"]["placements"] = [
        {
            "kind": "pattern",
            "id": "bad_pattern",
            "pattern": "gate",
            "at": {"segment": "missing", "along_m": 7.0, "offset_m": 0.0, "lane": "across"},
        },
        {
            "kind": "scatter",
            "id": "bad_scatter",
            "zone": {"segments": ["missing"], "lanes": ["center"]},
        },
    ]

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "obstacles.placements[0].prop" in fields
    assert "obstacles.placements[0].at.segment" in fields
    assert "obstacles.placements[1].density_per_10m" in fields
    assert "obstacles.placements[1].zone.segments[0]" in fields


def test_v2_scenario_template_contract_accepts_all_unreal_override_keys_and_optional_meet_offset() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    encounter = template["pedestrians"]["encounters"][0]
    encounter.pop("meet_offset_m", None)
    encounter["overrides"] = {
        "cooperation": {"min": 0.15, "max": 0.4},
        "evasiveness": {"min": 0.2, "max": 0.5},
        "personal_space_m": {"min": 0.6, "max": 0.9},
        "awareness_horizon_s": {"min": 1.5, "max": 2.5},
        "max_yield_wait_s": 2.0,
        "sidestep_distance_m": 0.4,
    }

    validation = agent.validator.validate(template)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_template_contract_accepts_segment_replaced_by_string_or_choices() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["corridor"]["segments"][0]["replaced_by"] = "grass"
    template["corridor"]["segments"][1]["replaced_by"] = {"choices": ["grass", "road"]}

    validation = agent.validator.validate(template)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_template_contract_rejects_choices_for_catalog_strings() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    template = response.template
    assert template is not None
    template["corridor"]["building_side"][0]["surface"] = {"choices": ["grass", "road"]}
    template["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": {"choices": ["traffic_cone_01"]},
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.0, "lane": {"choices": ["center"]}},
        }
    ]
    template["pedestrians"]["encounters"][0]["persona"] = {"choices": ["assertive"]}
    template["pedestrians"]["encounters"][0]["type"] = {"choices": ["oncoming_pass"]}
    template["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 15.0,
        "offset_m": 0.0,
        "heading": {"choices": ["forward"]},
    }

    validation = agent.validator.validate(template)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "corridor.building_side[0].surface" in fields
    assert "obstacles.placements[0].prop" in fields
    assert "obstacles.placements[0].at.lane" in fields
    assert "pedestrians.encounters[0].persona" in fields
    assert "pedestrians.encounters[0].type" in fields
    assert "robot.goal.heading" in fields


def test_v2_scenario_graph_passes_structured_output_schema_to_llm() -> None:
    fake = _FakeJsonClient([_llm_template("structured_llm_template")])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert fake.calls[0]["response_name"] == "scenario_graph_intent"
    assert fake.calls[0]["response_schema"]["name"] == "scenario_template_v1"
    assert fake.calls[0]["response_schema"]["strict"] is True
    assert fake.calls[0]["response_schema"]["schema"]["properties"]["schema"]["const"] == "scenario_template"


def test_v2_scenario_graph_uses_llm_assisted_repair_when_candidate_invalid() -> None:
    fake = _FakeJsonClient([
        {"schema": "scenario_template", "version": 1},
        _llm_template("llm_repaired_graph_template"),
    ])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.template_id == "llm_repaired_graph_template"
    assert response.validation.valid is True
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "scenario_graph_repair"
    assert "corridor.axis.type" in fake.calls[1]["user_prompt"]
    assert "수정 대상 JSON" in fake.calls[1]["user_prompt"]
    assert fake.calls[1]["response_schema"]["name"] == "scenario_template_v1"


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
    _assert_raw_scenario_template(first_payload)
    _assert_raw_scenario_template(second_payload)
    assert first_payload["template_id"] == second_payload["template_id"]
    assert first_payload == second_payload
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
    _assert_raw_scenario_template(payload)


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
    segment_ids = {segment["id"] for segment in payload["corridor"]["segments"]}

    _assert_raw_scenario_template(payload)
    assert {"schema", "version", "template_id", "intent", "corridor", "robot"} <= set(payload)
    assert all(placement["at"]["segment"] in segment_ids for placement in payload["obstacles"]["placements"])
    assert all(encounter["at"] in segment_ids for encounter in payload["pedestrians"]["encounters"])
    assert "encounters" in payload["pedestrians"]
    assert "placements" in payload["obstacles"]
    forbidden = {"sample_count", "base_seed", "experiment_id", "run_id", "scenario_id"}
    assert forbidden.isdisjoint(payload)


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
    operation = schema["paths"]["/api/v2/scenarios/generate"]["post"]
    request_ref = operation["requestBody"]["content"][
        "application/json"
    ]["schema"]["$ref"]
    component_name = request_ref.rsplit("/", 1)[-1]
    request_schema = schema["components"]["schemas"][component_name]
    response_ref = operation["responses"]["200"]["content"]["application/json"]["schema"]["$ref"]
    response_component_name = response_ref.rsplit("/", 1)[-1]
    response_schema = schema["components"]["schemas"][response_component_name]

    assert request_schema["required"] == ["prompt"]
    assert set(request_schema["properties"]) == {"prompt"}
    assert response_schema["required"] == [
        "schema",
        "version",
        "template_id",
        "intent",
        "corridor",
        "obstacles",
        "pedestrians",
        "robot",
    ]
    assert set(response_schema["properties"]) == set(response_schema["required"])


def test_v1_scenario_generate_still_exists_in_openapi() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    assert "/api/v1/scenarios/generate" in schema["paths"]
