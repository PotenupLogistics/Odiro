from __future__ import annotations

from copy import deepcopy
from types import SimpleNamespace

from fastapi.testclient import TestClient

from app.agents.common.json_response_parser import parse_json_response
from app.agents.common.llm_json_client import AgentLlmJsonClient
from app.agents.scenario_generation_v2 import ScenarioGenerationV2Agent
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2
from app.agents.scenario_generation_v2.repair_diagnostics import RepairDiagnosticCode
from app.agents.scenario_generation_v2.repair_handler import ROBOT_ANCHOR_EXCLUSION_RADIUS_M
from app.agents.scenario_generation_v2.scenario_preset_loader import ScenarioPresetLoader
from app.agents.scenario_generation_v2.scenario_template_schema import project_scenario_v1_json_schema
from app.agents.scenario_generation_v2.template_validator import ALLOWED_LANES, TemplateValidator
from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids
from app.core.settings import Settings
from app.main import app
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request


COMPLEX_G_SHAPE_CONSTRUCTION_PROMPT = (
    "ㄱ자 도로에 중간 공사구간이 있고, 커브 직전에 콘 3개를 지그재그로 배치해줘. "
    "전체 길이는 20m 정도로 하고 보행자는 없게 해줘."
)

COMPLEX_LONG_S_CURVE_CORNER_PROMPT = (
    "전체 길이 35m 정도의 긴 보도 시나리오를 만들어줘. 시작 부분은 직선이고, 중간에는 S자 커브가 있고, "
    "마지막에는 L자 코너로 꺾이는 도로였으면 해. 중간 공사구간에는 콘 5개를 지그재그로 배치하고, "
    "코너 직전에는 road cone 3개를 추가로 좌우 번갈아 배치해줘. 보행자는 없게 하고, "
    "출발 지점과 도착 지점 주변에는 장애물이 없게 해줘."
)

COMPLEX_40M_CONFLICTS_CORNER_PROMPT = (
    "40m 정도의 복잡한 보도 주행 시나리오를 만들어줘. 처음 10m는 직선, 그 다음은 S자 형태로 휘어지고, "
    "후반부는 ㄱ자 도로처럼 직각으로 꺾이게 해줘. 좁아지는 conflict 구간을 2개 만들고, "
    "첫 번째 구간에는 콘 4개, 두 번째 구간에는 obstacle.road_cone_01 4개를 지그재그로 배치해줘. "
    "장애물끼리는 겹치지 않게 하고 보행자는 없이 만들어줘."
)


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
    wrapper_fields = {
        "status",
        "summary",
        "scenario",
        "validation",
        "assumptions",
        "generation_mode",
        "warnings",
        "diagnostics",
        "repair_events",
    }
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


def _axis_path_length(points: list[list[float]]) -> float:
    """Return the polyline length used by scenario corridor assertions."""
    total = 0.0
    for previous, current in zip(points, points[1:], strict=False):
        total += (
            (float(current[0]) - float(previous[0])) ** 2
            + (float(current[1]) - float(previous[1])) ** 2
        ) ** 0.5
    return total


def _has_s_curve_shape(points: list[list[float]]) -> bool:
    """Return whether a polyline has both left and right lateral movement."""
    y_values = [float(point[1]) for point in points]
    return len(points) >= 5 and min(y_values) < -0.2 and max(y_values) > 0.2


def _has_late_corner_turn(points: list[list[float]]) -> bool:
    """Return whether the latter half contains a meaningful direction change."""
    if len(points) < 4:
        return False
    start_index = max(1, len(points) // 2)
    for index in range(start_index, len(points) - 1):
        previous = points[index - 1]
        current = points[index]
        following = points[index + 1]
        first = (float(current[0]) - float(previous[0]), float(current[1]) - float(previous[1]))
        second = (float(following[0]) - float(current[0]), float(following[1]) - float(current[1]))
        first_length = (first[0] ** 2 + first[1] ** 2) ** 0.5
        second_length = (second[0] ** 2 + second[1] ** 2) ** 0.5
        if first_length == 0 or second_length == 0:
            continue
        cross = abs(first[0] * second[1] - first[1] * second[0])
        sin_angle = cross / (first_length * second_length)
        if sin_angle >= 0.7:
            return True
    return False


def _range_center(value: object) -> float:
    """Return a comparable center for scalar or ranged scenario coordinates."""
    if isinstance(value, dict):
        return (float(value["min"]) + float(value["max"])) / 2.0
    return float(value)


def _range_bounds(value: object) -> tuple[float, float]:
    """Return comparable bounds for scalar or ranged scenario coordinates."""
    if isinstance(value, dict):
        return float(value["min"]), float(value["max"])
    numeric = float(value)
    return numeric, numeric


def _ranges_overlap(left: tuple[float, float], right: tuple[float, float]) -> bool:
    """Return whether two closed numeric intervals overlap."""
    return left[0] <= right[1] and right[0] <= left[1]


def _min_numeric_value(value: object) -> float | None:
    """Return the minimum numeric value from scalar or min/max scenario fields."""
    if isinstance(value, dict):
        return float(value["min"])
    if isinstance(value, int | float):
        return float(value)
    return None


def _max_numeric_value(value: object) -> float | None:
    """Return the maximum numeric value from scalar or min/max scenario fields."""
    if isinstance(value, dict):
        return float(value["max"])
    if isinstance(value, int | float):
        return float(value)
    return None


def _assert_scenario_quality_guardrails(
    payload: dict,
    *,
    safety_radius_m: float = ROBOT_ANCHOR_EXCLUSION_RADIUS_M,
) -> None:
    """Assert generated scenarios satisfy UE-facing obstacle quality guardrails."""
    _assert_raw_scenario(payload)
    assert TemplateValidator().validate(payload).valid is True
    allowed_props = get_allowed_static_obstacle_prop_ids()
    corridor = payload["corridor"]
    segment_ranges = {segment["id"]: segment["along_range_m"] for segment in corridor["segments"]}
    segment_ids = set(segment_ranges)
    for anchor in (payload["robot"]["start"], payload["robot"]["goal"]):
        assert anchor["segment"] in segment_ids
        start_m, end_m = segment_ranges[anchor["segment"]]
        assert float(start_m) <= float(anchor["along_m"]) <= float(end_m)
        assert anchor.get("lane") in ALLOWED_LANES

    placements = payload["obstacles"]["placements"]
    walkway_width_m = _min_numeric_value(corridor["walkway_width_m"])
    min_clear_width_m = _max_numeric_value(payload["obstacles"].get("min_clear_width_m"))
    for placement in placements:
        assert placement["prop"] in allowed_props
        at = placement["at"]
        assert at["segment"] in segment_ids
        assert at.get("lane") in ALLOWED_LANES
        segment_start, segment_end = segment_ranges[at["segment"]]
        along_bounds = _range_bounds(at["along_m"])
        assert float(segment_start) <= along_bounds[0] <= along_bounds[1] <= float(segment_end)
        for anchor in (payload["robot"]["start"], payload["robot"]["goal"]):
            if at["segment"] != anchor["segment"]:
                continue
            anchor_along = float(anchor["along_m"])
            forbidden = (anchor_along - safety_radius_m, anchor_along + safety_radius_m)
            assert not _ranges_overlap(along_bounds, forbidden)
        if walkway_width_m is not None and min_clear_width_m is not None:
            offset_min, offset_max = _range_bounds(at["offset_m"])
            half_width = walkway_width_m / 2.0
            assert max(offset_min + half_width, half_width - offset_max) + 1e-6 >= min_clear_width_m

    for left_index, left in enumerate(placements):
        for right in placements[left_index + 1 :]:
            if left["at"]["segment"] != right["at"]["segment"]:
                continue
            along_overlap = _ranges_overlap(_range_bounds(left["at"]["along_m"]), _range_bounds(right["at"]["along_m"]))
            offset_overlap = _ranges_overlap(_range_bounds(left["at"]["offset_m"]), _range_bounds(right["at"]["offset_m"]))
            assert not (along_overlap and offset_overlap)


def _assert_no_catalog_metadata_leaked(payload: dict) -> None:
    """Assert scenario output does not copy catalog documentation fields."""
    rendered = str(payload)
    assert "bbox_m" not in rendered
    assert "footprint_m" not in rendered
    assert "Prop Bounding Boxes" not in rendered


def _assert_complex_g_shape_construction_scenario(payload: dict) -> None:
    """Assert the complex Korean prompt keeps shape, count, and placement intent."""
    _assert_raw_scenario(payload)
    corridor = payload["corridor"]
    points = corridor["axis"]["points_m"]
    assert len(points) >= 3
    assert points != [[0.0, 0.0], [20.0, 0.0]]
    assert 19.0 <= _axis_path_length(points) <= 21.0

    segment_by_id = {segment["id"]: segment for segment in corridor["segments"]}
    assert "pre_corner_construction" in segment_by_id
    assert "turn_and_exit" in segment_by_id
    pre_corner = segment_by_id["pre_corner_construction"]
    assert pre_corner["type"] == "narrowing"
    pre_corner_range = pre_corner["along_range_m"]
    assert 6.5 <= float(pre_corner_range[0]) <= 7.5
    assert 9.5 <= float(pre_corner_range[1]) <= 10.5

    placements = payload["obstacles"]["placements"]
    assert len(placements) == 3
    assert {placement["prop"] for placement in placements} == {"obstacle.road_cone_01"}
    assert len({placement["id"] for placement in placements}) == 3

    positions: set[tuple[float, float]] = set()
    offsets: list[float] = []
    for placement in placements:
        at = placement["at"]
        assert at["segment"] == "pre_corner_construction"
        along_center = _range_center(at["along_m"])
        offset_center = _range_center(at["offset_m"])
        assert float(pre_corner_range[0]) <= along_center <= float(pre_corner_range[1])
        positions.add((round(along_center, 2), round(offset_center, 2)))
        offsets.append(offset_center)
    assert len(positions) == len(placements)
    assert len({round(offset, 2) for offset in offsets}) >= 2
    assert any(offset < 0 for offset in offsets)
    assert any(offset > 0 for offset in offsets)

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


def test_v2_scenario_api_falls_back_when_openai_fails_without_ollama_attempt(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "true")
    monkeypatch.setenv("LLM_PROVIDER_CHAIN", "openai,ollama")
    providers: list[str] = []

    def fail_generate_json(self, **_kwargs):
        providers.append(self.provider.value)
        raise ValueError("simulated provider failure")

    monkeypatch.setattr(AgentLlmJsonClient, "generate_json", fail_generate_json)

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물이 있고 보행자가 가로지르는 상황"},
    )

    assert response.status_code == 200, response.text
    _assert_raw_scenario(response.json())
    assert providers == ["openai"]


def test_v2_scenario_api_falls_back_when_selected_ollama_provider_fails(monkeypatch) -> None:
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "true")
    monkeypatch.setenv("LLM_PROVIDER_CHAIN", "ollama")
    providers: list[str] = []

    def fail_generate_json(self, **_kwargs):
        providers.append(self.provider.value)
        raise ValueError("simulated provider failure")

    monkeypatch.setattr(AgentLlmJsonClient, "generate_json", fail_generate_json)

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": "좁은 보도에서 장애물이 있고 보행자가 가로지르는 상황"},
    )

    assert response.status_code == 200, response.text
    _assert_raw_scenario(response.json())
    assert providers == ["ollama"]


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


def test_v2_scenario_graph_runner_records_internal_repair_events() -> None:
    scenario = _llm_scenario_with_obstacle("llm_legacy_repair_event")
    scenario["obstacles"]["placements"][0]["prop"] = "traffic_cone_01"
    fake = _FakeJsonClient([scenario])
    runner = ScenarioGenerationGraphRunnerV2(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = runner.run(ScenarioGenerateV2Request(prompt="좁은 보도에 콘 하나를 배치해줘"))

    assert response.status == "success"
    repair_events = runner.last_state["repair_events"]
    assert any(
        event["code"] == RepairDiagnosticCode.PROP_NORMALIZED.value
        and event["stage"] == "graph.llm_candidate.initial"
        and event["before"] == "traffic_cone_01"
        and event["after"] == "obstacle.road_cone_01"
        for event in repair_events
    )
    assert "repair_events" not in response.model_dump()


def test_v2_scenario_agent_records_internal_repair_events() -> None:
    scenario = _llm_scenario_with_obstacle("agent_legacy_repair_event")
    scenario["obstacles"]["placements"][0]["prop"] = "traffic_cone_01"
    fake = _FakeJsonClient([scenario])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도에 콘 하나를 배치해줘"))

    assert response.status == "success"
    assert any(
        event["code"] == RepairDiagnosticCode.PROP_NORMALIZED.value
        and event["stage"] == "agent.llm.initial"
        for event in agent.last_repair_events
    )
    assert "repair_events" not in response.model_dump()


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


def test_v2_deterministic_generic_obstacle_count_patches_code_generation_count() -> None:
    """Keep explicitly requested obstacle counts outside preset-backed prompts."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="장애물 2개 설치해줘"))

    assert response.scenario is not None
    assert response.validation.valid is True
    assert len(response.scenario["obstacles"]["placements"]) == 2


def test_v2_deterministic_catalog_prop_count_prompt_patches_code_generation_count() -> None:
    """Parse catalog prop id count prompts and keep catalog-safe placements."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="obstacle.road_cone_01 2개 설치해줘"))

    assert response.scenario is not None
    assert response.validation.valid is True
    placements = response.scenario["obstacles"]["placements"]
    assert len(placements) == 2
    assert {placement["prop"] for placement in placements} <= {"obstacle.road_cone_01", "obstacle.road_cone_02"}


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


def test_v2_deterministic_line_length_no_obstacle_phrase_overrides_default_obstacle() -> None:
    """Honor particle-bearing Korean no-obstacle phrases in compound length prompts."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="길을 길게 만들어줘. 10m 로 만들어주고 장애물은 없는 직선 보도 시나리오로 만들어줘.")
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    assert scenario["obstacles"]["placements"] == []
    assert scenario["corridor"]["axis"]["points_m"][-1] == [10.0, 0.0]
    segment_ranges = {segment["id"]: segment["along_range_m"] for segment in scenario["corridor"]["segments"]}
    assert max(along_range[1] for along_range in segment_ranges.values()) == 10.0
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


def test_v2_endpoint_complex_g_shape_construction_prompt_patches_preset(monkeypatch) -> None:
    """Preserve L-shape intent while patching the construction preset."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": COMPLEX_G_SHAPE_CONSTRUCTION_PROMPT},
    )

    assert response.status_code == 200, response.text
    _assert_complex_g_shape_construction_scenario(response.json())


def test_v2_endpoint_beta_quality_guardrails_for_natural_language_prompts(monkeypatch) -> None:
    """Keep generated obstacle scenarios drivable for beta UE setup tests."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")
    cases = [
        ("출발 지점 바로 앞에 콘 2개를 배치해줘.", 2),
        ("도착 지점 근처에 장애물 2개를 배치해줘.", 2),
        ("좁은 보도에 콘 4개를 지그재그로 배치해줘.", 4),
        (COMPLEX_G_SHAPE_CONSTRUCTION_PROMPT, 3),
        ("S자 커브 길에 중간 공사구간이 있고 콘 3개만 배치해줘. 보행자는 없게 해줘.", 3),
    ]

    client = TestClient(app)
    for prompt, expected_count in cases:
        response = client.post("/api/v2/scenarios/generate", json={"prompt": prompt})

        assert response.status_code == 200, response.text
        payload = response.json()
        _assert_scenario_quality_guardrails(payload)
        assert len(payload["obstacles"]["placements"]) == expected_count
        assert all(
            placement["prop"] == "obstacle.road_cone_01"
            for placement in payload["obstacles"]["placements"]
        )


def test_v2_endpoint_complex_long_s_curve_corner_prompt_preserves_obstacle_intent(monkeypatch) -> None:
    """Treat start/goal no-obstacle wording as clearance while preserving requested cones."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": COMPLEX_LONG_S_CURVE_CORNER_PROMPT},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_scenario_quality_guardrails(payload)

    points = payload["corridor"]["axis"]["points_m"]
    assert 30.0 <= _axis_path_length(points) <= 40.0
    assert _has_s_curve_shape(points)
    assert _has_late_corner_turn(points)
    assert sum(1 for segment in payload["corridor"]["segments"] if segment["type"] == "narrowing") >= 2

    placements = payload["obstacles"]["placements"]
    assert len(placements) == 8
    assert {placement["prop"] for placement in placements} == {"obstacle.road_cone_01"}
    assert len({placement["at"]["segment"] for placement in placements}) >= 2
    assert payload["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


def test_v2_endpoint_complex_conflict_count_prompt_splits_cones_across_conflicts(monkeypatch) -> None:
    """Do not confuse conflict segment count with requested obstacle count."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")

    response = TestClient(app).post(
        "/api/v2/scenarios/generate",
        json={"prompt": COMPLEX_40M_CONFLICTS_CORNER_PROMPT},
    )

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_scenario_quality_guardrails(payload)

    points = payload["corridor"]["axis"]["points_m"]
    assert 35.0 <= _axis_path_length(points) <= 45.0
    assert _has_s_curve_shape(points)
    assert _has_late_corner_turn(points)

    narrowing_segments = [segment["id"] for segment in payload["corridor"]["segments"] if segment["type"] == "narrowing"]
    assert len(narrowing_segments) >= 2

    placements = payload["obstacles"]["placements"]
    assert len(placements) == 8
    assert {placement["prop"] for placement in placements} == {"obstacle.road_cone_01"}
    placement_segments = {placement["at"]["segment"] for placement in placements}
    assert len(placement_segments & set(narrowing_segments)) >= 2
    assert payload["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


def test_v2_deterministic_g_shape_prompt_without_obstacles_builds_corner_corridor() -> None:
    """Build an obstacle-free L-shape corridor when only road shape is requested."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="ㄱ자 도로 생성해줘"))

    assert response.scenario is not None
    scenario = response.scenario
    _assert_raw_scenario(scenario)
    points = scenario["corridor"]["axis"]["points_m"]
    assert len(points) >= 3
    assert points != [[0.0, 0.0], [20.0, 0.0]]
    assert scenario["obstacles"]["placements"] == []


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


def test_v2_missing_preset_curved_prompt_uses_intent_based_curved_fallback() -> None:
    """Build a curved fallback from intent when the curved preset is unavailable."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="커브 길을 만들어줘"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    points = response.scenario["corridor"]["axis"]["points_m"]
    assert len(points) >= 3
    assert len({point[1] for point in points}) > 1
    assert response.scenario["obstacles"]["placements"] == []


def test_v2_missing_preset_length_prompt_uses_intent_based_10m_fallback() -> None:
    """Build a 10m fallback corridor without relying on the line preset file."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="10m 로 만들어줘"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    assert scenario["corridor"]["axis"]["points_m"][-1] == [10.0, 0.0]
    segment_ranges = {segment["id"]: segment["along_range_m"] for segment in scenario["corridor"]["segments"]}
    assert max(along_range[1] for along_range in segment_ranges.values()) == 10.0
    assert 0.0 <= scenario["robot"]["goal"]["along_m"] <= 10.0
    assert scenario["robot"]["goal"]["along_m"] == 9.0


def test_v2_missing_preset_obstacle_count_uses_intent_based_count_fallback() -> None:
    """Build exactly the requested obstacle count in fallback output."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="장애물 2개 설치해줘"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    assert len(response.scenario["obstacles"]["placements"]) == 2


def test_v2_missing_preset_complex_g_shape_construction_prompt_uses_intent_fallback() -> None:
    """Build the same complex L-shape construction scene when presets are unavailable."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt=COMPLEX_G_SHAPE_CONSTRUCTION_PROMPT))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    _assert_complex_g_shape_construction_scenario(response.scenario)


def test_v2_missing_preset_requested_catalog_prop_uses_intent_prop_fallback() -> None:
    """Use an explicitly requested catalog prop in fallback obstacle placements."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="obstacle.box_01 2개 설치해줘"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    placements = response.scenario["obstacles"]["placements"]
    assert len(placements) == 2
    assert {placement["prop"] for placement in placements} == {"obstacle.box_01"}


def test_v2_deterministic_korean_fixture_alias_prompt_uses_catalog_props() -> None:
    """Use Korean fixture aliases as static obstacle intent in deterministic generation."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="보도 가장자리에 휴지통과 우편함이 있고 중앙은 통과 가능한 시나리오를 만들어줘")
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    placements = scenario["obstacles"]["placements"]
    props = {placement["prop"] for placement in placements}
    assert len(placements) >= 2
    assert props & {"obstacle.trash_bin", "obstacle.bin"}
    assert "obstacle.mailbox" in props
    assert all(placement.get("allow_blocking") is not True for placement in placements)
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_mixed_korean_alias_prompt_preserves_prop_sequence() -> None:
    """Map mixed Korean obstacle aliases to their catalog prop ids."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="소화전, 벤치, 박스가 섞여 있는 복잡한 보도 장애물 시나리오를 만들어줘")
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    props = {placement["prop"] for placement in scenario["obstacles"]["placements"]}
    assert {"obstacle.fire_hydrant", "obstacle.street_bank", "obstacle.box_01"} <= props
    assert props <= get_allowed_static_obstacle_prop_ids()
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_alias_count_prompt_uses_requested_manhole_count() -> None:
    """Parse an alias-adjacent count such as manhole four as obstacle count."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="맨홀 4개가 낮은 장애물처럼 깔려 있는 시나리오를 만들어줘"))

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    placements = scenario["obstacles"]["placements"]
    assert len(placements) == 4
    assert {placement["prop"] for placement in placements} <= {
        "obstacle.manhole_01",
        "obstacle.manhole_02",
        "obstacle.manhole_03",
        "obstacle.manhole_04",
    }
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_grouped_alias_counts_preserve_each_prop_count() -> None:
    """Apply each alias-adjacent count to its own mentioned prop."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="우편함 2개와 맨홀 3개가 있는 시나리오를 만들어줘"))

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    props = [placement["prop"] for placement in scenario["obstacles"]["placements"]]
    assert len(props) == 5
    assert props.count("obstacle.mailbox") == 2
    assert sum(1 for prop in props if prop.startswith("obstacle.manhole_")) == 3
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_korean_word_grouped_alias_counts_preserve_each_prop_count() -> None:
    """Parse Korean number words next to static obstacle aliases."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="라바콘 세 개와 박스 두 개가 있는 공사 구간 시나리오를 만들어줘"))

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    props = [placement["prop"] for placement in scenario["obstacles"]["placements"]]
    assert len(props) == 5
    assert props.count("obstacle.road_cone_01") == 3
    assert props.count("obstacle.box_01") == 2
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_mixed_digit_alias_counts_preserve_each_prop_count() -> None:
    """Do not let generic cone count parsing hide other alias-count groups."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt="좁은 보도, 곡선 구간, 도로 가장자리, 바리케이드 1개, 라바콘 2개, 박스 1개가 함께 있는 복잡한 시나리오를 만들어줘"
        )
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    props = [placement["prop"] for placement in scenario["obstacles"]["placements"]]
    assert len(props) == 4
    assert props.count("obstacle.road_barrier_01") == 1
    assert props.count("obstacle.road_cone_01") == 2
    assert props.count("obstacle.box_01") == 1
    assert all(placement.get("allow_blocking") is not True for placement in scenario["obstacles"]["placements"])
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_between_obstacles_prompt_keeps_passable_gap() -> None:
    """Treat wall/road side wording as context while preserving box and trash-bin obstacles."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "건물 벽 쪽은 막혀 있고 차도 쪽은 위험 영역이며, "
                "로봇이 박스와 쓰레기통 사이를 지나가야 하는 시나리오를 만들어줘"
            )
        )
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    placements = scenario["obstacles"]["placements"]
    props = {placement["prop"] for placement in placements}
    assert "obstacle.box_01" in props
    assert props & {"obstacle.trash_bin", "obstacle.bin"}
    assert all(placement.get("allow_blocking") is not True for placement in placements)
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_unknown_signage_prompt_stays_catalog_safe() -> None:
    """Avoid inventing a prop id when a Korean object name is outside the catalog."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="안내판이 있는 시나리오를 만들어줘. catalog에 없는 prop id를 만들면 안 돼")
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    _assert_scenario_quality_guardrails(scenario)
    props = {placement["prop"] for placement in scenario["obstacles"]["placements"]}
    assert props
    assert props <= get_allowed_static_obstacle_prop_ids()
    _assert_no_catalog_metadata_leaked(scenario)


def test_v2_deterministic_pedestrian_alias_work_keeps_alpha_policy() -> None:
    """Keep alpha pedestrian output disabled while obstacle alias parsing evolves."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="보행자가 있는 좁은 보도 시나리오를 만들어줘"))

    assert response.scenario is not None
    assert response.validation.valid is True
    assert response.scenario["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


def test_v2_missing_preset_unknown_prop_fallback_keeps_catalog_safe_output() -> None:
    """Do not expose unknown prompt prop ids in validator-approved fallback output."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="obstacle.unknown_prop_99 장애물 2개 설치해줘"))

    assert response.status == "success"
    assert response.scenario is not None
    assert response.validation.valid is True
    props = {placement["prop"] for placement in response.scenario["obstacles"]["placements"]}
    assert "obstacle.unknown_prop_99" not in props
    assert props <= get_allowed_static_obstacle_prop_ids()


def test_v2_endpoint_missing_preset_fallback_returns_raw_scenario_json(monkeypatch) -> None:
    """Keep the external endpoint body unwrapped when preset loading falls back."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")
    monkeypatch.setattr(
        "app.agents.scenario_generation_v2.agent.ScenarioPresetLoader",
        lambda: _MissingScenarioPresetLoader(),
    )

    response = TestClient(app).post("/api/v2/scenarios/generate", json={"prompt": "커브 길을 만들어줘"})

    assert response.status_code == 200, response.text
    payload = response.json()
    _assert_raw_scenario(payload)
    assert TemplateValidator().validate(payload).valid is True


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
    assert response.scenario["scenario_id"] == "curved_road_static_obstacle"
    _assert_curved_road_scenario_contract(response.scenario, expect_obstacle=True)


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


def test_v2_deterministic_straight_obstacle_count_prompt_keeps_requested_count() -> None:
    """Keep straight corridor geometry while honoring an explicit obstacle count."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(ScenarioGenerateV2Request(prompt="장애물 3개 있는 직선 도로 만들어줘"))

    assert response.scenario is not None
    scenario = response.scenario
    assert scenario["corridor"]["axis"]["points_m"] == [[0.0, 0.0], [4.0, 0.0]]
    assert len(scenario["obstacles"]["placements"]) == 3


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
    response = _llm_curved_road_response_with_obstacle_along({"min": 5.0, "max": 6.0})

    assert response.scenario is not None
    placement = response.scenario["obstacles"]["placements"][0]
    assert placement["at"]["segment"] == "road_curve"
    assert placement["at"]["along_m"] == {"min": 5.0, "max": 6.0}


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
    _assert_scenario_quality_guardrails(response.scenario)


def test_v2_llm_curved_road_prompt_defaults_missing_obstacle_range() -> None:
    response = _llm_curved_road_response_with_obstacle_along(None, include_along=False)

    assert response.scenario is not None
    placement = response.scenario["obstacles"]["placements"][0]
    assert placement["at"]["segment"] == "road_curve"
    _assert_scenario_quality_guardrails(response.scenario)


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


def test_v2_llm_path_removes_catalog_metadata_and_keeps_alpha_pedestrians() -> None:
    """Strip catalog-only fields from valid LLM output before returning scenario JSON."""
    llm_template = _llm_scenario_with_obstacle("llm_catalog_metadata_leak")
    llm_template["obstacles"]["placements"][0]["prop"] = "obstacle.trash_bin"
    llm_template["obstacles"]["placements"][0]["bbox_m"] = [0.9, 0.9, 1.8]
    llm_template["obstacles"]["placements"][0]["footprint_m"] = [0.9, 0.9]
    llm_template["bbox_m"] = {"obstacle.trash_bin": [0.9, 0.9, 1.8]}
    llm_template["pedestrians"] = {
        "background": {"count": 2, "speed_mps": 1.0},
        "encounters": [
            {
                "id": "llm_crossing",
                "type": "cross_path",
                "at": "conflict",
                "persona": "normal",
            }
        ],
    }
    fake = _FakeJsonClient([llm_template])
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=True), llm_client=fake)

    response = agent.generate(ScenarioGenerateV2Request(prompt="보도 가장자리에 쓰레기통이 있는 시나리오"))

    assert response.generation_mode == "llm"
    assert response.scenario is not None
    assert response.scenario["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}
    _assert_no_catalog_metadata_leaked(response.scenario)
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


def test_v2_llm_failure_fallback_preserves_korean_multi_prop_intent() -> None:
    """Use deterministic alias parsing when LLM and repair outputs fall back."""
    fake = _FakeJsonClient([ValueError("bad json"), ValueError("repair bad json")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="보도 가장자리에 휴지통과 우편함이 있고 중앙은 통과 가능한 시나리오를 만들어줘")
    )

    assert response.generation_mode == "fallback"
    assert response.scenario is not None
    props = {placement["prop"] for placement in response.scenario["obstacles"]["placements"]}
    assert props <= get_allowed_static_obstacle_prop_ids()
    assert props & {"obstacle.trash_bin", "obstacle.bin"}
    assert "obstacle.mailbox" in props
    assert all(placement.get("allow_blocking") is not True for placement in response.scenario["obstacles"]["placements"])
    _assert_no_catalog_metadata_leaked(response.scenario)


def test_v2_llm_invalid_prop_output_stays_catalog_safe() -> None:
    """Do not expose non-catalog LLM prop ids in final output."""
    invalid_template = _llm_scenario_with_obstacle("llm_invalid_signboard")
    invalid_template["obstacles"]["placements"][0]["prop"] = "obstacle.signboard_01"
    fake = _FakeJsonClient([invalid_template, ValueError("repair bad json")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(
        ScenarioGenerateV2Request(prompt="안내판이 있는 시나리오를 만들어줘. catalog에 없는 prop id를 만들면 안 돼")
    )

    assert response.scenario is not None
    props = {placement["prop"] for placement in response.scenario["obstacles"]["placements"]}
    assert "obstacle.signboard_01" not in props
    assert props <= get_allowed_static_obstacle_prop_ids()
    _assert_no_catalog_metadata_leaked(response.scenario)


def test_v2_llm_malformed_fallback_preserves_manhole_alias_count() -> None:
    """Keep alias-adjacent counts when LLM and repair both fail."""
    fake = _FakeJsonClient([ValueError("bad json"), ValueError("repair bad json")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="맨홀 4개가 낮은 장애물처럼 깔려 있는 시나리오를 만들어줘"))

    assert response.generation_mode == "fallback"
    assert response.scenario is not None
    placements = response.scenario["obstacles"]["placements"]
    assert len(placements) == 4
    assert {placement["prop"] for placement in placements} <= {
        "obstacle.manhole_01",
        "obstacle.manhole_02",
        "obstacle.manhole_03",
        "obstacle.manhole_04",
    }
    _assert_no_catalog_metadata_leaked(response.scenario)


def test_v2_llm_missing_fields_fallback_preserves_box_trash_gap_intent() -> None:
    """Keep box and trash-bin props when a missing-field LLM candidate falls back."""
    fake = _FakeJsonClient([{"schema": "scenario", "version": 1}, ValueError("repair bad json")])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt=(
                "건물 벽 쪽은 막혀 있고 차도 쪽은 위험 영역이며, "
                "로봇이 박스와 쓰레기통 사이를 지나가야 하는 시나리오를 만들어줘"
            )
        )
    )

    assert response.generation_mode == "fallback"
    assert response.scenario is not None
    placements = response.scenario["obstacles"]["placements"]
    props = {placement["prop"] for placement in placements}
    assert "obstacle.box_01" in props
    assert props & {"obstacle.trash_bin", "obstacle.bin"}
    assert all(placement.get("allow_blocking") is not True for placement in placements)
    _assert_no_catalog_metadata_leaked(response.scenario)


def test_v2_llm_repair_preserves_explicit_multi_prop_alias_counts() -> None:
    """Keep explicit alias-count constraints after LLM-assisted repair succeeds."""
    repaired_template = _llm_scenario_with_obstacle("llm_repaired_underfit_multi_prop")
    repaired_template["obstacles"]["placements"][0]["prop"] = "obstacle.road_cone_01"
    fake = _FakeJsonClient([{"schema": "scenario", "version": 1}, repaired_template])
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=True, v2AgentLlmRepairEnabled=True),
        llm_client=fake,
    )

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt="우편함 2개와 맨홀 3개가 있는 시나리오를 만들어줘"
        )
    )

    assert response.generation_mode == "llm_repaired"
    assert response.scenario is not None
    placements = response.scenario["obstacles"]["placements"]
    props = [placement["prop"] for placement in placements]
    assert len(props) == 5
    assert props.count("obstacle.mailbox") == 2
    assert sum(1 for prop in props if prop.startswith("obstacle.manhole_")) == 3
    assert all(placement.get("allow_blocking") is not True for placement in placements)
    _assert_no_catalog_metadata_leaked(response.scenario)


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


def test_v2_preset_patcher_requested_catalog_prop_overrides_preset_props() -> None:
    """Apply user-requested catalog props inside preset patching, even without fallback source placements."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))
    intent = agent.intent_parser.parse("커브 길에 obstacle.box_01 2개 설치해줘")

    patched = agent._try_build_preset_scenario(intent, source_scenario={"obstacles": {"placements": []}})

    assert patched is not None
    assert agent.validator.validate(patched).valid is True
    placements = patched["obstacles"]["placements"]
    assert len(placements) == 2
    assert {placement["prop"] for placement in placements} == {"obstacle.box_01"}


def test_v2_preset_success_complex_prompt_applies_intent_count_prop_and_pedestrian_policy() -> None:
    """Keep preset-success output aligned with explicit count, prop, shape, and no-pedestrian intent."""
    agent = ScenarioGenerationV2Agent(settings=Settings(v2AgentLlmEnabled=False))

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt="공사구간이 있는 S자 커브 길을 만들어줘. obstacle.road_cone_01 2개만 설치하고 보행자는 없게 해줘."
        )
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    points = scenario["corridor"]["axis"]["points_m"]
    assert len(points) >= 5
    assert any(point[1] < 0 for point in points)
    assert any(point[1] > 0 for point in points)
    assert len(scenario["obstacles"]["placements"]) == 2
    assert {placement["prop"] for placement in scenario["obstacles"]["placements"]} == {"obstacle.road_cone_01"}
    assert scenario["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


def test_v2_fallback_complex_prompt_applies_same_core_intent_conditions() -> None:
    """Keep missing-preset fallback aligned with the same explicit count, prop, shape, and pedestrian intent."""
    agent = ScenarioGenerationV2Agent(
        settings=Settings(v2AgentLlmEnabled=False),
        scenario_preset_loader=_MissingScenarioPresetLoader(),
    )

    response = agent.generate(
        ScenarioGenerateV2Request(
            prompt="공사구간이 있는 S자 커브 길을 만들어줘. obstacle.road_cone_01 2개만 설치하고 보행자는 없게 해줘."
        )
    )

    assert response.scenario is not None
    assert response.validation.valid is True
    scenario = response.scenario
    points = scenario["corridor"]["axis"]["points_m"]
    assert len(points) >= 5
    assert any(point[1] < 0 for point in points)
    assert any(point[1] > 0 for point in points)
    assert len(scenario["obstacles"]["placements"]) == 2
    assert {placement["prop"] for placement in scenario["obstacles"]["placements"]} == {"obstacle.road_cone_01"}
    assert scenario["pedestrians"] == {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}


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
