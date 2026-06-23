from __future__ import annotations

from copy import deepcopy
from types import SimpleNamespace

from fastapi.testclient import TestClient

from app.agents.common.json_response_parser import parse_json_response
from app.agents.scenario_generation_v2 import ScenarioGenerationV2Agent
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2
from app.agents.scenario_generation_v2.scenario_preset_loader import ScenarioPresetLoader
from app.agents.scenario_generation_v2.scenario_template_schema import project_scenario_v1_json_schema
from app.agents.scenario_generation_v2.template_validator import TemplateValidator
from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids
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


class _ExplodingScenarioTypeSelector:
    def select(self, intent):
        raise AssertionError("deterministic pattern selection should not run before a valid LLM candidate is accepted")


class _MissingScenarioPresetLoader:
    """Test double that simulates an unavailable scenario preset file."""

    def try_load_scenario_preset(self, preset_id: str):
        """Return an optional-load miss without touching bundled static templates."""
        return SimpleNamespace(preset_id=preset_id, scenario=None, error=FileNotFoundError(preset_id))

    def load_scenario_preset(self, preset_id: str):
        """Keep legacy strict-load callers failing so fallback coverage catches them."""
        raise FileNotFoundError(preset_id)


class _InvalidScenarioPresetLoader:
    """Test double that returns a structurally invalid preset object."""

    def try_load_scenario_preset(self, preset_id: str):
        """Return a preset whose robot anchor does not match the corridor segments."""
        return SimpleNamespace(
            preset_id=preset_id,
            scenario={
                "schema": "scenario",
                "version": 1,
                "scenario_id": "invalid_preset_candidate",
                "intent": "Invalid preset used only by tests.",
                "corridor": {
                    "axis": {"type": "polyline", "points_m": [[0.0, 0.0], [4.0, 0.0]]},
                    "walkway_width_m": 3.0,
                    "building_side": [{"surface": "wall", "width_m": 0.5}],
                    "curb_side": [{"surface": "road", "width_m": 4.0}],
                    "segments": [{"id": "main", "type": "straight", "along_range_m": [0.0, 4.0]}],
                },
                "obstacles": {
                    "min_clear_width_m": 0.9,
                    "placements": [
                        {
                            "kind": "fixed",
                            "id": "invalid_prop",
                            "prop": "missing.prop",
                            "at": {"segment": "main", "along_m": 2.0, "offset_m": 0.0, "lane": "walkway"},
                            "yaw_deg": 0,
                        }
                    ],
                },
                "pedestrians": {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []},
                "robot": {
                    "start": {
                        "type": "corridor_pose",
                        "segment": "missing",
                        "along_m": 1.0,
                        "offset_m": 0.0,
                        "lane": "walkway",
                        "heading": "forward",
                    },
                    "goal": {
                        "type": "corridor_pose",
                        "segment": "missing",
                        "along_m": 3.0,
                        "offset_m": 0.0,
                        "lane": "walkway",
                        "heading": "forward",
                    },
                },
            },
            error=None,
        )

    def load_scenario_preset(self, preset_id: str):
        """Preserve a strict-load path for code that has not migrated yet."""
        loaded = self.try_load_scenario_preset(preset_id)
        return loaded.scenario


class _ObjectScenarioPresetLoader:
    """Test double that returns a caller-owned preset object without touching template files."""

    def __init__(self, scenario: dict):
        self.scenario = deepcopy(scenario)

    def try_load_scenario_preset(self, preset_id: str):
        """Return a deepcopy so patching tests cannot mutate the stored fixture."""
        return SimpleNamespace(preset_id=preset_id, scenario=deepcopy(self.scenario), error=None)

    def load_scenario_preset(self, preset_id: str):
        """Return the same object shape as the strict loader path."""
        return deepcopy(self.scenario)


def _llm_scenario(scenario_id: str = "llm_narrow_sidewalk") -> dict:
    return {
        "schema": "scenario",
        "version": 1,
        "scenario_id": scenario_id,
        "intent": "LLM generated narrow sidewalk risk scenario",
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
            "background": {"count": 0, "speed_mps": 1.0},
            "encounters": [],
        },
        "robot": {
            "start": {
                "type": "corridor_pose",
                "segment": "approach",
                "along_m": 1.0,
                "offset_m": 0.0,
                "lane": "walkway",
                "heading": "forward",
            },
            "goal": {
                "type": "corridor_pose",
                "segment": "exit",
                "along_m": 16.0,
                "offset_m": 0.0,
                "lane": "walkway",
                "heading": "forward",
            },
        },
    }


def _llm_scenario_with_obstacle(scenario_id: str = "llm_narrow_sidewalk") -> dict:
    """Return a valid LLM-shaped scenario with one fixed obstacle placement."""
    scenario = _llm_scenario(scenario_id)
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "llm_obstacle_01",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.0, "lane": "walkway"},
            "yaw_deg": 0,
        }
    ]
    return scenario


def _assert_raw_scenario(payload: dict) -> None:
    assert payload["schema"] == "scenario"
    assert payload["version"] == 1
    assert payload["scenario_id"]
    assert payload["intent"]
    assert "corridor" in payload
    assert "obstacles" in payload
    assert "pedestrians" in payload
    assert "robot" in payload
    wrapper_fields = {"status", "summary", "scenario", "validation", "assumptions", "generation_mode"}
    assert wrapper_fields.isdisjoint(payload)
    legacy_fields = {"ground_model", "static_obstacles"}
    assert legacy_fields.isdisjoint(payload)
    assert "path" not in payload["pedestrians"]


def _assert_alpha_static_scenario_contract(payload: dict) -> None:
    """Assert the alpha scenario body favors UE-loadable static obstacle scenes."""
    _assert_raw_scenario(payload)
    assert payload["schema"] == "scenario"
    assert payload["version"] == 1
    assert "template_id" not in payload
    assert payload["corridor"]["axis"]["type"] == "polyline"
    assert len(payload["corridor"]["axis"]["points_m"]) >= 2
    assert payload["corridor"]["walkway_width_m"]
    assert payload["corridor"]["segments"]
    assert payload["pedestrians"]["background"]["count"] == 0
    assert payload["pedestrians"]["encounters"] == []
    for placement in payload["obstacles"]["placements"]:
        assert "segment" not in placement
        assert {"segment", "along_m", "offset_m", "lane"} <= set(placement["at"])
    assert payload["robot"]["start"]["type"] == "corridor_pose"
    assert payload["robot"]["goal"]["type"] == "corridor_pose"


def _assert_curved_road_scenario_contract(payload: dict, *, expect_obstacle: bool) -> None:
    """Assert curved-road prompts produce preset-based corridor geometry."""
    _assert_raw_scenario(payload)
    points = payload["corridor"]["axis"]["points_m"]
    assert len(points) >= 3
    assert len({point[1] for point in points}) > 1
    segment_ranges = {
        segment["id"]: segment["along_range_m"]
        for segment in payload["corridor"]["segments"]
    }
    assert {"entry_straight", "road_curve"} <= set(segment_ranges)
    for anchor in (payload["robot"]["start"], payload["robot"]["goal"]):
        assert anchor["segment"] in segment_ranges
        start_m, end_m = segment_ranges[anchor["segment"]]
        assert start_m <= anchor["along_m"] <= end_m
    placements = payload["obstacles"]["placements"]
    assert bool(placements) is expect_obstacle
    for placement in placements:
        segment = placement["at"]["segment"]
        assert segment in segment_ranges
        along_m = placement["at"]["along_m"]
        start_m, end_m = segment_ranges[segment]
        if isinstance(along_m, dict):
            assert start_m <= along_m["min"] <= along_m["max"] <= end_m
        else:
            assert start_m <= along_m <= end_m
    assert payload["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


def _add_valid_encounter(scenario: dict) -> dict:
    """Attach one valid pedestrian encounter for validator-specific tests."""
    encounter = {
        "id": "main_conflict",
        "type": "oncoming_pass",
        "at": "conflict",
        "meet_offset_m": 0.0,
        "persona": "assertive",
        "overrides": {
            "personal_space_m": {"min": 0.6, "max": 0.9},
            "awareness_horizon_s": {"min": 1.5, "max": 2.5},
        },
    }
    scenario["pedestrians"]["encounters"] = [encounter]
    return encounter


def test_v2_scenario_generate_accepts_prompt_only() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물과 보행자 횡단 위험 시나리오를 만들어줘."},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_raw_scenario(payload)
    assert "template_path" not in payload


def test_v2_scenario_graph_runner_uses_langgraph_compile_invoke() -> None:
    runner = ScenarioGenerationGraphRunnerV2(settings=Settings(_env_file=None, v2AgentLlmEnabled=False))

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert runner.compiled_graph is not None
    assert runner.last_state["output"] is response
    assert response.scenario is not None
    assert response.scenario["schema"] == "scenario"
    forbidden_root_fields = {"ground_model", "static_obstacles", "template_id", "sample_count", "base_seed"}
    assert forbidden_root_fields.isdisjoint(response.scenario)


def test_v2_scenario_graph_endpoint_keeps_prompt_only_contract(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_raw_scenario(payload)
    assert "template_path" not in payload
    assert "scenario_path" not in payload
    assert "sample_id" not in payload
    assert "generated_count" not in payload
    assert "ue_payload" not in payload
    assert "policy" not in payload
    assert "robot_setup" not in payload


def test_v2_scenario_endpoint_uses_langgraph_runner(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 대향 보행자를 만나는 시나리오를 만들어줘"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_raw_scenario(payload)


def test_v2_scenario_graph_calls_llm_repair_and_falls_back_when_invalid() -> None:
    fake = _FakeJsonClient([{"schema": "scenario_template", "version": 1, "template_id": "legacy_llm_candidate"}])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.validation.valid is True
    assert response.scenario is not None
    assert response.scenario["schema"] == "scenario"
    assert fake.calls
    assert fake.calls[0]["response_name"] == "scenario_graph_intent"
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "scenario_graph_repair"
    assert any(
        warning.message == "LLM-assisted repair failed; deterministic fallback scenario was used."
        for warning in response.validation.warnings
    )


def test_v2_scenario_graph_accepts_valid_llm_before_deterministic_pattern_selection() -> None:
    fake = _FakeJsonClient([_llm_scenario("llm_first_graph_scenario")])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )
    runner.agent.type_selector = _ExplodingScenarioTypeSelector()

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.scenario_id == "llm_first_graph_scenario"
    assert len(fake.calls) == 1
    assert fake.calls[0]["response_name"] == "scenario_graph_intent"


def test_v2_scenario_schema_includes_validator_required_shape() -> None:
    schema = project_scenario_v1_json_schema()

    assert schema["type"] == "object"
    assert set(schema["required"]) == {
        "schema",
        "version",
        "scenario_id",
        "intent",
        "corridor",
        "obstacles",
        "pedestrians",
        "robot",
    }
    assert schema["properties"]["schema"]["const"] == "scenario"
    assert schema["properties"]["version"]["const"] == 1
    corridor = schema["properties"]["corridor"]
    assert {"axis", "walkway_width_m", "building_side", "curb_side", "segments"} <= set(corridor["required"])
    assert corridor["properties"]["axis"]["properties"]["type"]["const"] == "polyline"
    assert "placements" in schema["properties"]["obstacles"]["required"]
    encounter = schema["properties"]["pedestrians"]["properties"]["encounters"]["items"]
    assert set(encounter["properties"]["type"]["enum"]) >= {"oncoming_pass", "cross_path"}
    assert set(encounter["properties"]["persona"]["enum"]) >= {"normal", "assertive"}
    assert {"start", "goal"} <= set(schema["properties"]["robot"]["required"])


def test_v2_scenario_contract_accepts_corridor_pose_and_fixed_numbers() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["corridor"]["walkway_width_m"] = 1.6
    scenario["obstacles"]["min_clear_width_m"] = 0.95
    scenario["pedestrians"]["background"]["count"] = 1
    scenario["pedestrians"]["background"]["speed_mps"] = 1.0
    scenario["pedestrians"]["background"]["spawn_zone"] = {"segments": ["approach", "exit"]}
    scenario["robot"]["start"] = {
        "type": "corridor_pose",
        "segment": "approach",
        "along_m": 1.0,
        "offset_m": 0.0,
        "lane": "walkway",
        "heading": "forward",
    }
    scenario["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 15.0,
        "offset_m": {"min": -0.1, "max": 0.1},
        "lane": "walkway",
        "heading": "auto",
    }
    encounter = _add_valid_encounter(scenario)
    encounter["overrides"] = {
        "cooperation": {"min": 0.15, "max": 0.4},
        "personal_space_m": {"min": 0.6, "max": 0.9},
        "awareness_horizon_s": 2.0,
    }
    encounter["meet_offset_m"] = {"min": -0.1, "max": 0.1}
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "obstacle.road_cone_01",
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

    validation = agent.validator.validate(scenario)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_contract_rejects_invalid_corridor_pose_and_spawn_zone() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "missing",
        "along_m": 99.0,
        "offset_m": 0.0,
    }
    scenario["pedestrians"]["background"]["spawn_zone"] = {"segments": ["missing"]}

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.goal.segment" in fields
    assert "pedestrians.background.spawn_zone.segments[0]" in fields


def test_v2_scenario_contract_rejects_corridor_pose_along_out_of_range() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 99.0,
        "offset_m": 0.0,
    }

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.goal.along_m" in fields


def test_v2_scenario_contract_accepts_abstract_robot_anchors() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"] = {"start": {"type": "entry"}, "goal": {"type": "exit"}}

    validation = agent.validator.validate(scenario)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_contract_rejects_mixed_abstract_robot_anchors() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"] = {
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

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.start.segment" in fields
    assert "robot.start.along_m" in fields
    assert "robot.start.offset_m" in fields
    assert "robot.goal.segment" in fields
    assert "robot.goal.along_m" in fields
    assert "robot.goal.offset_m" in fields


def test_v2_scenario_contract_rejects_corridor_pose_missing_required_pose_fields() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"]["start"] = {"type": "corridor_pose", "segment": "approach", "along_m": 1.0}

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "robot.start.offset_m" in fields


def test_v2_repair_handler_converts_mixed_abstract_anchor_to_corridor_pose() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"]["start"] = {
        "type": "entry",
        "segment": "approach",
        "along_m": {"min": 0.5, "max": 1.5},
        "offset_m": 0.0,
        "lane": "center",
        "heading": "forward",
    }
    scenario["robot"]["goal"] = {
        "type": "exit",
        "lane": "center",
        "heading": "forward",
    }

    repaired = agent.repair_handler.repair(scenario)

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


def test_v2_scenario_structured_schema_allows_contract_extensions() -> None:
    schema = project_scenario_v1_json_schema()

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
    scenario = response.scenario
    assert scenario is not None
    scenario["robot"]["start"] = {
        "type": "entry",
        "segment": None,
        "along_m": None,
        "offset_m": None,
        "lane": None,
        "heading": None,
    }
    scenario["pedestrians"]["background"]["spawn_zone"] = None
    encounter = _add_valid_encounter(scenario)
    encounter["overrides"]["cooperation"] = None
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "obstacle.road_cone_01",
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

    repaired = agent.repair_handler.repair(scenario)

    assert repaired["robot"]["start"] == {"type": "entry"}
    assert "spawn_zone" not in repaired["pedestrians"]["background"]
    assert "cooperation" not in repaired["pedestrians"]["encounters"][0]["overrides"]
    assert "allow_blocking" not in repaired["obstacles"]["placements"][0]


def test_v2_scenario_contract_accepts_pattern_and_scatter_placements() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.0, "lane": "center"},
            "yaw_deg": {"min": -5.0, "max": 5.0},
            "allow_blocking": False,
        },
        {
            "kind": "pattern",
            "id": "gate_obstacles",
            "pattern": "gate",
            "prop": "obstacle.road_cone_01",
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
            "palette": {"categories": ["small_obstacle"], "classes": ["obstacle.road_cone_01"]},
            "allow_blocking": False,
        },
    ]

    validation = agent.validator.validate(scenario)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_contract_rejects_invalid_pattern_and_scatter_placements() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["obstacles"]["placements"] = [
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

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "obstacles.placements[0].prop" in fields
    assert "obstacles.placements[0].at.segment" in fields
    assert "obstacles.placements[1].density_per_10m" in fields
    assert "obstacles.placements[1].zone.segments[0]" in fields


def test_v2_scenario_contract_rejects_direct_obstacle_segment() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    scenario = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 정적 장애물")).scenario
    assert scenario is not None
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "legacy_direct_segment",
            "prop": "obstacle.road_cone_01",
            "segment": "conflict",
            "along_m": 7.0,
            "offset_m": 0.0,
            "lane": "walkway",
        }
    ]

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "obstacles.placements[0].segment" in fields
    assert "obstacles.placements[0].at" in fields


def test_v2_scenario_contract_accepts_all_unreal_override_keys_and_optional_meet_offset() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    encounter = _add_valid_encounter(scenario)
    encounter.pop("meet_offset_m", None)
    encounter["overrides"] = {
        "cooperation": {"min": 0.15, "max": 0.4},
        "evasiveness": {"min": 0.2, "max": 0.5},
        "personal_space_m": {"min": 0.6, "max": 0.9},
        "awareness_horizon_s": {"min": 1.5, "max": 2.5},
        "max_yield_wait_s": 2.0,
        "sidestep_distance_m": 0.4,
    }

    validation = agent.validator.validate(scenario)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_contract_accepts_segment_replaced_by_string_or_choices() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["corridor"]["segments"][0]["replaced_by"] = "grass"
    scenario["corridor"]["segments"][1]["replaced_by"] = {"choices": ["grass", "road"]}

    validation = agent.validator.validate(scenario)

    assert validation.valid is True
    assert validation.errors == []


def test_v2_scenario_contract_rejects_choices_for_catalog_strings() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자"))
    scenario = response.scenario
    assert scenario is not None
    scenario["corridor"]["building_side"][0]["surface"] = {"choices": ["grass", "road"]}
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "center_obstacle",
            "prop": {"choices": ["traffic_cone_01"]},
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.0, "lane": {"choices": ["center"]}},
        }
    ]
    encounter = _add_valid_encounter(scenario)
    encounter["persona"] = {"choices": ["assertive"]}
    encounter["type"] = {"choices": ["oncoming_pass"]}
    scenario["robot"]["goal"] = {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 15.0,
        "offset_m": 0.0,
        "heading": {"choices": ["forward"]},
    }

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "corridor.building_side[0].surface" in fields
    assert "obstacles.placements[0].prop" in fields
    assert "obstacles.placements[0].at.lane" in fields
    assert "pedestrians.encounters[0].persona" in fields
    assert "pedestrians.encounters[0].type" in fields
    assert "robot.goal.heading" in fields


def test_v2_scenario_graph_passes_structured_output_schema_to_llm() -> None:
    fake = _FakeJsonClient([_llm_scenario("structured_llm_scenario")])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert fake.calls[0]["response_name"] == "scenario_graph_intent"
    assert fake.calls[0]["response_schema"]["name"] == "project_scenario_v1"
    assert fake.calls[0]["response_schema"]["strict"] is True
    assert fake.calls[0]["response_schema"]["schema"]["properties"]["schema"]["const"] == "scenario"


def test_v2_scenario_graph_uses_llm_assisted_repair_when_candidate_invalid() -> None:
    fake = _FakeJsonClient([
        {"schema": "scenario", "version": 1},
        _llm_scenario("llm_repaired_graph_template"),
    ])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.scenario_id == "llm_repaired_graph_template"
    assert response.validation.valid is True
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "scenario_graph_repair"
    assert "corridor.axis.type" in fake.calls[1]["user_prompt"]
    assert "수정 대상 JSON" in fake.calls[1]["user_prompt"]
    assert fake.calls[1]["response_schema"]["name"] == "project_scenario_v1"


def test_v2_scenario_graph_uses_valid_llm_scenario_without_fallback_warning() -> None:
    fake = _FakeJsonClient([_llm_scenario("valid_llm_scenario")])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에서 대향 보행자를 만나는 시나리오"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.scenario_id == "valid_llm_scenario"
    assert response.validation.valid is True
    assert response.scenario is not None
    assert response.scenario["scenario_id"] == "valid_llm_scenario"
    assert not any("fallback scenario" in warning.message for warning in response.validation.warnings)


def test_v2_scenario_graph_curved_road_prompt_overrides_valid_straight_llm_candidate() -> None:
    fake = _FakeJsonClient([_llm_scenario("valid_straight_graph_candidate")])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="곡선 도로에서 장애물이 있는 시나리오를 생성해줘"))

    assert response.status == "success"
    assert response.generation_mode == "langgraph"
    assert response.scenario_id == "curved_road_static_obstacle"
    assert response.validation.valid is True
    assert response.scenario is not None
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)


def test_v2_scenario_llm_prompt_includes_validator_required_template_shape() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(_env_file=None, v2AgentLlmEnabled=True))

    prompt = agent._template_user_prompt("좁은 보도에서 대향 보행자")

    required_fragments = [
        '"schema": "scenario"',
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
        '"type": "corridor_pose"',
        '"segment"',
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
    _assert_raw_scenario(first_payload)
    _assert_raw_scenario(second_payload)
    assert first_payload["scenario_id"] == second_payload["scenario_id"]
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
    _assert_raw_scenario(payload)


def test_v2_scenario_agent_uses_deterministic_mode_when_llm_disabled() -> None:
    fake = _FakeJsonClient([_llm_scenario()])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "deterministic"
    assert fake.calls == []
    assert response.scenario is not None
    assert response.scenario["scenario_id"] != "llm_narrow_sidewalk"


def test_v2_deterministic_complex_prompt_returns_alpha_static_obstacle_contract() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "좁은 보도에서 로봇이 목적지로 이동하는 중, 전방에는 통로 일부를 막는 정적 장애물이 있고 "
                "반대편에서는 보행자가 걸어오며, 동시에 다른 보행자가 로봇 진행 경로를 가로지르는 위험 상황 "
                "시나리오를 생성해줘. 로봇은 보도를 벗어나지 않고 속도를 줄이거나 정지하면서 안전하게 회피해야 해."
            )
        )
    )

    assert response.scenario is not None
    _assert_alpha_static_scenario_contract(response.scenario)
    assert response.scenario["obstacles"]["placements"]
    assert response.scenario["scenario_id"] in {"narrow_sidewalk_cross_path", "static_obstacle_ahead"}


def test_v2_deterministic_curved_road_obstacle_prompt_uses_curved_preset() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="곡선 도로에서 장애물이 있는 시나리오를 생성해줘"))

    assert response.scenario is not None
    assert response.scenario["scenario_id"] == "curved_road_static_obstacle"
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)


def test_v2_deterministic_curve_road_keyword_uses_curved_preset() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브길에 장애물이 있는 시나리오를 만들어줘"))

    assert response.scenario is not None
    assert response.scenario["scenario_id"] == "curved_road_static_obstacle"
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)


def test_v2_endpoint_curved_prompt_returns_patched_scenario_without_raw_preset_obstacles(monkeypatch) -> None:
    """Ensure the external API does not return the raw six-obstacle curved preset."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post("/api/v2/scenarios/generate", json={"prompt": "커브 길을 만들어줘"})

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_curved_road_scenario_contract(payload, expect_obstacle=False)
    assert payload["obstacles"]["placements"] == []


def test_v2_deterministic_curved_prompt_without_obstacle_removes_preset_obstacles() -> None:
    """Use curved corridor skeleton only when the user did not ask for obstacles."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브 길을 만들어줘"))

    assert response.scenario is not None
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=False)
    assert response.scenario["obstacles"]["placements"] == []


def test_v2_deterministic_curved_no_obstacle_prompt_overrides_preset_obstacles() -> None:
    """Honor explicit no-obstacle intent even when the preset contains obstacles."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="장애물 없는 커브 길을 만들어줘"))

    assert response.scenario is not None
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=False)
    assert response.scenario["obstacles"]["placements"] == []


def test_v2_deterministic_curved_obstacle_count_patches_preset_count() -> None:
    """Keep exactly the obstacle count requested by the user on a curved preset."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브 길에 장애물 2개만 둬"))

    assert response.scenario is not None
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)
    assert len(response.scenario["obstacles"]["placements"]) == 2


def test_v2_deterministic_line_length_prompt_patches_corridor_and_robot_anchors() -> None:
    """Patch line preset length together with segment ranges and robot anchors."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="직선 10m 만들어줘"))

    assert response.scenario is not None
    scenario = response.scenario
    points = scenario["corridor"]["axis"]["points_m"]
    assert points[0] == [0.0, 0.0]
    assert points[-1] == [10.0, 0.0]
    segment_ranges = {segment["id"]: segment["along_range_m"] for segment in scenario["corridor"]["segments"]}
    assert max(along_range[1] for along_range in segment_ranges.values()) == 10.0
    for anchor in (scenario["robot"]["start"], scenario["robot"]["goal"]):
        start_m, end_m = segment_ranges[anchor["segment"]]
        assert start_m <= anchor["along_m"] <= end_m
    assert scenario["robot"]["goal"]["along_m"] == 9.0


def test_v2_deterministic_construction_prompt_uses_barricade_preset_skeleton() -> None:
    """Select the barricade preset for construction-area prompts."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="공사구간 만들어줘"))

    assert response.scenario is not None
    scenario = response.scenario
    assert scenario["scenario_id"] == "construction_zigzag_obstacle_corridor_6"
    assert len(scenario["obstacles"]["placements"]) >= 4
    assert scenario["corridor"]["segments"][1]["id"] == "conflict"


def test_v2_deterministic_s_curve_prompt_uses_s_curve_preset_skeleton() -> None:
    """Select the s-curve preset for multi-curve path prompts."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="S자 길 만들어줘"))

    assert response.scenario is not None
    scenario = response.scenario
    assert scenario["scenario_id"] == "long_multi_curve_open_clearance_03"
    points = scenario["corridor"]["axis"]["points_m"]
    assert len(points) >= 5
    assert any(point[1] < 0 for point in points)
    assert any(point[1] > 0 for point in points)


def test_v2_deterministic_construction_s_curve_prompt_keeps_s_curve_with_obstacle() -> None:
    """Prioritize S-curve geometry while reflecting construction as obstacle intent."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="공사구간이 있는 S자 커브 길을 만들어줘"))

    assert response.scenario is not None
    scenario = response.scenario
    points = scenario["corridor"]["axis"]["points_m"]
    assert len(points) >= 5
    assert any(point[1] < 0 for point in points)
    assert any(point[1] > 0 for point in points)
    assert len(scenario["obstacles"]["placements"]) > 0
    _assert_raw_scenario(scenario)


def test_v2_scenario_preset_aliases_resolve_to_canonical_ids() -> None:
    """Resolve legacy preset ids before the loader receives a filesystem id."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    assert agent.scenario_preset_registry.resolve("curved-road") == "curved"
    assert agent.scenario_preset_registry.resolve("demo") == "line"


def test_v2_scenario_missing_preset_uses_code_generation_fallback() -> None:
    """Treat a missing preset as fallback input, not as an API-failing exception."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브 길을 만들어줘"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    _assert_raw_scenario(response.scenario)


def test_v2_scenario_invalid_preset_uses_code_generation_fallback() -> None:
    """Fallback when loaded preset content cannot pass TemplateValidator."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_InvalidScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브 길에 장애물 1개 둬"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    assert response.scenario["scenario_id"] == "static_obstacle_ahead"


def test_v2_scenario_preset_loader_optional_load_handles_json_parse_failure(tmp_path) -> None:
    """Return an optional-load miss for parse errors without editing static templates."""
    template_dir = tmp_path / "static" / "templates" / "scenario"
    template_dir.mkdir(parents=True)
    (template_dir / "curved.json").write_text("{bad json", encoding="utf-8")
    loader = ScenarioPresetLoader(start_path=tmp_path)

    result = loader.try_load_scenario_preset("curved")

    assert result.scenario is None
    assert result.error is not None


def test_v2_deterministic_straight_obstacle_prompt_uses_line_preset_skeleton() -> None:
    """Use the line preset skeleton for straight obstacle prompts."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="직선 보도에서 장애물이 있는 시나리오를 생성해줘"))

    assert response.scenario is not None
    points = response.scenario["corridor"]["axis"]["points_m"]
    assert points == [[0.0, 0.0], [4.0, 0.0]]
    assert response.scenario["scenario_id"] == "demo_sidewalk_obstacle"
    assert len(response.scenario["obstacles"]["placements"]) == 1


def test_v2_deterministic_pedestrian_prompt_keeps_alpha_pedestrians_empty() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="좁은 보도에서 취약한 보행자 한 명이 옆에서 지나가는 상황")
    )

    assert response.scenario is not None
    assert response.scenario["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


def test_v2_deterministic_gate_prompt_does_not_mark_obstacles_blocking_by_default() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "좁은 보도 중간에 임시 안내판 두 개가 게이트처럼 놓여 있고, "
                "보행자 한 명이 그 사이를 가로질러 지나가는 상황"
            )
        )
    )

    assert response.scenario is not None
    placements = response.scenario["obstacles"]["placements"]
    assert len(placements) == 2
    assert all(placement.get("allow_blocking") is not True for placement in placements)
    assert {placement["prop"] for placement in placements} == {"obstacle.road_cone_01"}
    assert {placement["id"] for placement in placements} == {"gate_panel_left", "gate_panel_right"}


def test_v2_deterministic_explicit_blocking_prompt_allows_blocking_obstacle() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="좁은 보도에서 일부러 길을 막는 장애물 때문에 통행 불가 상황을 만들어줘")
    )

    assert response.scenario is not None
    assert any(placement.get("allow_blocking") is True for placement in response.scenario["obstacles"]["placements"])


def test_v2_deterministic_corridor_pose_only_prompt_minimizes_risk_elements() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "로봇이 보도 입구가 아니라 approach 구간의 1m 지점에서 출발하고, "
                "exit 구간의 16m 지점을 목표로 이동하는 시나리오를 만들어줘. "
                "로봇 시작점과 목표점은 entry/exit 추상 anchor가 아니라 corridor_pose로 표현해줘."
            )
        )
    )

    assert response.scenario is not None
    scenario = response.scenario
    assert scenario["robot"]["start"] == {
        "type": "corridor_pose",
        "segment": "approach",
        "along_m": 1.0,
        "offset_m": 0.0,
        "lane": "center",
        "heading": "forward",
    }
    assert scenario["robot"]["goal"] == {
        "type": "corridor_pose",
        "segment": "exit",
        "along_m": 16.0,
        "offset_m": 0.0,
        "lane": "center",
        "heading": "forward",
    }
    assert scenario["obstacles"]["placements"] == []
    assert scenario["pedestrians"]["background"]["count"] == 0
    assert scenario["pedestrians"]["encounters"] == []
    assert scenario["schema"] == "scenario"
    assert "scenario_id" in scenario
    assert "template_id" not in scenario


def test_v2_llm_gate_prompt_trims_duplicated_two_object_gate_pair() -> None:
    llm_scenario = _llm_scenario("llm_gate_with_duplicate_pair")
    llm_scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "gate_panel_left",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": -0.3, "lane": "center"},
            "yaw_deg": 0,
            "allow_blocking": False,
        },
        {
            "kind": "fixed",
            "id": "gate_panel_right",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.3, "lane": "center"},
            "yaw_deg": 0,
            "allow_blocking": False,
        },
        {
            "kind": "fixed",
            "id": "gate_panel_left_2",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 9.0, "offset_m": -0.3, "lane": "center"},
            "yaw_deg": 0,
            "allow_blocking": False,
        },
        {
            "kind": "fixed",
            "id": "gate_panel_right_2",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 9.0, "offset_m": 0.3, "lane": "center"},
            "yaw_deg": 0,
            "allow_blocking": False,
        },
    ]
    fake = _FakeJsonClient([llm_scenario])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "좁은 보도 중간에 임시 안내판 두 개가 게이트처럼 놓여 있고, "
                "보행자 한 명이 그 사이를 가로질러 지나가는 상황"
            )
        )
    )

    assert response.scenario is not None
    placements = response.scenario["obstacles"]["placements"]
    assert len(placements) == 2
    assert {placement["id"] for placement in placements} == {"gate_panel_left", "gate_panel_right"}
    assert all(placement.get("allow_blocking") is not True for placement in placements)
    _assert_raw_scenario(response.scenario)


def test_v2_llm_corridor_pose_only_prompt_removes_unrequested_encounters() -> None:
    llm_scenario = _llm_scenario("llm_corridor_pose_with_extra_encounter")
    llm_scenario["obstacles"]["placements"] = []
    llm_scenario["pedestrians"]["background"]["count"] = {"min": 0, "max": 0}
    llm_scenario["pedestrians"]["encounters"] = [
        {
            "id": "main_conflict",
            "type": "oncoming_pass",
            "at": "conflict",
            "meet_offset_m": 0.0,
            "persona": "assertive",
            "overrides": {"cooperation": {"min": 0.15, "max": 0.4}},
        }
    ]
    llm_scenario["robot"] = {
        "start": {
            "type": "corridor_pose",
            "segment": "approach",
            "along_m": 1.0,
            "offset_m": 0.0,
            "lane": "center",
            "heading": "forward",
        },
        "goal": {
            "type": "corridor_pose",
            "segment": "exit",
            "along_m": 16.0,
            "offset_m": 0.0,
            "lane": "center",
            "heading": "forward",
        },
    }
    fake = _FakeJsonClient([llm_scenario])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "로봇이 보도 입구가 아니라 approach 구간의 1m 지점에서 출발하고, "
                "exit 구간의 16m 지점을 목표로 이동하는 시나리오를 만들어줘. "
                "로봇 시작점과 목표점은 entry/exit 추상 anchor가 아니라 corridor_pose로 표현해줘."
            )
        )
    )

    assert response.scenario is not None
    scenario = response.scenario
    assert scenario["obstacles"]["placements"] == []
    assert scenario["pedestrians"]["background"]["count"] == 0
    assert scenario["pedestrians"]["encounters"] == []
    assert scenario["robot"]["start"]["type"] == "corridor_pose"
    assert scenario["robot"]["goal"]["type"] == "corridor_pose"
    _assert_raw_scenario(scenario)


def test_v2_llm_complex_prompt_is_postprocessed_to_alpha_static_contract() -> None:
    llm_scenario = _llm_scenario("llm_complex_pedestrian_scene")
    llm_scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "llm_cone",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.25, "lane": "walkway"},
            "yaw_deg": 0,
            "allow_blocking": False,
        }
    ]
    llm_scenario["pedestrians"] = {
        "background": {"count": {"min": 1, "max": 3}, "speed_mps": {"min": 0.8, "max": 1.4}},
        "encounters": [
            {
                "id": "llm_oncoming",
                "type": "oncoming_pass",
                "at": "conflict",
                "meet_offset_m": 0.0,
                "persona": "assertive",
                "overrides": {"personal_space_m": 0.7},
            }
        ],
    }
    fake = _FakeJsonClient([llm_scenario])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "좁은 보도에서 로봇이 목적지로 이동하는 중, 전방에는 통로 일부를 막는 정적 장애물이 있고 "
                "반대편에서는 보행자가 걸어오며, 동시에 다른 보행자가 로봇 진행 경로를 가로지르는 위험 상황"
            )
        )
    )

    assert response.scenario is not None
    _assert_alpha_static_scenario_contract(response.scenario)
    assert response.generation_mode == "llm"


def test_v2_llm_curved_road_prompt_overrides_valid_straight_candidate() -> None:
    llm_scenario = _llm_scenario("llm_valid_straight_candidate")
    llm_scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "llm_cone",
            "prop": "obstacle.road_cone_01",
            "at": {"segment": "conflict", "along_m": 7.0, "offset_m": 0.25, "lane": "walkway"},
            "yaw_deg": 0,
            "allow_blocking": False,
        }
    ]
    fake = _FakeJsonClient([llm_scenario])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    response = agent.generate(ScenarioGenerateV2Request(prompt="curved road with an obstacle"))

    assert response.scenario is not None
    assert response.generation_mode == "llm"
    assert response.scenario_id == "curved_road_static_obstacle"
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)


def _llm_curved_road_response_with_obstacle_along(along_m: object | None, *, include_along: bool = True):
    """Generate a curved-road response from an LLM candidate with a specific obstacle along value."""
    llm_scenario = _llm_scenario("llm_curved_obstacle_along_policy")
    at = {"segment": "conflict", "offset_m": 0.25, "lane": "walkway"}
    if include_along:
        at["along_m"] = along_m
    llm_scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "llm_cone",
            "prop": "obstacle.road_cone_01",
            "at": at,
            "yaw_deg": 0,
        }
    ]
    fake = _FakeJsonClient([llm_scenario])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    return agent.generate(ScenarioGenerateV2Request(prompt="curved road with an obstacle"))


def test_v2_llm_curved_road_prompt_preserves_valid_obstacle_range() -> None:
    response = _llm_curved_road_response_with_obstacle_along({"min": 7.0, "max": 9.0})

    assert response.scenario is not None
    placement = response.scenario["obstacles"]["placements"][0]
    assert placement["at"]["segment"] == "road_curve"
    assert placement["at"]["along_m"] == {"min": 7.0, "max": 9.0}


def test_v2_llm_curved_road_prompt_clamps_obstacle_range_inside_curve() -> None:
    response = _llm_curved_road_response_with_obstacle_along({"min": 3.0, "max": 5.0})

    assert response.scenario is not None
    placement = response.scenario["obstacles"]["placements"][0]
    assert placement["at"]["segment"] == "road_curve"
    assert placement["at"]["along_m"] == {"min": 4.0, "max": 5.0}


def test_v2_llm_curved_road_prompt_defaults_edge_collapsed_obstacle_range() -> None:
    response = _llm_curved_road_response_with_obstacle_along({"min": 12.0, "max": 14.0})

    assert response.scenario is not None
    placement = response.scenario["obstacles"]["placements"][0]
    assert placement["at"]["segment"] == "road_curve"
    assert placement["at"]["along_m"] == {"min": 6.5, "max": 8.5}


def test_v2_llm_curved_road_prompt_defaults_missing_obstacle_range() -> None:
    response = _llm_curved_road_response_with_obstacle_along(None, include_along=False)

    assert response.scenario is not None
    placement = response.scenario["obstacles"]["placements"][0]
    assert placement["at"]["segment"] == "road_curve"
    assert placement["at"]["along_m"] == {"min": 6.5, "max": 8.5}


def test_v2_scenario_agent_uses_llm_scenario_when_enabled() -> None:
    fake = _FakeJsonClient([_llm_scenario()])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm"
    assert response.scenario_id == "llm_narrow_sidewalk"
    assert response.scenario is not None
    assert response.scenario["intent"]
    assert fake.calls[0]["response_name"] == "scenario"


def test_v2_scenario_agent_accepts_valid_llm_before_deterministic_pattern_selection() -> None:
    fake = _FakeJsonClient([_llm_scenario("llm_first_agent_scenario")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True),
        llm_client=fake,
    )
    agent.type_selector = _ExplodingScenarioTypeSelector()

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm"
    assert response.scenario_id == "llm_first_agent_scenario"
    assert len(fake.calls) == 1
    assert fake.calls[0]["response_name"] == "scenario"


def test_v2_scenario_agent_repairs_invalid_llm_scenario() -> None:
    invalid_template = {"schema": "scenario", "version": 1}
    repaired_template = _llm_scenario("llm_repaired_template")
    fake = _FakeJsonClient([invalid_template, repaired_template])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm_repaired"
    assert response.scenario_id == "llm_repaired_template"
    assert len(fake.calls) == 2
    assert fake.calls[1]["response_name"] == "project_scenario_repair"


def test_v2_repair_handler_migrates_legacy_template_root_to_project_scenario() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    legacy = _llm_scenario("legacy_template_id")
    legacy["schema"] = "scenario_template"
    legacy["template_id"] = legacy.pop("scenario_id")

    repaired = agent.repair_handler.repair(legacy)

    assert repaired["schema"] == "scenario"
    assert repaired["scenario_id"] == "legacy_template_id"
    assert "template_id" not in repaired
    assert agent.validator.validate(repaired).valid is True


def test_v2_repair_handler_normalizes_legacy_obstacle_props() -> None:
    """Normalize known legacy prop ids before TemplateValidator sees repaired JSON."""
    expected_aliases = {
        "obstacle.crate_01": "obstacle.box_01",
        "crate_01": "obstacle.box_01",
        "traffic_cone_01": "obstacle.road_cone_01",
        "obstacle.traffic_cone_01": "obstacle.road_cone_01",
    }
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    for legacy_prop, expected_prop in expected_aliases.items():
        scenario = _llm_scenario_with_obstacle(f"legacy_{legacy_prop.replace('.', '_')}")
        scenario["obstacles"]["placements"][0]["prop"] = legacy_prop

        repaired = agent.repair_handler.repair(scenario)

        props = {placement["prop"] for placement in repaired["obstacles"]["placements"]}
        assert props == {expected_prop}
        assert agent.validator.validate(repaired).valid is True


def test_v2_repair_handler_preserves_unknown_non_catalog_prop_for_validation() -> None:
    """Leave unknown prop ids untouched so TemplateValidator can reject them."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    scenario = _llm_scenario_with_obstacle("unknown_legacy_prop")
    scenario["obstacles"]["placements"][0]["prop"] = "unknown.legacy_prop"

    repaired = agent.repair_handler.repair(scenario)
    validation = agent.validator.validate(repaired)

    assert repaired["obstacles"]["placements"][0]["prop"] == "unknown.legacy_prop"
    assert validation.valid is False
    assert "obstacles.placements[0].prop" in {issue.field for issue in validation.errors}


def test_v2_llm_path_normalizes_legacy_obstacle_prop_before_response() -> None:
    """Keep LLM-generated external scenario JSON free of legacy prop ids."""
    llm_template = _llm_scenario_with_obstacle("llm_legacy_prop")
    llm_template["obstacles"]["placements"][0]["prop"] = "obstacle.crate_01"
    fake = _FakeJsonClient([llm_template])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.generation_mode == "llm"
    assert response.scenario is not None
    props = {placement["prop"] for placement in response.scenario["obstacles"]["placements"]}
    assert props == {"obstacle.box_01"}
    assert response.validation.valid is True


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
    assert response.scenario is not None
    assert any(
        warning.message == "LLM output validation failed; deterministic fallback scenario was used."
        for warning in response.validation.warnings
    )


def test_v2_scenario_agent_curved_road_prompt_keeps_curved_preset_after_llm_fallback() -> None:
    fake = _FakeJsonClient([ValueError("bad json"), ValueError("repair bad json")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브길에 장애물이 있는 시나리오를 만들어줘"))

    assert response.generation_mode == "fallback"
    assert response.status == "success"
    assert response.validation.valid is True
    assert response.scenario is not None
    assert response.scenario_id == "curved_road_static_obstacle"
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)


def test_v2_scenario_generate_uses_current_template_contract() -> None:
    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 로봇 전방 장애물이 있고 보행자가 옆에서 지나가는 위험 상황"},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    segment_ids = {segment["id"] for segment in payload["corridor"]["segments"]}

    _assert_raw_scenario(payload)
    assert {"schema", "version", "scenario_id", "intent", "corridor", "robot"} <= set(payload)
    assert all(placement["at"]["segment"] in segment_ids for placement in payload["obstacles"]["placements"])
    assert all(encounter["at"] in segment_ids for encounter in payload["pedestrians"]["encounters"])
    assert "encounters" in payload["pedestrians"]
    assert "placements" in payload["obstacles"]
    forbidden = {"sample_count", "base_seed", "experiment_id", "run_id", "template_id"}
    assert forbidden.isdisjoint(payload)


def test_v2_validator_rejects_catalog_violations() -> None:
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    scenario = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 보행자가 가로지르는 상황")).scenario
    assert scenario is not None
    scenario["corridor"]["building_side"][0]["surface"] = "marble"
    encounter = _add_valid_encounter(scenario)
    encounter["type"] = "teleport"
    encounter["persona"] = "rude"

    validation = agent.validator.validate(scenario)

    assert validation.valid is False
    fields = {issue.field for issue in validation.errors}
    assert "corridor.building_side[0].surface" in fields
    assert "pedestrians.encounters[0].type" in fields
    assert "pedestrians.encounters[0].persona" in fields


def test_v2_validator_uses_static_obstacle_catalog_prop_ids() -> None:
    """Reject legacy scenario props that are not present in the static obstacle catalog."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    scenario = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에서 장애물이 있는 상황")).scenario
    assert scenario is not None
    placement = scenario["obstacles"]["placements"][0]

    placement["prop"] = "obstacle.box_01"
    assert agent.validator.validate(scenario).valid is True

    placement["prop"] = "obstacle.crate_01"
    validation = agent.validator.validate(scenario)
    assert validation.valid is False
    assert "obstacles.placements[0].prop" in {issue.field for issue in validation.errors}


def test_v2_construction_s_curve_response_uses_catalog_prop() -> None:
    """Ensure patched preset output never exposes legacy non-catalog crate props."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="공사구간이 있는 S자 커브 길을 만들어줘"))

    assert response.scenario is not None
    props = {placement["prop"] for placement in response.scenario["obstacles"]["placements"]}
    assert props
    assert "obstacle.crate_01" not in props
    assert props <= get_allowed_static_obstacle_prop_ids()


def test_v2_preset_patcher_normalizes_legacy_preset_props_to_catalog_ids() -> None:
    """Normalize known preset-era prop ids before the final validator gate."""
    expected_aliases = {
        "obstacle.crate_01": "obstacle.box_01",
        "crate_01": "obstacle.box_01",
        "traffic_cone_01": "obstacle.road_cone_01",
        "obstacle.traffic_cone_01": "obstacle.road_cone_01",
    }
    base = ScenarioPresetLoader().load_scenario_preset("barricade")
    base["obstacles"]["placements"] = [deepcopy(base["obstacles"]["placements"][0])]

    for legacy_prop, expected_prop in expected_aliases.items():
        scenario = deepcopy(base)
        scenario["obstacles"]["placements"][0]["prop"] = legacy_prop
        agent = ScenarioGenerationV2Agent(
            settings=Settings(v2AgentLlmEnabled=False),
            scenario_preset_loader=_ObjectScenarioPresetLoader(scenario),
        )
        intent = agent.intent_parser.parse("공사구간 만들어줘")

        patched = agent._try_build_preset_scenario(intent, source_scenario={"obstacles": {"placements": []}})

        assert patched is not None
        props = {placement["prop"] for placement in patched["obstacles"]["placements"]}
        assert props == {expected_prop}
        assert props <= get_allowed_static_obstacle_prop_ids()


def test_v2_unknown_non_catalog_preset_prop_uses_fallback_without_legacy_prop() -> None:
    """Do not normalize unknown prop ids; fallback should return catalog-safe output."""
    scenario = ScenarioPresetLoader().load_scenario_preset("barricade")
    scenario["obstacles"]["placements"][0]["prop"] = "unknown.legacy_prop"
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_ObjectScenarioPresetLoader(scenario),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="공사구간 만들어줘"))

    assert response.scenario is not None
    props = {placement["prop"] for placement in response.scenario["obstacles"]["placements"]}
    assert "unknown.legacy_prop" not in props
    assert props <= get_allowed_static_obstacle_prop_ids()


def test_v2_testclient_obstacle_responses_do_not_expose_legacy_props() -> None:
    """Keep external scenario JSON free of legacy preset prop ids."""
    legacy_props = {"obstacle.crate_01", "crate_01", "traffic_cone_01", "obstacle.traffic_cone_01"}
    client = TestClient(app)
    validator = TemplateValidator()

    for prompt in (
        "공사구간이 있는 S자 커브 길을 만들어줘",
        "곡선 도로에서 장애물이 있는 시나리오를 생성해줘",
        "커브 길에 장애물 2개만 둬",
        "장애물 3개 있는 직선 도로 만들어줘",
    ):
        response = client.post("/api/v2/scenarios/generate", json={"prompt": prompt})

        assert response.status_code == 200
        payload = response.json()
        props = {placement["prop"] for placement in payload["obstacles"]["placements"]}
        response_text = response.text
        _assert_raw_scenario(payload)
        assert validator.validate(payload).valid is True
        assert legacy_props.isdisjoint(props)
        assert props <= get_allowed_static_obstacle_prop_ids()
        assert all(legacy_prop not in response_text for legacy_prop in legacy_props)


def test_v2_agent_json_parser_accepts_markdown_json_block() -> None:
    parsed = parse_json_response('```json\n{"schema": "scenario", "version": 1}\n```')

    assert parsed == {"schema": "scenario", "version": 1}


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
    assert response_component_name == "ProjectScenarioV1Response"
    assert response_schema["required"] == [
        "schema",
        "version",
        "scenario_id",
        "intent",
        "corridor",
        "obstacles",
        "pedestrians",
        "robot",
    ]
    assert set(response_schema["properties"]) == set(response_schema["required"])
    assert "template_id" not in response_schema["properties"]


def test_v1_scenario_generate_still_exists_in_openapi() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    assert "/api/v1/scenarios/generate" in schema["paths"]
