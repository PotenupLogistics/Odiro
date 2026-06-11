from __future__ import annotations

from datetime import datetime, timezone
import json
import os
from pathlib import Path
from types import SimpleNamespace

from fastapi.testclient import TestClient

import app.api.routes as routes
from app.core.settings import Settings
from app.main import app
from app.models.run_queue import EpisodeRunQueue, EpisodeRunQueueItem
from app.services import scenario_generation_service
from app.services.environment_generation_constraints_builder import build_environment_sampling_context
from app.services.setup_pair_queue_generator import generate_setup_pair_queue


EXPLICIT_FIXED_PROMPT = (
    "보도 폭이 120cm인 좁은 직선 보도에서 배달 로봇이 출발점에서 10m 앞 목적지까지 이동해야 한다. "
    "보도 중앙에는 박스형 정적 장애물 2개가 3m, 6m 지점에 놓여 있고, "
    "보행자 3명이 로봇 진행 방향 반대편에서 걸어온다. "
    "로봇이 감속, 정지, 우회 판단을 해야 하는 시나리오를 생성해줘."
)
SIMPLE_BLOCKING_PROMPT = (
    "좁은 보도에서 정적 장애물이 배달 로봇의 경로 일부를 막고 있는 상황을 생성해줘. "
    "보행자는 없고, 로봇은 안전하게 감속하거나 우회해야 한다."
)


class _Dumpable:
    def __init__(self, payload: dict) -> None:
        self.payload = payload

    def model_dump(self, **kwargs):
        return self.payload


def _queue(run_count: int = 1) -> EpisodeRunQueue:
    return EpisodeRunQueue(
        runs=[
            EpisodeRunQueueItem(
                pair_id=f"obstacle_ahead_{index:03d}",
                episode_setup=f"Json/Input/EpisodeSetup_obstacle_ahead_{index:03d}.json",
                delivery_bot_setup=f"Json/Input/DeliveryBotSetup_obstacle_ahead_{index:03d}.json",
            )
            for index in range(run_count)
        ]
    )


def test_scenario_generation_route_accepts_prompt_only_and_returns_run_queue(monkeypatch) -> None:
    def stub_generate(request, **kwargs):
        assert request.episode_count is None
        return SimpleNamespace(queue=SimpleNamespace(run_queue=_queue(run_count=Settings().scenarioEpisodeDefaultCount)))

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate)
    monkeypatch.setattr(routes, "write_scenario_artifacts_to_local_dir", lambda artifacts: None)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "장애물이 경로를 막는 상황"})

    assert response.status_code == 200
    payload = response.json()
    assert set(payload) == {"schema", "version", "runs"}
    assert len(payload["runs"]) == Settings().scenarioEpisodeDefaultCount
    assert set(payload["runs"][0]) == {"pair_id", "episode_setup", "delivery_bot_setup"}


def test_scenario_generation_route_accepts_optional_episode_count(monkeypatch) -> None:
    observed_counts = []

    def stub_generate(request, **kwargs):
        observed_counts.append(request.episode_count)
        return SimpleNamespace(queue=SimpleNamespace(run_queue=_queue(run_count=request.episode_count)))

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate)
    monkeypatch.setattr(routes, "write_scenario_artifacts_to_local_dir", lambda artifacts: None)
    client = TestClient(app)

    one_response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})
    three_response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 3})

    assert one_response.status_code == 200
    assert three_response.status_code == 200
    assert len(one_response.json()["runs"]) == 1
    assert len(three_response.json()["runs"]) == 3
    assert observed_counts == [1, 3]


def test_scenario_generation_route_reports_generation_failure(monkeypatch) -> None:
    def fail_generation(request, **kwargs):
        raise RuntimeError("world config validation failed with api_key and token hidden")

    monkeypatch.setattr(routes, "generate_scenario_artifacts", fail_generation)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"] == {
        "code": "SCENARIO_GENERATION_FAILED",
        "message": "world config validation failed with [redacted] and [redacted] hidden",
        "stage": "scenario_generation",
    }


def test_generated_episode_setup_preserves_request_shape_and_robot_profile() -> None:
    queue = generate_setup_pair_queue(
        {
            "schemaVersion": "1.0",
            "worldId": "world-1",
            "scenarioId": "obstacle_ahead",
            "seed": 1001,
            "map": {"type": "Sidewalk", "lengthCm": 800, "sidewalkWidthCm": 120},
            "robot": {
                "botId": "robot_01",
                "spawn": {"x": 0, "y": 0, "z": 0},
                "goal": {"x": 800, "y": 0, "z": 0},
            },
            "obstacles": [
                {
                    "objectId": "obstacle_01",
                    "type": "Obstacle",
                    "position": {"x": 400, "y": 0, "z": 0},
                    "blockingRatio": 0.6,
                }
            ],
            "pedestrians": [],
            "runtime": {"maxDurationSec": 60},
        },
        episode_count=1,
    )

    episode = queue.items[0].episode_setup

    assert episode.robot_profile.width_m == 0.44
    assert episode.robot_profile.depth_m == 1.0
    assert episode.robot_profile.height_m == 0.64
    assert episode.robot_profile.min_passable_width_m == 0.84
    assert episode.actors.static_obstacles[0].properties["passability"] == "blocked_path"


def test_scenario_generation_route_accepts_episode_count_at_max(monkeypatch) -> None:
    max_count = Settings().scenarioEpisodeMaxCount

    def stub_generate(request, **kwargs):
        return SimpleNamespace(queue=SimpleNamespace(run_queue=_queue(run_count=request.episode_count)))

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate)
    monkeypatch.setattr(routes, "write_scenario_artifacts_to_local_dir", lambda artifacts: None)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": max_count})

    assert response.status_code == 200
    assert len(response.json()["runs"]) == max_count


def test_scenario_generation_route_rejects_extra_fields_and_empty_prompt() -> None:
    client = TestClient(app)

    extra_response = client.post("/api/v1/scenarios/generate", json={"prompt": "x", "episodeCount": 3})
    empty_response = client.post("/api/v1/scenarios/generate", json={"prompt": "   "})

    assert extra_response.status_code == 422
    assert empty_response.status_code == 422


def test_scenario_generation_route_rejects_invalid_episode_count_values() -> None:
    client = TestClient(app)

    max_count = Settings().scenarioEpisodeMaxCount
    for value in [0, -1, 1.5, "3", max_count + 1]:
        response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": value})
        assert response.status_code == 422


def test_scenario_generation_openapi_marks_episode_count_optional() -> None:
    schema = TestClient(app).get("/openapi.json").json()
    request_ref = schema["paths"]["/api/v1/scenarios/generate"]["post"]["requestBody"]["content"]["application/json"]["schema"]["$ref"]
    component_name = request_ref.rsplit("/", 1)[-1]
    request_schema = schema["components"]["schemas"][component_name]

    assert request_schema["required"] == ["prompt"]
    assert "episode_count" in request_schema["properties"]
    assert "minimum" in request_schema["properties"]["episode_count"]["anyOf"][0]
    assert request_schema["properties"]["episode_count"]["anyOf"][0]["maximum"] == Settings().scenarioEpisodeMaxCount


def test_scenario_generation_request_maps_explicit_prompt_to_fixed_constraints() -> None:
    request = scenario_generation_service._generation_request(
        routes.ScenarioGenerateRequest(prompt=EXPLICIT_FIXED_PROMPT, episode_count=2)
    )
    assert request.constraints.environmentSampling is not None
    sampler_fixed = request.constraints.environmentSampling["fixedParameters"]
    fixed = request.constraints.semanticFixedConstraints

    assert sampler_fixed == {"sidewalkWidthCm": 120}
    assert fixed is not None
    assert fixed["sidewalkWidthCm"] == 120
    assert fixed["goalDistanceM"] == 10.0
    assert fixed["obstacleCount"] == 2
    assert fixed["obstacleType"] == "box"
    assert fixed["obstaclePositionsFromStartM"] == [3.0, 6.0]
    assert fixed["obstacleLateralPosition"] == "center"
    assert fixed["pedestrianCount"] == 3
    assert fixed["pedestrianDirection"] == "opposite_direction"
    assert fixed["expectedRobotBehavior"] == ["SlowDown", "Stop", "ReplanPath"]


def test_scenario_generation_request_maps_simple_prompt_without_strict_numeric_constraints() -> None:
    request = scenario_generation_service._generation_request(
        routes.ScenarioGenerateRequest(prompt=SIMPLE_BLOCKING_PROMPT, episode_count=1)
    )
    assert request.constraints.environmentSampling is not None
    sampler_fixed = request.constraints.environmentSampling["fixedParameters"]
    fixed = request.constraints.semanticFixedConstraints

    assert sampler_fixed == {"sidewalkWidthCm": 150}
    assert fixed is not None
    assert fixed["sidewalkWidthCm"] == 150
    assert fixed["obstacleType"] == "static_obstacle"
    assert fixed["pedestrianCount"] == 0
    assert fixed["expectedRobotBehavior"] == ["SlowDown", "ReplanPath"]
    assert "goalDistanceM" not in fixed
    assert "obstacleCount" not in fixed
    assert "obstaclePositionsFromStartM" not in fixed


def test_scenario_generation_request_keeps_sampler_fixed_parameters_catalog_compatible() -> None:
    cases = [
        (
            (
                "좁은 보도에서 정적 장애물이 배달 로봇의 경로 일부를 막고 있는 상황을 생성해줘. "
                "보행자는 없고, 로봇은 안전하게 감속하거나 우회해야 한다."
            ),
            {"sidewalkWidthCm": 150},
        ),
        (
            (
                "보도 폭이 140cm인 직선 보도에서 로봇이 출발점에서 10m 앞 목적지까지 이동한다. "
                "보도 중앙에는 박스형 장애물 1개가 출발점 기준 5m 지점에 있고, "
                "보행자 1명이 로봇의 진행 방향을 가로질러 이동한다."
            ),
            {},
        ),
        (
            (
                "보도 폭이 180cm인 직선 보도에서 로봇이 12m 앞 목적지까지 이동한다. "
                "정적 장애물은 박스형 장애물 1개와 킥보드 형태 장애물 1개이며, "
                "각각 출발점 기준 4m, 8m 지점에 배치한다. "
                "보행자 2명이 장애물 근처에서 반대 방향으로 걸어온다."
            ),
            {},
        ),
    ]

    for prompt, expected_sampler_fixed in cases:
        request = scenario_generation_service._generation_request(
            routes.ScenarioGenerateRequest(prompt=prompt, episode_count=3)
        )

        assert request.constraints.environmentSampling is not None
        assert request.constraints.environmentSampling["fixedParameters"] == expected_sampler_fixed
        assert request.constraints.semanticFixedConstraints
        assert build_environment_sampling_context(request) is not None


def test_openapi_exposes_no_other_api_v1_routes() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    api_v1_paths = sorted(path for path in schema["paths"] if path.startswith("/api/v1/"))

    assert api_v1_paths == [
        "/api/v1/analysis/run",
        "/api/v1/scenarios/generate",
    ]


def test_removed_scenario_generation_endpoints_are_not_in_openapi() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    assert "/api/v1/scenarios/generate-artifacts" not in schema["paths"]
    assert "/api/v1/scenarios/generate-drive" not in schema["paths"]


def _artifact_result(tmp_path, run_count: int):
    items = []
    runs = []
    for index in range(run_count):
        episode_relative_path = f"Json/Input/EpisodeSetup_test_{index:03d}.json"
        bot_relative_path = f"Json/Input/DeliveryBotSetup_test_{index:03d}.json"
        items.append(
            SimpleNamespace(
                episode_setup=_Dumpable({"episode": index}),
                delivery_bot_setup=_Dumpable({"bot": index}),
                episode_setup_path=episode_relative_path,
                delivery_bot_setup_path=bot_relative_path,
            )
        )
        runs.append(
            EpisodeRunQueueItem(
                pair_id=f"artifact_pair_{index:03d}",
                episode_setup=episode_relative_path,
                delivery_bot_setup=bot_relative_path,
            )
        )
    run_queue = EpisodeRunQueue(runs=runs)
    return SimpleNamespace(
        queue=SimpleNamespace(
            request_id="REQ-TEST",
            export_base_dir=str(tmp_path / "default_exports"),
            run_queue=run_queue,
            run_queue_path="Json/Input/EpisodeRunQueue_test.json",
            items=items,
        ),
        export=SimpleNamespace(export_root=tmp_path, run_queue_path=None),
    )


def _real_artifact_result(tmp_path, run_count: int = 1):
    queue = generate_setup_pair_queue(
        {
            "schemaVersion": "1.0",
            "worldId": "world-1",
            "scenarioId": "obstacle_ahead",
            "seed": 1001,
            "map": {"type": "Sidewalk", "lengthCm": 800, "sidewalkWidthCm": 120},
            "robot": {
                "botId": "robot_01",
                "spawn": {"x": 0, "y": 0, "z": 0},
                "goal": {"x": 800, "y": 0, "z": 0},
            },
            "obstacles": [
                {
                    "objectId": "obstacle_01",
                    "type": "Obstacle",
                    "position": {"x": 400, "y": 0, "z": 0},
                    "blockingRatio": 0.6,
                }
            ],
            "pedestrians": [],
            "runtime": {"maxDurationSec": 60},
        },
        episode_count=run_count,
        request_id="REQ-ROBOT-PROFILE",
    )
    return SimpleNamespace(queue=queue, export=SimpleNamespace(export_root=tmp_path, run_queue_path=None))


def _artifact_result_with_null_artifacts(tmp_path):
    episode_relative_path = "Json/Input/EpisodeSetup_test_000.json"
    bot_relative_path = "Json/Input/DeliveryBotSetup_test_000.json"
    run_queue = EpisodeRunQueue(
        runs=[
            EpisodeRunQueueItem(
                pair_id="artifact_pair_000",
                episode_setup=episode_relative_path,
                delivery_bot_setup=bot_relative_path,
            )
        ]
    )
    return SimpleNamespace(
        queue=SimpleNamespace(
            request_id="REQ-TEST",
            export_base_dir=str(tmp_path / "default_exports"),
            run_queue=run_queue,
            run_queue_path="Json/Input/EpisodeRunQueue_test.json",
            items=[
                SimpleNamespace(
                    episode_setup=_Dumpable({"episode": 0, "properties": {"empty": None}}),
                    delivery_bot_setup=_Dumpable(
                        {
                            "schema": "delivery_bot_setup",
                            "version": 1,
                            "robot": {
                                "drive": {"max_speed_kmh": 10.0, "speed_limit_brake": None},
                                "path_follow": {"target_speed_kmh": 10.0, "draw_debug": None},
                                "lidar": {"scan_range_m": 5.0, "ignore_tags": None},
                            },
                        }
                    ),
                    episode_setup_path=episode_relative_path,
                    delivery_bot_setup_path=bot_relative_path,
                )
            ],
        ),
        export=SimpleNamespace(export_root=tmp_path, run_queue_path=None),
    )


def _generated_input_dir(output_root: Path) -> Path:
    return output_root / "Json" / "Input"


def _artifact_names() -> list[str]:
    return [
        "DeliveryBotSetup_test_000.json",
        "EpisodeRunQueue_test.json",
        "EpisodeSetup_test_000.json",
    ]


def _write_existing_artifacts(input_dir: Path, *, modified_time: float) -> None:
    input_dir.mkdir(parents=True, exist_ok=True)
    for name in _artifact_names():
        path = input_dir / name
        path.write_text("{}", encoding="utf-8")
        os.utime(path, (modified_time, modified_time))


def _backup_stamp(timestamp: float) -> str:
    return datetime.fromtimestamp(timestamp, timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def test_scenario_generate_writes_artifacts_to_default_output_dir_when_env_unset(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", "")
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    export_roots = list((tmp_path / "default_exports").glob("*_REQ-TEST"))
    assert len(export_roots) == 1
    input_dir = _generated_input_dir(export_roots[0])
    assert (input_dir / "EpisodeRunQueue_test.json").exists()
    assert (input_dir / "EpisodeSetup_test_000.json").exists()
    assert (input_dir / "DeliveryBotSetup_test_000.json").exists()


def test_scenario_generate_moves_existing_artifacts_to_backup_before_writing(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "configured_output"
    input_dir = _generated_input_dir(output_root)
    modified_time = datetime(2026, 6, 7, 2, 28, 49, tzinfo=timezone.utc).timestamp()
    _write_existing_artifacts(input_dir, modified_time=modified_time)
    (input_dir / "notes.json").write_text("{}", encoding="utf-8")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    backup_dir = input_dir / "backup" / f"{_backup_stamp(modified_time)}_REQ-TEST"
    assert sorted(path.name for path in backup_dir.glob("*.json")) == _artifact_names()
    assert sorted(path.name for path in input_dir.glob("*.json") if path.name.startswith(("EpisodeRunQueue_", "EpisodeSetup_", "DeliveryBotSetup_"))) == _artifact_names()
    assert (input_dir / "notes.json").exists()


def test_scenario_generate_does_not_create_backup_when_no_existing_artifacts(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "configured_output"
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    assert not (_generated_input_dir(output_root) / "backup").exists()


def test_scenario_generate_ignores_files_already_under_backup(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "configured_output"
    input_dir = _generated_input_dir(output_root)
    backup_existing = input_dir / "backup" / "20260607T000000Z_OLD"
    backup_existing.mkdir(parents=True)
    preserved = backup_existing / "EpisodeSetup_preserved_000.json"
    preserved.write_text("{}", encoding="utf-8")
    modified_time = datetime(2026, 6, 7, 2, 28, 49, tzinfo=timezone.utc).timestamp()
    _write_existing_artifacts(input_dir, modified_time=modified_time)
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    assert preserved.exists()
    assert sorted(path.name for path in backup_existing.glob("*.json")) == ["EpisodeSetup_preserved_000.json"]


def test_scenario_generate_skips_backup_and_writes_when_disabled_with_existing_artifacts(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "disabled_output"
    input_dir = _generated_input_dir(output_root)
    _write_existing_artifacts(input_dir, modified_time=datetime(2026, 6, 7, tzinfo=timezone.utc).timestamp())
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "false")
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    assert sorted(path.name for path in input_dir.glob("*.json")) == _artifact_names()
    assert not (input_dir / "backup").exists()


def test_scenario_generate_reports_backup_failure(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "configured_output"
    input_dir = _generated_input_dir(output_root)
    _write_existing_artifacts(input_dir, modified_time=datetime(2026, 6, 7, tzinfo=timezone.utc).timestamp())
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    def fail_move(source, destination):
        raise OSError("move failed")

    monkeypatch.setattr(scenario_generation_service.shutil, "move", fail_move)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"]["code"] == "SCENARIO_ARTIFACT_BACKUP_FAILED"
    assert sorted(path.name for path in input_dir.glob("*.json")) == _artifact_names()


def test_scenario_generate_writes_artifacts_to_configured_output_dir(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "configured_output"
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    input_dir = _generated_input_dir(output_root)
    assert sorted(path.name for path in input_dir.glob("*.json")) == [
        "DeliveryBotSetup_test_000.json",
        "EpisodeRunQueue_test.json",
        "EpisodeSetup_test_000.json",
    ]


def test_scenario_generate_skips_file_writes_when_disabled(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "disabled_output"
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "false")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    assert response.json()["runs"][0]["episode_setup"] == "Json/Input/EpisodeSetup_test_000.json"
    assert not output_root.exists()


def test_scenario_generate_writes_expected_artifact_counts_and_valid_json(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "three_episodes"
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    artifacts = _artifact_result(tmp_path, run_count=3)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 3})

    assert response.status_code == 200
    input_dir = _generated_input_dir(output_root)
    run_queue_files = sorted(input_dir.glob("EpisodeRunQueue_*.json"))
    episode_files = sorted(input_dir.glob("EpisodeSetup_*.json"))
    bot_files = sorted(input_dir.glob("DeliveryBotSetup_*.json"))
    assert len(run_queue_files) == 1
    assert len(episode_files) == 3
    assert len(bot_files) == 3
    run_queue_payload = json.loads(run_queue_files[0].read_text(encoding="utf-8"))
    assert [Path(run["episode_setup"]).name for run in run_queue_payload["runs"]] == [path.name for path in episode_files]
    assert [Path(run["delivery_bot_setup"]).name for run in run_queue_payload["runs"]] == [path.name for path in bot_files]
    for path in [*run_queue_files, *episode_files, *bot_files]:
        assert isinstance(json.loads(path.read_text(encoding="utf-8")), dict)


def test_scenario_generate_reports_invalid_output_dir(monkeypatch, tmp_path) -> None:
    output_file = tmp_path / "not_a_directory"
    output_file.write_text("occupied", encoding="utf-8")
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_file))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"]["code"] == "SCENARIO_ARTIFACT_OUTPUT_DIR_INVALID"


def test_scenario_generate_rejects_relative_output_dir_traversal(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", "../outside")
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"]["code"] == "SCENARIO_ARTIFACT_OUTPUT_DIR_INVALID"


def test_scenario_generate_reports_file_write_failure(monkeypatch, tmp_path) -> None:
    output_root = tmp_path / "write_failure"
    monkeypatch.setenv("SCENARIO_ARTIFACT_OUTPUT_DIR", str(output_root))
    monkeypatch.setenv("SCENARIO_ARTIFACT_WRITE_ENABLED", "true")
    artifacts = _artifact_result(tmp_path, run_count=1)
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request, **kwargs: artifacts)

    def fail_write(path, data):
        raise OSError("disk full")

    monkeypatch.setattr(scenario_generation_service, "write_json_report", fail_write)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"]["code"] == "SCENARIO_ARTIFACT_WRITE_FAILED"
    assert response.json()["detail"]["filename"] == "EpisodeRunQueue_test.json"


def test_removed_scenario_generation_endpoints_return_not_found(monkeypatch) -> None:
    called = False

    def fail_if_called(request, **kwargs):
        nonlocal called
        called = True
        raise AssertionError("removed endpoint must not invoke scenario generation")

    monkeypatch.setattr(routes, "generate_scenario_artifacts", fail_if_called)
    client = TestClient(app)

    artifacts_response = client.post("/api/v1/scenarios/generate-artifacts", json={"prompt": "test", "episode_count": 1})
    drive_response = client.post("/api/v1/scenarios/generate-drive", json={"prompt": "test", "episode_count": 1})

    assert artifacts_response.status_code == 404
    assert drive_response.status_code == 404
    assert called is False
